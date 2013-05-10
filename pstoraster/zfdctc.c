/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/
#include <config.h>
#ifdef HAVE_LIBJPEG

/* zfdctc.c */
/* Common code for DCT filter creation */
#include "memory_.h"
#include "stdio_.h"			/* for jpeglib.h */
#include "jpeglib.h"
#include "ghost.h"
#include "errors.h"
#include "opcheck.h"
#include "idict.h"
#include "idparam.h"
#include "imemory.h"			/* for iutil.h */
#include "ipacked.h"
#include "iutil.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/* Forward references */
private int quant_params(P4(const ref *, int, UINT16 *, floatp));
int zfdct_byte_params(P4(const ref *, int, int, UINT8 *));

/* Common setup for encoding and decoding filters. */
int
zfdct_setup_quantization_tables(const ref *op, stream_DCT_state *pdct,
				bool is_encode)
{	int code;
	int i, j;
	ref *pdval;
	const ref *pa;
	const ref *QuantArrays[NUM_QUANT_TBLS]; /* for detecting duplicates */
	int num_in_tables;
	int num_out_tables;
	jpeg_component_info * comp_info;
	JQUANT_TBL ** table_ptrs;
	JQUANT_TBL * this_table;

	if ( op == 0 || dict_find_string(op, "QuantTables", &pdval) <= 0 )
		return 0;
	if ( !r_has_type(pdval, t_array) )
		return_error(e_typecheck);
	if ( is_encode )
	{	num_in_tables = pdct->data.compress->cinfo.num_components;
		if ( r_size(pdval) < num_in_tables )
			return_error(e_rangecheck);
		comp_info = pdct->data.compress->cinfo.comp_info;
		table_ptrs = pdct->data.compress->cinfo.quant_tbl_ptrs;
	}
	else
	{	num_in_tables = r_size(pdval);
		comp_info = NULL; /* do not set for decompress case */
		table_ptrs = pdct->data.decompress->dinfo.quant_tbl_ptrs;
	}
	num_out_tables = 0;
	for ( i = 0, pa = pdval->value.const_refs;
	      i < num_in_tables; i++, pa++
	    )
	{	for ( j = 0; j < num_out_tables; j++ )
		{	if ( obj_eq(pa, QuantArrays[j]) )
				break;
		}
		if ( comp_info != NULL )
			comp_info[i].quant_tbl_no = j;
		if ( j < num_out_tables )
			continue;
		if ( ++num_out_tables > NUM_QUANT_TBLS )
			return_error(e_rangecheck);
		QuantArrays[j] = pa;
		this_table = table_ptrs[j];
		if ( this_table == NULL )
		{	this_table = gs_jpeg_alloc_quant_table(pdct);
			if ( this_table == NULL )
				return_error(e_VMerror);
			table_ptrs[j] = this_table;
		}
		if ( r_size(pa) != DCTSIZE2 )
			return_error(e_rangecheck);
		code = quant_params(pa, DCTSIZE2,
				    this_table->quantval, pdct->QFactor);
		if ( code < 0 )
			return code;
	}
	return 0;
}

