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

/*$Id: gxstroke.c,v 1.2 2000/03/08 23:15:06 mike Exp $ */
/* Path stroking procedures for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsdcolor.h"
#include "gxfixed.h"
#include "gxfarith.h"
#include "gxmatrix.h"
#include "gscoord.h"
#include "gsdevice.h"
#include "gxdevice.h"
#include "gxhttile.h"
#include "gxistate.h"
#include "gzline.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gxpaint.h"

/*
 * We don't really know whether it's a good idea to take fill adjustment
 * into account for stroking.  Disregarding it means that strokes
 * come out thinner than fills; observing it produces heavy-looking
 * strokes at low resolutions.  But in any case, we must disregard it
 * when stroking zero-width lines.
 */
#define USE_FILL_ADJUSTMENT

#ifdef USE_FILL_ADJUSTMENT
#  define stroke_adjustment(thin, pis, xy)\
     (thin ? fixed_0 : (pis)->fill_adjust.xy)
#else
#  define stroke_adjustment(thin, pis, xy) fixed_0
#endif

/*
 * For some reason, we commented out the optimization for portrait,
 * landscape, and uniform (non-scaled) transformations.  We have no record
 * of why we did this, and we don't know what bugs re-enabling it may
 * introduce.
 */
#define OPTIMIZE_ORIENTATION

/*
 * Compute the amount by which to expand a stroked bounding box to account
 * for line width, caps and joins.  Because of square caps and miter and
 * triangular joins, the maximum expansion on each side (in user space) is
 *      K * line_width/2
 * where K is determined as follows:
 *      If the path is only a single line segment, K = 1;
 *      if triangular joins, K = 2;
 *      if miter joins, K = miter_limit;
 *      otherwise, K = 1.
 * If the amount is too large to fit in a gs_fixed_point, return
 * gs_error_limitcheck.
 *
 * If the miter limit is very large, the foregoing computation will produce
 * a result that is much too large; we would like to tighten this up at
 * some point in the future.
 */
int
gx_stroke_path_expansion(const gs_imager_state * pis, const gx_path * ppath,
			 gs_fixed_point * ppt)
{
    const subpath *psub = ppath->first_subpath;
    const segment *pseg;
    double expand =
    (!gx_path_has_curves(ppath) && gx_path_subpath_count(ppath) <= 1 &&
     (psub == 0 || (pseg = psub->next) == 0 ||
      (pseg = pseg->next) == 0 || pseg->type == s_line_close) ? 1.0 :
     pis->line_params.join == gs_join_miter ?
     pis->line_params.miter_limit :
     pis->line_params.join == gs_join_triangle ? 2.0 : 1.0) *
    pis->line_params.half_width;

    /* Short-cut gs_bbox_transform. */
    float cx1 = pis->ctm.xx + pis->ctm.yx;
    float cy1 = pis->ctm.xy + pis->ctm.yy;
    float cx2 = pis->ctm.xx - pis->ctm.yx;
    float cy2 = pis->ctm.xy - pis->ctm.yy;

    if (cx1 < 0)
	cx1 = -cx1;
    if (cy1 < 0)
	cy1 = -cy1;
    if (cx2 < 0)
	cx2 = -cx2;
    if (cy2 < 0)
	cy2 = -cy2;
    {
	float exx = expand * max(cx1, cx2);
	float exy = expand * max(cy1, cy2);
	int code = set_float2fixed_vars(ppt->x, exx);

	if (code < 0)
	    return code;
	return set_float2fixed_vars(ppt->y, exy);
    }
}

/*
 * Structure for a partial line (passed to the drawing routine).
 * Two of these are required to do joins right.
 * Each endpoint includes the two ends of the cap as well,
 * and the deltas for square, round, and triangular cap computation.
 *
 * The two base values for computing the caps of a partial line are the
 * width and the end cap delta.  The width value is one-half the line
 * width (suitably transformed) at 90 degrees counter-clockwise
 * (in device space, but with "90 degrees" interpreted in *user*
 * coordinates) at the end (as opposed to the origin) of the line.
 * The cdelta value is one-half the transformed line width in the same
 * direction as the line.  From these, we compute two other values at each
 * end of the line: co and ce, which are the ends of the cap.
 * Note that the cdelta values at o are the negatives of the values at e,
 * as are the offsets from p to co and ce.
 *
 * Initially, only o.p, e.p, e.cdelta, width, and thin are set.
 * compute_caps fills in the rest.
 */
typedef gs_fixed_point *p_ptr;
typedef struct endpoint_s {
    gs_fixed_point p;		/* the end of the line */
    gs_fixed_point co, ce;	/* ends of the cap, p +/- width */
    gs_fixed_point cdelta;	/* +/- cap length */
} endpoint;
typedef endpoint *ep_ptr;
typedef const endpoint *const_ep_ptr;
typedef struct partial_line_s {
    endpoint o;			/* starting coordinate */
    endpoint e;			/* ending coordinate */
    gs_fixed_point width;	/* one-half line width, see above */
    bool thin;			/* true if minimum-width line */
} partial_line;
typedef partial_line *pl_ptr;

/* Assign a point.  Some compilers would do this with very slow code */
/* if we simply implemented it as an assignment. */
#define ASSIGN_POINT(pp, p)\
  ((pp)->x = (p).x, (pp)->y = (p).y)

/* Other forward declarations */
private bool width_is_thin(P1(pl_ptr));
private void adjust_stroke(P3(pl_ptr, const gs_imager_state *, bool));
private int line_join_points(P5(const gx_line_params * pgs_lp,
				pl_ptr plp, pl_ptr nplp,
				gs_fixed_point * join_points,
				const gs_matrix * pmat));
private void compute_caps(P1(pl_ptr));
private int add_points(P4(gx_path *, const gs_fixed_point *,
			  int, bool));
private int add_round_cap(P2(gx_path *, const_ep_ptr));
private int cap_points(P3(gs_line_cap, const_ep_ptr,
			  gs_fixed_point * /*[3] */ ));

/* Define the default implementation of the device stroke_path procedure. */
int
gx_default_stroke_path(gx_device * dev, const gs_imager_state * pis,
		       gx_path * ppath, const gx_stroke_params * params,
		       const gx_drawing_color * pdcolor,
		       const gx_clip_path * pcpath)
{
    return gx_stroke_path_only(ppath, (gx_path *) 0, dev, pis, params,
			       pdcolor, pcpath);
}

