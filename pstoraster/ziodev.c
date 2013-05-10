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

/* ziodev.c */
/* Standard IODevice implementation */
#include "memory_.h"
#include "stdio_.h"
#include "string_.h"
#include "ghost.h"
#include "gp.h"
#include "gpcheck.h"
#include "gsstruct.h"			/* for registering root */
#include "errors.h"
#include "oper.h"
#include "stream.h"
#include "ialloc.h"
#include "ivmspace.h"
#include "gxiodev.h"			/* must come after stream.h */
					/* and before files.h */
#include "files.h"
#include "store.h"

/* Complete the definition of the %os% device. */
/* The open_file routine is exported for pipes and for %null. */
int
iodev_os_open_file(gx_io_device *iodev, const char *fname, uint len,
  const char *file_access, stream **ps, gs_memory_t *mem)
{	return file_open_stream(fname, len, file_access,
				file_default_buffer_size, ps,
				iodev->procs.fopen);
}

/* Define the special devices. */
#define iodev_special(dname, init, open)\
  { dname, "Special",\
     { init, open, iodev_no_open_file, iodev_no_fopen, iodev_no_fclose,\
       iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,\
       iodev_no_enumerate_files, NULL, NULL,\
       iodev_no_get_params, iodev_no_put_params\
     }\
  }

#define stdin_buf_size 128
private ref ref_stdin;
bool gs_stdin_is_interactive;	/* exported for command line only */
private iodev_proc_init(stdin_init);
private iodev_proc_open_device(stdin_open);
gx_io_device gs_iodev_stdin =
  iodev_special("%stdin%", stdin_init, stdin_open);

#define stdout_buf_size 128
private ref ref_stdout;
private iodev_proc_init(stdout_init);
private iodev_proc_open_device(stdout_open);
gx_io_device gs_iodev_stdout =
  iodev_special("%stdout%", stdout_init, stdout_open);

#define stderr_buf_size 128
private ref ref_stderr;
private iodev_proc_init(stderr_init);
private iodev_proc_open_device(stderr_open);
gx_io_device gs_iodev_stderr =
  iodev_special("%stderr%", stderr_init, stderr_open);

#define lineedit_buf_size 20		/* initial size, not fixed size */
private iodev_proc_open_device(lineedit_open);
gx_io_device gs_iodev_lineedit =
  iodev_special("%lineedit%", iodev_no_init, lineedit_open);

private iodev_proc_open_device(statementedit_open);
gx_io_device gs_iodev_statementedit =
  iodev_special("%statementedit%", iodev_no_init, statementedit_open);

/* ------ Operators ------ */

/* <int> .getiodevice <string> */
private int
zgetiodevice(register os_ptr op)
{	gx_io_device *iodev;
	const byte *dname;
	check_type(*op, t_integer);
	if ( op->value.intval != (int)op->value.intval )
		return_error(e_rangecheck);
	iodev = gs_getiodevice((int)(op->value.intval));
	if ( iodev == 0 )		/* index out of range */
		return_error(e_rangecheck);
	dname = (const byte *)iodev->dname;
	make_const_string(op, a_readonly | avm_foreign,
			  strlen((const char *)dname), dname);
	return 0;
}

/* ------- %stdin, %stdout, and %stderr ------ */

/*
 * According to Adobe, it is legal to close the %std... files and then
 * re-open them later.  However, the re-opened file object is not 'eq' to
 * the original file object (in our implementation, it has a different
 * read_id or write_id).
 */

private gs_gc_root_t stdin_root, stdout_root, stderr_root;

private int
  s_stdin_read_process(P4(stream_state *, stream_cursor_read *,
    stream_cursor_write *, bool));

private int
stdin_init(gx_io_device *iodev, gs_memory_t *mem)
{	static ref *pstdin = &ref_stdin;
	make_file(&ref_stdin, a_readonly | avm_system, 1, invalid_file_entry);
	gs_stdin_is_interactive = true;
	gs_register_ref_root(mem, &stdin_root,
			     (void **)&pstdin, "ref_stdin");
	return 0;
}

