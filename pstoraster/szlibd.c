/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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
#include <config.h>
#ifdef HAVE_LIBZ

/* szlibd.c */
/* zlib decoding (decompression) filter stream */
#include "std.h"
#include "gsmemory.h"
#include "strimpl.h"
#include "szlibx.h"

#define ss ((stream_zlib_state *)st)
#define szs (&ss->zstate)

/* Initialize the filter. */
private int
s_zlibD_init(stream_state *st)
{	szs->zalloc = s_zlib_alloc;
	szs->zfree = s_zlib_free;
	szs->opaque = &gs_memory_default;
	if ( inflateInit(szs) != Z_OK )
	  return ERRC;		/****** WRONG ******/
	return 0;
}

/* Process a buffer */
private int
s_zlibD_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool ignore_last)
{	const byte *p = pr->ptr;
	int status;

	/* Detect no input or full output so that we don't get */
	/* a Z_BUF_ERROR return. */
	if ( pw->ptr == pw->limit )
	  return 1;
	if ( pr->ptr == pr->limit )
	  return 0;
	szs->next_in = p + 1;
	szs->avail_in = pr->limit - p;
	szs->next_out = pw->ptr + 1;
	szs->avail_out = pw->limit - pw->ptr;
	status = inflate(szs, Z_PARTIAL_FLUSH);
	pr->ptr = szs->next_in - 1;
	pw->ptr = szs->next_out - 1;
	switch ( status )
	  {
	  case Z_OK:
	    return (pw->ptr == pw->limit ? 1 : pr->ptr > p ? 0 : 1);
	  case Z_STREAM_END:
	    return EOFC;
	  default:
	    return ERRC;
	  }
}

/* Release the stream */
private void
s_zlibD_release(stream_state *st)
{	inflateEnd(szs);
}

/* Stream template */
const stream_template s_zlibD_template =
{	&st_zlib_state, s_zlibD_init, s_zlibD_process, 1, 1, s_zlibD_release
};

#undef ss
#endif
