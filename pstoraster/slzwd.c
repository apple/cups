/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* slzwd.c */
/* LZW decoding filter */
#include "stdio_.h"	/* includes std.h */
#include "gdebug.h"
#include "strimpl.h"
#include "slzwx.h"

/* ------ LZWDecode ------ */

/********************************************************/
/* LZW routines are based on:				*/
/* Dr. Dobbs Journal --- Oct. 1989. 			*/
/* Article on LZW Data Compression by Mark R. Nelson 	*/
/********************************************************/

/* Define the special codes in terms of code_escape, which is */
/* 1 << InitialCodeLength. */
#define code_reset (code_escape + 0)
#define code_eod (code_escape + 1)
#define code_0 (code_escape + 2)		/* first assignable code */

struct lzw_decode_s {
	byte datum;
	byte len;			/* length of code */
	ushort prefix;			/* code to be prefixed */
};
gs_private_st_simple(st_lzw_decode, lzw_decode, "lzw_decode");
/* We can use a simple type as the element type, */
/* because there are no pointers to enumerate or relocate. */
#define st_lzw_decode_element st_lzw_decode
#define lzw_decode_max 4096		/* must be 4096 */

#define ss ((stream_LZW_state *)st)

/* Initialize LZWDecode filter */
/* We separate out the reset function for some non-stream clients. */
int
s_LZWD_reset(stream_state *st)
{	register lzw_decode *dc = ss->table.decode;
	register int i;
	uint code_escape = 1 << ss->InitialCodeLength;

	ss->bits_left = 0;
	ss->bytes_left = 0;
	ss->next_code = code_0;
	ss->code_size = ss->InitialCodeLength + 1;
	ss->prev_code = -1;
	ss->copy_code = -1;
	dc[code_reset].len = 255;
	dc[code_eod].len = 255;
	for ( i = 0; i < code_escape; i++, dc++ )
	  dc->datum = i, dc->len = 1, dc->prefix = code_eod;
	return 0;
}
private int
s_LZWD_init(stream_state *st)
{	lzw_decode *dc =
	  gs_alloc_struct_array(st->memory, lzw_decode_max + 1,
				lzw_decode, &st_lzw_decode_element,
				"LZWDecode(init)");

	if ( dc == 0 )
	  return ERRC;		/****** WRONG ******/
	ss->table.decode = dc;
	return s_LZWD_reset(st);
}

/* Process a buffer */
private int
s_LZWD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
#ifdef DEBUG
	byte *q0 = q;
#endif
	const byte *rlimit = pr->limit;			/* constant pointer */
	byte *wlimit = pw->limit;			/* constant pointer */
	int status = 0;
	int code = ss->copy_code;
	int prev_code = ss->prev_code;
	uint prev_len = ss->prev_len;
	byte bits = ss->bits;
	int bits_left = ss->bits_left;
	int bytes_left = ss->bytes_left;
	int code_size = ss->code_size;
	int code_mask;
	int switch_code;
	int next_code = ss->next_code;
	lzw_decode *table = ss->table.decode;		/* constant pointer */
	lzw_decode *dc_next = table + next_code;	/* invariant */
	lzw_decode *dc;
	int code_escape = 1 << ss->InitialCodeLength;
	int eod = code_eod;
	bool low_order = ss->FirstBitLowOrder;
	uint len;
	int c;
	byte b;
	byte *q1;
	if_debug2('w', "[w]process decode: code_size=%d next_code=%d\n",
		  code_size, next_code);
#define set_code_size()\
  code_mask = (1 << code_size) - 1,\
  switch_code = code_mask + 1 - ss->EarlyChange
	set_code_size();
	if ( !ss->BlockData )
	  bytes_left = rlimit - p + 2; 	/* never stop for bytes_left */
	/* If we are in the middle of copying a string, */
	/* do some more now. */
	if ( code >= 0 )
	{	int rlen = ss->copy_left;
		int wlen = wlimit - q;
		int n = len = min(rlen, wlen);
		c = code;
		ss->copy_left = rlen -= len;
		if_debug3('W', "[W]copying 0x%x, %d byte(s) out of %d left\n",
			  code, len, rlen + len);
		while ( rlen )
			c = table[c].prefix,
			rlen--;
		q1 = q += len;
		n = len;
		while ( --n >= 0 )
		{	*q1-- = (dc = &table[c])->datum;
			c = dc->prefix;
		}
		if ( ss->copy_left )			/* more to do */
		{	pw->ptr = q;
			return 1;
		}
		ss->copy_code = -1;
		len = ss->copy_len;
		/* Retrieve the first byte of the code just copied. */
		if ( c == eod )
		{	/* We just copied the entire code, */
			/* so the byte we want is immediately available. */
			b = q1[1];
		}
		else
		{	/* We have to scan to the beginning of the code. */
			for ( ; c != eod; c = table[c].prefix )
				b = (byte)c;
		}
		goto add;
	}
