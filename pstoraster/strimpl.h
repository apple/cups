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

/* strimpl.h */
/* Definitions for stream implementors */
/* Requires stdio.h */

#ifndef strimpl_INCLUDED
#  define strimpl_INCLUDED

#include "scommon.h"
#include "gstypes.h"		/* for gsstruct.h */
#include "gsstruct.h"

/*
 * The 'process' procedure does the real work of the stream.
 * It must process as much input information (from pr->ptr + 1 through
 * pr->limit) as it can, subject to space available for output
 * (pw->ptr + 1 through pw->limit), updating pr->ptr and pw->ptr.
 *
 * The procedure return value must be one of:
 *	EOFC - an end-of-data pattern was detected in the input.
 *	ERRC - a syntactic error was detected in the input.
 *	0 - more input data is needed.
 *	1 - more output space is needed.
 * If the procedure returns EOFC, it can assume it will never be called
 * again for that stream.
 *
 * If the procedure is called with last = 1, this is an indication that
 * no more input will ever be supplied (after the input in the current
 * buffer defined by *pr); the procedure should produce as much output
 * as possible, including an end-of-data marker if applicable.
 * If the procedure is called with last = 1 and returns 1, it may be
 * called again (with last = 1); if it is called with last = 1 and returns
 * any other value, it will never be called again for that stream.
 * If the procedure is called with last = 1 and returns 0, this is taken
 * as equivalent to returning EOFC.
 *
 * Note that these specifications do not distinguish input from output
 * streams.  This is deliberate: The processing procedures should work
 * regardless of which way they are oriented in a stream pipeline.
 * (The PostScript language does take a position as whether any given
 * filter may be used for input or output, but this occurs at a higher level.)
 *
 * The value returned by the process procedure of a stream whose data source
 * or sink is external (i.e., not another stream) is interpreted slightly
 * differently.  For an external data source, a return value of 0 means
 * "no more input data are available now, but more might become available
 * later."  For an external data sink, a return value of 1 means "there is
 * no more room for output data now, but there might be room later."
 *
 * It appears that the Adobe specifications, read correctly, require that when
 * the process procedure of a decoding filter has filled up the output
 * buffer, it must still peek ahead in the input to determine whether or not
 * the next thing in the input stream is EOD.  If the next thing is an EOD (or
 * end-of-data, indicated by running out of input data with last = true), the
 * process procedure must return EOFC; if the next thing is definitely not
 * an EOD, the process procedure must return 1 (output full) (without, of
 * course, consuming the non-EOD datum); if the procedure cannot determine
 * whether or not the next thing is an EOD, it must return 0 (need more input).
 * Decoding filters that don't have EOD (for example, NullDecode) can use
 * a simpler algorithm: if the output buffer is full, then if there is more
 * input, return 1, otherwise return 0 (which is taken as EOFC if last
 * is true).  All this may seem a little awkward, but it is needed in order
 * to have consistent behavior regardless of where buffer boundaries fall --
 * in particular, if a buffer boundary falls just before an EOD.  It is
 * actually quite easy to implement if the main loop of the process
 * procedure tests for running out of input rather than for filling the
 * output: with this structure, exhausting the input always returns 0,
 * and discovering that the output buffer is full when attempting to store
 * more output always returns 1.
 *
 * Even this algorithm for handling end-of-buffer is not sufficient if an
 * EOD falls just after a buffer boundary, but the generic stream code
 * handles this case: the process procedures need only do what was just
 * described.
 */

/*
 * Define a template for creating a stream.
 *
 * The meaning of min_in_size and min_out_size is the following:
 * If the amount of input information is at least min_in_size,
 * and the available output space is at least min_out_size,
 * the process procedure guarantees that it will make some progress.
 * (It may make progress even if this condition is not met, but this is
 * not guaranteed.)
 */
struct stream_template_s {

		/* Define the structure type for the stream state. */
	gs_memory_type_ptr_t stype;

		/* Define an optional initialization procedure. */
	stream_proc_init((*init));

		/* Define the processing procedure. */
		/* (The init procedure can reset other procs if it wants.) */
	stream_proc_process((*process));

		/* Define the minimum buffer sizes. */
	uint min_in_size;		/* minimum size for process input */
	uint min_out_size;		/* minimum size for process output */

		/* Define an optional releasing procedure. */
	stream_proc_release((*release));

		/* Define an optional parameter defaulting procedure. */
	stream_proc_set_defaults((*set_defaults));

		/* Define an optional reinitialization procedure. */
	stream_proc_reinit((*reinit));

};

/* Utility procedures */
int stream_move(P2(stream_cursor_read *, stream_cursor_write *)); /* in stream.c */
/* Hex decoding utility procedure */
typedef enum {
	hex_ignore_garbage = 0,
	hex_ignore_whitespace = 1,
	hex_ignore_leading_whitespace = 2
} hex_syntax;
int s_hex_process(P4(stream_cursor_read *, stream_cursor_write *, int *, hex_syntax)); /* in sstring.c */

#endif					/* strimpl_INCLUDED */
