/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gspath.h */
/* Client interface to path manipulation facilities */
/* Requires gsstate.h */
#include "gspenum.h"

/* Path constructors */
int	gs_newpath(P1(gs_state *)),
	gs_moveto(P3(gs_state *, floatp, floatp)),
	gs_rmoveto(P3(gs_state *, floatp, floatp)),
	gs_lineto(P3(gs_state *, floatp, floatp)),
	gs_rlineto(P3(gs_state *, floatp, floatp)),
	gs_arc(P6(gs_state *, floatp, floatp, floatp, floatp, floatp)),
	gs_arcn(P6(gs_state *, floatp, floatp, floatp, floatp, floatp)),
/*
 * Because of an obscure bug in the IBM RS/6000 compiler, one (but not both)
 * bool argument(s) for gs_arc_add must come before the floatp arguments.
 */
	gs_arc_add(P8(gs_state *, bool, floatp, floatp, floatp, floatp, floatp, bool)),
	gs_arcto(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, float [4])),
	gs_curveto(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp)),
	gs_rcurveto(P7(gs_state *, floatp, floatp, floatp, floatp, floatp, floatp)),
	gs_closepath(P1(gs_state *));

/* Add the current path to the path in the previous graphics state. */
int	gs_upmergepath(P1(gs_state *));

/* Path accessors and transformers */
int	gs_currentpoint(P2(const gs_state *, gs_point *)),
	gs_upathbbox(P3(gs_state *, gs_rect *, bool)),
	gs_dashpath(P1(gs_state *)),
	gs_flattenpath(P1(gs_state *)),
	gs_reversepath(P1(gs_state *)),
	gs_strokepath(P1(gs_state *));
/* The extra argument for gs_upathbbox controls whether to include */
/* a trailing moveto in the bounding box. */
#define gs_pathbbox(pgs, prect)\
  gs_upathbbox(pgs, prect, false)

/* Path enumeration */

/* This interface makes a copy of the path. */
gs_path_enum *
	gs_path_enum_alloc(P2(gs_memory_t *, client_name_t));
int	gs_path_enum_init(P2(gs_path_enum *, const gs_state *));
int	gs_path_enum_next(P2(gs_path_enum *, gs_point [3])); /* 0 when done */
void	gs_path_enum_cleanup(P1(gs_path_enum *));

/* Clipping */
int	gs_clippath(P1(gs_state *)),
	gs_initclip(P1(gs_state *)),
	gs_clip(P1(gs_state *)),
	gs_eoclip(P1(gs_state *));
int	gs_setclipoutside(P2(gs_state *, bool));
bool	gs_currentclipoutside(P1(const gs_state *));
