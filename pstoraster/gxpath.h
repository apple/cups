/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxpath.h,v 1.2 2000/03/08 23:15:04 mike Exp $ */
/* Requires gxfixed.h */

#ifndef gxpath_INCLUDED
#  define gxpath_INCLUDED

#include "gscpm.h"
#include "gslparam.h"
#include "gspenum.h"
#include "gsrect.h"

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

/* Define 'notes' that describe the role of a path segment. */
/* These are only for internal use; a normal segment's notes are 0. */
typedef enum {
    sn_none = 0,
    sn_not_first = 1,		/* segment is in curve/arc and not first */
    sn_from_arc = 2		/* segment is part of an arc */
} segment_notes;

/* Debugging routines */
#ifdef DEBUG
void gx_dump_path(P2(const gx_path *, const char *));
void gx_path_print(P1(const gx_path *));

#endif

/* Path memory management */

/*
 * Path memory management is unfortunately a little tricky.  The
 * implementation details are in gzpath.h: we only present the API here.
 *
 * Path objects per se may be allocated in 3 different ways: on the
 * C stack, as separate objects in the heap, or (for the graphics state
 * only) contained in a larger heap-allocated object.
 *
 * Any number of paths may share segments.  The segments are stored in
 * their own, reference-counted object, and are freed when there are no
 * more references to that object.
 */

/*
 * Allocate a path on the heap, and initialize it.  If shared is NULL,
 * allocate a segments object; if shared is an existing path, share its
 * segments.
 */
gx_path *gx_path_alloc_shared(P3(const gx_path * shared, gs_memory_t * mem,
				 client_name_t cname));

#define gx_path_alloc(mem, cname)\
  gx_path_alloc_shared(NULL, mem, cname)
/*
 * Initialize a path contained in an already-heap-allocated object,
 * optionally allocating its segments.
 */
int gx_path_init_contained_shared(P4(gx_path * ppath, const gx_path * shared,
				   gs_memory_t * mem, client_name_t cname));

#define gx_path_alloc_contained(ppath, mem, cname)\
  gx_path_init_contained_shared(ppath, NULL, mem, cname)
/*
 * Initialize a stack-allocated path.  This doesn't allocate anything,
 * but may still share the segments.  Note that it returns an error if
 * asked to share the segments of another local path.
 */
int gx_path_init_local_shared(P3(gx_path * ppath, const gx_path * shared,
				 gs_memory_t * mem));

#define gx_path_init_local(ppath, mem)\
  (void)gx_path_init_local_shared(ppath, NULL, mem)	/* can't fail */

/*
 * Ensure that a path owns its segments, by copying the segments if
 * they currently have multiple references.
 */
int gx_path_unshare(P1(gx_path * ppath));

/*
 * Free a path by releasing its segments if they have no more references.
 * This also frees the path object iff it was allocated by gx_path_alloc.
 */
void gx_path_free(P2(gx_path * ppath, client_name_t cname));

/*
 * Assign one path to another, adjusting reference counts appropriately.
 * Note that this requires that segments of the two paths (but not the path
 * objects themselves) were allocated with the same allocator.  Note also
 * that if ppfrom is stack-allocated, ppto is not, and ppto's segments are
 * currently shared, gx_path_assign must do the equivalent of a
 * gx_path_new(ppto), which allocates a new segments object for ppto.
 */
int gx_path_assign_preserve(P2(gx_path * ppto, gx_path * ppfrom));

/*
 * Assign one path to another and free the first path at the same time.
 * (This may do less work than assign_preserve + free.)
 */
int gx_path_assign_free(P2(gx_path * ppto, gx_path * ppfrom));

/* Path constructors */
/* Note that all path constructors have an implicit initial gx_path_unshare. */

