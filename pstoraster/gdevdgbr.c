/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevdgbr.c,v 1.1 2000/03/08 23:14:22 mike Exp $ */
/* Default implementation of device get_bits[_rectangle] */
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxgetbit.h"
#include "gxlum.h"
#include "gdevmem.h"

int
gx_no_get_bits(gx_device * dev, int y, byte * data, byte ** actual_data)
{
    return_error(gs_error_unknownerror);
}
int
gx_default_get_bits(gx_device * dev, int y, byte * data, byte ** actual_data)
{	/*
	 * Hand off to get_bits_rectangle, being careful to avoid a
	 * possible recursion loop.
	 */
    dev_proc_get_bits((*save_get_bits)) = dev_proc(dev, get_bits);
    gs_int_rect rect;
    gs_get_bits_params_t params;
    int code;

    rect.p.x = 0, rect.p.y = y;
    rect.q.x = dev->width, rect.q.y = y + 1;
    params.options =
	(actual_data ? GB_RETURN_POINTER : 0) | GB_RETURN_COPY |
	(GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_RASTER_STANDARD |
    /* No depth specified, we always use native colors. */
	 GB_PACKING_CHUNKY | GB_COLORS_NATIVE | GB_ALPHA_NONE);
    params.x_offset = 0;
    params.raster = bitmap_raster(dev->width * dev->color_info.depth);
    params.data[0] = data;
    set_dev_proc(dev, get_bits, gx_no_get_bits);
    code = (*dev_proc(dev, get_bits_rectangle))
	(dev, &rect, &params, NULL);
    if (actual_data)
	*actual_data = params.data[0];
    set_dev_proc(dev, get_bits, save_get_bits);
    return code;
}

/*
 * Determine whether we can satisfy a request by simply using the stored
 * representation.
 */
private bool
requested_includes_stored(gs_get_bits_options_t requested,
			  gs_get_bits_options_t stored)
{
    gs_get_bits_options_t both = requested & stored;

    if (!(both & GB_PACKING_ALL))
	return false;
    if (both & GB_COLORS_NATIVE)
	return true;
    if (both & GB_COLORS_STANDARD_ALL) {
	if ((both & GB_ALPHA_ALL) && (both & GB_DEPTH_ALL))
	    return true;
    }
    return false;
}

/*
 * Try to implement get_bits_rectangle by returning a pointer.
 * Note that dev is used only for computing the default raster
 * and for color_info.depth.
 * This routine does not check x or h for validity.
 */
int
gx_get_bits_return_pointer(gx_device * dev, int x, int h,
		gs_get_bits_params_t * params, gs_get_bits_options_t stored,
			   byte * stored_base)
{
    gs_get_bits_options_t options = params->options;

    if (!(options & GB_RETURN_POINTER) ||
	!requested_includes_stored(options, stored)
	)
	return -1;
    /*
     * See whether we can return the bits in place.  Note that even if
     * offset_any isn't set, x_offset and x don't have to be equal: their
     * bit offsets only have to match modulo align_bitmap_mod * 8 (to
     * preserve alignment) if align_any isn't set, or mod 8 (since
     * byte alignment is always required) if align_any is set.
     */
    {
	int depth = dev->color_info.depth;
	uint dev_raster = gx_device_raster(dev, 1);
	uint raster =
	(options & (GB_RASTER_STANDARD | GB_RASTER_ANY) ? dev_raster :
	 params->raster);

	if (h <= 1 || raster == dev_raster) {
	    int x_offset =
	    (options & GB_OFFSET_ANY ? x :
	     options & GB_OFFSET_0 ? 0 : params->x_offset);

	    if (x_offset == x) {
		params->data[0] = stored_base;
		params->x_offset = x;
	    } else {
		uint align_mod =
		(options & GB_ALIGN_ANY ? 8 : align_bitmap_mod * 8);
		int bit_offset = x - x_offset;
		int bytes;

		if (bit_offset & (align_mod - 1))
		    return -1;	/* can't align */
		if (depth & (depth - 1)) {
		    /* step = lcm(depth, align_mod) */
		    int step = depth / igcd(depth, align_mod) * align_mod;

		    bytes = bit_offset / step * step;
		} else {
		    /* Use a faster algorithm if depth is a power of 2. */
		    bytes = bit_offset & (-depth & -align_mod);
		}
		params->data[0] = stored_base + arith_rshift(bytes, 3);
		params->x_offset = (bit_offset - bytes) / depth;
	    }
	    params->options =
		GB_ALIGN_STANDARD | GB_RETURN_POINTER | GB_RASTER_STANDARD |
		GB_PACKING_CHUNKY | stored |
		(params->x_offset == 0 ? GB_OFFSET_0 : GB_OFFSET_SPECIFIED);
	    return 0;
	}
    }
    return -1;
}

