/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gshtscr.c */
/* Screen (Type 1) halftone processing for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gzstate.h"
#include "gxdevice.h"			/* for gzht.h */
#include "gzht.h"

/* Structure descriptors */
private_st_gs_screen_enum();

/* GC procedures */
#define eptr ((gs_screen_enum *)vptr)

private ENUM_PTRS_BEGIN(screen_enum_enum_ptrs) {
	if ( index < 2 + st_ht_order_max_ptrs )
	  { gs_ptr_type_t ret = (*st_ht_order.enum_ptrs)(&eptr->order, sizeof(eptr->order), index-2, pep);
	    if ( ret == 0 )	/* don't stop early */
	      ret = ptr_struct_type, *pep = 0;
	    return ret;
	  }
	return (*st_halftone.enum_ptrs)(&eptr->halftone, sizeof(eptr->halftone), index-(2+st_ht_order_max_ptrs), pep);
	}
	ENUM_PTR(0, gs_screen_enum, pgs);
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(screen_enum_reloc_ptrs) {
	RELOC_PTR(gs_screen_enum, pgs);
	(*st_halftone.reloc_ptrs)(&eptr->halftone, sizeof(gs_halftone), gcst);
	(*st_ht_order.reloc_ptrs)(&eptr->order, sizeof(gx_ht_order), gcst);
} RELOC_PTRS_END

#undef eptr

/* Define the default value of AccurateScreens that affects */
/* setscreen and setcolorscreen. */
private bool screen_accurate_screens = false;

/* Default AccurateScreens control */
void
gs_setaccuratescreens(bool accurate)
{	screen_accurate_screens = accurate;
}
bool
gs_currentaccuratescreens(void)
{	return screen_accurate_screens;
}

/*
 * We construct a halftone tile as a super-cell consisting of multiple
 * copies of a multi-cell whose corners lie on integral coordinates and
 * which in turn is a square array of basic cells whose corners lie on
 * rational coordinates.
 *
 * Let T be the aspect ratio (ratio of physical pixel height to physical
 * pixel width), which is abs(xx/yy) for portrait devices and abs(yx/xy) for
 * landscape devices.  We characterize the basic cell by four rational
 * numbers U(') = M(')/R and V(') = N(')/R where R is positive, at least one
 * of U and V (and the corresponding one of V' and U') is non-zero, and U'
 * is approximately U/T and V' is approximately V*T; these numbers define
 * the vertices of the basic cell at device space coordinates (0,0), (U,V),
 * (U-V',U'+V), and (-V',U'); then the multi-cell is defined similarly by
 * M(') and N(').  R > 1 is only possible if AccurateScreens is true; if
 * AccurateScreens is false, multi-cells and basic cells are the same.  From
 * these definitions, the basic cell has an area of B = U*U' + V*V' = (M*M'
 * + N*N') / R^2 pixels, and the multi-cell has an area of C = B * R^2 =
 * M*M' + N*N' pixels.
 *
 * If the coefficients of the default device transformation matrix are xx,
 * xy, yx, and yy, then U and V are related to the frequency F and the angle
 * A by:
 *	P = 72 / F;
 *	U = P * (xx * cos(A) + yx * sin(A));
 *	V = P * (xy * cos(A) + yy * sin(A)).
 *
 * We can tile the plane with any rectangular super-cell that consists of
 * repetitions of the multi-cell and whose corners coincide with multi-cell
 * coordinates (0,0).  To determine the number of repetitions, we must find
 * the smallest (absolute value) coordinates where the corners of
 * multi-cells lie on the coordinate axes.  We observe that for any integers
 * i, j such that i*N - j*M' = 0, a multi-cell corner lies on the X axis at
 * W = i*M + j*N'; similarly, if i'*M - j'*N' = 0, a corner lies on the Y
 * axis at W' = i'*N + j'*M'.  We can compute the values of i and j that
 * give the minimum value of W by
 *	D = gcd(abs(M'), abs(N)), i = M'/D, j = N/D, W = C / D
 * and similarly
 *	D' = gcd(abs(M), abs(N')), i' = N'/D', j' = M/D', W' = C / D'
 * Then the super-cell occupies Z = W * W' pixels, consisting of Z / C
 * multi-cells or Z / B basic cells.  The trick in all this is to find
 * values of F and A that aren't too far from the requested ones, and that
 * yield a manageably small value for Z.
 *
 * Note that the super-cell only has to be so large because we want to use
 * it directly to tile the plane.  In fact, we can decompose it into W' / D
 * horizontal strips of width W and height D, shifted horizontally with
 * respect to each other by S pixels, where we compute S by finding h and k
 * such that h*N - k*M' = D and then S = h*M + k*N'.  The routines here only
 * generate a single strip of samples, and let gx_ht_construct_spot_order
 * construct the rest.  If W' is large, we actually keep only one strip, and
 * let the strip_tile_rectangle routines do the shifting at rendering time.
 *
 * **** NOTE: the current algorithms assume T = 1, U' = U, V' = V.
 * We are in the process of fixing this.
 */

