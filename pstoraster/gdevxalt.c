/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevxalt.c */
/* Alternative X Windows drivers for help in driver debugging */
#include "gx.h"			/* for gx_bitmap; includes std.h */
#include "math_.h"
#include "memory_.h"
#include "x_.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gxdevice.h"
#include "gdevx.h"

extern gx_device_X gs_x11_device;

/* ---------------- Generic procedures ---------------- */

/* Forward declarations */
private gx_device *dev_target(P1(gx_device *));
private void get_target_info(P1(gx_device *));
private gx_color_index x_alt_map_color(P2(gx_device *, gx_color_index));

/* "Wrappers" for driver procedures */

private int
x_wrap_open(gx_device *dev)
{	gx_device *tdev = dev_target(dev);
	int code = (*dev_proc(tdev, open_device))(tdev);
	if ( code < 0 )
	  return code;
	tdev->is_open = true;
	get_target_info(dev);
	return code;
}

private int
x_forward_sync_output(gx_device *dev)
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, sync_output))(tdev);
}

private int
x_forward_output_page(gx_device *dev, int num_copies, int flush)
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, output_page))(tdev, num_copies, flush);
}

private int
x_wrap_close(gx_device *dev)
{	gx_device *tdev = dev_target(dev);
	/* If Ghostscript is exiting, we might have closed the */
	/* underlying x11 device already.... */
	int code;
	if ( tdev->is_open )
	  {	code = (*dev_proc(tdev, close_device))(tdev);
		if ( code < 0 )
		  return code;
		tdev->is_open = false;
	  }
	else
	  code = 0;
	return code;
}

private int
x_wrap_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, map_color_rgb))(tdev,
						x_alt_map_color(dev, color),
						prgb);
}

private int
x_wrap_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, fill_rectangle))(tdev, x, y, w, h,
						 x_alt_map_color(dev, color));
}

private int
x_wrap_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, copy_mono))(tdev, base, sourcex, raster, id,
					    x, y, w, h,
					    x_alt_map_color(dev, zero),
					    x_alt_map_color(dev, one));

}

private int
x_forward_copy_color(gx_device *dev, const byte *base, int sourcex,
		     int raster, gx_bitmap_id id, int x, int y, int w, int h)
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, copy_color))(tdev, base, sourcex, raster, id,
					     x, y, w, h);
}

private int
x_forward_get_bits(gx_device *dev, int y, byte *str, byte **actual_data)
{	gx_device *tdev = dev_target(dev);
	return (*dev_proc(tdev, get_bits))(tdev, y, str, actual_data);
}

private int
x_wrap_get_bits(gx_device *dev, int y, byte *str, byte **actual_data)
{	gx_device *tdev = dev_target(dev);
	int width = tdev->width;
	int sdepth = tdev->color_info.depth;
	byte smask = (sdepth <= 8 ? (1 << sdepth) - 1 : 0xff);
	int depth = dev->color_info.depth;
	uint dsize = (width * sdepth + 7) / 8;
	gs_memory_t *mem =
	  (dev->memory == 0 ? &gs_memory_default : dev->memory);
	byte *row = gs_alloc_bytes(mem, dsize, "x_wrap_get_bits");
	byte *base;
	int code;
	gx_color_index pixel_in = gx_no_color_index;
	gx_color_index pixel_out;
	int xi;
	int sbit;
	declare_line_accum(str, depth, 0);

	if ( row == 0 )
	  return_error(gs_error_VMerror);
	code = (*dev_proc(tdev, get_bits))(tdev, y, row, &base);
	if ( code < 0 )
	  goto gx;
	for ( sbit = 0, xi = 0; xi < width; sbit += sdepth, ++xi )
	  {	const byte *sptr = base + (sbit >> 3);
		gx_color_index pixel;
		gx_color_value rgb[3];
		int i;

		if ( sdepth <= 8 )
		  pixel = (*sptr >> (8 - sdepth - (sbit & 7))) & smask;
		else
		  { pixel = 0;
		    for ( i = 0; i < depth; i += 8, ++sptr )
		      pixel = (pixel << 8) + *sptr;
		  }
		if ( pixel != pixel_in )
		  { (*dev_proc(tdev, map_color_rgb))(tdev, pixel, rgb);
		    pixel_in = pixel;
		    pixel_out = (*dev_proc(dev, map_rgb_color))(dev, rgb[0], rgb[1], rgb[2]);
		  }
		line_accum(pixel_out, depth);
	  }
	line_accum_store(depth);
gx:	gs_free_object(mem, row, "x_wrap_get_bits");
	*actual_data = str;
	return code;
}

