/* Copyright (C) 1989, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevmem.c,v 1.2 2000/03/08 23:14:24 mike Exp $ */
/* Generic "memory" (stored bitmap) device */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsrect.h"
#include "gsstruct.h"
#include "gxarith.h"
#include "gxdevice.h"
#include "gxgetbit.h"
#include "gxdevmem.h"		/* semi-public definitions */
#include "gdevmem.h"		/* private definitions */

/* Structure descriptor */
public_st_device_memory();

/* GC procedures */
#define mptr ((gx_device_memory *)vptr)
private 
ENUM_PTRS_BEGIN(device_memory_enum_ptrs)
{
    return ENUM_USING(st_device_forward, vptr, sizeof(gx_device_forward), index - 2);
}
case 0:
ENUM_RETURN((mptr->foreign_bits ? NULL : (void *)mptr->base));
ENUM_STRING_PTR(1, gx_device_memory, palette);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(device_memory_reloc_ptrs)
{
    if (!mptr->foreign_bits) {
	byte *base_old = mptr->base;
	long reloc;
	int y;

	RELOC_PTR(gx_device_memory, base);
	reloc = base_old - mptr->base;
	for (y = 0; y < mptr->height; y++)
	    mptr->line_ptrs[y] -= reloc;
	/* Relocate line_ptrs, which also points into the data area. */
	mptr->line_ptrs = (byte **) ((byte *) mptr->line_ptrs - reloc);
    }
    RELOC_CONST_STRING_PTR(gx_device_memory, palette);
    RELOC_USING(st_device_forward, vptr, sizeof(gx_device_forward));
}
RELOC_PTRS_END
#undef mptr

/* Define the palettes for monobit devices. */
private const byte b_w_palette_string[6] =
{0xff, 0xff, 0xff, 0, 0, 0};
const gs_const_string mem_mono_b_w_palette =
{b_w_palette_string, 6};
private const byte w_b_palette_string[6] =
{0, 0, 0, 0xff, 0xff, 0xff};
const gs_const_string mem_mono_w_b_palette =
{w_b_palette_string, 6};

/* ------ Generic code ------ */

/* Return the appropriate memory device for a given */
/* number of bits per pixel (0 if none suitable). */
const gx_device_memory *
gdev_mem_device_for_bits(int bits_per_pixel)
{
    switch (bits_per_pixel) {
	case 1:
	    return &mem_mono_device;
	case 2:
	    return &mem_mapped2_device;
	case 4:
	    return &mem_mapped4_device;
	case 8:
	    return &mem_mapped8_device;
	case 16:
	    return &mem_true16_device;
	case 24:
	    return &mem_true24_device;
	case 32:
	    return &mem_true32_device;
	default:
	    return 0;
    }
}
/* Do the same for a word-oriented device. */
const gx_device_memory *
gdev_mem_word_device_for_bits(int bits_per_pixel)
{
    switch (bits_per_pixel) {
	case 1:
	    return &mem_mono_word_device;
	case 2:
	    return &mem_mapped2_word_device;
	case 4:
	    return &mem_mapped4_word_device;
	case 8:
	    return &mem_mapped8_word_device;
	case 24:
	    return &mem_true24_word_device;
	case 32:
	    return &mem_true32_word_device;
	default:
	    return 0;
    }
}

