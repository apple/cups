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

/*$Id: gspath1.c,v 1.2 2000/03/08 23:14:46 mike Exp $ */
/* Additional PostScript Level 1 path routines for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxfarith.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gspath.h"
#include "gzpath.h"
#include "gscoord.h"		/* gs_itransform prototype */

/* ------ Arcs ------ */

/* Conversion parameters */
#define degrees_to_radians (M_PI / 180.0)

typedef enum {
    arc_nothing,
    arc_moveto,
    arc_lineto
} arc_action;

typedef struct arc_curve_params_s {
    /* The following are set once. */
    gx_path *ppath;
    gs_imager_state *pis;
    gs_point center;		/* (not used by arc_add) */
    double radius;
    /* The following may be updated dynamically. */
    arc_action action;
    segment_notes notes;
    gs_point p0, p3, pt;
    gs_sincos_t sincos;		/* (not used by arc_add) */
    fixed angle;		/* (not used by arc_add) */
} arc_curve_params_t;

/* Forward declarations */
private int arc_add(P1(const arc_curve_params_t *));

int
gs_arc(gs_state * pgs,
       floatp xc, floatp yc, floatp r, floatp ang1, floatp ang2)
{
    return gs_arc_add_inline(pgs, false, xc, yc, r, ang1, ang2, true);
}

int
gs_arcn(gs_state * pgs,
	floatp xc, floatp yc, floatp r, floatp ang1, floatp ang2)
{
    return gs_arc_add_inline(pgs, true, xc, yc, r, ang1, ang2, true);
}

int
gs_arc_add(gs_state * pgs, bool clockwise, floatp axc, floatp ayc,
	   floatp arad, floatp aang1, floatp aang2, bool add_line)
{
    return gs_arc_add_inline(pgs, clockwise, axc, ayc, arad,
			     aang1, aang2, add_line);
}

/* Compute the next curve as part of an arc. */
private int
next_arc_curve(arc_curve_params_t * arc, fixed anext)
{
    bool ortho = arc->sincos.orthogonal;
    double sin0 = arc->sincos.sin, cos0 = arc->sincos.cos;
    double x0 = arc->p0.x = arc->p3.x;
    double y0 = arc->p0.y = arc->p3.y;
    double x3, y3;

    gs_sincos_degrees(fixed2float(anext), &arc->sincos);
    arc->p3.x = x3 =
	arc->center.x + arc->radius * arc->sincos.cos;
    arc->p3.y = y3 =
	arc->center.y + arc->radius * arc->sincos.sin;
    if (ortho && arc->sincos.orthogonal) {
	/* The common tangent point is easy to compute. */
	if (x0 == arc->center.x)
	    arc->pt.x = x3, arc->pt.y = y0;
	else
	    arc->pt.x = x0, arc->pt.y = y3;
    } else {
	/* Do it the hard way. */
	double trad = arc->radius *
	tan(fixed2float(anext - arc->angle) *
	    (degrees_to_radians / 2));

	arc->pt.x = x0 - trad * sin0;
	arc->pt.y = y0 + trad * cos0;
    }
    arc->angle = anext;
    return arc_add(arc);
}

