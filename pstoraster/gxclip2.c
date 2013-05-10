/* Copyright (C) 1993, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclip2.c */
/* Mask clipping for patterns */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclip2.h"

private_st_device_tile_clip();

/* Device procedures */
private dev_proc_fill_rectangle(tile_clip_fill_rectangle);
private dev_proc_copy_mono(tile_clip_copy_mono);
private dev_proc_copy_color(tile_clip_copy_color);
private dev_proc_copy_alpha(tile_clip_copy_alpha);
private dev_proc_strip_copy_rop(tile_clip_strip_copy_rop);

/* The device descriptor. */
private const gx_device_tile_clip gs_tile_clip_device =
{	std_device_std_body_open(gx_device_tile_clip, 0, "tile clipper",
	  0, 0, 1, 1),
	{	gx_default_open_device,
		gx_forward_get_initial_matrix,
		gx_default_sync_output,
		gx_default_output_page,
		gx_default_close_device,
		gx_forward_map_rgb_color,
		gx_forward_map_color_rgb,
		tile_clip_fill_rectangle,
		gx_default_tile_rectangle,
		tile_clip_copy_mono,
		tile_clip_copy_color,
		gx_default_draw_line,
		gx_default_get_bits,
		gx_forward_get_params,
		gx_forward_put_params,
		gx_forward_map_cmyk_color,
		gx_forward_get_xfont_procs,
		gx_forward_get_xfont_device,
		gx_forward_map_rgb_alpha_color,
		gx_forward_get_page_device,
		gx_forward_get_alpha_bits,
		tile_clip_copy_alpha,
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
		tile_clip_strip_copy_rop,
	}
};

/* Initialize a tile clipping device from a mask. */
int
tile_clip_initialize(gx_device_tile_clip *cdev, const gx_strip_bitmap *tiles,
  gx_device *idev, int px, int py)
{	int buffer_width = tiles->size.x;
	int buffer_height =
	  tile_clip_buffer_size / (tiles->raster + sizeof(byte *));

	*cdev = gs_tile_clip_device;
	cdev->width = idev->width;
	cdev->height = idev->height;
	cdev->color_info = idev->color_info;
	cdev->target = idev;
	cdev->tiles = *tiles;
	tile_clip_set_phase(cdev, px, py);
	if ( buffer_height > tiles->size.y )
	  buffer_height = tiles->size.y;
	gs_make_mem_mono_device(&cdev->mdev, 0, 0);
	for ( ; ; )
	{	if ( buffer_height <= 0 )
		  return_error(gs_error_rangecheck);
		cdev->mdev.width = buffer_width;
		cdev->mdev.height = buffer_height;
		if ( gdev_mem_bitmap_size(&cdev->mdev) <= tile_clip_buffer_size )
		  break;
		buffer_height--;
	}
	cdev->mdev.base = cdev->buffer.bytes;
	return (*dev_proc(&cdev->mdev, open_device))((gx_device *)&cdev->mdev);
}

/* Set the phase of the tile. */
void
tile_clip_set_phase(gx_device_tile_clip *cdev, int px, int py)
{	cdev->phase.x = px;
	cdev->phase.y = py;
}

#define cdev ((gx_device_tile_clip *)dev)

/* Fill a rectangle by tiling with the mask. */
private int
tile_clip_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	gx_device *tdev = cdev->target;
	return (*dev_proc(tdev, strip_tile_rectangle))(tdev, &cdev->tiles,
		x, y, w, h,
		gx_no_color_index, color, cdev->phase.x, cdev->phase.y);
}

/* Calculate the X offset corresponding to a given Y, taking the phase */
/* and shift into account. */
#define x_offset(ty, cdev)\
  ((cdev)->phase.x + (((ty) + (cdev)->phase.y) / (cdev)->tiles.rep_height) *\
   (cdev)->tiles.rep_shift)

