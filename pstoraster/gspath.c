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

/*$Id: gspath.c,v 1.3 2000/06/23 14:48:50 mike Exp $ */
/* Basic path routines for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gscoord.h"		/* requires gsmatrix.h */
#include "gzstate.h"
#include "gzpath.h"
#include "gxdevice.h"		/* for gxcpath.h */
#include "gxdevmem.h"		/* for gs_device_is_memory */
#include "gzcpath.h"

/* ------ Miscellaneous ------ */

int
gs_newpath(gs_state * pgs)
{
    return gx_path_new(pgs->path);
}

int
gs_closepath(gs_state * pgs)
{
    gx_path *ppath = pgs->path;
    int code = gx_path_close_subpath(ppath);

    if (code < 0)
	return code;
    if (path_start_outside_range(ppath))
	path_set_outside_position(ppath, ppath->outside_start.x,
				  ppath->outside_start.y);
    return code;
}

int
gs_upmergepath(gs_state * pgs)
{
    return gx_path_add_path(pgs->saved->path, pgs->path);
}

/* Get the current path (for internal use only). */
gx_path *
gx_current_path(const gs_state * pgs)
{
    return pgs->path;
}

/* ------ Points and lines ------ */

/*
 * Define clamped values for out-of-range coordinates.
 * Currently the path drawing routines can't handle values
 * close to the edge of the representable space.
 */
#define max_coord_fixed (max_fixed - int2fixed(1000))	/* arbitrary */
#define min_coord_fixed (-max_coord_fixed)
private void
clamp_point(gs_fixed_point * ppt, floatp x, floatp y)
{
#define clamp_coord(xy)\
  ppt->xy = (xy > fixed2float(max_coord_fixed) ? max_coord_fixed :\
	     xy < fixed2float(min_coord_fixed) ? min_coord_fixed :\
	     float2fixed(xy))
    clamp_coord(x);
    clamp_coord(y);
#undef clamp_coord
}

int
gs_currentpoint(const gs_state * pgs, gs_point * ppt)
{
    gx_path *ppath = pgs->path;
    int code;
    gs_fixed_point pt;

    if (path_outside_range(ppath))
	return gs_itransform((gs_state *) pgs,
			     ppath->outside_position.x,
			     ppath->outside_position.y, ppt);
    code = gx_path_current_point(pgs->path, &pt);
    if (code < 0)
	return code;
    return gs_itransform((gs_state *) pgs,
			 fixed2float(pt.x), fixed2float(pt.y), ppt);
}

int
gs_moveto(gs_state * pgs, floatp x, floatp y)
{
    gx_path *ppath = pgs->path;
    gs_fixed_point pt;
    int code;

    if ((code = gs_point_transform2fixed(&pgs->ctm, x, y, &pt)) < 0) {
	if (pgs->clamp_coordinates) {	/* Handle out-of-range coordinates. */
	    gs_point opt;

	    if (code != gs_error_limitcheck ||
		(code = gs_transform(pgs, x, y, &opt)) < 0
		)
		return code;
	    clamp_point(&pt, opt.x, opt.y);
	    code = gx_path_add_point(ppath, pt.x, pt.y);
	    if (code < 0)
		return code;
	    path_set_outside_position(ppath, opt.x, opt.y);
	    ppath->outside_start = ppath->outside_position;
	    ppath->start_flags = ppath->state_flags;
	}
	return code;
    }
    return gx_path_add_point(ppath, pt.x, pt.y);
}

int
gs_rmoveto(gs_state * pgs, floatp x, floatp y)
{
    gs_fixed_point dpt;
    int code;

    if ((code = gs_distance_transform2fixed(&pgs->ctm, x, y, &dpt)) < 0 ||
	(code = gx_path_add_relative_point(pgs->path, dpt.x, dpt.y)) < 0
	) {			/* Handle all exceptional conditions here. */
	gs_point upt;

	if ((code = gs_currentpoint(pgs, &upt)) < 0)
	    return code;
	return gs_moveto(pgs, upt.x + x, upt.y + y);
    }
    return code;
}

