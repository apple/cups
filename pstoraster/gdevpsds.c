/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpsds.c,v 1.1 2000/03/08 23:14:25 mike Exp $ */
/* Image processing streams for PostScript and PDF writers */
#include "gx.h"
#include "memory_.h"
#include "gserrors.h"
#include "gxdcconv.h"
#include "gdevpsds.h"

/* ---------------- Convert between 1/2/4 and 8 bits ---------------- */
gs_private_st_simple(st_1248_state, stream_1248_state, "stream_1248_state");

/* Initialize the state. */
private int
s_1_init(stream_state * st)
{
    stream_1248_state *const ss = (stream_1248_state *) st;

    ss->left = ss->samples_per_row;
    ss->bits_per_sample = 1;
    return 0;
}
private int
s_2_init(stream_state * st)
{
    stream_1248_state *const ss = (stream_1248_state *) st;

    ss->left = ss->samples_per_row;
    ss->bits_per_sample = 2;
    return 0;
}
private int
s_4_init(stream_state * st)
{
    stream_1248_state *const ss = (stream_1248_state *) st;

    ss->left = ss->samples_per_row;
    ss->bits_per_sample = 4;
    return 0;
}

/* Process one buffer. */
#define BEGIN_1248\
	stream_1248_state * const ss = (stream_1248_state *)st;\
	const byte *p = pr->ptr;\
	const byte *rlimit = pr->limit;\
	byte *q = pw->ptr;\
	byte *wlimit = pw->limit;\
	uint left = ss->left;\
	int status;\
	int n
#define END_1248\
	pr->ptr = p;\
	pw->ptr = q;\
	ss->left = left;\
	return status

/* N-to-8 expansion */
#define FOREACH_N_8(in, nout)\
	status = 0;\
	for ( ; p < rlimit; left -= n, q += n, ++p ) {\
	  byte in = p[1];\
	  n = min(left, nout);\
	  if ( wlimit - q < n ) {\
	    status = 1;\
	    break;\
	  }\
	  switch ( n ) {\
	    case 0: left = ss->samples_per_row; --p; continue;
#define END_FOREACH_N_8\
	  }\
	}
private int
s_N_8_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    BEGIN_1248;

    switch (ss->bits_per_sample) {

	case 1:{
		FOREACH_N_8(in, 8)
	case 8:
		q[8] = (byte) - (in & 1);
	case 7:
		q[7] = (byte) - ((in >> 1) & 1);
	case 6:
		q[6] = (byte) - ((in >> 2) & 1);
	case 5:
		q[5] = (byte) - ((in >> 3) & 1);
	case 4:
		q[4] = (byte) - ((in >> 4) & 1);
	case 3:
		q[3] = (byte) - ((in >> 5) & 1);
	case 2:
		q[2] = (byte) - ((in >> 6) & 1);
	case 1:
		q[1] = (byte) - (in >> 7);
		END_FOREACH_N_8;
	    }
	    break;

	case 2:{
		static const byte b2[4] =
		{0x00, 0x55, 0xaa, 0xff};

		FOREACH_N_8(in, 4)
	case 4:
		q[4] = b2[in & 3];
	case 3:
		q[3] = b2[(in >> 2) & 3];
	case 2:
		q[2] = b2[(in >> 4) & 3];
	case 1:
		q[1] = b2[in >> 6];
		END_FOREACH_N_8;
	    }
	    break;

	case 4:{
		static const byte b4[16] =
		{
		    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
		};

		FOREACH_N_8(in, 2)
	case 2:
		q[2] = b4[in & 0xf];
	case 1:
		q[1] = b4[in >> 4];
		END_FOREACH_N_8;
	    }
	    break;

	default:
	    return ERRC;
    }

    END_1248;
}

/* 8-to-N reduction */
#define FOREACH_8_N(out, nin)\
	byte out;\
	status = 1;\
	for ( ; q < wlimit; left -= n, p += n, ++q ) {\
	  n = min(left, nin);\
	  if ( rlimit - p < n ) {\
	    status = 0;\
	    break;\
	  }\
	  out = 0;\
	  switch ( n ) {\
	    case 0: left = ss->samples_per_row; --q; continue;
#define END_FOREACH_8_N\
	    q[1] = out;\
	  }\
	}
