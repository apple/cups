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

/*$Id: gxclipm.c,v 1.1 2000/03/08 23:14:52 mike Exp $ */
/* Mask clipping device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclipm.h"

/* Device procedures */
private dev_proc_fill_rectangle(mask_clip_fill_rectangle);
private dev_proc_copy_mono(mask_clip_copy_mono);
private dev_proc_copy_color(mask_clip_copy_color);
private dev_proc_copy_alpha(mask_clip_copy_alpha);
private dev_proc_strip_copy_rop(mask_clip_strip_copy_rop);
private dev_proc_get_clipping_box(mask_clip_get_clipping_box);

/* The device descriptor. */
const gx_device_mask_clip gs_mask_clip_device =
{std_device_std_body_open(gx_device_mask_clip, 0, "mask clipper",
			  0, 0, 1, 1),
 {gx_default_open_device,
  gx_forward_get_initial_matrix,
  gx_default_sync_output,
  gx_default_output_page,
  gx_default_close_device,
  gx_forward_map_rgb_color,
  gx_forward_map_color_rgb,
  mask_clip_fill_rectangle,
  gx_default_tile_rectangle,
  mask_clip_copy_mono,
  mask_clip_copy_color,
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
  mask_clip_copy_alpha,
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
  mask_clip_strip_copy_rop,
  mask_clip_get_clipping_box,
  gx_default_begin_typed_image,
  gx_forward_get_bits_rectangle,
  gx_forward_map_color_rgb_alpha,
  gx_no_create_compositor,
  gx_forward_get_hardware_params,
  gx_default_text_begin
 }
};

/* Fill a rectangle by painting through the mask. */
private int
mask_clip_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			 gx_color_index color)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;

    /* Clip the rectangle to the region covered by the mask. */
    int mx0 = x + cdev->phase.x, my0 = y + cdev->phase.y;
    int mx1 = mx0 + w, my1 = my0 + h;

    if (mx0 < 0)
	mx0 = 0;
    if (my0 < 0)
	my0 = 0;
    if (mx1 > cdev->tiles.size.x)
	mx1 = cdev->tiles.size.x;
    if (my1 > cdev->tiles.size.y)
	my1 = cdev->tiles.size.y;
    return (*dev_proc(tdev, copy_mono))
	(tdev, cdev->tiles.data + my0 * cdev->tiles.raster, mx0,
	 cdev->tiles.raster, cdev->tiles.id,
	 mx0 - cdev->phase.x, my0 - cdev->phase.y,
	 mx1 - mx0, my1 - my0, gx_no_color_index, color);
}

/*
 * Clip the rectangle for a copy operation.
 * Sets m{x,y}{0,1} to the region in the mask coordinate system;
 * subtract cdev->phase.{x,y} to get target coordinates.
 * Sets sdata, sx to adjusted values of data, sourcex.
 * References cdev, data, sourcex, raster, x, y, w, h.
 */
#define DECLARE_MASK_COPY\
	const byte *sdata;\
	int sx, mx0, my0, mx1, my1
#define FIT_MASK_COPY(data, sourcex, raster, vx, vy, vw, vh)\
	BEGIN\
	  sdata = data, sx = sourcex;\
	  mx0 = vx + cdev->phase.x, my0 = vy + cdev->phase.y;\
	  mx1 = mx0 + vw, my1 = my0 + vh;\
	  if ( mx0 < 0 )\
	    sx -= mx0, mx0 = 0;\
	  if ( my0 < 0 )\
	    sdata -= my0 * raster, my0 = 0;\
	  if ( mx1 > cdev->tiles.size.x )\
	    mx1 = cdev->tiles.size.x;\
	  if ( my1 > cdev->tiles.size.y )\
	    my1 = cdev->tiles.size.y;\
	END