int
zfdct_setup_huffman_tables(const ref *op, stream_DCT_state *pdct,
			   bool is_encode)
{	int code;
	int i, j;
	ref *pdval;
	const ref *pa;
	const ref *DCArrays[NUM_HUFF_TBLS]; /* for detecting duplicates */
	const ref *ACArrays[NUM_HUFF_TBLS];
	int num_in_tables;
	int ndc, nac;
	int codes_size;
	jpeg_component_info * comp_info;
	JHUFF_TBL ** dc_table_ptrs;
	JHUFF_TBL ** ac_table_ptrs;
	JHUFF_TBL ** this_table_ptr;
	JHUFF_TBL * this_table;
	int max_tables = 2;		/* baseline limit */

	if ( op == 0 )			/* no dictionary */
		return 0;
	if ( (code = dict_find_string(op, "HuffTables", &pdval)) <= 0)
		return 0;
	if ( !r_has_type(pdval, t_array) )
		return_error(e_typecheck);
	if ( is_encode )
	{	num_in_tables = pdct->data.compress->cinfo.input_components * 2;
		if ( r_size(pdval) < num_in_tables )
			return_error(e_rangecheck);
		comp_info = pdct->data.compress->cinfo.comp_info;
		dc_table_ptrs = pdct->data.compress->cinfo.dc_huff_tbl_ptrs;
		ac_table_ptrs = pdct->data.compress->cinfo.ac_huff_tbl_ptrs;
		if ( pdct->data.common->Relax )
			max_tables = max(pdct->data.compress->cinfo.input_components, 2);
	}
	else
	{	num_in_tables = r_size(pdval);
		comp_info = NULL; /* do not set for decompress case */
		dc_table_ptrs = pdct->data.decompress->dinfo.dc_huff_tbl_ptrs;
		ac_table_ptrs = pdct->data.decompress->dinfo.ac_huff_tbl_ptrs;
		if ( pdct->data.common->Relax )
			max_tables = NUM_HUFF_TBLS;
	}
	ndc = nac = 0;
	for ( i = 0, pa = pdval->value.const_refs;
	      i < num_in_tables; i++, pa++
	    )
	{	if ( i & 1 )
		{	for ( j = 0; j < nac; j++ )
			{	if ( obj_eq(pa, ACArrays[j]) )
					break;
			}
			if ( comp_info != NULL )
				comp_info[i>>1].ac_tbl_no = j;
			if ( j < nac )
				continue;
			if ( ++nac > NUM_HUFF_TBLS )
				return_error(e_rangecheck);
			ACArrays[j] = pa;
			this_table_ptr = ac_table_ptrs + j;
		}
		else
		{	for ( j = 0; j < ndc; j++ )
			{	if ( obj_eq(pa, DCArrays[j]) )
					break;
			}
			if ( comp_info != NULL )
				comp_info[i>>1].dc_tbl_no = j;
			if ( j < ndc )
				continue;
			if ( ++ndc > NUM_HUFF_TBLS )
				return_error(e_rangecheck);
			DCArrays[j] = pa;
			this_table_ptr = dc_table_ptrs + j;
		}
		this_table = *this_table_ptr;
		if ( this_table == NULL )
		{	this_table = gs_jpeg_alloc_huff_table(pdct);
			if ( this_table == NULL )
				return_error(e_VMerror);
			*this_table_ptr = this_table;
		}
		if ( r_size(pa) < 16 )
			return_error(e_rangecheck);
		code = zfdct_byte_params(pa, 0, 16, this_table->bits + 1);
		if ( code < 0 )
			return code;
		for ( codes_size = 0, j = 1; j <= 16; j++ )
			codes_size += this_table->bits[j];
		if ( codes_size > 256 || r_size(pa) != codes_size+16 )
			return_error(e_rangecheck);
		code = zfdct_byte_params(pa, 16, codes_size, this_table->huffval);
		if ( code < 0 )
			return code;
	}
	if ( nac > max_tables || ndc > max_tables )
		return_error(e_rangecheck);
	return 0;
}

/* The main procedure */
int
zfdct_setup(const ref *op, stream_DCT_state *pdct)
{	const ref *dop;
	int npop;
	int code;

	/* Initialize the state in case we bail out. */
	pdct->Markers.data = 0;
	pdct->Markers.size = 0;
	if ( !r_has_type(op, t_dictionary) )
	{	npop = 0;
		dop = 0;
	}
	else
	{	check_dict_read(*op);
		npop = 1;
		dop = op;
	}
	/* These parameters are common to both, and are all defaultable. */
	if ( (code = dict_int_param(dop, "Picky", 0, 1, 0,
				    &pdct->data.common->Picky)) < 0 ||
	     (code = dict_int_param(dop, "Relax", 0, 1, 0,
				    &pdct->data.common->Relax)) < 0 ||
	     (code = dict_int_param(dop, "ColorTransform", -1, 2, -1,
				    &pdct->ColorTransform)) < 0 ||
	     (code = dict_float_param(dop, "QFactor", 1.0,
				      &pdct->QFactor)) < 0
	   )
		return code;
	if ( pdct->QFactor < 0.0 || pdct->QFactor > 1000000.0 )
		return_error(e_rangecheck);
	return npop;
}

