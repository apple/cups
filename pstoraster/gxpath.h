/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxpath.h */
/* Lower-level path routines for Ghostscript library */
/* Requires gxfixed.h */
#include "gscpm.h"
#include "gspenum.h"

/* The routines and types in this interface use */
/* device, rather than user, coordinates, and fixed-point, */
/* rather than floating, representation. */

/* Opaque type for a path */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif

/* Define the two insideness rules */
#define gx_rule_winding_number (-1)
#define gx_rule_even_odd 1

/* Debugging routines */
#ifdef DEBUG
void gx_dump_path(P2(const gx_path *, const char *));
void gx_path_print(P1(const gx_path *));
#endif

/* Path constructors */

gx_path *gx_path_alloc(P2(gs_memory_t *, client_name_t));
void	gx_path_init(P2(gx_path *, gs_memory_t *)),
	gx_path_reset(P1(gx_path *)),
	gx_path_release(P1(gx_path *)),
	gx_path_share(P1(gx_path *));
int	gx_path_add_point(P3(gx_path *, fixed, fixed)),
	gx_path_add_relative_point(P3(gx_path *, fixed, fixed)),
	gx_path_add_line(P3(gx_path *, fixed, fixed)),
	gx_path_add_lines(P3(gx_path *, const gs_fixed_point *, int)),
	gx_path_add_rectangle(P5(gx_path *, fixed, fixed, fixed, fixed)),
	gx_path_add_char_path(P3(gx_path *, gx_path *, gs_char_path_mode)),
	gx_path_add_curve(P7(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed)),
/*
 * gx_path_flattened_curve was removed in release 3.61.
	gx_path_add_flattened_curve(P8(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed, floatp)),
 *
 */
	gx_path_add_partial_arc(P6(gx_path *, fixed, fixed, fixed, fixed, floatp)),
	gx_path_add_path(P2(gx_path *, gx_path *)),
	gx_path_close_subpath(P1(gx_path *)),
	gx_path_pop_close_subpath(P1(gx_path *));
/* The last argument to gx_path_add_partial_arc is a fraction for computing */
/* the curve parameters.  Here is the correct value for quarter-circles. */
/* (stroke uses this to draw round caps and joins.) */
#define quarter_arc_fraction 0.552285

/* Path accessors */

int	gx_path_current_point(P2(const gx_path *, gs_fixed_point *)),
	gx_path_bbox(P2(gx_path *, gs_fixed_rect *));
bool	gx_path_has_curves(P1(const gx_path *)),
	gx_path_is_void(P1(const gx_path *)),	/* no segments */
	gx_path_is_null(P1(const gx_path *)),	/* nothing at all */
	gx_path_is_rectangle(P2(const gx_path *, gs_fixed_rect *)),
	gx_path_is_monotonic(P1(const gx_path *));
/* Inline versions of the above */
#define gx_path_has_curves_inline(ppath)\
  ((ppath)->curve_count != 0)
#define gx_path_is_void_inline(ppath)\
  ((ppath)->first_subpath == 0)
#define gx_path_is_null_inline(ppath)\
  (gx_path_is_void_inline(ppath) && !(ppath)->position_valid)

/* Path transformers */

/* gx_path_copy_reducing is internal. */
int	gx_path_copy_reducing(P5(const gx_path *ppath_old, gx_path *ppath_new,
				 fixed fixed_flatness, bool monotonize,
				 bool init));
#define gx_path_copy(old, new, init)\
  gx_path_copy_reducing(old, new, max_fixed, false, init)
#define gx_path_flatten(old, new, flatness)\
  gx_path_copy_reducing(old, new, float2fixed(flatness), false, true)
#define gx_path_monotonize(old, new)\
  gx_path_copy_reducing(old, new, max_fixed, true, true)
int	gx_path_expand_dashes(P3(const gx_path * /*old*/, gx_path * /*new*/, const gs_imager_state *)),
	gx_path_copy_reversed(P3(const gx_path * /*old*/, gx_path * /*new*/, bool /*init*/)),
	gx_path_translate(P3(gx_path *, fixed, fixed)),
	gx_path_scale_exp2(P3(gx_path *, int, int));
void	gx_point_scale_exp2(P3(gs_fixed_point *, int, int)),
	gx_rect_scale_exp2(P3(gs_fixed_rect *, int, int));

/* Path enumerator */

/* This interface does not make a copy of the path. */
/* Do not use gs_path_enum_cleanup with this interface! */
int	gx_path_enum_init(P2(gs_path_enum *, const gx_path *));
int	gx_path_enum_next(P2(gs_path_enum *, gs_fixed_point [3])); /* 0 when done */
bool	gx_path_enum_backup(P1(gs_path_enum *));

/* ------ Clipping paths ------ */

/* Opaque type for a clipping path */
#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;
#endif

/* Opaque type for a clip list. */
#ifndef gx_clip_list_DEFINED
#  define gx_clip_list_DEFINED
typedef struct gx_clip_list_s gx_clip_list;
#endif

int	gx_clip_to_rectangle(P2(gs_state *, gs_fixed_rect *)),
	gx_clip_to_path(P1(gs_state *)),
	gx_cpath_init(P2(gx_clip_path *, gs_memory_t *)),
	gx_cpath_from_rectangle(P3(gx_clip_path *, gs_fixed_rect *, gs_memory_t *)),
	gx_cpath_intersect(P4(gs_state *, gx_clip_path *, gx_path *, int)),
	gx_cpath_scale_exp2(P3(gx_clip_path *, int, int));
void	gx_cpath_release(P1(gx_clip_path *)),
	gx_cpath_share(P1(gx_clip_path *));
int	gx_cpath_path(P2(gx_clip_path *, gx_path *));
bool	gx_cpath_inner_box(P2(const gx_clip_path *, gs_fixed_rect *)),
	gx_cpath_outer_box(P2(const gx_clip_path *, gs_fixed_rect *)),
	gx_cpath_includes_rectangle(P5(const gx_clip_path *, fixed, fixed, fixed, fixed));
int	gx_cpath_set_outside(P2(gx_clip_path *, bool));
bool	gx_cpath_is_outside(P1(const gx_clip_path *));
