/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: srld.c,v 1.2 2000/03/08 23:15:27 mike Exp $ */
/* RunLengthDecode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "srlx.h"

/* ------ RunLengthDecode ------ */

private_st_RLD_state();

/* Set defaults */
private void
s_RLD_set_defaults(stream_state * st)
{
    stream_RLD_state *const ss = (stream_RLD_state *) st;

    s_RLD_set_defaults_inline(ss);
}

/* Initialize */
private int
s_RLD_init(stream_state * st)
{
    stream_RLD_state *const ss = (stream_RLD_state *) st;

    return s_RLD_init_inline(ss);
}

/* Refill the buffer */
private int
s_RLD_process(stream_state * st, stream_cursor_read * pr,
	      stream_cursor_write * pw, bool last)
{
    stream_RLD_state *const ss = (stream_RLD_state *) st;
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    const byte *rlimit = pr->limit;
    byte *wlimit = pw->limit;
    int left;
    int status = 0;

top:
    if ((left = ss->copy_left) > 0) {
	/*
	 * We suspended because the output buffer was full:;
	 * try again now.
	 */
	uint avail = wlimit - q;
	int copy_status = 1;

	if (left > avail)
	    left = avail;
	if (ss->copy_data >= 0)
	    memset(q + 1, ss->copy_data, left);
	else {
	    avail = rlimit - p;
	    if (left >= avail) {
		copy_status = 0;
		left = avail;
	    }
	    memcpy(q + 1, p + 1, left);
	    p += left;
	}
	q += left;
	if ((ss->copy_left -= left) > 0) {
	    status = copy_status;
	    goto x;
	}
    }
    while (p < rlimit) {
	int b = *++p;

	if (b < 128) {
	    if (++b > rlimit - p || b > wlimit - q) {
		ss->copy_left = b;
		ss->copy_data = -1;
		goto top;
	    }
	    memcpy(q + 1, p + 1, b);
	    p += b;
	    q += b;
	} else if (b == 128) {	/* end of data */
	    if (ss->EndOfData) {
		status = EOFC;
		break;
	    }
	} else if (p == rlimit) {
	    p--;
	    break;
	} else if ((b = 257 - b) > wlimit - q) {
	    ss->copy_left = b;
	    ss->copy_data = *++p;
	    goto top;
	} else {
	    memset(q + 1, *++p, b);
	    q += b;
	}
    }
x:  pr->ptr = p;
    pw->ptr = q;
    return status;
}

/* Stream template */
const stream_template s_RLD_template = {
    &st_RLD_state, s_RLD_init, s_RLD_process, 1, 1, NULL,
    s_RLD_set_defaults
};