/* Fill a partial stroked path.  Free variables: */
/* to_path, stroke_path_body, fill_params, always_thin, pis, dev, pdevc, */
/* code, ppath, exit(label). */
#define fill_stroke_path(thin)\
  if(to_path==&stroke_path_body && !gx_path_is_void(&stroke_path_body)) {\
    fill_params.adjust.x = stroke_adjustment(thin, pis, x);\
    fill_params.adjust.y = stroke_adjustment(thin, pis, y);\
    code = gx_fill_path_only(to_path, dev, pis, &fill_params, pdevc, NULL);\
    gx_path_free(&stroke_path_body, "fill_stroke_path");\
    if ( code < 0 ) goto exit;\
    gx_path_init_local(&stroke_path_body, ppath->memory);\
  }

/*
 * Define the internal procedures that stroke a partial_line
 * (the first pl_ptr argument).  If both partial_lines are non-null,
 * the procedure creates an appropriate join; otherwise, the procedure
 * creates an end cap.  If the first int is 0, the procedure also starts
 * with an appropriate cap.
 */
#define stroke_line_proc(proc)\
  int proc(P10(gx_path *, int, pl_ptr, pl_ptr, const gx_device_color *,\
		    gx_device *, const gs_imager_state *,\
		    const gx_stroke_params *, const gs_fixed_rect *, int))
typedef stroke_line_proc((*stroke_line_proc_t));

private stroke_line_proc(stroke_add);
private stroke_line_proc(stroke_fill);

/* Define the orientations we handle specially. */
typedef enum {
    orient_other = 0,
    orient_portrait,		/* [xx 0 0 yy tx ty] */
    orient_landscape		/* [0 xy yx 0 tx ty] */
} orientation;

/*
 * Stroke a path.  If to_path != 0, append the stroke outline to it;
 * if to_path == 0, draw the strokes on dev.
 *
 * Note that gx_stroke_path_only with to_path != NULL may clip the path to
 * the clipping path, as for to_path == NULL.  This is almost never
 * what is wanted.
 */
