/* Copyright (C) 1992, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxpcopy.c,v 1.2 2000/03/08 23:15:04 mike Exp $ */
/* Path copying and flattening */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gconfigv.h"		/* for USE_FPU */
#include "gxfixed.h"
#include "gxfarith.h"
#include "gzpath.h"

/* Forward declarations */
private void adjust_point_to_tangent(P3(segment *, const segment *,
					const gs_fixed_point *));
private int monotonize_internal(P2(gx_path *, const curve_segment *));

/* Copy a path, optionally flattening or monotonizing it. */
/* If the copy fails, free the new path. */
int
gx_path_copy_reducing(const gx_path * ppath_old, gx_path * ppath,
		      fixed fixed_flatness, gx_path_copy_options options)
{
    const segment *pseg;

    /*
     * Since we're going to be adding to the path, unshare it
     * before we start.
     */
    int code = gx_path_unshare(ppath);

    if (code < 0)
	return code;
#ifdef DEBUG
    if (gs_debug_c('P'))
	gx_dump_path(ppath_old, "before reducing");
#endif
    pseg = (const segment *)(ppath_old->first_subpath);
    while (pseg) {
	switch (pseg->type) {
	    case s_start:
		code = gx_path_add_point(ppath,
					 pseg->pt.x, pseg->pt.y);
		break;
	    case s_curve:
		{
		    const curve_segment *pc = (const curve_segment *)pseg;

		    if (fixed_flatness == max_fixed) {	/* don't flatten */
			if (options & pco_monotonize)
			    code = monotonize_internal(ppath, pc);
			else
			    code = gx_path_add_curve_notes(ppath,
				     pc->p1.x, pc->p1.y, pc->p2.x, pc->p2.y,
					   pc->pt.x, pc->pt.y, pseg->notes);
		    } else {
			fixed x0 = ppath->position.x;
			fixed y0 = ppath->position.y;
			int k = gx_curve_log2_samples(x0, y0, pc,
						      fixed_flatness);
			segment_notes notes = pseg->notes;
			segment *start;
			curve_segment cseg;

			if (options & pco_accurate) {	/* Add an extra line, which will become */
			    /* the tangent segment. */
			    code = gx_path_add_line_notes(ppath, x0, y0,
							  notes);
			    if (code < 0)
				break;
			    start = ppath->current_subpath->last;
			    notes |= sn_not_first;
			}
			cseg = *pc;
			code = gx_flatten_sample(ppath, k, &cseg, notes);
			if (options & pco_accurate) {	/*
							 * Adjust the first and last segments so that
							 * they line up with the tangents.
							 */
			    segment *end = ppath->current_subpath->last;

			    if (code < 0 ||
				(code = gx_path_add_line_notes(ppath,
							  ppath->position.x,
							  ppath->position.y,
					    pseg->notes | sn_not_first)) < 0
				)
				break;
			    adjust_point_to_tangent(start, start->next,
						    &pc->p1);
			    adjust_point_to_tangent(end, end->prev,
						    &pc->p2);
			}
		    }
		    break;
		}
	    case s_line:
		code = gx_path_add_line_notes(ppath,
				       pseg->pt.x, pseg->pt.y, pseg->notes);
		break;
	    case s_line_close:
		code = gx_path_close_subpath(ppath);
		break;
	    default:		/* can't happen */
		code = gs_note_error(gs_error_unregistered);
	}
	if (code < 0) {
	    gx_path_new(ppath);
	    return code;
	}
	pseg = pseg->next;
    }
    if (path_last_is_moveto(ppath_old))
	gx_path_add_point(ppath, ppath_old->position.x,
			  ppath_old->position.y);
#ifdef DEBUG
    if (gs_debug_c('P'))
	gx_dump_path(ppath, "after reducing");
#endif
    return 0;
}

/*
 * Adjust one end of a line (the first or last line of a flattened curve)
 * so it falls on the curve tangent.  The closest point on the line from
 * (0,0) to (C,D) to a point (U,V) -- i.e., the point on the line at which
 * a perpendicular line from the point intersects it -- is given by
 *      T = (C*U + D*V) / (C^2 + D^2)
 *      (X,Y) = (C*T,D*T)
 * However, any smaller value of T will also work: the one we actually
 * use is 0.25 * the value we just derived.  We must check that
 * numerical instabilities don't lead to a negative value of T.
 */
