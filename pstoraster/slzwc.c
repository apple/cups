/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: slzwc.c,v 1.2 2000/03/08 23:15:26 mike Exp $ */
/* Code common to LZW encoding and decoding streams */
#include "std.h"
#include "strimpl.h"
#include "slzwx.h"

/* Define the structure for the GC. */
public_st_LZW_state();

/* Set defaults */
void
s_LZW_set_defaults(stream_state * st)
{
    stream_LZW_state *const ss = (stream_LZW_state *) st;

    s_LZW_set_defaults_inline(ss);
}

/* Release a LZW filter. */
void
s_LZW_release(stream_state * st)
{
    stream_LZW_state *const ss = (stream_LZW_state *) st;

    gs_free_object(st->memory, ss->table.decode, "LZW(close)");
}
