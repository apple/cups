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

/* smtf.c */
/* MoveToFront filters */
#include "stdio_.h"
#include "strimpl.h"
#include "smtf.h"

/* ------ MoveToFrontEncode/Decode ------ */

private_st_MTF_state();

#define ss ((stream_MTF_state *)st)

/* Initialize */
private int
s_MTF_init(stream_state *st)
{	int i;
	for ( i = 0; i < 256; i++ )
	  ss->prev.b[i] = (byte)i;
	return 0;
}

/* Encode a buffer */
private int
s_MTFE_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	const byte *rlimit = pr->limit;
	uint count = rlimit - p;
	uint wcount = pw->limit - q;
	int status =
		(count < wcount ? 0 :
		 (rlimit = p + wcount, 1));
	while ( p < rlimit )
	{	byte b = *++p;
		int i;
		byte prev = b, repl;
		for ( i = 0; (repl = ss->prev.b[i]) != b; i++ )
		  ss->prev.b[i] = prev, prev = repl;
		ss->prev.b[i] = prev;
		*++q = (byte)i;
	}
	pr->ptr = p;
	pw->ptr = q;
	return status;
}

/* Stream template */
const stream_template s_MTFE_template =
{	&st_MTF_state, s_MTF_init, s_MTFE_process, 1, 1
};

/* Decode a buffer */
private int
s_MTFD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	const byte *rlimit = pr->limit;
	uint count = rlimit - p;
	uint wcount = pw->limit - q;
	int status = (count <= wcount ? 0 : (rlimit = p + wcount, 1));
	/* Cache the first few entries in local variables. */
	byte
	  v0 = ss->prev.b[0], v1 = ss->prev.b[1],
	  v2 = ss->prev.b[2], v3 = ss->prev.b[3];
	while ( p < rlimit )
	{	byte first;
		/* Zeros far outnumber all other bytes in the BWBS */
		/* code; check for them first. */
		if ( *++p == 0 )
		  {	*++q = v0;
			continue;
		  }
		switch ( *p )
		{
		default:
		  {	uint b = *p;
			byte *bp = &ss->prev.b[b];
			*++q = first = *bp;
#if arch_sizeof_long == 4
			ss->prev.b[3] = v3;
#endif
			/* Move trailing entries individually. */
			for ( ; ; bp--, b-- )
			  {	*bp = bp[-1];
				if ( !(b & (sizeof(long) - 1)) )
				  break;
			  }
			/* Move in long-size chunks. */
			for ( ; (b -= sizeof(long)) != 0; )
			  {	bp -= sizeof(long);
#if arch_is_big_endian
				*(ulong *)bp =
				  (*(ulong *)bp >> 8) |
				    (bp[-1] << ((sizeof(long) - 1) * 8));
#else
				*(ulong *)bp = (*(ulong *)bp << 8) | bp[-1];
#endif
			  }
		  }
#if arch_sizeof_long > 4	/* better be 8! */
			goto m7;
		case 7:
			*++q = first = ss->prev.b[7];
m7:			ss->prev.b[7] = ss->prev.b[6];
			goto m6;
		case 6:
			*++q = first = ss->prev.b[6];
m6:			ss->prev.b[6] = ss->prev.b[5];
			goto m5;
		case 5:
			*++q = first = ss->prev.b[5];
m5:			ss->prev.b[5] = ss->prev.b[4];
			goto m4;
		case 4:
			*++q = first = ss->prev.b[4];
m4:			ss->prev.b[4] = v3;
#endif
			goto m3;
		case 3:
			*++q = first = v3;
m3:			v3 = v2, v2 = v1, v1 = v0, v0 = first;
			break;
		case 2:
			*++q = first = v2;
			v2 = v1, v1 = v0, v0 = first;
			break;
		case 1:
			*++q = first = v1;
			v1 = v0, v0 = first;
			break;
		}
	}
	ss->prev.b[0] = v0;  ss->prev.b[1] = v1;
	ss->prev.b[2] = v2;  ss->prev.b[3] = v3;
	pr->ptr = p;
	pw->ptr = q;
	return status;
}

/* Stream template */
const stream_template s_MTFD_template =
{	&st_MTF_state, s_MTF_init, s_MTFD_process, 1, 1,
	NULL, NULL, s_MTF_init
};

#undef ss
