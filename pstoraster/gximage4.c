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

/*$Id: gximage4.c,v 1.2 2000/03/08 23:15:01 mike Exp $ */
/* ImageType 4 image implementation */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscspace.h"
#include "gsiparm3.h"
#include "gsiparm4.h"
#include "gxiparam.h"

/* Forward references */
private dev_proc_begin_typed_image(gx_begin_image4);
private image_enum_proc_plane_data(gx_image4_plane_data);
private image_enum_proc_end_image(gx_image4_end_image);

/* Define the image type for ImageType 4 images. */
private const gx_image_type_t image4_type = {
    gx_begin_image4, gx_data_image_source_size, 4
};
private const gx_image_enum_procs_t image4_enum_procs = {
    gx_image4_plane_data, gx_image4_end_image
};

/* Initialize an ImageType 4 image. */
void
gs_image4_t_init(gs_image4_t * pim, const gs_color_space * color_space)
{
    gs_pixel_image_t_init((gs_pixel_image_t *) pim, color_space);
    pim->type = &image4_type;
    pim->MaskColor_is_range = false;
}

/*
 * We implement ImageType 4 using ImageType 3 (or, if the image is known
 * to be completely opaque, ImageType 1).
 */
typedef struct gx_image4_enum_s {
    gx_image_enum_common;
    int num_components;
    int bpc;			/* BitsPerComponent */
    uint values[gs_image_max_components * 2];
    gs_memory_t *memory;
    gx_image_enum_common_t *info;	/* info for image3 or image1 */
    byte *mask;			/* one scan line of mask data, 0 if image1 */
    uint mask_size;
    int width;
    int y;
    int height;
} gx_image4_enum_t;

gs_private_st_ptrs2(st_image4_enum, gx_image4_enum_t, "gx_image4_enum_t",
		 image4_enum_enum_ptrs, image4_enum_reloc_ptrs, info, mask);

/* Begin an ImageType 4 image. */
private int
gx_begin_image4(gx_device * dev,
		const gs_imager_state * pis, const gs_matrix * pmat,
		const gs_image_common_t * pic, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		gs_memory_t * mem, gx_image_enum_common_t ** pinfo)
{
    const gs_image4_t *pim = (const gs_image4_t *)pic;
    int num_components = gs_color_space_num_components(pim->ColorSpace);
    bool opaque = false;
    gx_image4_enum_t *penum;
    uint mask_size = (pim->Width + 7) >> 3;
    byte *mask = 0;
    int code;

    penum = gs_alloc_struct(mem, gx_image4_enum_t, &st_image4_enum,
			    "gx_begin_image4");
    if (penum == 0)
	return_error(gs_error_VMerror);
    gx_image_enum_common_init((gx_image_enum_common_t *) penum,
			      pic, &image4_enum_procs, dev,
			      pim->BitsPerComponent,
			      num_components, pim->format);
    penum->memory = mem;
    {
	uint max_value = (1 << pim->BitsPerComponent) - 1;
	int i;

	for (i = 0; i < num_components * 2; i += 2) {
	    uint c0, c1;

	    if (pim->MaskColor_is_range)
		c0 = pim->MaskColor[i], c1 = pim->MaskColor[i + 1];
	    else
		c0 = c1 = pim->MaskColor[i >> 1];

	    if (c1 > max_value)
		c1 = max_value;
	    if (c0 > c1) {
		opaque = true;
		break;
	    }
	    penum->values[i] = c0;
	    penum->values[i + 1] = c1;
	}
    }
    if (opaque) {
	/*
	 * This image doesn't need masking at all, since at least one of
	 * the transparency keys can never be matched.  Process it as an
	 * ImageType 1 image.
	 */
	const gx_image_type_t *type;
	gs_image1_t image1;

	gs_image_t_init(&image1, pim->ColorSpace);
	type = image1.type;
	*(gs_pixel_image_t *)&image1 =
	    *(const gs_pixel_image_t *)pim;
	image1.type = type;
	penum->mask = 0;	/* indicates opaque image */
	code = gx_device_begin_typed_image(dev, pis, pmat,
					   (gs_image_common_t *)&image1,
				 prect, pdcolor, pcpath, mem, &penum->info);
    } else if ((mask = gs_alloc_bytes(mem, mask_size,
				      "gx_begin_image4(mask)")) == 0
	) {
	code = gs_note_error(gs_error_VMerror);
    } else {
	gs_image3_t image3;

	penum->num_components = num_components;
	gs_image3_t_init(&image3, pim->ColorSpace, interleave_scan_lines);
	{
	    const gx_image_type_t *type = image3.type;

	    *(gs_pixel_image_t *)&image3 =
		*(const gs_pixel_image_t *)pim;
	    image3.type = type;
	}
	*(gs_data_image_t *)&image3.MaskDict =
	    *(const gs_data_image_t *)pim;
	image3.MaskDict.BitsPerComponent = 1;
	/*
	 * The interpretation of Decode is backwards from the sensible
	 * one, but it's an Adobe convention that is now too hard to
	 * change in the code.
	 */
	image3.MaskDict.Decode[0] = 1;
	image3.MaskDict.Decode[1] = 0;
	image3.MaskDict.Interpolate = false;
	penum->bpc = pim->BitsPerComponent;
	penum->mask = mask;
	penum->mask_size = mask_size;
	if (prect)
	    penum->width = prect->q.x - prect->p.x,
		penum->y = prect->p.y, penum->height = prect->q.y - prect->p.y;
	else
	    penum->width = pim->Width,
		penum->y = 0, penum->height = pim->Height;
	code = gx_device_begin_typed_image(dev, pis, pmat,
					 (const gs_image_common_t *)&image3,
				 prect, pdcolor, pcpath, mem, &penum->info);
    }
    if (code < 0) {
	gs_free_object(mem, mask, "gx_begin_image4(mask)");
	gs_free_object(mem, penum, "gx_begin_image4");
    } else
	*pinfo = (gx_image_enum_common_t *) penum;
    return code;
}

