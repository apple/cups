/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* stream.c */
/* Stream package for Ghostscript interpreter */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "gdebug.h"
#include "gpcheck.h"
#include "stream.h"
#include "strimpl.h"

/* Forward declarations */
private int sreadbuf(P2(stream *, stream_cursor_write *));
private int swritebuf(P3(stream *, stream_cursor_read *, bool));
private void stream_compact(P2(stream *, bool));

/* Structure types for allocating streams. */
private_st_stream();
public_st_stream_state();	/* default */
/* GC procedures */
#define st ((stream *)vptr)
private ENUM_PTRS_BEGIN(stream_enum_ptrs) return 0;
	case 0:
		if ( st->foreign )
		  *pep = NULL;
		else if ( st->cbuf_string.data != 0 )
		  { ENUM_RETURN_STRING_PTR(stream, cbuf_string);
		  }
		else
		  *pep = st->cbuf;
		break;
	ENUM_PTR(1, stream, strm);
	ENUM_PTR(2, stream, prev);
	ENUM_PTR(3, stream, next);
	ENUM_PTR(4, stream, state);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(stream_reloc_ptrs) {
	byte *cbuf_old = st->cbuf;
	if ( cbuf_old != 0 && !st->foreign )
	{	long reloc;
		if ( st->cbuf_string.data != 0 )
		  {	RELOC_STRING_PTR(stream, cbuf_string);
			st->cbuf = st->cbuf_string.data;
		  }
		else
		  RELOC_PTR(stream, cbuf);
		reloc = cbuf_old - st->cbuf;
		/* Relocate the other buffer pointers. */
		st->srptr -= reloc;
		st->srlimit -= reloc;		/* same as swptr */
		st->swlimit -= reloc;
	}
	RELOC_PTR(stream, strm);
	RELOC_PTR(stream, prev);
	RELOC_PTR(stream, next);
	RELOC_PTR(stream, state);
} RELOC_PTRS_END
/* Finalize a stream by closing it. */
/* We only do this for file streams, because other kinds of streams */
/* may attempt to free storage when closing. */
private void
stream_finalize(void *vptr)
{	if_debug2('u', "[u]%s 0x%lx\n",
		  (!s_is_valid(st) ? "already closed:" :
		   st->is_temp ? "is_temp set:" :
		   st->file == 0 ? "not file:" :
		   "closing file:"), (ulong)st);
	if ( s_is_valid(st) && !st->is_temp && st->file != 0 )
	    {	/* Prevent any attempt to free the buffer. */
		st->cbuf = 0;
		st->cbuf_string.data = 0;
		sclose(st);		/* ignore errors */
	    }
}
#undef st

/* Dummy template for streams that don't have a separate state. */
private const stream_template s_no_template =
{	&st_stream_state, 0, 0, 1, 1, 0
};

/* ------ Generic procedures ------ */

/* Allocate a stream and initialize it minimally. */
stream *
s_alloc(gs_memory_t *mem, client_name_t cname)
{	stream *s = gs_alloc_struct(mem, stream, &st_stream, cname);
	if_debug2('s', "[s]alloc(%s) = 0x%lx\n",
		  client_name_string(cname), (ulong)s);
	if ( s == 0 )
	  return 0;
	s->memory = mem;
	s->report_error = s_no_report_error;
	s->prev = s->next = 0;		/* clean for GC */
	return s;
}

/* Allocate a stream state and initialize it minimally. */
stream_state *
s_alloc_state(gs_memory_t *mem, gs_memory_type_ptr_t stype,
  client_name_t cname)
{	stream_state *st = gs_alloc_struct(mem, stream_state, stype, cname);
	if_debug3('s', "[s]alloc_state %s(%s) = 0x%lx\n",
		  client_name_string(cname),
		  client_name_string(stype->sname),
		  (ulong)st);
	if ( st == 0 )
	  return 0;
	st->memory = mem;
	st->report_error = s_no_report_error;
	return st;
}

/* Vacuous stream initialization */
int
s_no_init(stream_state *st)
{	return 0;
}