int
gx_stroke_path_only(gx_path * ppath, gx_path * to_path, gx_device * pdev,
	       const gs_imager_state * pis, const gx_stroke_params * params,
		 const gx_device_color * pdevc, const gx_clip_path * pcpath)
{
    stroke_line_proc_t line_proc =
    (to_path == 0 ? stroke_fill : stroke_add);
    gs_fixed_rect ibox, cbox;
    gx_device_clip cdev;
    gx_device *dev = pdev;
    int code = 0;
    gx_fill_params fill_params;
    const gx_line_params *pgs_lp = gs_currentlineparams_inline(pis);
    int dash_count = pgs_lp->dash.pattern_size;
    gx_path fpath, dpath;
    gx_path stroke_path_body;
    const gx_path *spath;
    float xx = pis->ctm.xx, xy = pis->ctm.xy;
    float yx = pis->ctm.yx, yy = pis->ctm.yy;

    /*
       * We are dealing with a reflected coordinate system
       * if transform(1,0) is counter-clockwise from transform(0,1).
       * See the note in stroke_add for the algorithm.
     */
    int uniform;
    bool reflected;
    orientation orient =
    (
#ifdef OPTIMIZE_ORIENTATION
	is_fzero2(xy, yx) ?
	(uniform = (xx == yy ? 1 : xx == -yy ? -1 : 0),
	 reflected = (uniform ? uniform < 0 : (xx < 0) != (yy < 0)),
	 orient_portrait) :
	is_fzero2(xx, yy) ?
	(uniform = (xy == yx ? -1 : xy == -yx ? 1 : 0),
	 reflected = (uniform ? uniform < 0 : (xy < 0) == (yx < 0)),
	 orient_landscape) :
    /* We should optimize uniform rotated coordinate systems */
    /* here as well, but we don't. */
#endif
	(uniform = 0,
	 reflected = xy * yx > xx * yy,
	 orient_other));
    segment_notes not_first =
    (!is_fzero(pis->line_params.dot_length) ? sn_not_first : sn_none);
    float line_width = pgs_lp->half_width;	/* (*half* the line width) */
    bool always_thin;
    double line_width_and_scale, device_line_width_scale;
    double device_dot_length = pgs_lp->dot_length * fixed_1;
    const subpath *psub;

#ifdef DEBUG
    if (gs_debug_c('o')) {
	int count = pgs_lp->dash.pattern_size;
	int i;

	dlprintf3("[o]half_width=%f, cap=%d, join=%d,\n",
		  pgs_lp->half_width, (int)pgs_lp->cap, (int)pgs_lp->join);
	dlprintf2("   miter_limit=%f, miter_check=%f,\n",
		  pgs_lp->miter_limit, pgs_lp->miter_check);
	dlprintf1("   dash pattern=%d", count);
	for (i = 0; i < count; i++)
	    dprintf1(",%f", pgs_lp->dash.pattern[i]);
	dputs(",\n");
	dlprintf4("\toffset=%f, init(ink_on=%d, index=%d, dist_left=%f)\n",
		  pgs_lp->dash.offset, pgs_lp->dash.init_ink_on,
		  pgs_lp->dash.init_index, pgs_lp->dash.init_dist_left);
    }
#endif

    gx_path_bbox(ppath, &ibox);
    /* Expand the path bounding box by the scaled line width. */
    {
	gs_fixed_point expansion;

	if (gx_stroke_path_expansion(pis, ppath, &expansion) < 0) {
	    /* The expansion is so large it caused a limitcheck. */
	    ibox.p.x = ibox.p.y = min_fixed;
	    ibox.q.x = ibox.q.y = max_fixed;
	} else {
	    expansion.x += pis->fill_adjust.x;
	    expansion.y += pis->fill_adjust.y;
	    /*
	     * It's theoretically possible for the following computations to
	     * overflow, so we need to check for this.
	     */
	    ibox.p.x = (ibox.p.x < min_fixed + expansion.x ? min_fixed :
			ibox.p.x - expansion.x);
	    ibox.p.y = (ibox.p.y < min_fixed + expansion.y ? min_fixed :
			ibox.p.y - expansion.y);
	    ibox.q.x = (ibox.q.x > max_fixed - expansion.x ? max_fixed :
			ibox.q.x + expansion.x);
	    ibox.q.y = (ibox.q.y > max_fixed - expansion.y ? max_fixed :
			ibox.q.y + expansion.y);
	}
    }
    /* Check the expanded bounding box against the clipping regions. */
    if (pcpath)
	gx_cpath_inner_box(pcpath, &cbox);
    else if (pdevc)
	(*dev_proc(dev, get_clipping_box)) (dev, &cbox);
    else {
	/* This is strokepath, not stroke.  Don't clip. */
	cbox = ibox;
    }
    if (!rect_within(ibox, cbox)) {
	/* Intersect the path box and the clip bounding box. */
	/* If the intersection is empty, this call is a no-op. */
	gs_fixed_rect bbox;

	if (pcpath) {
	    gx_cpath_outer_box(pcpath, &bbox);
	    if_debug4('f', "   outer_box=(%g,%g),(%g,%g)\n",
		      fixed2float(bbox.p.x), fixed2float(bbox.p.y),
		      fixed2float(bbox.q.x), fixed2float(bbox.q.y));
	    rect_intersect(ibox, bbox);
	} else
	    rect_intersect(ibox, cbox);
	if (ibox.p.x >= ibox.q.x || ibox.p.y >= ibox.q.y) {
	    /* Intersection of boxes is empty! */
	    return 0;
	}
	/*
	 * The path is neither entirely inside the inner clip box
	 * nor entirely outside the outer clip box.
	 * If we had to flatten the path, this is where we would
	 * recompute its bbox and make the tests again,
	 * but we don't bother right now.
	 *
	 * If there is a clipping path, set up a clipping device.
	 */
	if (pcpath) {
	    gx_make_clip_device(&cdev, &cdev, gx_cpath_list(pcpath));
	    cdev.target = dev;
	    cdev.max_fill_band = dev->max_fill_band;
	    dev = (gx_device *) & cdev;
	    (*dev_proc(dev, open_device)) (dev);
	}
    }
    fill_params.rule = gx_rule_winding_number;
    fill_params.flatness = pis->flatness;
#ifdef USE_FILL_ADJUSTMENT
    fill_params.fill_zero_width =
	(pis->fill_adjust.x | pis->fill_adjust.y) != 0;
#else
    fill_params.fill_zero_width = false;
#endif
    if (line_width < 0)
	line_width = -line_width;
    line_width_and_scale = line_width * (double)int2fixed(1);
    if (is_fzero(line_width))
	always_thin = true;
    else {
	float xa, ya;

	switch (orient) {
	    case orient_portrait:
		xa = xx, ya = yy;
		goto sat;
	    case orient_landscape:
		xa = xy, ya = yx;
	      sat:
		if (xa < 0)
		    xa = -xa;
		if (ya < 0)
		    ya = -ya;
		always_thin = (max(xa, ya) * line_width < 0.5);
		if (!always_thin && uniform) {	/* Precompute a value we'll need later. */
		    device_line_width_scale = line_width_and_scale * xa;
		}
		break;
	    default:
		{
		    /* The check is more complicated, but it's worth it. */
		    double xsq = xx * xx + xy * xy;
		    double ysq = yx * yx + yy * yy;
		    double cross = xx * yx + xy * yy;

		    if (cross < 0)
			cross = 0;
		    always_thin =
			((max(xsq, ysq) + cross) * line_width * line_width
			 < 0.25);
		}
	}
    }
    if_debug7('o', "[o]ctm=(%g,%g,%g,%g,%g,%g) thin=%d\n",
	      xx, xy, yx, yy, pis->ctm.tx, pis->ctm.ty, always_thin);
    if (device_dot_length != 0) {
	/*
	 * Compute the dot length in device space.  We can't do this
	 * quite right for non-uniform coordinate systems; too bad.
	 */
	gs_matrix mat;
	const gs_matrix *pmat;

	if (pgs_lp->dot_length_absolute) {
	    gs_deviceinitialmatrix(pdev, &mat);
	    pmat = &mat;
	} else
	    pmat = (const gs_matrix *)&pis->ctm;
	device_dot_length *= fabs(pmat->xy) + fabs(pmat->yy);
    }
    /* Start by flattening the path.  We should do this on-the-fly.... */
    if (!gx_path_has_curves(ppath)) {	/* don't need to flatten */
	if (!ppath->first_subpath)
	    return 0;
	spath = ppath;
    } else {
	gx_path_init_local(&fpath, ppath->memory);
	if ((code = gx_path_add_flattened_accurate(ppath, &fpath,
				params->flatness, pis->accurate_curves)) < 0
	    )
	    return code;
	spath = &fpath;
    }
    if (dash_count) {
	gx_path_init_local(&dpath, ppath->memory);
	code = gx_path_add_dash_expansion(spath, &dpath, pis);
	if (code < 0)
	    goto exf;
	spath = &dpath;
    }
    if (to_path == 0) {
	/* We might try to defer this if it's expensive.... */
	to_path = &stroke_path_body;
	gx_path_init_local(&stroke_path_body, ppath->memory);
    }
    for (psub = spath->first_subpath; psub != 0;) {
	int index = 0;
	const segment *pseg = (const segment *)psub;
	fixed x = pseg->pt.x;
	fixed y = pseg->pt.y;
	bool is_closed = ((const subpath *)pseg)->is_closed;
	partial_line pl, pl_prev, pl_first;

	while ((pseg = pseg->next) != 0 &&
	       pseg->type != s_start
	    ) {
	    /* Compute the width parameters in device space. */
	    /* We work with unscaled values, for speed. */
	    fixed sx = pseg->pt.x, udx = sx - x;
	    fixed sy = pseg->pt.y, udy = sy - y;

	    pl.o.p.x = x, pl.o.p.y = y;
	  d:pl.e.p.x = sx, pl.e.p.y = sy;
	    if (!(udx | udy)) {	/* degenerate */
		/*
		 * If this is the first segment of the subpath,
		 * check the entire subpath for degeneracy.
		 * Otherwise, ignore the degenerate segment.
		 */
		if (index != 0)
		    continue;
		/* Check for a degenerate subpath. */
		while ((pseg = pseg->next) != 0 &&
		       pseg->type != s_start
		    ) {
		    sx = pseg->pt.x, udx = sx - x;
		    sy = pseg->pt.y, udy = sy - y;
		    if (udx | udy)
			goto d;
		}
		/*
		 * The entire subpath is degenerate, but it includes
		 * more than one point.  If we are using round caps or
		 * the dot length is non-zero, draw the caps, otherwise
		 * do nothing.
		 */
		if (!(pgs_lp->cap == gs_cap_round ||
		      pgs_lp->dot_length != 0)
		    )
		    break;
		/*
		 * Orient the dot according to the previous segment if
		 * any, or else the next segment if any, or else a
		 * vertical line.
		 */
		{
		    const segment *end = psub->prev;

		    if (end != 0 && (end->pt.x != x || end->pt.y != y))
			sx = end->pt.x, sy = end->pt.y;
		    else if (pseg != 0 &&
			     (pseg->pt.x != x || pseg->pt.y != y)
			)
			sx = pseg->pt.x, sy = pseg->pt.y;
		}
		/*
		 * Compute the properly oriented dot length, and then
		 * draw the dot like a very short line.
		 */
		udx = sx - x, udy = sy - y;
		if ((udx | udy) == 0)
		    udy = fixed_1;
		{
		    double scale = device_dot_length /
		    hypot((double)udx, (double)udy);

		    /*
		     * If we're using butt caps, make sure the "line" is
		     * long enough to show up.
		     */
		    if (pgs_lp->cap == gs_cap_butt) {
			fixed dmax = max(any_abs(udx), any_abs(udy));

			if (dmax * scale < fixed_1)
			    scale = (float)fixed_1 / dmax;
		    }
		    udx = (fixed) (udx * scale);
		    udy = (fixed) (udy * scale);
		    if ((udx | udy) == 0)
			udy = fixed_epsilon;
		    sx = x + udx;
		    sy = y + udy;
		}
		/*
		 * Back up 1 segment to keep the bookkeeping straight.
		 */
		pseg = (pseg != 0 ? pseg->prev : psub->last);
		goto d;
	    }
	    if (always_thin) {
		pl.e.cdelta.x = pl.e.cdelta.y = 0;
		pl.width.x = pl.width.y = 0;
		pl.thin = true;
	    } else {
		if (uniform != 0) {
		    /* We can save a lot of work in this case. */
		    /* We know orient != orient_other. */
		    float dpx = udx, dpy = udy;
		    float wl = device_line_width_scale /
		    hypot(dpx, dpy);

		    pl.e.cdelta.x = (fixed) (dpx * wl);
		    pl.e.cdelta.y = (fixed) (dpy * wl);
		    /* The width is the cap delta rotated by */
		    /* 90 degrees. */
		    pl.width.x = -pl.e.cdelta.y,
			pl.width.y = pl.e.cdelta.x;
		    pl.thin = false;	/* if not always_thin, */
		    /* then never thin. */

		} else {
		    gs_point dpt;	/* unscaled */
		    float wl;

		    gs_imager_idtransform(pis,
					  (float)udx, (float)udy, &dpt);
		    wl = line_width_and_scale /
			hypot(dpt.x, dpt.y);
		    /* Construct the width vector in */
		    /* user space, still unscaled. */
		    dpt.x *= wl;
		    dpt.y *= wl;
		    /*
		     * We now compute both perpendicular
		     * and (optionally) parallel half-widths,
		     * as deltas in device space.  We use
		     * a fixed-point, unscaled version of
		     * gs_dtransform.  The second computation
		     * folds in a 90-degree rotation (in user
		     * space, before transforming) in the
		     * direction that corresponds to counter-
		     * clockwise in device space.
		     */
		    pl.e.cdelta.x = (fixed) (dpt.x * xx);
		    pl.e.cdelta.y = (fixed) (dpt.y * yy);
		    if (orient != orient_portrait)
			pl.e.cdelta.x += (fixed) (dpt.y * yx),
			    pl.e.cdelta.y += (fixed) (dpt.x * xy);
		    if (!reflected)
			dpt.x = -dpt.x, dpt.y = -dpt.y;
		    pl.width.x = (fixed) (dpt.y * xx),
			pl.width.y = -(fixed) (dpt.x * yy);
		    if (orient != orient_portrait)
			pl.width.x -= (fixed) (dpt.x * yx),
			    pl.width.y += (fixed) (dpt.y * xy);
		    pl.thin = width_is_thin(&pl);
		}
		if (!pl.thin) {
		    adjust_stroke(&pl, pis, false);
		    compute_caps(&pl);
		}
	    }
	    if (index++) {
		int first;
		pl_ptr lptr;

		if (pgs_lp->join == gs_join_none &&
		    !(pseg->notes & not_first)
		    ) {
		    /* Fake the end of a subpath so we get */
		    /* caps instead of joins. */
		    first = 0;
		    lptr = 0;
		    index = 1;
		} else {
		    first = (is_closed ? 1 : index - 2);
		    lptr = &pl;
		}
		code = (*line_proc) (to_path, first, &pl_prev, lptr,
				     pdevc, dev, pis, params, &cbox,
				     uniform);
		if (code < 0)
		    goto exit;
		fill_stroke_path(always_thin);
	    } else
		pl_first = pl;
	    pl_prev = pl;
	    x = sx, y = sy;
	}
	if (index) {
	    /* If closed, join back to start, else cap. */
	    /* For some reason, the Borland compiler requires the cast */
	    /* in the following statement. */
	    pl_ptr lptr =
	    (!is_closed ||
	     (pgs_lp->join == gs_join_none &&
	      !((pseg == 0 ? (const segment *)spath->first_subpath :
		 pseg)->notes & not_first)) ?
	     (pl_ptr) 0 : (pl_ptr) & pl_first);

	    code = (*line_proc) (to_path, index - 1, &pl_prev, lptr,
				 pdevc, dev, pis, params, &cbox, uniform);
	    if (code < 0)
		goto exit;
	    fill_stroke_path(always_thin);
	}
	psub = (const subpath *)pseg;
    }
  exit:
    if (to_path == &stroke_path_body)
	gx_path_free(&stroke_path_body, "gx_stroke_path_only error");	/* (only needed if error) */
    if (dash_count)
	gx_path_free(&dpath, "gx_stroke_path exit(dash path)");
  exf:
    if (ppath->curve_count)
	gx_path_free(&fpath, "gx_stroke_path exit(flattened path)");
    return code;
}