private int
x_wrap_get_params(gx_device *dev, gs_param_list *plist)
{	gx_device *tdev = dev_target(dev);
	/* We assume that a get_params call has no side effects.... */
	gx_device_X save_dev;
	int code;

	save_dev = *(gx_device_X *)tdev;
	if ( tdev->is_open )
	  tdev->color_info = dev->color_info;
	tdev->dname = dev->dname;
	code = (*dev_proc(tdev, get_params))(tdev, plist);
	*(gx_device_X *)tdev = save_dev;
	return code;
}

private int
x_wrap_put_params(gx_device *dev, gs_param_list *plist)
{	gx_device *tdev = dev_target(dev);
	int code = (*dev_proc(tdev, put_params))(tdev, plist);
	if ( code < 0 )
	  return code;
	get_target_info(dev);
	return code;
}

/* Internal procedures */

/* Get the target, creating it if necessary. */
private gx_device *
dev_target(gx_device *dev)
{	gx_device *tdev = ((gx_device_forward *)dev)->target;
	if ( tdev == 0 )
	  {	/* Create or link to an X device instance. */
		if ( dev->memory == 0 )	/* static instance */
		  tdev = (gx_device *)&gs_x11_device;
		else
		  {	tdev = (gx_device *)gs_alloc_bytes(dev->memory,
						(gs_x11_device).params_size,
						"dev_target");
			if ( tdev == 0 )
			  {	/* Punt. */
				exit(1);
			  }
			*(gx_device_X *)tdev = gs_x11_device;
			tdev->memory = dev->memory;
			tdev->is_open = false;
		  }
		gx_device_fill_in_procs(tdev);
		((gx_device_forward *)dev)->target = tdev;
	  }
	return tdev;
}

/* Copy parameters back from the target. */
private void
get_target_info(gx_device *dev)
{	gx_device *tdev = dev_target(dev);

#define copy(m) dev->m = tdev->m;
#define copy2(m) copy(m[0]); copy(m[1])
#define copy4(m) copy2(m); copy(m[2]); copy(m[3])

	copy(width); copy(height);
	copy2(MediaSize);
	copy4(ImagingBBox);
	copy(ImagingBBox_set);
	copy2(HWResolution);
	copy2(MarginsHWResolution);
	copy2(Margins);
	copy4(HWMargins);
	if ( dev->color_info.num_components == 3 )
	  copy(color_info);

#undef copy4
#undef copy2
#undef copy
}

/* Map a fake CMYK or black/white color to a real X color if necessary. */
private gx_color_index
x_alt_map_color(gx_device *dev, gx_color_index color)
{	gx_device *tdev = dev_target(dev);
	gx_color_value r, g, b;

	if ( color == gx_no_color_index )
	  return color;
	switch ( dev->color_info.num_components )
	  {
	  case 3:	/* RGB, this is the real thing (possibly + alpha) */
	    return color & 0xffffff;
	  case 4:	/* CMYK */
	    if ( color & 1 )
	      r = g = b = 0;
	    else
	      { r = (color & 8 ? 0 : gx_max_color_value);
		g = (color & 4 ? 0 : gx_max_color_value);
		b = (color & 2 ? 0 : gx_max_color_value);
	      }
	    break;
	  default /*case 1*/:	/* black-and-white */
	    r = g = b = (color ? gx_max_color_value : 0);
	    break;
	  }
	return (*dev_proc(tdev, map_rgb_color))(tdev, r, g, b);
}

/* ---------------- CMYK procedures ---------------- */

/* Device procedures */
private dev_proc_map_rgb_color(x_cmyk_map_rgb_color);
private dev_proc_map_cmyk_color(x_cmyk_map_cmyk_color);

