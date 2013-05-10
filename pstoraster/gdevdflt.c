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

/* gdevdflt.c */
/* Default device implementation */
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxcpath.h"

/* ---------------- Default device procedures ---------------- */

/* Define the default implementations of RasterOp procedures. */
/* If the RasterOp option is linked in, it initializes these */
/* to different values. */
dev_proc_copy_rop((*gx_default_copy_rop_proc)) = gx_no_copy_rop;
dev_proc_copy_rop((*gx_forward_copy_rop_proc)) = gx_no_copy_rop;
dev_proc_strip_copy_rop((*gx_default_strip_copy_rop_proc)) = gx_no_strip_copy_rop;
dev_proc_strip_copy_rop((*gx_forward_strip_copy_rop_proc)) = gx_no_strip_copy_rop;

/* Fill in NULL procedures in a device procedure record. */
void
gx_device_fill_in_procs(register gx_device *dev)
{	gx_device_set_procs(dev);
	fill_dev_proc(dev, open_device, gx_default_open_device);
	fill_dev_proc(dev, get_initial_matrix, gx_default_get_initial_matrix);
	fill_dev_proc(dev, sync_output, gx_default_sync_output);
	fill_dev_proc(dev, output_page, gx_default_output_page);
	fill_dev_proc(dev, close_device, gx_default_close_device);
	fill_dev_proc(dev, map_rgb_color, gx_default_map_rgb_color);
	fill_dev_proc(dev, map_color_rgb, gx_default_map_color_rgb);
	/* NOT fill_rectangle */
	fill_dev_proc(dev, tile_rectangle, gx_default_tile_rectangle);
	fill_dev_proc(dev, copy_mono, gx_default_copy_mono);
	fill_dev_proc(dev, copy_color, gx_default_copy_color);
	fill_dev_proc(dev, draw_line, gx_default_draw_line);
	fill_dev_proc(dev, get_bits, gx_default_get_bits);
	fill_dev_proc(dev, get_params, gx_default_get_params);
	fill_dev_proc(dev, put_params, gx_default_put_params);
	fill_dev_proc(dev, map_cmyk_color, gx_default_map_cmyk_color);
	fill_dev_proc(dev, get_xfont_procs, gx_default_get_xfont_procs);
	fill_dev_proc(dev, get_xfont_device, gx_default_get_xfont_device);
	fill_dev_proc(dev, map_rgb_alpha_color, gx_default_map_rgb_alpha_color);
	fill_dev_proc(dev, get_page_device, gx_default_get_page_device);
	fill_dev_proc(dev, get_alpha_bits, gx_default_get_alpha_bits);
	fill_dev_proc(dev, copy_alpha, gx_default_copy_alpha);
	fill_dev_proc(dev, get_band, gx_default_get_band);
	fill_dev_proc(dev, copy_rop, gx_default_copy_rop_proc);
	fill_dev_proc(dev, fill_path, gx_default_fill_path);
	fill_dev_proc(dev, stroke_path, gx_default_stroke_path);
	fill_dev_proc(dev, fill_mask, gx_default_fill_mask);
	fill_dev_proc(dev, fill_trapezoid, gx_default_fill_trapezoid);
	fill_dev_proc(dev, fill_parallelogram, gx_default_fill_parallelogram);
	fill_dev_proc(dev, fill_triangle, gx_default_fill_triangle);
	fill_dev_proc(dev, draw_thin_line, gx_default_draw_thin_line);
	fill_dev_proc(dev, begin_image, gx_default_begin_image);
	fill_dev_proc(dev, image_data, gx_default_image_data);
	fill_dev_proc(dev, end_image, gx_default_end_image);
	fill_dev_proc(dev, strip_tile_rectangle, gx_default_strip_tile_rectangle);
	fill_dev_proc(dev, strip_copy_rop, gx_default_strip_copy_rop_proc);
}

int
gx_default_open_device(gx_device *dev)
{	return 0;
}

