/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: spcxd.c,v 1.2 2000/03/08 23:15:26 mike Exp $ */
/* PCXDecode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "spcxx.h"

/* ------ PCXDecode ------ */

/* Refill the buffer */
private int
s_PCXD_process(stream_state * st, stream_cursor_read * pr,
	       stream_cursor_write * pw, bool last)
{
    register const byte *p = pr->ptr;
    register byte *q = pw->ptr;
    const byte *rlimit = pr->limit;
    byte *wlimit = pw->limit;
    int status = 0;

    while (p < rlimit) {
	int b = *++p;

	if (b < 0xc0) {
	    if (q >= wlimit) {
		--p;
		status = 1;
		break;
	    }
	    *++q = b;
	} else if (p >= rlimit) {
	    --p;
	    break;
	} else if ((b -= 0xc0) > wlimit - q) {
	    --p;
	    status = 1;
	    break;
	} else {
	    memset(q + 1, *++p, b);
	    q += b;
	}
    }
    pr->ptr = p;
    pw->ptr = q;
    return status;
}

/* Stream template */
const stream_template s_PCXD_template = {
    &st_stream_state, NULL, s_PCXD_process, 2, 63
};
