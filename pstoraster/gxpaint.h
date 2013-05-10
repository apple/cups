/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxpaint.h */
/* Device coordinate painting interface for Ghostscript library */
/* Requires gsropt.h, gxfixed.h, gxpath.h */

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif

#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;
#endif

/* ------ Graphics-state-aware procedures ------ */

/*
 * The following procedures use information from the graphics state.
 * They are implemented in gxpaint.c.
 */

int gx_fill_path(P6(gx_path *ppath, gx_device_color *pdevc, gs_state *pgs,
		    int rule, fixed adjust_x, fixed adjust_y));
int gx_stroke_fill(P2(gx_path *ppath, gs_state *pgs));
int gx_stroke_add(P3(gx_path *ppath, gx_path *to_path, gs_state *pgs));

/* ------ Imager procedures ------ */

/*
 * Tweak the fill adjustment if necessary so that (nearly) empty
 * rectangles are guaranteed to produce some output.
 */
void gx_adjust_if_empty(P2(const gs_fixed_rect *, gs_fixed_point *));

/*
 * Compute the amount by which to expand a stroked bounding box to account
 * for line width, caps and joins.
 */
int gx_stroke_expansion(P2(const gs_imager_state *, gs_fixed_point *));

/*
 * The following procedures do not need a graphics state.
 * These procedures are implemented in gxfill.c and gxstroke.c.
 */

/* Define the parameters passed to the imager's filling routine. */
#ifndef gx_fill_params_DEFINED
#  define gx_fill_params_DEFINED
typedef struct gx_fill_params_s gx_fill_params;
#endif
struct gx_fill_params_s {
	int rule;		/* -1 = winding #, 1 = even/odd */
	gs_fixed_point adjust;
	float flatness;
	bool fill_zero_width;	/* if true, make zero-width/height */
				/* rectangles one pixel wide/high */
};

#define gx_fill_path_only(ppath, dev, pis, params, pdevc, pcpath)\
  (*dev_proc(dev, fill_path))(dev, pis, ppath, params, pdevc, pcpath)

/* Define the parameters passed to the imager's stroke routine. */
#ifndef gx_stroke_params_DEFINED
#  define gx_stroke_params_DEFINED
typedef struct gx_stroke_params_s gx_stroke_params;
#endif
struct gx_stroke_params_s {
	float flatness;
};

int gx_stroke_path_only(P7(gx_path *ppath, gx_path *to_path, gx_device *dev,
			   const gs_imager_state *pis,
			   const gx_stroke_params *params,
			   const gx_device_color *pdevc,
			   const gx_clip_path *pcpath));

/* Define rectangle operations used in implementing the above. */

/* Check whether a path bounding box is within a clipping box. */
#define rect_within(ibox, cbox)\
  (ibox.q.y <= cbox.q.y && ibox.q.x <= cbox.q.x &&\
   ibox.p.y >= cbox.p.y && ibox.p.x >= cbox.p.x)

/* Intersect a bounding box with a clipping box. */
#define rect_intersect(ibox, cbox)\
  { if ( cbox.p.x > ibox.p.x ) ibox.p.x = cbox.p.x;\
    if ( cbox.q.x < ibox.q.x ) ibox.q.x = cbox.q.x;\
    if ( cbox.p.y > ibox.p.y ) ibox.p.y = cbox.p.y;\
    if ( cbox.q.y < ibox.q.y ) ibox.q.y = cbox.q.y;\
  }