private void
adjust_point_to_tangent(segment * pseg, const segment * next,
			const gs_fixed_point * p1)
{
    const fixed x0 = pseg->pt.x, y0 = pseg->pt.y;
    const fixed fC = p1->x - x0, fD = p1->y - y0;

    /*
     * By far the commonest case is that the end of the curve is
     * horizontal or vertical.  Check for this specially, because
     * we can handle it with far less work (and no floating point).
     */
    if (fC == 0) {
	/* Vertical tangent. */
	const fixed DT = arith_rshift(next->pt.y - y0, 2);

	if (fD == 0)
	    return;		/* anomalous case */
	if_debug1('2', "[2]adjusting vertical: DT = %g\n",
		  fixed2float(DT));
	if (DT > 0)
	    pseg->pt.y = DT + y0;
    } else if (fD == 0) {
	/* Horizontal tangent. */
	const fixed CT = arith_rshift(next->pt.x - x0, 2);

	if_debug1('2', "[2]adjusting horizontal: CT = %g\n",
		  fixed2float(CT));
	if (CT > 0)
	    pseg->pt.x = CT + x0;
    } else {
	/* General case. */
	const double C = fC, D = fD;
	const double T = (C * (next->pt.x - x0) + D * (next->pt.y - y0)) /
	(C * C + D * D);

	if_debug3('2', "[2]adjusting: C = %g, D = %g, T = %g\n",
		  C, D, T);
	if (T > 0) {
	    pseg->pt.x = arith_rshift((fixed) (C * T), 2) + x0;
	    pseg->pt.y = arith_rshift((fixed) (D * T), 2) + y0;
	}
    }
}

/* ---------------- Curve flattening ---------------- */

#define x1 pc->p1.x
#define y1 pc->p1.y
#define x2 pc->p2.x
#define y2 pc->p2.y
#define x3 pc->pt.x
#define y3 pc->pt.y

#ifdef DEBUG
private void
dprint_curve(const char *str, fixed x0, fixed y0, const curve_segment * pc)
{
    dlprintf9("%s p0=(%g,%g) p1=(%g,%g) p2=(%g,%g) p3=(%g,%g)\n",
	      str, fixed2float(x0), fixed2float(y0),
	      fixed2float(pc->p1.x), fixed2float(pc->p1.y),
	      fixed2float(pc->p2.x), fixed2float(pc->p2.y),
	      fixed2float(pc->pt.x), fixed2float(pc->pt.y));
}
#endif

/* Initialize a cursor for rasterizing a monotonic curve. */
void
gx_curve_cursor_init(curve_cursor * prc, fixed x0, fixed y0,
		     const curve_segment * pc, int k)
{
    fixed v01, v12;
    int k2 = k + k, k3 = k2 + k;

#define bits_fit(v, n)\
  (any_abs(v) <= max_fixed >> (n))
/* The +2s are because of t3d and t2d, see below. */
#define coeffs_fit(a, b, c)\
  (k3 <= sizeof(fixed) * 8 - 3 &&\
   bits_fit(a, k3 + 2) && bits_fit(b, k2 + 2) && bits_fit(c, k + 1))

    prc->k = k;
    prc->p0.x = x0, prc->p0.y = y0;
    prc->pc = pc;
    /* Compute prc->a..c taking into account reversal of xy0/3 */
    /* in curve_x_at_y. */
    {
	fixed w0, w1, w2, w3;

	if (y0 < y3)
	    w0 = x0, w1 = x1, w2 = x2, w3 = x3;
	else
	    w0 = x3, w1 = x2, w2 = x1, w3 = x0;
	curve_points_to_coefficients(w0, w1, w2, w3,
				     prc->a, prc->b, prc->c, v01, v12);
    }
    prc->double_set = false;
    prc->fixed_limit =
	(coeffs_fit(prc->a, prc->b, prc->c) ? (1 << k) - 1 : -1);
    /* Initialize the cache. */
    prc->cache.ky0 = prc->cache.ky3 = y0;
    prc->cache.xl = x0;
    prc->cache.xd = 0;
}

