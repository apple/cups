/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* srle.c */
/* RunLengthEncode filter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "srlx.h"

/* ------ RunLengthEncode ------ */

private_st_RLE_state();

#define ss ((stream_RLE_state *)st)

/* Set defaults */
private void
s_RLE_set_defaults(stream_state *st)
{	s_RLE_set_defaults_inline(ss);
}

/* Initialize */
private int
s_RLE_init(stream_state *st)
{	return s_RLE_init_inline(ss);
}

/* Process a buffer */
private int
s_RLE_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	const byte *rlimit = pr->limit;
	byte *wlimit = pw->limit;
	int status = 0;
	ulong rleft = ss->record_left;
	while ( p < rlimit )
	{	const byte *beg = p;
		const byte *p1;
		uint count = rlimit - p;
		byte next;
		if ( count > rleft )
			count = rleft;
		if ( count > 127 )
			count = 127;
		p1 = p + count;
		if ( count > 2 && (next = p[1]) == p[2] && next == p[3] )
		{	if ( wlimit - q < 2 )
			{	status = 1;
				break;
			}
			/* Recognize leading repeated byte */
			p1--;
			do { p++; }
			while ( p < p1 && p[2] == next );
			p++;
			*++q = (byte)(257 - (p - beg));
			*++q = next;
		}
		else
		{	p1 -= 2;
			while ( p < p1 && (p[2] != p[1] || p[3] != p[1]) )
				p++;
			if ( p >= p1 )
				p = p1 + 2;
			count = p - beg;
			if ( wlimit - q < count + 1 )
			{	p = beg;
				status = 1;
				break;
			}
			*++q = count - 1;
			memcpy(q + 1, beg + 1, count);
			q += count;
		}
		rleft -= p - beg;
		if ( rleft == 0 )
			rleft = ss->record_size;
	}
	if ( last && status == 0 && ss->EndOfData )
	  {	if ( q < wlimit )
		  *++q = 128;
		else
		  status = 1;
	  }
	pr->ptr = p;
	pw->ptr = q;
	ss->record_left = rleft;
	return status;
}

#undef ss

/* Stream template */
const stream_template s_RLE_template =
{	&st_RLE_state, s_RLE_init, s_RLE_process, 128, 129, NULL,
	s_RLE_set_defaults, s_RLE_init
};