int
gs_lineto(gs_state * pgs, floatp x, floatp y)
{
    gx_path *ppath = pgs->path;
    int code;
    gs_fixed_point pt;

    if ((code = gs_point_transform2fixed(&pgs->ctm, x, y, &pt)) < 0) {
	if (pgs->clamp_coordinates) {	/* Handle out-of-range coordinates. */
	    gs_point opt;

	    if (code != gs_error_limitcheck ||
		(code = gs_transform(pgs, x, y, &opt)) < 0
		)
		return code;
	    clamp_point(&pt, opt.x, opt.y);
	    code = gx_path_add_line(ppath, pt.x, pt.y);
	    if (code < 0)
		return code;
	    path_set_outside_position(ppath, opt.x, opt.y);
	}
	return code;
    }
    return gx_path_add_line(pgs->path, pt.x, pt.y);
}

int
gs_rlineto(gs_state * pgs, floatp x, floatp y)
{
    gx_path *ppath = pgs->path;
    gs_fixed_point dpt;
    fixed nx, ny;
    int code;

    if (!path_position_in_range(ppath) ||
	(code = gs_distance_transform2fixed(&pgs->ctm, x, y, &dpt)) < 0 ||
    /* Check for overflow in addition. */
	(((nx = ppath->position.x + dpt.x) ^ dpt.x) < 0 &&
	 (ppath->position.x ^ dpt.x) >= 0) ||
	(((ny = ppath->position.y + dpt.y) ^ dpt.y) < 0 &&
	 (ppath->position.y ^ dpt.y) >= 0) ||
	(code = gx_path_add_line(ppath, nx, ny)) < 0
	) {			/* Handle all exceptional conditions here. */
	gs_point upt;

	if ((code = gs_currentpoint(pgs, &upt)) < 0)
	    return code;
	return gs_lineto(pgs, upt.x + x, upt.y + y);
    }
    return code;
}

/* ------ Curves ------ */

