/* Copyright (C) 1992, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: scfe.c,v 1.2 2000/03/08 23:15:22 mike Exp $ */
/* CCITTFax encoding filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "gdebug.h"
#include "strimpl.h"
#include "scf.h"
#include "scfx.h"

/* ------ Macros and support routines ------ */

/* Statistics */

#ifdef DEBUG

typedef struct stats_runs_s {
    ulong termination[64];
    ulong make_up[41];
} stats_runs_t;
private stats_runs_t stats_white_runs, stats_black_runs;

#define COUNT_RUN(tab, i) (tab)[i]++;

private void
print_run_stats(const stats_runs_t * stats)
{
    int i;
    ulong total;

    for (i = 0, total = 0; i < 41; i++)
	dprintf1(" %lu", stats->make_up[i]),
	    total += stats->make_up[i];
    dprintf1(" total=%lu\n\t", total);
    for (i = 0, total = 0; i < 64; i++)
	dprintf1(" %lu", stats->termination[i]),
	    total += stats->termination[i];
    dprintf1(" total=%lu\n", total);
}

#else /* !DEBUG */

#define COUNT_RUN(cnt, i) DO_NOTHING

#endif /* DEBUG */

/* Put a run onto the output stream. */
/* Free variables: q, bits, bits_left. */

#define CF_PUT_RUN(ss, lenv, rt, stats)\
BEGIN\
    cfe_run rr;\
\
    if ( lenv >= 64 ) {\
	hce_store_state();\
	q = cf_put_long_run(ss, q, lenv, &rt);\
	hce_load_state();\
	lenv &= 63;\
    }\
    rr = rt.termination[lenv];\
    COUNT_RUN(stats.termination, lenv);\
    hc_put_value(ss, q, rr.code, rr.code_length);\
END

private byte *
cf_put_long_run(stream_CFE_state * ss, byte * q, int lenv, const cf_runs * prt)
{
    hce_declare_state;
    cfe_run rr;

#ifdef DEBUG
    stats_runs_t *pstats =
    (prt == &cf_white_runs ? &stats_white_runs : &stats_black_runs);

#endif

    hce_load_state();
    while (lenv >= 2560 + 64) {
	rr = prt->make_up[40];
	COUNT_RUN(pstats->make_up, 40);
	hc_put_value(ss, q, rr.code, rr.code_length);
	lenv -= 2560;
    }
    rr = prt->make_up[lenv >> 6];
    COUNT_RUN(pstats->make_up, lenv >> 6);
    hc_put_value(ss, q, rr.code, rr.code_length);
    hce_store_state();
    return q;
}

#define CF_PUT_WHITE_RUN(ss, lenv)\
  CF_PUT_RUN(ss, lenv, cf_white_runs, stats_white_runs)

#define CF_PUT_BLACK_RUN(ss, lenv)\
  CF_PUT_RUN(ss, lenv, cf_black_runs, stats_black_runs)

/* ------ CCITTFaxEncode ------ */

private_st_CFE_state();

private void s_CFE_release(P1(stream_state *));

/* Set default parameter values. */
private void
s_CFE_set_defaults(register stream_state * st)
{
    stream_CFE_state *const ss = (stream_CFE_state *) st;

    s_CFE_set_defaults_inline(ss);
}

/* Initialize CCITTFaxEncode filter */
private int
s_CFE_init(register stream_state * st)
{
    stream_CFE_state *const ss = (stream_CFE_state *) st;
    int columns = ss->Columns;

    /*
     * The worst case for encoding is alternating white and black pixels.
     * For 1-D encoding, the worst case is 9 bits per 2 pixels; for 2-D
     * (horizontal), 12 bits per 2 pixels.  To fill out a scan line,
     * we may add up to 6 12-bit EOL codes.
     */
    int code_bytes =
    ((columns * (ss->K == 0 ? 9 : 12)) >> 4) + 20;	/* add slop */
    int raster = ss->raster =
    round_up((columns + 7) >> 3, ss->DecodedByteAlign);

    s_hce_init_inline(ss);
    ss->lbuf = ss->lprev = ss->lcode = 0;	/* in case we have to release */
    if (columns > cfe_max_width)
	return ERRC;
/****** WRONG ******/
    /* Because skip_white_pixels can look as many as 4 bytes ahead, */
    /* we need to allow 4 extra bytes at the end of the row buffers. */
    ss->lbuf = gs_alloc_bytes(st->memory, raster + 4, "CFE lbuf");
    ss->lcode = gs_alloc_bytes(st->memory, code_bytes, "CFE lcode");
    if (ss->lbuf == 0 || ss->lcode == 0) {
	s_CFE_release(st);
	return ERRC;
/****** WRONG ******/
    }
    if (ss->K != 0) {
	ss->lprev = gs_alloc_bytes(st->memory, raster + 4, "CFE lprev");
	if (ss->lprev == 0) {
	    s_CFE_release(st);
	    return ERRC;
/****** WRONG ******/
	}
	/* Clear the initial reference line for 2-D encoding. */
	/* Make sure it is terminated properly. */
	memset(ss->lprev, (ss->BlackIs1 ? 0 : 0xff), raster);
	if (columns & 7)
	    ss->lprev[raster - 1] ^= 0x80 >> (columns & 7);
	else
	    ss->lprev[raster] = ~ss->lprev[0];
    }
    ss->read_count = raster;
    ss->write_count = 0;
    ss->k_left = (ss->K > 0 ? 1 : ss->K);
    ss->max_code_bytes = code_bytes;
    return 0;
}