/* Standard stream initialization */
void
s_std_init(register stream *s, byte *ptr, uint len, const stream_procs *pp,
  int modes)
{	s->template = &s_no_template;
	s->cbuf = ptr;
	s->srptr = s->srlimit = s->swptr = ptr - 1;
	s->swlimit = ptr - 1 + len;
	s->end_status = 0;
	s->foreign = 0;
	s->modes = modes;
	s->cbuf_string.data = 0;
	s->position = 0;
	s->bsize = s->cbsize = len;
	s->strm = 0;			/* not a filter */
	s->is_temp = 0;
	s->procs = *pp;
	s->state = (stream_state *)s;	/* hack to avoid separate state */
	s->file = 0;
	if_debug4('s', "[s]init 0x%lx, buf=0x%lx, len=%u, modes=%d\n",
		  (ulong)s, (ulong)ptr, len, modes);
}

/* Implement a stream procedure as a no-op. */
int
s_std_null(stream *s)
{	return 0;
}

/* Discard the contents of the buffer when reading. */
void
s_std_read_reset(stream *s)
{	s->srptr = s->srlimit = s->cbuf - 1;
}

/* Discard the contents of the buffer when writing. */
void
s_std_write_reset(stream *s)
{	s->swptr = s->cbuf- 1;
}

/* Flush data to end-of-file when reading. */
int
s_std_read_flush(stream *s)
{	while ( 1 )
	{	s->srptr = s->srlimit = s->cbuf - 1;
		if ( s->end_status ) break;
		s_process_read_buf(s);
	}
	return (s->end_status == EOFC ? 0 : s->end_status);
}

/* Flush buffered data when writing. */
int
s_std_write_flush(stream *s)
{	return s_process_write_buf(s, false);
}

/* Indicate that the number of available input bytes is unknown. */
int
s_std_noavailable(stream *s, long *pl)
{	*pl = -1;
	return 0;
}

/* Indicate an error when asked to seek. */
int
s_std_noseek(stream *s, long pos)
{	return ERRC;
}

/* Standard stream closing. */
int
s_std_close(stream *s)
{	return 0;
}

/* Standard stream mode switching. */
int
s_std_switch_mode(stream *s, bool writing)
{	return ERRC;
}

/* Standard stream finalization.  Disable the stream. */
void
s_disable(register stream *s)
{	s->cbuf = 0;
	s->bsize = 0;
	s->end_status = EOFC;
	s->modes = 0;
	s->cbuf_string.data = 0;
	s->cursor.r.ptr = s->cursor.r.limit = (const byte *)0 - 1;
	s->cursor.w.limit = (byte *)0 - 1;
	s->procs.close = s_std_null;
	/* Clear pointers for GC */
	s->strm = 0;
	s->state = (stream_state *)s;
	s->template = &s_no_template;
	/****** SHOULD DO MORE THAN THIS ******/
	if_debug1('s', "[s]disable 0x%lx\n", (ulong)s);
}

/* Implement flushing for encoding filters. */
int
s_filter_write_flush(register stream *s)
{	int status = s_process_write_buf(s, false);
	if ( status != 0 )
	  return status;
	return sflush(s->strm);
}

/* Close a filter.  If this is an encoding filter, flush it first. */
int
s_filter_close(register stream *s)
{	if ( s_is_writing(s) )
	{	int status = s_process_write_buf(s, true);
		if ( status != 0 && status != EOFC )
		  return status;
	}
	return s_std_close(s);
}	

/* Disregard a stream error message. */
int
s_no_report_error(stream_state *st, const char *str)
{	return 0;
}

/* ------ Implementation-independent procedures ------ */

/* Store the amount of available data in a(n input) stream. */
int
savailable(stream *s, long *pl)
{	return (*(s)->procs.available)(s, pl);
}

/* Return the current position of a stream. */
long
stell(stream *s)
{	/* The stream might have been closed, but the position */
	/* is still meaningful in this case. */
	const byte *ptr = (s_is_writing(s) ? s->swptr : s->srptr);
	return (ptr == 0 ? 0 : ptr + 1 - s->cbuf) + s->position;
}