/* Get the initial matrix for a device with inverted Y. */
/* This includes essentially all printers and displays. */
void
gx_default_get_initial_matrix(gx_device *dev, register gs_matrix *pmat)
{	pmat->xx = dev->HWResolution[0] / 72.0;  /* x_pixels_per_inch */
	pmat->xy = 0;
	pmat->yx = 0;
	pmat->yy = dev->HWResolution[1] / -72.0;  /* y_pixels_per_inch */
		/****** tx/y is WRONG for devices with ******/
		/****** arbitrary initial matrix ******/
	pmat->tx = 0;
	pmat->ty = dev->height;
}
/* Get the initial matrix for a device with upright Y. */
/* This includes just a few printers and window systems. */
void
gx_upright_get_initial_matrix(gx_device *dev, register gs_matrix *pmat)
{	pmat->xx = dev->HWResolution[0] / 72.0;  /* x_pixels_per_inch */
	pmat->xy = 0;
	pmat->yx = 0;
	pmat->yy = dev->HWResolution[1] / 72.0;  /* y_pixels_per_inch */
		/****** tx/y is WRONG for devices with ******/
		/****** arbitrary initial matrix ******/
	pmat->tx = 0;
	pmat->ty = 0;
}

int
gx_default_sync_output(gx_device *dev)
{	return 0;
}

int
gx_default_output_page(gx_device *dev, int num_copies, int flush)
{	return (*dev_proc(dev, sync_output))(dev);
}

int
gx_default_close_device(gx_device *dev)
{	return 0;
}

/* By default, implement tile_rectangle using strip_tile_rectangle. */
int
gx_default_tile_rectangle(gx_device *dev, const gx_tile_bitmap *tile,
  int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
  int px, int py)
{	gx_strip_bitmap tiles;
	*(gx_tile_bitmap *)&tiles = *tile;
	tiles.shift = tiles.rep_shift = 0;
	return (*dev_proc(dev, strip_tile_rectangle))
	  (dev, &tiles, x, y, w, h, color0, color1, px, py);
}

/* Implement copy_mono by filling lots of small rectangles. */
/* This is very inefficient, but it works as a default. */
int
gx_default_copy_mono(gx_device *dev, const byte *data,
  int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	byte invert;
	gx_color_index color;
	dev_proc_fill_rectangle((*fill));
	const byte *row;
	int iy;

	fit_copy(dev, data, dx, raster, id, x, y, w, h);
	fill = dev_proc(dev, fill_rectangle);
	if ( one != gx_no_color_index )
	  { invert = 0;
	    color = one;
	    if ( zero != gx_no_color_index )
	      { int code = (*fill)(dev, x, y, w, h, zero);
		if ( code < 0 )
		  return code;
	      }
	  }
	else
	  { invert = 0xff;
	    color = zero;
	  }
	for ( row = data, iy = 0; iy < h; row += raster, iy++ )
	  { int ix;
	    for ( ix = 0; ix < w; )
	      { int i0;
#define row_bit(i) ((row[(i) >> 3] ^ invert) & (0x80 >> ((i) & 7)))
		while ( ix < w && row_bit(ix + dx) == 0 )
		  ix++;
		for ( i0 = ix; ix < w && row_bit(ix + dx) != 0; )
		  ix++;
		if ( ix > i0 )
		  { int code = (*fill)(dev, i0 + x, iy + y, ix - i0, 1, color);
		    if ( code < 0 )
		      return code;
		  }
#undef row_bit
	      }
	  }
	return 0;
}

