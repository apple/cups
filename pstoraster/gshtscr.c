/* Copyright (C) 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gshtscr.c,v 1.2 2000/03/08 23:14:42 mike Exp $ */
/* Screen (Type 1) halftone processing for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gzstate.h"
#include "gxdevice.h"		/* for gzht.h */
#include "gzht.h"

/* Define whether to force all halftones to be strip halftones, */
/* for debugging. */
#define FORCE_STRIP_HALFTONES 0

/* Structure descriptors */
private_st_gs_screen_enum();

/* GC procedures */
#define eptr ((gs_screen_enum *)vptr)

private 
ENUM_PTRS_BEGIN(screen_enum_enum_ptrs)
{
    if (index < 1 + st_ht_order_max_ptrs) {
	gs_ptr_type_t ret =
	    ENUM_USING(st_ht_order, &eptr->order, sizeof(eptr->order),
		       index - 1);

	if (ret == 0)		/* don't stop early */
	    ENUM_RETURN(0);
	return ret;
    }
    return ENUM_USING(st_halftone, &eptr->halftone, sizeof(eptr->halftone),
		      index - (1 + st_ht_order_max_ptrs));
}
ENUM_PTR(0, gs_screen_enum, pgs);
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(screen_enum_reloc_ptrs)
{
    RELOC_PTR(gs_screen_enum, pgs);
    RELOC_USING(st_halftone, &eptr->halftone, sizeof(gs_halftone));
    RELOC_USING(st_ht_order, &eptr->order, sizeof(gx_ht_order));
}
RELOC_PTRS_END

#undef eptr

/* Define the default value of AccurateScreens that affects */
/* setscreen and setcolorscreen. */
private bool screen_accurate_screens;

/* Default AccurateScreens control */
void
gs_setaccuratescreens(bool accurate)
{
    screen_accurate_screens = accurate;
}
bool
gs_currentaccuratescreens(void)
{
    return screen_accurate_screens;
}

/* Define the MinScreenLevels user parameter similarly. */
private uint screen_min_screen_levels;

void
gs_setminscreenlevels(uint levels)
{
    screen_min_screen_levels = levels;
}
uint
gs_currentminscreenlevels(void)
{
    return screen_min_screen_levels;
}

/* Initialize the screen control statics at startup. */
void
gs_gshtscr_init(gs_memory_t *mem)
{
    gs_setaccuratescreens(false);
    gs_setminscreenlevels(1);
}

/*
 * The following implementation notes complement the general discussion of
 * halftone tiles found in gxdht.h.
 *
 * Currently we allow R(') > 1 (i.e., multiple basic cells per multi-cell)
 * only if AccurateScreens is true or if B (the number of pixels in a basic
 * cell) < MinScreenLevels; if AccurateScreens is false and B >=
 * MinScreenLevels, multi-cells and basic cells are the same.
 *
 * To find the smallest super-cell for a given multi-cell size, i.e., the
 * smallest (absolute value) coordinates where the corners of multi-cells
 * lie on the coordinate axes, we compute the values of i and j that give
 * the minimum value of W by:
 *      D = gcd(abs(M'), abs(N)), i = M'/D, j = N/D, W = C / D,
 * and similarly
 *      D' = gcd(abs(M), abs(N')), i' = N'/D', j' = M/D', W' = C / D'.
 */

