/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* scfe.c */
/* CCITTFax encoding filter */
#include "stdio_.h"	/* includes std.h */
#include "memory_.h"
#include "gdebug.h"
#include "strimpl.h"
#include "scf.h"
#include "scfx.h"

/* ------ Macros and support routines ------ */

/* Statistics */

#ifdef DEBUG
private struct _r1d { ulong termination[64], make_up[41]; } runs_1d[2];
#  define count_run(tab, i) ((tab)[i]++)
#else
#  define count_run(cnt, n) DO_NOTHING
#endif

/* Put a run onto the output stream. */
/* Free variables: q, wlimit, status. */

#define cf_ensure_put_runs(n, color, out)\
	if ( wlimit - q < (n) * cfe_max_code_bytes )	/* worst case */\
	{	ss->run_color = color;\
		status = 1;\
		goto out;\
	}
#define cf_put_run(ss, lenv, tt, mut, tab)\
{	cfe_run rr;\
	if ( lenv >= 64 )\
	{	while ( lenv >= 2560 + 64 )\
		  {	rr = mut[40];\
			count_run(tab.make_up, 40);\
			hc_put_value(ss, q, rr.code, rr.code_length);\
			lenv -= 2560;\
		  }\
		rr = mut[lenv >> 6];\
		count_run(tab.make_up, lenv >> 6);\
		hc_put_value(ss, q, rr.code, rr.code_length);\
		lenv &= 63;\
	}\
	rr = tt[lenv];\
	count_run(tab.termination, lenv);\
	hc_put_value(ss, q, rr.code, rr.code_length);\
}

#define cf_put_white_run(ss, lenv)\
  cf_put_run(ss, lenv, cf_white_termination, cf_white_make_up, runs_1d[0])

#define cf_put_black_run(ss, lenv)\
  cf_put_run(ss, lenv, cf_black_termination, cf_black_make_up, runs_1d[1])

/* ------ CCITTFaxEncode ------ */

private_st_CFE_state();
	  
#define ss ((stream_CFE_state *)st)

private void s_CFE_release(P1(stream_state *));

/*
 * For the 2-D encoding modes, we leave the previous complete scan line
 * at the beginning of the buffer, and start the new data after it.
 */

/* Set default parameter values. */
private void
s_CFE_set_defaults(register stream_state *st)
{	s_CFE_set_defaults_inline(ss);
}

/* Initialize CCITTFaxEncode filter */
private int
s_CFE_init(register stream_state *st)
{	int columns = ss->Columns;
	int raster = ss->raster =
	  round_up((columns + 7) >> 3, ss->DecodedByteAlign);
	s_hce_init_inline(ss);
	ss->count = raster << 3;	/* starting a scan line */
	ss->lbuf = ss->lprev = 0;
	if ( columns > cfe_max_width )
	  return ERRC;			/****** WRONG ******/
	ss->lbuf = gs_alloc_bytes(st->memory, raster + 1,
				       "CFE lbuf");
	if ( ss->lbuf == 0 )
	  {	s_CFE_release(st);
		return ERRC;		/****** WRONG ******/
	  }
	if ( ss->K != 0 )
	{	ss->lprev = gs_alloc_bytes(st->memory, raster + 1,
					       "CFE lprev");
		if ( ss->lprev == 0 )
		  {	s_CFE_release(st);
			return ERRC;		/****** WRONG ******/
		  }
		/* Clear the initial reference line for 2-D encoding. */
		/* Make sure it is terminated properly. */
		memset(ss->lprev, (ss->BlackIs1 ? 0 : 0xff), raster);
		if ( columns & 7 )
		  ss->lprev[raster - 1] ^= 0x80 >> (columns & 7);
		else
		  ss->lprev[raster] = ~ss->lprev[0];
	}
	ss->copy_count = raster;
	ss->new_line = true;
	ss->k_left = (ss->K > 0 ? 1 : ss->K);
	return 0;
}

/* Release the filter. */
private void
s_CFE_release(stream_state *st)
{	gs_free_object(st->memory, ss->lprev, "CFE lprev(close)");
	gs_free_object(st->memory, ss->lbuf, "CFE lbuf(close)");
}

/* Flush the buffer */
private int cf_encode_1d(P4(stream_CFE_state *, const byte *,
  stream_cursor_write *, uint));
private int cf_encode_2d(P5(stream_CFE_state *, const byte *,
  stream_cursor_write *, uint, const byte *));
