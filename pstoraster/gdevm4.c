/* Copyright (C) 1992, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevm4.c,v 1.2 2000/03/08 23:14:23 mike Exp $ */
/* 4-bit-per-pixel "memory" (stored bitmap) device */
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
declare_mem_procs(mem_mapped4_copy_mono, mem_mapped4_copy_color, mem_mapped4_fill_rectangle);

/* The device descriptor. */
const gx_device_memory mem_mapped4_device =
mem_device("image4", 4, 0,
	   mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
  mem_mapped4_copy_mono, mem_mapped4_copy_color, mem_mapped4_fill_rectangle,
	   mem_gray_strip_copy_rop);

/* Convert x coordinate to byte offset in scan line. */
#undef x_to_byte
#define x_to_byte(x) ((x) >> 1)

/* Define the 4-bit fill patterns. */
static const mono_fill_chunk tile_patterns[16] =
{fpat(0x00), fpat(0x11), fpat(0x22), fpat(0x33),
 fpat(0x44), fpat(0x55), fpat(0x66), fpat(0x77),
 fpat(0x88), fpat(0x99), fpat(0xaa), fpat(0xbb),
 fpat(0xcc), fpat(0xdd), fpat(0xee), fpat(0xff)
};


/* Fill a rectangle with a color. */
private int
mem_mapped4_fill_rectangle(gx_device * dev,
			   int x, int y, int w, int h, gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    fit_fill(dev, x, y, w, h);
    bits_fill_rectangle(scan_line_base(mdev, y), x << 2, mdev->raster,
			tile_patterns[color], w << 2, h);
    return 0;
}

/* Copy a bitmap. */
private int
mem_mapped4_copy_mono(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    const byte *line;
    declare_scan_ptr(dest);
    byte invert, bb;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    setup_rect(dest);
    line = base + (sourcex >> 3);
    /* Divide into opaque and masked cases. */
    if (one == gx_no_color_index) {
	if (zero == gx_no_color_index)
	    return 0;		/* nothing to do */
	invert = 0xff;
	bb = ((byte) zero << 4) | (byte) zero;
    } else if (zero == gx_no_color_index) {
	invert = 0;
	bb = ((byte) one << 4) | (byte) one;
    } else {
	/* Opaque case. */
	int shift = ~(sourcex ^ x) & 1;
	byte oz[4];

	oz[0] = (byte)((zero << 4) | zero);
	oz[1] = (byte)((zero << 4) | one);
	oz[2] = (byte)((one << 4) | zero);
	oz[3] = (byte)((one << 4) | one);
	do {
	    register byte *dptr = (byte *) dest;
	    const byte *sptr = line;
	    register uint sbyte = *sptr++;
	    register int sbit = ~sourcex & 7;
	    int count = w;

	    /*
	     * If the first source bit corresponds to an odd X in the
	     * destination, process it now.
	     */
	    if (x & 1) {
		*dptr = (*dptr & 0xf0) |
		    ((sbyte >> sbit) & 1 ? one : zero);
		--count;	/* may now be 0 */
		if (--sbit < 0)
		    sbit = 7, sbyte = *sptr++;
		++dptr;
	    }
	    /*
	     * Now we know the next destination X is even.  We want to
	     * process 2 source bits at a time from now on, so set things up
	     * properly depending on whether the next source X (bit) is even
	     * or odd.  In both even and odd cases, the active source bits
	     * are in bits 8..1 of sbyte.
	     */
	    sbyte <<= shift;
	    sbit += shift - 1;
	    /*
	     * Now bit # sbit+1 is the most significant unprocessed bit
	     * in sbyte.  -1 <= sbit <= 7; sbit is odd.
	     * Note that if sbit = -1, all of sbyte has been processed.
	     *
	     * Continue processing pairs of bits in the first source byte.
	     */
	    while (count >= 2 && sbit >= 0) {
		*dptr++ = oz[(sbyte >> sbit) & 3];
		sbit -= 2, count -= 2;
	    }
	    /*
	     * Now sbit = -1 iff we have processed the entire first source
	     * byte.
	     *
	     * Process full source bytes.
	     */
	    if (shift) {
		sbyte >>= 1;	/* in case count < 8 */
		for (; count >= 8; dptr += 4, count -= 8) {
		    sbyte = *sptr++;
		    dptr[0] = oz[sbyte >> 6];
		    dptr[1] = oz[(sbyte >> 4) & 3];
		    dptr[2] = oz[(sbyte >> 2) & 3];
		    dptr[3] = oz[sbyte & 3];
		}
		sbyte <<= 1;
	    } else {
		for (; count >= 8; dptr += 4, count -= 8) {
		    sbyte = (sbyte << 8) | *sptr++;
		    dptr[0] = oz[(sbyte >> 7) & 3];
		    dptr[1] = oz[(sbyte >> 5) & 3];
		    dptr[2] = oz[(sbyte >> 3) & 3];
		    dptr[3] = oz[(sbyte >> 1) & 3];
		}
	    }
	    if (!count)
		continue;
	    /*
	     * Process pairs of bits in the final source byte.  Note that
	     * if sbit > 0, this is still the first source byte (the
	     * full-byte loop wasn't executed).
	     */
	    if (sbit < 0) {
		sbyte = (sbyte << 8) | (*sptr << shift);
		sbit = 7;
	    }
	    while (count >= 2) {
		*dptr++ = oz[(sbyte >> sbit) & 3];
		sbit -= 2, count -= 2;
	    }
	    /*
	     * If the final source bit corresponds to an even X value,
	     * process it now.
	     */
	    if (count) {
		*dptr = (*dptr & 0x0f) |
		    (((sbyte >> sbit) & 2 ? one : zero) << 4);
	    }
	} while ((line += sraster, inc_ptr(dest, draster), --h) > 0);
	return 0;
    }
    /* Masked case. */
    do {
	register byte *dptr = (byte *) dest;
	const byte *sptr = line;
	register int sbyte = *sptr++ ^ invert;
	register int sbit = 0x80 >> (sourcex & 7);
	register byte mask = (x & 1 ? 0x0f : 0xf0);
	int count = w;

	do {
	    if (sbyte & sbit)
		*dptr = (*dptr & ~mask) | (bb & mask);
	    if ((sbit >>= 1) == 0)
		sbit = 0x80, sbyte = *sptr++ ^ invert;
	    dptr += (mask = ~mask) >> 7;
	} while (--count > 0);
	line += sraster;
	inc_ptr(dest, draster);
    } while (--h > 0);
    return 0;
}