/* Set the position of a stream. */
int
spseek(stream *s, long pos)
{	if_debug3('s', "[s]seek 0x%lx to %ld, position was %ld\n",
		  (ulong)s, pos, stell(s));
	return (*(s)->procs.seek)(s, pos);
}

/* Switch a stream to read or write mode. */
/* Return 0 or ERRC. */
int
sswitch(register stream *s, bool writing)
{	if ( s->procs.switch_mode == 0 )
	  return ERRC;
	return (*s->procs.switch_mode)(s, writing);
}

/* Close a stream, disabling it if successful. */
/* (The stream may already be closed.) */
int
sclose(register stream *s)
{	stream_state *st;
	int code = (*s->procs.close)(s);
	if ( code < 0 )
	  return code;
	st = s->state;
	if ( st != 0 )
	  { stream_proc_release((*release)) = st->template->release;
	    if ( release != 0 )
	      (*release)(st);
	    if ( st != (stream_state *)s && st->memory != 0 )
	      gs_free_object(st->memory, st, "s_std_close");
	    s->state = (stream_state *)s;
	  }
	s_disable(s);
	return code;
}

/*
 * Implement sgetc when the buffer may be empty.
 * If the buffer really is empty, refill it and then read a byte.
 * Note that filters must read one byte ahead, so that they can close immediately
 * after the client reads the last data byte if the next thing is an EOD.
 */
int
spgetcc(register stream *s, bool close_on_eof)
{	int status, left;
	int min_left = sbuf_min_left(s);

	while ( status = s->end_status,
		left = s->srlimit - s->srptr,
		left <= min_left && status >= 0
	      )
	  s_process_read_buf(s);
	if ( left <= min_left && (left == 0 || (status != EOFC && status != ERRC)) )
	  { /* Compact the stream so stell will return the right result. */
	    stream_compact(s, true);
	    if ( status == EOFC && close_on_eof )
	      { status = sclose(s);
	        if ( status == 0 )
		  status = EOFC;
		s->end_status = status;
	      }
	    return status;
	  }
	return *++(s->srptr);
}

/* Implementing sputc when the buffer is full, */
/* by flushing the buffer and then writing the byte. */
int
spputc(register stream *s, byte b)
{	for ( ; ; )
	{	if ( s->end_status )
		  return s->end_status;
		if ( !sendwp(s) )
		{	*++(s->swptr) = b;
			return b;
		}
		s_process_write_buf(s, false);
	}
}

/* Push back a character onto a (read) stream. */
/* The character must be the same as the last one read. */
/* Return 0 on success, ERRC on failure. */
int
sungetc(register stream *s, byte c)
{	if ( !s_is_reading(s) || s->srptr < s->cbuf || *(s->srptr) != c )
	  return ERRC;
	s->srptr--;
	return 0;
}

/* Get a string from a stream. */
/* Return 0 if the string was filled, or an exception status. */
int
sgets(stream *s, byte *buf, uint nmax, uint *pn)
{	stream_cursor_write cw;
	int status = 0;
	int min_left = sbuf_min_left(s);

	cw.ptr = buf - 1;
	cw.limit = cw.ptr + nmax;
	while ( cw.ptr < cw.limit )
	{	int left;
		if ( (left = s->srlimit - s->srptr) > min_left )
		  { s->srlimit -= min_left;
		    stream_move(&s->cursor.r, &cw);
		    s->srlimit += min_left;
		  }
		else
		{	uint wanted = cw.limit - cw.ptr;
			int c;
			stream_state *st;
			if ( wanted >= s->bsize >> 2 &&
			     (st = s->state) != 0 &&
			     wanted >= st->template->min_out_size &&
			     s->end_status == 0 &&
			     left == 0
			   )
			{	byte *wptr = cw.ptr;
				cw.limit -= min_left;
				status = sreadbuf(s, &cw);
				cw.limit += min_left;
				/* We know the stream buffer is empty, */
				/* so it's safe to update position. */
				s->position += cw.ptr - wptr;
				if ( status != 1 || cw.ptr == cw.limit )
				  break;
			}
			c = spgetc(s);
			if ( c < 0 )
			{	status = c;
				break;
			}
			*++(cw.ptr) = c;
		}
	}
	*pn = cw.ptr + 1 - buf;
	return (status >= 0 ? 0 : status);
}

