/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxcmap.h,v 1.2 2000/03/08 23:14:55 mike Exp $ */
/* Requires gxdcolor.h, gxdevice.h. */

#ifndef gxcmap_INCLUDED
#  define gxcmap_INCLUDED

#include "gscsel.h"
#include "gxfmap.h"

/* Procedures for rendering colors specified by fractions. */

#define cmap_proc_gray(proc)\
  void proc(P5(frac, gx_device_color *, const gs_imager_state *,\
	       gx_device *, gs_color_select_t))
#define cmap_proc_rgb(proc)\
  void proc(P7(frac, frac, frac, gx_device_color *, const gs_imager_state *,\
	       gx_device *, gs_color_select_t))
#define cmap_proc_cmyk(proc)\
  void proc(P8(frac, frac, frac, frac, gx_device_color *,\
	       const gs_imager_state *, gx_device *, gs_color_select_t))
#define cmap_proc_rgb_alpha(proc)\
  void proc(P8(frac, frac, frac, frac, gx_device_color *,\
	       const gs_imager_state *, gx_device *, gs_color_select_t))

/* Because of a bug in the Watcom C compiler, */
/* we have to split the struct from the typedef. */
struct gx_color_map_procs_s {
    cmap_proc_gray((*map_gray));
    cmap_proc_rgb((*map_rgb));
    cmap_proc_cmyk((*map_cmyk));
    cmap_proc_rgb_alpha((*map_rgb_alpha));
};
typedef struct gx_color_map_procs_s gx_color_map_procs;

/* Determine the color mapping procedures for a device. */
const gx_color_map_procs *gx_device_cmap_procs(P1(const gx_device *));

/* Set the color mapping procedures in the graphics state. */
/* This is only needed when switching devices. */
void gx_set_cmap_procs(P2(gs_imager_state *, const gx_device *));

/* Remap a concrete (frac) RGB or CMYK color. */
/* These cannot fail, and do not return a value. */
#define gx_remap_concrete_rgb(cr, cg, cb, pdc, pgs, dev, select)\
  (*pgs->cmap_procs->map_rgb)(cr, cg, cb, pdc, pgs, dev, select)
#define gx_remap_concrete_cmyk(cc, cm, cy, ck, pdc, pgs, dev, select)\
  (*pgs->cmap_procs->map_cmyk)(cc, cm, cy, ck, pdc, pgs, dev, select)
#define gx_remap_concrete_rgb_alpha(cr, cg, cb, ca, pdc, pgs, dev, select)\
  (*pgs->cmap_procs->map_rgb_alpha)(cr, cg, cb, ca, pdc, pgs, dev, select)

/* Map a color, with optional tracing if we are debugging. */
#ifdef DEBUG
/* Use procedures in gxcmap.c */
#include "gxcvalue.h"
gx_color_index gx_proc_map_rgb_color(P4(gx_device *,
			   gx_color_value, gx_color_value, gx_color_value));
gx_color_index gx_proc_map_rgb_alpha_color(P5(gx_device *,
	   gx_color_value, gx_color_value, gx_color_value, gx_color_value));
gx_color_index gx_proc_map_cmyk_color(P5(gx_device *,
	   gx_color_value, gx_color_value, gx_color_value, gx_color_value));

#  define gx_map_rgb_color(dev, vr, vg, vb)\
     gx_proc_map_rgb_color(dev, vr, vg, vb)
#  define gx_map_rgb_alpha_color(dev, vr, vg, vb, va)\
     gx_proc_map_rgb_alpha_color(dev, vr, vg, vb, va)
#  define gx_map_cmyk_color(dev, vc, vm, vy, vk)\
     gx_proc_map_cmyk_color(dev, vc, vm, vy, vk)
#else
#  define gx_map_rgb_color(dev, vr, vg, vb)\
     (*dev_proc(dev, map_rgb_color))(dev, vr, vg, vb)
#  define gx_map_rgb_alpha_color(dev, vr, vg, vb, va)\
     (*dev_proc(dev, map_rgb_alpha_color))(dev, vr, vg, vb, va)
#  define gx_map_cmyk_color(dev, vc, vm, vy, vk)\
     (*dev_proc(dev, map_cmyk_color))(dev, vc, vm, vy, vk)
#endif

#endif /* gxcmap_INCLUDED */
