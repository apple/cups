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

/* gdevm8.c */
/* 8-bit-per-pixel "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"			/* semi-public definitions */
#include "gdevmem.h"			/* private definitions */

/**************** NOTE: copy_rop only works for gray scale ****************/
extern dev_proc_strip_copy_rop(mem_gray8_rgb24_strip_copy_rop);  /* in gdevmrop.c */
#define mem_gray8_strip_copy_rop mem_gray8_rgb24_strip_copy_rop

/* ================ Standard (byte-oriented) device ================ */

#undef chunk
#define chunk byte

/* Procedures */
declare_mem_procs(mem_mapped8_copy_mono, mem_mapped8_copy_color, mem_mapped8_fill_rectangle);

/* The device descriptor. */
const gx_device_memory far_data mem_mapped8_device =
  mem_device("image8", 8, 0,
    mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
    mem_mapped8_copy_mono, mem_mapped8_copy_color, mem_mapped8_fill_rectangle,
    mem_gray8_strip_copy_rop);

/* Convert x coordinate to byte offset in scan line. */
#undef x_to_byte
#define x_to_byte(x) (x)

/* Fill a rectangle with a color. */
private int
mem_mapped8_fill_rectangle(gx_device *dev,
  int x, int y, int w, int h, gx_color_index color)
{	fit_fill(dev, x, y, w, h);
	bytes_fill_rectangle(scan_line_base(mdev, y) + x, mdev->raster,
			     (byte)color, w, h);
	return 0;
}

/* Copy a monochrome bitmap. */
/* We split up this procedure because of limitations in the bcc32 compiler. */
private void mapped8_copy01(P9(chunk *, const byte *, int, int, uint,
  int, int, byte, byte));
private void mapped8_copyN1(P8(chunk *, const byte *, int, int, uint,
  int, int, byte));
private void mapped8_copy0N(P8(chunk *, const byte *, int, int, uint,
  int, int, byte));
private int
mem_mapped8_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	const byte *line;
	int first_bit;
	declare_scan_ptr(dest);
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	setup_rect(dest);
	line = base + (sourcex >> 3);
	first_bit = 0x80 >> (sourcex & 7);
#define is_color(c) ((int)(c) != (int)gx_no_color_index)
	if ( is_color(one) )
	{	if ( is_color(zero) )
		  mapped8_copy01(dest, line, first_bit, sraster, draster,
				 w, h, (byte)zero, (byte)one);
		else
		  mapped8_copyN1(dest, line, first_bit, sraster, draster,
				 w, h, (byte)one);
	}
	else if ( is_color(zero) )
	  mapped8_copy0N(dest, line, first_bit, sraster, draster,
			 w, h, (byte)zero);
#undef is_color
	return 0;
}
/* Macros for copy loops */
#define COPY_BEGIN\
	while ( h-- > 0 )\
	{	register byte *pptr = dest;\
		const byte *sptr = line;\
		register int sbyte = *sptr;\
		register uint bit = first_bit;\
		int count = w;\
		do\
		{
#define COPY_END\
			if ( (bit >>= 1) == 0 )\
				bit = 0x80, sbyte = *++sptr;\
			pptr++;\
		}\
		while ( --count > 0 );\
		line += sraster;\
		inc_ptr(dest, draster);\
	}
/* Halftone coloring */
private void
mapped8_copy01(chunk *dest, const byte *line, int first_bit,
  int sraster, uint draster, int w, int h, byte b0, byte b1)
{	COPY_BEGIN
	*pptr = (sbyte & bit ? b1 : b0);
	COPY_END
}
/* Stenciling */
private void
mapped8_copyN1(chunk *dest, const byte *line, int first_bit,
  int sraster, uint draster, int w, int h, byte b1)
{	COPY_BEGIN
	if ( sbyte & bit )
	  *pptr = b1;
	COPY_END
}
/* Reverse stenciling (probably never used) */
private void
mapped8_copy0N(chunk *dest, const byte *line, int first_bit,
  int sraster, uint draster, int w, int h, byte b0)
{	COPY_BEGIN
	if ( !(sbyte & bit) )
	  *pptr = b0;
	COPY_END
}
#undef COPY_BEGIN
#undef COPY_END

/* Copy a color bitmap. */
private int
mem_mapped8_copy_color(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	mem_copy_byte_rect(mdev, base, sourcex, sraster, x, y, w, h);
	return 0;
}

/* ================ "Word"-oriented device ================ */

/* Note that on a big-endian machine, this is the same as the */
/* standard byte-oriented-device. */

#if !arch_is_big_endian

/* Procedures */
declare_mem_procs(mem8_word_copy_mono, mem8_word_copy_color, mem8_word_fill_rectangle);

/* Here is the device descriptor. */
const gx_device_memory far_data mem_mapped8_word_device =
  mem_full_device("image8w", 8, 0, mem_open,
    mem_mapped_map_rgb_color, mem_mapped_map_color_rgb,
    mem8_word_copy_mono, mem8_word_copy_color, mem8_word_fill_rectangle,
    mem_word_get_bits, gx_default_map_cmyk_color,
    gx_default_strip_tile_rectangle, gx_no_strip_copy_rop);

/* Fill a rectangle with a color. */
private int
mem8_word_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	byte *base;
	uint raster;
	fit_fill(dev, x, y, w, h);
	base = scan_line_base(mdev, y);
	raster = mdev->raster;
	mem_swap_byte_rect(base, raster, x << 3, w << 3, h, true);
	bytes_fill_rectangle(base + x, raster, (byte)color, w, h);
	mem_swap_byte_rect(base, raster, x << 3, w << 3, h, true);
	return 0;
}

/* Copy a bitmap. */
private int
mem8_word_copy_mono(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{	byte *row;
	uint raster;
	bool store;
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	row = scan_line_base(mdev, y);
	raster = mdev->raster;
	store = (zero != gx_no_color_index && one != gx_no_color_index);
	mem_swap_byte_rect(row, raster, x << 3, w << 3, h, store);
	mem_mapped8_copy_mono(dev, base, sourcex, sraster, id,
			      x, y, w, h, zero, one);
	mem_swap_byte_rect(row, raster, x << 3, w << 3, h, false);
	return 0;
}

/* Copy a color bitmap. */
private int
mem8_word_copy_color(gx_device *dev,
  const byte *base, int sourcex, int sraster, gx_bitmap_id id,
  int x, int y, int w, int h)
{	byte *row;
	uint raster;
	fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
	row = scan_line_base(mdev, y);
	raster = mdev->raster;
	mem_swap_byte_rect(row, raster, x << 3, w << 3, h, true);
	mem_copy_byte_rect(mdev, base, sourcex, sraster, x, y, w, h);
	mem_swap_byte_rect(row, raster, x << 3, w << 3, h, false);
	return 0;
}

#endif				/* !arch_is_big_endian */