/* Write a string on a stream. */
/* Return 0 if the entire string was written, or an exception status. */
int
sputs(register stream *s, const byte *str, uint wlen, uint *pn)
{	uint len = wlen;
	int status = s->end_status;
	if ( status >= 0 )
	  while ( len > 0 )
	{	uint count = s->swlimit - s->swptr;
		if ( count > 0 )
		{	if ( count > len ) count = len;
			memcpy(s->swptr + 1, str, count);
			s->swptr += count;
			str += count;
			len -= count;
		}
		else
		{	byte ch = *str++;
			status = sputc(s, ch);
			if ( status < 0 )
			  break;
			len--;
		}
	}
	*pn = wlen - len;
	return (status >= 0 ? 0 : status);
}

/* Skip ahead a specified distance in a read stream. */
/* Return 0 or an exception code. */
/* Store the number of bytes skipped in *pskipped. */
int
spskip(register stream *s, long nskip, long *pskipped)
{	long n = nskip;
	int min_left;

	if ( nskip < 0 || !s_is_reading(s) )
	  { *pskipped = 0;
	    return ERRC;
	  }
	if ( s_can_seek(s) )
	  { long pos = stell(s);
	    int code = sseek(s, pos + n);
	    *pskipped = stell(s) - pos;
	    return code;
	  }
	min_left = sbuf_min_left(s);
	while ( sbufavailable(s) < n + min_left )
	  {	int code;
		n -= sbufavailable(s);
		s->srptr = s->srlimit;
		if ( s->end_status )
		  { *pskipped = nskip - n;
		    return s->end_status;
		  }
		code = sgetc(s);
		if ( code < 0 )
		  { *pskipped = nskip - n;
		    return code;
		  }
		--n;
	  }
	/* Note that if min_left > 0, n < 0 is possible; this is harmless. */
	s->srptr += n;
	*pskipped = nskip;
	return 0;
}	

/* ------ Utilities ------ */

/*
 * Attempt to refill the buffer of a read stream.  Only call this if the
 * end_status is not EOFC, and if the buffer is (nearly) empty.
 */
int
s_process_read_buf(stream *s)
{	int status;
	stream_compact(s, false);
	status = sreadbuf(s, &s->cursor.w);
	s->end_status = (status >= 0 ? 0 : status);
	return 0;
}

/*
 * Attempt to empty the buffer of a write stream.  Only call this if the
 * end_status is not EOFC.
 */
int
s_process_write_buf(stream *s, bool last)
{	int status = swritebuf(s, &s->cursor.r, last);
	stream_compact(s, false);
	if ( status >= 0 )
	  status = 0;
	s->end_status = status;
	return status;
}

/* Move forward or backward in a pipeline.  We temporarily reverse */
/* the direction of the pointers while doing this. */
/* (Cf the Deutsch-Schorr-Waite graph marking algorithm.) */
#define move_back(curr, prev)\
{ stream *back = prev->strm;\
  prev->strm = curr; curr = prev; prev = back;\
}
#define move_ahead(curr, prev)\
{ stream *ahead = curr->strm;\
  curr->strm = prev; prev = curr; curr = ahead;\
}