/* ------ Internal routines ------ */

/*
 * Test whether a line is thin, i.e., whether the half-width, measured
 * perpendicular to the line in device space, is less than 0.5 pixel.
 * Unfortunately, the width values we computed are perpendicular to the
 * line in *user* space, so we may have to do some extra work.
 */
private bool
width_is_thin(pl_ptr plp)
{
    fixed dx, dy, wx = plp->width.x, wy = plp->width.y;

    /* If the line is horizontal or vertical, things are easy. */
    if ((dy = plp->e.p.y - plp->o.p.y) == 0)
	return any_abs(wy) < fixed_half;
    if ((dx = plp->e.p.x - plp->o.p.x) == 0)
	return any_abs(wx) < fixed_half;

    /*
     * If both horizontal and vertical widths are less than
     * 0.5, the line is thin.
     */
    if (any_abs(wx) < fixed_half && any_abs(wy) < fixed_half)
	return true;

    /*
     * We have to do this the hard way, by actually computing the
     * perpendicular distance.  The distance from the point (U,V)
     * from a line from (0,0) to (C,D) is
     *      abs(C*V - D*U) / sqrt(C^2 + D^2)
     * In this case, (U,V) is plp->width, and (C,D) is (dx,dy).
     */
    {
	double C = dx, D = dy;
	double num = C * wy - D * wx;
	double denom = hypot(C, D);

	/* both num and denom are scaled by fixed_scale^2, */
	/* so we don't need to do any de-scaling for the test. */
	return fabs(num) < denom * 0.5;
    }
}

