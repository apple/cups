/* Copyright (C) 1994, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevm2.c,v 1.2 2000/03/08 23:14:23 mike Exp $ */
/* 2-bit-per-pixel "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* semi-public definitions */
#include "gdevmem.h"		/* private definitions */

extern dev_proc_strip_copy_rop(mem_gray_strip_copy_rop);

/* ================ Standard (byte-oriented) device ================ */

#undef chunk
#define chunk byte
#define fpat(byt) mono_fill_make_pattern(byt)

/* Procedures */
declare_mem_procs(mem_mapped2_copy_mono, mem_mapped2_copy_color, mem_mapped2_fill_rectangle);

/* The device descriptor. */
const gx_device_memory mem_mapped2_device =
mem_device("image2", 2, 0,
	   mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
  mem_mapped2_copy_mono, mem_mapped2_copy_color, mem_mapped2_fill_rectangle,
	   mem_gray_strip_copy_rop);

/* Convert x coordinate to byte offset in scan line. */
#undef x_to_byte
#define x_to_byte(x) ((x) >> 2)

/* Define the 2-bit fill patterns. */
static const mono_fill_chunk tile_patterns[4] =
{fpat(0x00), fpat(0x55), fpat(0xaa), fpat(0xff)
};

/* Fill a rectangle with a color. */
private int
mem_mapped2_fill_rectangle(gx_device * dev,
			   int x, int y, int w, int h, gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    fit_fill(dev, x, y, w, h);
    bits_fill_rectangle(scan_line_base(mdev, y), x << 1, mdev->raster,
			tile_patterns[color], w << 1, h);
    return 0;
}

/* Copy a bitmap. */
private int
mem_mapped2_copy_mono(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    const byte *line;
    int first_bit;
    byte first_mask, b0, b1, bxor, left_mask, right_mask;
    static const byte btab[4] =
    {0, 0x55, 0xaa, 0xff};
    static const byte bmask[4] =
    {0xc0, 0x30, 0xc, 3};
    static const byte lmask[4] =
    {0, 0xc0, 0xf0, 0xfc};

    declare_scan_ptr(dest);

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    setup_rect(dest);
    line = base + (sourcex >> 3);
    first_bit = 0x80 >> (sourcex & 7);
    first_mask = bmask[x & 3];
    left_mask = lmask[x & 3];
    right_mask = ~lmask[(x + w) & 3];
    if ((x & 3) + w <= 4)
	left_mask = right_mask = left_mask | right_mask;
    b0 = btab[zero & 3];
    b1 = btab[one & 3];
    bxor = b0 ^ b1;
    while (h-- > 0) {
	register byte *pptr = (byte *) dest;
	const byte *sptr = line;
	register int sbyte = *sptr++;
	register int bit = first_bit;
	register byte mask = first_mask;
	int count = w;

	/* We have 4 cases, of which only 2 really matter. */
	if (one != gx_no_color_index) {
	    if (zero != gx_no_color_index) {	/* Copying an opaque bitmap. */
		byte data =
		(*pptr & left_mask) | (b0 & ~left_mask);

		do {
		    if (sbyte & bit)
			data ^= bxor & mask;
		    if ((bit >>= 1) == 0)
			bit = 0x80, sbyte = *sptr++;
		    if ((mask >>= 2) == 0)
			mask = 0xc0, *pptr++ = data, data = b0;
		}
		while (--count > 0);
		if (mask != 0xc0)
		    *pptr =
			(*pptr & right_mask) | (data & ~right_mask);
	    } else {		/* Filling a mask. */
		do {
		    if (sbyte & bit)
			*pptr = (*pptr & ~mask) + (b1 & mask);
		    if ((bit >>= 1) == 0)
			bit = 0x80, sbyte = *sptr++;
		    if ((mask >>= 2) == 0)
			mask = 0xc0, pptr++;
		}
		while (--count > 0);
	    }
	} else {		/* Some other case. */
	    do {
		if (!(sbyte & bit)) {
		    if (zero != gx_no_color_index)
			*pptr = (*pptr & ~mask) + (b0 & mask);
		}
		if ((bit >>= 1) == 0)
		    bit = 0x80, sbyte = *sptr++;
		if ((mask >>= 2) == 0)
		    mask = 0xc0, pptr++;
	    }
	    while (--count > 0);
	}
	line += sraster;
	inc_ptr(dest, draster);
    }
    return 0;
}

/* Copy a color bitmap. */
private int
mem_mapped2_copy_color(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
		       int x, int y, int w, int h)
{
    int code;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    /* Use monobit copy_mono. */
    /* Patch the width in the device temporarily. */
    dev->width <<= 1;
    code = (*dev_proc(&mem_mono_device, copy_mono))
	(dev, base, sourcex << 1, sraster, id,
	 x << 1, y, w << 1, h, (gx_color_index) 0, (gx_color_index) 1);
    /* Restore the correct width. */
    dev->width >>= 1;
    return code;
}

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !arch_is_big_endian

/* Procedures */
declare_mem_procs(mem2_word_copy_mono, mem2_word_copy_color, mem2_word_fill_rectangle);

/* Here is the device descriptor. */
const gx_device_memory mem_mapped2_word_device =
mem_full_device("image2w", 2, 0, mem_open,
		mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
	mem2_word_copy_mono, mem2_word_copy_color, mem2_word_fill_rectangle,
		gx_default_map_cmyk_color, gx_default_strip_tile_rectangle,
		gx_no_strip_copy_rop, mem_word_get_bits_rectangle);

/* Fill a rectangle with a color. */
private int
mem2_word_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			 gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *base;
    uint raster;

    fit_fill(dev, x, y, w, h);
    base = scan_line_base(mdev, y);
    raster = mdev->raster;
    mem_swap_byte_rect(base, raster, x << 1, w << 1, h, true);
    bits_fill_rectangle(base, x << 1, raster,
			tile_patterns[color], w << 1, h);
    mem_swap_byte_rect(base, raster, x << 1, w << 1, h, true);
    return 0;
}

/* Copy a bitmap. */
private int
mem2_word_copy_mono(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *row;
    uint raster;
    bool store;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    row = scan_line_base(mdev, y);
    raster = mdev->raster;
    store = (zero != gx_no_color_index && one != gx_no_color_index);
    mem_swap_byte_rect(row, raster, x << 1, w << 1, h, store);
    mem_mapped2_copy_mono(dev, base, sourcex, sraster, id,
			  x, y, w, h, zero, one);
    mem_swap_byte_rect(row, raster, x << 1, w << 1, h, false);
    return 0;
}

/* Copy a color bitmap. */
private int
mem2_word_copy_color(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
		     int x, int y, int w, int h)
{
    int code;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    /* Use monobit copy_mono. */
    /* Patch the width in the device temporarily. */
    dev->width <<= 1;
    code = (*dev_proc(&mem_mono_word_device, copy_mono))
	(dev, base, sourcex << 1, sraster, id,
	 x << 1, y, w << 1, h, (gx_color_index) 0, (gx_color_index) 1);
    /* Restore the correct width. */
    dev->width >>= 1;
    return code;
}

#endif /* !arch_is_big_endian */
