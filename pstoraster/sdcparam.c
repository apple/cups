/*
  Copyright 1993-2000 by Easy Software Products.
  Copyright 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

#include <config.h>
#ifdef HAVE_LIBJPEG
/*$Id: sdcparam.c,v 1.2 2000/03/14 13:52:35 mike Exp $ */
/* DCT filter parameter setting and reading */
#include "memory_.h"
#include "jpeglib.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "strimpl.h"		/* sdct.h requires this */
#include "sdct.h"
#include "sdcparam.h"
#include "sjpeg.h"

/* Define the DCT parameters. */
#define dctp(key, type, stype, memb) { key, type, offset_of(stype, memb) }
private const gs_param_item_t s_DCT_param_items[] =
{
dctp("ColorTransform", gs_param_type_int, stream_DCT_state, ColorTransform),
    dctp("QFactor", gs_param_type_float, stream_DCT_state, QFactor),
    gs_param_item_end
};
private const gs_param_item_t jsd_param_items[] =
{
    dctp("Picky", gs_param_type_int, jpeg_stream_data, Picky),
    dctp("Relax", gs_param_type_int, jpeg_stream_data, Relax),
    gs_param_item_end
};

#undef dctp

/*
 * Adobe specifies the values to be supplied in zigzag order.
 * For IJG versions newer than v6, we need to convert this order
 * to natural array order.  Older IJG versions want zigzag order.
 */
#if JPEG_LIB_VERSION >= 61
	/* natural array position of n'th element of JPEG zigzag order */