private int
s_CFE_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	const byte *rlimit = pr->limit;
	byte *wlimit = pw->limit;
	int raster = ss->raster;
	int initial_count = raster << 3;
	int end_count = -ss->Columns & 7;
	byte end_mask = 1 << (-ss->Columns & 7);
	int status = 0;
	hce_declare_state;

	hce_load_state();
	while ( pr->ptr < rlimit || ss->count != initial_count )
	{	byte *end = ss->lbuf + raster - 1;
		if_debug7('w', "[w]CFE: copy_count = %d, pr = 0x%lx(%d)0x%lx, pw = 0x%lx(%d)0x%lx\n",
			  ss->copy_count, (ulong)pr->ptr,
			  (int)(rlimit - pr->ptr), (ulong)rlimit,
			  (ulong)pw->ptr, (int)(wlimit - pw->ptr),
			  (ulong)wlimit);
		/* Check whether we are still accumulating a scan line. */
		if ( ss->copy_count != 0 )
		{	int rcount = rlimit - pr->ptr;
			int ccount = min(rcount, ss->copy_count);
			memcpy(ss->lbuf + raster - ss->copy_count,
			       pr->ptr + 1, ccount);
			pr->ptr += ccount;
			if ( (ss->copy_count -= ccount) != 0 )
			  goto out;
			/*
			 * Ensure that the scan line ends with two
			 * polarity changes. 
			 */
			{ byte end_bit = *end & end_mask;
			  byte not_bit = end_bit ^ end_mask;
			  *end &= -end_mask;
			  if ( end_mask == 1 )
			    end[1] = (end_bit ? 0x40 : 0x80);
			  else if ( end_mask == 2 )
			    *end |= not_bit >> 1, end[1] = end_bit << 7;
			  else
			    *end |= (not_bit >> 1) | (end_bit >> 2);
			}
		}
		if ( ss->new_line )
		{	/* Start a new scan line. */
			byte *q = pw->ptr;
			if ( wlimit - q < 4 + cfe_max_code_bytes * 2 )	/* byte align, aligned eol, run_horizontal + 2 runs */
			{	status = 1;
				break;
			}
#ifdef DEBUG
			if ( ss->K > 0 )
			  { if_debug1('w', "[w]new row, k_left=%d\n",
				      ss->k_left);
			  }
			else
			  { if_debug0('w', "[w]new row\n");
			  }
#endif
			if ( ss->EndOfLine )
			{	const cfe_run *rp =
				  (ss->K <= 0 ? &cf_run_eol :
				   ss->k_left > 1 ? &cf2_run_eol_2d :
				   &cf2_run_eol_1d);
				cfe_run run;
				if ( ss->EncodedByteAlign )
				  {	run = *rp;
					/* Pad the run on the left */
					/* so it winds up byte-aligned. */
					run.code_length +=
					 (bits_left - run_eol_code_length) & 7;
					if ( run.code_length > 16 ) /* <= 23 */
					  bits_left -= run.code_length & 7,
					  run.code_length = 16;
					rp = &run;
				  }
				hc_put_code(ss, q, rp);
				pw->ptr = q;
			}
			else if ( ss->EncodedByteAlign )
			  bits_left &= ~7;
			ss->run_color = 0;
			ss->new_line = false;
		}
		hce_store_state();
		if ( ss->K > 0 )
		{	/* Group 3, mixed encoding */
			if ( --(ss->k_left) )	/* Use 2-D encoding */
			{	status = cf_encode_2d(ss, ss->lbuf, pw, end_count, ss->lprev);
				if ( status )
				  {	/* We didn't finish encoding */
					/* the line, so back out. */
					ss->k_left++;
				  }
			}
			else	/* Use 1-D encoding */
			{	status = cf_encode_1d(ss, ss->lbuf, pw, end_count);
				if ( status )
				  {	/* Didn't finish encoding the line, */
					/* back out. */
					ss->k_left++;
				  }
				else
				  ss->k_left = ss->K;
			}
		}
		else	/* Uniform encoding */
		{	status = (ss->K == 0 ?
				cf_encode_1d(ss, ss->lbuf, pw, end_count) :
				cf_encode_2d(ss, ss->lbuf, pw, end_count, ss->lprev));
		}
		hce_load_state();
		if ( status )
		  break;
		if ( ss->count == end_count )
		{	/* Finished a scan line, start a new one. */
			ss->count = initial_count;
			ss->new_line = true;
			if ( ss->K != 0 )
			{	byte *temp = ss->lbuf;
				ss->lbuf = ss->lprev;
				ss->lprev = temp;
			}
			ss->copy_count = raster;
		}
	}
	/* Check for end of data. */
	if ( last && status == 0 )
	{	const cfe_run *rp =
		  (ss->K > 0 ? &cf2_run_eol_1d : &cf_run_eol);
		int i = (!ss->EndOfBlock ? 0 : ss->K < 0 ? 2 : 6);
		uint bits_to_write =
		  hc_bits_size - bits_left + i * rp->code_length;
		byte *q = pw->ptr;
		if ( wlimit - q < (bits_to_write + 7) >> 3 )
		{	status = 1;
			goto out;
		}
		if ( ss->EncodedByteAlign )
		  bits_left &= ~7;
		while ( --i >= 0 )
		  hc_put_code(ss, q, rp);
		/* Force out the last byte or bytes. */
		pw->ptr = q = hc_put_last_bits((stream_hc_state *)ss, q);
		goto ns;
	}