/* The device descriptor */
private gx_device_procs x_cmyk_procs = {
	x_wrap_open,
	gx_forward_get_initial_matrix,
	x_forward_sync_output,
	x_forward_output_page,
	x_wrap_close,
	x_cmyk_map_rgb_color,
	x_wrap_map_color_rgb,
	x_wrap_fill_rectangle,
	gx_default_tile_rectangle,
	x_wrap_copy_mono,
	gx_default_copy_color,		/* do it the slow, painful way */
	gx_default_draw_line,
	x_wrap_get_bits,
	x_wrap_get_params,
	x_wrap_put_params,
	x_cmyk_map_cmyk_color,
	gx_forward_get_xfont_procs,
	gx_forward_get_xfont_device,
	NULL,			/* map_rgb_alpha_color */
	gx_forward_get_page_device,
	gx_forward_get_alpha_bits,
	NULL			/* copy_alpha */
};

/* The instance is public. */
gx_device_forward far_data gs_x11cmyk_device = {
	std_device_dci_body(gx_device_forward, &x_cmyk_procs, "x11cmyk",
	  FAKE_RES*85/10, FAKE_RES*11,	/* x and y extent (nominal) */
	  FAKE_RES, FAKE_RES,	/* x and y density (nominal) */
	  4, 4, 1, 1, 2, 2),
	{ 0 },			/* std_procs */
	0			/* target */
};

/* Device procedures */

private gx_color_index
x_cmyk_map_rgb_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b)
{	/* This should never be called! */
	return gx_no_color_index;
}

private gx_color_index
x_cmyk_map_cmyk_color(gx_device *dev,
  gx_color_value c, gx_color_value m, gx_color_value y,
  gx_color_value k)
{	return
	  (gx_color_index)
	    (((c >> (gx_color_value_bits - 4)) & 8) |
	     ((m >> (gx_color_value_bits - 3)) & 4) |
	     ((y >> (gx_color_value_bits - 2)) & 2) |
	     ((k >> (gx_color_value_bits - 1)) & 1));
}

/* ---------------- Black-and-white procedures ---------------- */

/* The device descriptor */
private gx_device_procs x_mono_procs = {
	x_wrap_open,
	gx_forward_get_initial_matrix,
	x_forward_sync_output,
	x_forward_output_page,
	x_wrap_close,
	gx_default_map_rgb_color,
	x_wrap_map_color_rgb,
	x_wrap_fill_rectangle,
	gx_default_tile_rectangle,
	x_wrap_copy_mono,
	gx_default_copy_color,
	gx_default_draw_line,
	x_wrap_get_bits,
	x_wrap_get_params,
	x_wrap_put_params,
	gx_default_map_cmyk_color,
	gx_forward_get_xfont_procs,
	gx_forward_get_xfont_device,
	NULL,			/* map_rgb_alpha_color */
	gx_forward_get_page_device,
	gx_forward_get_alpha_bits,
	NULL			/* copy_alpha */
};

/* The instance is public. */
gx_device_forward far_data gs_x11mono_device = {
	std_device_dci_body(gx_device_forward, &x_mono_procs, "x11mono",
	  FAKE_RES*85/10, FAKE_RES*11,	/* x and y extent (nominal) */
	  FAKE_RES, FAKE_RES,	/* x and y density (nominal) */
	  1, 1, 1, 0, 2, 0),
	{ 0 },			/* std_procs */
	0			/* target */
};

/* ---------------- Alpha procedures ---------------- */

/* Device procedures */
private dev_proc_map_color_rgb(x_alpha_map_color_rgb);
private dev_proc_map_rgb_alpha_color(x_alpha_map_rgb_alpha_color);
private dev_proc_get_alpha_bits(x_alpha_get_alpha_bits);
private dev_proc_copy_alpha(x_alpha_copy_alpha);

/* The device descriptor */
private gx_device_procs x_alpha_procs = {
	x_wrap_open,
	gx_forward_get_initial_matrix,
	x_forward_sync_output,
	x_forward_output_page,
	x_wrap_close,
	gx_forward_map_rgb_color,
	x_alpha_map_color_rgb,
	x_wrap_fill_rectangle,
	gx_default_tile_rectangle,
	x_wrap_copy_mono,
	x_forward_copy_color,
	gx_default_draw_line,
	x_forward_get_bits,
	gx_forward_get_params,
	x_wrap_put_params,
	gx_forward_map_cmyk_color,
	gx_forward_get_xfont_procs,
	gx_forward_get_xfont_device,
	x_alpha_map_rgb_alpha_color,
	gx_forward_get_page_device,
	x_alpha_get_alpha_bits,
	/*gx_default_copy_alpha*/	x_alpha_copy_alpha
};

