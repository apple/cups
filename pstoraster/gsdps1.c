/* Copyright (C) 1991, 1992, 1994, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsdps1.c,v 1.2 2000/03/08 23:14:40 mike Exp $ */
/* Display PostScript graphics additions for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gspaint.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gspath.h"
#include "gspath2.h"		/* defines interface */
#include "gzpath.h"
#include "gzcpath.h"
#include "gzstate.h"

/*
 * Define how much rounding slop setbbox should leave,
 * in device coordinates.  Because of rounding in transforming
 * path coordinates to fixed point, the minimum realistic value is:
 *
 *      #define box_rounding_slop_fixed (fixed_epsilon)
 *
 * But even this isn't enough to compensate for cumulative rounding error
 * in rmoveto or rcurveto.  Instead, we somewhat arbitrarily use:
 */
#define box_rounding_slop_fixed (fixed_epsilon * 3)

/* ------ Graphics state ------ */

/* Set the bounding box for the current path. */
int
gs_setbbox(gs_state * pgs, floatp llx, floatp lly, floatp urx, floatp ury)
{
    gs_rect ubox, dbox;
    gs_fixed_rect obox, bbox;
    gx_path *ppath = pgs->path;
    int code;

    if (llx > urx || lly > ury)
	return_error(gs_error_rangecheck);
    /* Transform box to device coordinates. */
    ubox.p.x = llx;
    ubox.p.y = lly;
    ubox.q.x = urx;
    ubox.q.y = ury;
    if ((code = gs_bbox_transform(&ubox, &ctm_only(pgs), &dbox)) < 0)
	return code;
    /* Round the corners in opposite directions. */
    /* Because we can't predict the magnitude of the dbox values, */
    /* we add/subtract the slop after fixing. */
    if (dbox.p.x < fixed2float(min_fixed + box_rounding_slop_fixed) ||
	dbox.p.y < fixed2float(min_fixed + box_rounding_slop_fixed) ||
	dbox.q.x >= fixed2float(max_fixed - box_rounding_slop_fixed + fixed_epsilon) ||
	dbox.q.y >= fixed2float(max_fixed - box_rounding_slop_fixed + fixed_epsilon)
	)
	return_error(gs_error_limitcheck);
    bbox.p.x =
	(fixed) floor(dbox.p.x * fixed_scale) - box_rounding_slop_fixed;
    bbox.p.y =
	(fixed) floor(dbox.p.y * fixed_scale) - box_rounding_slop_fixed;
    bbox.q.x =
	(fixed) ceil(dbox.q.x * fixed_scale) + box_rounding_slop_fixed;
    bbox.q.y =
	(fixed) ceil(dbox.q.y * fixed_scale) + box_rounding_slop_fixed;
    if (gx_path_bbox(ppath, &obox) >= 0) {	/* Take the union of the bboxes. */
	ppath->bbox.p.x = min(obox.p.x, bbox.p.x);
	ppath->bbox.p.y = min(obox.p.y, bbox.p.y);
	ppath->bbox.q.x = max(obox.q.x, bbox.q.x);
	ppath->bbox.q.y = max(obox.q.y, bbox.q.y);
    } else {			/* empty path *//* Just set the bbox. */
	ppath->bbox = bbox;
    }
    ppath->bbox_set = 1;
    return 0;
}

/* ------ Rectangles ------ */

/* Append a list of rectangles to a path. */
int
gs_rectappend(gs_state * pgs, const gs_rect * pr, uint count)
{
    for (; count != 0; count--, pr++) {
	floatp px = pr->p.x, py = pr->p.y, qx = pr->q.x, qy = pr->q.y;
	int code;

	/* Ensure counter-clockwise drawing. */
	if ((qx >= px) != (qy >= py))
	    qx = px, px = pr->q.x;	/* swap x values */
	if ((code = gs_moveto(pgs, px, py)) < 0 ||
	    (code = gs_lineto(pgs, qx, py)) < 0 ||
	    (code = gs_lineto(pgs, qx, qy)) < 0 ||
	    (code = gs_lineto(pgs, px, qy)) < 0 ||
	    (code = gs_closepath(pgs)) < 0
	    )
	    return code;
    }
    return 0;
}