/* Implement copy_color by filling lots of small rectangles. */
/* This is very inefficient, but it works as a default. */
int
gx_default_copy_color(gx_device *dev, const byte *data,
  int dx, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	int depth = dev->color_info.depth;
	byte mask;
	dev_proc_fill_rectangle((*fill));
	const byte *row;
	int iy;

	if ( depth == 1 )
	  return (*dev_proc(dev, copy_mono))(dev, data, dx, raster, id,
				x, y, w, h,
				(gx_color_index)0, (gx_color_index)1);
	fit_copy(dev, data, dx, raster, id, x, y, w, h);
	fill = dev_proc(dev, fill_rectangle);
	mask = (byte)((1 << depth) - 1);
	for ( row = data, iy = 0; iy < h; row += raster, ++iy )
	  { int ix;
	    gx_color_index c0 = gx_no_color_index;
	    const byte *ptr = row + ((dx * depth) >> 3);
	    int i0;

	    for ( i0 = ix = 0; ix < w; ++ix )
	      { gx_color_index color;

		if ( depth >= 8 )
		  { color = *ptr++;
		    switch ( depth )
		      {
		      case 32: color = (color << 8) + *ptr++;
		      case 24: color = (color << 8) + *ptr++;
		      case 16: color = (color << 8) + *ptr++;
		      }
		  }
		else
		  { uint dbit = (-(ix + dx + 1) * depth) & 7;
		    color = (*ptr >> dbit) & mask;
		    if ( dbit == 0 )
		      ptr++;
		  }
		if ( color != c0 )
		  { if ( ix > i0 )
		      { int code = (*fill)
			  (dev, i0 + x, iy + y, ix - i0, 1, c0);
			if ( code < 0 )
			  return code;
		      }
		    c0 = color;
		    i0 = ix;
		  }
	      }
	    if ( ix > i0 )
	      { int code = (*fill)(dev, i0 + x, iy + y, ix - i0, 1, c0);
		if ( code < 0 )
		  return code;
	      }
	  }
	return 0;
}

int
gx_default_get_bits(gx_device *dev, int y, byte *data, byte **actual_data)
{	return_error(gs_error_unknownerror);
}

gx_xfont_procs *
gx_default_get_xfont_procs(gx_device *dev)
{	return NULL;
}

gx_device *
gx_default_get_xfont_device(gx_device *dev)
{	return dev;
}

gx_device *
gx_default_get_page_device(gx_device *dev)
{	return NULL;
}
gx_device *
gx_page_device_get_page_device(gx_device *dev)
{	return dev;
}

int
gx_default_get_alpha_bits(gx_device *dev, graphics_object_type type)
{	return 1;
}

int
gx_no_copy_alpha(gx_device *dev, const byte *data, int data_x,
  int raster, gx_bitmap_id id, int x, int y, int width, int height,
  gx_color_index color, int depth)
{	return_error(gs_error_unknownerror);
}