/* Make a memory device. */
/* Note that the default for monobit devices is white = 0, black = 1. */
void
gs_make_mem_device(gx_device_memory * dev, const gx_device_memory * mdproto,
		   gs_memory_t * mem, int page_device, gx_device * target)
{
    gx_device_init((gx_device *) dev, (const gx_device *)mdproto,
		   mem, true);
    dev->stype = &st_device_memory;
    switch (page_device) {
	case -1:
	    set_dev_proc(dev, get_page_device, gx_default_get_page_device);
	    break;
	case 1:
	    set_dev_proc(dev, get_page_device, gx_page_device_get_page_device);
	    break;
    }
    dev->target = target;
    if (target != 0) {
	/* Forward the color mapping operations to the target. */
	gx_device_forward_color_procs((gx_device_forward *) dev);
    }
    if (dev->color_info.depth == 1)
	gdev_mem_mono_set_inverted(dev,
				   (target == 0 ||
				    (*dev_proc(target, map_rgb_color))
			    (target, (gx_color_value) 0, (gx_color_value) 0,
			     (gx_color_value) 0) != 0));
}
/* Make a monobit memory device.  This is never a page device. */
/* Note that white=0, black=1. */
void
gs_make_mem_mono_device(gx_device_memory * dev, gs_memory_t * mem,
			gx_device * target)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    *dev = mem_mono_device;
    dev->memory = mem;
    set_dev_proc(dev, get_page_device, gx_default_get_page_device);
    mdev->target = target;
    gdev_mem_mono_set_inverted(dev, true);
    rc_init(dev, mem, 0);
}


/* Define whether a monobit memory device is inverted (black=1). */
void
gdev_mem_mono_set_inverted(gx_device_memory * dev, bool black_is_1)
{
    if (black_is_1)
	dev->palette = mem_mono_b_w_palette;
    else
	dev->palette = mem_mono_w_b_palette;
}

/* Compute the size of the bitmap storage, */
/* including the space for the scan line pointer table. */
/* Note that scan lines are padded to a multiple of align_bitmap_mod bytes, */
/* and additional padding may be needed if the pointer table */
/* must be aligned to an even larger modulus. */
private ulong
mem_bitmap_bits_size(const gx_device_memory * dev, int width, int height)
{
    return round_up((ulong) height *
		    bitmap_raster(width * dev->color_info.depth),
		    max(align_bitmap_mod, arch_align_ptr_mod));
}
ulong
gdev_mem_data_size(const gx_device_memory * dev, int width, int height)
{
    return mem_bitmap_bits_size(dev, width, height) +
	(ulong) height *sizeof(byte *);

}
/*
 * Do the inverse computation: given a width (in pixels) and a buffer size,
 * compute the maximum height.
 */
int
gdev_mem_max_height(const gx_device_memory * dev, int width, ulong size)
{
    ulong max_height = size /
    (bitmap_raster(width * dev->color_info.depth) + sizeof(byte *));
    int height = (int)min(max_height, max_int);

    /*
     * Because of alignment rounding, the just-computed height might
     * be too large by a small amount.  Adjust it the easy way.
     */
    while (gdev_mem_data_size(dev, width, height) > size)
	--height;
    return height;
}

/* Open a memory device, allocating the data area if appropriate, */
/* and create the scan line table. */
private void mem_set_line_ptrs(P4(gx_device_memory *, byte **, byte *, int));
int
mem_open(gx_device * dev)
{
    return gdev_mem_open_scan_lines((gx_device_memory *)dev, dev->height);
}
int
gdev_mem_open_scan_lines(gx_device_memory *mdev, int setup_height)
{
    if (setup_height < 0 || setup_height > mdev->height)
	return_error(gs_error_rangecheck);
    if (mdev->bitmap_memory != 0) {	/* Allocate the data now. */
	ulong size = gdev_mem_bitmap_size(mdev);

	if ((uint) size != size)
	    return_error(gs_error_limitcheck);
	mdev->base = gs_alloc_bytes(mdev->bitmap_memory, (uint)size,
				    "mem_open");
	if (mdev->base == 0)
	    return_error(gs_error_VMerror);
	mdev->foreign_bits = false;
    }
/*
 * Macro for adding an offset to a pointer when setting up the
 * scan line table.  This isn't just pointer arithmetic, because of
 * the segmenting considerations discussed in gdevmem.h.
 */
#define huge_ptr_add(base, offset)\
   ((void *)((byte huge *)(base) + (offset)))
    mem_set_line_ptrs(mdev,
		      huge_ptr_add(mdev->base,
				   mem_bitmap_bits_size(mdev, mdev->width,
							mdev->height)),
		      mdev->base, setup_height);
    return 0;
}
/* Set up the scan line pointers of a memory device. */
/* Sets line_ptrs, base, raster; uses width, color_info.depth. */
private void
mem_set_line_ptrs(gx_device_memory * mdev, byte ** line_ptrs, byte * base,
		  int count /* >= 0 */)
{
    byte **pptr = mdev->line_ptrs = line_ptrs;
    byte **pend = pptr + count;
    byte *scan_line = mdev->base = base;
    uint raster = mdev->raster = gdev_mem_raster(mdev);

    while (pptr < pend) {
	*pptr++ = scan_line;
	scan_line = huge_ptr_add(scan_line, raster);
    }
}

