/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: spngp.c,v 1.2 2000/03/08 23:15:27 mike Exp $ */
/* PNG pixel prediction filters */
#include "memory_.h"
#include "strimpl.h"
#include "spngpx.h"

/* ------ PNGPredictorEncode/Decode ------ */

private_st_PNGP_state();

/* Define values for case dispatch. */
#define cNone 10
#define cSub 11
#define cUp 12
#define cAverage 13
#define cPaeth 14
#define cOptimum 15
#define cEncode -10
#define cDecode -4
private const byte pngp_case_needs_prev[] =
{0, 0, 1, 1, 1, 1};

/* Set defaults */
private void
s_PNGP_set_defaults(stream_state * st)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;

    s_PNGP_set_defaults_inline(ss);
}

/* Common (re)initialization. */
private int
s_PNGP_reinit(stream_state * st)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;

    if (ss->prev_row != 0)
	memset(ss->prev_row + ss->bpp, 0, ss->row_count);
    ss->row_left = 0;
    return 0;
}

/* Initialize PNGPredictorEncode filter. */
private int
s_pngp_init(stream_state * st, bool need_prev)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;
    int bits_per_pixel = ss->Colors * ss->BitsPerComponent;
    long bits_per_row = (long)bits_per_pixel * ss->Columns;
    byte *prev_row = 0;

#if arch_sizeof_long > arch_sizeof_int
    if (bits_per_row > max_uint * 7L)
	return ERRC;
/****** WRONG ******/
#endif
    ss->row_count = (uint) ((bits_per_row + 7) >> 3);
    ss->end_mask = (1 << (-bits_per_row & 7)) - 1;
    ss->bpp = (bits_per_pixel + 7) >> 3;
    if (need_prev) {
	prev_row = gs_alloc_bytes(st->memory, ss->bpp + ss->row_count,
				  "PNGPredictor prev row");
	if (prev_row == 0)
	    return ERRC;
/****** WRONG ******/
	memset(prev_row, 0, ss->bpp);
    }
    ss->prev_row = prev_row;
    /* case_index is only preset for encoding */
    return s_PNGP_reinit(st);
}

/* Initialize PNGPredictorEncode filter. */
private int
s_PNGPE_init(stream_state * st)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;

    return s_pngp_init(st, pngp_case_needs_prev[ss->Predictor - cNone]);
}

/* Initialize PNGPredictorDecode filter. */
private int
s_PNGPD_init(stream_state * st)
{
    return s_pngp_init(st, true);
}

/*
 * Process a partial buffer.  We pass in current and previous pointers
 * to both the current and preceding scan line.  Note that dprev is
 * p - bpp for encoding, q - bpp for decoding; similarly, the 'up' row
 * is the raw data for encoding, the filtered data for decoding.
 * Note also that the case_index cannot be cOptimum.
 */
