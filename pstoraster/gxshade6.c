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

/*$Id: gxshade6.c,v 1.1 2000/03/08 23:15:05 mike Exp $ */
/* Rendering for Coons and tensor patch shadings */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gxcspace.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "gzpath.h"

/* ================ Utilities ================ */

/* Define one segment (vertex and next control points) of a curve. */
typedef struct patch_curve_s {
    mesh_vertex_t vertex;
    gs_fixed_point control[2];
} patch_curve_t;

/* Get colors for patch vertices. */
private int
shade_next_colors(shade_coord_stream_t * cs, patch_curve_t * curves,
		  int num_vertices)
{
    int i, code = 0;

    for (i = 0; i < num_vertices && code >= 0; ++i)
	code = shade_next_color(cs, curves[i].vertex.cc);
    return code;
}

/* Get a Bezier or tensor patch element. */
private int
shade_next_curve(shade_coord_stream_t * cs, patch_curve_t * curve)
{
    int code = shade_next_coords(cs, &curve->vertex.p, 1);

    if (code >= 0)
	code = shade_next_coords(cs, curve->control,
				 countof(curve->control));
    return code;
}

/* Define a color to be used in curve rendering. */
/* This may be a real client color, or a parametric function argument. */
typedef struct patch_color_s {
    float t;			/* parametric value */
    gs_client_color cc;
} patch_color_t;

/*
 * Parse the next patch out of the input stream.  Return 1 if done,
 * 0 if patch, <0 on error.
 */
private int
shade_next_patch(shade_coord_stream_t * cs, int BitsPerFlag,
patch_curve_t curve[4], gs_fixed_point interior[4] /* 0 for Coons patch */ )
{
    int flag = shade_next_flag(cs, BitsPerFlag);
    int num_colors, code;

    if (flag < 0)
	return 1;		/* no more data */
    switch (flag & 3) {
	default:
	    return_error(gs_error_rangecheck);	/* not possible */
	case 0:
	    if ((code = shade_next_curve(cs, &curve[0])) < 0 ||
		(code = shade_next_coords(cs, &curve[1].vertex.p, 1)) < 0
		)
		return code;
	    num_colors = 4;
	    goto vx;
	case 1:
	    curve[0] = curve[1], curve[1].vertex = curve[2].vertex;
	    goto v3;
	case 2:
	    curve[0] = curve[2], curve[1].vertex = curve[3].vertex;
	    goto v3;
	case 3:
	    curve[1].vertex = curve[0].vertex, curve[0] = curve[3];
v3:	    num_colors = 2;
vx:	    if ((code = shade_next_coords(cs, curve[1].control, 2)) < 0 ||
		(code = shade_next_curve(cs, &curve[2])) < 0 ||
		(code = shade_next_curve(cs, &curve[3])) < 0 ||
		(interior != 0 &&
		 (code = shade_next_coords(cs, interior, 4)) < 0) ||
		(code = shade_next_colors(cs, &curve[4 - num_colors],
					  num_colors)) < 0
		)
		return code;
    }
    return 0;
}

/* Define the common state for rendering Coons and tensor patches. */
typedef struct patch_fill_state_s {
    mesh_fill_state_common;
    const gs_function_t *Function;
} patch_fill_state_t;

/* Calculate the interpolated color at a given point. */
/* Note that we must do this twice for bilinear interpolation. */
private void
patch_interpolate_color(patch_color_t * ppc, const patch_color_t * ppc0,
       const patch_color_t * ppc1, const patch_fill_state_t * pfs, floatp t)
{
    if (pfs->Function)
	ppc->t = ppc0->t + t * (ppc1->t - ppc0->t);
    else {
	int ci;

	for (ci = pfs->num_components - 1; ci >= 0; --ci)
	    ppc->cc.paint.values[ci] =
		ppc0->cc.paint.values[ci] +
		t * (ppc1->cc.paint.values[ci] - ppc0->cc.paint.values[ci]);
    }
}

/* Resolve a patch color using the Function if necessary. */
private void
patch_resolve_color(patch_color_t * ppc, const patch_fill_state_t * pfs)
{
    if (pfs->Function)
	gs_function_evaluate(pfs->Function, &ppc->t, ppc->cc.paint.values);
}