/* Forward references */
private void pick_cell_size(P7(gs_screen_halftone *ph,
  const gs_matrix *pmat, long max_size, bool accurate,
  gs_int_point *pMN, int *pR, gs_int_point *ptile));

/* Allocate a screen enumerator. */
gs_screen_enum *
gs_screen_enum_alloc(gs_memory_t *mem, client_name_t cname)
{	return gs_alloc_struct(mem, gs_screen_enum, &st_gs_screen_enum, cname);
}

/* Set up for halftone sampling. */
int
gs_screen_init(gs_screen_enum *penum, gs_state *pgs,
  gs_screen_halftone *phsp)
{	return gs_screen_init_accurate(penum, pgs, phsp,
				       screen_accurate_screens);
}
int
gs_screen_init_accurate(gs_screen_enum *penum, gs_state *pgs,
  gs_screen_halftone *phsp, bool accurate)
{	int code = gs_screen_order_init(&penum->order, pgs, phsp, accurate);
	if ( code < 0 )
		return code;
	return gs_screen_enum_init(penum, &penum->order, pgs, phsp);
}

/* Allocate and initialize a spot screen. */
/* This is the first half of gs_screen_init_accurate. */
int
gs_screen_order_init(gx_ht_order *porder, const gs_state *pgs,
  gs_screen_halftone *phsp, bool accurate)
{	gs_matrix imat;
	long max_size;
	gs_int_point MN, tile;
	uint shift;
	int code;
	int R, D;
	uint wd;

	if ( phsp->frequency < 0.1 )
	  return_error(gs_error_rangecheck);
	gs_deviceinitialmatrix(gs_currentdevice(pgs), &imat);
	max_size = (accurate ? max_ushort : 512);
	pick_cell_size(phsp, &imat, max_size, accurate, &MN, &R, &tile);
#define M MN.x
#define N MN.y
#define W tile.x
	D = igcd(M, N);
	wd = W / D;
	{	/* Unfortunately, we don't know any closed formula for */
		/* computing the shift, so we do it by enumeration. */
		uint K;
		uint vk = (N < 0 ? N + W : N) / D;

		for ( K = 0; K < wd; K++ )
		  {	if ( (K * vk) % wd == 1 )
			  break;
		  }
		shift = ((((M < 0 ? M + W : M) / D) * K) % wd) * D;
		if_debug2('h', "[h]strip=%d shift=%d\n", D, shift);
		/* We just computed what amounts to a right shift; */
		/* what we want is a left shift. */
		if ( shift != 0 )
		  shift = W - shift;
	}
	/* NOTE: patching the next line to 'false' forces all screens */
	/* to be strip halftones, even if they are small. */
	if ( tile.y <= max_size / bitmap_raster(tile.x) )
	  { /*
	     * Allocate an order for the entire tile, but only sample one
	     * strip.  Note that this causes the order parameters to be
	     * self-inconsistent until gx_ht_construct_spot_order fixes them
	     * up: see gxdht.h for more information.
	     */
	    code = gx_ht_alloc_order(porder, tile.x, tile.y, 0,
				     W * D, pgs->memory);
	    porder->height = porder->orig_height = D;
	    porder->shift = porder->orig_shift = shift;
	  }
	else
	  { /* Just allocate the order for a single strip. */
	    code = gx_ht_alloc_order(porder, tile.x, D, shift,
				     W * D, pgs->memory);
	  }
	if ( code < 0 )
	  return code;
	porder->multiple = R;
#undef M
#undef N
#undef W
	return 0;
}