int
gx_default_copy_alpha(gx_device *dev, const byte *data, int data_x,
  int raster, gx_bitmap_id id, int x, int y, int width, int height,
  gx_color_index color, int depth)
{	/* This might be called with depth = 1.... */
	if ( depth == 1 )
	  return (*dev_proc(dev, copy_mono))(dev, data, data_x, raster, id,
					     x, y, width, height,
					     gx_no_color_index, color);
	/*
	 * Simulate alpha by weighted averaging of RGB values.
	 * This is very slow, but functionally correct.
	 */
	{ const byte *row;
	  gs_memory_t *mem = &gs_memory_default; /* dev might not have one */
	  int bpp = dev->color_info.depth;
	  uint in_size = gx_device_raster(dev, false);
	  byte *lin;
	  uint out_size;
	  byte *lout;
	  int code = 0;
	  gx_color_value color_rgb[3];
	  int ry;

	  fit_copy(dev, data, data_x, raster, id, x, y, width, height);
	  row = data;
	  out_size = bitmap_raster(width * bpp);
	  lin = gs_alloc_bytes(mem, in_size, "copy_alpha(lin)");
	  lout = gs_alloc_bytes(mem, out_size, "copy_alpha(lout)");
	  if ( lin == 0 || lout == 0 )
	    { code = gs_note_error(gs_error_VMerror);
	      goto out;
	    }
	  (*dev_proc(dev, map_color_rgb))(dev, color, color_rgb);
	  for ( ry = y; ry < y + height; row += raster, ++ry )
	    { byte *line;
	      int sx, rx;
	      declare_line_accum(lout, bpp, x);

	      code = (*dev_proc(dev, get_bits))(dev, ry, lin, &line);
	      if ( code < 0 )
		break;
	      for ( sx = data_x, rx = x; sx < data_x + width; ++sx, ++rx )
		{ gx_color_index previous = gx_no_color_index;
		  gx_color_index composite;
		  int alpha2, alpha;

		  if ( depth == 2 )	/* map 0 - 3 to 0 - 15 */
		    alpha = ((row[sx >> 2] >> ((3 - (sx & 3)) << 1)) & 3) * 5;
		  else 
		    alpha2 = row[sx >> 1],
		    alpha = (sx & 1 ? alpha2 & 0xf : alpha2 >> 4);
blend:		  if ( alpha == 15 )
		    { /* Just write the new color. */
		      composite = color;
		    }
		  else
		    { if ( previous == gx_no_color_index )
			{ /* Extract the old color. */
			  if ( bpp < 8 )
			    { const uint bit = rx * bpp;
			      const byte *src = line + (bit >> 3);
			      previous =
				(*src >> (8 - (bit + bpp))) &
				  ((1 << bpp) - 1);
			    }
			  else
			    { const byte *src = line + (rx * (bpp >> 3));
			      previous = 0;
			      switch ( bpp >> 3 )
				{
				case 4:
				  previous += (gx_color_index)*src++ << 24;
				case 3:
				  previous += (gx_color_index)*src++ << 16;
				case 2:
				  previous += (gx_color_index)*src++ << 8;
				case 1:
				  previous += *src++;
				}
			    }
			}
		      if ( alpha == 0 )
			{ /* Just write the old color. */
			  composite = previous;
			}
		      else
			{ /* Blend RGB values. */
			  gx_color_value rgb[3];

			  (*dev_proc(dev, map_color_rgb))(dev, previous, rgb);
#if arch_ints_are_short
#  define b_int long
#else
#  define b_int int
#endif
#define make_shade(old, clr, alpha, amax) \
  (old) + (((b_int)(clr) - (b_int)(old)) * (alpha) / (amax))
			  rgb[0] = make_shade(rgb[0], color_rgb[0], alpha, 15);
			  rgb[1] = make_shade(rgb[1], color_rgb[1], alpha, 15);
			  rgb[2] = make_shade(rgb[2], color_rgb[2], alpha, 15);
#undef b_int
#undef make_shade
			  composite =
			    (*dev_proc(dev, map_rgb_color))(dev, rgb[0],
							    rgb[1], rgb[2]);
			  if ( composite == gx_no_color_index )
			    { /* The device can't represent this color. */
			      /* Move the alpha value towards 0 or 1. */
			      if ( alpha == 7 )	/* move 1/2 towards 1 */
				++alpha;
			      alpha = (alpha & 8) | (alpha >> 1);
			      goto blend;
			    }
			}
		    }
		  line_accum(composite, bpp);
		}
	      line_accum_copy(dev, lout, bpp, x, rx, raster, ry);
	    }
out:	  gs_free_object(mem, lout, "copy_alpha(lout)");
	  gs_free_object(mem, lin, "copy_alpha(lin)");
	  return code;
	}
}

int
gx_default_get_band(gx_device *dev, int y, int *band_start)
{	return 0;
}

int
gx_no_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_tile_bitmap *texture, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	return_error(gs_error_unknownerror);	/* not implemented */
}
int
gx_default_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_tile_bitmap *texture, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	return (*gx_default_copy_rop_proc)
	  (dev, sdata, sourcex, sraster, id, scolors, texture, tcolors,
	   x, y, width, height, phase_x, phase_y, lop);
}

