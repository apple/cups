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

/* sfileno.c */
/* File stream implementation using direct OS calls */
/******
 ****** NOTE: THIS FILE PROBABLY WILL NOT COMPILE ON NON-UNIX
 ****** PLATFORMS, AND IT MAY REQUIRE EDITING ON SOME UNIX PLATFORMS.
 ******/
#include "stdio_.h"		/* includes std.h */
#include "errno_.h"
#include "memory_.h"
/*
 * It's likely that you will have to edit the next line on some Unix
 * and most non-Unix platforms, since there is no standard (ANSI or
 * otherwise) for where to find these definitions.
 */
#include <unistd.h>		/* for read, write, fsync, lseek */
#include "gdebug.h"
#include "gpcheck.h"
#include "stream.h"
#include "strimpl.h"

/*
 * This is an alternate implementation of file streams.  It still uses
 * FILE * in the interface, but it uses direct OS calls for I/O.
 * It also includes workarounds for the nasty habit of System V Unix
 * of breaking out of read and write operations with EINTR, EAGAIN,
 * and/or EWOULDBLOCK "errors".
 *
 * The interface should be identical to that of sfile.c.  However,
 * in order to allow both implementations to exist in the same executable,
 * we use different names for sread_file, swrite_file, and sappend_file
 * (the public procedures).  If you want to use this implementation
 * instead of the one in sfile.c, uncomment the following #define.
 */
#define USE_SFILENO_FOR_SFILE

#ifdef USE_SFILENO_FOR_SFILE
#  define sread_fileno sread_file
#  define swrite_fileno swrite_file
#  define sappend_fileno sappend_file
#endif

/* Forward references for file stream procedures */
private int
  s_fileno_available(P2(stream *, long *)),
  s_fileno_read_seek(P2(stream *, long)),
  s_fileno_read_close(P1(stream *)),
  s_fileno_read_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool));
private int
  s_fileno_write_seek(P2(stream *, long)),
  s_fileno_write_flush(P1(stream *)),
  s_fileno_write_close(P1(stream *)),
  s_fileno_write_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool));
private int
  s_fileno_switch(P2(stream *, bool));

/* ------ File streams ------ */

/* Get the file descriptor number of a stream. */
#define sfileno(s) fileno((s)->file)

/* Initialize a stream for reading an OS file. */
void
sread_fileno(register stream *s, FILE *file, byte *buf, uint len)
{	static const stream_procs p =
	   {	s_fileno_available, s_fileno_read_seek, s_std_read_reset,
		s_std_read_flush, s_fileno_read_close, s_fileno_read_process,
		s_fileno_switch
	   };
	/*
	 * There is no really portable way to test seekability,
	 * but this should work on most systems.
	 */
	int fd = fileno(file);
	long curpos = lseek(fd, 0L, SEEK_CUR);
	bool seekable = (curpos != -1L && lseek(fd, curpos, SEEK_SET) != -1L);

	s_std_init(s, buf, len, &p,
		   (seekable ? s_mode_read + s_mode_seek : s_mode_read));
	if_debug2('s', "[s]read file=0x%lx, fd=%d\n", (ulong)file,
		  fileno(file));
	s->file = file;
	s->file_modes = s->modes;
}
/* Procedures for reading from a file */
private int
s_fileno_available(register stream *s, long *pl)
{	int fd = sfileno(s);
	*pl = sbufavailable(s);
	if ( sseekable(s) )
	   {	long pos, end;
		pos = lseek(fd, 0L, SEEK_CUR);
		if ( pos < 0 )
		  return ERRC;
		end = lseek(fd, 0L, SEEK_END);
		if ( lseek(fd, pos, SEEK_SET) < 0 || end < 0 )
		  return ERRC;
		*pl += end - pos;
		if ( *pl == 0 ) *pl = -1;	/* EOF */
	   }
	else
	   {	if ( *pl == 0 ) *pl = -1;	/* EOF */
	   }
	return 0;
}
private int
s_fileno_read_seek(register stream *s, long pos)
{	uint end = s->srlimit - s->cbuf + 1;
	long offset = pos - s->position;
	if ( offset >= 0 && offset <= end )
	   {	/* Staying within the same buffer */
		s->srptr = s->cbuf + offset - 1;
		return 0;
	   }
	if ( lseek(sfileno(s), pos, SEEK_SET) < 0 )
	  return ERRC;
	s->srptr = s->srlimit = s->cbuf - 1;
	s->end_status = 0;
	s->position = pos;
	return 0;
}
private int
s_fileno_read_close(stream *s)
{	FILE *file = s->file;
	if ( file != 0 )
	  {	s->file = 0;
		return fclose(file);
	  }
	return 0;
}