/*
 * The following routine looks for "good" values of U and V
 * in a simple-minded way, according to the following algorithm:
 * We compute trial values u and v from the original values of F and A.
 * Normally these will not be integers.  We then examine the 4 pairs of
 * integers obtained by rounding each of u and v independently up or down,
 * and pick the pair U, V that yields the smallest value of W. If no pair
 * yields an acceptably small W, we divide both u and v by 2 and try again.
 * Then we run the equations backward to obtain the actual F and A.
 * This is fairly easy given that we require either xx = yy = 0 or
 * xy = yx = 0.  In the former case, we have
 *	U = (72 / F * xx) * cos(A);
 *	V = (72 / F * yy) * sin(A);
 * from which immediately
 *	A = arctan((V / yy) / (U / xx)),
 * or equivalently
 *	A = arctan((V * xx) / (U * yy)).
 * We can then obtain F as
 *	F = (72 * xx / U) * cos(A),
 * or equivalently
 *	F = (72 * yy / V) * sin(A).
 * For landscape devices, we replace xx by yx, yy by xy, and interchange
 * sin and cos, resulting in
 *	A = arctan((U * xy) / (V * yx))
 * and
 *	F = (72 * yx / U) * sin(A)
 * or
 *	F = (72 * xy / V) * cos(A).
 */