/*
 * Determine the X value on a monotonic curve at a given Y value.  It turns
 * out that we use so few points on the curve that it's actually faster to
 * locate the desired point by recursive subdivision each time than to try
 * to keep a cursor that we move incrementally.  What's even more surprising
 * is that if floating point arithmetic is reasonably fast, it's faster to
 * compute the X value at the desired point explicitly than to do the
 * recursive subdivision on X as well as Y.
 */
#define SUBDIVIDE_X USE_FPU_FIXED
fixed
gx_curve_x_at_y(curve_cursor * prc, fixed y)
{
    fixed xl, xd;
    fixed yd, yrel;

    /* Check the cache before doing anything else. */
    if (y >= prc->cache.ky0 && y <= prc->cache.ky3) {
	yd = prc->cache.ky3 - prc->cache.ky0;
	yrel = y - prc->cache.ky0;
	xl = prc->cache.xl;
	xd = prc->cache.xd;
	goto done;
    } {
#define x0 prc->p0.x
#define y0 prc->p0.y
	const curve_segment *pc = prc->pc;

	/* Reduce case testing by ensuring y3 >= y0. */
	fixed cy0 = y0, cy1, cy2, cy3 = y3;
	fixed cx0;

#if SUBDIVIDE_X
	fixed cx1, cx2, cx3;

#else
	int t = 0;

#endif
	int k, i;

	if (cy0 > cy3)
	    cx0 = x3,
#if SUBDIVIDE_X
		cx1 = x2, cx2 = x1, cx3 = x0,
#endif
		cy0 = y3, cy1 = y2, cy2 = y1, cy3 = y0;
	else
	    cx0 = x0,
#if SUBDIVIDE_X
		cx1 = x1, cx2 = x2, cx3 = x3,
#endif
		cy1 = y1, cy2 = y2;
#define midpoint_fast(a,b)\
  arith_rshift_1((a) + (b) + 1)
	for (i = k = prc->k; i > 0; --i) {
	    fixed ym = midpoint_fast(cy1, cy2);
	    fixed yn = ym + arith_rshift(cy0 - cy1 - cy2 + cy3 + 4, 3);

#if SUBDIVIDE_X
	    fixed xm = midpoint_fast(cx1, cx2);
	    fixed xn = xm + arith_rshift(cx0 - cx1 - cx2 + cx3 + 4, 3);

#else
	    t <<= 1;
#endif

	    if (y < yn)
#if SUBDIVIDE_X
		cx1 = midpoint_fast(cx0, cx1),
		    cx2 = midpoint_fast(cx1, xm),
		    cx3 = xn,
#endif
		    cy1 = midpoint_fast(cy0, cy1),
		    cy2 = midpoint_fast(cy1, ym),
		    cy3 = yn;
	    else
#if SUBDIVIDE_X
		cx2 = midpoint_fast(cx2, cx3),
		    cx1 = midpoint_fast(xm, cx2),
		    cx0 = xn,
#else
		t++,
#endif
		    cy2 = midpoint_fast(cy2, cy3),
		    cy1 = midpoint_fast(ym, cy2),
		    cy0 = yn;
	}
#if SUBDIVIDE_X
	xl = cx0;
	xd = cx3 - cx0;
#else
	{
	    fixed a = prc->a, b = prc->b, c = prc->c;

/* We must use (1 << k) >> 1 instead of 1 << (k - 1) in case k == 0. */
#define compute_fixed(a, b, c)\
  arith_rshift(arith_rshift(arith_rshift(a * t3, k) + b * t2, k)\
	       + c * t + ((1 << k) >> 1), k)
#define compute_diff_fixed(a, b, c)\
  arith_rshift(arith_rshift(arith_rshift(a * t3d, k) + b * t2d, k)\
	       + c, k)

	    /* use multiply if possible */
#define np2(n) (1.0 / (1L << (n)))
	    static const double k_denom[11] =
	    {np2(0), np2(1), np2(2), np2(3), np2(4),
	     np2(5), np2(6), np2(7), np2(8), np2(9), np2(10)
	    };
	    static const double k2_denom[11] =
	    {np2(0), np2(2), np2(4), np2(6), np2(8),
	     np2(10), np2(12), np2(14), np2(16), np2(18), np2(20)
	    };
	    static const double k3_denom[11] =
	    {np2(0), np2(3), np2(6), np2(9), np2(12),
	     np2(15), np2(18), np2(21), np2(24), np2(27), np2(30)
	    };
	    double den1, den2;

#undef np2

#define setup_floating(da, db, dc, a, b, c)\
  (k >= countof(k_denom) ?\
   (den1 = ldexp(1.0, -k),\
    den2 = den1 * den1,\
    da = (den2 * den1) * a,\
    db = den2 * b,\
    dc = den1 * c) :\
   (da = k3_denom[k] * a,\
    db = k2_denom[k] * b,\
    dc = k_denom[k] * c))
#define compute_floating(da, db, dc)\
  ((fixed)(da * t3 + db * t2 + dc * t + 0.5))
#define compute_diff_floating(da, db, dc)\
  ((fixed)(da * t3d + db * t2d + dc))

	    if (t <= prc->fixed_limit) {	/* We can do everything in integer/fixed point. */
		int t2 = t * t, t3 = t2 * t;
		int t3d = (t2 + t) * 3 + 1, t2d = t + t + 1;

		xl = compute_fixed(a, b, c) + cx0;
		xd = compute_diff_fixed(a, b, c);
#ifdef DEBUG
		{
		    double fa, fb, fc;
		    fixed xlf, xdf;

		    setup_floating(fa, fb, fc, a, b, c);
		    xlf = compute_floating(fa, fb, fc) + cx0;
		    xdf = compute_diff_floating(fa, fb, fc);
		    if (any_abs(xlf - xl) > fixed_epsilon ||
			any_abs(xdf - xd) > fixed_epsilon
			)
			dlprintf9("Curve points differ: k=%d t=%d a,b,c=%g,%g,%g\n   xl,xd fixed=%g,%g floating=%g,%g\n",
				  k, t,
			     fixed2float(a), fixed2float(b), fixed2float(c),
				  fixed2float(xl), fixed2float(xd),
				  fixed2float(xlf), fixed2float(xdf));
/*xl = xlf, xd = xdf; */
		}
#endif
	    } else {		/*
				 * Either t3 (and maybe t2) won't fit in an int, or more
				 * likely the result of the multiplies won't fit.
				 */
#define fa prc->da
#define fb prc->db
#define fc prc->dc
		if (!prc->double_set) {
		    setup_floating(fa, fb, fc, a, b, c);
		    prc->double_set = true;
		}
		if (t < 1L << ((sizeof(long) * 8 - 1) / 3)) {	/*
								 * t3 (and maybe t2) might not fit in an int, but they
								 * will fit in a long.  If we have slow floating point,
								 * do the computation in double-precision fixed point,
								 * otherwise do it in fixed point.
								 */
		    long t2 = (long)t * t, t3 = t2 * t;
		    long t3d = (t2 + t) * 3 + 1, t2d = t + t + 1;

		    xl = compute_floating(fa, fb, fc) + cx0;
		    xd = compute_diff_floating(fa, fb, fc);
		} else {	/*
				 * t3 (and maybe t2) don't even fit in a long.
				 * Do the entire computation in floating point.
				 */
		    double t2 = (double)t * t, t3 = t2 * t;
		    double t3d = (t2 + t) * 3 + 1, t2d = t + t + 1;

		    xl = compute_floating(fa, fb, fc) + cx0;
		    xd = compute_diff_floating(fa, fb, fc);
		}
#undef fa
#undef fb
#undef fc
	    }
	}
#endif /* (!)SUBDIVIDE_X */

	/* Update the cache. */
	prc->cache.ky0 = cy0;
	prc->cache.ky3 = cy3;
	prc->cache.xl = xl;
	prc->cache.xd = xd;
	yd = cy3 - cy0;
	yrel = y - cy0;
#undef x0
#undef y0
    }
  done:
    /*
     * Now interpolate linearly between current and next.
     * We know that 0 <= yrel < yd.
     * It's unlikely but possible that cy0 = y = cy3:
     * handle this case specially.
     */
    if (yrel == 0)
	return xl;
    /*
     * Compute in fixed point if possible.
     */
#define half_fixed_bits ((fixed)1 << (sizeof(fixed) * 4))
    if (yrel < half_fixed_bits) {
	if (xd >= 0) {
	    if (xd < half_fixed_bits)
		return (ufixed) xd *(ufixed) yrel / (ufixed) yd + xl;
	} else {
	    if (xd > -half_fixed_bits)
		return -(fixed) ((ufixed) (-xd) * (ufixed) yrel / (ufixed) yd) + xl;
	}
    }
#undef half_fixed_bits
    return fixed_mult_quo(xd, yrel, yd) + xl;
}

