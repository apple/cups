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

/* gzcpath.h */
/* Private representation of clipping paths for Ghostscript library */
/* Requires gzpath.h. */
#include "gxcpath.h"

/* gx_clip_path is a 'subclass' of gx_path. */
struct gx_clip_path_s {
	gx_path path;
	int rule;			/* rule for insideness of path */
		/* Anything within the inner_box is guaranteed to fall */
		/* entirely within the clipping path. */
	gs_fixed_rect inner_box;
		/* Anything outside the outer_box is guaranteed to fall */
		/* entirely outside the clipping path.  This is the same */
		/* as the path bounding box, widened to pixel boundaries. */
	gs_fixed_rect outer_box;
	gx_clip_list list;
	char segments_valid;		/* segment representation is valid */
	char shares_list;		/* if true, this path shares its */
					/* clip list storage with the one in */
					/* the previous saved graphics state */
		/* The id changes whenever the clipping region changes. */
	gs_id id;
};
extern_st(st_clip_path);
#define public_st_clip_path()	/* in gxcpath.c */\
  gs_public_st_composite(st_clip_path, gx_clip_path, "clip_path",\
    clip_path_enum_ptrs, clip_path_reloc_ptrs)
#define st_clip_path_max_ptrs (st_path_max_ptrs + st_clip_list_max_ptrs)