/* Read from stdin into the buffer. */
/* If interactive, only read one character. */
private int
s_stdin_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{	FILE *file = ((stream *)st)->file;	/* hack for file streams */
	int wcount = (int)(pw->limit - pw->ptr);
	int count;

	if ( wcount > 0 )
	  {	if ( gs_stdin_is_interactive )
		  wcount = 1;
		count = fread(pw->ptr + 1, 1, wcount, file);
		if ( count < 0 )
		  count = 0;
		pw->ptr += count;
	  }
	else
	  count = 0;		/* return 1 if no error/EOF */
	process_interrupts();
	return (ferror(file) ? ERRC : feof(file) ? EOFC : count == wcount ? 1 : 0);
}

int
iodev_stdin_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	stream *s;
	if ( !streq1(access, 'r') )
	  return_error(e_invalidfileaccess);
	if ( !file_is_valid(s, &ref_stdin) )
	  {
		/****** stdin SHOULD NOT LINE-BUFFER ******/

		gs_memory_t *mem = imemory_system;
		byte *buf;
		s = file_alloc_stream(mem, "stdin_open(stream)");
		/* We want stdin to read only one character at a time, */
		/* but it must have a substantial buffer, in case it is used */
		/* by a stream that requires more than one input byte */
		/* to make progress. */
		buf = gs_alloc_bytes(mem, stdin_buf_size,
				     "stdin_open(buffer)");
		if ( s == 0 || buf == 0 )
		  return_error(e_VMerror);
		sread_file(s, gs_stdin, buf, stdin_buf_size);
		s->procs.process = s_stdin_read_process;
		s->save_close = s_std_null;
		s->procs.close = file_close_file;
		make_file(&ref_stdin, a_readonly | avm_system,
			  s->read_id, s);
		*ps = s;
		return 1;
	  }
	*ps = s;
	return 0;
}
private int
stdin_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stdin_open(iodev, access, ps, mem);
	return min(code, 0);
}
/* This is the public routine for getting the stdin stream. */
int
zget_stdin(stream **ps)
{	stream *s;
	if ( file_is_valid(s, &ref_stdin) )
	  {	*ps = s;
		return 0;
	  }
	return (*gs_iodev_stdin.procs.open_device)(&gs_iodev_stdin,
						   "r", ps, imemory_system);
}

private int
stdout_init(gx_io_device *iodev, gs_memory_t *mem)
{	static ref *pstdout = &ref_stdout;
	make_file(&ref_stdout, a_all | avm_system, 1, invalid_file_entry);
	gs_register_ref_root(mem, &stdout_root,
			     (void **)&pstdout, "ref_stdout");
	return 0;
}

int
iodev_stdout_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	stream *s;
	if ( !streq1(access, 'w') )
	  return_error(e_invalidfileaccess);
	if ( !file_is_valid(s, &ref_stdout) )
	  {	gs_memory_t *mem = imemory_system;
		byte *buf;
		s = file_alloc_stream(mem, "stdout_open(stream)");
		buf = gs_alloc_bytes(mem, stdout_buf_size,
				     "stdout_open(buffer)");
		if ( s == 0 || buf == 0 )
		  return_error(e_VMerror);
		swrite_file(s, gs_stdout, buf, stdout_buf_size);
		s->save_close = s_std_null;
		s->procs.close = file_close_file;
		make_file(&ref_stdout, a_write | avm_system, s->write_id, s);
		*ps = s;
		return 1;
	  }
	*ps = s;
	return 0;
}
private int
stdout_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stdout_open(iodev, access, ps, mem);
	return min(code, 0);
}
/* This is the public routine for getting the stdout stream. */
int
zget_stdout(stream **ps)
{	stream *s;
	if ( file_is_valid(s, &ref_stdout) )
	  {	*ps = s;
		return 0;
	  }
	return (*gs_iodev_stdout.procs.open_device)(&gs_iodev_stdout,
						    "w", ps, imemory_system);
}