/* Adjust the endpoints and width of a stroke segment */
/* to achieve more uniform rendering. */
/* Only o.p, e.p, e.cdelta, and width have been set. */
private void
adjust_stroke(pl_ptr plp, const gs_imager_state * pis, bool thin)
{
    fixed *pw;
    fixed *pov;
    fixed *pev;
    fixed w, w2;
    fixed adj2;

    if (!pis->stroke_adjust && plp->width.x != 0 && plp->width.y != 0)
	return;			/* don't adjust */
    if (any_abs(plp->width.x) < any_abs(plp->width.y)) {
	/* More horizontal stroke */
	pw = &plp->width.y, pov = &plp->o.p.y, pev = &plp->e.p.y;
	adj2 = stroke_adjustment(thin, pis, y) << 1;
    } else {
	/* More vertical stroke */
	pw = &plp->width.x, pov = &plp->o.p.x, pev = &plp->e.p.x;
	adj2 = stroke_adjustment(thin, pis, x) << 1;
    }
    /* Round the larger component of the width up or down, */
    /* whichever way produces a result closer to the correct width. */
    /* Note that just rounding the larger component */
    /* may not produce the correct result. */
    w = *pw;
    w2 = fixed_rounded(w << 1);	/* full line width */
    if (w2 == 0 && *pw != 0) {
	/* Make sure thin lines don't disappear. */
	w2 = (*pw < 0 ? -fixed_1 + adj2 : fixed_1 - adj2);
	*pw = arith_rshift_1(w2);
    }
    /* Only adjust the endpoints if the line is horizontal or vertical. */
    if (*pov == *pev) {
	/* We're going to round the endpoint coordinates, so */
	/* take the fill adjustment into account now. */
	if (w >= 0)
	    w2 += adj2;
	else
	    w2 = adj2 - w2;
	if (w2 & fixed_1)	/* odd width, move to half-pixel */
	    *pov = *pev = fixed_floor(*pov) + fixed_half;
	else			/* even width, move to pixel */
	    *pov = *pev = fixed_rounded(*pov);

    }
}

/* Compute the intersection of two lines.  This is a messy algorithm */
/* that somehow ought to be useful in more places than just here.... */
/* If the lines are (nearly) parallel, return -1 without setting *pi; */
/* otherwise, return 0 if the intersection is beyond *pp1 and *pp2 in */
/* the direction determined by *pd1 and *pd2, and 1 otherwise. */
private int
line_intersect(
		  p_ptr pp1,	/* point on 1st line */
		  p_ptr pd1,	/* slope of 1st line (dx,dy) */
		  p_ptr pp2,	/* point on 2nd line */
		  p_ptr pd2,	/* slope of 2nd line */
		  p_ptr pi)
{				/* return intersection here */
    /* We don't have to do any scaling, the factors all work out right. */
    float u1 = pd1->x, v1 = pd1->y;
    float u2 = pd2->x, v2 = pd2->y;
    double denom = u1 * v2 - u2 * v1;
    float xdiff = pp2->x - pp1->x;
    float ydiff = pp2->y - pp1->y;
    double f1;
    double max_result = any_abs(denom) * (double)max_fixed;

#ifdef DEBUG
    if (gs_debug_c('O')) {
	dlprintf4("[o]Intersect %f,%f(%f/%f)",
		  fixed2float(pp1->x), fixed2float(pp1->y),
		  fixed2float(pd1->x), fixed2float(pd1->y));
	dlprintf4(" & %f,%f(%f/%f),\n",
		  fixed2float(pp2->x), fixed2float(pp2->y),
		  fixed2float(pd2->x), fixed2float(pd2->y));
	dlprintf3("\txdiff=%f ydiff=%f denom=%f ->\n",
		  xdiff, ydiff, denom);
    }
#endif
    /* Check for degenerate result. */
    if (any_abs(xdiff) >= max_result || any_abs(ydiff) >= max_result) {
	/* The lines are nearly parallel, */
	/* or one of them has zero length.  Punt. */
	if_debug0('O', "\tdegenerate!\n");
	return -1;
    }
    f1 = (v2 * xdiff - u2 * ydiff) / denom;
    pi->x = pp1->x + (fixed) (f1 * u1);
    pi->y = pp1->y + (fixed) (f1 * v1);
    if_debug2('O', "\t%f,%f\n",
	      fixed2float(pi->x), fixed2float(pi->y));
    return (f1 >= 0 && (v1 * xdiff >= u1 * ydiff ? denom >= 0 : denom < 0) ? 0 : 1);
}

/* Set up the width and delta parameters for a thin line. */
/* We only approximate the width and height. */
private void
set_thin_widths(register pl_ptr plp)
{
    fixed dx = plp->e.p.x - plp->o.p.x, dy = plp->e.p.y - plp->o.p.y;

#define TRSIGN(v, c) ((v) >= 0 ? (c) : -(c))
    if (any_abs(dx) > any_abs(dy)) {
	plp->width.x = plp->e.cdelta.y = 0;
	plp->width.y = plp->e.cdelta.x = TRSIGN(dx, fixed_half);
    } else {
	plp->width.y = plp->e.cdelta.x = 0;
	plp->width.x = -(plp->e.cdelta.y = TRSIGN(dy, fixed_half));
    }
#undef TRSIGN
}