/*
 * Convert pixels between representations, primarily for get_bits_rectangle.
 * stored indicates how the data are actually stored, and includes:
 *      - one option from the GB_PACKING group;
 *      - if h > 1, one option from the GB_RASTER group;
 *      - optionally (and normally), GB_COLORS_NATIVE;
 *      - optionally, one option each from the GB_COLORS_STANDARD, GB_DEPTH,
 *      and GB_ALPHA groups.
 * Note that dev is used only for color mapping.  This routine assumes that
 * the stored data are aligned.
 *
 * Note: this routine does not check x, w, h for validity.
 */
int
gx_get_bits_copy(gx_device * dev, int x, int w, int h,
		 gs_get_bits_params_t * params, gs_get_bits_options_t stored,
		 const byte * src_base, uint dev_raster)
{
    gs_get_bits_options_t options = params->options;
    byte *data = params->data[0];
    int depth = dev->color_info.depth;
    int bit_x = x * depth;
    const byte *src = src_base;

    /*
     * If the stored representation matches a requested representation,
     * we can copy the data without any transformations.
     */
    bool direct_copy = requested_includes_stored(options, stored);

    /*
     * The request must include GB_PACKING_CHUNKY, GB_RETURN_COPY,
     * and an offset and raster specification.
     */
    if ((~options & (GB_PACKING_CHUNKY | GB_RETURN_COPY)) ||
	!(options & (GB_OFFSET_0 | GB_OFFSET_SPECIFIED)) ||
	!(options & (GB_RASTER_STANDARD | GB_RASTER_SPECIFIED))
	)
	return_error(gs_error_rangecheck);
    {
	int x_offset = (options & GB_OFFSET_0 ? 0 : params->x_offset);
	int end_bit = (x_offset + w) * depth;
	uint std_raster =
	(options & GB_ALIGN_STANDARD ? bitmap_raster(end_bit) :
	 (end_bit + 7) >> 3);
	uint raster =
	(options & GB_RASTER_STANDARD ? std_raster : params->raster);
	int dest_bit_x = x_offset * depth;
	int skew = bit_x - dest_bit_x;

	/*
	 * If the bit positions line up, use bytes_copy_rectangle.
	 * Since bytes_copy_rectangle doesn't require alignment,
	 * the bit positions only have to match within a byte,
	 * not within align_bitmap_mod bytes.
	 */
	if (!(skew & 7) && direct_copy) {
	    int bit_w = w * depth;

	    bytes_copy_rectangle(data + (dest_bit_x >> 3), raster,
				 src + (bit_x >> 3), dev_raster,
			      ((bit_x + bit_w + 7) >> 3) - (bit_x >> 3), h);
	} else if (direct_copy) {
	    /*
	     * Use the logic already in mem_mono_copy_mono to copy the
	     * bits to the destination.  We do this one line at a time,
	     * to avoid having to allocate a line pointer table.
	     */
	    gx_device_memory tdev;
	    byte *line_ptr = data;

	    tdev.line_ptrs = &tdev.base;
	    for (; h > 0; line_ptr += raster, src += dev_raster, --h) {
		/* Make sure the destination is aligned. */
		int align = alignment_mod(line_ptr, align_bitmap_mod);

		tdev.base = line_ptr - align;
		(*dev_proc(&mem_mono_device, copy_mono))
		    ((gx_device *) & tdev, src, bit_x, dev_raster, gx_no_bitmap_id,
		     dest_bit_x + (align << 3), 0, w, 1,
		     (gx_color_index) 0, (gx_color_index) 1);
	    }
	} else if (options & ~stored & GB_COLORS_NATIVE) {
	    /*
	     * Convert standard colors to native.  Note that the source
	     * may have depths other than 8 bits per component.
	     */
	    int dest_bit_offset = x_offset * depth;
	    byte *dest_line = data + (dest_bit_offset >> 3);
	    int ncolors =
	    (stored & GB_COLORS_RGB ? 3 : stored & GB_COLORS_CMYK ? 4 :
	     stored & GB_COLORS_GRAY ? 1 : -1);
	    int ncomp = ncolors +
	    ((stored & (GB_ALPHA_FIRST | GB_ALPHA_LAST)) != 0);
	    int src_depth = GB_OPTIONS_DEPTH(stored);
	    int src_bit_offset = x * src_depth * ncomp;
	    const byte *src_line = src_base + (src_bit_offset >> 3);
	    gx_color_value src_max = (1 << src_depth) - 1;

#define v2cv(value) ((ulong)(value) * gx_max_color_value / src_max)
	    gx_color_value alpha_default = src_max;

	    options &= ~GB_COLORS_ALL | GB_COLORS_NATIVE;
	    for (; h > 0; dest_line += raster, src_line += dev_raster, --h) {
		int i;

		sample_load_declare_setup(src, sbit, src_line,
					  src_bit_offset & 7, src_depth);
		sample_store_declare_setup(dest, dbit, dbyte, dest_line,
					   dest_bit_offset & 7, depth);

		for (i = 0; i < w; ++i) {
		    int j;
		    gx_color_value v[4], va = alpha_default;
		    gx_color_index pixel;

		    /* Fetch the source data. */
		    if (stored & GB_ALPHA_FIRST) {
			sample_load_next16(va, src, sbit, src_depth);
			va = v2cv(va);
		    }
		    for (j = 0; j < ncolors; ++j) {
			gx_color_value vj;

			sample_load_next16(vj, src, sbit, src_depth);
			v[j] = v2cv(vj);
		    }
		    if (stored & GB_ALPHA_LAST) {
			sample_load_next16(va, src, sbit, src_depth);
			va = v2cv(va);
		    }
		    /* Convert and store the pixel value. */
		    switch (ncolors) {
			case 1:
			    v[2] = v[1] = v[0];
			case 3:
			    pixel = (*dev_proc(dev, map_rgb_alpha_color))
				(dev, v[0], v[1], v[2], va);
			    break;
			case 4:
			    /****** NO ALPHA FOR CMYK ******/
			    pixel = (*dev_proc(dev, map_cmyk_color))
				(dev, v[0], v[1], v[2], v[3]);
			    break;
			default:
			    return_error(gs_error_rangecheck);
		    }
		    sample_store_next32(pixel, dest, dbit, depth, dbyte);
		}
		sample_store_flush(dest, dbit, depth, dbyte);
	    }
	} else if (!(options & GB_DEPTH_8)) {
	    /*
	     * We don't support general depths yet, or conversion between
	     * different formats.  Punt.
	     */
	    return_error(gs_error_rangecheck);
	} else {
	    /*
	     * We have to do some conversion to each pixel.  This is the
	     * slowest, most general case.
	     */
	    int src_bit_offset = x * depth;
	    const byte *src_line = src_base + (src_bit_offset >> 3);
	    int ncomp =
	    (options & (GB_ALPHA_FIRST | GB_ALPHA_LAST) ? 4 : 3);
	    byte *dest_line = data + x_offset * ncomp;

	    /* Pick the representation that's most likely to be useful. */
	    if (options & GB_COLORS_RGB)
		options &= ~GB_COLORS_STANDARD_ALL | GB_COLORS_RGB;
	    else if (options & GB_COLORS_CMYK)
		options &= ~GB_COLORS_STANDARD_ALL | GB_COLORS_CMYK;
	    else if (options & GB_COLORS_GRAY)
		options &= ~GB_COLORS_STANDARD_ALL | GB_COLORS_GRAY;
	    else
		return_error(gs_error_rangecheck);
	    for (; h > 0; dest_line += raster, src_line += dev_raster, --h) {
		int i;

		sample_load_declare_setup(src, bit, src_line, src_bit_offset & 7,
					  depth);
		byte *dest = dest_line;

		for (i = 0; i < w; ++i) {
		    gx_color_index pixel = 0;
		    gx_color_value rgba[4];

		    sample_load_next32(pixel, src, bit, depth);
		    (*dev_proc(dev, map_color_rgb_alpha)) (dev, pixel, rgba);
		    if (options & GB_ALPHA_FIRST)
			*dest++ = gx_color_value_to_byte(rgba[3]);
		    /* Convert to the requested color space. */
		    if (options & GB_COLORS_RGB) {
			dest[0] = gx_color_value_to_byte(rgba[0]);
			dest[1] = gx_color_value_to_byte(rgba[1]);
			dest[2] = gx_color_value_to_byte(rgba[2]);
			dest += 3;
		    } else if (options & GB_COLORS_CMYK) {
			/* Use the standard RGB to CMYK algorithm, */
			/* with maximum black generation and undercolor removal. */
			gx_color_value white = max(rgba[0], max(rgba[1], rgba[2]));

			dest[0] = gx_color_value_to_byte(white - rgba[0]);
			dest[1] = gx_color_value_to_byte(white - rgba[1]);
			dest[2] = gx_color_value_to_byte(white - rgba[2]);
			dest[3] = gx_color_value_to_byte(gx_max_color_value - white);
			dest += 4;
		    } else {	/* GB_COLORS_GRAY */
			/* Use the standard RGB to Gray algorithm. */
			*dest++ = gx_color_value_to_byte(
				       ((rgba[0] * (ulong) lum_red_weight) +
				      (rgba[1] * (ulong) lum_green_weight) +
					(rgba[2] * (ulong) lum_blue_weight) +
					(lum_all_weights / 2))
							 / lum_all_weights);
		    }
		    if (options & GB_ALPHA_LAST)
			*dest++ = gx_color_value_to_byte(rgba[3]);
		}
	    }
	}
	params->options =
	    (options & (GB_COLORS_ALL | GB_ALPHA_ALL)) | GB_PACKING_CHUNKY |
	    (options & GB_COLORS_NATIVE ? 0 : options & GB_DEPTH_ALL) |
	    (options & GB_ALIGN_STANDARD ? GB_ALIGN_STANDARD : GB_ALIGN_ANY) |
	    GB_RETURN_COPY |
	    (x_offset == 0 ? GB_OFFSET_0 : GB_OFFSET_SPECIFIED) |
	    (raster == std_raster ? GB_RASTER_STANDARD : GB_RASTER_SPECIFIED);
    }
    return 0;
}

