/* Copyright (C) 1992, 1995, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxdcconv.c,v 1.2 2000/03/08 23:14:56 mike Exp $ */
/* Conversion between device color spaces for Ghostscript */
#include "gx.h"
#include "gsdcolor.h"		/* for gxcmap.h */
#include "gxdcconv.h"		/* interface */
#include "gxdevice.h"		/* for gxcmap.h */
#include "gxcmap.h"
#include "gxfarith.h"
#include "gxlum.h"
#include "gxistate.h"

/*
 * The CMYK to RGB algorithms specified by Adobe are, e.g.,
 *      R = 1.0 - min(1.0, C + K)
 *      C = max(0.0, min(1.0, 1 - R - UCR))
 * but we get much better results with
 *      R = (1.0 - C) * (1.0 - K)
 *      C = max(0.0, min(1.0, 1 - R / (1 - UCR)))
 * For utmost compatibility, we offer the Adobe algorithms as an option:
 */
#define USE_ADOBE_CMYK_RGB

/* ------ Color space conversion ------ */

/* Only 4 of the 6 conversions are implemented here; */
/* the other 2 (Gray to RGB/CMYK) are trivial. */

/* Convert RGB to Gray. */
frac
color_rgb_to_gray(frac r, frac g, frac b, const gs_imager_state * pis)
{
    return (r * (unsigned long)lum_red_weight +
	    g * (unsigned long)lum_green_weight +
	    b * (unsigned long)lum_blue_weight +
	    (lum_all_weights / 2))
	/ lum_all_weights;
}

/* Convert RGB to CMYK. */
/* Note that this involves black generation and undercolor removal. */
void
color_rgb_to_cmyk(frac r, frac g, frac b, const gs_imager_state * pis,
		  frac cmyk[4])
{
    frac c = frac_1 - r, m = frac_1 - g, y = frac_1 - b;
    frac k = (c < m ? min(c, y) : min(m, y));

    /* The default UCR and BG functions are pretty arbitrary.... */
    frac bg =
    (pis->black_generation == NULL ? frac_0 :
     gx_map_color_frac(pis, k, black_generation));
    signed_frac ucr =
    (pis->undercolor_removal == NULL ? frac_0 :
     gx_map_color_frac(pis, k, undercolor_removal));

    if (ucr == frac_1)
	cmyk[0] = cmyk[1] = cmyk[2] = 0;
    else {
#ifdef USE_ADOBE_CMYK_RGB
	/* C = max(0.0, min(1.0, 1 - R - UCR)), etc. */
	signed_frac not_ucr = (ucr < 0 ? frac_1 + ucr : frac_1);

	cmyk[0] = (c < ucr ? frac_0 : c > not_ucr ? frac_1 : c - ucr);
	cmyk[1] = (m < ucr ? frac_0 : m > not_ucr ? frac_1 : m - ucr);
	cmyk[2] = (y < ucr ? frac_0 : y > not_ucr ? frac_1 : y - ucr);
#else
	/* C = max(0.0, min(1.0, 1 - R / (1 - UCR))), etc. */
	float denom = frac2float(frac_1 - ucr);		/* unscaled */
	float v;

	v = (float)frac_1 - r / denom;	/* unscaled */
	cmyk[0] =
	    (is_fneg(v) ? frac_0 : v >= (float)frac_1 ? frac_1 : (frac) v);
	v = (float)frac_1 - g / denom;	/* unscaled */
	cmyk[1] =
	    (is_fneg(v) ? frac_0 : v >= (float)frac_1 ? frac_1 : (frac) v);
	v = (float)frac_1 - b / denom;	/* unscaled */
	cmyk[2] =
	    (is_fneg(v) ? frac_0 : v >= (float)frac_1 ? frac_1 : (frac) v);
#endif
    }
    cmyk[3] = bg;
    if_debug7('c', "[c]RGB 0x%x,0x%x,0x%x -> CMYK 0x%x,0x%x,0x%x,0x%x\n",
	      r, g, b, cmyk[0], cmyk[1], cmyk[2], cmyk[3]);
}

/* Convert CMYK to Gray. */
frac
color_cmyk_to_gray(frac c, frac m, frac y, frac k, const gs_imager_state * pis)
{
    frac not_gray = color_rgb_to_gray(c, m, y, pis);

    return (not_gray > frac_1 - k ?	/* gray + k > 1.0 */
	    frac_0 : frac_1 - (not_gray + k));
}

/* Convert CMYK to RGB. */
void
color_cmyk_to_rgb(frac c, frac m, frac y, frac k, const gs_imager_state * pis,
		  frac rgb[3])
{
    switch (k) {
	case frac_0:
	    rgb[0] = frac_1 - c;
	    rgb[1] = frac_1 - m;
	    rgb[2] = frac_1 - y;
	    break;
	case frac_1:
	    rgb[0] = rgb[1] = rgb[2] = frac_0;
	    break;
	default:
	    {
#ifdef USE_ADOBE_CMYK_RGB
		/* R = 1.0 - min(1.0, C + K), etc. */
		frac not_k = frac_1 - k;

		rgb[0] = (c > not_k ? frac_0 : not_k - c);
		rgb[1] = (m > not_k ? frac_0 : not_k - m);
		rgb[2] = (y > not_k ? frac_0 : not_k - y);
#else
		/* R = (1.0 - C) * (1.0 - K), etc. */
		ulong not_k = frac_1 - k;

		/* Compute not_k * (frac_1 - v) / frac_1 efficiently. */
		ulong prod;

#define deduct_black(v)\
  (prod = (frac_1 - (v)) * not_k, frac_1_quo(prod))
		rgb[0] = deduct_black(c);
		rgb[1] = deduct_black(m);
		rgb[2] = deduct_black(y);
#undef deduct_black
#endif
	    }
    }
    if_debug7('c', "[c]CMYK 0x%x,0x%x,0x%x,0x%x -> RGB 0x%x,0x%x,0x%x\n",
	      c, m, y, k, rgb[0], rgb[1], rgb[2]);
}
