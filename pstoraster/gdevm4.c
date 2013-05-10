/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevm4.c */
/* 4-bit-per-pixel "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* semi-public definitions */
#include "gdevmem.h"			/* private definitions */

/**************** NOTE: copy_rop only works for gray scale ****************/
extern dev_proc_strip_copy_rop(mem_gray_strip_copy_rop);

/* ================ Standard (byte-oriented) device ================ */

#undef chunk
#define chunk byte
#define fpat(byt) mono_fill_make_pattern(byt)

/* Procedures */
declare_mem_procs(mem_mapped4_copy_mono, mem_mapped4_copy_color, mem_mapped4_fill_rectangle);

/* The device descriptor. */
const gx_device_memory far_data mem_mapped4_device =
  mem_device("image4", 4, 0,
    mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
    mem_mapped4_copy_mono, mem_mapped4_copy_color, mem_mapped4_fill_rectangle,
    mem_gray_strip_copy_rop);

/* Convert x coordinate to byte offset in scan line. */
#undef x_to_byte
#define x_to_byte(x) ((x) >> 1)

/* Define the 4-bit fill patterns. */
static const mono_fill_chunk tile_patterns[16] =
   {	fpat(0x00), fpat(0x11), fpat(0x22), fpat(0x33),
	fpat(0x44), fpat(0x55), fpat(0x66), fpat(0x77),
	fpat(0x88), fpat(0x99), fpat(0xaa), fpat(0xbb),
	fpat(0xcc), fpat(0xdd), fpat(0xee), fpat(0xff)
   };


/* Fill a rectangle with a color. */
private int
mem_mapped4_fill_rectangle(gx_device *dev,
  int x, int y, int w, int h, gx_color_index color)
{	fit_fill(dev, x, y, w, h);
	bits_fill_rectangle(scan_line_base(mdev, y), x << 2, mdev->raster,
			    tile_patterns[color], w << 2, h);
	return 0;
}

/* Copy a bitmap. */
private int
mem_mapped4_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	const byte *line;
	int first_bit;
	byte first_mask, b0, b1;
	declare_scan_ptr(dest);
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	setup_rect(dest);
	line = base + (sourcex >> 3);
	first_bit = 0x80 >> (sourcex & 7);
	first_mask = (x & 1 ? 0xf : 0xf0);
	b0 = ((byte)zero << 4) + (byte)zero;
	b1 = ((byte)one << 4) + (byte)one;
	while ( h-- > 0 )
	   {	register byte *pptr = (byte *)dest;
		const byte *sptr = line;
		register int sbyte = *sptr++;
		register int bit = first_bit;
		register byte mask = first_mask;
		int count = w;
		do
		   {	if ( sbyte & bit )
			   {	if ( one != gx_no_color_index )
				  *pptr = (*pptr & ~mask) + (b1 & mask);
			   }
			else
			   {	if ( zero != gx_no_color_index )
				  *pptr = (*pptr & ~mask) + (b0 & mask);
			   }
			if ( (bit >>= 1) == 0 )
				bit = 0x80, sbyte = *sptr++;
			if ( (mask = ~mask) == 0xf0 )
				pptr++;
		   }
		while ( --count > 0 );
		line += sraster;
		inc_ptr(dest, draster);
	   }
	return 0;
}

/* Copy a color bitmap. */
private int
mem_mapped4_copy_color(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	int code;
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	/* Use monobit copy_mono. */
	/* Patch the width in the device temporarily. */
	dev->width <<= 2;
	code = (*dev_proc(&mem_mono_device, copy_mono))
	  (dev, base, sourcex << 2, sraster, id,
	   x << 2, y, w << 2, h, (gx_color_index)0, (gx_color_index)1);
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
const gx_device_memory far_data mem_mapped4_word_device =
  mem_full_device("image4w", 4, 0, mem_open,
    mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
    mem4_word_copy_mono, mem4_word_copy_color, mem4_word_fill_rectangle,
    mem_word_get_bits, gx_default_map_cmyk_color,
    gx_default_strip_tile_rectangle, gx_no_strip_copy_rop);

/* Fill a rectangle with a color. */
private int
mem4_word_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	byte *base;
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
mem4_word_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	byte *row;
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
mem4_word_copy_color(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	int code;
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	/* Use monobit copy_mono. */
	/* Patch the width in the device temporarily. */
	dev->width <<= 2;
	code = (*dev_proc(&mem_mono_word_device, copy_mono))
	  (dev, base, sourcex << 2, sraster, id,
	   x << 2, y, w << 2, h, (gx_color_index)0, (gx_color_index)1);
	/* Restore the correct width. */
	dev->width >>= 2;
	return code;
}

#endif				/* !arch_is_big_endian */
