/* Copyright (C) 1994, 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevabuf.c,v 1.2 2000/03/08 23:14:21 mike Exp $ */
/* Alpha-buffering memory devices */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* semi-public definitions */
#include "gdevmem.h"		/* private definitions */

/* ================ Alpha devices ================ */

/*
 * These devices store 2 or 4 bits of alpha.  They are a hybrid of a
 * monobit device (for color mapping) and a 2- or 4-bit device (for painting).
 * Currently, we only use them for character rasterizing, but they might be
 * useful for other things someday.
 */

/* We can't initialize the device descriptor statically very well, */
/* so we patch up the image2 or image4 descriptor. */
private dev_proc_map_rgb_color(mem_alpha_map_rgb_color);
private dev_proc_map_color_rgb(mem_alpha_map_color_rgb);
private dev_proc_map_rgb_alpha_color(mem_alpha_map_rgb_alpha_color);
private dev_proc_get_alpha_bits(mem_alpha_get_alpha_bits);
private dev_proc_copy_alpha(mem_alpha_copy_alpha);

void
gs_make_mem_alpha_device(gx_device_memory * adev, gs_memory_t * mem,
			 gx_device * target, int alpha_bits)
{
    gs_make_mem_device(adev, gdev_mem_device_for_bits(alpha_bits),
		       mem, 0, target);
    /* This is a black-and-white device ... */
    adev->color_info = gdev_mem_device_for_bits(1)->color_info;
    /* ... but it has multiple bits per pixel ... */
    adev->color_info.depth = alpha_bits;
    /* ... and different color mapping. */
    set_dev_proc(adev, map_rgb_color, mem_alpha_map_rgb_color);
    set_dev_proc(adev, map_color_rgb, mem_alpha_map_color_rgb);
    set_dev_proc(adev, map_rgb_alpha_color, mem_alpha_map_rgb_alpha_color);
    set_dev_proc(adev, get_alpha_bits, mem_alpha_get_alpha_bits);
    set_dev_proc(adev, copy_alpha, mem_alpha_copy_alpha);
}

/* Reimplement color mapping. */
private gx_color_index
mem_alpha_map_rgb_color(gx_device * dev, gx_color_value r, gx_color_value g,
			gx_color_value b)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gx_color_index color = gx_forward_map_rgb_color(dev, r, g, b);

    return (color == 0 || color == gx_no_color_index ? color :
	    (gx_color_index) ((1 << mdev->log2_alpha_bits) - 1));
}
private int
mem_alpha_map_color_rgb(gx_device * dev, gx_color_index color,
			gx_color_value prgb[3])
{
    return
	gx_forward_map_color_rgb(dev,
				 (color == 0 ? color : (gx_color_index) 1),
				 prgb);
}
private gx_color_index
mem_alpha_map_rgb_alpha_color(gx_device * dev, gx_color_value r,
		   gx_color_value g, gx_color_value b, gx_color_value alpha)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gx_color_index color = gx_forward_map_rgb_color(dev, r, g, b);

    return (color == 0 || color == gx_no_color_index ? color :
	    (gx_color_index) (alpha >> (gx_color_value_bits -
					mdev->log2_alpha_bits)));
}
private int
mem_alpha_get_alpha_bits(gx_device * dev, graphics_object_type type)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    return 1 << mdev->log2_alpha_bits;
}
/* Implement alpha copying. */
private int
mem_alpha_copy_alpha(gx_device * dev, const byte * data, int data_x,
	   int raster, gx_bitmap_id id, int x, int y, int width, int height,
		     gx_color_index color, int depth)
{				/* Just use copy_color. */
    return (color == 0 ?
	    (*dev_proc(dev, fill_rectangle)) (dev, x, y, width, height,
					      color) :
	    (*dev_proc(dev, copy_color)) (dev, data, data_x, raster, id,
					  x, y, width, height));
}

/* ================ Alpha-buffer device ================ */

/*
 * This device converts graphics sampled at a higher resolution to
 * alpha values at a lower resolution.  It does this by accumulating
 * the bits of a band and then converting the band to alphas.
 * In order to make this work, the client of the device must promise
 * only to visit each band at most once, except possibly for a single
 * scan line overlapping the adjacent band, and must promise only to write
 * a single color into the output.  In particular, this works
 * within a single call on gx_fill_path (if the fill loop is constrained
 * to process bands of limited height on each pass) or a single masked image
 * scanned in Y order, but not across such calls and not for other
 * kinds of painting operations.
 *
 * We implement this device as a subclass of a monobit memory device.
 * (We put its state in the definition of gx_device_memory just because
 * actual subclassing introduces a lot of needless boilerplate.)
 * We only allocate enough bits for one band.  The height of the band
 * must be a multiple of the Y scale factor; the minimum height
 * of the band is twice the Y scale factor.
 *
 * The bits in storage are actually a sliding window on the true
 * oversampled image.  To avoid having to copy the bits around when we
 * move the window, we adjust the mapping between the client's Y values
 * and our own, as follows:
 *      Client          Stored
 *      ------          ------
 *      y0..y0+m-1      n-m..n-1
 *      y0+m..y0+n-1    0..n-m-1
 * where n and m are multiples of the Y scale factor and 0 <= m <= n <=
 * the height of the band.  (In the device structure, m is called
 * mapped_start and n is called mapped_height.)  This allows us to slide
 * the window incrementally in either direction without copying any bits.
 */