/* Copy a monochrome bitmap.  We divide it up into maximal chunks */
/* that line up with a single tile, and then do the obvious Boolean */
/* combination of the tile mask and the source. */
private int
tile_clip_copy_mono(gx_device *dev,
  const byte *data, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h,
  gx_color_index color0, gx_color_index color1)
{	gx_color_index color, mcolor0, mcolor1;
	int ty, ny;
	int code;
	if ( color1 != gx_no_color_index )
	{	if ( color0 != gx_no_color_index )
		{	/* Pre-fill with color0. */
			code = tile_clip_fill_rectangle(dev, x, y, w, h, color0);
			if ( code < 0 )
			  return code;
		}
		color = color1;
		mcolor0 = 0, mcolor1 = gx_no_color_index;
	}
	else if ( color0 != gx_no_color_index )
	{	color = color0;
		mcolor0 = gx_no_color_index, mcolor1 = 0;
	}
	else
		return 0;
	for ( ty = y; ty < y + h; ty += ny )
	{	int tx, nx;
		int cy = (ty + cdev->phase.y) % cdev->tiles.rep_height;
		int xoff = x_offset(ty, cdev);

		ny = min(y + h - ty, cdev->tiles.size.y - cy);
		if ( ny > cdev->mdev.height )
			ny = cdev->mdev.height;
		for ( tx = x; tx < x + w; tx += nx )
		{	int cx = (tx + xoff) % cdev->tiles.rep_width;
			nx = min(x + w - tx, cdev->tiles.size.x - cx);
			/* Copy a tile slice to the memory device buffer. */
			memcpy(cdev->buffer.bytes,
			       cdev->tiles.data + cy * cdev->tiles.raster,
			       cdev->tiles.raster * ny);
			/* Intersect the tile with the source data. */
			/* mcolor0 and mcolor1 invert the data if needed. */
			/* This call can't fail. */
			(*dev_proc(&cdev->mdev, copy_mono))((gx_device *)&cdev->mdev,
			  data + (ty - y) * raster, sourcex + tx - x,
			  raster, gx_no_bitmap_id,
			  cx, 0, nx, ny, mcolor0, mcolor1);
			/* Now copy the color through the double mask. */
			code = (*dev_proc(cdev->target, copy_mono))(cdev->target,
			  cdev->buffer.bytes, cx, cdev->tiles.raster,
			  gx_no_bitmap_id,
			  tx, ty, nx, ny, gx_no_color_index, color);
			if ( code < 0 )
			  return code;
		}
	}
	return 0;
}

/* Copy a color rectangle.  We can't use the BitBlt tricks; we have to */
/* scan for runs of 1s.  There are many obvious ways to speed this up; */
/* we'll implement some if we need to. */
private int
tile_clip_copy_color(gx_device *dev,
  const byte *data, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	const byte *data_row = data;
	int cy = (y + cdev->phase.y) % cdev->tiles.rep_height;
	const byte *tile_row = cdev->tiles.data + cy * cdev->tiles.raster;
	int ty;

	for ( ty = y; ty < y + h; ty++, data_row += raster )
	{	int cx = (x + x_offset(ty, cdev)) % cdev->tiles.rep_width;
		const byte *tp = tile_row + (cx >> 3);
		byte tbit = 0x80 >> (cx & 7);
		int tx;
		int code;

		for ( tx = x; tx < x + w; )
		{	int tx1;
#define t_next()\
  if ( ++cx == cdev->tiles.size.x )\
    cx = 0, tp = tile_row, tbit = 0x80;\
  else if ( (tbit >>= 1) == 0 )\
    tp++, tbit = 0x80;\
  tx++
			/* Skip a run of 0s. */
			while ( tx < x + w && (*tp & tbit) == 0 )
			{	t_next();
			}
			if ( tx == x + w )
				break;
			/* Scan a run of 1s. */
			tx1 = tx;
			do
			{	t_next();
			}
			while ( tx < x + w && (*tp & tbit) != 0 );
			/* Copy the run. */
			code = (*dev_proc(cdev->target, copy_color))(cdev->target,
			  data_row, sourcex + tx1 - x, raster,
			  gx_no_bitmap_id, tx1, ty, tx - tx1, 1);
			if ( code < 0 )
			  return code;
		}
		if ( ++cy == cdev->tiles.size.y )
			cy = 0, tile_row = cdev->tiles.data;
		else
			tile_row += cdev->tiles.raster;
	}

	return 0;
}