#undef x1
#undef y1
#undef x2
#undef y2
#undef x3
#undef y3

/* ---------------- Monotonic curves ---------------- */

/* Test whether a path is free of non-monotonic curves. */
bool
gx_path_is_monotonic(const gx_path * ppath)
{
    const segment *pseg = (const segment *)(ppath->first_subpath);
    gs_fixed_point pt0;

    while (pseg) {
	switch (pseg->type) {
	    case s_start:
		{
		    const subpath *psub = (const subpath *)pseg;

		    /* Skip subpaths without curves. */
		    if (!psub->curve_count)
			pseg = psub->last;
		}
		break;
	    case s_curve:
		{
		    const curve_segment *pc = (const curve_segment *)pseg;
		    double t[2];
		    int nz = gx_curve_monotonic_points(pt0.y,
					   pc->p1.y, pc->p2.y, pc->pt.y, t);

		    if (nz != 0)
			return false;
		    nz = gx_curve_monotonic_points(pt0.x,
					   pc->p1.x, pc->p2.x, pc->pt.x, t);
		    if (nz != 0)
			return false;
		}
		break;
	    default:
		;
	}
	pt0 = pseg->pt;
	pseg = pseg->next;
    }
    return true;
}

/* Monotonize a curve, by splitting it if necessary. */
/* In the worst case, this could split the curve into 9 pieces. */
private int
monotonize_internal(gx_path * ppath, const curve_segment * pc)
{
    fixed x0 = ppath->position.x, y0 = ppath->position.y;
    segment_notes notes = pc->notes;
    double t[2];

#define max_segs 9
    curve_segment cs[max_segs];
    const curve_segment *pcs;
    curve_segment *pcd;
    int i, j, nseg;
    int nz;

    /* Monotonize in Y. */
    nz = gx_curve_monotonic_points(y0, pc->p1.y, pc->p2.y, pc->pt.y, t);
    nseg = max_segs - 1 - nz;
    pcd = cs + nseg;
    if (nz == 0)
	*pcd = *pc;
    else {
	gx_curve_split(x0, y0, pc, t[0], pcd, pcd + 1);
	if (nz == 2)
	    gx_curve_split(pcd->pt.x, pcd->pt.y, pcd + 1,
			   (t[1] - t[0]) / (1 - t[0]),
			   pcd + 1, pcd + 2);
    }

    /* Monotonize in X. */
    for (pcs = pcd, pcd = cs, j = nseg; j < max_segs; ++pcs, ++j) {
	nz = gx_curve_monotonic_points(x0, pcs->p1.x, pcs->p2.x,
				       pcs->pt.x, t);

	if (nz == 0)
	    *pcd = *pcs;
	else {
	    gx_curve_split(x0, y0, pcs, t[0], pcd, pcd + 1);
	    if (nz == 2)
		gx_curve_split(pcd->pt.x, pcd->pt.y, pcd + 1,
			       (t[1] - t[0]) / (1 - t[0]),
			       pcd + 1, pcd + 2);
	}
	pcd += nz + 1;
	x0 = pcd[-1].pt.x;
	y0 = pcd[-1].pt.y;
    }
    nseg = pcd - cs;

    /* Add the segment(s) to the output. */
#ifdef DEBUG
    if (gs_debug_c('2')) {
	int pi;
	gs_fixed_point pp0;

	pp0 = ppath->position;
	if (nseg == 1)
	    dprint_curve("[2]No split", pp0.x, pp0.y, pc);
	else {
	    dlprintf1("[2]Split into %d segments:\n", nseg);
	    dprint_curve("[2]Original", pp0.x, pp0.y, pc);
	    for (pi = 0; pi < nseg; ++pi) {
		dprint_curve("[2] =>", pp0.x, pp0.y, cs + pi);
		pp0 = cs[pi].pt;
	    }
	}
    }
#endif
    for (pcs = cs, i = 0; i < nseg; ++pcs, ++i) {
	int code = gx_path_add_curve_notes(ppath, pcs->p1.x, pcs->p1.y,
					   pcs->p2.x, pcs->p2.y,
					   pcs->pt.x, pcs->pt.y,
					   notes |
					   (i > 0 ? sn_not_first :
					    sn_none));

	if (code < 0)
	    return code;
    }

    return 0;
}

