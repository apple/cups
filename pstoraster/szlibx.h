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

/* szlibx.h */
/* Definitions for zlib filters */
/* Requires strimpl.h */
/* Must be compiled with -I$(ZSRCDIR) */
#include "zlib.h"

/* Provide zlib-compatible allocation and freeing functions. */
void *s_zlib_alloc(P3(void *mem, uint items, uint size));
void s_zlib_free(P2(void *mem, void *address));

typedef struct stream_zlib_state_s {
	stream_state_common;
	z_stream zstate;
} stream_zlib_state;
/* The state descriptor is public only to allow us to split up */
/* the encoding and decoding filters. */
/* Note that we allocate all of zlib's private data directly from */
/* the C heap, to avoid garbage collection issues. */
extern_st(st_zlib_state);
#define public_st_zlib_state()	/* in szlibc.c */\
  gs_public_st_simple(st_zlib_state, stream_zlib_state,\
    "zlibEncode/Decode state")
extern const stream_template s_zlibD_template;
extern const stream_template s_zlibE_template;