/* Draw a line on the device. */
/* Treat no join the same as a bevel join. */
private int
stroke_fill(gx_path * ppath, int first, register pl_ptr plp, pl_ptr nplp,
const gx_device_color * pdevc, gx_device * dev, const gs_imager_state * pis,
	    const gx_stroke_params * params, const gs_fixed_rect * pbbox,
	    int uniform)
{
    const fixed lix = plp->o.p.x;
    const fixed liy = plp->o.p.y;
    const fixed litox = plp->e.p.x;
    const fixed litoy = plp->e.p.y;

    if (plp->thin) {
	/* Minimum-width line, don't have to be careful. */
	/* We do have to check for the entire line being */
	/* within the clipping rectangle, allowing for some */
	/* slop at the ends. */
	fixed x0 = lix, y0 = liy;
	fixed x1 = litox, y1 = litoy;
	fixed t;

#define slop int2fixed(2)
	if (x0 > x1)
	    t = x0, x0 = x1 - slop, x1 = t + slop;
	else
	    x0 -= slop, x1 += slop;
	if (y0 > y1)
	    t = y0, y0 = y1 - slop, y1 = t + slop;
	else
	    y0 -= slop, y1 += slop;
#undef slop

	if (pbbox->p.x <= x0 && x1 <= pbbox->q.x &&
	    pbbox->p.y <= y0 && y1 <= pbbox->q.y
	    )
	    return (*dev_proc(dev, draw_thin_line)) (dev,
						     lix, liy, litox, litoy,
						     pdevc, pis->log_op);
	/* We didn't set up the endpoint parameters before, */
	/* because the line was thin.  stroke_add will do this. */
    }
    /* Check for being able to fill directly. */
    {
	const gx_line_params *pgs_lp = gs_currentlineparams_inline(pis);
	gs_line_cap cap = pgs_lp->cap;
	gs_line_join join = pgs_lp->join;

	if (!plp->thin && (nplp == 0 || !nplp->thin)
	    && ((first != 0 && nplp != 0) || cap == gs_cap_butt
		|| cap == gs_cap_square)
	    && (join == gs_join_bevel || join == gs_join_miter ||
		join == gs_join_none)
	    && (pis->fill_adjust.x | pis->fill_adjust.y) == 0
	    && lop_is_idempotent(pis->log_op)
	    ) {
	    gs_fixed_point points[6];
	    int npoints, code;

	    npoints = cap_points((first == 0 ? cap : gs_cap_butt),
				 &plp->o, points);
	    if (nplp == 0)
		code = cap_points(cap, &plp->e, points + npoints);
	    else
		code = line_join_points(pgs_lp, plp, nplp, points + npoints,
					(uniform ? (gs_matrix *) 0 :
					 &ctm_only(pis)));
	    if (code < 0)
		return code;
	    if (nplp != 0) {
		if (join == gs_join_miter) {
		    /* Make sure we have a bevel and not a miter. */
		    if (!(points[2].x == plp->e.co.x &&
			  points[2].y == plp->e.co.y &&
			  points[5].x == plp->e.ce.x &&
			  points[5].y == plp->e.ce.y)
			)
			goto fill;
		} {
		    const gs_fixed_point *bevel = points + 2;

		    /* Identify which 3 points define the bevel triangle. */
		    if (points[3].x == nplp->o.p.x &&
			points[3].y == nplp->o.p.y
			)
			++bevel;
		    /* Fill the bevel. */
		    code = (*dev_proc(dev, fill_triangle)) (dev,
							 bevel->x, bevel->y,
			       bevel[1].x - bevel->x, bevel[1].y - bevel->y,
			       bevel[2].x - bevel->x, bevel[2].y - bevel->y,
							pdevc, pis->log_op);
		    if (code < 0)
			return code;
		}
	    }
	    /* Fill the body of the stroke. */
	    return (*dev_proc(dev, fill_parallelogram)) (dev,
						   points[1].x, points[1].y,
						  points[0].x - points[1].x,
						  points[0].y - points[1].y,
						  points[2].x - points[1].x,
						  points[2].y - points[1].y,
							 pdevc, pis->log_op);
	  fill:
	    code = add_points(ppath, points, npoints + code, true);
	    if (code < 0)
		return code;
	    return gx_path_close_subpath(ppath);
	}
    }
    /* General case: construct a path for the fill algorithm. */
    return stroke_add(ppath, first, plp, nplp, pdevc, dev, pis, params,
		      pbbox, uniform);
}

/* Add a segment to the path.  This handles all the complex cases. */
private int
stroke_add(gx_path * ppath, int first, register pl_ptr plp, pl_ptr nplp,
const gx_device_color * pdevc, gx_device * dev, const gs_imager_state * pis,
	const gx_stroke_params * params, const gs_fixed_rect * ignore_pbbox,
	   int uniform)
{
    const gx_line_params *pgs_lp = gs_currentlineparams_inline(pis);
    gs_fixed_point points[8];
    int npoints;
    int code;
    bool moveto_first = true;

    if (plp->thin) {
	/* We didn't set up the endpoint parameters before, */
	/* because the line was thin.  Do it now. */
	set_thin_widths(plp);
	adjust_stroke(plp, pis, true);
	compute_caps(plp);
    }
    /* Create an initial cap if desired. */
    if (first == 0 && pgs_lp->cap == gs_cap_round) {
	if ((code = gx_path_add_point(ppath, plp->o.co.x, plp->o.co.y)) < 0 ||
	    (code = add_round_cap(ppath, &plp->o)) < 0
	    )
	    return code;
	npoints = 0;
	moveto_first = false;
    } else {
	if ((npoints = cap_points((first == 0 ? pgs_lp->cap : gs_cap_butt), &plp->o, points)) < 0)
	    return npoints;
    }
    if (nplp == 0) {
	/* Add a final cap. */
	if (pgs_lp->cap == gs_cap_round) {
	    ASSIGN_POINT(&points[npoints], plp->e.co);
	    ++npoints;
	    if ((code = add_points(ppath, points, npoints, moveto_first)) < 0)
		return code;
	    code = add_round_cap(ppath, &plp->e);
	    goto done;
	}
	code = cap_points(pgs_lp->cap, &plp->e, points + npoints);
    } else if (pgs_lp->join == gs_join_round) {
	ASSIGN_POINT(&points[npoints], plp->e.co);
	++npoints;
	if ((code = add_points(ppath, points, npoints, moveto_first)) < 0)
	    return code;
	code = add_round_cap(ppath, &plp->e);
	goto done;
    } else if (nplp->thin)	/* no join */
	code = cap_points(gs_cap_butt, &plp->e, points + npoints);
    else			/* non-round join */
	code = line_join_points(pgs_lp, plp, nplp, points + npoints,
			      (uniform ? (gs_matrix *) 0 : &ctm_only(pis)));
    if (code < 0)
	return code;
    code = add_points(ppath, points, npoints + code, moveto_first);
  done:
    if (code < 0)
	return code;
    return gx_path_close_subpath(ppath);
}