/* The instance is public. */
gx_device_forward far_data gs_x11alpha_device = {
	std_device_dci_body(gx_device_forward, &x_alpha_procs, "x11alpha",
	  FAKE_RES*85/10, FAKE_RES*11,	/* x and y extent (nominal) */
	  FAKE_RES, FAKE_RES,	/* x and y density (nominal) */
	  3, 32, 255, 255, 256, 256),
	{ 0 },			/* std_procs */
	0			/* target */
};

/* Device procedures */

/* We encode a complemented alpha value in the top 8 bits of the */
/* device color. */
private int
x_alpha_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	return gx_forward_map_color_rgb(dev, color & 0xffffff, prgb);
}
private gx_color_index
x_alpha_map_rgb_alpha_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b, gx_color_value alpha)
{	gx_color_index color = gx_forward_map_rgb_color(dev, r, g, b);
	byte abyte = alpha >> (gx_color_value_bits - 8);

	return (abyte == 0 ? 0xff000000 :
		((gx_color_index)(abyte ^ 0xff) << 24) + color);
}

private int
x_alpha_get_alpha_bits(gx_device *dev, graphics_object_type type)
{	return 4;
}

private int
x_alpha_copy_alpha(gx_device *dev, const unsigned char *base, int sourcex,
		   int raster, gx_bitmap_id id, int x, int y, int w, int h,
		   gx_color_index color, int depth)
{	gx_device *tdev = dev_target(dev);
	int xi, yi;
	const byte *row = base;
	gx_color_index base_color = color & 0xffffff;
	/* We fake alpha by interpreting it as saturation, i.e., */
	/* alpha = 0 is white, alpha = 15/15 is the full color. */
	gx_color_value rgb[3];
	gx_color_index shades[16];
	int i;

/**************** PATCH for measuring rasterizer speed ****************/
/*if ( 1 ) return 0;*/

	for ( i = 0; i < 15; ++i )
	  shades[i] = gx_no_color_index;
	shades[15] = base_color;
	(*dev_proc(tdev, map_color_rgb))(tdev, base_color, rgb);
	/* Do the copy operation pixel-by-pixel. */
	/* For the moment, if the base color has alpha in it, we ignore it. */
	for ( yi = y; yi < y + h; row += raster, ++yi )
	  {	int prev_x = x;
		gx_color_index prev_color = gx_no_color_index;
		uint prev_alpha = 0x10;		/* not a possible value */

		for ( xi = x; xi < x + w; ++xi )
		  {	int sx = sourcex + xi - x;
			uint alpha2 = row[sx >> 1];
			uint alpha = (sx & 1 ? alpha2 & 0xf : alpha2 >> 4);
			gx_color_index a_color;

			if ( alpha == prev_alpha )
			  continue;
			prev_alpha = alpha;
			if ( alpha == 0 )
			  a_color = gx_no_color_index;
			else
			  while ( (a_color = shades[alpha]) == gx_no_color_index )
			  {	/* Map the color now. */
#define make_shade(v, alpha)\
  (gx_max_color_value -\
   ((gx_max_color_value - (v)) * (alpha) / 15))
				gx_color_value r = make_shade(rgb[0], alpha);
				gx_color_value g = make_shade(rgb[1], alpha);
				gx_color_value b = make_shade(rgb[2], alpha);
#undef make_shade
				a_color = (*dev_proc(tdev, map_rgb_color))(tdev, r, g, b);
				if ( a_color != gx_no_color_index )
				  {	shades[alpha] = a_color;
					break;
				  }
				/* Try a higher saturation.  (We know */
				/* the fully saturated color exists.) */
				alpha += (16 - alpha) >> 1;
			  }
			if ( a_color != prev_color )
			  { if ( prev_color != gx_no_color_index )
			      (*dev_proc(tdev, fill_rectangle))(tdev,
					prev_x, yi, xi - prev_x, 1,
					prev_color);
			    prev_x = xi;
			    prev_color = a_color;
			  }
		  }
		if ( prev_color != gx_no_color_index )
		  (*dev_proc(tdev, fill_rectangle))(tdev,
					prev_x, yi, x + w - prev_x, 1,
					prev_color);
	  }
	return 0;
}
