/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zfilterx.c,v 1.2 2000/03/08 23:15:36 mike Exp $ */
/* Extended (non-standard) filter creation */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"
#include "strimpl.h"
#include "sfilter.h"
#include "sbwbs.h"
#include "sbhc.h"
#include "sbtx.h"
#include "shcgen.h"
#include "smtf.h"
#include "spcxx.h"
#include "ifilter.h"

/* ------ Bounded Huffman code filters ------ */

/* Common setup for encoding and decoding filters */
private int
bhc_setup(os_ptr op, stream_BHC_state * pbhcs)
{
    int code;
    int num_counts;
    int data[max_hc_length + 1 + 256 + max_zero_run + 1];
    uint dsize;
    int i;
    uint num_values, accum;
    ushort *counts;
    ushort *values;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    if ((code = dict_bool_param(op, "FirstBitLowOrder", false,
				&pbhcs->FirstBitLowOrder)) < 0 ||
	(code = dict_int_param(op, "MaxCodeLength", 1, max_hc_length,
			       max_hc_length, &num_counts)) < 0 ||
	(code = dict_bool_param(op, "EndOfData", true,
				&pbhcs->EndOfData)) < 0 ||
	(code = dict_uint_param(op, "EncodeZeroRuns", 2, 256,
				256, &pbhcs->EncodeZeroRuns)) < 0 ||
    /* Note: the code returned from the following call */
    /* is actually the number of elements in the array. */
	(code = dict_int_array_param(op, "Tables", countof(data),
				     data)) <= 0
	)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    dsize = code;
    if (dsize <= num_counts + 2)
	return_error(e_rangecheck);
    for (i = 0, num_values = 0, accum = 0; i <= num_counts;
	 i++, accum <<= 1
	) {
	int count = data[i];

	if (count < 0)
	    return_error(e_rangecheck);
	num_values += count;
	accum += count;
    }
    if (dsize != num_counts + 1 + num_values ||
	accum != 1 << (num_counts + 1) ||
	pbhcs->EncodeZeroRuns >
	(pbhcs->EndOfData ? num_values - 1 : num_values)
	)
	return_error(e_rangecheck);
    for (; i < num_counts + 1 + num_values; i++) {
	int value = data[i];

	if (value < 0 || value >= num_values)
	    return_error(e_rangecheck);
    }
    pbhcs->definition.counts = counts =
	(ushort *) ialloc_byte_array(num_counts + 1, sizeof(ushort),
				     "bhc_setup(counts)");
    pbhcs->definition.values = values =
	(ushort *) ialloc_byte_array(num_values, sizeof(ushort),
				     "bhc_setup(values)");
    if (counts == 0 || values == 0) {
	ifree_object(values, "bhc_setup(values)");
	ifree_object(counts, "bhc_setup(counts)");
	return_error(e_VMerror);
    }
    for (i = 0; i <= num_counts; i++)
	counts[i] = data[i];
    pbhcs->definition.counts = counts;
    pbhcs->definition.num_counts = num_counts;
    for (i = 0; i < num_values; i++)
	values[i] = data[i + num_counts + 1];
    pbhcs->definition.values = values;
    pbhcs->definition.num_values = num_values;
    return 0;
}

/* <target> <dict> BoundedHuffmanEncode/filter <file> */
private int
zBHCE(os_ptr op)
{
    stream_BHCE_state bhcs;
    int code = bhc_setup(op, (stream_BHC_state *)&bhcs);

    if (code < 0)
	return code;
    return filter_write(op, 0, &s_BHCE_template, (stream_state *)&bhcs, 0);
}

/* <source> <dict> BoundedHuffmanDecode/filter <file> */
private int
zBHCD(os_ptr op)
{
    stream_BHCD_state bhcs;
    int code = bhc_setup(op, (stream_BHC_state *)&bhcs);

    if (code < 0)
	return code;
    return filter_read(op, 0, &s_BHCD_template, (stream_state *)&bhcs, 0);
}