int
gs_curveto(gs_state * pgs,
	   floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3)
{
    gs_fixed_point p1, p2, p3;
    int code1 = gs_point_transform2fixed(&pgs->ctm, x1, y1, &p1);
    int code2 = gs_point_transform2fixed(&pgs->ctm, x2, y2, &p2);
    int code3 = gs_point_transform2fixed(&pgs->ctm, x3, y3, &p3);
    gx_path *ppath = pgs->path;

    if ((code1 | code2 | code3) < 0) {
	if (pgs->clamp_coordinates) {	/* Handle out-of-range coordinates. */
	    gs_point opt1, opt2, opt3;
	    int code;

	    if ((code1 < 0 && code1 != gs_error_limitcheck) ||
		(code1 = gs_transform(pgs, x1, y1, &opt1)) < 0
		)
		return code1;
	    if ((code2 < 0 && code2 != gs_error_limitcheck) ||
		(code2 = gs_transform(pgs, x2, y2, &opt2)) < 0
		)
		return code2;
	    if ((code3 < 0 && code3 != gs_error_limitcheck) ||
		(code3 = gs_transform(pgs, x3, y3, &opt3)) < 0
		)
		return code3;
	    clamp_point(&p1, opt1.x, opt1.y);
	    clamp_point(&p2, opt2.x, opt2.y);
	    clamp_point(&p3, opt3.x, opt3.y);
	    code = gx_path_add_curve(ppath,
				     p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	    if (code < 0)
		return code;
	    path_set_outside_position(ppath, opt3.x, opt3.y);
	    return code;
	} else
	    return (code1 < 0 ? code1 : code2 < 0 ? code2 : code3);
    }
    return gx_path_add_curve(ppath,
			     p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

int
gs_rcurveto(gs_state * pgs,
     floatp dx1, floatp dy1, floatp dx2, floatp dy2, floatp dx3, floatp dy3)
{
    gx_path *ppath = pgs->path;
    gs_fixed_point p1, p2, p3;
    fixed ptx, pty;
    int code;

/****** SHOULD CHECK FOR OVERFLOW IN ADDITION ******/
    if (!path_position_in_range(ppath) ||
	(code = gs_distance_transform2fixed(&pgs->ctm, dx1, dy1, &p1)) < 0 ||
	(code = gs_distance_transform2fixed(&pgs->ctm, dx2, dy2, &p2)) < 0 ||
	(code = gs_distance_transform2fixed(&pgs->ctm, dx3, dy3, &p3)) < 0 ||
	(ptx = ppath->position.x, pty = ppath->position.y,
	 code = gx_path_add_curve(ppath, ptx + p1.x, pty + p1.y,
				  ptx + p2.x, pty + p2.y,
				  ptx + p3.x, pty + p3.y)) < 0
	) {			/* Handle all exceptional conditions here. */
	gs_point upt;

	if ((code = gs_currentpoint(pgs, &upt)) < 0)
	    return code;
	return gs_curveto(pgs, upt.x + dx1, upt.y + dy1,
			  upt.x + dx2, upt.y + dy2,
			  upt.x + dx3, upt.y + dy3);
    }
    return code;
}

/* ------ Clipping ------ */

/* Forward references */
private int common_clip(P2(gs_state *, int));

/*
 * Return the effective clipping path of a graphics state.  Sometimes this
 * is the intersection of the clip path and the view clip path; sometimes it
 * is just the clip path.  We aren't sure what the correct algorithm is for
 * this: for now, we use view clipping unless the current device is a memory
 * device.  This takes care of the most important case, where the current
 * device is a cache device.
 */
int
gx_effective_clip_path(gs_state * pgs, gx_clip_path ** ppcpath)
{
    gs_id view_clip_id =
	(pgs->view_clip == 0 || pgs->view_clip->rule == 0 ? gs_no_id :
	 pgs->view_clip->id);

    if (gs_device_is_memory(pgs->device)) {
	*ppcpath = pgs->clip_path;
	return 0;
    }
    if (pgs->effective_clip_id == pgs->clip_path->id &&
	pgs->effective_view_clip_id == view_clip_id
	) {
	*ppcpath = pgs->effective_clip_path;
	return 0;
    }
    /* Update the cache. */
    if (view_clip_id == gs_no_id) {
	if (!pgs->effective_clip_shared)
	    gx_cpath_free(pgs->effective_clip_path, "gx_effective_clip_path");
	pgs->effective_clip_path = pgs->clip_path;
	pgs->effective_clip_shared = true;
    } else {
	gs_fixed_rect cbox, vcbox;

	gx_cpath_inner_box(pgs->clip_path, &cbox);
	gx_cpath_outer_box(pgs->view_clip, &vcbox);
	if (rect_within(vcbox, cbox)) {
	    if (!pgs->effective_clip_shared)
		gx_cpath_free(pgs->effective_clip_path,
			      "gx_effective_clip_path");
	    pgs->effective_clip_path = pgs->view_clip;
	    pgs->effective_clip_shared = true;
	} else {
	    /* Construct the intersection of the two clip paths. */
	    int code;
	    gx_clip_path ipath;
	    gx_path vpath;
	    gx_clip_path *npath = pgs->effective_clip_path;

	    if (pgs->effective_clip_shared) {
		npath = gx_cpath_alloc(pgs->memory, "gx_effective_clip_path");
		if (npath == 0)
		    return_error(gs_error_VMerror);
	    }
	    gx_cpath_init_local(&ipath, pgs->memory);
	    code = gx_cpath_assign_preserve(&ipath, pgs->clip_path);
	    if (code < 0)
		return code;
	    gx_path_init_local(&vpath, pgs->memory);
	    code = gx_cpath_to_path(pgs->view_clip, &vpath);
	    if (code < 0 ||
		(code = gx_cpath_clip(pgs, &ipath, &vpath,
				      gx_rule_winding_number)) < 0 ||
		(code = gx_cpath_assign_free(npath, &ipath)) < 0
		)
		DO_NOTHING;
	    gx_path_free(&vpath, "gx_effective_clip_path");
	    gx_cpath_free(&ipath, "gx_effective_clip_path");
	    if (code < 0)
		return code;
	    pgs->effective_clip_path = npath;
	    pgs->effective_clip_shared = false;
	}
    }
    pgs->effective_clip_id = pgs->clip_path->id;
    pgs->effective_view_clip_id = view_clip_id;
    *ppcpath = pgs->effective_clip_path;
    return 0;
}

#ifdef DEBUG
/* Note that we just set the clipping path (internal). */
private void
note_set_clip_path(const gs_state * pgs)
{
    if (gs_debug_c('P')) {
	extern void gx_cpath_print(P1(const gx_clip_path *));

	dlprintf("[P]Clipping path:\n");
	gx_cpath_print(pgs->clip_path);
    }
}
#else
#  define note_set_clip_path(pgs) DO_NOTHING
#endif

int
gs_clippath(gs_state * pgs)
{
    gx_path cpath;
    int code;

    gx_path_init_local(&cpath, pgs->memory);
    code = gx_cpath_to_path(pgs->clip_path, &cpath);
    if (code >= 0)
	code = gx_path_assign_free(pgs->path, &cpath);
    if (code < 0)
	gx_path_free(&cpath, "gs_clippath");
    return code;
}

int
gs_initclip(gs_state * pgs)
{
    gs_fixed_rect box;
    int code = gx_default_clip_box(pgs, &box);

    if (code < 0)
	return code;
    return gx_clip_to_rectangle(pgs, &box);
}

int
gs_clip(gs_state * pgs)
{
    return common_clip(pgs, gx_rule_winding_number);
}

int
gs_eoclip(gs_state * pgs)
{
    return common_clip(pgs, gx_rule_even_odd);
}

private int
common_clip(gs_state * pgs, int rule)
{
    int code = gx_cpath_clip(pgs, pgs->clip_path, pgs->path, rule);
    if (code < 0)
	return code;
    pgs->clip_path->rule = rule;
    note_set_clip_path(pgs);
    return 0;
}

int
gs_setclipoutside(gs_state * pgs, bool outside)
{
    return gx_cpath_set_outside(pgs->clip_path, outside);
}

bool
gs_currentclipoutside(const gs_state * pgs)
{
    return gx_cpath_is_outside(pgs->clip_path);
}

/* Establish a rectangle as the clipping path. */
/* Used by initclip and by the character and Pattern cache logic. */
int
gx_clip_to_rectangle(gs_state * pgs, gs_fixed_rect * pbox)
{
    int code = gx_cpath_from_rectangle(pgs->clip_path, pbox);

    if (code < 0)
	return code;
    pgs->clip_path->rule = gx_rule_winding_number;
    note_set_clip_path(pgs);
    return 0;
}

/* Set the clipping path to the current path, without intersecting. */
/* This is very inefficient right now. */
int
gx_clip_to_path(gs_state * pgs)
{
    gs_fixed_rect bbox;
    int code;

    if ((code = gx_path_bbox(pgs->path, &bbox)) < 0 ||
	(code = gx_clip_to_rectangle(pgs, &bbox)) < 0 ||
	(code = gs_clip(pgs)) < 0
	)
	return code;
    note_set_clip_path(pgs);
    return 0;
}

/* Get the default clipping box. */
int
gx_default_clip_box(const gs_state * pgs, gs_fixed_rect * pbox)
{
    register gx_device *dev = gs_currentdevice(pgs);
    gs_rect bbox;
    gs_matrix imat;
    int code;

    if (dev->ImagingBBox_set) {	/* Use the ImagingBBox, relative to default user space. */
	gs_defaultmatrix(pgs, &imat);
	bbox.p.x = dev->ImagingBBox[0];
	bbox.p.y = dev->ImagingBBox[1];
	bbox.q.x = dev->ImagingBBox[2];
	bbox.q.y = dev->ImagingBBox[3];
    } else {			/* Use the PageSize indented by the HWMargins, */
	/* relative to unrotated user space adjusted by */
	/* the Margins.  (We suspect this isn't quite right, */
	/* but the whole issue of "margins" is such a mess that */
	/* we don't think we can do any better.) */
	(*dev_proc(dev, get_initial_matrix)) (dev, &imat);
	/* Adjust for the Margins. */
	imat.tx += dev->Margins[0] * dev->HWResolution[0] /
	    dev->MarginsHWResolution[0];
	imat.ty += dev->Margins[1] * dev->HWResolution[1] /
	    dev->MarginsHWResolution[1];
	bbox.p.x = dev->HWMargins[0];
	bbox.p.y = dev->HWMargins[1];
	bbox.q.x = dev->PageSize[0] - dev->HWMargins[2];
	bbox.q.y = dev->PageSize[1] - dev->HWMargins[3];
    }
    code = gs_bbox_transform(&bbox, &imat, &bbox);
    if (code < 0)
	return code;
    /* Round the clipping box so that it doesn't get ceilinged. */
    pbox->p.x = fixed_rounded(float2fixed(bbox.p.x));
    pbox->p.y = fixed_rounded(float2fixed(bbox.p.y));
    pbox->q.x = fixed_rounded(float2fixed(bbox.q.x));
    pbox->q.y = fixed_rounded(float2fixed(bbox.q.y));
    return 0;
}
