/* Copyright (C) 1994, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: seexec.c,v 1.2 2000/03/08 23:15:23 mike Exp $ */
/* eexec filters */
#include "stdio_.h"		/* includes std.h */
#include "strimpl.h"
#include "sfilter.h"
#include "gscrypt1.h"
#include "scanchar.h"

/* ------ eexecEncode ------ */

/* Encoding is much simpler than decoding, because we don't */
/* worry about initial characters or hex vs. binary (the client */
/* has to take care of these aspects). */

private_st_exE_state();

/* Process a buffer */
private int
s_exE_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_exE_state *const ss = (stream_exE_state *) st;
    const byte *p = pr->ptr;
    byte *q = pw->ptr;
    uint rcount = pr->limit - p;
    uint wcount = pw->limit - q;
    uint count;
    int status;

    if (rcount <= wcount)
	count = rcount, status = 0;
    else
	count = wcount, status = 1;
    gs_type1_encrypt(q + 1, p + 1, count, (crypt_state *)&ss->cstate);
    pr->ptr += count;
    pw->ptr += count;
    return status;
}

/* Stream template */
const stream_template s_exE_template = {
    &st_exE_state, NULL, s_exE_process, 1, 2
};

/* ------ eexecDecode ------ */

private_st_exD_state();

/* Set defaults. */
private void
s_exD_set_defaults(stream_state * st)
{
    stream_exD_state *const ss = (stream_exD_state *) st;

    ss->lenIV = 4;
}

/* Initialize the state for reading and decrypting. */
/* Decrypting streams are not positionable. */
private int
s_exD_init(stream_state * st)
{
    stream_exD_state *const ss = (stream_exD_state *) st;

    ss->odd = -1;
    ss->binary = -1;		/* unknown */
    ss->record_left = max_long;
    ss->skip = ss->lenIV;
    return 0;
}

/* Process a buffer. */
private int
s_exD_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_exD_state *const ss = (stream_exD_state *) st;
    const byte *p = pr->ptr;
    byte *q = pw->ptr;
    int skip = ss->skip;
    int rcount = pr->limit - p;
    int wcount = pw->limit - q;
    int status = 0;
    int count = (wcount < rcount ? (status = 1, wcount) : rcount);

    if (ss->binary < 0) {
	/*
	 * This is the very first time we're filling the buffer.
	 * Determine whether this is ASCII or hex encoding.
	 */
	const byte *const decoder = scan_char_decoder;
	int i;

	if (rcount < 8)
	    return 0;
	/*
	 * Adobe's documentation doesn't actually specify the test
	 * that eexec should use, but we believe the following
	 * gives correct answers even on certain non-conforming
	 * PostScript files encountered in practice:
	 */
	ss->binary = 0;
	for (i = 1; i <= 8; i++)
	    if (!(decoder[p[i]] <= 0xf ||
		  decoder[p[i]] == ctype_space)
		) {
		ss->binary = 1;
		if (ss->pfb_state != 0) {
		    /* Stop at the end of the .PFB binary data. */
		    ss->record_left = ss->pfb_state->record_left;
		}
		break;
	    }
    }
    if (ss->binary) {
	if (count > ss->record_left) {
	    count = ss->record_left;
	    status = 0;
	}
	/*
	 * We pause at the end of the .PFB binary data,
	 * in an attempt to keep from reading beyond the end of
	 * the encrypted data.
	 */
	if ((ss->record_left -= count) == 0)
	    ss->record_left = max_long;
	pr->ptr = p + count;
    } else {
	/*
	 * We only ignore leading whitespace, in an attempt to
	 * keep from reading beyond the end of the encrypted data.
	 */
	status = s_hex_process(pr, pw, &ss->odd,
			       hex_ignore_leading_whitespace);
	p = q;
	count = pw->ptr - q;
    }
    if (skip >= count && skip != 0) {
	gs_type1_decrypt(q + 1, p + 1, count,
			 (crypt_state *) & ss->cstate);
	ss->skip -= count;
	count = 0;
	status = 0;
    } else {
	gs_type1_decrypt(q + 1, p + 1, skip,
			 (crypt_state *) & ss->cstate);
	count -= skip;
	gs_type1_decrypt(q + 1, p + 1 + skip, count,
			 (crypt_state *) & ss->cstate);
	ss->skip = 0;
    }
    pw->ptr = q + count;
    return status;
}

/* Stream template */
/*
 * The specification of eexec decoding requires that it never read more than
 * 512 source bytes ahead.  The only reliable way to ensure this is to
 * limit the size of the output buffer to 256.  We set it a little smaller
 * so that it will stay under the limit even after adding min_in_size
 * for a subsequent filter in a pipeline.  Note that we have to specify
 * a size of at least 128 so that filter_read won't round it up.
 */
const stream_template s_exD_template = {
    &st_exD_state, s_exD_init, s_exD_process, 8, 200,
    NULL, s_exD_set_defaults
};