/* Return the initial transformation matrix */
void
mem_get_initial_matrix(gx_device * dev, gs_matrix * pmat)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    pmat->xx = mdev->initial_matrix.xx;
    pmat->xy = mdev->initial_matrix.xy;
    pmat->yx = mdev->initial_matrix.yx;
    pmat->yy = mdev->initial_matrix.yy;
    pmat->tx = mdev->initial_matrix.tx;
    pmat->ty = mdev->initial_matrix.ty;
}

/* Test whether a device is a memory device */
bool
gs_device_is_memory(const gx_device * dev)
{				/* We can't just compare the procs, or even an individual proc, */
    /* because we might be tracing.  Instead, check the identity of */
    /* the device name. */
    const gx_device_memory *bdev =
    gdev_mem_device_for_bits(dev->color_info.depth);

    if (bdev != 0 && bdev->dname == dev->dname)
	return true;
    bdev = gdev_mem_word_device_for_bits(dev->color_info.depth);
    return (bdev != 0 && bdev->dname == dev->dname);
}

/* Close a memory device, freeing the data area if appropriate. */
int
mem_close(gx_device * dev)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;

    if (mdev->bitmap_memory != 0)
	gs_free_object(mdev->bitmap_memory, mdev->base, "mem_close");
    return 0;
}

/* Copy bits to a client. */
#undef chunk
#define chunk byte
int
mem_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    gs_get_bits_options_t options = params->options;
    int x = prect->p.x, w = prect->q.x - x, y = prect->p.y, h = prect->q.y - y;

    if (options == 0) {
	params->options =
	    (GB_ALIGN_STANDARD | GB_ALIGN_ANY) |
	    (GB_RETURN_COPY | GB_RETURN_POINTER) |
	    (GB_OFFSET_0 | GB_OFFSET_SPECIFIED | GB_OFFSET_ANY) |
	    (GB_RASTER_STANDARD | GB_RASTER_SPECIFIED | GB_RASTER_ANY) |
	    GB_PACKING_CHUNKY | GB_COLORS_NATIVE | GB_ALPHA_NONE;
	return_error(gs_error_rangecheck);
    }
    if ((w <= 0) | (h <= 0)) {
	if ((w | h) < 0)
	    return_error(gs_error_rangecheck);
	return 0;
    }
    if (x < 0 || w > dev->width - x ||
	y < 0 || h > dev->height - y
	)
	return_error(gs_error_rangecheck);
    {
	byte *base = scan_line_base(mdev, y);
	int code = gx_get_bits_return_pointer(dev, x, h, params,
				      GB_COLORS_NATIVE | GB_PACKING_CHUNKY |
					      GB_ALPHA_NONE, base);

	if (code >= 0)
	    return code;
	return gx_get_bits_copy(dev, x, w, h, params,
				GB_COLORS_NATIVE | GB_PACKING_CHUNKY |
				GB_ALPHA_NONE, base,
				gx_device_raster(dev, true));
    }
}

#if !arch_is_big_endian

/*
 * Swap byte order in a rectangular subset of a bitmap.  If store = true,
 * assume the rectangle will be overwritten, so don't swap any bytes where
 * it doesn't matter.  The caller has already done a fit_fill or fit_copy.
 * Note that the coordinates are specified in bits, not in terms of the
 * actual device depth.
 */