/* ================ Specific shadings ================ */

/*
 * The curves are stored in a clockwise or counter-clockwise order that maps
 * to the patch definition in a non-intuitive way:
 */
/* The starting points of the curves: */
#define C1START 0
#define D1START 0
#define C2START 3
#define D2START 1
/* The control points of the curves (x means reversed order): */
#define C1CTRL 0
#define D1XCTRL 3
#define C2XCTRL 2
#define D2CTRL 1
/* The end points of the curves: */
#define C1END 1
#define D1END 3
#define C2END 2
#define D2END 2

/* ---------------- Common code ---------------- */

/* Evaluate a curve at a given point. */
private void
curve_eval(gs_fixed_point * pt, const gs_fixed_point * p0,
	   const gs_fixed_point * p1, const gs_fixed_point * p2,
	   const gs_fixed_point * p3, floatp t)
{
    fixed a, b, c, d;
    fixed t01, t12;

    d = p0->x;
    curve_points_to_coefficients(d, p1->x, p2->x, p3->x,
				 a, b, c, t01, t12);
    pt->x = (fixed) (((a * t + b) * t + c) * t + d);
    d = p0->y;
    curve_points_to_coefficients(d, p1->y, p2->y, p3->y,
				 a, b, c, t01, t12);
    pt->y = (fixed) (((a * t + b) * t + c) * t + d);
    if_debug3('2', "[2]t=%g => (%g,%g)\n", t, fixed2float(pt->x),
	      fixed2float(pt->y));
}

/*
 * Merge two arrays of splits, sorted in increasing order.
 * Return the number of entries in the result, which might be less than
 * n1 + n2 (if an a1 entry is equal to an a2 entry).
 * a1 or a2 may overlap out as long as a1 - out >= n2 or a2 - out >= n1
 * respectively.
 */
private int
merge_splits(double *out, const double *a1, int n1, const double *a2, int n2)
{
    double *p = out;
    int i1 = 0, i2 = 0;

    /*
     * We would like to write the body of the loop as an assignement
     * with a conditional expression on the right, but gcc 2.7.2.3
     * generates incorrect code if we do this.
     */
    while (i1 < n1 || i2 < n2)
	if (i1 == n1)
	    *p++ = a2[i2++];
	else if (i2 == n2 || a1[i1] < a2[i2])
	    *p++ = a1[i1++];
	else if (a1[i1] > a2[i2])
	    *p++ = a2[i2++];
	else
	    i1++, *p++ = a2[i2++];
    return p - out;
}

/* Split a curve in both X and Y.  Return the number of split points. */
private int
split_xy(double out[4], const patch_curve_t * curve, const gs_fixed_point * p3)
{
    double tx[2], ty[2];

    return merge_splits(out, tx,
			gx_curve_monotonic_points(curve->vertex.p.x,
						  curve->control[0].x,
						  curve->control[1].x,
						  p3->x, tx),
			ty,
			gx_curve_monotonic_points(curve->vertex.p.y,
						  curve->control[0].y,
						  curve->control[1].y,
						  p3->y, ty));
}

/*
 * Compute the joint split points of 2 curves.
 * Return the number of split points.
 */
private int
split2_xy(double out[8], const patch_curve_t * curve1,
	  const gs_fixed_point * p31, const patch_curve_t * curve2,
	  const gs_fixed_point * p32)
{
    double t1[4], t2[4];

    return merge_splits(out, t1, split_xy(t1, curve1, p31),
			t2, split_xy(t2, curve2, p32));
}