/* Release the filter. */
private void
s_CFE_release(stream_state * st)
{
    stream_CFE_state *const ss = (stream_CFE_state *) st;

    gs_free_object(st->memory, ss->lprev, "CFE lprev(close)");
    gs_free_object(st->memory, ss->lcode, "CFE lcode(close)");
    gs_free_object(st->memory, ss->lbuf, "CFE lbuf(close)");
}

/* Flush the buffer */
private void cf_encode_1d(P3(stream_CFE_state *, const byte *,
			     stream_cursor_write *));
private void cf_encode_2d(P4(stream_CFE_state *, const byte *,
			     stream_cursor_write *, const byte *));
private int
s_CFE_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_CFE_state *const ss = (stream_CFE_state *) st;
    const byte *rlimit = pr->limit;
    byte *wlimit = pw->limit;
    int raster = ss->raster;
    byte end_mask = 1 << (-ss->Columns & 7);
    int status = 0;

    for (;;) {
	stream_cursor_write w;

	if_debug2('w', "[w]CFE: read_count = %d, write_count=%d,\n",
		  ss->read_count, ss->write_count);
	if_debug6('w', "    pr = 0x%lx(%d)0x%lx, pw = 0x%lx(%d)0x%lx\n",
		  (ulong) pr->ptr, (int)(rlimit - pr->ptr), (ulong) rlimit,
		  (ulong) pw->ptr, (int)(wlimit - pw->ptr), (ulong) wlimit);
	if (ss->write_count) {
	    /* Copy more of an encoded line to the caller. */
	    int wcount = wlimit - pw->ptr;
	    int ccount = min(wcount, ss->write_count);

	    memcpy(pw->ptr + 1, ss->lcode + ss->code_bytes - ss->write_count,
		   ccount);
	    pw->ptr += ccount;
	    if ((ss->write_count -= ccount) > 0) {
		status = 1;
		break;
	    }
	}
	if (ss->read_count) {
	    /* Copy more of an unencoded line from the caller. */
	    int rcount = rlimit - pr->ptr;
	    int ccount = min(rcount, ss->read_count);

	    if (rcount == 0 && last)
		break;
	    memcpy(ss->lbuf + raster - ss->read_count,
		   pr->ptr + 1, ccount);
	    pr->ptr += ccount;
	    if ((ss->read_count -= ccount) != 0)
		break;
	}
	/*
	 * We have a full scan line in lbuf.  Ensure that it ends with
	 * two polarity changes.
	 */
	{
	    byte *end = ss->lbuf + raster - 1;
	    byte end_bit = *end & end_mask;
	    byte not_bit = end_bit ^ end_mask;

	    *end &= -end_mask;
	    if (end_mask == 1)
		end[1] = (end_bit ? 0x40 : 0x80);
	    else if (end_mask == 2)
		*end |= not_bit >> 1, end[1] = end_bit << 7;
	    else
		*end |= (not_bit >> 1) | (end_bit >> 2);
	}
	/*
	 * Write the output directly to the caller's buffer if it's large
	 * enough, otherwise to our own buffer.
	 */
	if (wlimit - pw->ptr >= ss->max_code_bytes) {
	    w = *pw;
	} else {
	    w.ptr = ss->lcode - 1;
	    w.limit = w.ptr + ss->max_code_bytes;
	}
#ifdef DEBUG
	if (ss->K > 0) {
	    if_debug1('w', "[w]new row, k_left=%d\n",
		      ss->k_left);
	} else {
	    if_debug0('w', "[w]new row\n");
	}
#endif
	/*
	 * Write an EOL (actually a "beginning of line") if requested.
	 */
	if (ss->EndOfLine) {
	    const cfe_run *rp =
	    (ss->K <= 0 ? &cf_run_eol :
	     ss->k_left > 1 ? &cf2_run_eol_2d :
	     &cf2_run_eol_1d);
	    cfe_run run;

	    hce_declare_state;

	    hce_load_state();
	    if (ss->EncodedByteAlign) {
		run = *rp;
		/* Pad the run on the left */
		/* so it winds up byte-aligned. */
		run.code_length +=
		    (bits_left - run_eol_code_length) & 7;
		if (run.code_length > 16)	/* <= 23 */
		    bits_left -= run.code_length & 7,
			run.code_length = 16;
		rp = &run;
	    }
	    hc_put_code(ss, w.ptr, rp);
	    hce_store_state();
	} else if (ss->EncodedByteAlign)
	    ss->bits_left &= ~7;
	/* Encode the line. */
	if (ss->K == 0)
	    cf_encode_1d(ss, ss->lbuf, &w);	/* pure 1-D */
	else if (ss->K < 0)
	    cf_encode_2d(ss, ss->lbuf, &w, ss->lprev);	/* pure 2-D */
	else if (--(ss->k_left))	/* mixed, use 2-D */
	    cf_encode_2d(ss, ss->lbuf, &w, ss->lprev);
	else {			/* mixed, use 1-D */
	    cf_encode_1d(ss, ss->lbuf, &w);
	    ss->k_left = ss->K;
	}
	/*
	 * If we didn't write directly to the client's buffer, schedule
	 * the output data to be written.
	 */
	if (w.limit == wlimit)
	    pw->ptr = w.ptr;
	else
	    ss->write_count = ss->code_bytes = w.ptr - (ss->lcode - 1);
	if (ss->K != 0) {
	    /* In 2-D modes, swap the current and previous scan lines. */
	    byte *temp = ss->lbuf;

	    ss->lbuf = ss->lprev;
	    ss->lprev = temp;
	}
	/* Note that the input buffer needs refilling. */
	ss->read_count = raster;
    }
    /*
     * When we exit from the loop, we know that write_count = 0, and
     * there is no line waiting to be processed in the input buffer.
     */
    if (last && status == 0) {
	const cfe_run *rp =
	(ss->K > 0 ? &cf2_run_eol_1d : &cf_run_eol);
	int i = (!ss->EndOfBlock ? 0 : ss->K < 0 ? 2 : 6);
	uint bits_to_write =
	hc_bits_size - ss->bits_left + i * rp->code_length;
	byte *q = pw->ptr;

	hce_declare_state;

	if (wlimit - q < (bits_to_write + 7) >> 3) {
	    status = 1;
	    goto out;
	}
	hce_load_state();
	if (ss->EncodedByteAlign)
	    bits_left &= ~7;
	while (--i >= 0)
	    hc_put_code(ss, q, rp);
	/* Force out the last byte or bytes. */
	pw->ptr = hc_put_last_bits((stream_hc_state *) ss, q);
    }
  out:
    if_debug9('w', "[w]CFE exit %d: read_count = %d, write_count = %d,\n     pr = 0x%lx(%d)0x%lx; pw = 0x%lx(%d)0x%lx\n",
	      status, ss->read_count, ss->write_count,
	      (ulong) pr->ptr, (int)(rlimit - pr->ptr), (ulong) rlimit,
	      (ulong) pw->ptr, (int)(wlimit - pw->ptr), (ulong) wlimit);