/* Initialize a stream for writing an OS file. */
void
swrite_fileno(register stream *s, FILE *file, byte *buf, uint len)
{	static const stream_procs p =
	   {	s_std_noavailable, s_fileno_write_seek, s_std_write_reset,
		s_fileno_write_flush, s_fileno_write_close, s_fileno_write_process,
		s_fileno_switch
	   };
	s_std_init(s, buf, len, &p,
		   (file == stdout ? s_mode_write : s_mode_write + s_mode_seek));
	if_debug2('s', "[s]write file=0x%lx, fd=%d\n", (ulong)file,
		  fileno(file));
	s->file = file;
	s->file_modes = s->modes;
}
/* Initialize for appending to an OS file. */
void
sappend_fileno(register stream *s, FILE *file, byte *buf, uint len)
{	swrite_fileno(s, file, buf, len);
	s->modes = s_mode_write + s_mode_append;	/* no seek */
	s->file_modes = s->modes;
	s->position = lseek(fileno(file), 0L, SEEK_END);
}
/* Procedures for writing on a file */
private int
s_fileno_write_seek(stream *s, long pos)
{	/* We must flush the buffer to reposition. */
	int code = sflush(s);
	if ( code < 0 )
	  return code;
	if ( lseek(sfileno(s), pos, SEEK_SET) < 0 )
	  return ERRC;
	s->position = pos;
	return 0;
}
private int
s_fileno_write_flush(register stream *s)
{	int result = s_process_write_buf(s, false);
	discard(fsync(sfileno(s)));
	return result;
}
private int
s_fileno_write_close(register stream *s)
{	s_process_write_buf(s, true);
	return s_fileno_read_close(s);
}

#define ss ((stream *)st)

/* Define the switch cases for System V interrupts. */
#define case_again(errn) case errn: goto again;
#ifdef EINTR
#  define handle_EINTR() case_again(EINTR)
#else
#  define handle_EINTR() /* */
#endif
#if defined(EAGAIN) && (!defined(EINTR) || EAGAIN != EINTR)
#  define handle_EAGAIN() case_again(EAGAIN)
#else
#  define handle_EAGAIN() /* */
#endif
#if defined(EWOULDBLOCK) && (!defined(EINTR) || EWOULDBLOCK != EINTR) && (!defined(EAGAIN) || EWOULDBLOCK != EAGAIN)
#  define handle_EWOULDBLOCK() case_again(EWOULDBLOCK)
#else
#  define handle_EWOULDBLOCK() /* */
#endif
#define handle_SYSV_errno()\
  handle_EINTR() handle_EAGAIN() handle_EWOULDBLOCK()

/* Process a buffer for a file reading stream. */
/* This is the first stream in the pipeline, so pr is irrelevant. */
private int
s_fileno_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{	int nread, status;
again:	nread = read(sfileno(ss), pw->ptr + 1, (uint)(pw->limit - pw->ptr));
	if ( nread > 0 )
	  {	pw->ptr += nread;
		status = 0;
	  }
	else if ( nread == 0 )
	  status = EOFC;
	else switch ( errno )
	  {
	  handle_SYSV_errno() /* Handle System V interrupts */
	  default:
	    status = ERRC;
	  }
	process_interrupts();
	return status;
}

/* Process a buffer for a file writing stream. */
/* This is the last stream in the pipeline, so pw is irrelevant. */
private int
s_fileno_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	int nwrite, status;
	uint count;
again:	count = pr->limit - pr->ptr;
	/* Some versions of the DEC C library on AXP architectures */
	/* give an error on write if the count is zero! */
	if ( count == 0 )
	  {	process_interrupts();
		return 0;
	  }
	nwrite = write(sfileno(ss), pr->ptr + 1, count);
	if ( nwrite >= 0 )
	  {	pr->ptr += nwrite;
		status = 0;
	  }
	else switch ( errno )
	  {
	  handle_SYSV_errno() /* Handle System V interrupts */
	  default:
	    status = ERRC;
	  }
	process_interrupts();
	return status;
}

#undef ss

/* Switch a file stream to reading or writing. */
private int
s_fileno_switch(stream *s, bool writing)
{	uint modes = s->file_modes;
	int fd = sfileno(s);
	long pos;
	if ( writing )
	  {	if ( !(s->file_modes & s_mode_write) )
		  return ERRC;
		pos = stell(s);
		if_debug2('s', "[s]switch 0x%lx to write at %ld\n",
			  (ulong)s, pos);
		lseek(fd, pos, SEEK_SET);	/* pacify OS */
		if ( modes & s_mode_append )
		  {	sappend_file(s, s->file, s->cbuf, s->cbsize);	/* sets position */
		  }
		else
		  {	swrite_file(s, s->file, s->cbuf, s->cbsize);
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
		lseek(fd, 0L, SEEK_CUR);	/* pacify OS */
		sread_file(s, s->file, s->cbuf, s->cbsize);
		s->modes |= modes & s_mode_append;	/* don't lose append info */
		s->position = pos;
	  }
	s->file_modes = modes;
	return 0;
}