private int
s_8_N_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    BEGIN_1248;

    switch (ss->bits_per_sample) {

	case 1:{
		FOREACH_8_N(out, 8)
	case 8:
		out = p[8] >> 7;
	case 7:
		out |= (p[7] >> 7) << 1;
	case 6:
		out |= (p[6] >> 7) << 2;
	case 5:
		out |= (p[5] >> 7) << 3;
	case 4:
		out |= (p[4] >> 7) << 4;
	case 3:
		out |= (p[3] >> 7) << 5;
	case 2:
		out |= (p[2] >> 7) << 6;
	case 1:
		out |= p[1] & 0x80;
		END_FOREACH_8_N;
	    }
	    break;

	case 2:{
		FOREACH_8_N(out, 4)
	case 4:
		out |= p[4] >> 6;
	case 3:
		out |= (p[3] >> 6) << 2;
	case 2:
		out |= (p[2] >> 6) << 4;
	case 1:
		out |= p[1] & 0xc0;
		END_FOREACH_8_N;
	    }
	    break;

	case 4:{
		FOREACH_8_N(out, 2)
	case 2:
		out |= p[2] >> 4;
	case 1:
		out |= p[1] & 0xf0;
		END_FOREACH_8_N;
	    }
	    break;

	default:
	    return ERRC;
    }

    END_1248;
}

const stream_template s_1_8_template =
{
    &st_1248_state, s_1_init, s_N_8_process, 1, 8
};
const stream_template s_2_8_template =
{
    &st_1248_state, s_2_init, s_N_8_process, 1, 4
};
const stream_template s_4_8_template =
{
    &st_1248_state, s_4_init, s_N_8_process, 1, 2
};

const stream_template s_8_1_template =
{
    &st_1248_state, s_1_init, s_8_N_process, 8, 1
};
const stream_template s_8_2_template =
{
    &st_1248_state, s_2_init, s_8_N_process, 4, 1
};
const stream_template s_8_4_template =
{
    &st_1248_state, s_4_init, s_8_N_process, 2, 1
};

/* ---------------- CMYK => RGB conversion ---------------- */

private_st_C2R_state();

/* Process one buffer. */
private int
s_C2R_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_C2R_state *const ss = (stream_C2R_state *) st;
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    byte *q = pw->ptr;
    byte *wlimit = pw->limit;

    for (; rlimit - p >= 4 && wlimit - q >= 3; p += 4, q += 3) {
	byte bc = p[1], bm = p[2], by = p[3], bk = p[4];
	frac rgb[3];

	color_cmyk_to_rgb(byte2frac(bc), byte2frac(bm), byte2frac(by),
			  byte2frac(bk), ss->pis, rgb);
	q[1] = frac2byte(rgb[0]);
	q[2] = frac2byte(rgb[1]);
	q[3] = frac2byte(rgb[2]);
    }
    pr->ptr = p;
    pw->ptr = q;
    return (rlimit - p < 4 ? 0 : 1);
}

const stream_template s_C2R_template =
{
    &st_C2R_state, 0 /*NULL */ , s_C2R_process, 4, 3
};

/* ---------------- Downsampling ---------------- */

private void
s_Downsample_set_defaults(register stream_state * st)
{
    stream_Downsample_state *const ss =
	(stream_Downsample_state *) st;

    s_Downsample_set_defaults_inline(ss);
}

/* Subsample */
/****** DOESN'T IMPLEMENT padY YET ******/

gs_private_st_simple(st_Subsample_state, stream_Subsample_state,
		     "stream_Subsample_state");

/* Initialize the state. */
private int
s_Subsample_init(stream_state * st)
{
    stream_Subsample_state *const ss = (stream_Subsample_state *) st;

    ss->x = ss->y = 0;
    return 0;
}