/* Clip to a list of rectangles. */
int
gs_rectclip(gs_state * pgs, const gs_rect * pr, uint count)
{
    int code;
    gx_path save;

    gx_path_init_local(&save, pgs->memory);
    gx_path_assign_preserve(&save, pgs->path);
    gs_newpath(pgs);
    if ((code = gs_rectappend(pgs, pr, count)) < 0 ||
	(code = gs_clip(pgs)) < 0
	) {
	gx_path_assign_free(pgs->path, &save);
	return code;
    }
    gx_path_free(&save, "gs_rectclip");
    gs_newpath(pgs);
    return 0;
}

/* Fill a list of rectangles. */
/* We take the trouble to do this efficiently in the simple cases. */
int
gs_rectfill(gs_state * pgs, const gs_rect * pr, uint count)
{
    const gs_rect *rlist = pr;
    gx_clip_path *pcpath;
    uint rcount = count;
    int code;

    gx_set_dev_color(pgs);
    if ((is_fzero2(pgs->ctm.xy, pgs->ctm.yx) ||
	 is_fzero2(pgs->ctm.xx, pgs->ctm.yy)) &&
	gx_effective_clip_path(pgs, &pcpath) >= 0 &&
	clip_list_is_rectangle(gx_cpath_list(pcpath)) &&
	gs_state_color_load(pgs) >= 0 &&
	(*dev_proc(pgs->device, get_alpha_bits)) (pgs->device, go_graphics)
	<= 1
	) {
	uint i;
	gs_fixed_rect clip_rect;

	gx_cpath_inner_box(pcpath, &clip_rect);
	for (i = 0; i < count; ++i) {
	    gs_fixed_point p, q;
	    gs_fixed_rect draw_rect;
	    int x, y, w, h;

	    if (gs_point_transform2fixed(&pgs->ctm, pr[i].p.x, pr[i].p.y, &p) < 0 ||
	    gs_point_transform2fixed(&pgs->ctm, pr[i].q.x, pr[i].q.y, &q) < 0
		) {		/* Switch to the slow algorithm. */
		goto slow;
	    }
	    draw_rect.p.x = min(p.x, q.x) - pgs->fill_adjust.x;
	    draw_rect.p.y = min(p.y, q.y) - pgs->fill_adjust.y;
	    draw_rect.q.x = max(p.x, q.x) + pgs->fill_adjust.x;
	    draw_rect.q.y = max(p.y, q.y) + pgs->fill_adjust.y;
	    rect_intersect(draw_rect, clip_rect);
	    x = fixed2int_pixround(draw_rect.p.x);
	    y = fixed2int_pixround(draw_rect.p.y);
	    w = fixed2int_pixround(draw_rect.q.x) - x;
	    h = fixed2int_pixround(draw_rect.q.y) - y;
	    if (w > 0 && h > 0) {
		if (gx_fill_rectangle(x, y, w, h, pgs->dev_color, pgs) < 0)
		    goto slow;
	    }
	}
	return 0;
      slow:rlist = pr + i;
	rcount = count - i;
    } {
	bool do_save = !gx_path_is_null(pgs->path);

	if (do_save) {
	    if ((code = gs_gsave(pgs)) < 0)
		return code;
	    gs_newpath(pgs);
	}
	if ((code = gs_rectappend(pgs, rlist, rcount)) < 0 ||
	    (code = gs_fill(pgs)) < 0
	    )
	    DO_NOTHING;
	if (do_save)
	    gs_grestore(pgs);
	else if (code < 0)
	    gs_newpath(pgs);
    }
    return code;
}

/* Stroke a list of rectangles. */
/* (We could do this a lot more efficiently.) */
int
gs_rectstroke(gs_state * pgs, const gs_rect * pr, uint count,
	      const gs_matrix * pmat)
{
    bool do_save = pmat != NULL || !gx_path_is_null(pgs->path);
    int code;

    if (do_save) {
	if ((code = gs_gsave(pgs)) < 0)
	    return code;
	gs_newpath(pgs);
    }
    if ((code = gs_rectappend(pgs, pr, count)) < 0 ||
	(pmat != NULL && (code = gs_concat(pgs, pmat)) < 0) ||
	(code = gs_stroke(pgs)) < 0
	)
	DO_NOTHING;
    if (do_save)
	gs_grestore(pgs);
    else if (code < 0)
	gs_newpath(pgs);
    return code;
}