int
gs_imager_arc_add(gx_path * ppath, gs_imager_state * pis, bool clockwise,
	    floatp axc, floatp ayc, floatp arad, floatp aang1, floatp aang2,
		  bool add_line)
{
    double ar = arad;
    fixed ang1 = float2fixed(aang1), ang2 = float2fixed(aang2), anext;
    double ang1r;		/* reduced angle */
    arc_curve_params_t arc;
    int code;

    arc.ppath = ppath;
    arc.pis = pis;
    arc.center.x = axc;
    arc.center.y = ayc;
#define fixed_90 int2fixed(90)
#define fixed_180 int2fixed(180)
#define fixed_360 int2fixed(360)
    if (ar < 0) {
	ang1 += fixed_180;
	ang2 += fixed_180;
	ar = -ar;
    }
    arc.radius = ar;
    arc.action = (add_line ? arc_lineto : arc_moveto);
    arc.notes = sn_none;
    ang1r = fixed2float(ang1 % fixed_360);
    gs_sincos_degrees(ang1r, &arc.sincos);
    arc.p3.x = axc + ar * arc.sincos.cos;
    arc.p3.y = ayc + ar * arc.sincos.sin;
    if (clockwise) {
	while (ang1 < ang2)
	    ang2 -= fixed_360;
	if (ang2 < 0) {
	    fixed adjust = round_up(-ang2, fixed_360);

	    ang1 += adjust, ang2 += adjust;
	}
	arc.angle = ang1;
	/*
	 * Cut at multiples of 90 degrees.  Invariant: ang1 >= ang2 >= 0.
	 */
	while ((anext = round_down(arc.angle - fixed_epsilon, fixed_90)) > ang2) {
	    code = next_arc_curve(&arc, anext);
	    if (code < 0)
		return code;
	    arc.action = arc_nothing;
	    arc.notes = sn_not_first;
	}
    } else {
	while (ang2 < ang1)
	    ang2 += fixed_360;
	if (ang1 < 0) {
	    fixed adjust = round_up(-ang1, fixed_360);

	    ang1 += adjust, ang2 += adjust;
	}
	arc.angle = ang1;
	/*
	 * Cut at multiples of 90 degrees.  Invariant: 0 <= ang1 <= ang2.
	 * We can't use round_up because of the inchoate definition of
	 * % and / for negative numbers.
	 */
	while ((anext = round_up(arc.angle + fixed_epsilon, fixed_90)) < ang2) {
	    code = next_arc_curve(&arc, anext);
	    if (code < 0)
		return code;
	    arc.action = arc_nothing;
	    arc.notes = sn_not_first;
	}
    }
    /*
     * Do the last curve of the arc.
     */
    return next_arc_curve(&arc, ang2);
}

int
gs_arcto(gs_state * pgs,
floatp ax1, floatp ay1, floatp ax2, floatp ay2, floatp arad, float retxy[4])
{
    double xt0, yt0, xt2, yt2;
    gs_point up0;

#define ax0 up0.x
#define ay0 up0.y
    /* Transform the current point back into user coordinates. */
    int code = gs_currentpoint(pgs, &up0);

    if (code < 0)
	return code;
    {				/* Now we have to compute the tangent points. */
	/* Basically, the idea is to compute the tangent */
	/* of the bisector by using tan(x+y) and tan(z/2) */
	/* formulas, without ever using any trig. */
	double dx0 = ax0 - ax1, dy0 = ay0 - ay1;
	double dx2 = ax2 - ax1, dy2 = ay2 - ay1;

	/* Compute the squared lengths from p1 to p0 and p2. */
	double sql0 = dx0 * dx0 + dy0 * dy0;
	double sql2 = dx2 * dx2 + dy2 * dy2;

	/* Compute the distance from p1 to the tangent points. */
	/* This is the only messy part. */
	double num = dy0 * dx2 - dy2 * dx0;
	double denom = sqrt(sql0 * sql2) - (dx0 * dx2 + dy0 * dy2);

	/* Check for collinear points. */
	if (denom == 0) {
	    code = gs_lineto(pgs, ax1, ay1);
	    xt0 = xt2 = ax1;
	    yt0 = yt2 = ay1;
	} else {		/* not collinear */
	    double dist = fabs(arad * num / denom);
	    double l0 = dist / sqrt(sql0), l2 = dist / sqrt(sql2);
	    arc_curve_params_t arc;

	    arc.ppath = pgs->path;
	    arc.pis = (gs_imager_state *) pgs;
	    arc.radius = arad;
	    arc.action = arc_lineto;
	    arc.notes = sn_none;
	    if (arad < 0)
		l0 = -l0, l2 = -l2;
	    arc.p0.x = xt0 = ax1 + dx0 * l0;
	    arc.p0.y = yt0 = ay1 + dy0 * l0;
	    arc.p3.x = xt2 = ax1 + dx2 * l2;
	    arc.p3.y = yt2 = ay1 + dy2 * l2;
	    arc.pt.x = ax1;
	    arc.pt.y = ay1;
	    code = arc_add(&arc);
	}
    }
    if (retxy != 0) {
	retxy[0] = xt0;
	retxy[1] = yt0;
	retxy[2] = xt2;
	retxy[3] = yt2;
    }
    return code;
}