static const byte natural_order[DCTSIZE2] =
{
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

#define jpeg_order(x)  natural_order[x]
	/* invert natural_order for getting parameters */
static const byte inverse_natural_order[DCTSIZE2] =
{
    0, 1, 5, 6, 14, 15, 27, 28,
    2, 4, 7, 13, 16, 26, 29, 42,
    3, 8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

#define jpeg_inverse_order(x)  inverse_natural_order[x]
#else
#define jpeg_order(x)  (x)
#define jpeg_inverse_order(x) (x)
#endif

/* ================ Get parameters ================ */

private int
quant_param_string(gs_param_string * pstr, int count, const UINT16 * pvals,
		   floatp QFactor, gs_memory_t * mem)
{
    byte *data;
    int code = 0;
    int i;

    data = gs_alloc_string(mem, count, "quant_param_string");
    if (data == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < count; ++i) {
	floatp val = pvals[jpeg_inverse_order(i)] / QFactor;

	data[i] =
	    (val < 1 ? (code = 1) : val > 255 ? (code = 255) : (byte) val);
    }
    pstr->data = data;
    pstr->size = count;
    pstr->persistent = true;
    return code & 1;
}

private int
quant_param_array(gs_param_float_array * pfa, int count, const UINT16 * pvals,
		  floatp QFactor, gs_memory_t * mem)
{
    float *data;
    int i;

    data = (float *)gs_alloc_byte_array(mem, count, sizeof(float),
					"quant_param_array");

    if (data == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < count; ++i)
	data[i] = pvals[jpeg_inverse_order(i)] / QFactor;
    pfa->data = data;
    pfa->size = count;
    pfa->persistent = true;
    return 0;
}

int
s_DCT_get_quantization_tables(gs_param_list * plist,
	   const stream_DCT_state * pdct, const stream_DCT_state * defaults,
			      bool is_encode)
{
    gs_memory_t *mem = pdct->memory;
    jpeg_component_info d_comp_info[4];
    int num_in_tables;
    const jpeg_component_info *comp_info;
    const jpeg_component_info *default_comp_info;
    JQUANT_TBL **table_ptrs;
    JQUANT_TBL **default_table_ptrs;
    gs_param_array quant_tables;
    floatp QFactor = pdct->QFactor;
    int i;
    int code;

    if (is_encode) {
	num_in_tables = pdct->data.compress->cinfo.num_components;
	comp_info = pdct->data.compress->cinfo.comp_info;
	table_ptrs = pdct->data.compress->cinfo.quant_tbl_ptrs;
	if (defaults) {
	    default_comp_info = defaults->data.compress->cinfo.comp_info;
	    default_table_ptrs = defaults->data.compress->cinfo.quant_tbl_ptrs;
	}
    } else {
	num_in_tables = quant_tables.size;
	for (i = 0; i < num_in_tables; ++i)
	    d_comp_info[i].quant_tbl_no = i;
	comp_info = d_comp_info;
	table_ptrs = pdct->data.decompress->dinfo.quant_tbl_ptrs;
	if (defaults) {
	    default_comp_info = d_comp_info;
	    default_table_ptrs =
		defaults->data.decompress->dinfo.quant_tbl_ptrs;
	}
    }

    /* Check whether all tables match defaults. */
    if (defaults) {
	bool match = true;

	for (i = 0; i < num_in_tables; ++i) {
	    JQUANT_TBL *tbl = table_ptrs[comp_info[i].quant_tbl_no];
	    JQUANT_TBL *default_tbl =
	    (default_comp_info == 0 || default_table_ptrs == 0 ? 0 :
	     default_table_ptrs[default_comp_info[i].quant_tbl_no]);

	    if (tbl == default_tbl)
		continue;
	    if (tbl == 0 || default_tbl == 0 ||
		memcmp(tbl->quantval, default_tbl->quantval,
		       DCTSIZE2 * sizeof(UINT16))
		) {
		match = false;
		break;
	    }
	}
	if (match)
	    return 0;
    }
    quant_tables.size = num_in_tables;
    code = param_begin_write_collection(plist, "QuantTables",
					&quant_tables,
					gs_param_collection_array);
    if (code < 0)
	return code;
    for (i = 0; i < num_in_tables; ++i) {
	char key[3];
	gs_param_string str;
	gs_param_float_array fa;

	sprintf(key, "%d", i);
	if (QFactor == 1.0) {
	    code = quant_param_string(&str, DCTSIZE2,
			    table_ptrs[comp_info[i].quant_tbl_no]->quantval,
				      QFactor, mem);
	    switch (code) {
		case 0:
		    code = param_write_string(quant_tables.list, key, &str);
		    if (code < 0)
			return code;	/* should dealloc */
		    continue;
		default:
		    return code;	/* should dealloc */
		case 1:
		    break;
	    }
	    /* break const to free the string */
	    gs_free_string(mem, (byte *) str.data, str.size,
			   "quant_param_string");
	}
	code = quant_param_array(&fa, DCTSIZE2,
			    table_ptrs[comp_info[i].quant_tbl_no]->quantval,
				 QFactor, mem);
	if (code < 0)
	    return code;	/* should dealloc */
	code = param_write_float_array(quant_tables.list, key, &fa);
	if (code < 0)
	    return code;	/* should dealloc */
    }
    return param_end_write_dict(plist, "QuantTables", &quant_tables);
}

private int
pack_huff_table(gs_param_string * pstr, const JHUFF_TBL * table,
		gs_memory_t * mem)
{
    int total;
    int i;
    byte *data;

    for (i = 1, total = 0; i <= 16; ++i)
	total += table->bits[i];
    data = gs_alloc_string(mem, 16 + total, "pack_huff_table");
    if (data == 0)
	return_error(gs_error_VMerror);
    memcpy(data, table->bits + 1, 16);
    memcpy(data + 16, table->huffval, total);
    pstr->data = data;
    pstr->size = 16 + total;
    pstr->persistent = true;
    return 0;
}

int
s_DCT_get_huffman_tables(gs_param_list * plist,
	   const stream_DCT_state * pdct, const stream_DCT_state * defaults,
			 bool is_encode)
{
    gs_memory_t *mem = pdct->memory;
    gs_param_string *huff_data;
    gs_param_string_array hta;
    int num_in_tables;
    jpeg_component_info *comp_info;
    JHUFF_TBL **dc_table_ptrs;
    JHUFF_TBL **ac_table_ptrs;
    int i;
    int code = 0;

    if (is_encode) {
	dc_table_ptrs = pdct->data.compress->cinfo.dc_huff_tbl_ptrs;
	ac_table_ptrs = pdct->data.compress->cinfo.ac_huff_tbl_ptrs;
	num_in_tables = pdct->data.compress->cinfo.input_components * 2;
	comp_info = pdct->data.compress->cinfo.comp_info;
    } else {
	dc_table_ptrs = pdct->data.decompress->dinfo.dc_huff_tbl_ptrs;
	ac_table_ptrs = pdct->data.decompress->dinfo.ac_huff_tbl_ptrs;
	for (i = 2; i > 0; --i)
	    if (dc_table_ptrs[i - 1] || ac_table_ptrs[i - 1])
		break;
	num_in_tables = i * 2;
	comp_info = NULL;	/* do not set for decompress case */
    }
/****** byte_array IS WRONG ******/
    huff_data = (gs_param_string *)
	gs_alloc_byte_array(mem, num_in_tables, sizeof(gs_param_string),
			    "get huffman tables");
    if (huff_data == 0)
	return_error(gs_error_VMerror);
    for (i = 0; i < num_in_tables; i += 2) {
	if ((code = pack_huff_table(huff_data + i, ac_table_ptrs[i >> 1], mem)) < 0 ||
	    (code = pack_huff_table(huff_data + i + 1, dc_table_ptrs[i >> 1], mem))
	    )
	    break;
    }
    if (code < 0)
	return code;
    hta.data = huff_data;
    hta.size = num_in_tables;
    hta.persistent = true;
    return param_write_string_array(plist, "HuffTables", &hta);
}

int
s_DCT_get_params(gs_param_list * plist, const stream_DCT_state * ss,
		 const stream_DCT_state * defaults)
{
    int code =
    gs_param_write_items(plist, ss, defaults, s_DCT_param_items);

    if (code >= 0)
	code = gs_param_write_items(plist, ss->data.common,
				    (defaults ? defaults->data.common :
				     NULL),
				    jsd_param_items);
    return code;
}

/* ================ Put parameters ================ */

stream_state_proc_put_params(s_DCT_put_params, stream_DCT_state);	/* check */

/* ---------------- Utilities ---------------- */

/*
 * Get N byte-size values from an array or a string.
 * Used for HuffTables, HSamples, VSamples.
 */
int
s_DCT_byte_params(gs_param_list * plist, gs_param_name key, int start,
		  int count, UINT8 * pvals)
{
    int i;
    gs_param_string bytes;
    gs_param_float_array floats;
    int code = param_read_string(plist, key, &bytes);

    switch (code) {
	case 0:
	    if (bytes.size < start + count) {
		code = gs_note_error(gs_error_rangecheck);
		break;
	    }
	    for (i = 0; i < count; ++i)
		pvals[i] = (UINT8) bytes.data[start + i];
	    return 0;
	default:		/* might be a float array */
	    code = param_read_float_array(plist, key, &floats);
	    if (!code) {
		if (floats.size < start + count) {
		    code = gs_note_error(gs_error_rangecheck);
		    break;
		}
		for (i = 0; i < count; ++i) {
		    float v = floats.data[start + i];

		    if (v < 0 || v > 255) {
			code = gs_note_error(gs_error_rangecheck);
			break;
		    }
		    pvals[i] = (UINT8) (v + 0.5);
		}
	    }
    }
    if (code < 0)
	param_signal_error(plist, key, code);
    return code;
}

/* Get N quantization values from an array or a string. */
private int
quant_params(gs_param_list * plist, gs_param_name key, int count,
	     UINT16 * pvals, floatp QFactor)
{
    int i;
    gs_param_string bytes;
    gs_param_float_array floats;
    int code = param_read_string(plist, key, &bytes);

    switch (code) {
	case 0:
	    if (bytes.size != count) {
		code = gs_note_error(gs_error_rangecheck);
		break;
	    }
	    for (i = 0; i < count; ++i) {
		double v = bytes.data[i] * QFactor;

		pvals[jpeg_order(i)] =
		    (UINT16) (v < 1 ? 1 : v > 255 ? 255 : v + 0.5);
	    }
	    return 0;
	default:		/* might be a float array */
	    code = param_read_float_array(plist, key, &floats);
	    if (!code) {
		if (floats.size != count) {
		    code = gs_note_error(gs_error_rangecheck);
		    break;
		}
		for (i = 0; i < count; ++i) {
		    double v = floats.data[i] * QFactor;

		    pvals[jpeg_order(i)] =
			(UINT16) (v < 1 ? 1 : v > 255 ? 255 : v + 0.5);
		}
	    }
    }
    if (code < 0)
	param_signal_error(plist, key, code);
    return code;
#undef jpeg_order
}

/* ---------------- Main procedures ---------------- */

/* Put common scalars. */
int
s_DCT_put_params(gs_param_list * plist, stream_DCT_state * pdct)
{
    int code =
    gs_param_read_items(plist, pdct, s_DCT_param_items);

    if (code < 0)
	return code;
    code = gs_param_read_items(plist, pdct->data.common, jsd_param_items);
    if (code < 0)
	return code;
    if (pdct->data.common->Picky < 0 || pdct->data.common->Picky > 1 ||
	pdct->data.common->Relax < 0 || pdct->data.common->Relax > 1 ||
	pdct->ColorTransform < -1 || pdct->ColorTransform > 2 ||
	pdct->QFactor < 0.0 || pdct->QFactor > 1000000.0
	)
	return_error(gs_error_rangecheck);
    return 0;
}

/* Put quantization tables. */
int
s_DCT_put_quantization_tables(gs_param_list * plist, stream_DCT_state * pdct,
			      bool is_encode)
{
    int code;
    int i, j;
    gs_param_array quant_tables;	/* array of strings/arrays */
    int num_in_tables;
    int num_out_tables;
    jpeg_component_info *comp_info;
    JQUANT_TBL **table_ptrs;
    JQUANT_TBL *this_table;

    switch ((code = param_begin_read_dict(plist, "QuantTables",
					  &quant_tables, true))
	) {
	case 1:
	    return 0;
	default:
	    return param_signal_error(plist, "QuantTables", code);
	case 0:
	    ;
    }
    if (is_encode) {
	num_in_tables = pdct->data.compress->cinfo.num_components;
	if (quant_tables.size < num_in_tables)
	    return_error(gs_error_rangecheck);
	comp_info = pdct->data.compress->cinfo.comp_info;
	table_ptrs = pdct->data.compress->cinfo.quant_tbl_ptrs;
    } else {
	num_in_tables = quant_tables.size;
	comp_info = NULL;	/* do not set for decompress case */
	table_ptrs = pdct->data.decompress->dinfo.quant_tbl_ptrs;
    }
    num_out_tables = 0;
    for (i = 0; i < num_in_tables; ++i) {
	char istr[5];		/* i converted to string key */
	UINT16 values[DCTSIZE2];

	sprintf(istr, "%d", i);
	code = quant_params(quant_tables.list, istr, DCTSIZE2, values,
			    pdct->QFactor);
	if (code < 0)
	    return code;
	/* Check for duplicate tables. */
	for (j = 0; j < num_out_tables; j++) {
	    if (!memcmp(table_ptrs[j]->quantval, values, sizeof(values)))
		break;
	}
	if (comp_info != NULL)
	    comp_info[i].quant_tbl_no = j;
	if (j < num_out_tables)	/* found a duplicate */
	    continue;
	if (++num_out_tables > NUM_QUANT_TBLS)
	    return_error(gs_error_rangecheck);
	this_table = table_ptrs[j];
	if (this_table == NULL) {
	    this_table = gs_jpeg_alloc_quant_table(pdct);
	    if (this_table == NULL)
		return_error(gs_error_VMerror);
	    table_ptrs[j] = this_table;
	}
	memcpy(this_table->quantval, values, sizeof(values));
    }
    return 0;
}

/* Put Huffman tables. */
private int
find_huff_values(JHUFF_TBL ** table_ptrs, int num_tables,
	       const UINT8 counts[16], const UINT8 * values, int codes_size)
{
    int j;

    for (j = 0; j < num_tables; ++j)
	if (!memcmp(table_ptrs[j]->bits, counts, sizeof(counts)) &&
	    !memcmp(table_ptrs[j]->huffval, values,
		    codes_size * sizeof(values[0])))
	    break;
    return j;
}
int
s_DCT_put_huffman_tables(gs_param_list * plist, stream_DCT_state * pdct,
			 bool is_encode)
{
    int code;
    int i, j;
    gs_param_array huff_tables;
    int num_in_tables;
    int ndc, nac;
    int codes_size;
    jpeg_component_info *comp_info;
    JHUFF_TBL **dc_table_ptrs;
    JHUFF_TBL **ac_table_ptrs;
    JHUFF_TBL **this_table_ptr;
    JHUFF_TBL *this_table;
    int max_tables = 2;		/* baseline limit */

    switch ((code = param_begin_read_dict(plist, "HuffTables",
					  &huff_tables, true))
	) {
	case 1:
	    return 0;
	default:
	    return param_signal_error(plist, "HuffTables", code);
	case 0:
	    ;
    }
    if (is_encode) {
	num_in_tables = pdct->data.compress->cinfo.input_components * 2;
	if (huff_tables.size < num_in_tables)
	    return_error(gs_error_rangecheck);
	comp_info = pdct->data.compress->cinfo.comp_info;
	dc_table_ptrs = pdct->data.compress->cinfo.dc_huff_tbl_ptrs;
	ac_table_ptrs = pdct->data.compress->cinfo.ac_huff_tbl_ptrs;
	if (pdct->data.common->Relax)
	    max_tables = max(pdct->data.compress->cinfo.input_components, 2);
    } else {
	num_in_tables = huff_tables.size;
	comp_info = NULL;	/* do not set for decompress case */
	dc_table_ptrs = pdct->data.decompress->dinfo.dc_huff_tbl_ptrs;
	ac_table_ptrs = pdct->data.decompress->dinfo.ac_huff_tbl_ptrs;
	if (pdct->data.common->Relax)
	    max_tables = NUM_HUFF_TBLS;
    }
    ndc = nac = 0;
    for (i = 0; i < num_in_tables; ++i) {
	char istr[5];		/* i converted to string key */
	UINT8 counts[16], values[256];

	/* Collect the Huffman parameters. */
	sprintf(istr, "%d", i);
	code = s_DCT_byte_params(huff_tables.list, istr, 0, 16, counts);
	if (code < 0)
	    return code;
	for (codes_size = 0, j = 0; j < 16; j++)
	    codes_size += counts[j];
	if (codes_size > 256 /*|| r_size(pa) != codes_size+16 */ )
	    return_error(gs_error_rangecheck);
	code = s_DCT_byte_params(huff_tables.list, istr, 16, codes_size,
				 values);
	if (code < 0)
	    return code;
	if (i & 1) {
	    j = find_huff_values(ac_table_ptrs, nac, counts, values,
				 codes_size);
	    if (comp_info != NULL)
		comp_info[i >> 1].ac_tbl_no = j;
	    if (j < nac)
		continue;
	    if (++nac > NUM_HUFF_TBLS)
		return_error(gs_error_rangecheck);
	    this_table_ptr = ac_table_ptrs + j;
	} else {
	    j = find_huff_values(dc_table_ptrs, ndc, counts, values,
				 codes_size);
	    if (comp_info != NULL)
		comp_info[i >> 1].dc_tbl_no = j;
	    if (j < ndc)
		continue;
	    if (++ndc > NUM_HUFF_TBLS)
		return_error(gs_error_rangecheck);
	    this_table_ptr = dc_table_ptrs + j;
	}
	this_table = *this_table_ptr;
	if (this_table == NULL) {
	    this_table = gs_jpeg_alloc_huff_table(pdct);
	    if (this_table == NULL)
		return_error(gs_error_VMerror);
	    *this_table_ptr = this_table;
	}
	memcpy(this_table->bits, counts, sizeof(counts));
	memcpy(this_table->huffval, values, codes_size * sizeof(values[0]));
    }
    if (nac > max_tables || ndc > max_tables)
	return_error(gs_error_rangecheck);
    return 0;
}
#endif /* HAVE_LIBJPEG */
