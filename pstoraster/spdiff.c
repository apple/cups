/* Copyright (C) 1994, 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: spdiff.c,v 1.2 2000/03/08 23:15:27 mike Exp $ */
/* Pixel differencing filters */
#include "stdio_.h"		/* should be std.h, but needs NULL */
#include "strimpl.h"
#include "spdiffx.h"

/* ------ PixelDifferenceEncode/Decode ------ */

private_st_PDiff_state();

/* Define values for case dispatch. */
#define cBits1 0
#define cBits2 4
#define cBits4 8
#define cBits8 12
#define cEncode -1
#define cDecode 15

/* Set defaults */
private void
s_PDiff_set_defaults(stream_state * st)
{
    stream_PDiff_state *const ss = (stream_PDiff_state *) st;

    s_PDiff_set_defaults_inline(ss);
}

/* Common (re)initialization. */
private int
s_PDiff_reinit(stream_state * st)
{
    stream_PDiff_state *const ss = (stream_PDiff_state *) st;

    ss->row_left = 0;
    return 0;
}

/* Initialize PixelDifferenceEncode filter. */
private int
s_PDiffE_init(stream_state * st)
{
    stream_PDiff_state *const ss = (stream_PDiff_state *) st;
    long bits_per_row =
    ss->Colors * ss->BitsPerComponent * (long)ss->Columns;
    static const byte cb_values[] =
    {0, cBits1, cBits2, 0, cBits4, 0, 0, 0, cBits8};

    ss->row_count = (uint) ((bits_per_row + 7) >> 3);
    ss->end_mask = (1 << (-bits_per_row & 7)) - 1;
    ss->case_index =
	cb_values[ss->BitsPerComponent] + ss->Colors + cEncode;
    return s_PDiff_reinit(st);
}

/* Initialize PixelDifferenceDecode filter. */
private int
s_PDiffD_init(stream_state * st)
{
    stream_PDiff_state *const ss = (stream_PDiff_state *) st;

    s_PDiffE_init(st);
    ss->case_index += cDecode - cEncode;
    return 0;
}