/* ------ Internal routines ------ */

/* Get N quantization values from an array or a string. */

private int
quant_params(const ref *op, int count, UINT16 *pvals, floatp QFactor)
{	int i;
	const ref_packed *pref;
	double val;
	/* Adobe specifies the values to be supplied in zigzag order.
	 * For IJG versions newer than v6, we need to convert this order
	 * to natural array order.  Older IJG versions want zigzag order.
	 */
#if JPEG_LIB_VERSION >= 61
	/* natural array position of n'th element of JPEG zigzag order */
	static const int natural_order[DCTSIZE2] = {
	   0,  1,  8, 16,  9,  2,  3, 10,
	  17, 24, 32, 25, 18, 11,  4,  5,
	  12, 19, 26, 33, 40, 48, 41, 34,
	  27, 20, 13,  6,  7, 14, 21, 28,
	  35, 42, 49, 56, 57, 50, 43, 36,
	  29, 22, 15, 23, 30, 37, 44, 51,
	  58, 59, 52, 45, 38, 31, 39, 46,
	  53, 60, 61, 54, 47, 55, 62, 63
	};
#define jpeg_order(x)  natural_order[x]
#else
#define jpeg_order(x)  (x)
#endif
	switch ( r_type(op) )
	{
	case t_string:
		check_read(*op);
		for ( i = 0; i < count; i++ )
		{
			val = op->value.const_bytes[i] * QFactor;
			if ( val < 1 ) val = 1;
			if ( val > 255 ) val = 255;
			pvals[jpeg_order(i)] = (UINT16) (val + 0.5);
		}
		return 0;
	case t_array:
		check_read(*op);
		pref = (const ref_packed *)op->value.const_refs;
		break;
	case t_shortarray:
	case t_mixedarray:
		check_read(*op);
		pref = op->value.packed;
		break;
	default:
		return_error(e_typecheck);
	}
	for ( i = 0; i < count; pref = packed_next(pref), i++ )
	{	ref nref;
		packed_get(pref, &nref);
		switch ( r_type(&nref) )
		{
		case t_integer:
			val = nref.value.intval * QFactor;
			break;
		case t_real:
			val = nref.value.realval * QFactor;
			break;
		default:
			return_error(e_typecheck);
		}
		if ( val < 1 ) val = 1;
		if ( val > 255 ) val = 255;
		pvals[jpeg_order(i)] = (UINT16) (val + 0.5);
	}
	return 0;
#undef jpeg_order
}

/* Get N byte-size values from an array or a string.
 * Used for HuffTables, HSamples, VSamples.
 */
int
zfdct_byte_params(const ref *op, int start, int count, UINT8 *pvals)
{	int i;
	const ref_packed *pref;
	UINT8 *pval;
	switch ( r_type(op) )
	{
	case t_string:
		check_read(*op);
		for ( i = 0, pval = pvals; i < count; i++, pval++ )
			*pval = (UINT8)op->value.const_bytes[start+i];
		return 0;
	case t_array:
		check_read(*op);
		pref = (const ref_packed *)(op->value.const_refs + start);
		break;
	case t_shortarray:
	case t_mixedarray:
		check_read(*op);
		pref = op->value.packed;
		for ( i = 0; i < start; i++ )
		  pref = packed_next(pref);
		break;
	default:
		return_error(e_typecheck);
	}
	for ( i = 0, pval = pvals; i < count;
	      pref = packed_next(pref), i++, pval++
	    )
	{	ref nref;
		packed_get(pref, &nref);
		switch ( r_type(&nref) )
		{
		case t_integer:
			if ( nref.value.intval < 0 || nref.value.intval > 255 )
				return_error(e_rangecheck);
			*pval = (UINT8)nref.value.intval;
			break;
		case t_real:
			if ( nref.value.realval < 0 || nref.value.realval > 255 )
				return_error(e_rangecheck);
			*pval = (UINT8)(nref.value.realval + 0.5);
			break;
		default:
			return_error(e_typecheck);
		}
	}
	return 0;
}
#endif /* HAVE_LIBJPEG */