top:	if ( code_size > bits_left )
	  {	if ( bytes_left == 0 )
		  {	if ( p == rlimit )
			  goto out;
			bytes_left = *++p;
			if_debug1('W', "[W]block count %d\n", bytes_left);
			if ( bytes_left == 0 )
			  {	status = EOFC;
				goto out;
			  }
			goto top;
		  }
		if ( low_order )
		  code = bits >> (8 - bits_left);
		else
		  code = (uint)bits << (code_size - bits_left);
		if ( bits_left + 8 < code_size )
		  {	/* Need 2 more data bytes */
			if ( bytes_left == 1 )
			  {	if ( rlimit - p < 3 )
				  goto out;
				bytes_left = p[2];
				if_debug1('W', "[W]block count %d\n",
					  bytes_left);
				if ( bytes_left == 0 )
				  {	status = EOFC;
					goto out;
				  }
				bytes_left++;
				bits = p[1];
				p++;
			  }
			else
			  {	if ( rlimit - p < 2 )
				  goto out;
				bits = p[1];
			  }
			if ( low_order )
			  code += (uint)bits << bits_left;
			else
			  code += (uint)bits << (code_size - 8 - bits_left);
			bits_left += 8;
			bits = p[2];
			p += 2;
			bytes_left -= 2;
		  }
		else
		  {	if ( p == rlimit )
			  goto out;
			bits = *++p;
			bytes_left--;
		  }
		if ( low_order )
		  code += (uint)bits << bits_left,
		  bits_left += 8 - code_size;
		else
		  bits_left += 8 - code_size,
		  code += bits >> bits_left;
	  }
	else
	  {	if ( low_order )
		  code = bits >> (8 - bits_left),
		  bits_left -= code_size;
		else
		  bits_left -= code_size,
		  code = bits >> bits_left;
	  }
	code &= code_mask;
	if_debug2('W', "[W]reading 0x%x,%d\n", code, code_size);
	/*
	 * There is an anomalous case where a code S is followed
	 * immediately by another occurrence of the S string.
	 * In this case, the next available code will be defined as
	 * S followed by the first character of S, and will be
	 * emitted immediately after the code S.  We have to
	 * recognize this case specially, by noting that the code is
	 * equal to next_code.
	 */
	if ( code >= next_code )
	{	if ( code > next_code )
		  {
#ifdef DEBUG
			lprintf2("[W]code = %d > next_code = %d\n",
				 code, next_code);
#endif
			status = ERRC;
			goto out;
		  }
		/* Fabricate the entry for the code.  It will be */
		/* overwritten immediately, of course. */
		for ( c = prev_code; c != eod; c = table[c].prefix )
			dc_next->datum = c;
		len = prev_len + 1;
		dc_next->len = min(len, 255);
		dc_next->prefix = prev_code;
		if_debug3('w', "[w]decoding anomalous 0x%x=0x%x+%c\n",
			  next_code, prev_code, dc_next->datum);
	}
	/* See if there is enough room for the code. */
	len = table[code].len;
	if ( len == 255 )
	{	/* Check for special code (reset or end). */
		/* We set their lengths to 255 to avoid doing */
		/* an extra check in the normal case. */
		if ( code == code_reset )
		{	if_debug1('w', "[w]reset: next_code was %d\n",
				  next_code);
			next_code = code_0;
			dc_next = table + code_0;
			code_size = ss->InitialCodeLength + 1;
			set_code_size();
			prev_code = -1;
			goto top;
		}
		else if ( code == eod )
		{	status = EOFC;
			goto out;
		}
		/* The code length won't fit in a byte, */
		/* compute it the hard way. */
		for ( c = code, len = 0; c != eod; len++ )
			c = table[c].prefix;
		if_debug2('w', "[w]long code %d, length=%d\n", code, len);
	}
	if ( wlimit - q < len )
	{	ss->copy_code = code;
		ss->copy_left = ss->copy_len = len;
		status = 1;
		goto out;
	}
	/* Copy the string to the buffer (back to front). */
	/* Optimize for short codes, which are the most frequent. */
	dc = &table[code];
	switch ( len )
	{
	default:
	{	byte *q1 = q += len;
		c = code;
		do
		{	*q1-- = (dc = &table[c])->datum;
		}
		while ( (c = dc->prefix) != eod );
		b = q1[1];
	}	break;
	case 3:
		q[3] = dc->datum;
		dc = &table[dc->prefix];
	case 2:
		q[2] = dc->datum;
		dc = &table[dc->prefix];
	case 1:
		q[1] = b = dc->datum;
		q += len;
	}
add:	/* Add a new entry to the table */
	if ( prev_code >= 0 )
	{	/* Unfortunately, we have to check for next_code == */
		/* lzw_decode_max every time: just checking at power */
		/* of 2 boundaries stops us one code too soon. */
		if ( next_code == lzw_decode_max )
		{	status = ERRC;
			goto out;
		}
		dc_next->datum = b;		/* added char of string */
		dc_next->len = min(prev_len, 254) + 1;
		dc_next->prefix = prev_code;
		dc_next++;
		if_debug4('W', "[W]adding 0x%x=0x%x+%c(%d)\n",
			  next_code, prev_code, b, min(len, 255));
		if ( ++next_code == switch_code )
		{	/* Crossed a power of 2. */
			/* We have to make a strange special check for */
			/* reaching the end of the code space. */
			if ( next_code < lzw_decode_max - 1 )
			{	code_size++;
				set_code_size();
				if_debug2('w', "[w]crossed power of 2: new code_size=%d, next_code=%d\n",
					  code_size, next_code);
			}
		}
	}
	prev_code = code;
	prev_len = len;
	goto top;
out:	pr->ptr = p;
	pw->ptr = q;
	ss->code_size = code_size;
	ss->prev_code = prev_code;
	ss->prev_len = prev_len;
	ss->bits = bits;
	ss->bits_left = bits_left;
	ss->bytes_left = bytes_left;
	ss->next_code = next_code;
	if_debug3('w', "[w]decoded %d bytes, prev_code=%d, next_code=%d\n",
		  (int)(q - q0), prev_code, next_code);
	return status;
}

#undef ss

/* Stream template */
const stream_template s_LZWD_template =
{	&st_LZW_state, s_LZWD_init, s_LZWD_process, 3, 1, s_LZW_release,
	s_LZW_set_defaults, s_LZWD_reset
};