/* Compute the derived values of a halftone tile. */
void
gx_compute_cell_values(gx_ht_cell_params_t * phcp)
{
    const int M = phcp->M, N = phcp->N, M1 = phcp->M1, N1 = phcp->N1;
    const uint m = any_abs(M), n = any_abs(N);
    const uint m1 = any_abs(M1), n1 = any_abs(N1);
    const ulong C = phcp->C = (ulong) m * m1 + (ulong) n * n1;
    const int D = igcd(m1, n);
    const int D1 = igcd(m, n1);

    phcp->D = D, phcp->D1 = D1;
    phcp->W = C / D, phcp->W1 = C / D1;
    /* Compute the shift value. */
    /* If M1 or N is zero, the shift is zero. */
    if (M1 && N) {
	int h = 0, k = 0, dy = 0;
	int shift;

	/*
	 * There may be a faster way to do this: see Knuth vol. 2,
	 * section 4.5.2, Algorithm X (p. 302) and exercise 15
	 * (p. 315, solution p. 523).
	 */
	while (dy != D)
	    if (dy > D) {
		if (M1 > 0)
		    ++k;
		else
		    --k;
		dy -= m1;
	    } else {
		if (N > 0)
		    ++h;
		else
		    --h;
		dy += n;
	    }
	shift = h * M + k * N1;
	/* We just computed what amounts to a right shift; */
	/* what we want is a left shift. */
	phcp->S = imod(-shift, phcp->W);
    } else
	phcp->S = 0;
    if_debug12('h', "[h]MNR=(%d,%d)/%d, M'N'R'=(%d,%d)/%d => C=%lu, D=%d, D'=%d, W=%u, W'=%u, S=%d\n",
	       M, N, phcp->R, M1, N1, phcp->R1,
	       C, D, D1, phcp->W, phcp->W1, phcp->S);
}

/* Forward references */
private int pick_cell_size(P6(gs_screen_halftone * ph,
     const gs_matrix * pmat, ulong max_size, uint min_levels, bool accurate,
			      gx_ht_cell_params_t * phcp));

/* Allocate a screen enumerator. */
gs_screen_enum *
gs_screen_enum_alloc(gs_memory_t * mem, client_name_t cname)
{
    return gs_alloc_struct(mem, gs_screen_enum, &st_gs_screen_enum, cname);
}

/* Set up for halftone sampling. */
int
gs_screen_init(gs_screen_enum * penum, gs_state * pgs,
	       gs_screen_halftone * phsp)
{
    return gs_screen_init_accurate(penum, pgs, phsp,
				   screen_accurate_screens);
}
int
gs_screen_init_memory(gs_screen_enum * penum, gs_state * pgs,
		gs_screen_halftone * phsp, bool accurate, gs_memory_t * mem)
{
    int code =
    gs_screen_order_init_memory(&penum->order, pgs, phsp, accurate, mem);

    if (code < 0)
	return code;
    return
	gs_screen_enum_init_memory(penum, &penum->order, pgs, phsp, mem);
}

/* Allocate and initialize a spot screen. */
/* This is the first half of gs_screen_init_accurate. */
int
gs_screen_order_init_memory(gx_ht_order * porder, const gs_state * pgs,
		gs_screen_halftone * phsp, bool accurate, gs_memory_t * mem)
{
    gs_matrix imat;
    ulong max_size = pgs->ht_cache->bits_size;
    uint num_levels;
    int code;

    if (phsp->frequency < 0.1)
	return_error(gs_error_rangecheck);
    gs_deviceinitialmatrix(gs_currentdevice(pgs), &imat);
    code = pick_cell_size(phsp, &imat, max_size,
			  screen_min_screen_levels, accurate,
			  &porder->params);
    if (code < 0)
	return code;
    gx_compute_cell_values(&porder->params);
    num_levels = porder->params.W * porder->params.D;
#if !FORCE_STRIP_HALFTONES
    if (((ulong)porder->params.W1 * bitmap_raster(porder->params.W) +
	 num_levels * sizeof(*porder->levels) +
	 porder->params.W * porder->params.W1 * sizeof(*porder->bits)) <=
	max_size) {
	/*
	 * Allocate an order for the entire tile, but only sample one
	 * strip.  Note that this causes the order parameters to be
	 * self-inconsistent until gx_ht_construct_spot_order fixes them
	 * up: see gxdht.h for more information.
	 */
	code = gx_ht_alloc_order(porder, porder->params.W,
				 porder->params.W1, 0,
				 num_levels, mem);
	porder->height = porder->orig_height = porder->params.D;
	porder->shift = porder->orig_shift = porder->params.S;
    } else
#endif
    {	/* Just allocate the order for a single strip. */
	code = gx_ht_alloc_order(porder, porder->params.W,
				 porder->params.D, porder->params.S,
				 num_levels, mem);
    }
    if (code < 0)
	return code;
    return 0;
}

