/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: zfreuse.c,v 1.1 2000/03/08 23:15:38 mike Exp $ */
/* ReusableStreamDecode filter support */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "stream.h"
#include "strimpl.h"
#include "sfilter.h"		/* for SubFileDecode */
#include "files.h"
#include "idict.h"
#include "idparam.h"
#include "iname.h"
#include "store.h"

/*
 * The actual work of constructing the filter is done in PostScript code.
 * The operators in this file are internal ones that handle the dirty work.
 */

/* <dict|null> .rsdparams <filters> <decodeparms|null> */
/* filters is always an array, and decodeparms is always either an array */
/* of the same length as filters, or null. */
private int
zrsdparams(os_ptr op)
{
    ref *pFilter;
    ref *pDecodeParms;
    int Intent;
    bool AsyncRead;
    ref empty_array, filter1_array, parms1_array;
    uint i;
    int code;

    make_empty_array(&empty_array, a_readonly);
    if (dict_find_string(op, "Filter", &pFilter) > 0) {
	if (!r_is_array(pFilter)) {
	    if (!r_has_type(pFilter, t_name))
		return_error(e_typecheck);
	    make_array(&filter1_array, a_readonly, 1, pFilter);
	    pFilter = &filter1_array;
	}
    } else
	pFilter = &empty_array;
    /* If Filter is undefined, ignore DecodeParms. */
    if (pFilter != &empty_array &&
	dict_find_string(op, "DecodeParms", &pDecodeParms) > 0
	) {
	if (pFilter == &filter1_array) {
	    make_array(&parms1_array, a_readonly, 1, pDecodeParms);
	    pDecodeParms = &parms1_array;
	} else if (!r_is_array(pDecodeParms))
	    return_error(e_typecheck);
	else if (r_size(pFilter) != r_size(pDecodeParms))
	    return_error(e_rangecheck);
    } else
	pDecodeParms = 0;
    for (i = 0; i < r_size(pFilter); ++i) {
	ref f, fname, dp;

	array_get(pFilter, (long)i, &f);
	if (!r_has_type(&f, t_name))
	    return_error(e_typecheck);
	name_string_ref(&f, &fname);
	if (r_size(&fname) < 6 ||
	    !memcmp(fname.value.bytes + r_size(&fname) - 6, "Decode", 6)
	    )
	    return_error(e_rangecheck);
	if (pDecodeParms) {
	    array_get(pDecodeParms, (long)i, &dp);
	    if (!(r_has_type(&dp, t_dictionary) || r_has_type(&dp, t_null)))
		return_error(e_typecheck);
	}
    }
    if ((code = dict_int_param(op, "Intent", 0, 3, 0, &Intent)) < 0 ||
	(code = dict_bool_param(op, "AsyncRead", false, &AsyncRead)) < 0
	)
	return code;
    push(1);
    op[-1] = *pFilter;
    if (pDecodeParms)
	*op = *pDecodeParms;
    else
	make_null(op);
    return 0;
}

/* <file|string> <length|null> <CloseSource> .reusablestream <filter> */
/*
 * The file|string operand must be a "reusable source", either:
 *      - A string;
 *      - A readable, positionable file stream;
 *      - A SubFileDecode filter with an empty EODString and a reusable
 *      source;
 *      - A reusable stream.
 */
private int make_rss(P6(os_ptr op, const byte * data, uint size, long offset,
			long length, bool close_source));
private int
zreusablestream(os_ptr op)
{
    os_ptr source_op = op - 2;
    os_ptr length_op = op - 1;
    long length;
    bool close_source;
    int code;

    if (r_has_type(length_op, t_integer)) {
	length = length_op->value.intval;
	if (length < 0)
	    return_error(e_rangecheck);
    } else
	length = -1;
    check_type(*op, t_boolean);
    close_source = op->value.boolval;
    if (r_has_type(source_op, t_string)) {
	check_read(*source_op);
	code = make_rss(source_op, source_op->value.const_bytes,
			r_size(source_op), 0L, length, close_source);
    } else {
	long offset = 0;
	stream *source;

	check_read_file(source, source_op);
rs:
	if (source->cbuf_string.data != 0) {
	    /* The data source is a string. */
	    long avail;

	    offset += stell(source);
	    savailable(source, &avail);
	    if (avail < 0)
		avail = 0;
	    if (avail > length)
		avail = length;
	    code = make_rss(source_op, source->cbuf_string.data,
			    source->cbuf_string.size, offset, avail,
			    close_source);
	} else if (source->file != 0) {
	    /* The data source is a file. */
/****** NYI ******/
	} else if (source->state->template == &s_SFD_template) {
	    /* The data source is a SubFileDecode filter. */
	    const stream_SFD_state *const sfd_state =
	    (const stream_SFD_state *)source->state;

	    if (sfd_state->eod.size != 0)
		return_error(e_rangecheck);
	    if (sfd_state->count != 0) {
		long left = sfd_state->count + sbufavailable(source);

		if (left < length)
		    length = left;
	    }
	    source = source->strm;
	    goto rs;
	}
/****** REUSABLE CASE IS NYI ******/
	else
	    return_error(e_rangecheck);
    }
    if (code >= 0)
	pop(2);
    return code;
}

/* Make a reusable string stream. */
private int
make_rss(os_ptr op, const byte * data, uint size, long offset,
	 long length, bool close_source)
{
/****** NYI ******/
    return_error(e_rangecheck);
}

/* ---------------- Initialization procedure ---------------- */

const op_def zfreuse_op_defs[] =
{
    {"2.rsdparams", zrsdparams},
    {"2.reusablestream", zreusablestream},
    op_def_end(0)
};
