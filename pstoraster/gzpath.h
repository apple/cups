/* Copyright (C) 1989, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gzpath.h,v 1.2 2000/03/08 23:15:07 mike Exp $ */
/* Requires gxfixed.h */

#ifndef gzpath_INCLUDED
#  define gzpath_INCLUDED

#include "gxpath.h"
#include "gsmatrix.h"
#include "gsrefct.h"
#include "gsstruct.h"		/* for extern_st */

/*
 * Paths are represented as a linked list of line or curve segments,
 * similar to what pathforall reports.
 */

/*
 * Define path segment types: segment start, line, or Bezier curve.
 * We have a special type for the line added by closepath.
 */
typedef enum {
    s_start,
    s_line,
    s_line_close,
    s_curve
} segment_type;

/* Define the common structure for all segments. */
#define segment_common\
	segment *prev;\
	segment *next;\
	ushort /*segment_type*/ type;\
	ushort /*segment_notes*/ notes;\
	gs_fixed_point pt;		/* initial point for starts, */\
				/* final point for others */

/* Forward declarations for structure types */
#ifndef segment_DEFINED
#  define segment_DEFINED
typedef struct segment_s segment;

#endif
typedef struct subpath_s subpath;

/*
 * Define a generic segment.  This is never instantiated,
 * but we define a descriptor anyway for the benefit of subclasses.
 */
struct segment_s {
    segment_common
};

#define private_st_segment()	/* in gxpath.c */\
  gs_private_st_ptrs2(st_segment, struct segment_s, "segment",\
    segment_enum_ptrs, segment_reloc_ptrs, prev, next)

/* Line segments have no special data. */
typedef struct {
    segment_common
} line_segment;

#define private_st_line()	/* in gxpath.c */\
  gs_private_st_suffix_add0(st_line, line_segment, "line",\
    line_enum_ptrs, line_reloc_ptrs, st_segment)

/* Line_close segments are for the lines appended by closepath. */
/* They point back to the subpath being closed. */
typedef struct {
    segment_common
    subpath * sub;
} line_close_segment;

#define private_st_line_close()	/* in gxpath.c */\
  gs_private_st_suffix_add1(st_line_close, line_close_segment, "close",\
    close_enum_ptrs, close_reloc_ptrs, st_segment, sub)

/*
 * We use two different representations for curve segments: one defined by
 * two endpoints (p0, p3) and two control points (p1, p2), and one defined
 * by two sets of parametric cubic coefficients (ax ... dy).  Here is how
 * they are related (v = x or y).  We spell out some multiplies by 3 for
 * the benefit of compilers too simple to optimize this.
 */
#define curve_points_to_coefficients(v0, v1, v2, v3, a, b, c, t01, t12)\
  (/*d = (v0),*/\
   t01 = (v1) - (v0), c = (t01 << 1) + t01,\
   t12 = (v2) - (v1), b = (t12 << 1) + t12 - c,\
   a = (v3) - b - c - (v0))
/*
 * or conversely
 */
#define curve_coefficients_to_points(a, b, c, d, v1, v2, v3)\
  (/*v0 = (d),*/\
   v1 = (d) + ((c) / 3),\
   v2 = v1 + (((b) + (c)) / 3),\
   v3 = (a) + (b) + (c) + (d))

/* Curve segments store the control points. */
typedef struct {
    segment_common
    gs_fixed_point p1, p2;
} curve_segment;

#define private_st_curve()	/* in gxpath.c */\
  gs_private_st_suffix_add0_local(st_curve, curve_segment, "curve",\
    segment_enum_ptrs, segment_reloc_ptrs, st_segment)

/*
 * Define a start segment.  This serves as the head of a subpath.
 * The closer is only used temporarily when filling,
 * to close an open subpath.
 */
struct subpath_s {
    segment_common
    segment * last;		/* last segment of subpath, */
    /* points back to here if empty */
    int curve_count;		/* # of curves */
    line_close_segment closer;
    char /*bool */ is_closed;	/* true if subpath is closed */
};

#define private_st_subpath()	/* in gxpath.c */\
  gs_private_st_suffix_add1(st_subpath, subpath, "subpath",\
    subpath_enum_ptrs, subpath_reloc_ptrs, st_segment, last)

/* Test whether a subpath is a rectangle; if so, also return */
/* the start of the next subpath. */
gx_path_rectangular_type
gx_subpath_is_rectangular(P3(const subpath * pstart, gs_fixed_rect * pbox,
			     const subpath ** ppnext));

#define gx_subpath_is_rectangle(pstart, pbox, ppnext)\
  (gx_subpath_is_rectangular(pstart, pbox, ppnext) != prt_none)

/* Curve manipulation */

/* Return the smallest value k such that 2^k segments will approximate */
/* the curve to within the desired flatness. */
int gx_curve_log2_samples(P4(fixed, fixed, const curve_segment *, fixed));

