/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxpcopy.c */
/* Path copying and flattening */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxarith.h"
#include "gzpath.h"

/* Define whether to merge nearly collinear line segments when flattening */
/* curves.  This is very good for performance, but we feel a little */
/* uneasy about its effects on character appearance. */
#define MERGE_COLLINEAR_SEGMENTS 1

/* Forward declarations */
private int flatten_internal(P3(gx_path *, const curve_segment *, fixed));
private int flatten_sample(P3(gx_path *, int, curve_segment *));
private int monotonize_internal(P2(gx_path *, const curve_segment *));

/* Copy a path, optionally flattening or monotonizing it. */
/* If the copy fails, free the new path. */
int
gx_path_copy_reducing(const gx_path *ppath_old, gx_path *ppath,
  fixed fixed_flatness, bool monotonize, bool init)
{	gx_path old;
	const segment *pseg;
	int code;
#ifdef DEBUG
	if ( gs_debug_c('P') )
	  gx_dump_path(ppath_old, "before copy_path");
#endif
	old = *ppath_old;
	if ( init )
	  gx_path_init(ppath, ppath_old->memory);
	pseg = (const segment *)(old.first_subpath);
	while ( pseg )
	   {	switch ( pseg->type )
		  {
		  case s_start:
			code = gx_path_add_point(ppath, pseg->pt.x, pseg->pt.y);
			break;
		  case s_curve:
		   {	const curve_segment *pc = (const curve_segment *)pseg;
			if ( fixed_flatness == max_fixed ) /* don't flatten */
			  { if ( monotonize )
			      code = monotonize_internal(ppath, pc);
			    else
			      code = gx_path_add_curve(ppath,
						       pc->p1.x, pc->p1.y,
						       pc->p2.x, pc->p2.y,
						       pc->pt.x, pc->pt.y);
			  }
			else
			  code = flatten_internal(ppath, pc, fixed_flatness);
			break;
		   }
		  case s_line:
			code = gx_path_add_line(ppath, pseg->pt.x, pseg->pt.y);
			break;
		  case s_line_close:
			code = gx_path_close_subpath(ppath);
			break;
		  default:		/* can't happen */
			code = gs_note_error(gs_error_unregistered);
		  }
		if ( code )
		   {	gx_path_release(ppath);
			if ( ppath == ppath_old )
			  *ppath = old;
			return code;
		   }
		pseg = pseg->next;
	}
	if ( old.subpath_open < 0 )
	  gx_path_add_point(ppath, old.position.x, old.position.y);
#ifdef DEBUG
	if ( gs_debug_c('P') )
	  gx_dump_path(ppath, "after copy_path");
#endif
	return 0;
}

/* ---------------- Curve flattening ---------------- */

#define x1 pc->p1.x
#define y1 pc->p1.y
#define x2 pc->p2.x
#define y2 pc->p2.y
#define x3 pc->pt.x
#define y3 pc->pt.y

/*
 * To calculate how many points to sample along a path in order to
 * approximate it to the desired degree of flatness, we define
 *	dist((x,y)) = abs(x) + abs(y);
 * then the number of points we need is
 *	N = 1 + sqrt(3/4 * D / flatness),
 * where
 *	D = max(dist(p0 - 2*p1 + p2), dist(p1 - 2*p2 + p3)).
 * Since we are going to use a power of 2 for the number of intervals,
 * we can avoid the square root by letting
 *	N = 1 + 2^(ceiling(log2(3/4 * D / flatness) / 2)).
 * (Reference: DEC Paris Research Laboratory report #1, May 1989.)
 *
 * We treat two cases specially.  First, if the curve is very
 * short, we halve the flatness, to avoid turning short shallow curves
 * into short straight lines.  Second, if the curve forms part of a
 * character, or the flatness is less than half a pixel, we let
 *	N = 1 + 2 * max(abs(x3-x0), abs(y3-y0)).
 * This is probably too conservative, but it produces good results.
 */