/* Add lines with a possible initial moveto. */
private int
add_points(gx_path * ppath, const gs_fixed_point * points, int npoints,
	   bool moveto_first)
{
    if (moveto_first) {
	int code = gx_path_add_point(ppath, points[0].x, points[0].y);

	if (code < 0)
	    return code;
	return gx_path_add_lines(ppath, points + 1, npoints - 1);
    } else
	return gx_path_add_lines(ppath, points, npoints);
}

/* ---------------- Join computation ---------------- */

/* Compute the points for a bevel, miter, or triangle join. */
/* Treat no join the same as a bevel join. */
/* If pmat != 0, we must inverse-transform the distances for */
/* the miter check. */
private int
line_join_points(const gx_line_params * pgs_lp, pl_ptr plp, pl_ptr nplp,
		 gs_fixed_point * join_points, const gs_matrix * pmat)
{
    gs_line_join join = pgs_lp->join;

#define jp1 join_points[0]
#define np1 join_points[1]
#define np2 join_points[2]
#define jp2 join_points[3]
#define jpx join_points[4]
    /*
     * Set np to whichever of nplp->o.co or .ce is outside
     * the current line.  We observe that the point (x2,y2)
     * is counter-clockwise from (x1,y1), relative to the origin,
     * iff
     *  (arctan(y2/x2) - arctan(y1/x1)) mod 2*pi < pi,
     * taking the signs of xi and yi into account to determine
     * the quadrants of the results.  It turns out that
     * even though arctan is monotonic only in the 4th/1st
     * quadrants and the 2nd/3rd quadrants, case analysis on
     * the signs of xi and yi demonstrates that this test
     * is equivalent to the much less expensive test
     *  x1 * y2 > x2 * y1
     * in all cases.
     *
     * In the present instance, x1,y1 are plp->width,
     * x2,y2 are nplp->width, and the origin is
     * their common point (plp->e.p, nplp->o.p).
     * ccw will be true iff nplp.o.co (nplp.o.p + width) is
     * counter-clockwise from plp.e.ce (plp.e.p + width),
     * in which case we want tan(a-b) rather than tan(b-a).
     *
     * We make the test using double arithmetic only because
     * the !@#&^*% C language doesn't give us access to
     * the double-width-result multiplication operation
     * that almost all CPUs provide!
     */
    bool ccw =
    (double)(plp->width.x) *	/* x1 */
    (nplp->width.y) >		/* y2 */
    (double)(nplp->width.x) *	/* x2 */
    (plp->width.y);
    p_ptr outp, np;

    /* Initialize for a bevel join. */
    ASSIGN_POINT(&jp1, plp->e.co);
    ASSIGN_POINT(&jp2, plp->e.ce);

    /*
     * Because of stroke adjustment, it is possible that
     * plp->e.p != nplp->o.p.  For that reason, we must use
     * nplp->o.p as np1 or np2.
     */
    if (!ccw) {
	outp = &jp2;
	ASSIGN_POINT(&np2, nplp->o.co);
	ASSIGN_POINT(&np1, nplp->o.p);
	np = &np2;
    } else {
	outp = &jp1;
	ASSIGN_POINT(&np1, nplp->o.ce);
	ASSIGN_POINT(&np2, nplp->o.p);
	np = &np1;
    }
    if_debug1('O', "[o]use %s\n", (ccw ? "co (ccw)" : "ce (cw)"));

    /* Handle triangular joins now. */
    if (join == gs_join_triangle) {
	fixed tpx = outp->x - nplp->o.p.x + np->x;
	fixed tpy = outp->y - nplp->o.p.y + np->y;

	ASSIGN_POINT(&jpx, jp2);
	if (!ccw) {
	    /* Insert tp between np2 and jp2. */
	    jp2.x = tpx, jp2.y = tpy;
	} else {
	    /* Insert tp between jp1 and np1. */
	    ASSIGN_POINT(&jp2, np2);
	    ASSIGN_POINT(&np2, np1);
	    np1.x = tpx, np1.y = tpy;
	}
	return 5;
    }
    /*
     * Don't bother with the miter check if the two
     * points to be joined are very close together,
     * namely, in the same square half-pixel.
     */
    if (join == gs_join_miter &&
	!(fixed2long(outp->x << 1) == fixed2long(np->x << 1) &&
	  fixed2long(outp->y << 1) == fixed2long(np->y << 1))
	) {
	/*
	 * Check whether a miter join is appropriate.
	 * Let a, b be the angles of the two lines.
	 * We check tan(a-b) against the miter_check
	 * by using the following formula:
	 *      If tan(a)=u1/v1 and tan(b)=u2/v2, then
	 *      tan(a-b) = (u1*v2 - u2*v1) / (u1*u2 + v1*v2).
	 *
	 * We can do all the computations unscaled,
	 * because we're only concerned with ratios.
	 * However, if we have a non-uniform coordinate
	 * system (indicated by pmat != 0), we must do the
	 * computations in user space.
	 */
	float check = pgs_lp->miter_check;
	double u1 = plp->e.cdelta.y, v1 = plp->e.cdelta.x;
	double u2 = nplp->o.cdelta.y, v2 = nplp->o.cdelta.x;
	double num, denom;

	if (pmat) {
	    gs_point pt;

	    gs_distance_transform_inverse(v1, u1, pmat, &pt);
	    v1 = pt.x, u1 = pt.y;
	    gs_distance_transform_inverse(v2, u2, pmat, &pt);
	    v2 = pt.x, u2 = pt.y;
	    /*
	     * We need to recompute ccw according to the
	     * relative positions of the lines in user space.
	     * We repeat the computation described above,
	     * using the cdelta values instead of the widths.
	     * Because the definition of ccw above is inverted
	     * from the intuitive one (for historical reasons),
	     * we actually have to do the test backwards.
	     */
	    ccw = v1 * u2 < v2 * u1;
#ifdef DEBUG
	    {
		double a1 = atan2(u1, v1), a2 = atan2(u2, v2), dif = a1 - a2;

		if (dif < 0)
		    dif += 2 * M_PI;
		else if (dif >= 2 * M_PI)
		    dif -= 2 * M_PI;
		if (dif != 0 && (dif < M_PI) != ccw)
		    lprintf8("ccw wrong: tan(a1=%g)=%g/%g, tan(a2=%g)=%g,%g, dif=%g, ccw=%d\n",
			     a1, u1, v1, a2, u2, v2, dif, ccw);
	    }
#endif
	}
	num = u1 * v2 - u2 * v1;
	denom = u1 * u2 + v1 * v2;
	/*
	 * We will want either tan(a-b) or tan(b-a)
	 * depending on the orientations of the lines.
	 * Fortunately we know the relative orientations already.
	 */
	if (!ccw)		/* have plp - nplp, want vice versa */
	    num = -num;
#ifdef DEBUG
	if (gs_debug_c('O')) {
	    dlprintf4("[o]Miter check: u1/v1=%f/%f, u2/v2=%f/%f,\n",
		      u1, v1, u2, v2);
	    dlprintf3("        num=%f, denom=%f, check=%f\n",
		      num, denom, check);
	}
#endif
	/*
	 * If we define T = num / denom, then we want to use
	 * a miter join iff arctan(T) >= arctan(check).
	 * We know that both of these angles are in the 1st
	 * or 2nd quadrant, and since arctan is monotonic
	 * within each quadrant, we can do the comparisons
	 * on T and check directly, taking signs into account
	 * as follows:
	 *              sign(T) sign(check)     atan(T) >= atan(check)
	 *              ------- -----------     ----------------------
	 *              +       +               T >= check
	 *              -       +               true
	 *              +       -               false
	 *              -       -               T >= check
	 */
	if (denom < 0)
	    num = -num, denom = -denom;
	/* Now denom >= 0, so sign(num) = sign(T). */
	if (check > 0 ?
	    (num < 0 || num >= denom * check) :
	    (num < 0 && num >= denom * check)
	    ) {
	    /* OK to use a miter join. */
	    gs_fixed_point mpt;

	    if_debug0('O', "	... passes.\n");
	    /* Compute the intersection of */
	    /* the extended edge lines. */
	    if (line_intersect(outp, &plp->e.cdelta, np,
			       &nplp->o.cdelta, &mpt) == 0
		)
		ASSIGN_POINT(outp, mpt);
	}
    }
    return 4;
}
/* ---------------- Cap computations ---------------- */