private int
paeth_predictor(int a, int b, int c)
{
#undef any_abs			/* just in case */
#define any_abs(u) ((u) < 0 ? -(u) : (u))
    int px = a + b - c;
    int pa = any_abs(px - a), pb = any_abs(px - b), pc = any_abs(px - c);

    return (pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
#undef any_abs
}
private void
s_pngp_process(stream_state * st, stream_cursor_write * pw,
	       const byte * dprev, stream_cursor_read * pr,
	       const byte * upprev, const byte * up, uint count)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;
    byte *q = pw->ptr + 1;
    const byte *p = pr->ptr + 1;

    pr->ptr += count;
    pw->ptr += count;
    ss->row_left -= count;
    switch (ss->case_index) {
	case cEncode + cNone:
	case cDecode + cNone:
	    memcpy(q, p, count);
	    break;
	case cEncode + cSub:
	    for (; count; ++q, ++dprev, ++p, --count)
		*q = (byte) (*p - *dprev);
	    break;
	case cDecode + cSub:
	    for (; count; ++q, ++dprev, ++p, --count)
		*q = (byte) (*p + *dprev);
	    break;
	case cEncode + cUp:
	    for (; count; ++q, ++up, ++p, --count)
		*q = (byte) (*p - *up);
	    break;
	case cDecode + cUp:
	    for (; count; ++q, ++up, ++p, --count)
		*q = (byte) (*p + *up);
	    break;
	case cEncode + cAverage:
	    for (; count; ++q, ++dprev, ++up, ++p, --count)
		*q = (byte) (*p - arith_rshift_1((int)*dprev + (int)*up));
	    break;
	case cDecode + cAverage:
	    for (; count; ++q, ++dprev, ++up, ++p, --count)
		*q = (byte) (*p + arith_rshift_1((int)*dprev + (int)*up));
	    break;
	case cEncode + cPaeth:
	    for (; count; ++q, ++dprev, ++up, ++upprev, ++p, --count)
		*q = (byte) (*p - paeth_predictor(*dprev, *up, *upprev));
	    break;
	case cDecode + cPaeth:
	    for (; count; ++q, ++dprev, ++up, ++upprev, ++p, --count)
		*q = (byte) (*p + paeth_predictor(*dprev, *up, *upprev));
	    break;
    }
}

/* Calculate the number of bytes for the next processing step, */
/* the min of (input data, output data, remaining row length). */
private uint
s_pngp_count(const stream_state * st_const, const stream_cursor_read * pr,
	     const stream_cursor_write * pw)
{
    const stream_PNGP_state *const ss_const =
    (const stream_PNGP_state *)st_const;
    uint rcount = pr->limit - pr->ptr;
    uint wcount = pw->limit - pw->ptr;
    uint row_left = ss_const->row_left;

    if (rcount < row_left)
	row_left = rcount;
    if (wcount < row_left)
	row_left = wcount;
    return row_left;
}

/*
 * Encode a buffer.  Let N = ss->row_count, P = N - ss->row_left,
 * and B = ss->bpp.  Consider that bytes [-B .. -1] of every row are zero.
 * Then:
 *      prev_row[0 .. P - 1] contain bytes -B .. P - B - 1
 *        of the current input row.
 *      ss->prev[0 .. B - 1] contain bytes P - B .. P - 1
 *        of the current input row.
 *      prev_row[P .. N + B - 1] contain bytes P - B .. N - 1
 *        of the previous input row.
 */
private int
optimum_predictor(const stream_state * st, const stream_cursor_read * pr)
{
    return cSub;
}
private int
s_PNGPE_process(stream_state * st, stream_cursor_read * pr,
		stream_cursor_write * pw, bool last)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;
    int bpp = ss->bpp;
    int code = 0;

    while (pr->ptr < pr->limit) {
	uint count;

	if (ss->row_left == 0) {	/* Beginning of row, write algorithm byte. */
	    int predictor;

	    if (pw->ptr >= pw->limit) {
		code = 1;
		break;
	    }
	    predictor =
		(ss->Predictor == cOptimum ?
		 optimum_predictor(st, pr) :
		 ss->Predictor);
	    *++(pw->ptr) = (byte) predictor - cNone;
	    ss->case_index = predictor + cEncode;
	    ss->row_left = ss->row_count;
	    memset(ss->prev, 0, bpp);
	    continue;
	}
	count = s_pngp_count(st, pr, pw);
	if (count == 0) {	/* We know we have input, so output must be full. */
	    code = 1;
	    break;
	} {
	    byte *up = ss->prev_row + bpp + ss->row_count - ss->row_left;
	    uint n = min(count, bpp);

	    /* Process bytes whose predecessors are in prev. */
	    s_pngp_process(st, pw, ss->prev, pr, up - bpp, up, n);
	    if (ss->prev_row)
		memcpy(up - bpp, ss->prev, n);
	    if (n < bpp) {	/* We didn't have enough data to use up all of prev. */
		/* Shift more data into prev and exit. */
		int prev_left = bpp - n;

		memmove(ss->prev, ss->prev + n, prev_left);
		memcpy(ss->prev + prev_left, pr->ptr - (n - 1), n);
		break;
	    }
	    /* Process bytes whose predecessors are in the input. */
	    /* We know we have at least bpp input and output bytes, */
	    /* and that n = bpp. */
	    count -= bpp;
	    s_pngp_process(st, pw, pr->ptr - (bpp - 1), pr,
			   up, up + bpp, count);
	    memcpy(ss->prev, pr->ptr - (bpp - 1), bpp);
	    if (ss->prev_row) {
		memcpy(up, pr->ptr - (bpp + count - 1), count);
		if (ss->row_left == 0)
		    memcpy(up + count, ss->prev, bpp);
	    }
	}
    }
    return code;
}