private int
stderr_init(gx_io_device *iodev, gs_memory_t *mem)
{	static ref *pstderr = &ref_stderr;
	make_file(&ref_stderr, a_all | avm_system, 1, invalid_file_entry);
	gs_register_ref_root(mem, &stderr_root,
			     (void **)&pstderr, "ref_stderr");
	return 0;
}

int
iodev_stderr_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	stream *s;
	if ( !streq1(access, 'w') )
	  return_error(e_invalidfileaccess);
	if ( !file_is_valid(s, &ref_stderr) )
	  {	gs_memory_t *mem = imemory_system;
		byte *buf;
		s = file_alloc_stream(mem, "stderr_open(stream)");
		buf = gs_alloc_bytes(mem, stderr_buf_size,
				     "stderr_open(buffer)");
		if ( s == 0 || buf == 0 )
		  return_error(e_VMerror);
		swrite_file(s, gs_stderr, buf, stderr_buf_size);
		s->save_close = s_std_null;
		s->procs.close = file_close_file;
		make_file(&ref_stderr, a_write | avm_system, s->write_id, s);
		*ps = s;
		return 1;
	  }
	*ps = s;
	return 0;
}
private int
stderr_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stderr_open(iodev, access, ps, mem);
	return min(code, 0);
}
/* This is the public routine for getting the stderr stream. */
int
zget_stderr(stream **ps)
{	stream *s;
	if ( file_is_valid(s, &ref_stderr) )
	  {	*ps = s;
		return 0;
	  }
	return (*gs_iodev_stderr.procs.open_device)(&gs_iodev_stderr,
						    "w", ps, imemory_system);
}

/* ------ %lineedit and %statementedit ------ */

private int
lineedit_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	uint count = 0;
	bool in_eol = false;
	int code;
	stream *s;
	stream *ins;
	byte *buf;
	uint buf_size = lineedit_buf_size;

	if ( strcmp(access, "r") )
	  return_error(e_invalidfileaccess);
	s = file_alloc_stream(mem, "lineedit_open(stream)");
	if ( s == 0 )
	  return_error(e_VMerror);
	code = (gs_iodev_stdin.procs.open_device)(&gs_iodev_stdin, access,
						  &ins, mem);
	if ( code < 0 )
	  return code;
	buf = gs_alloc_string(mem, buf_size, "lineedit_open(buffer)");
	if ( buf == 0 )
	  return_error(e_VMerror);
rd:	code = zreadline_from(ins, buf, buf_size, &count, &in_eol);
	switch ( code )
	  {
	  case EOFC:
	    code = gs_note_error(e_undefinedfilename);
	    /* falls through */
	  case 0:
	    break;
	  default:
	    code = gs_note_error(e_ioerror);
	    break;
	  case 1:		/* filled buffer */
	    { uint nsize;
	      byte *nbuf;

#if arch_ints_are_short
	      if ( nsize == max_uint )
		{ code = gs_note_error(e_limitcheck);
		  break;
		}
	      else if ( nsize >= max_uint / 2 )
		nsize = max_uint;
	      else
#endif
		nsize = buf_size * 2;
	      nbuf = gs_resize_string(mem, buf, buf_size, nsize,
				      "lineedit_open(grow buffer)");
	      if ( nbuf == 0 )
		{ code = gs_note_error(e_VMerror);
		  break;
		}
	      buf = nbuf;
	      buf_size = nsize;
	      goto rd;
	    }
	  }
	if ( code != 0 )
	  { gs_free_string(mem, buf, buf_size, "lineedit_open(buffer)");
	    return code;
	  }
	buf = gs_resize_string(mem, buf, buf_size, count,
			       "lineedit_open(resize buffer)");
	if ( buf == 0 )
	  return_error(e_VMerror);
	sread_string(s, buf, count);
	s->save_close = s->procs.close;
	s->procs.close = file_close_disable;
	*ps = s;
	return 0;
}

private int
statementedit_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	/* NOT IMPLEMENTED PROPERLY YET */
	return lineedit_open(iodev, access, ps, mem);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(ziodev_op_defs) {
	{"1.getiodevice", zgetiodevice},
END_OP_DEFS(0) }