/* ph->frequency and ph->angle are input parameters; */
/* the routine sets ph->actual_frequency and ph->actual_angle. */
private void
pick_cell_size(gs_screen_halftone *ph, const gs_matrix *pmat, long max_size,
  bool accurate, gs_int_point *pMN, int *pR, gs_int_point *ptile)
{	bool landscape = (pmat->xy != 0.0 || pmat->yx != 0.0);
	/* Account for a possibly reflected coordinate system. */
	/* See gxstroke.c for the algorithm. */
	bool reflected =
	  (landscape ? pmat->xy * pmat->yx > pmat->xx * pmat->yy :
	   (pmat->xx < 0) != (pmat->yy < 0));
	int reflection = (reflected ? -1 : 1);
	int rotation =
	  (landscape ? (pmat->yx < 0 ? 90 : -90) : pmat->xx < 0 ? 180 : 0);
	const double f0 = ph->frequency, a0 = ph->angle;
	gs_matrix rmat;
	gs_point t;
	double u0, v0;
	int m, n, r, rt = 1, w;
	double f = 0, a = 0;
	double f_best = max_int, a_best = 360, e_best = 1000;
	long w_size = max_size;
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

	gs_make_rotation(a0 * reflection + rotation, &rmat);
	gs_distance_transform(72.0 / f0, 0.0, &rmat, &t);
	gs_distance_transform(t.x, t.y, pmat, &t);
	if_debug9('h', "[h]Requested: f=%g a=%g mat=[%g %g %g %g] max_size=%ld =>\n     u=%g v=%g\n",
		  ph->frequency, ph->angle,
		  pmat->xx, pmat->xy, pmat->yx, pmat->yy, max_size,
		  t.x, t.y);

	/* Adjust u and v to reasonable values. */

	u0 = t.x;
	v0 = t.y;
	if ( u0 == 0 && v0 == 0 )
	  u0 = v0 = 1;
	while ( fabs(u0) + fabs(v0) < 4 )
	  u0 *= 2, v0 *= 2;
try_size:
	better = false;
	{ int m0 = (int)floor(u0 * rt + 0.0001);
	  int n0 = (int)floor(v0 * rt + 0.0001);
	  int mt, nt;

	  for ( mt = m0 + 1; mt >= m0; mt-- )
	    for ( nt = n0 + 1; nt >= n0; nt-- )
	      {	int d = igcd(mt, nt);
		long mtd = mt / d * (long)mt, ntd = nt / d * (long)nt;
		long wt = any_abs(mtd) + any_abs(ntd);
		long raster;
		long wt_size;
		double fr, ar, ft, at, f_diff, a_diff, f_err, a_err;

		if_debug3('h', "[h]trying m=%d, n=%d, r=%d\n", mt, nt, rt);
		if ( wt >= max_short )
		  continue;
		/* Check the strip size, not the full tile size, */
		/* against max_size. */
		raster = bitmap_raster(wt);
		if ( raster > max_size / d || raster > max_long / wt )
		  continue;
		wt_size = raster * wt;

		/* Compute the corresponding values of F and A. */

		if ( landscape )
		  ar = atan2(mt * pmat->xy, nt * pmat->yx),
		  fr = 72.0 * (mt == 0 ? pmat->xy / nt * cos(ar) :
			       pmat->yx / mt * sin(ar));
		else
		  ar = atan2(nt * pmat->xx, mt * pmat->yy),
		  fr = 72.0 * (mt == 0 ? pmat->yy / nt * sin(ar) :
			       pmat->xx / mt * cos(ar));
		ft = fabs(fr) * rt;
		/****** FOLLOWING IS WRONG IF NON-SQUARE PIXELS ******/
		/* Normalize the angle to the requested quadrant. */
		at = (ar * radians_to_degrees - rotation) * reflection;
		at -= floor(at / 180.0) * 180.0;
		at += floor(a0 / 180.0) * 180.0;
		f_diff = fabs(ft - f0);
		a_diff = fabs(at - a0);
		f_err = f_diff / fabs(f0);
		a_err = (a0 == 0 ? a_diff : a_diff / fabs(a0));

		if_debug5('h', " ==> d=%d, wt=%ld, wt_size=%ld, f=%g, a=%g\n",
			  d, wt, bitmap_raster(wt) * wt, ft, at);

		/*
		 * If AccurateScreens is true, minimize angle and frequency
		 * error within the permitted maximum super-cell size;
		 * if AccurateScreens is false, minimize cell size.
		 */

		if ( accurate )
		  { /* Minimize percentage error. */
		    double err = f_err * a_err;
		    if ( err > e_best )
		      continue;
		    e_best = err;
		  }
		else
		  { /* Minimize cell size. */
		    if ( wt_size >= w_size )
		      continue;
		  }
		m = mt, n = nt, r = rt, w = wt, f = ft, a = at;
		w_size = wt_size, f_best = f_diff, a_best = a_diff;
		better = true;
		if_debug3('h', "*** w_size=%ld, f_best=%g, a_best=%g\n",
			  w_size, f_best, a_best);
		if ( f_err <= 0.01 && a_err <= 0.01 )
		  goto done;
	      }
	}
	if ( better )
	  {	/* If we want accurate screens, continue till we fail. */
		if ( accurate )
		  { ++rt;
		    goto try_size;
		  }
	  }
	else
	  {	/*
		 * We couldn't find an acceptable M and N.  If R > 1, quit;
		 * if R = 1, shrink the cell and try again.
		 */
		if ( rt == 1 )
		  { u0 /= 2;
		    v0 /= 2;
		    goto try_size;
		  }
	  }

	/* Deliver the results. */
done:
	if_debug6('h', "[h]Chosen: f=%g a=%g m=%d n=%d r=%d w=%d\n",
		  f, a, m, n, r, w);
	pMN->x = m, pMN->y = n;
	*pR = r;
	ptile->x = w, ptile->y = w;
	ph->actual_frequency = f;
	ph->actual_angle = a;
}

