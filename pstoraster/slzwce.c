/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* slzwce.c */
/* Simple encoder compatible with LZW decoding filter */
#include "stdio_.h"	/* includes std.h */
#include "gdebug.h"
#include "strimpl.h"
#include "slzwx.h"

/* ------ Alternate LZWEncode filter implementation ------ */

/*

The encoded data stream produced by this implementation of the LZWEncode
filter consists of a sequence of 9-bit data elements.  These elements are
packed into bytes in big-endian order, e.g. the elements

	100000000 001100001

occurring at the very beginning of the data stream would be packed into
bytes as

	10000000 00011000 01......

The first bit of each data element is a control bit.  If the control bit is
0, the remaining 8 bits of the data element are a data byte.  If the control
bit is 1, the remaining 8 bits of the data element define a control
function:

	1 00000000	synchronization mark, see below
	1 00000001	end of data
	1 xxxxxxxx	not used (all other values)

The synchronization mark occurs at the beginning of the data stream, and at
least once every 254 data bytes thereafter.

This format is derived from basic principles of data encoding (the use of a
separate flag bit to distinguish out-of-band control information from data
per se, and the use of a periodic synchronization mark to help verify the
validity of a data stream); it has no relationship to data compression.  It
is, however, compatible with LZW decompressors.  It produces output that is
approximately 9/8 times the size of the input.

*/

/* Define the special codes, relative to 1 << InitialCodeLength. */
#define code_reset 0
#define code_eod 1
#define code_0 2			/* first assignable code */

/* Internal routine to put a code into the output buffer. */
/* Let S = ss->code_size. */
/* Relevant invariants: 9 <= S <= 15, 0 <= code < 1 << S; */
/* 1 <= ss->bits_left <= 8; only the rightmost (8 - ss->bits_left) */
/* bits of ss->bits contain valid data. */
private byte *
lzw_put_code(register stream_LZW_state *ss, byte *q, uint code)
{	uint size = ss->code_size;
	byte cb = (ss->bits << ss->bits_left) +
	  (code >> (size - ss->bits_left));
	if_debug2('W', "[w]writing 0x%x,%d\n", code, ss->code_size);
	*++q = cb;
	if ( (ss->bits_left += 8 - size) <= 0 )
	  {	*++q = code >> -ss->bits_left;
		ss->bits_left += 8;
	  }
	ss->bits = code;
	return q;
}

#define ss ((stream_LZW_state *)st)

/* Initialize LZW-compatible encoding filter. */
int
s_LZWE_reset(stream_state *st)
{	ss->code_size = ss->InitialCodeLength + 1;
	ss->bits_left = 8;
	ss->next_code = (1 << ss->InitialCodeLength) + code_0;
	return 0;
}
private int
s_LZWE_init(stream_state *st)
{	ss->InitialCodeLength = 8;
	ss->table.encode = 0;
	return s_LZWE_reset(st);
}

/* Process a buffer */
private int
s_LZWE_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	register byte *q = pw->ptr;
	byte *wlimit = pw->limit;
	int status = 0;
	int signal = 1 << (ss->code_size - 1);
	uint limit_code = (1 << ss->code_size) - 1;
	uint next_code = ss->next_code;

	while ( p < rlimit )
	  {	if ( next_code == limit_code )
		  {	/* Emit a reset code. */
			if ( wlimit - q < 2 )
			  {	status = 1;
				break;
			  }
			q = lzw_put_code(ss, q, signal + code_reset);
			next_code = signal + code_0;
		  }
		if ( wlimit - q < 2 )
		  {	status = 1;
			break;
		  }
		q = lzw_put_code(ss, q, *++p);
		next_code++;
	  }
	if ( last && status == 0 )
	  {	if ( wlimit - q < 2 )
			status = 1;
		else
		  {	q = lzw_put_code(ss, q, signal + code_eod);
			if ( ss->bits_left < 8 )
			  *++q = ss->bits << ss->bits_left;  /* final byte */
		  }
	  }
	ss->next_code = next_code;
	pr->ptr = p;
	pw->ptr = q;
	return status;
}

#undef ss

/* Stream template */
const stream_template s_LZWE_template =
{	&st_LZW_state, s_LZWE_init, s_LZWE_process, 1, 2, NULL,
	s_LZW_set_defaults, s_LZWE_reset
};