int gx_path_new(P1(gx_path *)),
    gx_path_add_point(P3(gx_path *, fixed, fixed)),
    gx_path_add_relative_point(P3(gx_path *, fixed, fixed)),
    gx_path_add_line_notes(P4(gx_path *, fixed, fixed, segment_notes)),
    gx_path_add_lines_notes(P4(gx_path *, const gs_fixed_point *, int, segment_notes)),
    gx_path_add_rectangle(P5(gx_path *, fixed, fixed, fixed, fixed)),
    gx_path_add_char_path(P3(gx_path *, gx_path *, gs_char_path_mode)),
    gx_path_add_curve_notes(P8(gx_path *, fixed, fixed, fixed, fixed, fixed, fixed, segment_notes)),
    gx_path_add_partial_arc_notes(P7(gx_path *, fixed, fixed, fixed, fixed, floatp, segment_notes)),
    gx_path_add_path(P2(gx_path *, gx_path *)),
    gx_path_close_subpath_notes(P2(gx_path *, segment_notes)),
	  /* We have to remove the 'subpath' from the following name */
	  /* to keep it unique in the first 23 characters. */
    gx_path_pop_close_notes(P2(gx_path *, segment_notes));

/* The last argument to gx_path_add_partial_arc is a fraction for computing */
/* the curve parameters.  Here is the correct value for quarter-circles. */
/* (stroke uses this to draw round caps and joins.) */
#define quarter_arc_fraction 0.552285
/*
 * Backward-compatible constructors that don't take a notes argument.
 */
#define gx_path_add_line(ppath, x, y)\
  gx_path_add_line_notes(ppath, x, y, sn_none)
#define gx_path_add_lines(ppath, pts, count)\
  gx_path_add_lines_notes(ppath, pts, count, sn_none)
#define gx_path_add_curve(ppath, x1, y1, x2, y2, x3, y3)\
  gx_path_add_curve_notes(ppath, x1, y1, x2, y2, x3, y3, sn_none)
#define gx_path_add_partial_arc(ppath, x3, y3, xt, yt, fraction)\
  gx_path_add_partial_arc_notes(ppath, x3, y3, xt, yt, fraction, sn_none)
#define gx_path_close_subpath(ppath)\
  gx_path_close_subpath_notes(ppath, sn_none)
#define gx_path_pop_close_subpath(ppath)\
  gx_path_pop_close_notes(ppath, sn_none)

/* Path accessors */

gx_path *gx_current_path(P1(const gs_state *));
int gx_path_current_point(P2(const gx_path *, gs_fixed_point *)),
    gx_path_bbox(P2(gx_path *, gs_fixed_rect *));
bool gx_path_has_curves(P1(const gx_path *)),
    gx_path_is_void(P1(const gx_path *)),	/* no segments */
    gx_path_is_null(P1(const gx_path *)),	/* nothing at all */
    gx_path_is_monotonic(P1(const gx_path *));
typedef enum {
    prt_none = 0,
    prt_open = 1,		/* only 3 sides */
    prt_fake_closed = 2,	/* 4 lines, no closepath */
    prt_closed = 3		/* 3 or 4 lines + closepath */
} gx_path_rectangular_type;

gx_path_rectangular_type
gx_path_is_rectangular(P2(const gx_path *, gs_fixed_rect *));

#define gx_path_is_rectangle(ppath, pbox)\
  (gx_path_is_rectangular(ppath, pbox) != prt_none)
/* Inline versions of the above */
#define gx_path_is_null_inline(ppath)\
  (gx_path_is_void(ppath) && !path_position_valid(ppath))

/* Path transformers */

/* gx_path_copy_reducing is internal. */
typedef enum {
    pco_none = 0,
    pco_monotonize = 1,		/* make curves monotonic */
    pco_accurate = 2		/* flatten with accurate tangents at ends */
} gx_path_copy_options;
int gx_path_copy_reducing(P4(const gx_path * ppath_old, gx_path * ppath_new,
			     fixed fixed_flatness,
			     gx_path_copy_options options));

#define gx_path_copy(old, new)\
  gx_path_copy_reducing(old, new, max_fixed, pco_none)
#define gx_path_add_flattened(old, new, flatness)\
  gx_path_copy_reducing(old, new, float2fixed(flatness), pco_none)
#define gx_path_add_flattened_accurate(old, new, flatness, accurate)\
  gx_path_copy_reducing(old, new, float2fixed(flatness),\
			(accurate ? pco_accurate : pco_none))
#define gx_path_add_monotonized(old, new)\
  gx_path_copy_reducing(old, new, max_fixed, pco_monotonize)
int gx_path_add_dash_expansion(P3(const gx_path * /*old */ , gx_path * /*new */ , const gs_imager_state *)),
      gx_path_copy_reversed(P2(const gx_path * /*old */ , gx_path * /*new */ )),
      gx_path_translate(P3(gx_path *, fixed, fixed)),
      gx_path_scale_exp2(P3(gx_path *, int, int));