#ifdef DEBUG
    if (pr->ptr > rlimit || pw->ptr > wlimit) {
	lprintf("Pointer overrun!\n");
	status = ERRC;
    }
    if (gs_debug_c('w') && status == 1) {
	dlputs("[w]white runs:");
	print_run_stats(&stats_white_runs);
	dlputs("[w]black runs:");
	print_run_stats(&stats_black_runs);
    }
#endif
    return status;
}

/* Encode a 1-D scan line. */
private void
cf_encode_1d(stream_CFE_state * ss, const byte * lbuf, stream_cursor_write * pw)
{
    uint count = ss->raster << 3;
    byte *q = pw->ptr;
    int end_count = -ss->Columns & 7;
    int rlen;

    hce_declare_state;
    const byte *p = lbuf;
    byte invert = (ss->BlackIs1 ? 0 : 0xff);

    /* Invariant: data = p[-1] ^ invert. */
    uint data = *p++ ^ invert;

    hce_load_state();
    while (count != end_count) {
	/* Parse a white run. */
	skip_white_pixels(data, p, count, invert, rlen);
	CF_PUT_WHITE_RUN(ss, rlen);
	if (count == end_count)
	    break;
	/* Parse a black run. */
	skip_black_pixels(data, p, count, invert, rlen);
	CF_PUT_BLACK_RUN(ss, rlen);
    }
    hce_store_state();
    pw->ptr = q;
}