/* Read from a pipeline. */
private int
sreadbuf(stream *s, stream_cursor_write *pbuf)
{	stream *prev = 0;
	stream *curr = s;
	int status;
	for ( ; ; )
	{	stream *strm;
		for ( ; ; )
		{	/* Descend into the recursion. */
			stream_cursor_read cr;
			stream_cursor_read *pr;
			stream_cursor_write *pw;
			bool eof;

			strm = curr->strm;
			if ( strm == 0 )
			  { cr.ptr = 0, cr.limit = 0;
			    pr = &cr;
			    eof = false;
			  }
			else
			  { pr = &strm->cursor.r;
			    eof = strm->end_status == EOFC;
			  }
			pw = (prev == 0 ? pbuf : &curr->cursor.w);
			if_debug4('s', "[s]read process 0x%lx, nr=%u, nw=%u, eof=%d\n",
				  (ulong)curr, (uint)(pr->limit - pr->ptr),
				  (uint)(pw->limit - pw->ptr), eof);
			status = (*curr->procs.process)(curr->state, pr, pw, eof);
			if_debug4('s', "[s]after read 0x%lx, nr=%u, nw=%u, status=%d\n",
				  (ulong)curr, (uint)(pr->limit - pr->ptr),
				  (uint)(pw->limit - pw->ptr), status);
			if ( strm == 0 || status != 0 )
			  break;
			status = strm->end_status;
			if ( status < 0 )
			  break;
			move_ahead(curr, prev);
			stream_compact(curr, false);
		}
		/* If curr reached EOD and is a filter stream, close it. */
		if ( strm != 0 && status == EOFC &&
		     curr->cursor.r.ptr >= curr->cursor.r.limit
		   )
		  { int cstat = sclose(curr);
		    if ( cstat != 0 )
		      status = cstat;
		  }
#if 0
		/* If we need to do a callout, unwind all the way now. */
		if ( status == CALLC )
		  { while ( (curr->end_status = status), prev != 0 )
		      { move_back(curr, prev);
		      }
		    return status;
		  }
#endif
		/* Unwind from the recursion. */
		curr->end_status = (status >= 0 ? 0 : status);
		if ( prev == 0 )
		  return status;
		move_back(curr, prev);
	}
}

/* Write to a pipeline. */
private int
swritebuf(stream *s, stream_cursor_read *pbuf, bool last)
{	stream *prev = 0;
	stream *curr = s;
	int depth = 1;		/* depth of nesting in non-temp streams */
	int level = 0;		/* depth of recursion */
	int top_level = 0;	/* level below which all streams have */
				/* returned 0 when called with last = true */
	int status;

	for ( ; ; )
	{	for ( ; ; )
		{	/* Descend into the recursion. */
			stream *strm = curr->strm;
			stream_cursor_write cw;
			stream_cursor_read *pr;
			stream_cursor_write *pw;
			/*
			 * We only want to set the last/end flag for
			 * the top-level stream and any temporary streams
			 * immediately below it.
			 */
			bool end = last && depth <= 1 && level == top_level;

			if ( strm == 0 )
			  cw.ptr = 0, cw.limit = 0, pw = &cw;
			else
			  pw = &strm->cursor.w;
			if ( prev == 0 )
			  pr = pbuf;
			else
			  pr = &curr->cursor.r;
			if_debug4('s', "[s]write process 0x%lx, nr=%u, nw=%u, end=%d\n",
				  (ulong)curr, (uint)(pr->limit - pr->ptr),
				  (uint)(pw->limit - pw->ptr), end);
			status = (*curr->procs.process)(curr->state, pr, pw,
							end);
			if_debug4('s', "[s]after write 0x%lx, nr=%u, nw=%u, status=%d\n",
				  (ulong)curr, (uint)(pr->limit - pr->ptr),
				  (uint)(pw->limit - pw->ptr), status);
			if ( strm == 0 || status < 0 )
			  break;
			if ( status == 1 )
			  end = false;
			else
			{	/* Keep going if we are closing */
				/* a filter with a sub-stream. */
				/* We know status == 0. */
				if ( !end || !strm->is_temp )
				  break;
				/* This level is finished, don't come back. */
				top_level = level + 1;
			}
			status = strm->end_status;
			if ( status < 0 )
			  break;
			move_ahead(curr, prev);
			stream_compact(curr, false);
			++level;
			if ( !curr->is_temp )
			  ++depth;
		}
		/* Unwind from the recursion. */
		curr->end_status = (status >= 0 ? 0 : status);
		if ( level <= top_level )
		  { /* All streams above here were called with last = true */
		    /* and returned 0: finish unwinding and then return. */
		    while ( prev )
		      { move_back(curr, prev);
			curr->end_status = (status >= 0 ? 0 : status);
		      }
		    return status;
		  }
		move_back(curr, prev);
		--level;
		if ( !curr->is_temp )
		  --depth;
	}
}

