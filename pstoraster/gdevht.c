/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevht.c */
/* Halftoning device implementation */
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevht.h"
#include "gxdcolor.h"
#include "gxdcconv.h"
#include "gxdither.h"

/* The device procedures */
private dev_proc_open_device(ht_open);
private dev_proc_map_rgb_color(ht_map_rgb_color);
private dev_proc_map_color_rgb(ht_map_color_rgb);
private dev_proc_fill_rectangle(ht_fill_rectangle);
private dev_proc_map_cmyk_color(ht_map_cmyk_color);
private dev_proc_map_rgb_alpha_color(ht_map_rgb_alpha_color);
private const gx_device_ht far_data gs_ht_device =
{	std_device_dci_body(gx_device_ht, 0, "halftoner",
			    0, 0, 1, 1,
			    1, 8, 255, 0, 0, 0),
	  {	ht_open,
		gx_forward_get_initial_matrix,
		gx_forward_sync_output,
		gx_forward_output_page,
		gx_default_close_device,
		ht_map_rgb_color,
		ht_map_color_rgb,
		ht_fill_rectangle,
		gx_default_tile_rectangle,
		gx_default_copy_mono,
		gx_default_copy_color,
		gx_default_draw_line,
		gx_default_get_bits,
		gx_forward_get_params,
		gx_forward_put_params,
		ht_map_cmyk_color,
		gx_forward_get_xfont_procs,
		gx_forward_get_xfont_device,
		ht_map_rgb_alpha_color,
		gx_forward_get_page_device,
		gx_forward_get_alpha_bits,
		gx_default_copy_alpha,
		gx_forward_get_band,
		gx_default_copy_rop,
		gx_default_fill_path,
		gx_default_stroke_path,
		gx_default_fill_mask,
		gx_default_fill_trapezoid,
		gx_default_fill_parallelogram,
		gx_default_fill_triangle,
		gx_default_draw_thin_line,
		gx_default_begin_image,
		gx_default_image_data,
		gx_default_end_image,
		gx_default_strip_tile_rectangle,
		gx_default_strip_copy_rop
	  }
};

/*
 * Define the packing of two target colors and a halftone level into
 * a gx_color_index.  Since C doesn't let us cast between a structure
 * and a scalar, we have to use explicit shifting and masking.
 */
#define cx_bits (sizeof(gx_color_index) * 8)
#define cx_color_mask ((1 << ht_target_max_depth) - 1)
#define cx_color0(color) ((color) >> (cx_bits - ht_target_max_depth))
#define cx_color1(color) (((color) >> (ht_level_depth)) & cx_color_mask)
#define cx_level(color) ((color) & ((1 << ht_level_depth) - 1))
#define cx_values(c0, c1, lev)\
  ( ((((c0) << ht_target_max_depth) + (c1)) << ht_level_depth) + (lev) )

/* Open the device.  Right now we just make some error checks. */
private int
ht_open(gx_device *dev)
{	if ( htdev->target == 0 ||
	     htdev->target->color_info.depth > ht_target_max_depth
	   )
	  return_error(gs_error_rangecheck);
	htdev->phase.x = imod(-htdev->ht_phase.x, htdev->dev_ht->lcm_width);
	htdev->phase.y = imod(-htdev->ht_phase.y, htdev->dev_ht->lcm_height);
	return 0;
}

/* Map from RGB or CMYK colors to the packed representation. */
private gx_color_index
ht_finish_map_color(int code, const gx_device_color *pdevc)
{	if ( code < 0 )
	  return gx_no_color_index;
	if ( pdevc->type == &gx_dc_pure )
	  return cx_values(pdevc->colors.pure, 0, 0);
	if ( pdevc->type == &gx_dc_ht_binary )
	  return cx_values(pdevc->colors.binary.color[0],
			   pdevc->colors.binary.color[1],
			   pdevc->colors.binary.b_level);
	lprintf("bad type in ht color mapping!");
	return gx_no_color_index;
}
private gx_color_index
ht_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	return ht_map_rgb_alpha_color(dev, r, g, b, gx_max_color_value);
}
gx_color_index
ht_map_cmyk_color(gx_device *dev, gx_color_value c, gx_color_value m,
  gx_color_value y, gx_color_value k)
{	gx_device_color devc;
	frac fc = cv2frac(k);
	frac fk = cv2frac(k);
	int code =
	  (c == m & m == y ?
	   gx_render_device_gray(color_cmyk_to_gray(fc, fc, fc, fk, NULL),
				 gx_max_color_value,
				 &devc, htdev->target, htdev->dev_ht,
				 &htdev->ht_phase) :
	   gx_render_device_color(fc, cv2frac(m), cv2frac(y),
				  fk, false, gx_max_color_value,
				  &devc, htdev->target, htdev->dev_ht,
				  &htdev->ht_phase));
	return ht_finish_map_color(code, &devc);
}
gx_color_index
ht_map_rgb_alpha_color(gx_device *dev, gx_color_value r,
  gx_color_value g, gx_color_value b, gx_color_value alpha)
{	gx_device_color devc;
	int code =
	  (r == g & g == b ?
	   gx_render_device_gray(cv2frac(r), alpha,
				 &devc, htdev->target, htdev->dev_ht,
				 &htdev->ht_phase) :
	   gx_render_device_color(cv2frac(r), cv2frac(g), cv2frac(b),
				  frac_0, false, alpha,
				  &devc, htdev->target, htdev->dev_ht,
				  &htdev->ht_phase));
	return ht_finish_map_color(code, &devc);
}

/* Map back to an RGB color. */
private int
ht_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	gx_color_index color0 = cx_color0(color);
	uint level = cx_level(color);
	gx_device *tdev = htdev->target;
	dev_proc_map_color_rgb((*map)) = dev_proc(tdev, map_color_rgb);

	if ( level == 0 )
	  return (*map)(tdev, color0, prgb);
	{ gx_color_index color1 = cx_color1(color);
	  gx_color_value rgb0[3], rgb1[3];
	  uint num_levels = htdev->dev_ht->order.num_levels;
	  int i;

	  (*map)(tdev, color0, rgb0);
	  (*map)(tdev, color1, rgb1);
	  for ( i = 0; i < 3; ++i )
	    prgb[i] = rgb0[i] +
	      (rgb1[i] - rgb0[i]) * (ulong)level / num_levels;
	  return 0;
	}
}

/* Fill a rectangle by tiling with a halftone. */
private int
ht_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	gx_color_index color0 = cx_color0(color);
	uint level = cx_level(color);
	gx_device *tdev = htdev->target;

	if ( level == 0 )
	  return (*dev_proc(tdev, fill_rectangle))
	    (tdev, x, y, w, h, color0);
	{ gx_color_index color1 = cx_color1(color);
	  const gx_ht_order *porder = &htdev->dev_ht->order;
	  gx_ht_cache *pcache = porder->cache;
	  gx_ht_tile *tile;

	  /* Ensure that the tile cache is current. */
	  if ( pcache->order.bits != porder->bits )
	    gx_ht_init_cache(pcache, porder);
	  /* Ensure that the tile we want is cached. */
	  tile = gx_render_ht(pcache, level);
	  if ( tile == 0 )
	    return_error(gs_error_Fatal);
	  /* Fill the rectangle with the tile. */
	  return (*dev_proc(tdev, tile_rectangle))
	    (tdev, &tile->tile, x, y, w, h, color0, color1,
	     htdev->phase.x, htdev->phase.y);
	}
}