/* Prepare to sample a spot screen. */
/* This is the second half of gs_screen_init_accurate. */
int
gs_screen_enum_init(gs_screen_enum *penum, const gx_ht_order *porder,
  gs_state *pgs, gs_screen_halftone *phsp)
{	uint W = porder->width;
	uint D = porder->num_levels / W;

	penum->pgs = pgs;		/* ensure clean for GC */
	penum->order = *porder;
	penum->halftone.type = ht_type_screen;
	penum->halftone.params.screen = *phsp;
	penum->x = penum->y = 0;
	penum->strip = D;
	penum->shift = porder->shift;
	/* The transformation matrix must include normalization to the */
	/* interval (-1..1), and rotation by the negative of the angle. */
	/****** WRONG IF NON-SQUARE PIXELS ******/
	{	float xscale = 2.0 / sqrt((double)W * D) * porder->multiple;
		float yscale = xscale;
		gs_make_rotation(-phsp->actual_angle, &penum->mat);
		penum->mat.xx *= xscale, penum->mat.xy *= xscale;
		penum->mat.yx *= yscale, penum->mat.yy *= yscale;
		penum->mat.tx = -1.0;
		penum->mat.ty = -1.0;
		if_debug7('h', "[h]Screen: (%dx%d)/%d [%f %f %f %f]\n",
			  porder->width, porder->height, porder->multiple,
			  penum->mat.xx, penum->mat.xy,
			  penum->mat.yx, penum->mat.yy);
	}
	return 0;
}

/* Report current point for sampling */
int
gs_screen_currentpoint(gs_screen_enum *penum, gs_point *ppt)
{	gs_point pt;
	int code;
	if ( penum->y >= penum->strip )		/* all done */
	{	gx_ht_construct_spot_order(&penum->order);
		return 1;
	}
	/* We displace the sampled coordinates very slightly */
	/* in order to reduce the likely number of points */
	/* for which the spot function returns the same value. */
	if ( (code = gs_point_transform(penum->x + 0.501, penum->y + 0.498, &penum->mat, &pt)) < 0 )
		return code;
	if ( pt.x < -1.0 )
		pt.x += ((int)(-ceil(pt.x)) + 1) & ~1;
	else if ( pt.x >= 1.0 )
		pt.x -= ((int)pt.x + 1) & ~1;
	if ( pt.y < -1.0 )
		pt.y += ((int)(-ceil(pt.y)) + 1) & ~1;
	else if ( pt.y >= 1.0 )
		pt.y -= ((int)pt.y + 1) & ~1;
	*ppt = pt;
	return 0;
}

/* Record next halftone sample */
int
gs_screen_next(gs_screen_enum *penum, floatp value)
{	ht_sample_t sample;
	int width = penum->order.width;
	if ( value < -1.0 || value > 1.0 )
		return_error(gs_error_rangecheck);
	/* The following statement was split into two */
	/* to work around a bug in the Siemens C compiler. */
	sample = (ht_sample_t)(value * max_ht_sample);
	sample += max_ht_sample;	/* convert from signed to biased */
#ifdef DEBUG
if ( gs_debug_c('H') )
   {	gs_point pt;
	gs_screen_currentpoint(penum, &pt);
	dprintf6("[H]sample x=%d y=%d (%f,%f): %f -> %u\n",
		 penum->x, penum->y, pt.x, pt.y, value, sample);
   }
#endif
	penum->order.bits[penum->y * width + penum->x].mask = sample;
	if ( ++(penum->x) >= width )
		penum->x = 0, ++(penum->y);
	return 0;
}

/* Install a fully constructed screen in the gstate. */
int
gs_screen_install(gs_screen_enum *penum)
{	gx_device_halftone dev_ht;
	dev_ht.order = penum->order;
	dev_ht.components = 0;
	return gx_ht_install(penum->pgs, &penum->halftone, &dev_ht);
}