private int
patch_fill(const patch_fill_state_t * pfs, const patch_curve_t curve[4],
	   const gs_fixed_point interior[4],
	   void (*transform) (P5(gs_fixed_point *, const patch_curve_t[4],
				 const gs_fixed_point[4], floatp, floatp)))
{	/*
	 * The specification says the output must appear to be produced in
	 * order of increasing values of v, and for equal v, in order of
	 * increasing u.  However, all we actually have to do is follow this
	 * order with respect to sub-patches that might overlap, which can
	 * only occur as a result of non-monotonic curves; we can render
	 * each monotonic sub-patch in any order we want.  Therefore, we
	 * begin by breaking up the patch into pieces that are monotonic
	 * with respect to all 4 edges.  Since each edge has no more than
	 * 2 X and 2 Y split points (for a total of 4), taking both edges
	 * together we have a maximum of 8 split points for each axis.
	 *
	 * The current documentation doesn't say how the 4 curves
	 * correspond to the 'u' or 'v' edges.  Pending clarification from
	 * Adobe, we assume the 1st and 3rd are the 'u' edges and the
	 * 2nd and 4th are the 'v' edges.
	 ****** CHECK AGAINST UPDATED DOC ******
	 */
    double u[9], v[9];
    int nu = split2_xy(u, &curve[0], &curve[1].vertex.p,
		       &curve[2], &curve[3].vertex.p);
    int nv = split2_xy(v, &curve[1], &curve[2].vertex.p,
		       &curve[3], &curve[0].vertex.p);
    int iu, iv, ju, jv, ku, kv;
    double du, dv;
    double v0, v1, vn, u0, u1, un;
    patch_color_t c0, c1, c2, c3;
    /*
     * At some future time, we should set check = false if the curves
     * fall entirely within the bounding rectangle.  (Only a small
     * performance optimization, to avoid making this check for each
     * triangle.)
     */
    bool check = true;

#ifdef DEBUG
    if (gs_debug_c('2')) {
	int k;

	dlputs("[2]patch curves:\n");
	for (k = 0; k < 4; ++k)
	    dprintf6("        (%g,%g) (%g,%g)(%g,%g)\n",
		     fixed2float(curve[k].vertex.p.x),
		     fixed2float(curve[k].vertex.p.y),
		     fixed2float(curve[k].control[0].x),
		     fixed2float(curve[k].control[0].y),
		     fixed2float(curve[k].control[1].x),
		     fixed2float(curve[k].control[1].y));
    }
#endif
    /* Add boundary values to simplify the iteration. */
    u[nu] = 1;
    v[nv] = 1;

    /*
     * We're going to fill the curves by flattening them and then filling
     * the resulting triangles.  Start by computing the number of
     * segments required for flattening each side of the patch.
     */
    {
	fixed flatness = float2fixed(pfs->pis->flatness);
	int i;
	int log2_k[4];

	for (i = 0; i < 4; ++i) {
	    curve_segment cseg;

	    cseg.p1 = curve[i].control[0];
	    cseg.p2 = curve[i].control[1];
	    cseg.pt = curve[(i + 1) & 3].vertex.p;
	    log2_k[i] =
		gx_curve_log2_samples(curve[i].vertex.p.x, curve[i].vertex.p.y,
				      &cseg, flatness);
	}
	ku = 1 << max(log2_k[0], log2_k[2]);
	kv = 1 << max(log2_k[1], log2_k[3]);
    }
    /*
     * Since ku and kv are powers of 2, and since log2(k) is surely less
     * than the number of bits in the mantissa of a double, 1/k ...
     * (k-1)/k can all be represented exactly as doubles.
     */
    du = 1.0 / ku;
    dv = 1.0 / kv;

    /* Precompute the colors at the corners. */

#define PATCH_SET_COLOR(c, v)\
  if ( pfs->Function ) c.t = v.cc[0];\
  else memcpy(c.cc.paint.values, v.cc, sizeof(c.cc.paint.values))

	PATCH_SET_COLOR(c0, curve[0].vertex);
	PATCH_SET_COLOR(c1, curve[1].vertex);
	PATCH_SET_COLOR(c2, curve[2].vertex);
	PATCH_SET_COLOR(c3, curve[3].vertex);

#undef PATCH_SET_COLOR

    /* Now iterate over the sub-patches. */
    for (iv = 0, jv = 0, v0 = 0, v1 = vn = dv; jv < kv; v0 = v1, v1 = vn) {
	patch_color_t cv[4];

	/* Subdivide the interval if it crosses a split point. */

#define CHECK_SPLIT(ix, jx, x1, xn, dx, ax)\
  if (x1 > ax[ix])\
      x1 = ax[ix++];\
  else {\
      xn += dx;\
      jx++;\
      if (x1 == ax[ix])\
	  ix++;\
  }

	CHECK_SPLIT(iv, jv, v1, vn, dv, v);

	patch_interpolate_color(&cv[0], &c0, &c3, pfs, v0);
	patch_interpolate_color(&cv[1], &c0, &c3, pfs, v1);
	patch_interpolate_color(&cv[2], &c1, &c2, pfs, v0);
	patch_interpolate_color(&cv[3], &c1, &c2, pfs, v1);

	for (iu = 0, ju = 0, u0 = 0, u1 = un = du; ju < ku; u0 = u1, u1 = un) {
	    patch_color_t cu[4];
	    int code;

	    CHECK_SPLIT(iu, ju, u1, un, du, u);

#undef CHECK_SPLIT

	    patch_interpolate_color(&cu[0], &cv[0], &cv[2], pfs, u0);
	    patch_resolve_color(&cu[0], pfs);
	    patch_interpolate_color(&cu[1], &cv[0], &cv[2], pfs, u1);
	    patch_resolve_color(&cu[1], pfs);
	    patch_interpolate_color(&cu[2], &cv[1], &cv[3], pfs, u1);
	    patch_resolve_color(&cu[2], pfs);
	    patch_interpolate_color(&cu[3], &cv[1], &cv[3], pfs, u0);
	    patch_resolve_color(&cu[3], pfs);
	    if_debug6('2', "[2]u[%d]=(%g,%g), v[%d]=(%g,%g)\n",
		      iu, u0, u1, iv, v0, v1);

	    /* Fill the sub-patch given by ((u0,v0),(u1,v1)). */
	    {
		mesh_vertex_t mv[4];

		(*transform)(&mv[0].p, curve, interior, u0, v0);
		(*transform)(&mv[1].p, curve, interior, u1, v0);
		(*transform)(&mv[2].p, curve, interior, u1, v1);
		(*transform)(&mv[3].p, curve, interior, u0, v1);
		memcpy(&mv[0].cc, cu[0].cc.paint.values, sizeof(mv[0].cc));
		memcpy(&mv[1].cc, cu[1].cc.paint.values, sizeof(mv[1].cc));
		memcpy(&mv[2].cc, cu[2].cc.paint.values, sizeof(mv[2].cc));
		memcpy(&mv[3].cc, cu[3].cc.paint.values, sizeof(mv[3].cc));
		code = mesh_fill_triangle((const mesh_fill_state_t *)pfs,
					  &mv[0], &mv[1], &mv[2], check);
		if (code < 0)
		    return code;
		code = mesh_fill_triangle((const mesh_fill_state_t *)pfs,
					  &mv[2], &mv[3], &mv[0], check);
		if (code < 0)
		    return code;
	    }
	}
    }
    return 0;
}