/* Process a buffer.  Note that this handles both Encode and Decode. */
private int
s_PDiff_process(stream_state * st, stream_cursor_read * pr,
		stream_cursor_write * pw, bool last)
{
    stream_PDiff_state *const ss = (stream_PDiff_state *) st;
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    int rcount, wcount;
    register int count;
    int status = 0;
    register byte s0 = ss->s0;
    register byte t;
    byte save_last;
    const byte end_mask = ss->end_mask;

  row:if (ss->row_left == 0)
	ss->row_left = ss->row_count,
	    s0 = ss->s1 = ss->s2 = ss->s3 = 0;
    rcount = pr->limit - p;
    wcount = pw->limit - q;
    if (ss->row_left < rcount)
	rcount = ss->row_left;
    count = (wcount < rcount ? (status = 1, wcount) : rcount);
    ss->row_left -= count;
    if (ss->row_left == count)
	save_last = p[count];

    /*
     * Encoding and decoding are fundamentally different.
     * Encoding computes E[i] = D[i] - D[i-1];
     * decoding computes D[i] = E[i] + D[i-1].
     * Nevertheless, the loop structures are similar enough that
     * we put the code for both functions in the same place.
     */

#define loopn(n, body)\
  while ( count >= n ) p += n, q += n, body, count -= n

    switch (ss->case_index) {

	    /* 1 bit per component */

#define eloop1(ee)\
  loopn(1, (t = *p, *q = ee, s0 = t))

	case cEncode + cBits1 + 1:
	    eloop1(t ^ ((s0 << 7) | (t >> 1)));
	case cEncode + cBits1 + 2:
	    eloop1(t ^ ((s0 << 6) | (t >> 2)));
	case cEncode + cBits1 + 3:
	    eloop1(t ^ ((s0 << 5) | (t >> 3)));
	case cEncode + cBits1 + 4:
	    eloop1(t ^ ((s0 << 4) | (t >> 4)));

#define dloop1(te, de)\
  loopn(1, (t = te, s0 = *q = de)); break

	case cDecode + cBits1 + 1:
	    dloop1(*p ^ (s0 << 7),
		   (t ^= t >> 1, t ^= t >> 2, t ^ (t >> 4)));
	case cDecode + cBits1 + 2:
	    dloop1(*p ^ (s0 << 6),
		   (t ^= (t >> 2), t ^ (t >> 4)));
	case cDecode + cBits1 + 3:
	    dloop1(*p ^ (s0 << 5),
		   t ^ (t >> 3) ^ (t >> 6));
	case cDecode + cBits1 + 4:
	    dloop1(*p ^ (s0 << 4),
		   t ^ (t >> 4));

	    /* 2 bits per component */

#define add4x2(a, b) ( (((a) & (b) & 0x55) << 1) ^ (a) ^ (b) )
#define sub4x2(a, b) ( ((~(a) & (b) & 0x55) << 1) ^ (a) ^ (b) )

	case cEncode + cBits2 + 1:
	    eloop1((s0 = (s0 << 6) | (t >> 2), sub4x2(t, s0)));
	case cEncode + cBits2 + 2:
	    eloop1((s0 = (s0 << 4) | (t >> 4), sub4x2(t, s0)));
	case cEncode + cBits2 + 3:
	    eloop1((s0 = (s0 << 2) | (t >> 6), sub4x2(t, s0)));
	case cEncode + cBits2 + 4:
	    eloop1(sub4x2(t, s0));

	case cDecode + cBits2 + 1:
	    dloop1(*p + (s0 << 6),
		   (t = add4x2(t >> 2, t),
		    add4x2(t >> 4, t)));
	case cDecode + cBits2 + 2:
	    dloop1(*p, (t = add4x2(t, s0 << 4),
			add4x2(t >> 4, t)));
	case cDecode + cBits2 + 3:
	    dloop1(*p, (t = add4x2(t, s0 << 2),
			add4x2(t >> 6, t)));
	case cDecode + cBits2 + 4:
	    dloop1(*p, add4x2(t, s0));

#undef add4x2
#undef sub4x2

	    /* 4 bits per component */

#define add2x4(a, b) ( (((a) + (b)) & 0xf) + ((a) & 0xf0) + ((b) & 0xf0) )
#define add2x4r4(a) ( (((a) + ((a) >> 4)) & 0xf) + ((a) & 0xf0) )
#define sub2x4(a, b) ( (((a) - (b)) & 0xf) + ((a) & 0xf0) - ((b) & 0xf0) )
#define sub2x4r4(a) ( (((a) - ((a) >> 4)) & 0xf) + ((a) & 0xf0) )

	case cEncode + cBits4 + 1:
	    eloop1(((t - (s0 << 4)) & 0xf0) | ((t - (t >> 4)) & 0xf));
	case cEncode + cBits4 + 2:
	    eloop1(sub2x4(t, s0));
	case cEncode + cBits4 + 3:
	    {
		register byte s1 = ss->s1;

		loopn(1, (t = *p,
			  *q =
			  ((t - (s0 << 4)) & 0xf0) | ((t - (s1 >> 4)) & 0xf),
			  s0 = s1, s1 = t));
		ss->s1 = s1;
	    } break;
	case cEncode + cBits4 + 4:
	    {
		register byte s1 = ss->s1;

		loopn(2,
		      (t = p[-1], q[-1] = sub2x4(t, s0), s0 = t,
		       t = *p, *q = sub2x4(t, s1), s1 = t));
		ss->s1 = s1;
	    } break;

	case cDecode + cBits4 + 1:
	    dloop1(*p + (s0 << 4), add2x4r4(t));
	case cDecode + cBits4 + 2:
	    dloop1(*p, add2x4(t, s0));
	case cDecode + cBits4 + 3:
	    {
		register byte s1 = ss->s1;

		loopn(1, (t = (s0 << 4) + (s1 >> 4),
			  s0 = s1, s1 = *q = add2x4(*p, t)));
		ss->s1 = s1;
	    } break;
	case cDecode + cBits4 + 4:
	    {
		register byte s1 = ss->s1;

		loopn(2,
		      (t = p[-1], s0 = q[-1] = add2x4(s0, t),
		       t = *p, s1 = *q = add2x4(s1, t)));
		ss->s1 = s1;
	    } break;

#undef add2x4
#undef add2x4r4
#undef sub2x4
#undef sub2x4r4

	    /* 8 bits per component */

#define encode8(s, d) (q[d] = p[d] - s, s = p[d])
#define decode8(s, d) s = q[d] = s + p[d]

	case cEncode + cBits8 + 1:
	    loopn(1, encode8(s0, 0));
	    break;
	case cDecode + cBits8 + 1:
	    loopn(1, decode8(s0, 0));
	    break;
	case cEncode + cBits8 + 2:
	    {
		register byte s1 = ss->s1;

		loopn(2, (encode8(s0, -1), encode8(s1, 0)));
		ss->s1 = s1;
	    } break;
	case cDecode + cBits8 + 2:
	    {
		register byte s1 = ss->s1;

		loopn(2, (decode8(s0, -1), decode8(s1, 0)));
		ss->s1 = s1;
	    } break;
	case cEncode + cBits8 + 3:
	    {
		register byte s1 = ss->s1, s2 = ss->s2;

		loopn(3, (encode8(s0, -2), encode8(s1, -1),
			  encode8(s2, 0)));
		ss->s1 = s1, ss->s2 = s2;
	    } break;
	case cDecode + cBits8 + 3:
	    {
		register byte s1 = ss->s1, s2 = ss->s2;

		loopn(3, (decode8(s0, -2), decode8(s1, -1),
			  decode8(s2, 0)));
		ss->s1 = s1, ss->s2 = s2;
	    } break;
	case cEncode + cBits8 + 4:
	    {
		register byte s1 = ss->s1, s2 = ss->s2, s3 = ss->s3;

		loopn(4, (encode8(s0, -3), encode8(s1, -2),
			  encode8(s2, -1), encode8(s3, 0)));
		ss->s1 = s1, ss->s2 = s2, ss->s3 = s3;
	    } break;
	case cDecode + cBits8 + 4:
	    {
		register byte s1 = ss->s1, s2 = ss->s2, s3 = ss->s3;

		loopn(4, (decode8(s0, -3), decode8(s1, -2),
			  decode8(s2, -1), decode8(s3, 0)));
		ss->s1 = s1, ss->s2 = s2, ss->s3 = s3;
	    } break;

#undef encode8
#undef decode8

    }
#undef loopn
#undef dloop1
    ss->row_left += count;	/* leftover bytes are possible */
    if (ss->row_left == 0) {
	if (end_mask != 0)
	    *q = (*q & ~end_mask) | (save_last & end_mask);
	if (p < pr->limit && q < pw->limit)
	    goto row;
    }
    ss->s0 = s0;
    pr->ptr = p;
    pw->ptr = q;
    return status;
}

/* Stream templates */
const stream_template s_PDiffE_template =
{&st_PDiff_state, s_PDiffE_init, s_PDiff_process, 1, 1, NULL,
 s_PDiff_set_defaults, s_PDiff_reinit
};
const stream_template s_PDiffD_template =
{&st_PDiff_state, s_PDiffD_init, s_PDiff_process, 1, 1, NULL,
 s_PDiff_set_defaults, s_PDiff_reinit
};