int
gx_default_fill_mask(gx_device *orig_dev,
  const byte *data, int dx, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  const gx_drawing_color *pdcolor, int depth,
  gs_logical_operation_t lop, const gx_clip_path *pcpath)
{	gx_device *dev;
	gx_device_clip cdev;
	gx_color_index colors[2];
	gx_strip_bitmap *tile;

	if ( gx_dc_is_pure(pdcolor) )
	  { tile = 0;
	    colors[0] = gx_no_color_index;
	    colors[1] = gx_dc_pure_color(pdcolor);
	  }
	else if ( gx_dc_is_binary_halftone(pdcolor) )
	  { tile = gx_dc_binary_tile(pdcolor);
	    colors[0] = gx_dc_binary_color0(pdcolor);
	    colors[1] = gx_dc_binary_color1(pdcolor);
	  }
	else
	  return_error(gs_error_unknownerror);	/* not implemented */
	if ( pcpath != 0 )
	  {	gx_make_clip_path_device(&cdev, pcpath);
		cdev.target = orig_dev;
		dev = (gx_device *)&cdev;
		(*dev_proc(dev, open_device))(dev);
	  }
	else
	  dev = orig_dev;
	if ( depth > 1 )
	  { /****** CAN'T DO ROP OR HALFTONE WITH ALPHA ******/
	    return (*dev_proc(dev, copy_alpha))
	      (dev, data, dx, raster, id, x, y, w, h, colors[1], depth);
	  }
	if ( lop != lop_default )
	  { gx_color_index scolors[2];

	    scolors[0] = 1;
	    scolors[1] = 0;
	    return (*dev_proc(dev, strip_copy_rop))
	      (dev, data, dx, raster, id, scolors, tile, colors,
	       x, y, w, h,
	       gx_dc_phase(pdcolor).x, gx_dc_phase(pdcolor).y, lop);
	  }
	if ( tile == 0 )
	  { return (*dev_proc(dev, copy_mono))
	      (dev, data, dx, raster, id, x, y, w, h,
	       gx_no_color_index, colors[1]);
	  }
	/*
	 * Use the same approach as the default copy_mono (above).  We
	 * should really clip to the intersection of the bounding boxes of
	 * the device and the clipping path, but it's too much work.
	 */
	fit_copy(orig_dev, data, dx, raster, id, x, y, w, h);
	{ dev_proc_strip_tile_rectangle((*tile_proc)) =
	    dev_proc(dev, strip_tile_rectangle);
	  const byte *row;
	  int iy;

	  for ( row = data, iy = 0; iy < h; row += raster, iy++ )
	    { int ix;
	      for ( ix = 0; ix < w; )
		{ int i0;
#define row_bit(i) (row[(i) >> 3] & (0x80 >> ((i) & 7)))
		  while ( ix < w && row_bit(ix + dx) == 0 )
		    ix++;
		  for ( i0 = ix; ix < w && row_bit(ix + dx) != 0; )
		    ix++;
		  if ( ix > i0 )
		    { int code = (*tile_proc)
			(dev, tile, i0 + x, iy + y, ix - i0, 1,
			 colors[0], colors[1],
			 gx_dc_phase(pdcolor).x, gx_dc_phase(pdcolor).y);
		      if ( code < 0 )
			return code;
		    }
#undef row_bit
		}
	  }
	}
	return 0;
}