/*
 * Given a desired frequency, angle, and minimum number of levels, a maximum
 * cell size, and an AccurateScreens flag, pick values for M('), N('), and
 * R(').  We want to get a good fit to the requested frequency and angle,
 * provide at least the requested minimum number of levels, and keep
 * rendering as fast as possible; trading these criteria off against each
 * other is what makes the code complicated.
 *
 * We compute trial values u and v from the original values of F and A.
 * Normally these will not be integers.  We then examine the 4 pairs of
 * integers obtained by rounding each of u and v independently up or down,
 * and pick the pair U, V that yields the closest match to the requested
 * F and A values and doesn't require more than max_size storage for a
 * single tile.  If no pair
 * yields an acceptably small W, we divide both u and v by 2 and try again.
 * Then we run the equations backward to obtain the actual F and A.
 * This is fairly easy given that we require either xx = yy = 0 or
 * xy = yx = 0.  In the former case, we have
 *      U = (72 / F * xx) * cos(A);
 *      V = (72 / F * yy) * sin(A);
 * from which immediately
 *      A = arctan((V / yy) / (U / xx)),
 * or equivalently
 *      A = arctan((V * xx) / (U * yy)).
 * We can then obtain F as
 *      F = (72 * xx / U) * cos(A),
 * or equivalently
 *      F = (72 * yy / V) * sin(A).
 * For landscape devices, we replace xx by yx, yy by xy, and interchange
 * sin and cos, resulting in
 *      A = arctan((U * xy) / (V * yx))
 * and
 *      F = (72 * yx / U) * sin(A)
 * or
 *      F = (72 * xy / V) * cos(A).
 */