/* Procedures */
private dev_proc_close_device(mem_abuf_close);
private dev_proc_copy_mono(mem_abuf_copy_mono);
private dev_proc_fill_rectangle(mem_abuf_fill_rectangle);
private dev_proc_get_clipping_box(mem_abuf_get_clipping_box);

/* The device descriptor. */
private const gx_device_memory mem_alpha_buffer_device =
mem_device("image(alpha buffer)", 0, 1,
	   gx_forward_map_rgb_color, gx_forward_map_color_rgb,
	 mem_abuf_copy_mono, gx_default_copy_color, mem_abuf_fill_rectangle,
	   gx_no_strip_copy_rop);

/* Make an alpha-buffer memory device. */
/* We use abuf instead of alpha_buffer because */
/* gcc under VMS only retains 23 characters of procedure names. */
void
gs_make_mem_abuf_device(gx_device_memory * adev, gs_memory_t * mem,
		     gx_device * target, const gs_log2_scale_point * pscale,
			int alpha_bits, int mapped_x)
{
    gs_make_mem_device(adev, &mem_alpha_buffer_device, mem, 0, target);
    adev->max_fill_band = 1 << pscale->y;
    adev->log2_scale = *pscale;
    adev->log2_alpha_bits = alpha_bits >> 1;	/* works for 1,2,4 */
    adev->mapped_x = mapped_x;
    set_dev_proc(adev, close_device, mem_abuf_close);
    set_dev_proc(adev, get_clipping_box, mem_abuf_get_clipping_box);
}

/* Test whether a device is an alpha-buffering device. */
bool
gs_device_is_abuf(const gx_device * dev)
{				/* We can't just compare the procs, or even an individual proc, */
    /* because we might be tracing.  Instead, check the identity of */
    /* the device name. */
    return dev->dname == mem_alpha_buffer_device.dname;
}

/* Internal routine to flush a block of the buffer. */
/* A block is a group of scan lines whose initial Y is a multiple */
/* of the Y scale and whose height is equal to the Y scale. */
private int
abuf_flush_block(gx_device_memory * adev, int y)
{
    gx_device *target = adev->target;
    int block_height = 1 << adev->log2_scale.y;
    int alpha_bits = 1 << adev->log2_alpha_bits;
    int ddepth =
    (adev->width >> adev->log2_scale.x) << adev->log2_alpha_bits;
    uint draster = bitmap_raster(ddepth);
    int buffer_y = y - adev->mapped_y + adev->mapped_start;
    byte *bits;

    if (buffer_y >= adev->height)
	buffer_y -= adev->height;
    bits = scan_line_base(adev, buffer_y);
    {				/*
				 * Many bits are typically zero.  Save time by computing
				 * an accurate X bounding box before compressing.
				 * Unfortunately, in order to deal with alpha nibble swapping
				 * (see gsbitops.c), we can't expand the box only to pixel
				 * boundaries:
				 int alpha_mask = -1 << adev->log2_alpha_bits;
				 * Instead, we must expand it to byte boundaries,
				 */
	int alpha_mask = ~7;
	gs_int_rect bbox;
	int width;

	bits_bounding_box(bits, block_height, adev->raster, &bbox);
	bbox.p.x &= alpha_mask;
	bbox.q.x = (bbox.q.x + ~alpha_mask) & alpha_mask;
	width = bbox.q.x - bbox.p.x;
	bits_compress_scaled(bits, bbox.p.x, width, block_height,
			     adev->raster, bits, draster, &adev->log2_scale,
			     adev->log2_alpha_bits);
	return (*dev_proc(target, copy_alpha)) (target,
					  bits, 0, draster, gx_no_bitmap_id,
					      (adev->mapped_x + bbox.p.x) >>
						adev->log2_scale.x,
						y >> adev->log2_scale.y,
					     width >> adev->log2_scale.x, 1,
					      adev->save_color, alpha_bits);
    }
}
/* Flush the entire buffer. */
private int
abuf_flush(gx_device_memory * adev)
{
    int y, code = 0;
    int block_height = 1 << adev->log2_scale.y;

    for (y = 0; y < adev->mapped_height; y += block_height)
	if ((code = abuf_flush_block(adev, adev->mapped_y + y)) < 0)
	    return code;
    adev->mapped_height = adev->mapped_start = 0;
    return 0;
}

