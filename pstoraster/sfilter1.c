/* Copyright (C) 1993, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* sfilter1.c */
/* Filters included in Level 1 systems: NullEncode/Decode, PFBDecode, */
/*   SubFileDecode, RunLengthEncode/Decode. */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "strimpl.h"
#include "sfilter.h"

/* ------ NullEncode/Decode ------ */

/* Process a buffer */
private int
s_Null_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	return stream_move(pr, pw);
}

/* Stream template */
const stream_template s_Null_template =
{	&st_stream_state, NULL, s_Null_process, 1, 1
};

/* ------ PFBDecode ------ */

private_st_PFBD_state();

#define ss ((stream_PFBD_state *)st)

/* Initialize the state */
private int
s_PFBD_init(stream_state *st)
{	ss->record_type = -1;
	return 0;
}

/* Process a buffer */
private int
s_PFBD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	int rcount, wcount;
	int c;
	int status = 0;
top:	rcount = pr->limit - p;
	wcount = pw->limit - q;
	switch ( ss->record_type )
	{
	case -1:			/* new record */
		if ( rcount < 2 )
			goto out;
		if ( p[1] != 0x80 )
			goto err;
		c = p[2];
		switch ( c )
		{
		case 1: case 2:
			break;
		case 3:
			status = EOFC;
			p += 2;
			goto out;
		default:
			p += 2;
			goto err;
		}
		if ( rcount < 6 )
			goto out;
		ss->record_type = c;
		ss->record_left = p[3] + ((uint)p[4] << 8) +
					((ulong)p[5] << 16) +
					((ulong)p[6] << 24);
		p += 6;
		goto top;
	case 1:				/* text data */
		/* Translate \r to \n. */
	{	int count = (wcount < rcount ? (status = 1, wcount) : rcount);
		if ( count > ss->record_left )
			count = ss->record_left,
			status = 0;
		ss->record_left -= count;
		for ( ; count != 0; count-- )
		{	c = *++p;
			*++q = (c == '\r' ? '\n' : c);
		}
	}	break;
	case 2:				/* binary data */
		if ( ss->binary_to_hex )
		{	/* Translate binary to hex. */
			int count;
			register const char _ds *hex_digits =
							"0123456789abcdef";
			wcount >>= 1;		/* 2 chars per input byte */
			count = (wcount < rcount ? (status = 1, wcount) : rcount);
			if ( count > ss->record_left )
				count = ss->record_left,
				status = 0;
			ss->record_left -= count;
			for ( ; count != 0; count-- )
			{	c = *++p;
				q[1] = hex_digits[c >> 4];
				q[2] = hex_digits[c & 0xf];
				q += 2;
			}
		}
		else
		{	/* Just read binary data. */
			int count = (wcount < rcount ? (status = 1, wcount) : rcount);
			if ( count > ss->record_left )
				count = ss->record_left,
				status = 0;
			ss->record_left -= count;
			memcpy(q + 1, p + 1, count);
			p += count;
			q += count;
		}
		break;
	   }
	if ( ss->record_left == 0 )
	{	ss->record_type = -1;
		goto top;
	}
out:	pr->ptr = p;
	pw->ptr = q;
	return status;
err:	pr->ptr = p;
	pw->ptr = q;
	return ERRC;
}

#undef ss

/* Stream template */
const stream_template s_PFBD_template =
{	&st_PFBD_state, s_PFBD_init, s_PFBD_process, 6, 2
};

/* ------ SubFileDecode ------ */

private_st_SFD_state();
/* GC procedures */
private ENUM_PTRS_BEGIN(sfd_enum_ptrs) return 0;
	ENUM_CONST_STRING_PTR(0, stream_SFD_state, eod);
} }
private RELOC_PTRS_BEGIN(sfd_reloc_ptrs) ;
	RELOC_CONST_STRING_PTR(stream_SFD_state, eod);
RELOC_PTRS_END

#define ss ((stream_SFD_state *)st)

/* Initialize the stream */
private int
s_SFD_init(stream_state *st)
{	ss->match = 0;
	ss->copy_count = 0;
	return 0;
}

/* Refill the buffer */
private int
s_SFD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{	register const byte *p = pr->ptr;
	register byte *q = pw->ptr;
	const byte *rlimit = pr->limit;
	byte *wlimit = pw->limit;
	int status = 0;
	if ( ss->eod.size == 0 )
	{	/* Just read, with no EOD pattern. */
		int rcount = rlimit - p;
		int wcount = wlimit - q;
		int count = min(rcount, wcount);
		if ( ss->count == 0 )		/* no EOD limit */
			return stream_move(pr, pw);
		else if ( ss->count > count )	/* not EOD yet */
		{	ss->count -= count;
			return stream_move(pr, pw);
		}
		else
		{	/* We're going to reach EOD. */
			count = ss->count;
			memcpy(q + 1, p + 1, count);
			pr->ptr = p + count;
			pw->ptr = q + count;
			return EOFC;
		}
	}
	else
	{	/* Read looking for an EOD pattern. */
		const byte *pattern = ss->eod.data;
		uint match = ss->match;
cp:		/* Check whether we're still copying a partial match. */
		if ( ss->copy_count )
		{	int count = min(wlimit - q, ss->copy_count);
			memcpy(q + 1, ss->eod.data + ss->copy_ptr, count);
			ss->copy_count -= count;
			ss->copy_ptr += count;
			q += count;
			if ( ss->copy_count != 0 )	/* hit wlimit */
			{	status = 1;
				goto xit;
			}
			else if ( ss->count < 0 )
			{	status = EOFC;
				goto xit;
			}
		}
		while ( p < rlimit )
		{	int c = *++p;
			if ( c == pattern[match] )
			{	if ( ++match == ss->eod.size )
				{	switch ( ss->count )
					{
					case 0:
						status = EOFC;
						goto xit;
					case 1:
						ss->count = -1;
						break;
					default:
						ss->count--;
					}
					ss->copy_ptr = 0;
					ss->copy_count = match;
					match = 0;
					goto cp;
				}
				continue;
			}
			/* No match here, back up to find the longest one. */
			/* This may be quadratic in string_size, but */
			/* we don't expect this to be a real problem. */
			if ( match > 0 )
			  {	int end = match;
				while ( match > 0 )
				  {	match--;
					if ( !memcmp(pattern,
						     pattern + end - match,
						     match)
					   )
					  break;
				  }
				/* Copy the unmatched initial portion of */
				/* the EOD string to the output. */
				p--;
				ss->copy_ptr = 0;
				ss->copy_count = end - match;
				goto cp;
			  }
			if ( q == wlimit )
			{	p--;
				status = 1;
				break;
			}
			*++q = c;
		}
xit:		pr->ptr = p;
		pw->ptr = q;
		ss->match = match;
	}
	return status;
}

#undef ss

/* Stream template */
const stream_template s_SFD_template =
{	&st_SFD_state, s_SFD_init, s_SFD_process, 1, 1
};