/* Encode a 2-D scan line. */
private void
cf_encode_2d(stream_CFE_state * ss, const byte * lbuf, stream_cursor_write * pw,
	     const byte * lprev)
{
    byte invert_white = (ss->BlackIs1 ? 0 : 0xff);
    byte invert = invert_white;
    uint count = ss->raster << 3;
    int end_count = -ss->Columns & 7;
    const byte *p = lbuf;
    byte *q = pw->ptr;
    uint data = *p++ ^ invert;

    hce_declare_state;
    /*
     * In order to handle the nominal 'changing white' at the beginning of
     * each scan line, we need to suppress the test for an initial black bit
     * in the reference line when we are at the very beginning of the scan
     * line.  To avoid an extra test, we use two different mask tables.
     */
    static const byte initial_count_bit[8] =
    {
	0, 1, 2, 4, 8, 0x10, 0x20, 0x40
    };
    static const byte further_count_bit[8] =
    {
	0x80, 1, 2, 4, 8, 0x10, 0x20, 0x40
    };
    const byte *count_bit = initial_count_bit;

    hce_load_state();
    while (count != end_count) {
	/*
	 * If invert == invert_white, white and black have their
	 * correct meanings; if invert == ~invert_white,
	 * black and white are interchanged.
	 */
	uint a0 = count;
	uint a1;

#define b1 (a1 - diff)		/* only for printing */
	int diff;
	uint prev_count = count;
	const byte *prev_p = p - lbuf + lprev;
	byte prev_data = prev_p[-1] ^ invert;
	int rlen;

	/* Find the a1 and b1 transitions. */
	skip_white_pixels(data, p, count, invert, rlen);
	a1 = count;
	if ((prev_data & count_bit[prev_count & 7])) {
	    /* Look for changing white first. */
	    skip_black_pixels(prev_data, prev_p, prev_count, invert, rlen);
	}
	count_bit = further_count_bit;	/* no longer at beginning */
      pass:
	if (prev_count != end_count)
	    skip_white_pixels(prev_data, prev_p, prev_count, invert, rlen);
	diff = a1 - prev_count;	/* i.e., logical b1 - a1 */
	/* In all the comparisons below, remember that count */
	/* runs downward, not upward, so the comparisons are */
	/* reversed. */
	if (diff <= -2) {
	    /* Could be a pass mode.  Find b2. */
	    if (prev_count != end_count)
		skip_black_pixels(prev_data, prev_p,
				  prev_count, invert, rlen);
	    if (prev_count > a1) {
		/* Use pass mode. */
		if_debug4('W', "[W]pass: count = %d, a1 = %d, b1 = %d, new count = %d\n",
			  a0, a1, b1, prev_count);
		hc_put_value(ss, q, cf2_run_pass_value, cf2_run_pass_length);
		a0 = prev_count;
		goto pass;
	    }
	}
	/* Check for vertical coding. */
	if (diff <= 3 && diff >= -3) {
	    /* Use vertical coding. */
	    const cfe_run *cp = &cf2_run_vertical[diff + 3];

	    if_debug5('W', "[W]vertical %d: count = %d, a1 = %d, b1 = %d, new count = %d\n",
		      diff, a0, a1, b1, count);
	    hc_put_code(ss, q, cp);
	    invert = ~invert;	/* a1 polarity changes */
	    data ^= 0xff;
	    continue;
	}
	/* No luck, use horizontal coding. */
	if (count != end_count)
	    skip_black_pixels(data, p, count, invert, rlen);	/* find a2 */
	hc_put_value(ss, q, cf2_run_horizontal_value,
		     cf2_run_horizontal_length);
	a0 -= a1;
	a1 -= count;
	if (invert == invert_white) {
	    if_debug3('W', "[W]horizontal: white = %d, black = %d, new count = %d\n",
		      a0, a1, count);
	    CF_PUT_WHITE_RUN(ss, a0);
	    CF_PUT_BLACK_RUN(ss, a1);
	} else {
	    if_debug3('W', "[W]horizontal: black = %d, white = %d, new count = %d\n",
		      a0, a1, count);
	    CF_PUT_BLACK_RUN(ss, a0);
	    CF_PUT_WHITE_RUN(ss, a1);
#undef b1
	}
    }
    hce_store_state();
    pw->ptr = q;
}

/* Stream template */
const stream_template s_CFE_template =
{
    &st_CFE_state, s_CFE_init, s_CFE_process, 1, 1,
    s_CFE_release, s_CFE_set_defaults
};