/* Process one buffer. */
private int
s_Subsample_process(stream_state * st, stream_cursor_read * pr,
		    stream_cursor_write * pw, bool last)
{
    stream_Subsample_state *const ss = (stream_Subsample_state *) st;
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    int spp = ss->Colors;
    int width = ss->Columns;
    int xf = ss->XFactor, yf = ss->YFactor;
    int xf2 = xf / 2, yf2 = yf / 2;
    int xlimit = (width / xf) * xf;
    int xlast = (ss->padX && xlimit < width ? xlimit + (width % xf) / 2 : -1);
    int x = ss->x, y = ss->y;
    int status = 0;

    for (; rlimit - p >= spp; p += spp) {
	if (y == yf2 && ((x % xf == xf2 && x < xlimit) || x == xlast)) {
	    if (wlimit - q < spp) {
		status = 1;
		break;
	    }
	    memcpy(q + 1, p + 1, spp);
	    q += spp;
	}
	if (++x == width) {
	    x = 0;
	    if (++y == yf) {
		y = 0;
	    }
	}
    }
    pr->ptr = p;
    pw->ptr = q;
    ss->x = x, ss->y = y;
    return status;
}

const stream_template s_Subsample_template =
{
    &st_Subsample_state, s_Subsample_init, s_Subsample_process, 4, 4,
    0 /* NULL */, s_Downsample_set_defaults
};

/* Average */

private_st_Average_state();

/* Initialize the state. */
private int
s_Average_init(stream_state * st)
{
    stream_Average_state *const ss = (stream_Average_state *) st;

    ss->sum_size =
	ss->Colors * ((ss->Columns + ss->XFactor - 1) / ss->XFactor);
    ss->copy_size = ss->sum_size -
	(ss->padX || (ss->Columns % ss->XFactor == 0) ? 0 : ss->Colors);
    ss->sums =
	(uint *)gs_alloc_byte_array(st->memory, ss->sum_size,
				    sizeof(uint), "Average sums");
    if (ss->sums == 0)
	return ERRC;	/****** WRONG ******/
    memset(ss->sums, 0, ss->sum_size * sizeof(uint));
    return s_Subsample_init(st);
}

/* Release the state. */
private void
s_Average_release(stream_state * st)
{
    stream_Average_state *const ss = (stream_Average_state *) st;

    gs_free_object(st->memory, ss->sums, "Average sums");
}

/* Process one buffer. */
private int
s_Average_process(stream_state * st, stream_cursor_read * pr,
		  stream_cursor_write * pw, bool last)
{
    stream_Average_state *const ss = (stream_Average_state *) st;
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    byte *q = pw->ptr;
    byte *wlimit = pw->limit;
    int spp = ss->Colors;
    int width = ss->Columns;
    int xf = ss->XFactor, yf = ss->YFactor;
    int x = ss->x, y = ss->y;
    uint *sums = ss->sums;
    int status = 0;

top:
    if (y == yf || (last && p >= rlimit && ss->padY && y != 0)) {
	/* We're copying averaged values to the output. */
	int ncopy = min(ss->copy_size - x, wlimit - q);

	if (ncopy) {
	    int scale = xf * y;

	    while (--ncopy >= 0)
		*++q = (byte) (sums[x++] / scale);
	}
	if (x < ss->copy_size) {
	    status = 1;
	    goto out;
	}
	/* Done copying. */
	x = y = 0;
	memset(sums, 0, ss->sum_size * sizeof(uint));
    }
    while (rlimit - p >= spp) {
	uint *bp = sums + x / xf * spp;
	int i;

	for (i = spp; --i >= 0;)
	    *bp++ += *++p;
	if (++x == width) {
	    x = 0;
	    ++y;
	    goto top;
	}
    }
out:
    pr->ptr = p;
    pw->ptr = q;
    ss->x = x, ss->y = y;
    return status;
}

const stream_template s_Average_template =
{
    &st_Average_state, s_Average_init, s_Average_process, 4, 4,
    s_Average_release, s_Downsample_set_defaults
};