int
gx_curve_log2_samples(fixed x0, fixed y0, const curve_segment *pc,
  fixed fixed_flat)
{	fixed
	  x03 = x3 - x0,
	  y03 = y3 - y0;
	int k;

	if ( x03 < 0 )
	  x03 = -x03;
	if ( y03 < 0 )
	  y03 = -y03;
	if ( (x03 | y03) < int2fixed(16) )
	  fixed_flat >>= 1;
	if ( fixed_flat < fixed_half )
	{	/* Use the conservative method. */
		fixed m = max(x03, y03);
		for ( k = 1; m > fixed_1; )
		  k++, m >>= 1;
	}
	else
	{	const fixed
		  x12 = x1 - x2,
		  y12 = y1 - y2,
		  dx0 = x0 - x1 - x12,
		  dy0 = y0 - y1 - y12,
		  dx1 = x12 - x2 + x3,
		  dy1 = y12 - y2 + y3,
		  adx0 = any_abs(dx0),
		  ady0 = any_abs(dy0),
		  adx1 = any_abs(dx1),
		  ady1 = any_abs(dy1);
		fixed
		  d = max(adx0, adx1) + max(ady0, ady1);
		fixed q;

		if_debug6('2', "[2]d01=%g,%g d12=%g,%g d23=%g,%g\n",
			  fixed2float(x1 - x0), fixed2float(y1 - y0),
			  fixed2float(-x12), fixed2float(-y12),
			  fixed2float(x3 - x2), fixed2float(y3 - y2));
		if_debug2('2', "     D=%f, flat=%f,",
			  fixed2float(d), fixed2float(fixed_flat));
		d -= d >> 2;			/* 3/4 * D */
		if ( d < (fixed)1 << (sizeof(fixed) * 8 - _fixed_shift - 1) )
		  q = (d << _fixed_shift) / fixed_flat;
		else
		  q = float2fixed((float)d / fixed_flat);
		/* Now we want to set k = ceiling(log2(q) / 2). */
		for ( k = 0; q > fixed_1; )
		  k++, q >>= 2;
		if_debug1('2', " k=%d\n", k);
	}
	return k;
}

private int
flatten_internal(gx_path *ppath, const curve_segment *pc, fixed fixed_flat)
{	int k = gx_curve_log2_samples(ppath->position.x, ppath->position.y,
				      pc, fixed_flat);
	curve_segment cseg;

	cseg.p1.x = x1, cseg.p1.y = y1;
	cseg.p2.x = x2, cseg.p2.y = y2;
	cseg.pt.x = x3, cseg.pt.y = y3;
	return flatten_sample(ppath, k, &cseg);
}

/*
 * Define the maximum number of points for sampling if we want accurate
 * rasterizing.  2^(k_sample_max*3)-1 must fit into a uint with a bit
 * to spare; also, we must be able to compute 1/2^(3*k) by table lookup.
 */
#define k_sample_max min((size_of(int) * 8 - 1) / 3, 10)

/*
 * Split a curve segment into two pieces at the (parametric) midpoint.
 * Algorithm is from "The Beta2-split: A special case of the Beta-spline
 * Curve and Surface Representation," B. A. Barsky and A. D. DeRose, IEEE,
 * 1985, courtesy of Crispin Goswell.
 */
#ifdef DEBUG
private void
dprint_curve(const char *str, fixed x0, fixed y0, const curve_segment *pc)
{	dprintf9("%s p0=(%g,%g) p1=(%g,%g) p2=(%g,%g) p3=(%g,%g)\n",
		 str, fixed2float(x0), fixed2float(y0),
		 fixed2float(pc->p1.x), fixed2float(pc->p1.y),
		 fixed2float(pc->p2.x), fixed2float(pc->p2.y),
		 fixed2float(pc->pt.x), fixed2float(pc->pt.y));
}
#endif
private void
split_curve_midpoint(fixed x0, fixed y0, const curve_segment *pc,
  curve_segment *pc1, curve_segment *pc2)
{	/*
	 * We have to define midpoint carefully to avoid overflow.
	 * (If it overflows, something really pathological is going
	 * on, but we could get infinite recursion that way....)
	 */
#define midpoint(a,b)\
  (arith_rshift_1(a) + arith_rshift_1(b) + ((a) & (b) & 1) + 1)
	fixed x12 = midpoint(x1, x2);
	fixed y12 = midpoint(y1, y2);

	/*
	 * pc1 or pc2 may be the same as pc, so we must be a little careful
	 * about the order in which we store the results.
	 */
	pc1->p1.x = midpoint(x0, x1);
	pc1->p1.y = midpoint(y0, y1);
	pc2->p2.x = midpoint(x2, x3);
	pc2->p2.y = midpoint(y2, y3);
	pc1->p2.x = midpoint(pc1->p1.x, x12);
	pc1->p2.y = midpoint(pc1->p1.y, y12);
	pc2->p1.x = midpoint(x12, pc2->p2.x);
	pc2->p1.y = midpoint(y12, pc2->p2.y);
	if ( pc2 != pc )
	  pc2->pt.x = pc->pt.x,
	  pc2->pt.y = pc->pt.y;
	pc1->pt.x = midpoint(pc1->p2.x, pc2->p1.x);
	pc1->pt.y = midpoint(pc1->p2.y, pc2->p1.y);
#undef midpoint
}