void gx_point_scale_exp2(P3(gs_fixed_point *, int, int)), gx_rect_scale_exp2(P3(gs_fixed_rect *, int, int));

/* Path enumerator */

/* This interface does not make a copy of the path. */
/* Do not use gs_path_enum_cleanup with this interface! */
int gx_path_enum_init(P2(gs_path_enum *, const gx_path *));
int gx_path_enum_next(P2(gs_path_enum *, gs_fixed_point[3]));	/* 0 when done */

segment_notes
gx_path_enum_notes(P1(const gs_path_enum *));
bool gx_path_enum_backup(P1(gs_path_enum *));

/* ------ Clipping paths ------ */

/* Opaque type for a clipping path */
#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;

#endif

/* Graphics state clipping */
int gx_clip_to_rectangle(P2(gs_state *, gs_fixed_rect *));
int gx_clip_to_path(P1(gs_state *));
int gx_default_clip_box(P2(const gs_state *, gs_fixed_rect *));
int gx_effective_clip_path(P2(gs_state *, gx_clip_path **));

/* Opaque type for a clip list. */
#ifndef gx_clip_list_DEFINED
#  define gx_clip_list_DEFINED
typedef struct gx_clip_list_s gx_clip_list;

#endif

/* Opaque type for a clipping path enumerator. */
typedef struct gs_cpath_enum_s gs_cpath_enum;

/*
 * Provide similar memory management for clip paths to what we have for
 * paths (see above for details).
 */
gx_clip_path *gx_cpath_alloc_shared(P3(const gx_clip_path * shared,
				       gs_memory_t * mem,
				       client_name_t cname));

#define gx_cpath_alloc(mem, cname)\
  gx_cpath_alloc_shared(NULL, mem, cname)
int gx_cpath_init_contained_shared(P4(gx_clip_path * pcpath,
				      const gx_clip_path * shared,
				      gs_memory_t * mem,
				      client_name_t cname));

#define gx_cpath_alloc_contained(pcpath, mem, cname)\
  gx_cpath_init_contained_shared(pcpath, NULL, mem, cname)
int gx_cpath_init_local_shared(P3(gx_clip_path * pcpath,
				  const gx_clip_path * shared,
				  gs_memory_t * mem));

#define gx_cpath_init_local(pcpath, mem)\
  (void)gx_cpath_init_local_shared(pcpath, NULL, mem)	/* can't fail */
int gx_cpath_unshare(P1(gx_clip_path * pcpath));
void gx_cpath_free(P2(gx_clip_path * pcpath, client_name_t cname));
int gx_cpath_assign_preserve(P2(gx_clip_path * pcpto, gx_clip_path * pcpfrom));
int gx_cpath_assign_free(P2(gx_clip_path * pcpto, gx_clip_path * pcpfrom));

/* Clip path constructors and accessors */

int
    gx_cpath_reset(P1(gx_clip_path *)),		/* from_rectangle ((0,0),(0,0)) */
    gx_cpath_from_rectangle(P2(gx_clip_path *, gs_fixed_rect *)),
    gx_cpath_clip(P4(gs_state *, gx_clip_path *, gx_path *, int)),
    gx_cpath_scale_exp2(P3(gx_clip_path *, int, int)),
    gx_cpath_to_path(P2(gx_clip_path *, gx_path *));
bool
    gx_cpath_inner_box(P2(const gx_clip_path *, gs_fixed_rect *)),
    gx_cpath_outer_box(P2(const gx_clip_path *, gs_fixed_rect *)),
    gx_cpath_includes_rectangle(P5(const gx_clip_path *, fixed, fixed,
				   fixed, fixed));
int gx_cpath_set_outside(P2(gx_clip_path *, bool));
bool gx_cpath_is_outside(P1(const gx_clip_path *));

/* Enumerate a clipping path.  This interface does not copy the path. */
/* However, it does write into the path's "visited" flags. */
int gx_cpath_enum_init(P2(gs_cpath_enum *, gx_clip_path *));
int gx_cpath_enum_next(P2(gs_cpath_enum *, gs_fixed_point[3]));		/* 0 when done */

segment_notes
gx_cpath_enum_notes(P1(const gs_cpath_enum *));

#endif /* gxpath_INCLUDED */