/*
 * Decode a buffer.  Let N = ss->row_count, P = N - ss->row_left,
 * and B = ss->bpp.  Consider that bytes [-B .. -1] of every row are zero.
 * Then:
 *      prev_row[0 .. P - 1] contain bytes -B .. P - B - 1
 *        of the current output row.
 *      ss->prev[0 .. B - 1] contain bytes P - B .. P - 1
 *        of the current output row.
 *      prev_row[P .. N + B - 1] contain bytes P - B .. N - 1
 *        of the previous output row.
 */
private int
s_PNGPD_process(stream_state * st, stream_cursor_read * pr,
		stream_cursor_write * pw, bool last)
{
    stream_PNGP_state *const ss = (stream_PNGP_state *) st;
    int bpp = ss->bpp;
    int code = 0;

    while (pr->ptr < pr->limit) {
	uint count;

	if (ss->row_left == 0) {	/* Beginning of row, read algorithm byte. */
	    int predictor = pr->ptr[1];

	    if (predictor >= cOptimum - cNone) {
		code = ERRC;
		break;
	    }
	    pr->ptr++;
	    ss->case_index = predictor + cNone + cDecode;
	    ss->row_left = ss->row_count;
	    memset(ss->prev, 0, bpp);
	    continue;
	}
	count = s_pngp_count(st, pr, pw);
	if (count == 0) {	/* We know we have input, so output must be full. */
	    code = 1;
	    break;
	} {
	    byte *up = ss->prev_row + bpp + ss->row_count - ss->row_left;
	    uint n = min(count, bpp);

	    /* Process bytes whose predecessors are in prev. */
	    s_pngp_process(st, pw, ss->prev, pr, up - bpp, up, n);
	    if (ss->prev_row)
		memcpy(up - bpp, ss->prev, n);
	    if (n < bpp) {	/* We didn't have enough data to use up all of prev. */
		/* Shift more data into prev and exit. */
		int prev_left = bpp - n;

		memmove(ss->prev, ss->prev + n, prev_left);
		memcpy(ss->prev + prev_left, pw->ptr - (n - 1), n);
		break;
	    }
	    /* Process bytes whose predecessors are in the output. */
	    /* We know we have at least bpp input and output bytes, */
	    /* and that n = bpp. */
	    count -= bpp;
	    s_pngp_process(st, pw, pw->ptr - (bpp - 1), pr,
			   up, up + bpp, count);
	    memcpy(ss->prev, pw->ptr - (bpp - 1), bpp);
	    if (ss->prev_row) {
		memcpy(up, pw->ptr - (bpp + count - 1), count);
		if (ss->row_left == 0)
		    memcpy(up + count, ss->prev, bpp);
	    }
	}
    }
    return code;
}

/* Stream templates */
const stream_template s_PNGPE_template =
{&st_PNGP_state, s_PNGPE_init, s_PNGPE_process, 1, 1, 0 /*NULL */ ,
 s_PNGP_set_defaults, s_PNGP_reinit
};
const stream_template s_PNGPD_template =
{&st_PNGP_state, s_PNGPD_init, s_PNGPD_process, 1, 1, 0 /*NULL */ ,
 s_PNGP_set_defaults, s_PNGP_reinit
};