/* Close the device, flushing the buffer. */
private int
mem_abuf_close(gx_device * dev)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int code = abuf_flush(mdev);

    if (code < 0)
	return code;
    return mem_close(dev);
}

/*
 * Framework for mapping a requested imaging operation to the buffer.
 * For now, we assume top-to-bottom transfers and use a very simple algorithm.
 */
typedef struct y_transfer_s {
    int y_next;
    int height_left;
    int transfer_y;
    int transfer_height;
} y_transfer;
private void near
y_transfer_init(y_transfer * pyt, gx_device * dev, int ty, int th)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int bh = 1 << mdev->log2_scale.y;

    if (ty < mdev->mapped_y || ty > mdev->mapped_y + mdev->mapped_height) {
	abuf_flush(mdev);
	mdev->mapped_y = ty & -bh;
	mdev->mapped_height = bh;
	memset(scan_line_base(mdev, 0), 0, bh * mdev->raster);
    }
    pyt->y_next = ty;
    pyt->height_left = th;
    pyt->transfer_height = 0;
}
/* while ( yt.height_left > 0 ) { y_transfer_next(&yt, mdev); ... } */
private void near
y_transfer_next(y_transfer * pyt, gx_device * dev)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    int my = mdev->mapped_y, mh = mdev->mapped_height;
    int ms = mdev->mapped_start;
    int ty = pyt->y_next += pyt->transfer_height;
    int th = pyt->height_left;
    int bh = 1 << mdev->log2_scale.y;

    /* From here on, we know that my <= ty <= my + mh. */
    int tby, tbh;

    if (ty == my + mh) {	/* Add a new block at my1. */
	if (mh == mdev->height) {
	    abuf_flush_block(mdev, my);
	    mdev->mapped_y = my += bh;
	    if ((mdev->mapped_start = ms += bh) == mh)
		mdev->mapped_start = ms = 0;
	} else {		/* Because we currently never extend backwards, */
	    /* we know we can't wrap around in this case. */
	    mdev->mapped_height = mh += bh;
	}
	memset(scan_line_base(mdev, (ms == 0 ? mh : ms) - bh),
	       0, bh * mdev->raster);
    }
    /* Now we know that my <= ty < my + mh. */
    tby = ty - my + ms;
    if (tby < mdev->height) {
	tbh = mdev->height - ms;
	if (tbh > mh)
	    tbh = mh;
	tbh -= tby - ms;
    } else {			/* wrap around */
	tby -= mdev->height;
	tbh = ms + mh - dev->height - tby;
    }
    if_debug7('v', "[v]my=%d, mh=%d, ms=%d, ty=%d, th=%d, tby=%d, tbh=%d\n",
	      my, mh, ms, ty, th, tby, tbh);
    if (tbh > th)
	tbh = th;
    pyt->height_left = th - tbh;
    pyt->transfer_y = tby;
    pyt->transfer_height = tbh;
}

/* Copy a monobit image. */
private int
mem_abuf_copy_mono(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    y_transfer yt;

    if (zero != gx_no_color_index || one == gx_no_color_index)
	return_error(gs_error_undefinedresult);
    x -= mdev->mapped_x;
    fit_copy_xyw(dev, base, sourcex, sraster, id, x, y, w, h);	/* don't limit h */
    if (w <= 0 || h <= 0)
	return 0;
    mdev->save_color = one;
    y_transfer_init(&yt, dev, y, h);
    while (yt.height_left > 0) {
	y_transfer_next(&yt, dev);
	(*dev_proc(&mem_mono_device, copy_mono)) (dev,
					   base + (yt.y_next - y) * sraster,
					  sourcex, sraster, gx_no_bitmap_id,
				    x, yt.transfer_y, w, yt.transfer_height,
				     gx_no_color_index, (gx_color_index) 1);
    }
    return 0;
}

/* Fill a rectangle. */
private int
mem_abuf_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    y_transfer yt;

    x -= mdev->mapped_x;
    fit_fill_xy(dev, x, y, w, h);
    fit_fill_w(dev, x, w);	/* don't limit h */
    /* or check w <= 0, h <= 0 */
    mdev->save_color = color;
    y_transfer_init(&yt, dev, y, h);
    while (yt.height_left > 0) {
	y_transfer_next(&yt, dev);
	(*dev_proc(&mem_mono_device, fill_rectangle)) (dev,
				    x, yt.transfer_y, w, yt.transfer_height,
						       (gx_color_index) 1);
    }
    return 0;
}

/* Get the clipping box.  We must scale this up by the number of alpha bits. */
private void
mem_abuf_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gx_device *tdev = mdev->target;

    (*dev_proc(tdev, get_clipping_box)) (tdev, pbox);
    pbox->p.x <<= mdev->log2_scale.x;
    pbox->p.y <<= mdev->log2_scale.y;
    pbox->q.x <<= mdev->log2_scale.x;
    pbox->q.y <<= mdev->log2_scale.y;
}