/*
 * Split a curve if necessary into pieces that are monotonic in X or Y as a
 * function of the curve parameter t.  This allows us to rasterize curves
 * directly without pre-flattening.  This takes a fair amount of analysis....
 * Store the values of t of the split points in pst[0] and pst[1].  Return
 * the number of split points (0, 1, or 2).
 */
int
gx_curve_monotonic_points(fixed v0, fixed v1, fixed v2, fixed v3,
			  double pst[2])
{
    /*
       Let
       v(t) = a*t^3 + b*t^2 + c*t + d, 0 <= t <= 1.
       Then
       dv(t) = 3*a*t^2 + 2*b*t + c.
       v(t) has a local minimum or maximum (or inflection point)
       precisely where dv(t) = 0.  Now the roots of dv(t) = 0 (i.e.,
       the zeros of dv(t)) are at
       t =  ( -2*b +/- sqrt(4*b^2 - 12*a*c) ) / 6*a
       =    ( -b +/- sqrt(b^2 - 3*a*c) ) / 3*a
       (Note that real roots exist iff b^2 >= 3*a*c.)
       We want to know if these lie in the range (0..1).
       (The endpoints don't count.)  Call such a root a "valid zero."
       Since computing the roots is expensive, we would like to have
       some cheap tests to filter out cases where they don't exist
       (i.e., where the curve is already monotonic).
     */
    fixed v01, v12, a, b, c, b2, a3;
    fixed dv_end, b2abs, a3abs;

    curve_points_to_coefficients(v0, v1, v2, v3, a, b, c, v01, v12);
    b2 = b << 1;
    a3 = (a << 1) + a;
    /*
       If a = 0, the only possible zero is t = -c / 2*b.
       This zero is valid iff sign(c) != sign(b) and 0 < |c| < 2*|b|.
     */
    if (a == 0) {
	if ((b ^ c) < 0 && any_abs(c) < any_abs(b2) && c != 0) {
	    *pst = (double)(-c) / b2;
	    return 1;
	} else
	    return 0;
    }
    /*
       Iff a curve is horizontal at t = 0, c = 0.  In this case,
       there can be at most one other zero, at -2*b / 3*a.
       This zero is valid iff sign(a) != sign(b) and 0 < 2*|b| < 3*|a|.
     */
    if (c == 0) {
	if ((a ^ b) < 0 && any_abs(b2) < any_abs(a3) && b != 0) {
	    *pst = (double)(-b2) / a3;
	    return 1;
	} else
	    return 0;
    }
    /*
       Similarly, iff a curve is horizontal at t = 1, 3*a + 2*b + c = 0.
       In this case, there can be at most one other zero,
       at -1 - 2*b / 3*a, iff sign(a) != sign(b) and 1 < -2*b / 3*a < 2,
       i.e., 3*|a| < 2*|b| < 6*|a|.
     */
    else if ((dv_end = a3 + b2 + c) == 0) {
	if ((a ^ b) < 0 &&
	    (b2abs = any_abs(b2)) > (a3abs = any_abs(a3)) &&
	    b2abs < a3abs << 1
	    ) {
	    *pst = (double)(-b2 - a3) / a3;
	    return 1;
	} else
	    return 0;
    }
    /*
       If sign(dv_end) != sign(c), at least one valid zero exists,
       since dv(0) and dv(1) have opposite signs and hence
       dv(t) must be zero somewhere in the interval [0..1].
     */
    else if ((dv_end ^ c) < 0);
    /*
       If sign(a) = sign(b), no valid zero exists,
       since dv is monotonic on [0..1] and has the same sign
       at both endpoints.
     */
    else if ((a ^ b) >= 0)
	return 0;
    /*
       Otherwise, dv(t) may be non-monotonic on [0..1]; it has valid zeros
       iff its sign anywhere in this interval is different from its sign
       at the endpoints, which occurs iff it has an extremum in this
       interval and the extremum is of the opposite sign from c.
       To find this out, we look for the local extremum of dv(t)
       by observing
       d2v(t) = 6*a*t + 2*b
       which has a zero only at
       t1 = -b / 3*a
       Now if t1 <= 0 or t1 >= 1, no valid zero exists.
       Note that we just determined that sign(a) != sign(b), so we know t1 > 0.
     */
    else if (any_abs(b) >= any_abs(a3))
	return 0;
    /*
       Otherwise, we just go ahead with the computation of the roots,
       and test them for being in the correct range.  Note that a valid
       zero is an inflection point of v(t) iff d2v(t) = 0; we don't
       bother to check for this case, since it's rare.
     */
    {
	double nbf = (double)(-b);
	double a3f = (double)a3;
	double radicand = nbf * nbf - a3f * c;

	if (radicand < 0) {
	    if_debug1('2', "[2]negative radicand = %g\n", radicand);
	    return 0;
	} {
	    double root = sqrt(radicand);
	    int nzeros = 0;
	    double z = (nbf - root) / a3f;

	    /*
	     * We need to return the zeros in the correct order.
	     * We know that root is non-negative, but a3f may be either
	     * positive or negative, so we need to check the ordering
	     * explicitly.
	     */
	    if_debug2('2', "[2]zeros at %g, %g\n", z, (nbf + root) / a3f);
	    if (z > 0 && z < 1)
		*pst = z, nzeros = 1;
	    if (root != 0) {
		z = (nbf + root) / a3f;
		if (z > 0 && z < 1) {
		    if (nzeros && a3f < 0)	/* order is reversed */
			pst[1] = *pst, *pst = z;
		    else
			pst[nzeros] = z;
		    nzeros++;
		}
	    }
	    return nzeros;
	}
    }
}