/* Copy a color bitmap. */
private int
mem_mapped4_copy_color(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
		       int x, int y, int w, int h)
{
    /* Use monobit copy_mono. */
    int code;

    /* Patch the width in the device temporarily. */
    dev->width <<= 2;
    code = (*dev_proc(&mem_mono_device, copy_mono))
	(dev, base, sourcex << 2, sraster, id,
	 x << 2, y, w << 2, h, (gx_color_index) 0, (gx_color_index) 1);
    /* Restore the correct width. */
    dev->width >>= 2;
    return code;
}

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !arch_is_big_endian

/* Procedures */
declare_mem_procs(mem4_word_copy_mono, mem4_word_copy_color, mem4_word_fill_rectangle);

/* Here is the device descriptor. */
const gx_device_memory mem_mapped4_word_device =
mem_full_device("image4w", 4, 0, mem_open,
		mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
	mem4_word_copy_mono, mem4_word_copy_color, mem4_word_fill_rectangle,
		gx_default_map_cmyk_color, gx_default_strip_tile_rectangle,
		gx_no_strip_copy_rop, mem_word_get_bits_rectangle);

/* Fill a rectangle with a color. */
private int
mem4_word_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			 gx_color_index color)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *base;
    uint raster;

    fit_fill(dev, x, y, w, h);
    base = scan_line_base(mdev, y);
    raster = mdev->raster;
    mem_swap_byte_rect(base, raster, x << 2, w << 2, h, true);
    bits_fill_rectangle(base, x << 2, raster,
			tile_patterns[color], w << 2, h);
    mem_swap_byte_rect(base, raster, x << 2, w << 2, h, true);
    return 0;
}

/* Copy a bitmap. */
private int
mem4_word_copy_mono(gx_device * dev,
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
    mem_swap_byte_rect(row, raster, x << 2, w << 2, h, store);
    mem_mapped4_copy_mono(dev, base, sourcex, sraster, id,
			  x, y, w, h, zero, one);
    mem_swap_byte_rect(row, raster, x << 2, w << 2, h, false);
    return 0;
}

/* Copy a color bitmap. */
private int
mem4_word_copy_color(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
		     int x, int y, int w, int h)
{
    int code;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    /* Use monobit copy_mono. */
    /* Patch the width in the device temporarily. */
    dev->width <<= 2;
    code = (*dev_proc(&mem_mono_word_device, copy_mono))
	(dev, base, sourcex << 2, sraster, id,
	 x << 2, y, w << 2, h, (gx_color_index) 0, (gx_color_index) 1);
    /* Restore the correct width. */
    dev->width >>= 2;
    return code;
}

#endif /* !arch_is_big_endian */