/* Default implementation of strip_tile_rectangle */
int
gx_default_strip_tile_rectangle(gx_device *dev, const gx_strip_bitmap *tiles,
  int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
  int px, int py)
{	/* Fill the rectangle in chunks. */
	int width = tiles->size.x;
	int height = tiles->size.y;
	int raster = tiles->raster;
	int rwidth = tiles->rep_width;
	int rheight = tiles->rep_height;
	int shift = tiles->shift;
	int xoff =
	  (shift == 0 ? px :
	   px + (y + py) / rheight * tiles->rep_shift);
	int irx = ((rwidth & (rwidth - 1)) == 0 ?	/* power of 2 */
		   (x + xoff) & (rwidth - 1) :
		   (x + xoff) % rwidth);
	int ry = ((rheight & (rheight - 1)) == 0 ?	/* power of 2 */
		  (y + py) & (rheight - 1) :
		  (y + py) % rheight);
	int icw = width - irx;
	int ch = height - ry;
	byte *row = tiles->data + ry * raster;
	dev_proc_copy_mono((*proc_mono));
	dev_proc_copy_color((*proc_color));
	int code;

#ifdef DEBUG
if ( gs_debug_c('t') )
   {	int ptx, pty;
	const byte *ptp = tiles->data;
	dprintf3("[t]tile %dx%d raster=%d;",
		tiles->size.x, tiles->size.y, tiles->raster);
	dprintf6(" x,y=%d,%d w,h=%d,%d p=%d,%d\n",
		x, y, w, h, px, py);
	for ( pty = 0; pty < tiles->size.y; pty++ )
	   {	dprintf("   ");
		for ( ptx = 0; ptx < tiles->raster; ptx++ )
			dprintf1("%3x", *ptp++);
	   }
	dputc('\n');
   }
#endif

	/*
	 * We should really make the following check before doing
	 * all the computations above, but since we expect devices
	 * to implement strip_tile_rectangle rather than tile_rectangle,
	 * the check will rarely succeed.
	 */
	if ( dev_proc(dev, tile_rectangle) != gx_default_tile_rectangle )
	  { if ( shift == 0 )
	      { /*
		 * Temporarily patch the tile_rectangle procedure in the
		 * device so we don't get into a recursion loop if the
		 * device has a tile_rectangle procedure that conditionally
		 * calls the strip_tile_rectangle procedure.
		 */
		dev_proc_tile_rectangle((*tile_proc)) =
		  dev_proc(dev, tile_rectangle);
		int code;

		set_dev_proc(dev, tile_rectangle, gx_default_tile_rectangle);
		code = (*tile_proc)
		  (dev, (const gx_tile_bitmap *)tiles, x, y, w, h,
		   color0, color1, px, py);
		set_dev_proc(dev, tile_rectangle, tile_proc);
		return code;
	      }
	    /* We should probably optimize this case too, for the benefit */
	    /* of window systems, but we don't yet. */
	  }

	if ( color0 == gx_no_color_index && color1 == gx_no_color_index )
	  proc_color = dev_proc(dev, copy_color);
	else
	  proc_color = 0, proc_mono = dev_proc(dev, copy_mono);

/****** SHOULD ALSO PASS id IF COPYING A FULL TILE ******/
#define real_copy_tile(srcx, tx, ty, tw, th)\
  code =\
    (proc_color != 0 ?\
     (*proc_color)(dev, row, srcx, raster, gx_no_bitmap_id, tx, ty, tw, th) :\
     (*proc_mono)(dev, row, srcx, raster, gx_no_bitmap_id, tx, ty, tw, th, color0, color1));\
  if ( code < 0 ) return_error(code);\
  return_if_interrupt()
#ifdef DEBUG
#define copy_tile(sx, tx, ty, tw, th)\
  if ( gs_debug_c('t') )\
	dprintf5("   copy sx=%d x=%d y=%d w=%d h=%d\n",\
		 sx, tx, ty, tw, th);\
  real_copy_tile(sx, tx, ty, tw, th)
#else
#define copy_tile(sx, tx, ty, tw, th)\
  real_copy_tile(sx, tx, ty, tw, th)
#endif
	if ( ch >= h )
	   {	/* Shallow operation */
		if ( icw >= w )
		   {	/* Just one (partial) tile to transfer. */
			copy_tile(irx, x, y, w, h);
		   }
		else
		  {	int ex = x + w;
			int fex = ex - width;
			int cx = x + icw;

			copy_tile(irx, x, y, icw, h);
			while ( cx <= fex )
			  {	copy_tile(0, cx, y, width, h);
				cx += width;
			      }
			if ( cx < ex )
			  {	copy_tile(0, cx, y, ex - cx, h);
			  }
		  }
	  }
	else if ( icw >= w && shift == 0 )
	   {	/* Narrow operation, no shift */
		int ey = y + h;
		int fey = ey - height;
		int cy = y + ch;

		copy_tile(irx, x, y, w, ch);
		row = tiles->data;
		do
		   {	ch = (cy > fey ? ey - cy : height);
			copy_tile(irx, x, cy, w, ch);
		   }
		while ( (cy += ch) < ey );
	   }
	else
	   {	/* Full operation.  If shift != 0, some scan lines */
		/* may be narrow.  We could test shift == 0 in advance */
		/* and use a slightly faster loop, but right now */
		/* we don't bother. */
		int ex = x + w, ey = y + h;
		int fex = ex - width, fey = ey - height;
		int cx, cy;

		for ( cy = y; ; )
		   {	if ( icw >= w )
			  { copy_tile(irx, x, cy, w, ch);
			  }
			else
			  { copy_tile(irx, x, cy, icw, ch);
			    cx = x + icw;
			    while ( cx <= fex )
			      { copy_tile(0, cx, cy, width, ch);
				cx += width;
			      }
			    if ( cx < ex )
			      { copy_tile(0, cx, cy, ex - cx, ch);
			      }
			  }
			if ( (cy += ch) >= ey ) break;
			ch = (cy > fey ? ey - cy : height);
			if ( (irx += shift) >= rwidth )
			  irx -= rwidth;
			icw = width - irx;
			row = tiles->data;
		   }
	   }
#undef copy_tile
#undef real_copy_tile
	return 0;
}

