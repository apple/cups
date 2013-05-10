/* Copyright (C) 1993, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* sfile.c */
/* File stream implementation */
#include "stdio_.h"		/* includes std.h */
#include "memory_.h"
#include "gdebug.h"
#include "gpcheck.h"
#include "stream.h"
#include "strimpl.h"

/* Forward references for file stream procedures */
private int
  s_file_available(P2(stream *, long *)),
  s_file_read_seek(P2(stream *, long)),
  s_file_read_close(P1(stream *)),
  s_file_read_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool));
private int
  s_file_write_seek(P2(stream *, long)),
  s_file_write_flush(P1(stream *)),
  s_file_write_close(P1(stream *)),
  s_file_write_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool));
private int
  s_file_switch(P2(stream *, bool));

/* ------ File streams ------ */

/* Initialize a stream for reading an OS file. */
void
sread_file(register stream *s, FILE *file, byte *buf, uint len)
{	static const stream_procs p =
	   {	s_file_available, s_file_read_seek, s_std_read_reset,
		s_std_read_flush, s_file_read_close, s_file_read_process,
		s_file_switch
	   };
	/*
	 * There is no really portable way to test seekability,
	 * but this should work on most systems.
	 * Note that if our probe sets the ferror bit for the stream,
	 * we have to clear it again to avoid trouble later.
	 */
	int had_error = ferror(file);
	long curpos = ftell(file);
	bool seekable = (curpos != -1L && fseek(file, curpos, SEEK_SET) == 0);

	if ( !had_error )
	  clearerr(file);
	s_std_init(s, buf, len, &p,
		   (seekable ? s_mode_read + s_mode_seek : s_mode_read));
	if_debug1('s', "[s]read file=0x%lx\n", (ulong)file);
	s->file = file;
	s->file_modes = s->modes;
}
/* Procedures for reading from a file */
private int
s_file_available(register stream *s, long *pl)
{	*pl = sbufavailable(s);
	if ( sseekable(s) )
	   {	long pos, end;
		pos = ftell(s->file);
		if ( fseek(s->file, 0L, SEEK_END) ) return ERRC;
		end = ftell(s->file);
		if ( fseek(s->file, pos, SEEK_SET) ) return ERRC;
		*pl += end - pos;
		if ( *pl == 0 ) *pl = -1;	/* EOF */
	   }
	else
	   {	if ( *pl == 0 && feof(s->file) ) *pl = -1;	/* EOF */
	   }
	return 0;
}
private int
s_file_read_seek(register stream *s, long pos)
{	uint end = s->srlimit - s->cbuf + 1;
	long offset = pos - s->position;
	if ( offset >= 0 && offset <= end )
	   {	/* Staying within the same buffer */
		s->srptr = s->cbuf + offset - 1;
		return 0;
	   }
	if ( fseek(s->file, pos, SEEK_SET) != 0 )
		return ERRC;
	s->srptr = s->srlimit = s->cbuf - 1;
	s->end_status = 0;
	s->position = pos;
	return 0;
}
private int
s_file_read_close(stream *s)
{	FILE *file = s->file;
	if ( file != 0 )
	  {	s->file = 0;
		return fclose(file);
	  }
	return 0;
}

/* Initialize a stream for writing an OS file. */
void
swrite_file(register stream *s, FILE *file, byte *buf, uint len)
{	static const stream_procs p =
	   {	s_std_noavailable, s_file_write_seek, s_std_write_reset,
		s_file_write_flush, s_file_write_close, s_file_write_process,
		s_file_switch
	   };
	s_std_init(s, buf, len, &p,
		   (file == stdout ? s_mode_write : s_mode_write + s_mode_seek));
	if_debug1('s', "[s]write file=0x%lx\n", (ulong)file);
	s->file = file;
	s->file_modes = s->modes;
}
/* Initialize for appending to an OS file. */
void
sappend_file(register stream *s, FILE *file, byte *buf, uint len)
{	swrite_file(s, file, buf, len);
	s->modes = s_mode_write + s_mode_append;	/* no seek */
	s->file_modes = s->modes;
	fseek(file, 0L, SEEK_END);
	s->position = ftell(file);
}
/* Procedures for writing on a file */
private int
s_file_write_seek(stream *s, long pos)
{	/* We must flush the buffer to reposition. */
	int code = sflush(s);
	if ( code < 0 )
		return code;
	if ( fseek(s->file, pos, SEEK_SET) != 0 )
		return ERRC;
	s->position = pos;
	return 0;
}
private int
s_file_write_flush(register stream *s)
{	int result = s_process_write_buf(s, false);
	fflush(s->file);
	return result;
}
private int
s_file_write_close(register stream *s)
{	s_process_write_buf(s, true);
	return s_file_read_close(s);
}

#define ss ((stream *)st)

/* Process a buffer for a file reading stream. */
/* This is the first stream in the pipeline, so pr is irrelevant. */
private int
s_file_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{	FILE *file = ss->file;
	int count = fread(pw->ptr + 1, 1, (uint)(pw->limit - pw->ptr), file);
	if ( count < 0 )
		count = 0;
	pw->ptr += count;
	process_interrupts();
	return (ferror(file) ? ERRC : feof(file) ? EOFC : 1);
}

/* Process a buffer for a file writing stream. */
/* This is the last stream in the pipeline, so pw is irrelevant. */
private int
s_file_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	/* The DEC C library on AXP architectures gives an error on */
	/* fwrite if the count is zero! */
	uint count = pr->limit - pr->ptr;
	if ( count != 0 )
	  {	FILE *file = ss->file;
		int written = fwrite(pr->ptr + 1, 1, count, file);
		if ( written < 0 )
		  written = 0;
		pr->ptr += written;
		process_interrupts();
		return (ferror(file) ? ERRC : 0);
	  }
	else
	  {	process_interrupts();
		return 0;
	  }
}

#undef ss

/* Switch a file stream to reading or writing. */
private int
s_file_switch(stream *s, bool writing)
{	uint modes = s->file_modes;
	FILE *file = s->file;
	long pos;
	if ( writing )
	  {	if ( !(s->file_modes & s_mode_write) )
		  return ERRC;
		pos = stell(s);
		if_debug2('s', "[s]switch 0x%lx to write at %ld\n",
			  (ulong)s, pos);
		fseek(file, pos, SEEK_SET);
		if ( modes & s_mode_append )
		  {	sappend_file(s, file, s->cbuf, s->cbsize);	/* sets position */
		  }
		else
		  {	swrite_file(s, file, s->cbuf, s->cbsize);
			s->position = pos;
		  }
		s->modes = modes;
	  }
	else
	  {	if ( !(s->file_modes & s_mode_read) )
		  return ERRC;
		pos = stell(s);
		if_debug2('s', "[s]switch 0x%lx to read at %ld\n",
			  (ulong)s, pos);
		if ( sflush(s) < 0 )
		  return ERRC;
		fseek(file, 0L, SEEK_CUR);		/* pacify C library */
		sread_file(s, file, s->cbuf, s->cbsize);
		s->modes |= modes & s_mode_append;	/* don't lose append info */
		s->position = pos;
	  }
	s->file_modes = modes;
	return 0;
}