/* Move as much data as possible from one buffer to another. */
/* Return 0 if the input became empty, 1 if the output became full. */
int
stream_move(stream_cursor_read *pr, stream_cursor_write *pw)
{	uint rcount = pr->limit - pr->ptr;
	uint wcount = pw->limit - pw->ptr;
	uint count;
	int status;
	if ( rcount <= wcount )
	  count = rcount, status = 0;
	else
	  count = wcount, status = 1;
	memmove(pw->ptr + 1, pr->ptr + 1, count);
	pr->ptr += count;
	pw->ptr += count;
	return status;
}

/* If possible, compact the information in a stream buffer to the bottom. */
private void
stream_compact(stream *s, bool always)
{	if ( s->cursor.r.ptr >= s->cbuf && (always || s->end_status >= 0) )
	{	uint dist = s->cursor.r.ptr + 1 - s->cbuf;
		memmove(s->cbuf, s->cursor.r.ptr + 1,
			(uint)(s->cursor.r.limit - s->cursor.r.ptr));
		s->cursor.r.ptr = s->cbuf - 1;
		s->cursor.r.limit -= dist;		/* same as w.ptr */
		s->position += dist;
	}
}

/* ------ String streams ------ */

/* String stream procedures */
private int
  s_string_available(P2(stream *, long *)),
  s_string_read_seek(P2(stream *, long)),
  s_string_write_seek(P2(stream *, long)),
  s_string_read_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool)),
  s_string_write_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool));

/* Initialize a stream for reading a string. */
void
sread_string(register stream *s, const byte *ptr, uint len)
{	static const stream_procs p =
	   {	s_string_available, s_string_read_seek, s_std_read_reset,
		s_std_read_flush, s_std_null, s_string_read_process
	   };
	s_std_init(s, (byte *)ptr, len, &p, s_mode_read + s_mode_seek);
	s->cbuf_string.data = (byte *)ptr;
	s->cbuf_string.size = len;
	s->end_status = EOFC;
	s->srlimit = s->swlimit;
}
/* Return the number of available bytes when reading from a string. */
private int
s_string_available(stream *s, long *pl)
{	*pl = sbufavailable(s);
	if ( *pl == 0 )		/* EOF */
	  *pl = -1;
	return 0;
}

/* Seek in a string being read.  Return 0 if OK, ERRC if not. */
private int
s_string_read_seek(register stream *s, long pos)
{	if ( pos < 0 || pos > s->bsize ) return ERRC;
	s->srptr = s->cbuf + pos - 1;
	return 0;
}

/* Initialize a stream for writing a string. */
void
swrite_string(register stream *s, byte *ptr, uint len)
{	static const stream_procs p =
	   {	s_std_noavailable, s_string_write_seek, s_std_write_reset,
		s_std_null, s_std_null, s_string_write_process
	   };
	s_std_init(s, ptr, len, &p, s_mode_write + s_mode_seek);
	s->cbuf_string.data = ptr;
	s->cbuf_string.size = len;
}

/* Seek in a string being written.  Return 0 if OK, ERRC if not. */
private int
s_string_write_seek(register stream *s, long pos)
{	if ( pos < 0 || pos > s->bsize ) return ERRC;
	s->swptr = s->cbuf + pos - 1;
	return 0;
}

/* Since we initialize the input buffer of a string read stream */
/* to contain all of the data in the string, if we are ever asked */
/* to refill the buffer, we should signal EOF. */
private int
s_string_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{	return EOFC;
}
/* Similarly, if we are ever asked to empty the buffer, it means that */
/* there has been an overrun (unless we are closing the stream). */
private int
s_string_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	return (last ? EOFC : ERRC);
}