/* Copy an alpha rectangle similarly. */
private int
tile_clip_copy_alpha(gx_device *dev,
  const byte *data, int sourcex, int raster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index color, int depth)
{	const byte *data_row = data;
	int ty;

	for ( ty = y; ty < y + h; ty++, data_row += raster )
	{	const byte *tile_row = cdev->tiles.data +
			((ty + cdev->phase.y) % cdev->tiles.rep_height) *
			cdev->tiles.raster;
		int cx = (x + x_offset(ty, cdev)) % cdev->tiles.rep_width;
		const byte *tp = tile_row + (cx >> 3);
		byte tbit = 0x80 >> (cx & 7);
		int tx;
		int code;

		for ( tx = x; tx < x + w; )
		{	int tx1;
#define t_next()\
  if ( ++cx == cdev->tiles.size.x )\
    cx = 0, tp = tile_row, tbit = 0x80;\
  else if ( (tbit >>= 1) == 0 )\
    tp++, tbit = 0x80;\
  tx++
			/* Skip a run of 0s. */
			while ( tx < x + w && (*tp & tbit) == 0 )
			{	t_next();
			}
			if ( tx == x + w )
				break;
			/* Scan a run of 1s. */
			tx1 = tx;
			do
			{	t_next();
			}
			while ( tx < x + w && (*tp & tbit) != 0 );
			/* Copy the run. */
			code = (*dev_proc(cdev->target, copy_alpha))(cdev->target,
			  data_row, sourcex + tx1 - x, raster,
			  gx_no_bitmap_id, tx1, ty, tx - tx1, 1,
			  color, depth);
			if ( code < 0 )
			  return code;
		}
	}

	return 0;
}

/* Copy a RasterOp rectangle similarly. */
private int
tile_clip_strip_copy_rop(gx_device *dev,
  const byte *data, int sourcex, uint raster, gx_bitmap_id id,
  const gx_color_index *scolors,
  const gx_strip_bitmap *textures, const gx_color_index *tcolors,
  int x, int y, int w, int h,
  int phase_x, int phase_y, gs_logical_operation_t lop)
{	const byte *data_row = data;
	int ty;

	for ( ty = y; ty < y + h; ty++, data_row += raster )
	{	const byte *tile_row = cdev->tiles.data +
			((ty + cdev->phase.y) % cdev->tiles.rep_height) *
			cdev->tiles.raster;
		int cx = (x + x_offset(ty, cdev)) % cdev->tiles.rep_width;
		const byte *tp = tile_row + (cx >> 3);
		byte tbit = 0x80 >> (cx & 7);
		int tx;
		int code;

		for ( tx = x; tx < x + w; )
		{	int tx1;
#define t_next()\
  if ( ++cx == cdev->tiles.size.x )\
    cx = 0, tp = tile_row, tbit = 0x80;\
  else if ( (tbit >>= 1) == 0 )\
    tp++, tbit = 0x80;\
  tx++
			/* Skip a run of 0s. */
			while ( tx < x + w && (*tp & tbit) == 0 )
			{	t_next();
			}
			if ( tx == x + w )
				break;
			/* Scan a run of 1s. */
			tx1 = tx;
			do
			{	t_next();
			}
			while ( tx < x + w && (*tp & tbit) != 0 );
			/* Copy the run. */
			code = (*dev_proc(cdev->target, strip_copy_rop))
			  (cdev->target,
			   data_row, sourcex + tx1 - x, raster,
			   gx_no_bitmap_id, scolors, textures, tcolors,
			   tx1, ty, tx - tx1, 1, phase_x, phase_y, lop);
			if ( code < 0 )
			  return code;
		}
	}

	return 0;
}