/* Internal routine for adding an arc to the path. */
private int
arc_add(const arc_curve_params_t * arc)
{
    gx_path *path = arc->ppath;
    gs_imager_state *pis = arc->pis;
    double r = arc->radius;
    double x0 = arc->p0.x, y0 = arc->p0.y;
    double x3 = arc->p3.x, y3 = arc->p3.y;
    double xt = arc->pt.x, yt = arc->pt.y;
    floatp dx = xt - x0, dy = yt - y0;
    double dist = dx * dx + dy * dy;
    double r2 = r * r;
    floatp fraction;
    gs_fixed_point p0, p3, pt, cpt;
    int code;

    /* Compute the fraction coefficient for the curve. */
    /* See gx_path_add_partial_arc for details. */
    if (dist >= r2 * 1.0e8)	/* almost zero radius; */
	/* the >= catches dist == r == 0 */
	fraction = 0.0;
    else
	fraction = (4.0 / 3.0) / (1 + sqrt(1 + dist / r2));
    if_debug8('r',
	      "[r]Arc f=%f p0=(%f,%f) pt=(%f,%f) p3=(%f,%f) action=%d\n",
	      fraction, x0, y0, xt, yt, x3, y3, (int)arc->action);
    if ((code = gs_point_transform2fixed(&pis->ctm, x0, y0, &p0)) < 0 ||
	(code = gs_point_transform2fixed(&pis->ctm, x3, y3, &p3)) < 0 ||
	(code = gs_point_transform2fixed(&pis->ctm, xt, yt, &pt)) < 0 ||
	(code =
	 (arc->action == arc_nothing ? 0 :
	  arc->action == arc_lineto &&
	  gx_path_current_point(path, &cpt) >= 0 ?
	  gx_path_add_line(path, p0.x, p0.y) :
    /* action == arc_moveto */
	  gx_path_add_point(path, p0.x, p0.y))) < 0
	)
	return code;
    return gx_path_add_partial_arc_notes(path, p3.x, p3.y, pt.x, pt.y,
					 fraction, arc->notes);
}

/* ------ Path transformers ------ */

int
gs_dashpath(gs_state * pgs)
{
    gx_path fpath;
    int code;

    if (gs_currentdash_length(pgs) == 0)
	return 0;		/* no dash pattern */
    code = gs_flattenpath(pgs);
    if (code < 0)
	return code;
    gx_path_init_local(&fpath, pgs->memory);
    code = gx_path_add_dash_expansion(pgs->path, &fpath,
				      (gs_imager_state *) pgs);
    if (code < 0) {
	gx_path_free(&fpath, "gs_dashpath");
	return code;
    }
    gx_path_assign_free(pgs->path, &fpath);
    return 0;
}

int
gs_flattenpath(gs_state * pgs)
{
    gx_path *ppath = pgs->path;
    gx_path fpath;
    int code;

    if (!gx_path_has_curves(ppath))
	return 0;		/* nothing to do */
    gx_path_init_local(&fpath, ppath->memory);
    code = gx_path_add_flattened_accurate(ppath, &fpath, pgs->flatness,
					  pgs->accurate_curves);
    if (code < 0) {
	gx_path_free(&fpath, "gs_flattenpath");
	return code;
    }
    gx_path_assign_free(ppath, &fpath);
    return 0;
}