/* <array> <max_length> .computecodes <array> */
/* The first max_length+1 elements of the array will be filled in with */
/* the code counts; the remaining elements will be replaced with */
/* the code values.  This is the form needed for the Tables element of */
/* the dictionary parameter for the BoundedHuffman filters. */
private int
zcomputecodes(os_ptr op)
{
    os_ptr op1 = op - 1;
    uint asize;
    hc_definition def;
    ushort *data;
    long *freqs;
    int code = 0;

    check_type(*op, t_integer);
    check_write_type(*op1, t_array);
    asize = r_size(op1);
    if (op->value.intval < 1 || op->value.intval > max_hc_length)
	return_error(e_rangecheck);
    def.num_counts = op->value.intval;
    if (asize < def.num_counts + 2)
	return_error(e_rangecheck);
    def.num_values = asize - (def.num_counts + 1);
    data = (ushort *) gs_alloc_byte_array(imemory, asize, sizeof(ushort),
					  "zcomputecodes");
    freqs = (long *)gs_alloc_byte_array(imemory, def.num_values,
					sizeof(long),
					"zcomputecodes(freqs)");

    if (data == 0 || freqs == 0)
	code = gs_note_error(e_VMerror);
    else {
	uint i;

	def.counts = data;
	def.values = data + (def.num_counts + 1);
	for (i = 0; i < def.num_values; i++) {
	    const ref *pf = op1->value.const_refs + i + def.num_counts + 1;

	    if (!r_has_type(pf, t_integer)) {
		code = gs_note_error(e_typecheck);
		break;
	    }
	    freqs[i] = pf->value.intval;
	}
	if (!code) {
	    code = hc_compute(&def, freqs, imemory);
	    if (code >= 0) {
		/* Copy back results. */
		for (i = 0; i < asize; i++)
		    make_int(op1->value.refs + i, data[i]);
	    }
	}
    }
    gs_free_object(imemory, freqs, "zcomputecodes(freqs)");
    gs_free_object(imemory, data, "zcomputecodes");
    if (code < 0)
	return code;
    pop(1);
    return code;
}

/* ------ Burrows/Wheeler block sorting filters ------ */

/* Common setup for encoding and decoding filters */
private int
bwbs_setup(os_ptr op, stream_BWBS_state * pbwbss)
{
    int code =
	dict_int_param(op, "BlockSize", 1, max_int / sizeof(int) - 10, 16384,
		       &pbwbss->BlockSize);

    if (code < 0)
	return code;
    return 0;
}

/* <target> <dict> BWBlockSortEncode/filter <file> */
private int
zBWBSE(os_ptr op)
{
    stream_BWBSE_state bwbss;
    int code;

    check_type(*op, t_dictionary);
    check_dict_read(*op);
    code = bwbs_setup(op, (stream_BWBS_state *)&bwbss);
    if (code < 0)
	return code;
    return filter_write(op, 0, &s_BWBSE_template, (stream_state *)&bwbss, 0);
}

/* <source> <dict> BWBlockSortDecode/filter <file> */
private int
zBWBSD(os_ptr op)
{
    stream_BWBSD_state bwbss;
    int code = bwbs_setup(op, (stream_BWBS_state *)&bwbss);

    if (code < 0)
	return code;
    return filter_read(op, 0, &s_BWBSD_template, (stream_state *)&bwbss, 0);
}

/* ------ Byte translation filters ------ */

/* Common setup */
private int
bt_setup(os_ptr op, stream_BT_state * pbts)
{
    check_read_type(*op, t_string);
    if (r_size(op) != 256)
	return_error(e_rangecheck);
    memcpy(pbts->table, op->value.const_bytes, 256);
    return 0;
}

/* <target> <table> ByteTranslateEncode/filter <file> */
/* <target> <table> <dict> ByteTranslateEncode/filter <file> */
private int
zBTE(os_ptr op)
{
    stream_BT_state bts;
    int code = bt_setup(op, &bts);

    if (code < 0)
	return code;
    return filter_write(op, 0, &s_BTE_template, (stream_state *)&bts, 0);
}

/* <target> <table> ByteTranslateDecode/filter <file> */
/* <target> <table> <dict> ByteTranslateDecode/filter <file> */
private int
zBTD(os_ptr op)
{
    stream_BT_state bts;
    int code = bt_setup(op, &bts);

    if (code < 0)
	return code;
    return filter_read(op, 0, &s_BTD_template, (stream_state *)&bts, 0);
}

/* ------ Move-to-front filters ------ */

/* <target> MoveToFrontEncode/filter <file> */
/* <target> <dict> MoveToFrontEncode/filter <file> */
private int
zMTFE(os_ptr op)
{
    return filter_write_simple(op, &s_MTFE_template);
}

/* <source> MoveToFrontDecode/filter <file> */
/* <source> <dict> MoveToFrontDecode/filter <file> */
private int
zMTFD(os_ptr op)
{
    return filter_read_simple(op, &s_MTFD_template);
}

/* ------ PCX decoding filter ------ */

/* <source> PCXDecode/filter <file> */
/* <source> <dict> PCXDecode/filter <file> */
private int
zPCXD(os_ptr op)
{
    return filter_read_simple(op, &s_PCXD_template);
}

/* ================ Initialization procedure ================ */

const op_def zfilterx_op_defs[] =
{
    {"2.computecodes", zcomputecodes},	/* not a filter */
    op_def_begin_filter(),
		/* Non-standard filters */
    {"2BoundedHuffmanEncode", zBHCE},
    {"2BoundedHuffmanDecode", zBHCD},
    {"2BWBlockSortEncode", zBWBSE},
    {"2BWBlockSortDecode", zBWBSD},
    {"2ByteTranslateEncode", zBTE},
    {"2ByteTranslateDecode", zBTD},
    {"1MoveToFrontEncode", zMTFE},
    {"1MoveToFrontDecode", zMTFD},
    {"1PCXDecode", zPCXD},
    op_def_end(0)
};
