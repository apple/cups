/* Copyright (C) 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevmpla.c */
/* Any-depth planar "memory" (stored bitmap) devices */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* semi-public definitions */
#include "gdevmem.h"			/* private definitions */

/*
 * Planar memory devices store the bits by planes instead of by chunks.
 * The plane corresponding to the least significant bit of the color index
 * is stored first.
 *
 * The current implementations are quite inefficient.
 * We may improve them someday if anyone cares.
 */

/* Procedures */
declare_mem_map_procs(mem_planar_map_rgb_color, mem_planar_map_color_rgb);
declare_mem_procs(mem_planar_copy_mono, mem_planar_copy_color, mem_planar_fill_rectangle);

/* The device descriptor. */
/* The instance is public. */
/* The default instance has depth = 1, but clients may set this */
/* to other values before opening the device. */
private dev_proc_open_device(mem_planar_open);
private dev_proc_get_bits(mem_planar_get_bits);
const gx_device_memory far_data mem_planar_device =
  mem_full_device("image(planar)", 0, 1, mem_planar_open,
    mem_planar_map_rgb_color, mem_planar_map_color_rgb,
    mem_planar_copy_mono, mem_planar_copy_color, mem_planar_fill_rectangle,
    mem_planar_get_bits, gx_default_map_cmyk_color,
    gx_default_strip_tile_rectangle, gx_no_strip_copy_rop);

/* Open a planar memory device. */
private int
mem_planar_open(gx_device *dev)
{	/* Temporarily reset the parameters, and call */
	/* the generic open procedure. */
	int depth = dev->color_info.depth;
	int height = dev->height;
	int code;

	dev->height *= depth;
	dev->color_info.depth = 1;
	code = mem_open(dev);
	dev->height = height;
	dev->color_info.depth = depth;
	return code;
}

/* Map a r-g-b color to a color index. */
private gx_color_index
mem_planar_map_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	int depth = dev->color_info.depth;
	return (*dev_proc(gdev_mem_device_for_bits(depth), map_rgb_color))
	  (dev, r, g, b);
}

/* Map a color index to a r-g-b color. */
private int
mem_planar_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	int depth = dev->color_info.depth;
	return (*dev_proc(gdev_mem_device_for_bits(depth), map_color_rgb))
	  (dev, color, prgb);
}

/* Fill a rectangle with a color. */
private int
mem_planar_fill_rectangle(gx_device *dev,
  int x, int y, int w, int h, gx_color_index color)
{	byte **ptrs = mdev->line_ptrs;
	int i;
	for ( i = 0; i < dev->color_info.depth;
	      i++, mdev->line_ptrs += dev->height
	    )
	  (*dev_proc(&mem_mono_device, fill_rectangle))(dev,
			x, y, w, h, (color >> i) & 1);
	mdev->line_ptrs = ptrs;
	return 0;
}

/* Copy a bitmap. */
private int
mem_planar_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	byte **ptrs = mdev->line_ptrs;
	int i;
	for ( i = 0; i < dev->color_info.depth;
	      i++, mdev->line_ptrs += dev->height
	    )
	  (*dev_proc(&mem_mono_device, copy_mono))(dev,
			base, sourcex, sraster, id, x, y, w, h,
			(zero == gx_no_color_index ? gx_no_color_index :
			 (zero >> i) & 1),
			(one == gx_no_color_index ? gx_no_color_index :
			 (one >> i) & 1));
	mdev->line_ptrs = ptrs;
	return 0;
}

/* Copy a color bitmap. */
/* This is very slow and messy. */
private int
mem_planar_copy_color(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	byte **ptrs = mdev->line_ptrs;
	int depth = dev->color_info.depth; 
	int wleft = w;
	int hleft = h;
	const byte *srow = base;
	int ynext = y;
#define max_w 32
	union _b {
		long l[max_w / sizeof(long)];
		byte b[max_w / 8];
	} buf;

	while ( wleft > max_w )
	{	mem_planar_copy_color(dev, base,
			sourcex + wleft - max_w, sraster, gx_no_bitmap_id,
			x + wleft - max_w, y, max_w, h);
		wleft -= max_w;
	}
	for ( ; hleft > 0;
	      srow += sraster, ynext++, hleft--,
		mdev->line_ptrs += dev->height
	    )
	{	int i;
		for ( i = 0; i < depth;
		      i++, mdev->line_ptrs += dev->height
		    )
		{	int sx, bx;
			memset(buf.b, 0, sizeof(buf.b));
			for ( sx = 0, bx = sourcex * depth + depth - 1 - i;
			      sx < w; sx++, bx += depth
			    )
				if ( srow[bx >> 3] & (0x80 >> (bx & 7)) )
					buf.b[sx >> 3] |= 0x80 >> (sx & 7);
			(*dev_proc(&mem_mono_device, copy_mono))(dev,
				buf.b, 0, sizeof(buf), gx_no_bitmap_id,
				x, ynext, w, 1,
				(gx_color_index)0, (gx_color_index)1);
		}
		mdev->line_ptrs = ptrs;
	}
	return 0;
}

/* Copy bits back from a planar memory device. */
/****** NOT IMPLEMENTED YET ******/
private int
mem_planar_get_bits(gx_device *dev, int y, byte *str, byte **actual_data)
{	return -1;
}
