/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxshade1.c,v 1.1 2000/03/08 23:15:05 mike Exp $ */
/* Rendering for non-mesh shadings */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gspath.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxpath.h"
#include "gxshade.h"

/* ================ Utilities ================ */

/* Check whether 2 colors fall within the smoothness criterion. */
private bool
shade_colors2_converge(const gs_client_color cc[2],
		       const shading_fill_state_t * pfs)
{
    int ci;

    for (ci = pfs->num_components - 1; ci >= 0; --ci)
	if (fabs(cc[1].paint.values[ci] - cc[0].paint.values[ci]) >
	    pfs->cc_max_error[ci]
	    )
	    return false;
    return true;
}

/* Fill a user space rectangle that is also a device space rectangle. */
private int
shade_fill_device_rectangle(const shading_fill_state_t * pfs,
			    const gs_fixed_point * p0,
			    const gs_fixed_point * p1,
			    gx_device_color * pdevc)
{
    gs_imager_state *pis = pfs->pis;
    fixed xmin, ymin, xmax, ymax;
    int x, y;

    if (p0->x < p1->x)
	xmin = p0->x, xmax = p1->x;
    else
	xmin = p1->x, xmax = p0->x;
    if (p0->y < p1->y)
	ymin = p0->y, ymax = p1->y;
    else
	ymin = p1->y, ymax = p0->y;
    /****** NOT QUITE RIGHT FOR PIXROUND ******/
    xmin -= pis->fill_adjust.x;
    xmax += pis->fill_adjust.x;
    ymin -= pis->fill_adjust.y;
    ymax += pis->fill_adjust.y;
    x = fixed2int_var(xmin);
    y = fixed2int_var(ymin);
    return
	gx_fill_rectangle_device_rop(x, y,
				     fixed2int_var(xmax) - x,
				     fixed2int_var(ymax) - y,
				     pdevc, pfs->dev, pis->log_op);
}

/* ================ Specific shadings ================ */

/* ---------------- Function-based shading ---------------- */

typedef struct Fb_fill_state_s {
    shading_fill_state_common;
    const gs_shading_Fb_t *psh;
    gs_matrix_fixed ptm;	/* parameter space -> device space */
    bool orthogonal;		/* true iff ptm is xxyy or xyyx */
} Fb_fill_state_t;