/* Copy a monochrome bitmap by playing Boolean games. */
private int
mask_clip_copy_mono(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		    int x, int y, int w, int h,
		    gx_color_index color0, gx_color_index color1)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;
    gx_color_index color, mcolor0, mcolor1;

    DECLARE_MASK_COPY;
    int cy, ny;
    int code;

    setup_mask_copy_mono(cdev, color, mcolor0, mcolor1);
    FIT_MASK_COPY(data, sourcex, raster, x, y, w, h);
    for (cy = my0; cy < my1; cy += ny) {
	int ty = cy - cdev->phase.y;
	int cx, nx;

	ny = my1 - cy;
	if (ny > cdev->mdev.height)
	    ny = cdev->mdev.height;
	for (cx = mx0; cx < mx1; cx += nx) {
	    int tx = cx - cdev->phase.x;

	    nx = mx1 - cx;	/* also should be min */
	    /* Copy a tile slice to the memory device buffer. */
	    memcpy(cdev->buffer.bytes,
		   cdev->tiles.data + cy * cdev->tiles.raster,
		   cdev->tiles.raster * ny);
	    /* Intersect the tile with the source data. */
	    /* mcolor0 and mcolor1 invert the data if needed. */
	    /* This call can't fail. */
	    (*dev_proc(&cdev->mdev, copy_mono)) ((gx_device *) & cdev->mdev,
				     sdata + (ty - y) * raster, sx + tx - x,
						 raster, gx_no_bitmap_id,
					   cx, 0, nx, ny, mcolor0, mcolor1);
	    /* Now copy the color through the double mask. */
	    code = (*dev_proc(tdev, copy_mono)) (cdev->target,
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
 * Define the run enumerator for the other copying operations.  We can't use
 * the BitBlt tricks: we have to scan for runs of 1s.  There are obvious
 * ways to speed this up; we'll implement some if we need to.
 */
private int
clip_runs_enumerate(gx_device_mask_clip * cdev,
		    int (*process) (P5(clip_callback_data_t * pccd, int xc, int yc, int xec, int yec)),
		    clip_callback_data_t * pccd)
{
    DECLARE_MASK_COPY;
    int cy;
    const byte *tile_row;

    FIT_MASK_COPY(pccd->data, pccd->sourcex, pccd->raster,
		  pccd->x, pccd->y, pccd->w, pccd->h);
    tile_row = cdev->tiles.data + my0 * cdev->tiles.raster + (mx0 >> 3);
    for (cy = my0; cy < my1; cy++) {
	int cx = mx0;
	const byte *tp = tile_row;
	byte tbit = 0x80 >> (cx & 7);

	while (cx < mx1) {
	    int tx1, tx, ty;
	    int code;

	    /* Skip a run of 0s. */
	    while (cx < mx1 && (*tp & tbit) == 0) {
		if ((tbit >>= 1) == 0)
		    tp++, tbit = 0x80;
		++cx;
	    }
	    if (cx == mx1)
		break;
	    /* Scan a run of 1s. */
	    tx1 = cx - cdev->phase.x;
	    do {
		if ((tbit >>= 1) == 0)
		    tp++, tbit = 0x80;
		++cx;
	    } while (cx < mx1 && (*tp & tbit) != 0);
	    tx = cx - cdev->phase.x;
	    ty = cy - cdev->phase.y;
	    code = (*process) (pccd, tx1, ty, tx, ty + 1);
	    if (code < 0)
		return code;
	}
	tile_row += cdev->tiles.raster;
    }
    return 0;
}

/* Copy a color rectangle */
private int
mask_clip_copy_color(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		     int x, int y, int w, int h)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    return clip_runs_enumerate(cdev, clip_call_copy_color, &ccdata);
}

/* Copy a rectangle with alpha */
private int
mask_clip_copy_alpha(gx_device * dev,
		const byte * data, int sourcex, int raster, gx_bitmap_id id,
		int x, int y, int w, int h, gx_color_index color, int depth)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.color[0] = color, ccdata.depth = depth;
    return clip_runs_enumerate(cdev, clip_call_copy_alpha, &ccdata);
}

private int
mask_clip_strip_copy_rop(gx_device * dev,
	       const byte * data, int sourcex, uint raster, gx_bitmap_id id,
			 const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			 int x, int y, int w, int h,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    clip_callback_data_t ccdata;

    ccdata.tdev = cdev->target;
    ccdata.x = x, ccdata.y = y, ccdata.w = w, ccdata.h = h;
    ccdata.data = data, ccdata.sourcex = sourcex, ccdata.raster = raster;
    ccdata.scolors = scolors, ccdata.textures = textures,
	ccdata.tcolors = tcolors;
    ccdata.phase.x = phase_x, ccdata.phase.y = phase_y, ccdata.lop = lop;
    return clip_runs_enumerate(cdev, clip_call_strip_copy_rop, &ccdata);
}

private void
mask_clip_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_mask_clip *cdev = (gx_device_mask_clip *) dev;
    gx_device *tdev = cdev->target;
    gs_fixed_rect tbox;

    (*dev_proc(tdev, get_clipping_box)) (tdev, &tbox);
    pbox->p.x = tbox.p.x - cdev->phase.x;
    pbox->p.y = tbox.p.y - cdev->phase.y;
    pbox->q.x = tbox.q.x - cdev->phase.x;
    pbox->q.y = tbox.q.y - cdev->phase.y;
}
