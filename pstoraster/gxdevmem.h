/* Copyright (C) 1989, 1995, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxdevmem.h,v 1.2 2000/03/08 23:14:57 mike Exp $ */
/* Requires gxdevice.h */

#ifndef gxdevmem_INCLUDED
#  define gxdevmem_INCLUDED

/*
 * A 'memory' device is essentially a stored bitmap.
 * There are several different kinds: 1-bit black and white,
 * 2-, 4-, and 8-bit mapped color, 16- and 24-bit RGB color,
 * and 32-bit CMYK color.  (16-bit uses 5/6/5 bits per color.)
 * All use the same structure, since it's so awkward to get the effect of
 * subclasses in C.
 *
 * Memory devices come in two flavors: standard, which always stores bytes
 * big-endian, and word-oriented, which stores bytes in the machine order
 * within 32-bit "words".  The source data for copy_mono and
 * copy_color must be in big-endian order, and since memory devices
 * also are guaranteed to allocate the bitmap consecutively,
 * the bitmap of a standard memory device can serve directly as input
 * to copy_mono or copy_color operations.  This is not true of word-oriented
 * memory devices, which are provided only in response to a request by
 * a customer with their own image processing library that uses this format.
 */
#ifndef gx_device_memory_DEFINED
#  define gx_device_memory_DEFINED
typedef struct gx_device_memory_s gx_device_memory;

#endif

struct gx_device_memory_s {
    gx_device_forward_common;	/* (see gxdevice.h) */
    gs_matrix initial_matrix;	/* the initial transformation */
    uint raster;		/* bytes per scan line, */
    /* filled in by 'open' */
    bool foreign_bits;		/* if true, bits are not in */
    /* GC-able space */
    byte *base;
    byte **line_ptrs;		/* scan line pointers */
#define scan_line_base(dev,y) ((dev)->line_ptrs[y])
    /* If the bitmap_memory pointer is non-zero, it is used for */
    /* allocating the bitmap when the device is opened, */
    /* and freeing it when the device is closed. */
    gs_memory_t *bitmap_memory;
    /* Following is used for mapped color, */
    /* including 1-bit devices (to specify polarity). */
    gs_const_string palette;	/* RGB triples */
    /* Following is only used for 24-bit color. */
    struct _c24 {
	gx_color_index rgb;	/* cache key */
	bits32 rgbr, gbrg, brgb;	/* cache value */
    } color24;
    /* Following are only used for alpha buffers. */
    /* The client initializes those marked with $; */
    /* they don't change after initialization. */
    gs_log2_scale_point log2_scale;	/* $ oversampling scale factors */
    int log2_alpha_bits;	/* $ log2 of # of alpha bits being produced */
    int mapped_x;		/* $ X value mapped to buffer X=0 */
    int mapped_y;		/* lowest Y value mapped to buffer */
    int mapped_height;		/* # of Y values mapped to buffer */
    int mapped_start;		/* local Y value corresponding to mapped_y */
    gx_color_index save_color;	/* last (only) color displayed */
};

extern_st(st_device_memory);
#define public_st_device_memory() /* in gdevmem.c */\
  gs_public_st_composite(st_device_memory, gx_device_memory,\
    "gx_device_memory", device_memory_enum_ptrs, device_memory_reloc_ptrs)
#define st_device_memory_max_ptrs (st_device_forward_max_ptrs + 2)
#define mem_device_init_private\
	{ identity_matrix_body },	/* initial matrix (filled in) */\
	0,			/* raster (filled in) */\
	true,			/* foreign_bits (default) */\
	(byte *)0,		/* base (filled in) */\
	(byte **)0,		/* line_ptrs (filled in by mem_open) */\
	0,			/* bitmap_memory */\
	{ (byte *)0, 0 },	/* palette (filled in for color) */\
	{ gx_no_color_index },	/* color24 */\
	{ 0, 0 }, 0,		/* scale, log2_alpha_bits */\
	0, 0, 0, 0,		/* mapped_* */\
	gx_no_color_index	/* save_color */

/*
 * Memory devices may have special setup requirements.
 * In particular, it may not be obvious how much space to allocate
 * for the bitmap.  Here is the routine that computes this
 * from the width and height.
 */
ulong gdev_mem_data_size(P3(const gx_device_memory *, int, int));

#define gdev_mem_bitmap_size(mdev)\
  gdev_mem_data_size(mdev, (mdev)->width, (mdev)->height)
/*
 * Do the inverse computation: given the device width and a buffer size,
 * compute the maximum height.
 */
int gdev_mem_max_height(P3(const gx_device_memory *, int, ulong));

/*
 * Compute the raster (data bytes per line) similarly.
 */
#define gdev_mem_raster(mdev)\
  gx_device_raster((const gx_device *)(mdev), true)

/* Determine the appropriate memory device for a given */
/* number of bits per pixel (0 if none suitable). */
const gx_device_memory *gdev_mem_device_for_bits(P1(int));

/* Determine the word-oriented memory device for a given depth. */
const gx_device_memory *gdev_mem_word_device_for_bits(P1(int));

/* Make a memory device. */
/* mem is 0 if the device is temporary and local, */
/* or the allocator that was used to allocate it if it is a real object. */
/* page_device is 1 if the device should be a page device, */
/* 0 if it should propagate this property from its target, or */
/* -1 if it should not be a page device. */
void gs_make_mem_mono_device(P3(gx_device_memory * mdev, gs_memory_t * mem,
				gx_device * target));
void gs_make_mem_device(P5(gx_device_memory * mdev,
			   const gx_device_memory * mdproto,
			   gs_memory_t * mem, int page_device,
			   gx_device * target));
void gs_make_mem_abuf_device(P6(gx_device_memory * adev, gs_memory_t * mem,
				gx_device * target,
				const gs_log2_scale_point * pscale,
				int alpha_bits, int mapped_x));
void gs_make_mem_alpha_device(P4(gx_device_memory * adev, gs_memory_t * mem,
				 gx_device * target, int alpha_bits));

/*
 * Open a memory device, only setting line pointers to a subset of its
 * scan lines.  Banding devices use this (see gxclread.c).
 */
int gdev_mem_open_scan_lines(P2(gx_device_memory *mdev, int setup_height));

/* Define whether a monobit memory device is inverted (black=1). */
void gdev_mem_mono_set_inverted(P2(gx_device_memory * mdev, bool black_is_1));

/* Test whether a device is a memory device. */
bool gs_device_is_memory(P1(const gx_device *));

/* Test whether a device is an alpha-buffering device. */
bool gs_device_is_abuf(P1(const gx_device *));

#endif /* gxdevmem_INCLUDED */
