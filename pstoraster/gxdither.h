/* Copyright (C) 1994, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxdither.h,v 1.2 2000/03/08 23:14:57 mike Exp $ */
/* Interface to gxdither.c */

#ifndef gxdither_INCLUDED
#  define gxdither_INCLUDED

#ifndef gx_device_halftone_DEFINED
#  define gx_device_halftone_DEFINED
typedef struct gx_device_halftone_s gx_device_halftone;

#endif

/*
 * Note that in the procedures below, the colors are specified by fracs,
 * but the alpha value is a gx_color_value.  This is a design flaw that
 * we might be able to fix eventually.
 */

/* Render a gray, possibly by halftoning. */
/* Return 0 if complete, 1 if caller must do gx_color_load, <0 on error. */
int gx_render_device_gray(P6(frac gray, gx_color_value alpha,
			     gx_device_color * pdevc, gx_device * dev,
			     const gx_device_halftone * dev_ht,
			     const gs_int_point * ht_phase));

#define gx_render_gray_alpha(gray, alpha, pdevc, pis, dev, select)\
  gx_render_device_gray(gray, alpha, pdevc, dev, pis->dev_ht,\
			&pis->screen_phase[select])
#define gx_render_gray(gray, pdevc, pis, dev, select)\
  gx_render_gray_alpha(gray, pis->alpha, pdevc, pis, dev, select)

/* Render a color, possibly by halftoning. */
/* Return as for gx_render_[device_]gray. */
int gx_render_device_color(P10(frac red, frac green, frac blue, frac white,
			       bool cmyk, gx_color_value alpha,
			       gx_device_color * pdevc, gx_device * dev,
			       const gx_device_halftone * pdht,
			       const gs_int_point * ht_phase));

#define gx_render_color_alpha(r, g, b, w, a, cmyk, pdevc, pis, dev, select)\
  gx_render_device_color(r, g, b, w, cmyk, a, pdevc, dev,\
			 pis->dev_ht, &pis->screen_phase[select])
#define gx_render_color(r, g, b, w, cmyk, pdevc, pis, dev, select)\
  gx_render_color_alpha(r, g, b, w, pis->alpha, cmyk, pdevc, pis, dev, select)
#define gx_render_rgb(r, g, b, pdevc, pis, dev, select)\
  gx_render_color(r, g, b, frac_0, false, pdevc, pis, dev, select)
#define gx_render_cmyk(c, m, y, k, pdevc, pis, dev, select)\
  gx_render_color(c, m, y, k, true, pdevc, pis, dev, select)
#define gx_render_rgb_alpha(r, g, b, a, pdevc, pis, dev, select)\
  gx_render_color_alpha(r, g, b, frac_0, a, false, pdevc, pis, dev, select)

#endif /* gxdither_INCLUDED */