private int
Fb_fill_region(const Fb_fill_state_t * pfs, gs_client_color cc[4],
	       floatp x0, floatp y0, floatp x1, floatp y1)
{
    const gs_shading_Fb_t * const psh = pfs->psh;
    gs_imager_state *pis = pfs->pis;

top:
    if (!shade_colors4_converge(cc, (const shading_fill_state_t *)pfs)) {
	/*
	 * The colors don't converge.  Does the region color more than
	 * a single pixel?
	 */
	gs_rect region;

	region.p.x = x0, region.p.y = y0;
	region.q.x = x1, region.q.y = y1;
	gs_bbox_transform(&region, (const gs_matrix *)&pfs->ptm, &region);
	if (region.q.x - region.p.x > 1 || region.q.y - region.p.y > 1)
	    goto recur;
	{
	    /*
	     * More precisely, does the bounding box of the region,
	     * taking fill adjustment into account, span more than 1
	     * pixel center in either X or Y?
	     */
	    fixed ax = pis->fill_adjust.x;
	    int nx =
		fixed2int_pixround(float2fixed(region.q.x) + ax) -
		fixed2int_pixround(float2fixed(region.p.x) - ax);
	    fixed ay = pis->fill_adjust.y;
	    int ny =
		fixed2int_pixround(float2fixed(region.q.y) + ay) -
		fixed2int_pixround(float2fixed(region.p.y) - ay);

	    if ((nx > 1 && ny != 0) || (ny > 1 && nx != 0))
		goto recur;
	}
	/* We could do the 1-pixel case a lot faster! */
    }
    /* Fill the region with the color. */
    {
	gx_device_color dev_color;
	const gs_color_space *pcs = psh->params.ColorSpace;
	gs_fixed_point pts[4];
	int code;

	if_debug0('|', "[|]... filling region\n");
	(*pcs->type->restrict_color)(&cc[0], pcs);
	(*pcs->type->remap_color)(&cc[0], pcs, &dev_color, pis,
				  pfs->dev, gs_color_select_texture);
	gs_point_transform2fixed(&pfs->ptm, x0, y0, &pts[0]);
	gs_point_transform2fixed(&pfs->ptm, x1, y1, &pts[2]);
	if (pfs->orthogonal) {
	    code =
		shade_fill_device_rectangle((const shading_fill_state_t *)pfs,
					    &pts[0], &pts[2], &dev_color);
	} else {
	    gx_path *ppath = gx_path_alloc(pis->memory, "Fb_fill");

	    gs_point_transform2fixed(&pfs->ptm, x1, y0, &pts[1]);
	    gs_point_transform2fixed(&pfs->ptm, x0, y1, &pts[3]);
	    gx_path_add_point(ppath, pts[0].x, pts[0].y);
	    gx_path_add_lines(ppath, pts + 1, 3);
	    code = shade_fill_path((const shading_fill_state_t *)pfs,
				   ppath, &dev_color);
	    gx_path_free(ppath, "Fb_fill");
	}
	return code;
    }

    /*
     * No luck.  Subdivide the region and recur.
     *
     * We should subdivide on the axis that has the largest color
     * discrepancy, but for now we subdivide on the axis with the
     * largest coordinate difference.
     */
recur:
    {
	gs_client_color mid[2];
	gs_client_color rcc[4];
	gs_function_t *pfn = psh->params.Function;
	float v[2];
	int code;

	if (y1 - y0 > x1 - x0) {
	    /* Subdivide in Y. */
	    float ym = (y0 + y1) * 0.5;

	    if_debug1('|', "[|]dividing at y=%g\n", ym);
	    v[1] = ym;
	    v[0] = x0;
	    code = gs_function_evaluate(pfn, v, mid[0].paint.values);
	    if (code < 0)
		return code;
	    v[0] = x1;
	    code = gs_function_evaluate(pfn, v, mid[1].paint.values);
	    if (code < 0)
		return code;
	    rcc[0].paint = cc[0].paint;
	    rcc[1].paint = cc[1].paint;
	    rcc[2].paint = mid[0].paint;
	    rcc[3].paint = mid[1].paint;
	    code = Fb_fill_region(pfs, rcc, x0, y0, x1, ym);
	    cc[0].paint = mid[0].paint;
	    cc[1].paint = mid[1].paint;
	    y0 = ym;
	} else {
	    /* Subdivide in X. */
	    float xm = (x0 + x1) * 0.5;

	    if_debug1('|', "[|]dividing at x=%g\n", xm);
	    v[0] = xm;
	    v[1] = y0;
	    code = gs_function_evaluate(pfn, v, mid[0].paint.values);
	    if (code < 0)
		return code;
	    v[1] = y1;
	    code = gs_function_evaluate(pfn, v, mid[2].paint.values);
	    if (code < 0)
		return code;
	    rcc[0].paint = cc[0].paint;
	    rcc[1].paint = mid[0].paint;
	    rcc[2].paint = cc[2].paint;
	    rcc[3].paint = mid[1].paint;
	    code = Fb_fill_region(pfs, rcc, x0, y0, xm, y1);
	    cc[0].paint = mid[0].paint;
	    cc[2].paint = mid[1].paint;
	    x0 = xm;
	}
	if (code < 0)
	    return code;
    }
    goto top;
}

int
gs_shading_Fb_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			     gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Fb_t * const psh = (const gs_shading_Fb_t *)psh0;
    gs_matrix save_ctm;
    int xi, yi, code;
    float x[2], y[2];
    Fb_fill_state_t state;
    gs_client_color cc[4];

    shade_init_fill_state((shading_fill_state_t *) & state, psh0, dev, pis);
    state.psh = psh;
    /****** HACK FOR FIXED-POINT MATRIX MULTIPLY ******/
    gs_currentmatrix((gs_state *) pis, &save_ctm);
    gs_concat((gs_state *) pis, &psh->params.Matrix);
    state.ptm = pis->ctm;
    gs_setmatrix((gs_state *) pis, &save_ctm);
    state.orthogonal = is_xxyy(&state.ptm) || is_xyyx(&state.ptm);
    /* Compute the parameter X and Y ranges. */
    {
	gs_rect pbox;

	gs_bbox_transform_inverse(rect, &psh->params.Matrix, &pbox);
	x[0] = max(pbox.p.x, psh->params.Domain[0]);
	x[1] = min(pbox.q.x, psh->params.Domain[1]);
	y[0] = max(pbox.p.y, psh->params.Domain[2]);
	y[1] = min(pbox.q.y, psh->params.Domain[3]);
    }
    for (xi = 0; xi < 2; ++xi)
	for (yi = 0; yi < 2; ++yi) {
	    float v[2];

	    v[0] = x[xi], v[1] = y[yi];
	    gs_function_evaluate(psh->params.Function, v,
				 cc[yi * 2 + xi].paint.values);
	}
    code = Fb_fill_region(&state, cc, x[0], y[0], x[1], y[1]);
    return code;
}

/* ---------------- Axial shading ---------------- */

typedef struct A_fill_state_s {
    shading_fill_state_common;
    const gs_shading_A_t *psh;
    gs_rect rect;
    gs_point delta;
    double length, dd;
} A_fill_state_t;