/*
 * If necessary, find the values of t (never more than 2) which split the
 * curve into monotonic parts.  Return the number of split points.
 */
int gx_curve_monotonic_points(P5(fixed, fixed, fixed, fixed, double[2]));

/* Split a curve at an arbitrary value of t. */
void gx_curve_split(P6(fixed, fixed, const curve_segment *, double,
		       curve_segment *, curve_segment *));

/* Flatten a partial curve by sampling (internal procedure). */
int gx_flatten_sample(P4(gx_path *, int, curve_segment *, segment_notes));

/* Initialize a cursor for rasterizing a monotonic curve. */
typedef struct curve_cursor_s {
    /* Following are set at initialization */
    int k;			/* 2^k segments */
    gs_fixed_point p0;		/* starting point */
    const curve_segment *pc;	/* other points */
    fixed a, b, c;		/* curve coefficients */
    double da, db, dc;		/* scaled double versions of a, b, c */
    bool double_set;		/* true if da/b/c set */
    int fixed_limit;		/* can do in fixed point if t <= limit */
    /* Following are updated dynamically. */
    struct ccc_ {		/* one-element cache */
	fixed ky0, ky3;		/* key (range) */
	fixed xl, xd;		/* value */
    } cache;
} curve_cursor;
void gx_curve_cursor_init(P5(curve_cursor * prc, fixed x0, fixed y0,
			     const curve_segment * pc, int k));

/* Return the value of X at a given Y value on a monotonic curve. */
/* y must lie between prc->p0.y and prc->pt.y. */
fixed gx_curve_x_at_y(P2(curve_cursor * prc, fixed y));

/*
 * The path state flags reflect the most recent operation on the path
 * as follows:
 *      Operation       position_valid  subpath_open    is_drawing
 *      newpath         no              no              no
 *      moveto          yes             yes             no
 *      lineto/curveto  yes             yes             yes
 *      closepath       yes             no              no
 * If position_valid is true, outside_range reflects whether the most
 * recent operation went outside of the representable coordinate range.
 * If this is the case, the corresponding member of position (x and/or y)
 * is min_fixed or max_fixed, and outside_position is the true position.
 */
/*
 */
typedef enum {
    /* Individual flags.  These may be or'ed together, per above. */
    psf_position_valid = 1,
    psf_subpath_open = 2,
    psf_is_drawing = 4,
    psf_outside_range = 8,
    /* Values stored by path building operations. */
    psf_last_newpath = 0,
    psf_last_moveto = psf_position_valid | psf_subpath_open,
    psf_last_draw = psf_position_valid | psf_subpath_open | psf_is_drawing,
    psf_last_closepath = psf_position_valid
} gx_path_state_flags;

/*
 * Individual tests
 */
#define path_position_valid(ppath)\
  (((ppath)->state_flags & psf_position_valid) != 0)
#define path_subpath_open(ppath)\
  (((ppath)->state_flags & psf_subpath_open) != 0)
#define path_is_drawing(ppath)\
  (((ppath)->state_flags & psf_is_drawing) != 0)
#define path_outside_range(ppath)\
  (((ppath)->state_flags & psf_outside_range) != 0)
/*
 * Composite tests
 */
#define path_last_is_moveto(ppath)\
  (((ppath)->state_flags & ~psf_outside_range) == psf_last_moveto)
#define path_position_in_range(ppath)\
  (((ppath)->state_flags & (psf_position_valid + psf_outside_range)) ==\
   psf_position_valid)
#define path_start_outside_range(ppath)\
  ((ppath)->state_flags != 0 &&\
   ((ppath)->start_flags & psf_outside_range) != 0)
/*
 * Updating operations
 */
#define path_update_newpath(ppath)\
  ((ppath)->state_flags = psf_last_newpath)
#define path_update_moveto(ppath)\
  ((ppath)->state_flags = (ppath)->start_flags = psf_last_moveto)
#define path_update_draw(ppath)\
  ((ppath)->state_flags = psf_last_draw)
#define path_update_closepath(ppath)\
  ((ppath)->state_flags = psf_last_closepath)
#define path_set_outside_position(ppath, px, py)\
  ((ppath)->outside_position.x = (px),\
   (ppath)->outside_position.y = (py),\
   (ppath)->state_flags |= psf_outside_range)

/*
 * In order to be able to reclaim path segments at the right time, we need
 * to reference-count them.  To minimize disruption, we would like to do
 * this by creating a structure (gx_path_segments) consisting of only a
 * reference count that counts the number of paths that share the same
 * segments.  (Logically, we should put the segments themselves --
 * first/last_subpath, subpath/curve_count -- in this object, but that would
 * cause too much disruption to existing code.)  However, we need to put at
 * least first_subpath and current_subpath in this structure so that we can
 * free the segments when the reference count becomes zero.
 */
