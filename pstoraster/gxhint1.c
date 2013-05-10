/* Copyright (C) 1990, 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gxhint1.c */
/* Font level hints for Type 1 fonts */
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"

/* ------ Initialization ------ */

typedef zone_table(1) a_zone_table;
typedef stem_table(1) a_stem_table;
private void near
  compute_snaps(P6(const gs_matrix_fixed *, const a_stem_table *,
    stem_snap_table *, int, int, const char *));
private alignment_zone *near
  compute_zones(P6(const gs_matrix_fixed *, const font_hints *,
    const a_zone_table *, const a_zone_table *, alignment_zone *, int));
private int near
  transform_zone(P4(const gs_matrix_fixed *, const font_hints *,
    const float *, alignment_zone *));

/* Reset the font-level hints. */
void
reset_font_hints(font_hints *pfh, const gs_log2_scale_point *plog2_scale)
{	set_pixel_scale(&pfh->scale.x, plog2_scale->x);
	set_pixel_scale(&pfh->scale.y, plog2_scale->y);
	pfh->axes_swapped = pfh->x_inverted = pfh->y_inverted = false;
	pfh->use_x_hints = pfh->use_y_hints = false;
	pfh->snap_h.count = pfh->snap_v.count = 0;
	pfh->a_zone_count = 0;
}

/* Compute the font-level hints from the font and the matrix. */
/* We should cache this with the font/matrix pair.... */
void
compute_font_hints(font_hints *pfh, const gs_matrix_fixed *pmat,
  const gs_log2_scale_point *plog2_scale, const gs_type1_data *pdata)
{	alignment_zone *zp = &pfh->a_zones[0];
	reset_font_hints(pfh, plog2_scale);
	/* Figure out which hints, if any, to use, */
	/* and the orientation of the axes. */
	if ( is_fzero(pmat->xy) )
		pfh->y_inverted = is_fneg(pmat->yy),
		pfh->use_y_hints = true;
	else if ( is_fzero(pmat->xx) )
		pfh->y_inverted = is_fneg(pmat->xy),
		pfh->axes_swapped = true,
		pfh->use_y_hints = true;
	if ( is_fzero(pmat->yx) )
		pfh->x_inverted = is_fneg(pmat->xx),
		pfh->use_x_hints = true;
	else if ( is_fzero(pmat->yy) )
		pfh->x_inverted = is_fneg(pmat->yx),
		pfh->axes_swapped = true,
		pfh->use_x_hints = true;
	if_debug6('y', "[y]ctm=[%g %g %g %g %g %g]\n",
		  pmat->xx, pmat->xy, pmat->yx, pmat->yy,
		  pmat->tx, pmat->ty);
	if_debug7('y', "[y]scale=%d/%d, swapped=%d, x/y_hints=%d,%d, x/y_inverted=%d,%d\n",
		  1 << plog2_scale->x, 1 << plog2_scale->y,
		  pfh->axes_swapped, pfh->use_x_hints, pfh->use_y_hints,
		  pfh->x_inverted, pfh->y_inverted);
	/* Transform the actual hints. */
	if ( pfh->use_x_hints )
	{	compute_snaps(pmat, (const a_stem_table *)&pdata->StdHW,
			      &pfh->snap_h, 0, pfh->axes_swapped, "h");
		compute_snaps(pmat, (const a_stem_table *)&pdata->StemSnapH,
			      &pfh->snap_h, 0, pfh->axes_swapped, "h");
	}
	if ( pfh->use_y_hints )
	{	gs_fixed_point vw;
		fixed *vp = (pfh->axes_swapped ? &vw.x : &vw.y);
		pixel_scale *psp =
		  (pfh->axes_swapped ? &pfh->scale.x : &pfh->scale.y);
		/* Convert BlueFuzz to device pixels. */
		if ( gs_distance_transform2fixed(pmat, 0.0,
						 (float)pdata->BlueFuzz,
						 &vw) < 0
		   )
		  vw.x = vw.y = fixed_0;
		pfh->blue_fuzz = any_abs(*vp);
		/*
		 * Decide whether to suppress overshoots.  The formula in
		 * section 5.6 of the "Adobe Type 1 Font Format" says that
		 * at 300 dpi, if BlueScale = (P - 0.49) / 240, overshoot
		 * suppression turns off at point sizes at least P, i.e.:
		 *	P >= BlueScale * 240 + 0.49.
		 * At 300 dpi, P = |CTM.yy| / (300/72), so the condition is
		 * equivalent to
		 *	|CTM.yy| >= BlueScale * 1000 + 2.0417,
		 * or
		 *	BlueScale >= (|CTM.yy| - 2.0417) / 1000.
		 * Since *pmat is the concatenation of the FontMatrix and
		 * CTM, if we assume a 1000-unit scale, this is equivalent to
		 *	BlueScale >= |pmat->yy| - 0.00020417.
		 * Since the constant term is slightly smaller than
		 * fixed_epsilon, we just disregard it.
		 *
		 * According to the same section of the Adobe documentation,
		 * there is a requirement that BlueScale times the maximum
		 * alignment zone height must be less than 1.  We enforced
		 * this when the font was constructed (in zfont1.c).
		 */
		if ( gs_distance_transform2fixed(pmat, 0.0, 1.0, &vw) < 0 )
		  vw.x = vw.y = fixed_0;
		pfh->suppress_overshoot =
		  fixed2float(any_abs(*vp) >> psp->log2_unit) <
		    pdata->BlueScale;
		if ( gs_distance_transform2fixed(pmat, 0.0, pdata->BlueShift,
						 &vw) < 0
		   )
		  vw.x = vw.y = fixed_0;
		pfh->blue_shift = any_abs(*vp);
		/* Tweak up blue_shift if it is less than half a pixel. */
		/* See the discussion of BlueShift in section 5.7 of */
		/* "Adobe Type 1 Font Format." */
		if ( pfh->blue_shift < psp->half )
		  pfh->blue_shift = psp->half;
		if_debug6('y', "[y]blue_fuzz=%d->%g, blue_scale=%g, blue_shift=%g->%g, sup_ov=%d\n",
			  pdata->BlueFuzz, fixed2float(pfh->blue_fuzz),
			  pdata->BlueScale,
			  pdata->BlueShift, fixed2float(pfh->blue_shift),
			  pfh->suppress_overshoot);
		zp = compute_zones(pmat, pfh,
				   (const a_zone_table *)&pdata->BlueValues,
				   (const a_zone_table *)&pdata->FamilyBlues,
				   zp, 1);
		zp = compute_zones(pmat, pfh,
				   (const a_zone_table *)&pdata->OtherBlues,
				   (const a_zone_table *)&pdata->FamilyOtherBlues,
				   zp, max_OtherBlues);
		compute_snaps(pmat, (const a_stem_table *)&pdata->StdVW,
			      &pfh->snap_v, 1, !pfh->axes_swapped, "v");
		compute_snaps(pmat, (const a_stem_table *)&pdata->StemSnapV,
			      &pfh->snap_v, 1, !pfh->axes_swapped, "v");
	}
	pfh->a_zone_count = zp - &pfh->a_zones[0];
}

