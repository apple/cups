/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsdevmem.c,v 1.2 2000/03/08 23:14:39 mike Exp $ */
/* Memory device creation for Ghostscript library */
#include "math_.h"		/* for fabs */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxdevice.h"
#include "gxdevmem.h"

/* Make a memory (image) device. */
/* If colors_size = -16, -24, or -32, this is a true-color device; */
/* otherwise, colors_size is the size of the palette in bytes */
/* (2^N for gray scale, 3*2^N for RGB color). */
/* We separate device allocation and initialization at customer request. */
int
gs_initialize_wordimagedevice(gx_device_memory * new_dev, const gs_matrix * pmat,
	      uint width, uint height, const byte * colors, int colors_size,
		    bool word_oriented, bool page_device, gs_memory_t * mem)
{
    const gx_device_memory *proto_dev;
    int palette_count = colors_size;
    int num_components = 1;
    int pcount;
    int bits_per_pixel;
    float x_pixels_per_unit, y_pixels_per_unit;
    byte palette[256 * 3];
    byte *dev_palette;
    bool has_color;

    switch (colors_size) {
	case 3 * 2:
	    palette_count = 2;
	    num_components = 3;
	case 2:
	    bits_per_pixel = 1;
	    break;
	case 3 * 4:
	    palette_count = 4;
	    num_components = 3;
	case 4:
	    bits_per_pixel = 2;
	    break;
	case 3 * 16:
	    palette_count = 16;
	    num_components = 3;
	case 16:
	    bits_per_pixel = 4;
	    break;
	case 3 * 256:
	    palette_count = 256;
	    num_components = 3;
	case 256:
	    bits_per_pixel = 8;
	    break;
	case -16:
	    bits_per_pixel = 16;
	    palette_count = 0;
	    break;
	case -24:
	    bits_per_pixel = 24;
	    palette_count = 0;
	    break;
	case -32:
	    bits_per_pixel = 32;
	    palette_count = 0;
	    break;
	default:
	    return_error(gs_error_rangecheck);
    }
    proto_dev = (word_oriented ?
		 gdev_mem_word_device_for_bits(bits_per_pixel) :
		 gdev_mem_device_for_bits(bits_per_pixel));
    if (proto_dev == 0)		/* no suitable device */
	return_error(gs_error_rangecheck);
    pcount = palette_count * 3;
    /* Check to make sure the palette contains white and black, */
    /* and, if it has any colors, the six primaries. */
    if (bits_per_pixel <= 8) {
	const byte *p;
	byte *q;
	int primary_mask = 0;
	int i;

	has_color = false;
	for (i = 0, p = colors, q = palette;
	     i < palette_count; i++, q += 3
	    ) {
	    int mask = 1;

	    switch (num_components) {
		case 1:	/* gray */
		    q[0] = q[1] = q[2] = *p++;
		    break;
		default /* case 3 */ :		/* RGB */
		    q[0] = p[0], q[1] = p[1], q[2] = p[2];
		    p += 3;
	    }
#define shift_mask(b,n)\
  switch ( b ) { case 0xff: mask <<= n; case 0: break; default: mask = 0; }
	    shift_mask(q[0], 4);
	    shift_mask(q[1], 2);
	    shift_mask(q[2], 1);
#undef shift_mask
	    primary_mask |= mask;
	    if (q[0] != q[1] || q[0] != q[2])
		has_color = true;
	}
	switch (primary_mask) {
	    case 129:		/* just black and white */
		if (has_color)	/* color but no primaries */
		    return_error(gs_error_rangecheck);
	    case 255:		/* full color */
		break;
	    default:
		return_error(gs_error_rangecheck);
	}
    } else
	has_color = true;
    /*
     * The initial transformation matrix must map 1 user unit to
     * 1/72".  Let W and H be the width and height in pixels, and
     * assume the initial matrix is of the form [A 0 0 B X Y].
     * Then the size of the image in user units is (W/|A|,H/|B|),
     * hence the size in inches is ((W/|A|)/72,(H/|B|)/72), so
     * the number of pixels per inch is
     * (W/((W/|A|)/72),H/((H/|B|)/72)), or (|A|*72,|B|*72).
     * Similarly, if the initial matrix is [0 A B 0 X Y] for a 90
     * or 270 degree rotation, the size of the image in user
     * units is (W/|B|,H/|A|), so the pixels per inch are
     * (|B|*72,|A|*72).  We forbid non-orthogonal transformation
     * matrices.
     */
    if (is_fzero2(pmat->xy, pmat->yx))
	x_pixels_per_unit = pmat->xx, y_pixels_per_unit = pmat->yy;
    else if (is_fzero2(pmat->xx, pmat->yy))
	x_pixels_per_unit = pmat->yx, y_pixels_per_unit = pmat->xy;
    else
	return_error(gs_error_undefinedresult);
    /* All checks done, allocate the device. */
    if (bits_per_pixel != 1) {
	dev_palette = gs_alloc_string(mem, pcount,
				      "gs_makeimagedevice(palette)");
	if (dev_palette == 0)
	    return_error(gs_error_VMerror);
    }
    gs_make_mem_device(new_dev, proto_dev, mem,
		       (page_device ? 1 : -1), 0);
    if (!has_color) {
	new_dev->color_info.num_components = 1;
	new_dev->color_info.max_color = 0;
	new_dev->color_info.dither_colors = 0;
    }
    if (bits_per_pixel == 1) {	/* Determine the polarity from the palette. */
	/* This is somewhat bogus, but does the right thing */
	/* in the only cases we care about. */
	gdev_mem_mono_set_inverted(new_dev,
			       (palette[0] | palette[1] | palette[2]) != 0);
    } else {
	new_dev->palette.size = pcount;
	new_dev->palette.data = dev_palette;
	memcpy(dev_palette, palette, pcount);
    }
    new_dev->initial_matrix = *pmat;
    new_dev->MarginsHWResolution[0] = new_dev->HWResolution[0] =
	fabs(x_pixels_per_unit) * 72;
    new_dev->MarginsHWResolution[1] = new_dev->HWResolution[1] =
	fabs(y_pixels_per_unit) * 72;
    gx_device_set_width_height((gx_device *) new_dev, width, height);
    /* Set the ImagingBBox so we get a correct clipping region. */
    {
	gs_rect bbox;

	bbox.p.x = 0;
	bbox.p.y = 0;
	bbox.q.x = width;
	bbox.q.y = height;
	gs_bbox_transform_inverse(&bbox, pmat, &bbox);
	new_dev->ImagingBBox[0] = bbox.p.x;
	new_dev->ImagingBBox[1] = bbox.p.y;
	new_dev->ImagingBBox[2] = bbox.q.x;
	new_dev->ImagingBBox[3] = bbox.q.y;
	new_dev->ImagingBBox_set = true;
    }
    /* The bitmap will be allocated when the device is opened. */
    new_dev->is_open = false;
    new_dev->bitmap_memory = mem;
    return 0;
}

int
gs_makewordimagedevice(gx_device ** pnew_dev, const gs_matrix * pmat,
	       uint width, uint height, const byte * colors, int num_colors,
		    bool word_oriented, bool page_device, gs_memory_t * mem)
{
    int code;
    gx_device_memory *pnew =
    gs_alloc_struct(mem, gx_device_memory, &st_device_memory,
		    "gs_makeimagedevice(device)");

    if (pnew == 0)
	return_error(gs_error_VMerror);
    code = gs_initialize_wordimagedevice(pnew, pmat, width, height,
					 colors, num_colors, word_oriented,
					 page_device, mem);
    if (code < 0) {
	gs_free_object(mem, pnew, "gs_makeimagedevice(device)");
	return code;
    }
    *pnew_dev = (gx_device *) pnew;
    return 0;
}
