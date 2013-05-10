/* Copyright (C) 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* scommon.h */
/* Definitions common to stream clients and implementors */

#ifndef scommon_DEFINED
#  define scommon_DEFINED

#include "gsmemory.h"
#include "gstypes.h"			/* for gs_string */
#include "gsstruct.h"			/* for extern_st */

/*
 * There are three major structures involved in the stream package.
 *
 * A stream is an "object" that owns a buffer, which it uses to implement
 * byte-oriented sequential access in a standard way, and a set of
 * procedures that handle things like buffer refilling.  See stream.h
 * for more information about streams.
 */
#ifndef stream_DEFINED
#  define stream_DEFINED
typedef struct stream_s stream;
#endif
/*
 * A stream_state records the state specific to a given variety of stream.
 * The buffer processing function of a stream maintains this state.
 */
typedef struct stream_state_s stream_state;
/*
 * A stream_template provides the information needed to create a stream.
 * The client must fill in any needed setup parameters in the appropriate
 * variety of stream_state, and then call the initialization function
 * provided by the template.  See strimpl.h for more information about
 * stream_templates.
 */
typedef struct stream_template_s stream_template;

/*
 * The stream package works with bytes, not chars.
 * This is to ensure unsigned representation on all systems.
 * A stream currently can only be read or written, not both.
 * Note also that the read procedure returns an int, not a char or a byte;
 * we use negative values to indicate exceptional conditions.
 * (We cast these values to int explicitly, because some compilers
 * don't do this if the other arm of a conditional is a byte.)
 */
/* End of data */
#define EOFC ((int)(-1))
/* Error */
#define ERRC ((int)(-2))
/* Interrupt */
#define INTC ((int)(-3))
/****** INTC IS NOT USED YET ******/
/* Callout */
#define CALLC ((int)(-4))
#define max_stream_exception 4
/* The following hack is needed for initializing scan_char_array in iscan.c. */
#define stream_exception_repeat(x) x, x, x, x

/*
 * Define cursors for reading from or writing into a buffer.
 * We lay them out this way so that we can alias
 * the write pointer and the read limit.
 */
typedef struct stream_cursor_read_s {
	const byte *ptr;
	const byte *limit;
	byte *_skip;
} stream_cursor_read;
typedef struct stream_cursor_write_s {
	const byte *_skip;
	byte *ptr;
	byte *limit;
} stream_cursor_write;
typedef union stream_cursor_s {
	stream_cursor_read r;
	stream_cursor_write w;
} stream_cursor;

/*
 * Define the prototype for the procedures known to both the generic
 * stream code and the stream implementations.
 */

/* Initialize the stream state (after the client parameters are set). */
#define stream_proc_init(proc)\
  int proc(P1(stream_state *))

/* Process a buffer.  See strimpl.h for details. */
#define stream_proc_process(proc)\
  int proc(P4(stream_state *, stream_cursor_read *,\
    stream_cursor_write *, bool))

/* Release the stream state when closing. */
#define stream_proc_release(proc)\
  void proc(P1(stream_state *))

/* Initialize the client parameters to default values. */
#define stream_proc_set_defaults(proc)\
  void proc(P1(stream_state *))

/* Reinitialize any internal stream state.  Note that this does not */
/* affect buffered data.  We declare this as returning an int so that */
/* it can be the same as the init procedure; however, reinit cannot fail. */
#define stream_proc_reinit(proc)\
  int proc(P1(stream_state *))

/* Report an error.  Note that this procedure is stored in the state, */
/* not in the main stream structure. */
#define stream_proc_report_error(proc)\
  int proc(P2(stream_state *, const char *))
stream_proc_report_error(s_no_report_error);

/*
 * Define a generic stream state.  If a processing procedure has no
 * state of its own, it can use stream_state; otherwise, it must
 * create a "subclass".  There is a hack in stream.h to allow the stream
 * itself to serve as the "state" of a couple of heavily used stream types.
 *
 * In order to simplify the structure descriptors for concrete streams,
 * we require that the generic stream state not contain any pointers
 * to garbage-collectable storage.
 */
#define stream_state_common\
	const stream_template *template;\
	gs_memory_t *memory;\
	stream_proc_report_error((*report_error))
struct stream_state_s {
	stream_state_common;
};
extern_st(st_stream_state);
#define public_st_stream_state() /* in stream.c */\
  gs_public_st_simple(st_stream_state, stream_state, "stream_state")

#endif					/* scommon_INCLUDED */