/* ph->frequency and ph->angle are input parameters; */
/* the routine sets ph->actual_frequency and ph->actual_angle. */
private int
pick_cell_size(gs_screen_halftone * ph, const gs_matrix * pmat, ulong max_size,
	       uint min_levels, bool accurate, gx_ht_cell_params_t * phcp)
{
    const bool landscape = (pmat->xy != 0.0 || pmat->yx != 0.0);

    /* Account for a possibly reflected coordinate system. */
    /* See gxstroke.c for the algorithm. */
    const bool reflected = pmat->xy * pmat->yx > pmat->xx * pmat->yy;
    const int reflection = (reflected ? -1 : 1);
    const int rotation =
    (landscape ? (pmat->yx < 0 ? 90 : -90) : pmat->xx < 0 ? 180 : 0);
    const double f0 = ph->frequency, a0 = ph->angle;
    const double T =
    fabs((landscape ? pmat->yx / pmat->xy : pmat->xx / pmat->yy));
    gs_point uv0;

#define u0 uv0.x
#define v0 uv0.y
    int rt = 1;
    double f = 0, a = 0;
    double e_best = 1000;
    bool better;

    /*
     * We need to find a vector in device space whose length is
     * 1 inch / ph->frequency and whose angle is ph->angle.
     * Because device pixels may not be square, we can't simply
     * map the length to device space and then rotate it;
     * instead, since we know that user space is uniform in X and Y,
     * we calculate the correct angle in user space before rotation.
     */

    /* Compute trial values of u and v. */

    {
	gs_matrix rmat;

	gs_make_rotation(a0 * reflection + rotation, &rmat);
	gs_distance_transform(72.0 / f0, 0.0, &rmat, &uv0);
	gs_distance_transform(u0, v0, pmat, &uv0);
	if_debug10('h', "[h]Requested: f=%g a=%g mat=[%g %g %g %g] max_size=%lu min_levels=%u =>\n     u=%g v=%g\n",
		   ph->frequency, ph->angle,
		   pmat->xx, pmat->xy, pmat->yx, pmat->yy,
		   max_size, min_levels, u0, v0);
    }

    /* Adjust u and v to reasonable values. */

    if (u0 == 0 && v0 == 0)
	return_error(gs_error_rangecheck);
    while ((fabs(u0) + fabs(v0)) * rt < 4)
	++rt;
  try_size:
    better = false;
    {
	int m0 = (int)floor(u0 * rt + 0.0001);
	int n0 = (int)floor(v0 * rt + 0.0001);
	gx_ht_cell_params_t p;

	p.R = p.R1 = rt;
	for (p.M = m0 + 1; p.M >= m0; p.M--)
	    for (p.N = n0 + 1; p.N >= n0; p.N--) {
		long raster, wt, wt_size;
		double fr, ar, ft, at, f_diff, a_diff, f_err, a_err;

		p.M1 = (int)floor(p.M / T + 0.5);
		p.N1 = (int)floor(p.N * T + 0.5);
		gx_compute_cell_values(&p);
		if_debug3('h', "[h]trying m=%d, n=%d, r=%d\n", p.M, p.N, rt);
		wt = p.W;
		if (wt >= max_short)
		    continue;
		/* Check the strip size, not the full tile size, */
		/* against max_size. */
		raster = bitmap_raster(wt);
		if (raster > max_size / p.D || raster > max_long / wt)
		    continue;
		wt_size = raster * wt;

		/* Compute the corresponding values of F and A. */

		if (landscape)
		    ar = atan2(p.M * pmat->xy, p.N * pmat->yx),
			fr = 72.0 * (p.M == 0 ? pmat->xy / p.N * cos(ar) :
				     pmat->yx / p.M * sin(ar));
		else
		    ar = atan2(p.N * pmat->xx, p.M * pmat->yy),
			fr = 72.0 * (p.M == 0 ? pmat->yy / p.N * sin(ar) :
				     pmat->xx / p.M * cos(ar));
		ft = fabs(fr) * rt;
		/* Normalize the angle to the requested quadrant. */
		at = (ar * radians_to_degrees - rotation) * reflection;
		at -= floor(at / 180.0) * 180.0;
		at += floor(a0 / 180.0) * 180.0;
		f_diff = fabs(ft - f0);
		a_diff = fabs(at - a0);
		f_err = f_diff / fabs(f0);
		/*
		 * We used to compute the percentage difference here:
		 *      a_err = (a0 == 0 ? a_diff : a_diff / fabs(a0));
		 * but using the angle difference makes more sense:
		 */
		a_err = a_diff;

		if_debug5('h', " ==> d=%d, wt=%ld, wt_size=%ld, f=%g, a=%g\n",
			  p.D, wt, bitmap_raster(wt) * wt, ft, at);

		/*
		 * Minimize angle and frequency error within the
		 * permitted maximum super-cell size.
		 */

		{
		    double err = f_err * a_err;

		    if (err > e_best)
			continue;
		    e_best = err;
		}
		*phcp = p;
		f = ft, a = at;
		better = true;
		if_debug3('h', "*** best wt_size=%ld, f_diff=%g, a_diff=%g\n",
			  wt_size, f_diff, a_diff);
		if (f_err <= 0.01 && a_err <= 0.01)
		    goto done;
	    }
    }
    if (phcp->C < min_levels) {	/* We don't have enough levels yet.  Keep going. */
	++rt;
	goto try_size;
    }
    if (better) {		/* If we want accurate screens, continue till we fail. */
	if (accurate) {
	    ++rt;
	    goto try_size;
	}
    } else {			/*
				 * We couldn't find an acceptable M and N.  If R > 1,
				 * take what we've got; if R = 1, give up.
				 */
	if (rt == 1)
	    return_error(gs_error_rangecheck);
    }

    /* Deliver the results. */
  done:
    if_debug5('h', "[h]Chosen: f=%g a=%g M=%d N=%d R=%d\n",
	      f, a, phcp->M, phcp->N, phcp->R);
    ph->actual_frequency = f;
    ph->actual_angle = a;
    return 0;
#undef u0
#undef v0
}

