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

/* gzpath.h */
/* Private representation of paths for Ghostscript library */
/* Requires gxfixed.h */
#include "gxpath.h"
#include "gsstruct.h"			/* for extern_st */

/* Paths are represented as a linked list of line or curve segments, */
/* similar to what pathforall reports. */

/* Definition of a path segment: a segment start, a line, */
/* or a Bezier curve. */
typedef enum {
	s_start,
	s_line,
	s_line_close,
	s_curve
} segment_type;
#define segment_common\
	segment *prev;\
	segment *next;\
	segment_type type;\
	gs_fixed_point pt;		/* initial point for starts, */\
					/* final point for others */
/* Forward declarations for structure types */
#ifndef segment_DEFINED
#  define segment_DEFINED
typedef struct segment_s segment;
#endif
typedef struct subpath_s subpath;

/* A generic segment.  This is never instantiated, */
/* but we define a descriptor anyway for the benefit of subclasses. */
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
	subpath *sub;
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
  gs_private_st_composite_only(st_curve, curve_segment, "curve",\
    segment_enum_ptrs, segment_reloc_ptrs)

/* A start segment.  This serves as the head of a subpath. */
/* The closer is only used temporarily when filling, */
/* to close an open subpath. */
struct subpath_s {
	segment_common
	segment *last;			/* last segment of subpath, */
					/* points back to here if empty */
	int curve_count;		/* # of curves */
	line_close_segment closer;
	char/*bool*/ is_closed;		/* true if subpath is closed */
};
#define private_st_subpath()	/* in gxpath.c */\
  gs_private_st_suffix_add1(st_subpath, subpath, "subpath",\
    subpath_enum_ptrs, subpath_reloc_ptrs, st_segment, last)

/* Test whether a subpath is a rectangle; if so, also return */
/* the start of the next subpath. */
bool gx_subpath_is_rectangle(P3(const subpath *pstart, gs_fixed_rect *pbox,
				const subpath **ppnext));

/* Curve manipulation */

/* Return the smallest value k such that 2^k segments will approximate */
/* the curve to within the desired flatness. */
int	gx_curve_log2_samples(P4(fixed, fixed, const curve_segment *, fixed));

/* Return up to 2 values of t which split the curve into monotonic parts. */
int	gx_curve_monotonic_points(P5(fixed, fixed, fixed, fixed, double [2]));

/* Split a curve at an arbitrary value of t. */
void	gx_curve_split(P6(fixed, fixed, const curve_segment *, double,
			  curve_segment *, curve_segment *));

/* Initialize a cursor for rasterizing a monotonic curve. */
/* Currently this has no dynamic elements, but it might someday. */
typedef struct curve_cursor_s {
		/* Following are set at initialization */
	int k;			/* 2^k segments */
	gs_fixed_point p0;	/* starting point */
	const curve_segment *pc;  /* other points */
	fixed a, b, c;		/* curve coefficients */
	double da, db, dc;	/* double versions of a, b, c */
	bool double_set;	/* true if da/b/c set */
	bool fixed_fits;	/* true if compute in fixed point */
} curve_cursor;
void	gx_curve_cursor_init(P5(curve_cursor *prc, fixed x0, fixed y0,
				const curve_segment *pc, int k));

/* Return the value of X at a given Y value on a monotonic curve. */
/* y must lie between prc->p0.y and prc->pt.y. */
fixed	gx_curve_x_at_y(P2(curve_cursor *prc, fixed y));

/*
 * Here is the actual structure of a path.
 * The path state reflects the most recent operation on the path as follows:
 *	Operation	position_valid	subpath_open
 *	newpath		0		0
 *	moveto		1		-1
 *	lineto/curveto	1		1
 *	closepath	1		0
 */
struct gx_path_s {
	gs_memory_t *memory;
	gs_fixed_rect bbox;		/* bounding box (in device space) */
	segment *box_last;		/* bbox incorporates segments */
					/* up to & including this one */
	subpath *first_subpath;
	subpath *current_subpath;
	int subpath_count;
	int curve_count;
	gs_fixed_point position;	/* current position */
	int subpath_open;		/* 0 = newpath or closepath, */
					/* -1 = moveto, 1 = lineto/curveto */
	char/*bool*/ position_valid;
	char/*bool*/ bbox_set;		/* true if setbbox is in effect */
	char/*bool*/ shares_segments;	/* if true, this path shares its */
					/* segment storage with the one in */
					/* the previous saved graphics state */
};
extern_st(st_path);
#define public_st_path()	/* in gxpath.c */\
  gs_public_st_ptrs3(st_path, gx_path, "path",\
    path_enum_ptrs, path_reloc_ptrs, box_last, first_subpath, current_subpath)
#define st_path_max_ptrs 3

/* Path enumeration structure */
struct gs_path_enum_s {
	const segment *pseg;
	const gs_state *pgs;
	const gx_path *path;	/* path being enumerated */
	gx_path *copied_path;	/* if the path was copied, this is the */
				/* the same as path, to be released */
				/* when done enumerating */
	bool moveto_done;	/* have we reported a final moveto yet? */
};
#define private_st_path_enum()	/* in gxpath2.c */\
  gs_private_st_ptrs4(st_gs_path_enum, gs_path_enum, "gs_path_enum",\
    path_enum_enum_ptrs, path_enum_reloc_ptrs, pseg, pgs, path, copied_path)

/* Macros equivalent to a few heavily used procedures. */
/* Be aware that these macros may evaluate arguments more than once. */
#define gx_path_current_point_inline(ppath,ppt)\
 ( !ppath->position_valid ? gs_note_error(gs_error_nocurrentpoint) :\
   ((ppt)->x = ppath->position.x, (ppt)->y = ppath->position.y, 0) )
/* ...rel_point rather than ...relative_point is because */
/* some compilers dislike identifiers of >31 characters. */
#define gx_path_add_rel_point_inline(ppath,dx,dy)\
 ( !ppath->position_valid || ppath->bbox_set ?\
   gx_path_add_relative_point(ppath, dx, dy) :\
   (ppath->position.x += dx, ppath->position.y += dy,\
    ppath->subpath_open = -1, 0) )