int
gx_no_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	return_error(gs_error_unknownerror);	/* not implemented */
}
int
gx_default_strip_copy_rop(gx_device *dev,
  const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int width, int height,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	return (*gx_default_strip_copy_rop_proc)
	  (dev, sdata, sourcex, sraster, id, scolors, textures, tcolors,
	   x, y, width, height, phase_x, phase_y, lop);
}

/* The following is not really a device procedure.  See gxdevice.h. */

/* Create an ordinary memory device for page or band buffering. */
int
gx_default_make_buffer_device(gx_device_memory *mdev,
  gx_device *target, gs_memory_t *mem, bool for_band)
{	const gx_device_memory *mdproto =
	  gdev_mem_device_for_bits(target->color_info.depth);

	if ( mdproto == 0 )
	  return_error(gs_error_rangecheck);
	if ( target == (gx_device *)mdev )
	  mdev->std_procs = mdproto->std_procs;
	else
	  gs_make_mem_device(mdev, mdproto, mem, (for_band ? 1 : 0),
			     (target == (gx_device *)mdev ? 0 : target));
	return 0;
}

/* ---------------- Default per-instance procedures ---------------- */

int
gx_default_install(gx_device *dev, gs_state *pgs)
{	return 0;
}

int
gx_default_begin_page(gx_device *dev, gs_state *pgs)
{	return 0;
}

int
gx_default_end_page(gx_device *dev, int reason, gs_state *pgs)
{	return (reason != 2 ? 1 : 0);
}

/* ---------------- Unaligned copy operations ---------------- */

/*
 * Implementing unaligned operations in terms of the standard aligned
 * operations requires adjusting the bitmap origin and/or the raster
 * to be aligned.  Adjusting the origin is simple, but non-portable,
 * since there is no portable way to determine the alignment of a pointer.
 * Adjusting the raster requires doing the operation one scan line at a time.
 */