int
gs_reversepath(gs_state * pgs)
{
    gx_path *ppath = pgs->path;
    gx_path rpath;
    int code;

    gx_path_init_local(&rpath, ppath->memory);
    code = gx_path_copy_reversed(ppath, &rpath);
    if (code < 0) {
	gx_path_free(&rpath, "gs_reversepath");
	return code;
    }
    gx_path_assign_free(ppath, &rpath);
    return 0;
}

/* ------ Accessors ------ */

int
gs_upathbbox(gs_state * pgs, gs_rect * pbox, bool include_moveto)
{
    gs_fixed_rect fbox;		/* box in device coordinates */
    gs_rect dbox;
    int code = gx_path_bbox(pgs->path, &fbox);

    if (code < 0)
	return code;
    /* If the path ends with a moveto and include_moveto is true, */
    /* include the moveto in the bounding box. */
    if (path_last_is_moveto(pgs->path) && include_moveto) {
	gs_fixed_point pt;

	gx_path_current_point_inline(pgs->path, &pt);
	if (pt.x < fbox.p.x)
	    fbox.p.x = pt.x;
	if (pt.y < fbox.p.y)
	    fbox.p.y = pt.y;
	if (pt.x > fbox.q.x)
	    fbox.q.x = pt.x;
	if (pt.y > fbox.q.y)
	    fbox.q.y = pt.y;
    }
    /* Transform the result back to user coordinates. */
    dbox.p.x = fixed2float(fbox.p.x);
    dbox.p.y = fixed2float(fbox.p.y);
    dbox.q.x = fixed2float(fbox.q.x);
    dbox.q.y = fixed2float(fbox.q.y);
    return gs_bbox_transform_inverse(&dbox, &ctm_only(pgs), pbox);
}

/* ------ Enumerators ------ */

/* Start enumerating a path */
int
gs_path_enum_copy_init(gs_path_enum * penum, const gs_state * pgs, bool copy)
{
    gs_memory_t *mem = pgs->memory;

    if (copy) {
	gx_path *copied_path =
	gx_path_alloc(mem, "gs_path_enum_init");
	int code;

	if (copied_path == 0)
	    return_error(gs_error_VMerror);
	code = gx_path_copy(pgs->path, copied_path);
	if (code < 0) {
	    gx_path_free(copied_path, "gs_path_enum_init");
	    return code;
	}
	gx_path_enum_init(penum, copied_path);
	penum->copied_path = copied_path;
    } else {
	gx_path_enum_init(penum, pgs->path);
    }
    penum->memory = mem;
    gs_currentmatrix(pgs, &penum->mat);
    return 0;
}

/* Enumerate the next element of a path. */
/* If the path is finished, return 0; */
/* otherwise, return the element type. */
int
gs_path_enum_next(gs_path_enum * penum, gs_point ppts[3])
{
    gs_fixed_point fpts[3];
    int pe_op = gx_path_enum_next(penum, fpts);
    int code;

    switch (pe_op) {
	case 0:		/* all done */
	case gs_pe_closepath:
	    break;
	case gs_pe_curveto:
	    if ((code = gs_point_transform_inverse(
						      fixed2float(fpts[1].x),
						      fixed2float(fpts[1].y),
					      &penum->mat, &ppts[1])) < 0 ||
		(code = gs_point_transform_inverse(
						      fixed2float(fpts[2].x),
						      fixed2float(fpts[2].y),
						&penum->mat, &ppts[2])) < 0)
		return code;
	    /* falls through */
	case gs_pe_moveto:
	case gs_pe_lineto:
	    if ((code = gs_point_transform_inverse(
						      fixed2float(fpts[0].x),
						      fixed2float(fpts[0].y),
						&penum->mat, &ppts[0])) < 0)
		return code;
	default:		/* error */
	    break;
    }
    return pe_op;
}

/* Clean up after a pathforall. */
void
gs_path_enum_cleanup(gs_path_enum * penum)
{
    if (penum->copied_path != 0) {
	gx_path_free(penum->copied_path, "gs_path_enum_cleanup");
	penum->path = 0;
	penum->copied_path = 0;
    }
}