/* Transform one set of stem snap widths. */
private void near
compute_snaps(const gs_matrix_fixed *pmat, const a_stem_table *pst,
  stem_snap_table *psst, int from_y, int to_y, const char *tname)
{	gs_fixed_point wxy;
	fixed *wp = (to_y ? &wxy.y : &wxy.x);
	int i;
	int j = psst->count;
	for ( i = 0; i < pst->count; i++ )
	{	float w = pst->values[i];
		int code =
		  (from_y ?
		   gs_distance_transform2fixed(pmat, 0.0, w, &wxy) :
		   gs_distance_transform2fixed(pmat, w, 0.0, &wxy)
		  );
		if ( code < 0 )
		  continue;
		psst->data[j] = any_abs(*wp);
		if_debug3('y', "[y]snap_%s[%d]=%g\n", tname, j,
			  fixed2float(psst->data[j]));
		j++;
	}
	psst->count = j;
}

/* Compute the alignment zones for one set of 'blue' values. */
private alignment_zone *near
compute_zones(const gs_matrix_fixed *pmat, const font_hints *pfh,
  const a_zone_table *blues, const a_zone_table *family_blues,
  alignment_zone *zp, int bottom_count)
{	int i;
	fixed fuzz = pfh->blue_fuzz;
	int inverted =
		(pfh->axes_swapped ? pfh->x_inverted : pfh->y_inverted);
	for ( i = 0; i < blues->count; i += 2 )
	{	const float *vp = &blues->values[i];
		zp->is_top_zone = i >> 1 >= bottom_count;
		if ( transform_zone(pmat, pfh, vp, zp) < 0 )
		  continue;
		if_debug5('y', "[y]blues[%d]=%g,%g -> %g,%g\n",
			  i >> 1, vp[0], vp[1],
			  fixed2float(zp->v0), fixed2float(zp->v1));
		if ( i < family_blues->count )
		{	/* Check whether family blues should supersede. */
			alignment_zone fz;
			const float *fvp = &family_blues->values[i];
			fixed unit = (pfh->axes_swapped ?
				pfh->scale.x.unit : pfh->scale.y.unit);
			fixed diff;
			if ( transform_zone(pmat, pfh, fvp, &fz) < 0 )
			  continue;
			if_debug5('y', "[y]f_blues[%d]=%g,%g -> %g,%g\n",
				  i >> 1, fvp[0], fvp[1],
				  fixed2float(fz.v0), fixed2float(fz.v1));
			diff = (zp->v1 - zp->v0) - (fz.v1 - fz.v0);
			if ( diff > -unit && diff < unit )
				zp->v0 = fz.v0, zp->v1 = fz.v1;
		}
		/* Compute the flat position, and add the fuzz. */
		if ( (inverted ? zp->is_top_zone : !zp->is_top_zone) )
			zp->flat = zp->v1, zp->v0 -= fuzz;
		else
			zp->flat = zp->v0, zp->v1 += fuzz;
		zp++;
	}
	return zp;
}

/* Transform a single alignment zone to device coordinates, */
/* taking axis swapping into account. */
private int near
transform_zone(const gs_matrix_fixed *pmat, const font_hints *pfh,
  const float *vp, alignment_zone *zp)
{	gs_fixed_point p0, p1;
	fixed v0, v1;
	int code;
	if ( (code = gs_point_transform2fixed(pmat, 0.0, vp[0], &p0)) < 0 ||
	     (code = gs_point_transform2fixed(pmat, 0.0, vp[1], &p1)) < 0
	   )
	  return code;
	if ( pfh->axes_swapped ) v0 = p0.x, v1 = p1.x;
	else v0 = p0.y, v1 = p1.y;
	if ( v0 <= v1 ) zp->v0 = v0, zp->v1 = v1;
	else zp->v0 = v1, zp->v1 = v0;
	return 0;
}