/* Initialize a cursor for rasterizing a monotonic curve. */
void
gx_curve_cursor_init(curve_cursor *prc, fixed x0, fixed y0,
  const curve_segment *pc, int k)
{	fixed v01, v12;
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
	curve_points_to_coefficients(x0, x1, x2, x3,
				     prc->a, prc->b, prc->c, v01, v12);
	prc->double_set = false;
	prc->fixed_fits = coeffs_fit(prc->a, prc->b, prc->c);
}

/*
 * Determine the X value on a monotonic curve at a given Y value.  It turns
 * out that we use so few points on the curve that it's actually faster to
 * locate the desired point by recursive subdivision each time than to try
 * to keep a cursor that we move incrementally.  What's even more surprising
 * is that it's faster to compute the X value at the desired point explicitly
 * than to do the recursive subdivision on X as well as Y.
 */
fixed
gx_curve_x_at_y(curve_cursor *prc, fixed y)
{	const curve_segment *pc = prc->pc;
#define x0 prc->p0.x
#define y0 prc->p0.y
	fixed cy0 = y0, cy1 = y1, cy2 = y2, cy3 = y3;
	bool increasing = cy0 < cy3;
	int k = prc->k;
	int i, t;
	fixed xl, xd;

#define midpoint_fast(a,b)\
  arith_rshift_1((a) + (b) + 1)
	for ( t = i = 0; i < k; ++i )
	  {
	    fixed ym = midpoint_fast(cy1, cy2);
	    fixed yn = ym + arith_rshift(cy0 - cy1 - cy2 + cy3 + 4, 3);

	    t <<= 1;
	    if ( (y < yn) == increasing )
	      cy1 = midpoint_fast(cy0, cy1),
	      cy2 = midpoint_fast(cy1, ym),
	      cy3 = yn;
	    else
	      cy2 = midpoint_fast(cy2, cy3),
	      cy1 = midpoint_fast(ym, cy2),
	      cy0 = yn, t++;
	  }
	{ fixed a = prc->a, b = prc->b, c = prc->c;

/* We must use (1 << k) >> 1 instead of 1 << (k - 1) in case k == 0. */
#define compute_fixed(a, b, c)\
  arith_rshift(arith_rshift(arith_rshift(a * t3, k) + b * t2, k)\
	       + c * t + ((1 << k) >> 1), k)
#define compute_diff_fixed(a, b, c)\
  arith_rshift(arith_rshift(arith_rshift(a * t3d, k) + b * t2d, k)\
	       + c, k)

#define np2(n) (1.0 / (1L << (n)))
	      static const double k_denom[11] =
		{ np2(0), np2(1), np2(2), np2(3), np2(4),
		  np2(5), np2(6), np2(7), np2(8), np2(9), np2(10)
		};
	      static const double k2_denom[11] =
		{ np2(0), np2(2), np2(4), np2(6), np2(8),
		  np2(10), np2(12), np2(14), np2(16), np2(18), np2(20)
		};
	      static const double k3_denom[11] =
		{ np2(0), np2(3), np2(6), np2(9), np2(12),
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

	  if ( prc->fixed_fits )
	    { int t2 = t * t, t3 = t2 * t;
	      int t3d = (t2 + t) * 3 + 1, t2d = t + t + 1;
	      xl = compute_fixed(a, b, c) + x0;
	      xd = compute_diff_fixed(a, b, c);
#ifdef DEBUG
	      { double fa, fb, fc;
		fixed xlf, xdf;
		setup_floating(fa, fb, fc, a, b, c);
		xlf = compute_floating(fa, fb, fc) + x0;
		xdf = compute_diff_floating(fa, fb, fc);
		if ( any_abs(xlf - xl) > fixed_epsilon ||
		     any_abs(xdf - xd) > fixed_epsilon
		   )
		  dprintf9("Curve points differ: k=%d t=%d a,b,c=%g,%g,%g\n   xl,xd fixed=%g,%g floating=%g,%g\n",
			   k, t,
			   fixed2float(a), fixed2float(b), fixed2float(c),
			   fixed2float(xl), fixed2float(xd),
			   fixed2float(xlf), fixed2float(xdf));
/*xl = xlf, xd = xdf;*/
	      }
#endif
	    }
#define fa prc->da
#define fb prc->db
#define fc prc->dc
#if arch_sizeof_long > arch_sizeof_int
	  else if ( t < 1L << ((sizeof(long) * 8 - 1) / 3) )
	    { /* t3 (and maybe t2) might not fit in an int, */
	      /* but they will fit in a long. */
	      long t2 = (long)t * t, t3 = t2 * t;
	      long t3d = (t2 + t) * 3 + 1, t2d = t + t + 1;
	      if ( !prc->double_set )
		{ setup_floating(fa, fb, fc, a, b, c);
		  prc->double_set = true;
		}
	      xl = compute_floating(fa, fb, fc) + x0;
	      xd = compute_diff_floating(fa, fb, fc);
	    }
#endif
	  else
	    { /* t3 (and maybe t2) don't even fit in a long. */
	      double t2 = (double)t * t, t3 = t2 * t;
	      double t3d = (t2 + t) * 3 + 1, t2d = t + t + 1;
	      if ( !prc->double_set )
		{ setup_floating(fa, fb, fc, a, b, c);
		  prc->double_set = true;
		}
	      xl = compute_floating(fa, fb, fc) + x0;
	      xd = compute_diff_floating(fa, fb, fc);
	    }
#undef fa
#undef fb
#undef fc
	}

	/*
	 * Now interpolate linearly between current and next.
	 */

	{ fixed yd = cy3 - cy0, yrel = y - cy0;

	  /* It's unlikely but possible that cy0 = y = cy3. */
	  /* Handle this case specially. */
	  if ( yrel == 0 )
	    return xl;
	  /* Compute in fixed point if possible. */
	  if ( any_abs(yrel) < ((fixed)1 << (sizeof(fixed) * 4 - 1)) &&
	       any_abs(xd) < ((fixed)1 << (sizeof(fixed) * 4 - 1))
	     )
	    return xd * yrel / yd + xl;
	  return fixed_mult_quo(xd, yrel, yd) + xl;
	}
#undef x0
#undef y0
}

/*
 * Flatten a segment of the path by repeated sampling.
 * 2^k is the number of lines to produce (i.e., the number of points - 1,
 * including the endpoints); we require k >= 1.
 * If k or any of the coefficient values are too large,
 * use recursive subdivision to whittle them down.
 */
private int
flatten_sample(gx_path *ppath, int k, curve_segment *pc)
{	fixed x0, y0;
	/* x1 ... y3 were defined above */
	fixed cx, bx, ax, cy, by, ay;
	fixed ptx, pty;
	fixed x, y;
	/*
	 * We can compute successive values by finite differences,
	 * using the formulas:
		x(t) =
		  a*t^3 + b*t^2 + c*t + d =>
		dx(t) = x(t+e)-x(t) =
		  a*(3*t^2*e + 3*t*e^2 + e^3) + b*(2*t*e + e^2) + c*e =
		  (3*a*e)*t^2 + (3*a*e^2 + 2*b*e)*t + (a*e^3 + b*e^2 + c*e) =>
		d2x(t) = dx(t+e)-dx(t) =
		  (3*a*e)*(2*t*e + e^2) + (3*a*e^2 + 2*b*e)*e =
		  (6*a*e^2)*t + (6*a*e^3 + 2*b*e^2) =>
		d3x(t) = d2x(t+e)-d2x(t) =
		  6*a*e^3;
		x(0) = d, dx(0) = (a*e^3 + b*e^2 + c*e),
		  d2x(0) = 6*a*e^3 + 2*b*e^2;
	 * In these formulas, e = 1/2^k; of course, there are separate
	 * computations for the x and y values.
	 *
	 * There is a tradeoff in doing the above computation in fixed
	 * point.  If we separate out the constant term (d) and require that
	 * all the other values fit in a long, then on a 32-bit machine with
	 * 12 bits of fraction in a fixed, k = 4 implies a maximum curve
	 * size of 128 pixels; anything larger requires subdividing the
	 * curve.  On the other hand, doing the computations in explicit
	 * double precision slows down the loop by a factor of 3 or so.  We
	 * found to our surprise that the latter is actually faster, because
	 * the additional subdivisions cost more than the slower loop.
	 *
	 * We represent each quantity as I+R/M, where I is an "integer" and
	 * the "remainder" R lies in the range 0 <= R < M=2^(3*k).  Note
	 * that R may temporarily exceed M; for this reason, we require that
	 * M have at least one free high-order bit.  To reduce the number of
	 * variables, we don't actually compute M, only M-1 (rmask).  */
	uint i;
	uint rmask;			/* M-1 */
	fixed idx, idy, id2x, id2y, id3x, id3y;		/* I */
	uint rx, ry, rdx, rdy, rd2x, rd2y, rd3x, rd3y;	/* R */
	gs_fixed_point *ppt;
#define max_points 50			/* arbitrary */
	gs_fixed_point points[max_points + 1];

top:	x0 = ppath->position.x;
	y0 = ppath->position.y;
#ifdef DEBUG
	if ( gs_debug_c('3') )
	  dprintf4("[3]x0=%f y0=%f x1=%f y1=%f\n",
		   fixed2float(x0), fixed2float(y0),
		   fixed2float(x1), fixed2float(y1)),
	  dprintf5("   x2=%f y2=%f x3=%f y3=%f  k=%d\n",
		   fixed2float(x2), fixed2float(y2),
		   fixed2float(x3), fixed2float(y3), k);
#endif
	{	fixed x01, x12, y01, y12;
		curve_points_to_coefficients(x0, x1, x2, x3, ax, bx, cx,
					     x01, x12);
		curve_points_to_coefficients(y0, y1, y2, y3, ay, by, cy,
					     y01, y12);
	}

	if_debug6('3', "[3]ax=%f bx=%f cx=%f\n   ay=%f by=%f cy=%f\n",
		  fixed2float(ax), fixed2float(bx), fixed2float(cx),
		  fixed2float(ay), fixed2float(by), fixed2float(cy));
#define max_fast (max_fixed / 6)
#define min_fast (-max_fast)
#define in_range(v) (v < max_fast && v > min_fast)
	if ( k == 0 )
	{	/* The curve is very short, or anomalous in some way. */
		/* Just add a line and exit. */
		return gx_path_add_line(ppath, x3, y3);
	}
	if ( k <= k_sample_max &&
	     in_range(ax) && in_range(ay) &&
	     in_range(bx) && in_range(by) &&
	     in_range(cx) && in_range(cy)
	   )
	{	x = x0, y = y0;
		rx = ry = 0;
		ppt = points;
		/* Fast check for n == 3, a common special case */
		/* for small characters. */
		if ( k == 1 )
		{
#define poly2(a,b,c)\
  arith_rshift_1(arith_rshift_1(arith_rshift_1(a) + b) + c)
			x += poly2(ax, bx, cx);
			y += poly2(ay, by, cy);
#undef poly2
			if_debug2('3', "[3]dx=%f, dy=%f\n",
				  fixed2float(x - x0), fixed2float(y - y0));
			if_debug3('3', "[3]%s x=%g, y=%g\n",
				  (((x ^ x0) | (y ^ y0)) & float2fixed(-0.5) ?
				   "add" : "skip"),
				  fixed2float(x), fixed2float(y));
			if ( ((x ^ x0) | (y ^ y0)) & float2fixed(-0.5) )
			  ppt->x = ptx = x,
			  ppt->y = pty = y,
			  ppt++;
			goto last;
		}
		else
		{	fixed bx2 = bx << 1, by2 = by << 1;
			fixed ax6 = ((ax << 1) + ax) << 1,
			      ay6 = ((ay << 1) + ay) << 1;
#define adjust_rem(r, q)\
  if ( r > rmask ) q ++, r &= rmask
			const int k2 = k << 1;
			const int k3 = k2 + k;
			rmask = (1 << k3) - 1;
			/* We can compute all the remainders as ints, */
			/* because we know they don't exceed M. */
			/* cx/y terms */
			idx = arith_rshift(cx, k),
			  idy = arith_rshift(cy, k);
			rdx = ((uint)cx << k2) & rmask,
			  rdy = ((uint)cy << k2) & rmask;
			/* bx/y terms */
			id2x = arith_rshift(bx2, k2),
			  id2y = arith_rshift(by2, k2);
			rd2x = ((uint)bx2 << k) & rmask,
			  rd2y = ((uint)by2 << k) & rmask;
			idx += arith_rshift_1(id2x),
			  idy += arith_rshift_1(id2y);
			rdx += ((uint)bx << k) & rmask,
			  rdy += ((uint)by << k) & rmask;
			adjust_rem(rdx, idx);
			adjust_rem(rdy, idy);
			/* ax/y terms */
			idx += arith_rshift(ax, k3),
			  idy += arith_rshift(ay, k3);
			rdx += (uint)ax & rmask,
			  rdy += (uint)ay & rmask;
			adjust_rem(rdx, idx);
			adjust_rem(rdy, idy);
			id2x += id3x = arith_rshift(ax6, k3),
			  id2y += id3y = arith_rshift(ay6, k3);
			rd2x += rd3x = (uint)ax6 & rmask,
			  rd2y += rd3y = (uint)ay6 & rmask;
			adjust_rem(rd2x, id2x);
			adjust_rem(rd2y, id2y);
#undef adjust_rem
		}
	}
	else
	{	/*
		 * Curve is too long.  Break into two pieces and recur.
		 */
		curve_segment cseg;
		int code;

		k--;
		split_curve_midpoint(x0, y0, pc, &cseg, pc);
		code = flatten_sample(ppath, k, &cseg);
		if ( code < 0 )
		  return code;
		goto top;
	}
	if_debug1('2', "[2]sampling k=%d\n", k);
	ptx = x0, pty = y0;
	for ( i = (1 << k) - 1; ; )
	{	int code;
#ifdef DEBUG
		if ( gs_debug_c('3') )
		  dprintf4("[3]dx=%f+%d, dy=%f+%d\n",
			   fixed2float(idx), rdx,
			   fixed2float(idy), rdy),
		  dprintf4("   d2x=%f+%d, d2y=%f+%d\n",
			   fixed2float(id2x), rd2x,
			   fixed2float(id2y), rd2y),
		  dprintf4("   d3x=%f+%d, d3y=%f+%d\n",
			   fixed2float(id3x), rd3x,
			   fixed2float(id3y), rd3y);
#endif
#define accum(i, r, di, dr)\
  if ( (r += dr) > rmask ) r &= rmask, i += di + 1;\
  else i += di
		accum(x, rx, idx, rdx);
		accum(y, ry, idy, rdy);
		if_debug3('3', "[3]%s x=%g, y=%g\n",
			  (((x ^ ptx) | (y ^ pty)) & float2fixed(-0.5) ?
			   "add" : "skip"),
			  fixed2float(x), fixed2float(y));
		/*
		 * Skip very short segments -- those that lie entirely within
		 * a square half-pixel.  Also merge nearly collinear
		 * segments -- those where one coordinate of all three points
		 * (the two endpoints and the midpoint) lie within the same
		 * half-pixel and both coordinates are monotonic.
		 * Note that ptx/y, the midpoint, is the same as ppt[-1].x/y;
		 * the previous point is ppt[-2].x/y.
		 */
#define coord_near(v, ptv)\
  (!( ((v) ^ (ptv)) & float2fixed(-0.5) ))
#define coords_in_order(v0, v1, v2)\
  ( (((v1) - (v0)) ^ ((v2) - (v1))) >= 0 )
		if ( coord_near(x, ptx) )
		  {	/* X coordinates are within a half-pixel. */
			if ( coord_near(y, pty) )
			  goto skip;		/* short segment */
#if MERGE_COLLINEAR_SEGMENTS
			/* Check for collinear segments. */
			if ( ppt > points + 1 && coord_near(x, ppt[-2].x) &&
			     coords_in_order(ppt[-2].x, ptx, x) &&
			     coords_in_order(ppt[-2].y, pty, y)
			   )
			  --ppt;	/* remove middle point */
#endif
		  }
		else if ( coord_near(y, pty) )
		  {	/* Y coordinates are within a half-pixel. */
#if MERGE_COLLINEAR_SEGMENTS
			/* Check for collinear segments. */
			if ( ppt > points + 1 && coord_near(y, ppt[-2].y) &&
			     coords_in_order(ppt[-2].x, ptx, x) &&
			     coords_in_order(ppt[-2].y, pty, y)
			   )
			  --ppt;	/* remove middle point */
#endif
		  }
#undef coord_near
#undef coords_in_order
		/* Add a line. */
		{	if ( ppt == &points[max_points] )
			  {	if ( (code = gx_path_add_lines(ppath, points, max_points)) < 0 )
				  return code;
				ppt = points;
			  }
			ppt->x = ptx = x;
			ppt->y = pty = y;
			ppt++;
		}
skip:		if ( --i == 0 )
		  break;		/* don't bother with last accum */
		accum(idx, rdx, id2x, rd2x);
		accum(id2x, rd2x, id3x, rd3x);
		accum(idy, rdy, id2y, rd2y);
		accum(id2y, rd2y, id3y, rd3y);
#undef accum
	}
last:	if_debug2('3', "[3]last x=%g, y=%g\n",
		  fixed2float(x3), fixed2float(y3));
	ppt->x = x3, ppt->y = y3;
	return gx_path_add_lines(ppath, points, (int)(ppt + 1 - points));
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
gx_path_is_monotonic(const gx_path *ppath)
{	const segment *pseg = (const segment *)(ppath->first_subpath);
	gs_fixed_point pt0;

	while ( pseg )
	   {	switch ( pseg->type )
		  {
		  case s_start:
		    {	const subpath *psub = (const subpath *)pseg;
			/* Skip subpaths without curves. */
			if ( !psub->curve_count )
			  pseg = psub->last;
		    }
		    break;
		  case s_curve:
		    {	const curve_segment *pc = (const curve_segment *)pseg;
			double t[2];
			int nz = gx_curve_monotonic_points(pt0.y,
					pc->p1.y, pc->p2.y, pc->pt.y, t);
			if ( nz != 0 )
			  return false;
			nz = gx_curve_monotonic_points(pt0.x,
					pc->p1.x, pc->p2.x, pc->pt.x, t);
			if ( nz != 0 )
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
monotonize_internal(gx_path *ppath, const curve_segment *pc)
{	fixed x0 = ppath->position.x, y0 = ppath->position.y;
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
	if ( nz == 0 )
	  *pcd = *pc;
	else
	  { gx_curve_split(x0, y0, pc, t[0], pcd, pcd + 1);
	    if ( nz == 2 )
	      gx_curve_split(pcd->pt.x, pcd->pt.y, pcd + 1, t[1] * (1 - t[0]),
			     pcd + 1, pcd + 2);
	  }

	/* Monotonize in X. */
	for ( pcs = pcd, pcd = cs, i = 0, j = nseg; j < max_segs; ++pcs, ++j )
	  { nz = gx_curve_monotonic_points(x0, pcs->p1.x, pcs->p2.x,
					   pcs->pt.x, t);
	    
	    if ( nz == 0 )
	      *pcd = *pcs;
	    else
	      { gx_curve_split(x0, y0, pcs, t[0], pcd, pcd + 1);
		if ( nz == 2 )
		  gx_curve_split(pcd->pt.x, pcd->pt.y, pcd + 1,
				 t[1] * (1 - t[0]), pcd + 1, pcd + 2);
	      }
	    pcd += nz + 1;
	    x0 = pcd[-1].pt.x;
	  }
	nseg = pcd - cs;

	/* Add the segment(s) to the output. */
#ifdef DEBUG
	if ( gs_debug_c('2') )
	  { int pi;
	    gs_fixed_point pp0;

	    pp0 = ppath->position;
	    if ( nseg == 1 )
	      dprint_curve("[2]No split", pp0.x, pp0.y, pc);
	    else
	      { dprintf1("[2]Split into %d segments:\n", nseg);
		dprint_curve("[2]Original", pp0.x, pp0.y, pc);
		for ( pi = 0; pi < nseg; ++pi )
		  { dprint_curve("[2] =>", pp0.x, pp0.y, cs + pi);
		    pp0 = cs[pi].pt;
		  }
	      }
	  }
#endif
	for ( pcs = cs, i = 0; i < nseg; ++pcs, ++i )
	  { int code = gx_path_add_curve(ppath, pcs->p1.x, pcs->p1.y,
					 pcs->p2.x, pcs->p2.y,
					 pcs->pt.x, pcs->pt.y);
	    if ( code < 0 )
	      return code;
	  }

	return 0;
}

/*
 * Split a curve if necessary into pieces that are monotonic in X or Y as a
 * function of the curve parameter t.  This will eventually allow us to
 * rasterize curves on the fly.  This takes a fair amount of analysis....
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
		  t =	( -2*b +/- sqrt(4*b^2 - 12*a*c) ) / 6*a
		    =	( -b +/- sqrt(b^2 - 3*a*c) ) / 3*a
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
	if ( a == 0 )
	  { if ( (b ^ c) < 0 && any_abs(c) < any_abs(b2) && c != 0 )
	      { *pst = (double)(-c) / b2;
		return 1;
	      }
	    else
	      return 0;
	  }
	/*
	   Iff a curve is horizontal at t = 0, c = 0.  In this case,
	   there can be at most one other zero, at -2*b / 3*a.
	   This zero is valid iff sign(a) != sign(b) and 0 < 2*|b| < 3*|a|.
	 */
	if ( c == 0 )
	  { if ( (a ^ b) < 0 && any_abs(b2) < any_abs(a3) && b != 0 )
	      { *pst = (double)(-b2) / a3;
		return 1;
	      }
	    else
	      return 0;
	  }
	/*
	   Similarly, iff a curve is horizontal at t = 1, 3*a + 2*b + c = 0.
	   In this case, there can be at most one other zero,
	   at -1 - 2*b / 3*a, iff sign(a) != sign(b) and 1 < -2*b / 3*a < 2,
	   i.e., 3*|a| < 2*|b| < 6*|a|.
	 */
	else if ( (dv_end = a3 + b2 + c) == 0 )
	  { if ( (a ^ b) < 0 &&
		 (b2abs = any_abs(b2)) > (a3abs = any_abs(a3)) &&
		 b2abs < a3abs << 1
	       )
	      { *pst = (double)(-b2 - a3) / a3;
		return 1;
	      }
	    else
	      return 0;
	  }
	/*
	   If sign(dv_end) != sign(c), at least one valid zero exists,
	   since dv(0) and dv(1) have opposite signs and hence
	   dv(t) must be zero somewhere in the interval [0..1].
	 */
	else if ( (dv_end ^ c) < 0 )
	  ;
	/*
	   If sign(a) = sign(b), no valid zero exists,
	   since dv is monotonic on [0..1] and has the same sign
	   at both endpoints.
	 */
	else if ( (a ^ b) >= 0 )
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
	else if ( any_abs(b) >= any_abs(a3) )
	  return 0;
	/*
	   Otherwise, we just go ahead with the computation of the roots,
	   and test them for being in the correct range.  Note that a valid
	   zero is an inflection point of v(t) iff d2v(t) = 0; we don't
	   bother to check for this case, since it's rare.
	 */
	{ double nbf = (double)(-b);
	  double a3f = (double)a3;
	  double radicand = nbf * nbf - a3f * c;

	  if ( radicand < 0 )
	    { if_debug1('2', "[2]negative radicand = %g\n", radicand);
	      return 0;
	    }
	  { double root = sqrt(radicand);
	    int nzeros = 0;
	    double z = (nbf + root) / a3f;

	    if_debug2('2', "[2]zeros at %g, %g\n", z, (nbf - root) / a3f);
	    if ( z > 0 && z < 1 )
	      *pst = z, nzeros = 1;
	    if ( root != 0 )
	      { z = (nbf - root) / a3f;
		if ( z > 0 && z < 1 )
		  pst[nzeros] = z, nzeros++;
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
gx_curve_split(fixed x0, fixed y0, const curve_segment *pc, double t,
  curve_segment *pc1, curve_segment *pc2)
{	/*
	 * If the original function was v(t), we want to compute the points
	 * for the functions v1(T) = v(t * T) and v2(T) = v(t + (1 - t) * T).
	 * Straightforwardly,
	 *	v1(T) = a*t^3*T^3 + b*t^2*T^2 + c*t*T + d
	 * i.e.
	 *	a1 = a*t^3, b1 = b*t^2, c1 = c*t, d1 = d.
	 * Similarly,
	 *	v2(T) = a*[t + (1-t)*T]^3 + b*[t + (1-t)*T]^2 +
	 *		c*[t + (1-t)*T] + d
	 *	      = a*[(1-t)^3*T^3 + 3*t*(1-t)^2*T^2 + 3*t^2*(1-t)*T +
	 *		   t^3] + b*[(1-t)^2*T^2 + 2*t*(1-t)*T + t^2] +
	 *		   c*[(1-t)*T + t] + d
	 *	      = a*(1-t)^3*T^3 + [a*3*t + b]*(1-t)^2*T^2 +
	 *		   [a*3*t^2 + b*2*t + c]*(1-t)*T +
	 *		   a*t^3 + b*t^2 + c*t + d
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