/* Compute the endpoints of the two caps of a segment. */
/* Only o.p, e.p, width, and cdelta have been set. */
private void
compute_caps(register pl_ptr plp)
{
    fixed wx2 = plp->width.x;
    fixed wy2 = plp->width.y;

    plp->o.co.x = plp->o.p.x + wx2, plp->o.co.y = plp->o.p.y + wy2;
    plp->o.cdelta.x = -plp->e.cdelta.x,
	plp->o.cdelta.y = -plp->e.cdelta.y;
    plp->o.ce.x = plp->o.p.x - wx2, plp->o.ce.y = plp->o.p.y - wy2;
    plp->e.co.x = plp->e.p.x - wx2, plp->e.co.y = plp->e.p.y - wy2;
    plp->e.ce.x = plp->e.p.x + wx2, plp->e.ce.y = plp->e.p.y + wy2;
#ifdef DEBUG
    if (gs_debug_c('O')) {
	dlprintf4("[o]Stroke o=(%f,%f) e=(%f,%f)\n",
		  fixed2float(plp->o.p.x), fixed2float(plp->o.p.y),
		  fixed2float(plp->e.p.x), fixed2float(plp->e.p.y));
	dlprintf4("\twxy=(%f,%f) lxy=(%f,%f)\n",
		  fixed2float(wx2), fixed2float(wy2),
		  fixed2float(plp->e.cdelta.x),
		  fixed2float(plp->e.cdelta.y));
    }
#endif
}

#define px endp->p.x
#define py endp->p.y
#define xo endp->co.x
#define yo endp->co.y
#define xe endp->ce.x
#define ye endp->ce.y
#define cdx endp->cdelta.x
#define cdy endp->cdelta.y

/* Add a round cap to a path. */
/* Assume the current point is the cap origin (endp->co). */
private int
add_round_cap(gx_path * ppath, const_ep_ptr endp)
{
    fixed xm = px + cdx;
    fixed ym = py + cdy;
    int code;

    if ((code = gx_path_add_partial_arc(ppath, xm, ym,
			   xo + cdx, yo + cdy, quarter_arc_fraction)) < 0 ||
	(code = gx_path_add_partial_arc(ppath, xe, ye,
			      xe + cdx, ye + cdy, quarter_arc_fraction)) < 0
	)
	return code;
    return 0;
}

/* Compute the points for a non-round cap. */
/* Return the number of points. */
private int
cap_points(gs_line_cap type, const_ep_ptr endp,
	   gs_fixed_point * pts /* [3] */ )
{
#define PUT_POINT(i, px, py)\
  pts[i].x = (px), pts[i].y = (py)
    switch (type) {
	case gs_cap_butt:
	    PUT_POINT(0, xo, yo);
	    PUT_POINT(1, xe, ye);
	    return 2;
	case gs_cap_square:
	    PUT_POINT(0, xo + cdx, yo + cdy);
	    PUT_POINT(1, xe + cdx, ye + cdy);
	    return 2;
	case gs_cap_triangle:	/* (not supported by PostScript) */
	    PUT_POINT(0, xo, yo);
	    PUT_POINT(1, px + cdx, py + cdy);
	    PUT_POINT(2, xe, ye);
	    return 3;
	default:		/* can't happen */
	    return_error(gs_error_unregistered);
    }
#undef PUT_POINT
}