void
mem_swap_byte_rect(byte * base, uint raster, int x, int w, int h, bool store)
{
    int xbit = x & 31;

    if (store) {
	if (xbit + w > 64) {	/* Operation spans multiple words. */
	    /* Just swap the words at the left and right edges. */
	    if (xbit != 0)
		mem_swap_byte_rect(base, raster, x, 1, h, false);
	    x += w - 1;
	    xbit = x & 31;
	    if (xbit == 31)
		return;
	    w = 1;
	}
    }
    /* Swap the entire rectangle (or what's left of it). */
    {
	byte *row = base + ((x >> 5) << 2);
	int nw = (xbit + w + 31) >> 5;
	int ny;

	for (ny = h; ny > 0; row += raster, --ny) {
	    int nx = nw;
	    bits32 *pw = (bits32 *) row;

	    do {
		bits32 w = *pw;

		*pw++ = (w >> 24) + ((w >> 8) & 0xff00) +
		    ((w & 0xff00) << 8) + (w << 24);
	    }
	    while (--nx);
	}
    }
}

/* Copy a word-oriented rectangle to the client, swapping bytes as needed. */
int
mem_word_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte *src;
    uint dev_raster = gx_device_raster(dev, 1);
    int x = prect->p.x;
    int w = prect->q.x - x;
    int y = prect->p.y;
    int h = prect->q.y - y;
    int bit_x, bit_w;
    int code;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0) {
	/*
	 * It's easiest to just keep going with an empty rectangle.
	 * We pass the original rectangle to mem_get_bits_rectangle,
	 * so unread will be filled in correctly.
	 */
	x = y = w = h = 0;
    }
    bit_x = x * dev->color_info.depth;
    bit_w = w * dev->color_info.depth;
    src = scan_line_base(mdev, y);
    mem_swap_byte_rect(src, dev_raster, bit_x, bit_w, h, false);
    code = mem_get_bits_rectangle(dev, prect, params, unread);
    mem_swap_byte_rect(src, dev_raster, bit_x, bit_w, h, false);
    return code;
}

#endif /* !arch_is_big_endian */

/* Map a r-g-b color to a color index for a mapped color memory device */
/* (2, 4, or 8 bits per pixel.) */
/* This requires searching the palette. */
gx_color_index
mem_mapped_map_rgb_color(gx_device * dev, gx_color_value r, gx_color_value g,
			 gx_color_value b)
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    byte br = gx_color_value_to_byte(r);
    byte bg = gx_color_value_to_byte(g);
    byte bb = gx_color_value_to_byte(b);
    register const byte *pptr = mdev->palette.data;
    int cnt = mdev->palette.size;
    const byte *which = 0;	/* initialized only to pacify gcc */
    int best = 256 * 3;

    while ((cnt -= 3) >= 0) {
	register int diff = *pptr - br;

	if (diff < 0)
	    diff = -diff;
	if (diff < best) {	/* quick rejection */
	    int dg = pptr[1] - bg;

	    if (dg < 0)
		dg = -dg;
	    if ((diff += dg) < best) {	/* quick rejection */
		int db = pptr[2] - bb;

		if (db < 0)
		    db = -db;
		if ((diff += db) < best)
		    which = pptr, best = diff;
	    }
	}
	pptr += 3;
    }
    return (gx_color_index) ((which - mdev->palette.data) / 3);
}

/* Map a color index to a r-g-b color for a mapped color memory device. */
int
mem_mapped_map_color_rgb(gx_device * dev, gx_color_index color,
			 gx_color_value prgb[3])
{
    gx_device_memory * const mdev = (gx_device_memory *)dev;
    const byte *pptr = mdev->palette.data + (int)color * 3;

    prgb[0] = gx_color_value_from_byte(pptr[0]);
    prgb[1] = gx_color_value_from_byte(pptr[1]);
    prgb[2] = gx_color_value_from_byte(pptr[2]);
    return 0;
}
