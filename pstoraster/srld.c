/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* srld.c */
/* RunLengthDecode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "srlx.h"

/* ------ RunLengthDecode ------ */

private_st_RLD_state();

#define ss ((stream_RLD_state *)st)

/* Set defaults */
private void
s_RLD_set_defaults(stream_state *st)
{	s_RLD_set_defaults_inline(ss);
}

/* Refill the buffer */
private int
s_RLD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	const byte *rlimit = pr->limit;
	byte *wlimit = pw->limit;
	int status = 0;

	while ( p < rlimit )
	{	int b = *++p;
		if ( b < 128 )
		{	if ( b >= rlimit - p )
			{	p--;
				break;
			}
			else if ( b >= wlimit - q )
			{	p--;
				status = 1;
				break;
			}
			memcpy(q + 1, p + 1, ++b);
			p += b;
			q += b;
		}
		else if ( b == 128 )	/* end of data */
		{	if ( ss->EndOfData )
			  {	status = EOFC;
				break;
			  }
		}
		else if ( p == rlimit )
		{	p--;
			break;
		}
		else if ( (b = 257 - b) > wlimit - q )
		{	p--;
			status = 1;
			break;		/* won't fit */
		}
		else
		{	memset(q + 1, *++p, b);
			q += b;
		}
	}
	pr->ptr = p;
	pw->ptr = q;
	return status;
}

#undef ss

/* Stream template */
const stream_template s_RLD_template =
{	&st_RLD_state, NULL, s_RLD_process, 129, 128, NULL,
	s_RLD_set_defaults
};