out:	hce_store_state();
ns:	if_debug9('w', "[w]CFE exit %d: count = %d, run_color = %d,\n     pr = 0x%lx(%d)0x%lx; pw = 0x%lx(%d)0x%lx\n",
		  status, ss->count, ss->run_color,
		  (ulong)pr->ptr, (int)(rlimit - pr->ptr), (ulong)rlimit,
		  (ulong)pw->ptr, (int)(wlimit - pw->ptr), (ulong)wlimit);
#ifdef DEBUG
	if ( pr->ptr > rlimit || pw->ptr > wlimit )
	{	lprintf("Pointer overrun!\n");
		status = ERRC;
	}
	if ( gs_debug_c('w') && status == 1 )
	  {	int ti;
		for ( ti = 0; ti < 2; ti++ )
		  {	int i;
			ulong total;
			dprintf1("[w]runs[%d]", ti);
			for ( i = 0, total = 0; i < 41; i++ )
			  dprintf1(" %lu", runs_1d[ti].make_up[i]),
			  total += runs_1d[ti].make_up[i];
			dprintf1(" total=%lu\n\t", total);
			for ( i = 0, total = 0; i < 64; i++ )
			  dprintf1(" %lu", runs_1d[ti].termination[i]),
			  total += runs_1d[ti].termination[i];
			dprintf1(" total=%lu\n", total);
		  }
	  }
#endif
	return status;
}

#undef ss

/*
 * For all encoding methods, we know we have a full scan line of input,
 * but we must be prepared to suspend if we run out of space to store
 * the output.
 */

/* Encode a 1-D scan line. */
private int
cf_encode_1d(stream_CFE_state *ss, const byte *lbuf,
  stream_cursor_write *pw, uint end_count)
{	uint count = ss->count;
	byte *q = pw->ptr;
	byte *wlimit = pw->limit;
	int rlen;
	int status = 0;
	hce_declare_state;

	{ register const byte *p = lbuf + ss->raster - ((count + 7) >> 3);
	  byte invert = (ss->BlackIs1 ? 0 : 0xff);
	  /* Invariant: data = p[-1] ^ invert. */
	  register uint data = *p++ ^ invert;

	  hce_load_state();
	  while ( count != end_count )
	    {	/* Parse a white run. */
		cf_ensure_put_runs(2, 0, out);
		skip_white_pixels(data, p, count, invert, rlen);
		cf_put_white_run(ss, rlen);
		if ( count == end_count )
		  break;
		/* Parse a black run. */
		skip_black_pixels(data, p, count, invert, rlen);
		cf_put_black_run(ss, rlen);
	    }
	}

out:	hce_store_state();
	pw->ptr = q;
	ss->count = count;
	return status;
}

