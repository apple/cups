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

/*$Id: gdevhit.c,v 1.1 2000/03/08 23:14:23 mike Exp $ */
/* Hit detection device */
#include "std.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxdevice.h"

/* Define the value returned for a detected hit. */
const int gs_hit_detected = gs_error_hit_detected;

/*
 * Define a minimal device for insideness testing.
 * It returns e_hit whenever it is asked to actually paint any pixels.
 */
private dev_proc_fill_rectangle(hit_fill_rectangle);
const gx_device gs_hit_device =
{std_device_std_body(gx_device, 0, "hit detector",
		     0, 0, 1, 1),
 {NULL,				/* open_device */
  NULL,				/* get_initial_matrix */
  NULL,				/* sync_output */
  NULL,				/* output_page */
  NULL,				/* close_device */
  gx_default_map_rgb_color,
  gx_default_map_color_rgb,
  hit_fill_rectangle,
  NULL,				/* tile_rectangle */
  NULL,				/* copy_mono */
  NULL,				/* copy_color */
  gx_default_draw_line,
  NULL,				/* get_bits */
  NULL,				/* get_params */
  NULL,				/* put_params */
  gx_default_map_cmyk_color,
  NULL,				/* get_xfont_procs */
  NULL,				/* get_xfont_device */
  gx_default_map_rgb_alpha_color,
  gx_default_get_page_device,
  gx_default_get_alpha_bits,
  NULL,				/* copy_alpha */
  gx_default_get_band,
  NULL,				/* copy_rop */
  gx_default_fill_path,
  NULL,				/* stroke_path */
  NULL,				/* fill_mask */
  gx_default_fill_trapezoid,
  gx_default_fill_parallelogram,
  gx_default_fill_triangle,
  gx_default_draw_thin_line,
  gx_default_begin_image,
  gx_default_image_data,
  gx_default_end_image,
  gx_default_strip_tile_rectangle,
  gx_default_strip_copy_rop,
  gx_get_largest_clipping_box,
  gx_default_begin_typed_image,
  NULL,				/* get_bits_rectangle */
  gx_default_map_color_rgb_alpha,
  gx_non_imaging_create_compositor,
  NULL				/* get_hardware_params */
 }
};

/* Test for a hit when filling a rectangle. */
private int
hit_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
		   gx_color_index color)
{
    if (w > 0 && h > 0)
	return_error(gs_error_hit_detected);
    return 0;
}