/* ---------------- Coons patch shading ---------------- */

/* Calculate the device-space coordinate corresponding to (u,v). */
private void
Cp_transform(gs_fixed_point * pt, const patch_curve_t curve[4],
	     const gs_fixed_point ignore_interior[4], floatp u, floatp v)
{
    double co_u = 1.0 - u, co_v = 1.0 - v;
    gs_fixed_point c1u, d1v, c2u, d2v;

    curve_eval(&c1u, &curve[C1START].vertex.p,
	       &curve[C1CTRL].control[0], &curve[C1CTRL].control[1],
	       &curve[C1END].vertex.p, u);
    curve_eval(&d1v, &curve[D1START].vertex.p,
	       &curve[D1XCTRL].control[1], &curve[D1XCTRL].control[0],
	       &curve[D1END].vertex.p, v);
    curve_eval(&c2u, &curve[C2START].vertex.p,
	       &curve[C2XCTRL].control[1], &curve[C2XCTRL].control[0],
	       &curve[C2END].vertex.p, u);
    curve_eval(&d2v, &curve[D2START].vertex.p,
	       &curve[D2CTRL].control[0], &curve[D2CTRL].control[1],
	       &curve[D2END].vertex.p, v);
#define COMPUTE_COORD(xy)\
    pt->xy = (fixed)\
	((co_v * c1u.xy + v * c2u.xy) + (co_u * d1v.xy + u * d2v.xy) -\
	 (co_v * (co_u * curve[C1START].vertex.p.xy +\
		  u * curve[C1END].vertex.p.xy) +\
	  v * (co_u * curve[C2START].vertex.p.xy +\
	       u * curve[C2END].vertex.p.xy)))
    COMPUTE_COORD(x);
    COMPUTE_COORD(y);
#undef COMPUTE_COORD
    if_debug4('2', "[2](u=%g,v=%g) => (%g,%g)\n",
	      u, v, fixed2float(pt->x), fixed2float(pt->y));
}