/* Note t0 and t1 vary over [0..1], not the Domain. */
private int
A_fill_region(const A_fill_state_t * pfs, gs_client_color cc[2],
	      floatp t0, floatp t1)
{
    const gs_shading_A_t * const psh = pfs->psh;

top:
    if (!shade_colors2_converge(cc, (const shading_fill_state_t *)pfs)) {
	/*
	 * The colors don't converge.  Is the stripe less than 1 pixel wide?
	 */
	if (pfs->length * (t1 - t0) > 1)
	    goto recur;
    }
    /* Fill the region with the color. */
    {
	gx_device_color dev_color;
	const gs_color_space *pcs = psh->params.ColorSpace;
	gs_imager_state *pis = pfs->pis;
	double
	    x0 = psh->params.Coords[0] + pfs->delta.x * t0,
	    y0 = psh->params.Coords[1] + pfs->delta.y * t0;
	double
	    x1 = psh->params.Coords[0] + pfs->delta.x * t1,
	    y1 = psh->params.Coords[1] + pfs->delta.y * t1;
	gs_fixed_point pts[4];
	int code;

	(*pcs->type->restrict_color)(&cc[0], pcs);
	(*pcs->type->remap_color)(&cc[0], pcs, &dev_color, pis,
				  pfs->dev, gs_color_select_texture);
	if (x0 == x1) {
	    /* Stripe is horizontal. */
	    x0 = pfs->rect.p.x;
	    x1 = pfs->rect.q.x;
	} else if (y0 == y1) {
	    /* Stripe is vertical. */
	    y0 = pfs->rect.p.y;
	    y1 = pfs->rect.q.y;
	} else {
	    /*
	     * Stripe is neither horizontal nor vertical.
	     * Extend it to the edges of the rectangle.
	     */
	    gx_path *ppath = gx_path_alloc(pis->memory, "A_fill");
	    double dist = max(pfs->rect.q.x - pfs->rect.p.x,
			      pfs->rect.q.y - pfs->rect.p.y);
	    double denom = hypot(pfs->delta.x, pfs->delta.y);
	    double dx = dist * pfs->delta.y / denom,
		dy = -dist * pfs->delta.x / denom;

	    if_debug6('|', "[|]p0=(%g,%g), p1=(%g,%g), dxy=(%g,%g)\n",
		      x0, y0, x1, y1, dx, dy);
	    gs_point_transform2fixed(&pis->ctm, x0 - dx, y0 - dy, &pts[0]);
	    gs_point_transform2fixed(&pis->ctm, x0 + dx, y0 + dy, &pts[1]);
	    gs_point_transform2fixed(&pis->ctm, x1 + dx, y1 + dy, &pts[2]);
	    gs_point_transform2fixed(&pis->ctm, x1 - dx, y1 - dy, &pts[3]);
	    gx_path_add_point(ppath, pts[0].x, pts[0].y);
	    gx_path_add_lines(ppath, pts + 1, 3);
	    code = shade_fill_path((const shading_fill_state_t *)pfs,
				   ppath, &dev_color);
	    gx_path_free(ppath, "A_fill");
	    return code;
	}
	/* Stripe is horizontal or vertical. */
	gs_point_transform2fixed(&pis->ctm, x0, y0, &pts[0]);
	gs_point_transform2fixed(&pis->ctm, x1, y1, &pts[1]);
	return
	    shade_fill_device_rectangle((const shading_fill_state_t *)pfs,
					&pts[0], &pts[1], &dev_color);
    }

    /*
     * No luck.  Subdivide the interval and recur.
     */
recur:
    {
	gs_client_color ccm, rcc[2];
	gs_function_t *pfn = psh->params.Function;
	float tm = (t0 + t1) * 0.5;
	float dm = tm * pfs->dd + psh->params.Domain[0];

	gs_function_evaluate(pfn, &dm, ccm.paint.values);
	rcc[0].paint = cc[0].paint;
	rcc[1].paint = ccm.paint;
	A_fill_region(pfs, rcc, t0, tm);
	cc[0].paint = ccm.paint;
	t0 = tm;
	goto top;
    }
}

int
gs_shading_A_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_A_t *const psh = (const gs_shading_A_t *)psh0;
    A_fill_state_t state;
    gs_client_color cc[2];
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1], dd = d1 - d0;
    float t[2];
    gs_point dist;
    int i;

    shade_init_fill_state((shading_fill_state_t *) & state, psh0, dev, pis);
    state.psh = psh;
    state.rect = *rect;
    /* Compute the parameter range. */
    t[0] = d0;
    t[1] = d1;
/****** INTERSECT Domain WITH rect ******/
    for (i = 0; i < 2; ++i)
	gs_function_evaluate(psh->params.Function, &t[i],
			     cc[i].paint.values);
    state.delta.x = psh->params.Coords[2] - psh->params.Coords[0];
    state.delta.y = psh->params.Coords[3] - psh->params.Coords[1];
    gs_distance_transform(state.delta.x, state.delta.y, &ctm_only(pis),
			  &dist);
    state.length = hypot(dist.x, dist.y);	/* device space line length */
    state.dd = dd;