/* Process the next piece of an ImageType 4 image. */
/* We disregard the depth in the image planes: BitsPerComponent prevails. */
private int
gx_image4_plane_data(gx_device * dev,
 gx_image_enum_common_t * info, const gx_image_plane_t * planes, int height)
{
    gx_image4_enum_t *penum = (gx_image4_enum_t *) info;
    int num_planes = penum->num_planes;
    int bpc = penum->bpc;
    int spp = (num_planes > 1 ? 1 : penum->num_components);
    byte *mask = penum->mask;
    uint mask_size = (penum->width + 7) >> 3;
    gx_image_plane_t sources[gs_image_max_components];
    int h = min(height, penum->height - penum->y);

    if (penum->mask == 0)	/* opaque image */
	return gx_image_plane_data(penum->info, planes, height);
    if (mask_size > penum->mask_size) {
	mask = gs_resize_object(penum->memory, mask, mask_size,
				"gx_image4_data(resize mask)");
	if (mask == 0)
	    return_error(gs_error_VMerror);
	penum->mask = mask;
	penum->mask_size = mask_size;
    }
    sources[0].data = mask;
    sources[0].data_x = 0;
    sources[0].raster = mask_size;
    memcpy(sources + 1, planes, num_planes * sizeof(planes[0]));
    for (; h > 0; ++(penum->y), --h) {
	int pi;
	int code;

	memset(mask, 0, (penum->width + 7) >> 3);
	for (pi = 0; pi < num_planes; ++pi) {
	    byte *mptr = mask;
	    byte mbit = 0x80;
	    uint sx_bit = sources[pi + 1].data_x * bpc;
	    const byte *sptr = sources[pi + 1].data + (sx_bit >> 3);
	    uint sx_shift = sx_bit & 7;
	    int ix;

#define advance_sx()\
  BEGIN\
    if ( (sx_shift += bpc) >= 8 ) {\
      sptr += sx_shift >> 3;\
      sx_shift &= 7;\
    }\
  END

	    for (ix = 0; ix < penum->width; ++ix) {
		int ci;

		for (ci = 0; ci < spp; ++ci) {
		    /*
		     * The following odd-looking computation is, in fact,
		     * correct both for chunky (pi = 0) and planar (ci = 0)
		     * data formats.
		     */
		    int vi = (ci + pi) * 2;
		    uint sample;

		    if (bpc <= 8) {
			sample = (*sptr >> (8 - sx_shift - bpc)) &
			    ((1 << bpc) - 1);
		    } else {
			/* bpc == 12 */
			if (sx_shift /* == 4 */ )
			    sample = ((*sptr & 0xf) << 8) + sptr[1];
			else
			    sample = (*sptr << 8) + (sptr[1] >> 4);
		    }
		    if (sample < penum->values[vi] ||
			sample > penum->values[vi + 1]
			)
			*mptr |= mbit;
		    advance_sx();
		}
		if ((mbit >>= 1) == 0)
		    mbit = 0x80, ++mptr;
	    }
	}
	code = gx_image_plane_data(penum->info, sources, 1);
	if (code < 0)
	    return code;
	for (pi = 1; pi <= num_planes; ++pi)
	    sources[pi].data += sources[pi].raster;
    }
#undef advance_sx
    return penum->y >= penum->height;
}

/* Clean up after processing an ImageType 4 image. */
private int
gx_image4_end_image(gx_device * dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    gx_image4_enum_t *penum = (gx_image4_enum_t *) info;
    gs_memory_t *mem = penum->memory;

    /* Finish processing the ImageType 3 (or 1) image. */
    int code = gx_image_end(penum->info, draw_last);

    gs_free_object(mem, penum->mask, "gx_image4_end_image(mask)");
    gs_free_object(mem, penum, "gx_image4_end_image");
    return code;
}