int
gs_shading_Cp_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			     gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Cp_t * const psh = (const gs_shading_Cp_t *)psh0;
    patch_fill_state_t state;
    shade_coord_stream_t cs;
    patch_curve_t curve[4];
    int code;

    mesh_init_fill_state((mesh_fill_state_t *) & state,
			 (const gs_shading_mesh_t *)psh0, rect, dev, pis);
    state.Function = psh->params.Function;
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((code = shade_next_patch(&cs, psh->params.BitsPerFlag,
				    curve, NULL)) == 0 &&
	   (code = patch_fill(&state, curve, NULL, Cp_transform)) >= 0
	)
	DO_NOTHING;
    return min(code, 0);
}

/* ---------------- Tensor product patch shading ---------------- */

/* Calculate the device-space coordinate corresponding to (u,v). */
private void
Tpp_transform(gs_fixed_point * pt, const patch_curve_t curve[4],
	      const gs_fixed_point interior[4], floatp u, floatp v)
{
    double Bu[4], Bv[4];
    gs_fixed_point pts[4][4];
    int i, j;
    fixed x = 0, y = 0;

    /* Compute the Bernstein polynomials of u and v. */
    {
	double u2 = u * u, co_u = 1.0 - u, co_u2 = co_u * co_u;
	double v2 = v * v, co_v = 1.0 - v, co_v2 = co_v * co_v;

	Bu[0] = co_u * co_u2, Bu[1] = 3 * u * co_u2,
	    Bu[2] = 3 * u2 * co_u, Bu[3] = u * u2;
	Bv[0] = co_v * co_v2, Bv[1] = 3 * v * co_v2,
	    Bv[2] = 3 * v2 * co_v, Bv[3] = v * v2;
    }

    /* Arrange the points into an indexable order. */
    pts[0][0] = curve[0].vertex.p;
    pts[1][0] = curve[0].control[0];
    pts[2][0] = curve[0].control[1];
    pts[3][0] = curve[1].vertex.p;
    pts[3][1] = curve[1].control[0];
    pts[3][2] = curve[1].control[1];
    pts[3][3] = curve[2].vertex.p;
    pts[2][3] = curve[2].control[0];
    pts[1][3] = curve[2].control[1];
    pts[0][3] = curve[3].vertex.p;
    pts[0][2] = curve[3].control[0];
    pts[0][1] = curve[3].control[1];
    pts[1][1] = interior[0];
    pts[2][1] = interior[1];
    pts[2][2] = interior[2];
    pts[1][2] = interior[3];

    /* Now compute the actual point. */
    for (i = 0; i < 4; ++i)
	for (j = 0; j < 4; ++j) {
	    double coeff = Bu[i] * Bv[j];

	    x += pts[i][j].x * coeff, y += pts[i][j].y * coeff;
	}
    pt->x = x, pt->y = y;
}

int
gs_shading_Tpp_fill_rectangle(const gs_shading_t * psh0, const gs_rect * rect,
			      gx_device * dev, gs_imager_state * pis)
{
    const gs_shading_Tpp_t * const psh = (const gs_shading_Tpp_t *)psh0;
    patch_fill_state_t state;
    shade_coord_stream_t cs;
    patch_curve_t curve[4];
    gs_fixed_point interior[4];
    int code;

    mesh_init_fill_state((mesh_fill_state_t *) & state,
			 (const gs_shading_mesh_t *)psh0, rect, dev, pis);
    state.Function = psh->params.Function;
    shade_next_init(&cs, (const gs_shading_mesh_params_t *)&psh->params,
		    pis);
    while ((code = shade_next_patch(&cs, psh->params.BitsPerFlag,
				    curve, interior)) == 0 &&
	   (code = patch_fill(&state, curve, interior, Tpp_transform)) >= 0
	)
	DO_NOTHING;
    return min(code, 0);
}