/****** DOESN'T HANDLE Extend ******/
    return A_fill_region(&state, cc, (t[0] - d0) / dd, (t[1] - d0) / dd);
}

/* ---------------- Radial shading ---------------- */

typedef struct R_fill_state_s {
    shading_fill_state_common;
    const gs_shading_R_t *psh;
    gs_rect rect;
    gs_point delta;
    double dr, width, dd;
} R_fill_state_t;

/* Note t0 and t1 vary over [0..1], not the Domain. */
private int
R_fill_region(const R_fill_state_t * pfs, gs_client_color cc[2],
	      floatp t0, floatp t1)
{
    const gs_shading_R_t * const psh = pfs->psh;

top:
    if (!shade_colors2_converge(cc, (const shading_fill_state_t *)pfs)) {
	/*
	 * The colors don't converge.  Is the annulus less than 1 pixel wide?
	 */
	if (pfs->width * (t1 - t0) > 1)
	    goto recur;
	/* We could do the 1-pixel case a lot faster! */
    }
    /* Fill the region with the color. */
    {
	gx_device_color dev_color;
	const gs_color_space *pcs = psh->params.ColorSpace;
	gs_imager_state *pis = pfs->pis;
	double
	    x0 = psh->params.Coords[0] + pfs->delta.x * t0,
	    y0 = psh->params.Coords[1] + pfs->delta.y * t0,
	    r0 = psh->params.Coords[2] + pfs->dr * t0;
	double
	    x1 = psh->params.Coords[0] + pfs->delta.x * t1,
	    y1 = psh->params.Coords[1] + pfs->delta.y * t1,
	    r1 = psh->params.Coords[2] + pfs->dr * t1;
	gx_path *ppath = gx_path_alloc(pis->memory, "R_fill");
	int code;

	(*pcs->type->restrict_color)(&cc[0], pcs);
	(*pcs->type->remap_color)(&cc[0], pcs, &dev_color, pis,
				  pfs->dev, gs_color_select_texture);
	if ((code = gs_imager_arc_add(ppath, pis, false, x0, y0, r0,
				      0.0, 360.0, false)) >= 0 &&
	    (code = gs_imager_arc_add(ppath, pis, true, x1, y1, r1,
				      0.0, 360.0, false)) >= 0
	    ) {
	    code = shade_fill_path((const shading_fill_state_t *)pfs,
				   ppath, &dev_color);
	}
	gx_path_free(ppath, "R_fill");
	return code;
    }

    /*
     * No luck.  Subdivide the interval and recur.
     */
recur:
    {
	gs_client_color ccm, rcc[2];
	gs_function_t *pfn = psh->params.Function;
	float tm = (t0 + t1) * 0.5;
	float dm = tm * pfs->dd + psh->params.Domain[0];

	gs_function_evaluate(pfn, &dm, ccm.paint.values);
	rcc[0].paint = cc[0].paint;
	rcc[1].paint = ccm.paint;
	R_fill_region(pfs, rcc, t0, tm);
	cc[0].paint = ccm.paint;
	t0 = tm;
	goto top;
    }
}

int
gs_shading_R_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			    gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_R_t *const psh = (const gs_shading_R_t *)psh0;
    R_fill_state_t state;
    gs_client_color cc[2];
    float d0 = psh->params.Domain[0], d1 = psh->params.Domain[1], dd = d1 - d0;
    float t[2];
    int i;

    shade_init_fill_state((shading_fill_state_t *) & state, psh0, dev, pis);
    state.psh = psh;
    state.rect = *rect;
    /* Compute the parameter range. */
    t[0] = d0;
    t[1] = d1;
/****** INTERSECT Domain WITH rect ******/
    for (i = 0; i < 2; ++i)
	gs_function_evaluate(psh->params.Function, &t[i],
			     cc[i].paint.values);
    state.delta.x = psh->params.Coords[3] - psh->params.Coords[0];
    state.delta.y = psh->params.Coords[4] - psh->params.Coords[1];
    state.dr = psh->params.Coords[5] - psh->params.Coords[2];
    /*
     * Compute the annulus width in its thickest direction.  This is
     * only used for a conservative check, so it can be pretty crude
     * (and it is!).
     */
    state.width =
	(fabs(pis->ctm.xx) + fabs(pis->ctm.xy) + fabs(pis->ctm.yx) +
	 fabs(pis->ctm.yy)) * fabs(state.dr);
    state.dd = dd;
/****** DOESN'T HANDLE Extend ******/
    return R_fill_region(&state, cc, (t[0] - d0) / dd, (t[1] - d0) / dd);
}
