/* Copyright (C) 1993, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclip2.c,v 1.2 2000/03/08 23:14:52 mike Exp $ */
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
{std_device_std_body_open(gx_device_tile_clip, 0, "tile clipper",
			  0, 0, 1, 1),
 {gx_default_open_device,
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
  gx_forward_get_bits,
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
  gx_forward_get_clipping_box,
  gx_default_begin_typed_image,
  gx_forward_get_bits_rectangle,
  gx_forward_map_color_rgb_alpha,
  gx_no_create_compositor,
  gx_forward_get_hardware_params,
  gx_default_text_begin
 }
};

/* Initialize a tile clipping device from a mask. */
int
tile_clip_initialize(gx_device_tile_clip * cdev, const gx_strip_bitmap * tiles,
		     gx_device * tdev, int px, int py)
{
    int code =
    gx_mask_clip_initialize(cdev, &gs_tile_clip_device,
			    (const gx_bitmap *)tiles,
			    tdev, 0, 0);	/* phase will be reset */

    if (code >= 0) {
	cdev->tiles = *tiles;
	tile_clip_set_phase(cdev, px, py);
    }
    return code;
}

/* Set the phase of the tile. */
void
tile_clip_set_phase(gx_device_tile_clip * cdev, int px, int py)
{
    cdev->phase.x = px;
    cdev->phase.y = py;
}

/* Fill a rectangle by tiling with the mask. */
private int
tile_clip_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			 gx_color_index color)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;
    gx_device *tdev = cdev->target;

    return (*dev_proc(tdev, strip_tile_rectangle)) (tdev, &cdev->tiles,
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
tile_clip_copy_mono(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		    int x, int y, int w, int h,
		    gx_color_index color0, gx_color_index color1)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;
    gx_color_index color, mcolor0, mcolor1;
    int ty, ny;
    int code;

    setup_mask_copy_mono(cdev, color, mcolor0, mcolor1);
    for (ty = y; ty < y + h; ty += ny) {
	int tx, nx;
	int cy = (ty + cdev->phase.y) % cdev->tiles.rep_height;
	int xoff = x_offset(ty, cdev);

	ny = min(y + h - ty, cdev->tiles.size.y - cy);
	if (ny > cdev->mdev.height)
	    ny = cdev->mdev.height;
	for (tx = x; tx < x + w; tx += nx) {
	    int cx = (tx + xoff) % cdev->tiles.rep_width;

	    nx = min(x + w - tx, cdev->tiles.size.x - cx);
	    /* Copy a tile slice to the memory device buffer. */
	    memcpy(cdev->buffer.bytes,
		   cdev->tiles.data + cy * cdev->tiles.raster,
		   cdev->tiles.raster * ny);
	    /* Intersect the tile with the source data. */
	    /* mcolor0 and mcolor1 invert the data if needed. */
	    /* This call can't fail. */
	    (*dev_proc(&cdev->mdev, copy_mono)) ((gx_device *) & cdev->mdev,
				 data + (ty - y) * raster, sourcex + tx - x,
						 raster, gx_no_bitmap_id,
					   cx, 0, nx, ny, mcolor0, mcolor1);
	    /* Now copy the color through the double mask. */
	    code = (*dev_proc(cdev->target, copy_mono)) (cdev->target,
				 cdev->buffer.bytes, cx, cdev->tiles.raster,
							 gx_no_bitmap_id,
				  tx, ty, nx, ny, gx_no_color_index, color);
	    if (code < 0)
		return code;
	}
    }
    return 0;
}

/*
 * Define the skeleton for the other copying operations.  We can't use the
 * BitBlt tricks: we have to scan for runs of 1s.  There are many obvious
 * ways to speed this up; we'll implement some if we need to.  The schema
 * is:
 *      FOR_RUNS(data_row, tx1, tx, ty) {
 *        ... process the run ([tx1,tx),ty) ...
 *      } END_FOR_RUNS();
 * Free variables: cdev, data, sourcex, raster, x, y, w, h.
 */
#define t_next(tx)\
  BEGIN {\
    if ( ++cx == cdev->tiles.size.x )\
      cx = 0, tp = tile_row, tbit = 0x80;\
    else if ( (tbit >>= 1) == 0 )\
      tp++, tbit = 0x80;\
    tx++;\
  } END
#define FOR_RUNS(data_row, tx1, tx, ty)\
	const byte *data_row = data;\
	int cy = (y + cdev->phase.y) % cdev->tiles.rep_height;\
	const byte *tile_row = cdev->tiles.data + cy * cdev->tiles.raster;\
	int ty;\
\
	for ( ty = y; ty < y + h; ty++, data_row += raster ) {\
	  int cx = (x + x_offset(ty, cdev)) % cdev->tiles.rep_width;\
	  const byte *tp = tile_row + (cx >> 3);\
	  byte tbit = 0x80 >> (cx & 7);\
	  int tx;\
\
	  for ( tx = x; tx < x + w; ) {\
	    int tx1;\
\
	    /* Skip a run of 0s. */\
	    while ( tx < x + w && (*tp & tbit) == 0 )\
	      t_next(tx);\
	    if ( tx == x + w )\
	      break;\
	    /* Scan a run of 1s. */\
	    tx1 = tx;\
	    do {\
	      t_next(tx);\
	    } while ( tx < x + w && (*tp & tbit) != 0 );\
	    if_debug3('T', "[T]run x=(%d,%d), y=%d\n", tx1, tx, ty);
/* (body goes here) */
#define END_FOR_RUNS()\
	  }\
	  if ( ++cy == cdev->tiles.size.y )\
	    cy = 0, tile_row = cdev->tiles.data;\
	  else\
	    tile_row += cdev->tiles.raster;\
	}

/* Copy a color rectangle. */
private int
tile_clip_copy_color(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		     int x, int y, int w, int h)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    FOR_RUNS(data_row, txrun, tx, ty) {
	/* Copy the run. */
	int code = (*dev_proc(cdev->target, copy_color))
	(cdev->target, data_row, sourcex + txrun - x, raster,
	 gx_no_bitmap_id, txrun, ty, tx - txrun, 1);

	if (code < 0)
	    return code;
    }
    END_FOR_RUNS();
    return 0;
}

/* Copy an alpha rectangle similarly. */
private int
tile_clip_copy_alpha(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		int x, int y, int w, int h, gx_color_index color, int depth)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    FOR_RUNS(data_row, txrun, tx, ty) {
	/* Copy the run. */
	int code = (*dev_proc(cdev->target, copy_alpha))
	(cdev->target, data_row, sourcex + txrun - x, raster,
	 gx_no_bitmap_id, txrun, ty, tx - txrun, 1, color, depth);

	if (code < 0)
	    return code;
    }
    END_FOR_RUNS();
    return 0;
}

/* Copy a RasterOp rectangle similarly. */
private int
tile_clip_strip_copy_rop(gx_device * dev,
	       const byte * data, int sourcex, uint raster, gx_bitmap_id id,
			 const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			 int x, int y, int w, int h,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_tile_clip *cdev = (gx_device_tile_clip *) dev;

    FOR_RUNS(data_row, txrun, tx, ty) {
	/* Copy the run. */
	int code = (*dev_proc(cdev->target, strip_copy_rop))
	(cdev->target, data_row, sourcex + txrun - x, raster,
	 gx_no_bitmap_id, scolors, textures, tcolors,
	 txrun, ty, tx - txrun, 1, phase_x, phase_y, lop);

	if (code < 0)
	    return code;
    }
    END_FOR_RUNS();
    return 0;
}