/* Encode a 2-D scan line. */
private int
cf_encode_2d(stream_CFE_state *ss, const byte *lbuf,
  stream_cursor_write *pw, uint end_count, const byte *lprev)
{	byte invert_white = (ss->BlackIs1 ? 0 : 0xff);
	byte invert = (ss->run_color ? ~invert_white : invert_white);
	register uint count = ss->count;
	const byte *p = lbuf + ss->raster - ((count + 7) >> 3);
	byte *q = pw->ptr;
	byte *wlimit = pw->limit;
	register uint data = *p++ ^ invert;
	int status = 0;
	hce_declare_state;
	/* In order to handle the nominal 'changing white' at the */
	/* beginning of each scan line, we need to suppress the test for */
	/* an initial black bit in the reference line when we are at */
	/* the very beginning of the scan line.  To avoid an extra test, */
	/* we use two different mask tables. */
	static const byte initial_count_bit[8] =
	  { 0, 1, 2, 4, 8, 0x10, 0x20, 0x40 };
	static const byte further_count_bit[8] =
	  { 0x80, 1, 2, 4, 8, 0x10, 0x20, 0x40 };
	const byte _ds *count_bit =
	  (count == ss->raster << 3 ? initial_count_bit : further_count_bit);

	hce_load_state();
	while ( count != end_count )
	{	/* If invert == invert_white, white and black have their */
		/* correct meanings; if invert == ~invert_white, */
		/* black and white are interchanged. */
		uint a0 = count;
		uint a1;
#define b1 (a1 - diff)		/* only for printing */
		int diff;
		uint prev_count = count;
		const byte *prev_p = p - lbuf + lprev;
		byte prev_data = prev_p[-1] ^ invert;
		int rlen;

		/* Make sure we have room for a run_horizontal plus */
		/* two data runs. */
		cf_ensure_put_runs(3, invert != invert_white, out);
		/* Find the a1 and b1 transitions. */
		skip_white_pixels(data, p, count, invert, rlen);
		a1 = count;
		if ( (prev_data & count_bit[prev_count & 7]) )
		{	/* Look for changing white first. */
			skip_black_pixels(prev_data, prev_p, prev_count, invert, rlen);
		}
		count_bit = further_count_bit;	/* no longer at beginning */
pass:		if ( prev_count != end_count )
		{	skip_white_pixels(prev_data, prev_p, prev_count, invert, rlen);
		}
		diff = a1 - prev_count;	/* i.e., logical b1 - a1 */
		/* In all the comparisons below, remember that count */
		/* runs downward, not upward, so the comparisons are */
		/* reversed. */
		if ( diff <= -2 )
		{	/* Could be a pass mode.  Find b2. */
			if ( prev_count != end_count )
			{	skip_black_pixels(prev_data, prev_p,
						 prev_count, invert, rlen);
			}
			if ( prev_count > a1 )
			{	/* Use pass mode. */
				if_debug4('W', "[W]pass: count = %d, a1 = %d, b1 = %d, new count = %d\n",
					  a0, a1, b1, prev_count);
				hc_put_value(ss, q, cf2_run_pass_value,
					     cf2_run_pass_length);
				cf_ensure_put_runs(3, invert != invert_white,
						   pass_out);
				a0 = prev_count;
				goto pass;
pass_out:			count = prev_count;
				break;
			}
		}
		/* Check for vertical coding. */
		if ( diff <= 3 && diff >= -3 )
		{	/* Use vertical coding. */
			const cfe_run *cp;
			if_debug5('W', "[W]vertical %d: count = %d, a1 = %d, b1 = %d, new count = %d\n",
				  diff, a0, a1, b1, count);
			cp = &cf2_run_vertical[diff + 3];
			hc_put_code(ss, q, cp);
			invert = ~invert;	/* a1 polarity changes */
			data ^= 0xff;
			continue;
		}
		/* No luck, use horizontal coding. */
		if ( count != end_count )
		{	skip_black_pixels(data, p, count, invert, rlen);	/* find a2 */
		}
		hc_put_value(ss, q, cf2_run_horizontal_value,
			     cf2_run_horizontal_length);
		a0 -= a1;
		a1 -= count;
		if ( invert == invert_white )
		{	if_debug3('W', "[W]horizontal: white = %d, black = %d, new count = %d\n",
				  a0, a1, count);
			cf_put_white_run(ss, a0);
			cf_put_black_run(ss, a1);
		}
		else
		{	if_debug3('W', "[W]horizontal: black = %d, white = %d, new count = %d\n",
				  a0, a1, count);
			cf_put_black_run(ss, a0);
			cf_put_white_run(ss, a1);
#undef b1
		}
	}
out:	hce_store_state();
	pw->ptr = q;
	ss->count = count;
	return status;
}

/* Stream template */
const stream_template s_CFE_template =
{	&st_CFE_state, s_CFE_init, s_CFE_process,
	2, 15, /* 31 left-over bits + 7 bits of padding + 6 13-bit EOLs */
	s_CFE_release, s_CFE_set_defaults
};