typedef struct gx_path_segments_s {
    rc_header rc;
    struct psc_ {
	subpath *subpath_first;
	subpath *subpath_current;
    } contents;
} gx_path_segments;

#define private_st_path_segments()	/* in gxpath.c */\
  gs_private_st_ptrs2(st_path_segments, gx_path_segments, "path segments",\
    path_segments_enum_ptrs, path_segments_reloc_ptrs,\
    contents.subpath_first, contents.subpath_current)

/* Record how a path was allocated, so freeing will do the right thing. */
typedef enum {
    path_allocated_on_stack,	/* on stack */
    path_allocated_contained,	/* inside another object */
    path_allocated_on_heap	/* on the heap */
} gx_path_allocation_t;

/* Here is the actual structure of a path. */
struct gx_path_s {
    /*
     * In order to be able to have temporary paths allocated entirely
     * on the stack, we include a segments structure within the path,
     * used only for this purpose.  In order to avoid having the
     * path's segments pointer point into the middle of an object,
     * the segments structure must come first.
     *
     * Note that since local_segments is used only for temporary paths
     * on the stack, and not for path structures in allocated memory,
     * we don't declare any pointers in it for the GC.  (As it happens,
     * there aren't any such pointers at the moment, but this could
     * change.)
     */
    gx_path_segments local_segments;
    gs_memory_t *memory;
    gx_path_allocation_t allocation;	/* how this path was allocated */
    gx_path_segments *segments;
    gs_fixed_rect bbox;		/* bounding box (in device space) */
    segment *box_last;		/* bbox incorporates segments */
    /* up to & including this one */
#define first_subpath segments->contents.subpath_first	/* (hack) */
#define current_subpath segments->contents.subpath_current	/* (ditto) */
    int subpath_count;
    int curve_count;
    gs_fixed_point position;	/* current position */
    gs_point outside_position;	/* position if outside_range is set */
    gs_point outside_start;	/* outside_position of last moveto */
             byte /*gx_path_state_flags */ start_flags;		/* flags of moveto */
             byte /*gx_path_state_flags */ state_flags;		/* (see above) */
             byte /*bool */ bbox_set;	/* true if setbbox is in effect */
};

/* st_path should be private, but it's needed for the clip_path subclass. */
extern_st(st_path);
#define public_st_path()	/* in gxpath.c */\
  gs_public_st_ptrs2(st_path, gx_path, "path",\
    path_enum_ptrs, path_reloc_ptrs, segments, box_last)
#define st_path_max_ptrs 2

/* Path enumeration structure */
struct gs_path_enum_s {
    gs_memory_t *memory;
    gs_matrix mat;		/* CTM for inverse-transforming points */
    const segment *pseg;
    const gx_path *path;	/* path being enumerated */
    gx_path *copied_path;	/* if the path was copied, this is the */
    /* the same as path, to be released */
    /* when done enumerating */
    bool moveto_done;		/* have we reported a final moveto yet? */
    segment_notes notes;	/* notes from most recent segment */
};

/* We export st_path_enum only so that st_cpath_enum can subclass it. */
extern_st(st_path_enum);
#define public_st_path_enum()	/* in gxpath2.c */\
  gs_public_st_ptrs3(st_path_enum, gs_path_enum, "gs_path_enum",\
    path_enum_enum_ptrs, path_enum_reloc_ptrs, pseg, path, copied_path)

/* Inline path accessors. */
#define gx_path_has_curves_inline(ppath)\
  ((ppath)->curve_count != 0)
#define gx_path_has_curves(ppath)\
  gx_path_has_curves_inline(ppath)
#define gx_path_is_void_inline(ppath)\
  ((ppath)->first_subpath == 0)
#define gx_path_is_void(ppath)\
  gx_path_is_void_inline(ppath)
#define gx_path_subpath_count(ppath)\
  ((ppath)->subpath_count)
#define gx_path_is_shared(ppath)\
  ((ppath)->segments->rc.ref_count > 1)

/* Macros equivalent to a few heavily used procedures. */
/* Be aware that these macros may evaluate arguments more than once. */
#define gx_path_current_point_inline(ppath,ppt)\
 ( !path_position_valid(ppath) ? gs_note_error(gs_error_nocurrentpoint) :\
   ((ppt)->x = ppath->position.x, (ppt)->y = ppath->position.y, 0) )
/* ...rel_point rather than ...relative_point is because */
/* some compilers dislike identifiers of >31 characters. */
#define gx_path_add_rel_point_inline(ppath,dx,dy)\
 ( !path_position_in_range(ppath) || ppath->bbox_set ?\
   gx_path_add_relative_point(ppath, dx, dy) :\
   (ppath->position.x += dx, ppath->position.y += dy,\
    path_update_moveto(ppath), 0) )

#endif /* gzpath_INCLUDED */