int
gx_copy_mono_unaligned(gx_device *dev, const byte *data,
  int dx, int raster, gx_bitmap_id id, int x, int y, int w, int h,
  gx_color_index zero, gx_color_index one)
{	dev_proc_copy_mono((*copy_mono)) = dev_proc(dev, copy_mono);
	uint offset = (uint)(data - (const byte *)0) & (align_bitmap_mod - 1);
	int step = raster & (align_bitmap_mod - 1);

	/* Adjust the origin. */
	data -= offset;
	dx += offset << 3;

	/* Adjust the raster. */
	if ( !step )
	  { /* No adjustment needed. */
	    return (*copy_mono)(dev, data, dx, raster, id,
				x, y, w, h, zero, one);
	  }

	/* Do the transfer one scan line at a time. */
	{ const byte *p = data;
	  int d = dx;
	  int code = 0;
	  int i;

	  for ( i = 0; i < h && code >= 0;
	        ++i, p += raster - step, d += step << 3
	      )
	    code = (*copy_mono)(dev, p, d, raster, gx_no_bitmap_id,
				x, y + i, w, 1, zero, one);
	  return code;
	}
}

int
gx_copy_color_unaligned(gx_device *dev, const byte *data,
  int data_x, int raster, gx_bitmap_id id,
  int x, int y, int width, int height)
{	dev_proc_copy_color((*copy_color)) = dev_proc(dev, copy_color);
	int depth = dev->color_info.depth;
	uint offset = (uint)(data - (const byte *)0) & (align_bitmap_mod - 1);
	int step = raster & (align_bitmap_mod - 1);

	/*
	 * Adjust the origin.
	 * We have to do something very special for 24-bit data,
	 * because that is the only depth that doesn't divide
	 * align_bitmap_mod exactly.  In particular, we need to find
	 * M*B + R == 0 mod 3, where M is align_bitmap_mod, R is the
	 * offset value just calculated, and B is an integer unknown;
	 * the new value of offset will be M*B + R.
	 */
	if ( depth == 24 )
	  offset += (offset % 3) *
	    (align_bitmap_mod * (3 - (align_bitmap_mod % 3)));
	data -= offset;
	data_x += (offset << 3) / depth;

	/* Adjust the raster. */
	if ( !step )
	  { /* No adjustment needed. */
	    return (*copy_color)(dev, data, data_x, raster, id,
				 x, y, width, height);
	  }

	/* Do the transfer one scan line at a time. */
	{ const byte *p = data;
	  int d = data_x;
	  int dstep = (step << 3) / depth;
	  int code = 0;
	  int i;

	  for ( i = 0; i < height && code >= 0;
	        ++i, p += raster - step, d += dstep
	      )
	    code = (*copy_color)(dev, p, d, raster, gx_no_bitmap_id,
				 x, y + i, width, 1);
	  return code;
	}
}

int
gx_copy_alpha_unaligned(gx_device *dev, const byte *data, int data_x,
  int raster, gx_bitmap_id id, int x, int y, int width, int height,
  gx_color_index color, int depth)
{	dev_proc_copy_alpha((*copy_alpha)) = dev_proc(dev, copy_alpha);
	uint offset = (uint)(data - (const byte *)0) & (align_bitmap_mod - 1);
	int step = raster & (align_bitmap_mod - 1);

	/* Adjust the origin. */
	data -= offset;
	data_x += (offset << 3) / depth;

	/* Adjust the raster. */
	if ( !step )
	  { /* No adjustment needed. */
	    return (*copy_alpha)(dev, data, data_x, raster, id,
				 x, y, width, height, color, depth);
	  }

	/* Do the transfer one scan line at a time. */
	{ const byte *p = data;
	  int d = data_x;
	  int dstep = (step << 3) / depth;
	  int code = 0;
	  int i;

	  for ( i = 0; i < height && code >= 0;
	        ++i, p += raster - step, d += dstep
	      )
	    code = (*copy_alpha)(dev, p, d, raster, gx_no_bitmap_id,
				 x, y + i, width, 1, color, depth);
	  return code;
	}
}