/* Prepare to sample a spot screen. */
/* This is the second half of gs_screen_init_accurate. */
int
gs_screen_enum_init_memory(gs_screen_enum * penum, const gx_ht_order * porder,
	       gs_state * pgs, gs_screen_halftone * phsp, gs_memory_t * mem)
{
    penum->pgs = pgs;		/* ensure clean for GC */
    penum->order = *porder;
    penum->halftone.rc.memory = mem;
    penum->halftone.type = ht_type_screen;
    penum->halftone.params.screen = *phsp;
    penum->x = penum->y = 0;
    penum->strip = porder->num_levels / porder->width;
    penum->shift = porder->shift;
    /*
     * We want a transformation matrix that maps the parallelogram
     * (0,0), (U,V), (U-V',V+U'), (-V',U') to the square (+/-1, +/-1).
     * If the coefficients are [a b c d e f] and we let
     *      u = U = M/R, v = V = N/R,
     *      r = -V' = -N'/R', s = U' = M'/R',
     * then we just need to solve the equations:
     *      a*0 + c*0 + e = -1      b*0 + d*0 + f = -1
     *      a*u + c*v + e = 1       b*u + d*v + f = 1
     *      a*r + c*s + e = -1      b*r + d*s + f = 1
     * This has the following solution:
     *      Q = 2 / (M*M' + N*N')
     *      a = Q * R * M'
     *      b = -Q * R' * N
     *      c = Q * R * N'
     *      d = Q * R' * M
     *      e = -1
     *      f = -1
     */
    {
	const int M = porder->params.M, N = porder->params.N, R = porder->params.R;
	const int M1 = porder->params.M1, N1 = porder->params.N1, R1 = porder->params.R1;
	double Q = 2.0 / ((long)M * M1 + (long)N * N1);

	penum->mat.xx = Q * (R * M1);
	penum->mat.xy = Q * (-R1 * N);
	penum->mat.yx = Q * (R * N1);
	penum->mat.yy = Q * (R1 * M);
	penum->mat.tx = -1.0;
	penum->mat.ty = -1.0;
    }
    if_debug7('h', "[h]Screen: (%dx%d)/%d [%f %f %f %f]\n",
	      porder->width, porder->height, porder->params.R,
	      penum->mat.xx, penum->mat.xy,
	      penum->mat.yx, penum->mat.yy);
    return 0;
}

/* Report current point for sampling */
int
gs_screen_currentpoint(gs_screen_enum * penum, gs_point * ppt)
{
    gs_point pt;
    int code;

    if (penum->y >= penum->strip) {	/* all done */
	gx_ht_construct_spot_order(&penum->order);
	return 1;
    }
    /* We displace the sampled coordinates very slightly */
    /* in order to reduce the likely number of points */
    /* for which the spot function returns the same value. */
    if ((code = gs_point_transform(penum->x + 0.501, penum->y + 0.498, &penum->mat, &pt)) < 0)
	return code;
    if (pt.x < -1.0)
	pt.x += ((int)(-ceil(pt.x)) + 1) & ~1;
    else if (pt.x >= 1.0)
	pt.x -= ((int)pt.x + 1) & ~1;
    if (pt.y < -1.0)
	pt.y += ((int)(-ceil(pt.y)) + 1) & ~1;
    else if (pt.y >= 1.0)
	pt.y -= ((int)pt.y + 1) & ~1;
    *ppt = pt;
    return 0;
}

/* Record next halftone sample */
int
gs_screen_next(gs_screen_enum * penum, floatp value)
{
    ht_sample_t sample;
    int width = penum->order.width;

    if (value < -1.0 || value > 1.0)
	return_error(gs_error_rangecheck);
    /* The following statement was split into two */
    /* to work around a bug in the Siemens C compiler. */
    sample = (ht_sample_t) (value * max_ht_sample);
    sample += max_ht_sample;	/* convert from signed to biased */
#ifdef DEBUG
    if (gs_debug_c('H')) {
	gs_point pt;

	gs_screen_currentpoint(penum, &pt);
	dlprintf6("[H]sample x=%d y=%d (%f,%f): %f -> %u\n",
		  penum->x, penum->y, pt.x, pt.y, value, sample);
    }
#endif
    penum->order.bits[penum->y * width + penum->x].mask = sample;
    if (++(penum->x) >= width)
	penum->x = 0, ++(penum->y);
    return 0;
}

/* Install a fully constructed screen in the gstate. */
int
gs_screen_install(gs_screen_enum * penum)
{
    gx_device_halftone dev_ht;

    dev_ht.rc.memory = penum->halftone.rc.memory;
    dev_ht.order = penum->order;
    dev_ht.components = 0;
    return gx_ht_install(penum->pgs, &penum->halftone, &dev_ht);
}