/*
 * Split a curve at an arbitrary point t.  The above midpoint split is a
 * special case of this with t = 0.5.
 */
void
gx_curve_split(fixed x0, fixed y0, const curve_segment * pc, double t,
	       curve_segment * pc1, curve_segment * pc2)
{				/*
				 * If the original function was v(t), we want to compute the points
				 * for the functions v1(T) = v(t * T) and v2(T) = v(t + (1 - t) * T).
				 * Straightforwardly,
				 *      v1(T) = a*t^3*T^3 + b*t^2*T^2 + c*t*T + d
				 * i.e.
				 *      a1 = a*t^3, b1 = b*t^2, c1 = c*t, d1 = d.
				 * Similarly,
				 *      v2(T) = a*[t + (1-t)*T]^3 + b*[t + (1-t)*T]^2 +
				 *              c*[t + (1-t)*T] + d
				 *            = a*[(1-t)^3*T^3 + 3*t*(1-t)^2*T^2 + 3*t^2*(1-t)*T +
				 *                 t^3] + b*[(1-t)^2*T^2 + 2*t*(1-t)*T + t^2] +
				 *                 c*[(1-t)*T + t] + d
				 *            = a*(1-t)^3*T^3 + [a*3*t + b]*(1-t)^2*T^2 +
				 *                 [a*3*t^2 + b*2*t + c]*(1-t)*T +
				 *                 a*t^3 + b*t^2 + c*t + d
				 * We do this in the simplest way, namely, we convert the points to
				 * coefficients, do the arithmetic, and convert back.  It would
				 * obviously be faster to do the arithmetic directly on the points,
				 * as the midpoint code does; this is just an implementation issue
				 * that we can revisit if necessary.
				 */
    double t2 = t * t, t3 = t2 * t;
    double omt = 1 - t, omt2 = omt * omt, omt3 = omt2 * omt;
    fixed v01, v12, a, b, c, na, nb, nc;

    if_debug1('2', "[2]splitting at t = %g\n", t);
#define compute_seg(v0, v)\
	curve_points_to_coefficients(v0, pc->p1.v, pc->p2.v, pc->pt.v,\
				     a, b, c, v01, v12);\
	na = (fixed)(a * t3), nb = (fixed)(b * t2), nc = (fixed)(c * t);\
	curve_coefficients_to_points(na, nb, nc, v0,\
				     pc1->p1.v, pc1->p2.v, pc1->pt.v);\
	na = (fixed)(a * omt3);\
	nb = (fixed)((a * t * 3 + b) * omt2);\
	nc = (fixed)((a * t2 * 3 + b * 2 * t + c) * omt);\
	curve_coefficients_to_points(na, nb, nc, pc1->pt.v,\
				     pc2->p1.v, pc2->p2.v, pc2->pt.v)
    compute_seg(x0, x);
    compute_seg(y0, y);
#undef compute_seg
}