int
gx_no_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    return_error(gs_error_unknownerror);
}
int
gx_default_get_bits_rectangle(gx_device * dev, const gs_int_rect * prect,
		       gs_get_bits_params_t * params, gs_int_rect ** unread)
{
    dev_proc_get_bits_rectangle((*save_get_bits_rectangle)) =
	dev_proc(dev, get_bits_rectangle);
    int depth = dev->color_info.depth;
    uint min_raster = (dev->width * depth + 7) >> 3;
    gs_get_bits_options_t options = params->options;
    int code;

    /* Avoid a recursion loop. */
    set_dev_proc(dev, get_bits_rectangle, gx_no_get_bits_rectangle);
    /*
     * If the parameters are right, try to call get_bits directly.  Note
     * that this may fail if a device only implements get_bits_rectangle
     * (not get_bits) for a limited set of options.  Note also that this
     * must handle the case of the recursive call from within
     * get_bits_rectangle (see below): because of this, and only because
     * of this, it must handle partial scan lines.
     */
    if (prect->q.y == prect->p.y + 1 &&
	!(~options &
	  (GB_RETURN_COPY | GB_PACKING_CHUNKY | GB_COLORS_NATIVE)) &&
	(options & (GB_ALIGN_STANDARD | GB_ALIGN_ANY)) &&
	((options & (GB_OFFSET_0 | GB_OFFSET_ANY)) ||
	 ((options & GB_OFFSET_SPECIFIED) && params->x_offset == 0)) &&
	((options & (GB_RASTER_STANDARD | GB_RASTER_ANY)) ||
	 ((options & GB_RASTER_SPECIFIED) &&
	  params->raster >= min_raster)) &&
	unread == NULL
	) {
	byte *data = params->data[0];
	byte *row = data;

	if (!(prect->p.x == 0 && prect->q.x == dev->width)) {
	    /* Allocate an intermediate row buffer. */
	    row = gs_alloc_bytes(dev->memory, min_raster,
				 "gx_default_get_bits_rectangle");

	    if (row == 0) {
		code = gs_note_error(gs_error_VMerror);
		goto ret;
	    }
	}
	code = (*dev_proc(dev, get_bits))
	    (dev, prect->p.y, row, &params->data[0]);
	if (code >= 0) {
	    if (row != data) {
		if (prect->p.x == 0 && params->data[0] != row) {
		    /*
		     * get_bits returned an appropriate pointer: we can
		     * avoid doing any copying.
		     */
		    DO_NOTHING;
		} else {
		    /* Copy the partial row into the supplied buffer. */
		    int width_bits = (prect->q.x - prect->p.x) * depth;
		    gx_device_memory tdev;

		    tdev.width = width_bits;
		    tdev.height = 1;
		    tdev.line_ptrs = &tdev.base;
		    tdev.base = data;
		    code = (*dev_proc(&mem_mono_device, copy_mono))
			((gx_device *) & tdev, params->data[0], prect->p.x * depth,
			 min_raster, gx_no_bitmap_id, 0, 0, width_bits, 1,
			 (gx_color_index) 0, (gx_color_index) 1);
		    params->data[0] = data;
		}
		gs_free_object(dev->memory, row,
			       "gx_default_get_bits_rectangle");
	    }
	    params->options =
		GB_ALIGN_STANDARD | GB_OFFSET_0 | GB_PACKING_CHUNKY |
		GB_ALPHA_NONE | GB_COLORS_NATIVE | GB_RASTER_STANDARD |
		(params->data[0] == data ? GB_RETURN_COPY : GB_RETURN_POINTER);
	    goto ret;
	}
    } {
	/* Do the transfer row-by-row using a buffer. */
	int x = prect->p.x, w = prect->q.x - x;
	int bits_per_pixel = depth;
	byte *row;

	if (options & GB_COLORS_STANDARD_ALL) {
	    /*
	     * Make sure the row buffer can hold the standard color
	     * representation, in case the device decides to use it.
	     */
	    int bpc = GB_OPTIONS_MAX_DEPTH(options);
	    int nc =
	    (options & GB_COLORS_CMYK ? 4 :
	     options & GB_COLORS_RGB ? 3 : 1) +
	    (options & (GB_ALPHA_ALL - GB_ALPHA_NONE) ? 1 : 0);
	    int bpp = bpc * nc;

	    if (bpp > bits_per_pixel)
		bits_per_pixel = bpp;
	}
	row = gs_alloc_bytes(dev->memory, (bits_per_pixel * w + 7) >> 3,
			     "gx_default_get_bits_rectangle");
	if (row == 0) {
	    code = gs_note_error(gs_error_VMerror);
	} else {
	    uint dev_raster = gx_device_raster(dev, true);
	    uint raster =
	    (options & GB_RASTER_SPECIFIED ? params->raster :
	     options & GB_ALIGN_STANDARD ? bitmap_raster(depth * w) :
	     (depth * w + 7) >> 3);
	    gs_int_rect rect;
	    gs_get_bits_params_t copy_params;
	    gs_get_bits_options_t copy_options =
	    GB_ALIGN_ANY | (GB_RETURN_COPY | GB_RETURN_POINTER) |
	    (GB_OFFSET_0 | GB_OFFSET_ANY) |
	    (GB_RASTER_STANDARD | GB_RASTER_ANY) | GB_PACKING_CHUNKY |
	    GB_COLORS_NATIVE | (options & (GB_DEPTH_ALL | GB_COLORS_ALL)) |
	    GB_ALPHA_ALL;
	    byte *dest = params->data[0];
	    int y;

	    rect.p.x = x, rect.q.x = x + w;
	    code = 0;
	    for (y = prect->p.y; y < prect->q.y; ++y) {
		rect.p.y = y, rect.q.y = y + 1;
		copy_params.options = copy_options;
		copy_params.data[0] = row;
		code = (*save_get_bits_rectangle)
		    (dev, &rect, &copy_params, NULL);
		if (code < 0)
		    break;
		if (copy_params.options & GB_OFFSET_0)
		    copy_params.x_offset = 0;
		params->data[0] = dest + (y - prect->p.y) * raster;
		code = gx_get_bits_copy(dev, copy_params.x_offset, w, 1,
					params, copy_params.options,
					copy_params.data[0], dev_raster);
		if (code < 0)
		    break;
	    }
	    gs_free_object(dev->memory, row, "gx_default_get_bits_rectangle");
	    params->data[0] = dest;
	}
    }
  ret:set_dev_proc(dev, get_bits_rectangle, save_get_bits_rectangle);
    return (code < 0 ? code : 0);
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

void
debug_print_gb_options(gx_bitmap_format_t options)
{
    static const char *const option_names[] =
    {
	GX_BITMAP_FORMAT_NAMES
    };
    const char *prev = "   ";
    int i;

    dlprintf1("0x%lx", (ulong) options);
    for (i = 0; i < sizeof(options) * 8; ++i)
	if ((options >> i) & 1) {
	    dprintf2("%c%s",
		     (!memcmp(prev, option_names[i], 3) ? '|' : ','),
		     option_names[i]);
	    prev = option_names[i];
	}
    dputc('\n');
}

void 
debug_print_gb_params(gs_get_bits_params_t * params)
{
    gs_get_bits_options_t options = params->options;

    debug_print_gb_options(options);
    dprintf1("data[0]=0x%lx", (ulong) params->data[0]);
    if (options & GB_OFFSET_SPECIFIED)
	dprintf1(" x_offset=%d", params->x_offset);
    if (options & GB_RASTER_SPECIFIED)
	dprintf1(" raster=%u", params->raster);
    dputc('\n');
}

#endif /* DEBUG */
