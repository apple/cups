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

/*$Id: gximage3.c,v 1.2 2000/03/08 23:15:01 mike Exp $ */
/* ImageType 3 image implementation */
#include "math_.h"		/* for ceil, floor */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsbitops.h"
#include "gscspace.h"
#include "gsiparm3.h"
#include "gsstruct.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclipm.h"
#include "gxiparam.h"
#include "gxistate.h"

/* Forward references */
private dev_proc_begin_typed_image(gx_begin_image3);
private image_enum_proc_plane_data(gx_image3_plane_data);
private image_enum_proc_end_image(gx_image3_end_image);

/* GC descriptor */
extern_st(st_gs_pixel_image);
public_st_gs_image3();

/* Define the image type for ImageType 3 images. */
private const gx_image_type_t image3_type = {
    gx_begin_image3, gx_data_image_source_size, 3
};
private const gx_image_enum_procs_t image3_enum_procs = {
    gx_image3_plane_data, gx_image3_end_image
};

/* Initialize an ImageType 3 image. */
void
gs_image3_t_init(gs_image3_t * pim, const gs_color_space * color_space,
		 gs_image3_interleave_type_t interleave_type)
{
    gs_pixel_image_t_init((gs_pixel_image_t *) pim, color_space);
    pim->type = &image3_type;
    pim->InterleaveType = interleave_type;
    gs_data_image_t_init(&pim->MaskDict, -1);
}

/*
 * We implement ImageType 3 images by interposing a mask clipper in
 * front of an ordinary ImageType 1 image.  Note that we build up the
 * mask row-by-row as we are processing the image.
 */
typedef struct gx_image3_enum_s {
    gx_image_enum_common;
    gx_device_memory *mdev;
    gx_device_mask_clip *pcdev;
    gx_image_enum_common_t *pixel_info;
    gx_image_enum_common_t *mask_info;
    gs_image3_interleave_type_t InterleaveType;
    int num_components;		/* (not counting mask) */
    int bpc;			/* BitsPerComponent */
    gs_memory_t *memory;
    int mask_width;
    int pixel_width;
    byte *pixel_data;		/* (if chunky) */
    byte *mask_data;		/* (if chunky) */
    int y;			/* counts up to max(p'height, m'height) */
    int pixel_height;
    int mask_height;
} gx_image3_enum_t;

gs_private_st_ptrs6(st_image3_enum, gx_image3_enum_t, "gx_image3_enum_t",
		    image3_enum_enum_ptrs, image3_enum_reloc_ptrs,
		 mdev, pcdev, pixel_info, mask_info, pixel_data, mask_data);

/* Begin an ImageType 3 image. */
private int
gx_begin_image3(gx_device * dev,
		const gs_imager_state * pis, const gs_matrix * pmat,
		const gs_image_common_t * pic, const gs_int_rect * prect,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath,
		gs_memory_t * mem, gx_image_enum_common_t ** pinfo)
{
    const gs_image3_t *pim = (const gs_image3_t *)pic;
    gx_image3_enum_t *penum;
    gx_device_memory *mdev;
    gx_device_mask_clip *pcdev;
    gs_image_t i_pixel, i_mask;
    gs_matrix mat;
    gs_rect mrect;
    gs_int_point origin;
    int code;

    /* Validate the parameters. */
    if (pim->Height <= 0 || pim->MaskDict.Height <= 0)
	return_error(gs_error_rangecheck);
    switch (pim->InterleaveType) {
	default:
	    return_error(gs_error_rangecheck);
	case interleave_chunky:
	    if (pim->MaskDict.Width != pim->Width ||
		pim->MaskDict.Height != pim->Height ||
		pim->MaskDict.BitsPerComponent != pim->BitsPerComponent ||
		pim->format != gs_image_format_chunky
		)
		return_error(gs_error_rangecheck);
	    break;
	case interleave_scan_lines:
	    if (pim->MaskDict.Height % pim->Height != 0 &&
		pim->Height % pim->MaskDict.Height != 0
		)
		return_error(gs_error_rangecheck);
	    /* falls through */
	case interleave_separate_source:
	    if (pim->MaskDict.BitsPerComponent != 1)
		return_error(gs_error_rangecheck);
    }
    /****** CHECK FOR COMPATIBLE ImageMatrix ******/
    penum = gs_alloc_struct(mem, gx_image3_enum_t, &st_image3_enum,
			    "gx_begin_image3");
    if (penum == 0)
	return_error(gs_error_VMerror);
    penum->num_components =
	gs_color_space_num_components(pim->ColorSpace);
    gx_image_enum_common_init((gx_image_enum_common_t *) penum,
			      pic, &image3_enum_procs, dev,
			      pim->BitsPerComponent,
			      1 + penum->num_components,
			      pim->format);
    if (prect)
	penum->pixel_width = prect->q.x - prect->p.x,
	    penum->y = prect->p.y,
	    penum->pixel_height = prect->q.y - prect->p.y;
    else
	penum->pixel_width = pim->Width,
	    penum->y = 0,
	    penum->pixel_height = pim->Height;
    penum->mask_width = pim->MaskDict.Width;
    penum->mask_height = pim->MaskDict.Height;
    penum->mdev = mdev =
	gs_alloc_struct(mem, gx_device_memory, &st_device_memory,
			"gx_begin_image3(mdev)");
    if (mdev == 0)
	goto out1;
    penum->pcdev = pcdev =
	gs_alloc_struct(mem, gx_device_mask_clip, &st_device_mask_clip,
			"gx_begin_image3(pcdev)");
    if (pcdev == 0)
	goto out2;
    penum->mask_info = 0;
    penum->pixel_info = 0;
    penum->mask_data = 0;
    penum->pixel_data = 0;
    if (pim->InterleaveType == interleave_chunky) {
	/* Allocate row buffers for the mask and pixel data. */
	penum->pixel_data =
	    gs_alloc_bytes(mem,
			   (penum->pixel_width * pim->BitsPerComponent *
			    penum->num_components + 7) >> 3,
			   "gx_begin_image3(pixel_data)");
	penum->mask_data =
	    gs_alloc_bytes(mem, (penum->mask_width + 7) >> 3,
			   "gx_begin_image3(mask_data)");
	if (penum->pixel_data == 0 || penum->mask_data == 0)
	    goto out3;
    }
    penum->InterleaveType = pim->InterleaveType;
    penum->bpc = pim->BitsPerComponent;
    penum->memory = mem;
    gs_make_mem_mono_device(mdev, mem, NULL);
    mdev->bitmap_memory = mem;
    mrect.p.x = mrect.p.y = 0;
    mrect.q.x = pim->MaskDict.Width;
    mrect.q.y = pim->MaskDict.Height;
    if (pmat == 0)
	pmat = &ctm_only(pis);
    if ((code = gs_matrix_invert(&pim->MaskDict.ImageMatrix, &mat)) < 0 ||
	(code = gs_matrix_multiply(&mat, pmat, &mat)) < 0 ||
	(code = gs_bbox_transform(&mrect, &mat, &mrect)) < 0
	)
	return code;
    origin.x = floor(mrect.p.x);
    mdev->width = (int)ceil(mrect.q.x) - origin.x;
    origin.y = floor(mrect.p.y);
    mdev->height = (int)ceil(mrect.q.y) - origin.y;
    gx_device_fill_in_procs((gx_device *) mdev);
    code = (*dev_proc(mdev, open_device)) ((gx_device *) mdev);
    if (code < 0)
	goto out3;
    mdev->is_open = true;
    {
	gx_strip_bitmap bits;	/* only gx_bitmap */

	bits.data = mdev->base;
	bits.raster = mdev->raster;
	bits.size.x = mdev->width;
	bits.size.y = mdev->height;
	bits.id = gx_no_bitmap_id;
	code = gx_mask_clip_initialize(pcdev, &gs_mask_clip_device,
				       (const gx_bitmap *)&bits, dev,
				       origin.x, origin.y);
	if (code < 0)
	    goto out4;
	pcdev->tiles = bits;
    }
    gs_image_t_init_mask(&i_mask, false);
    i_mask.adjust = false;
    {
	const gx_image_type_t *type1 = i_mask.type;

	*(gs_data_image_t *)&i_mask = pim->MaskDict;
	i_mask.type = type1;
    }
    {
	gx_drawing_color dcolor;
	gs_matrix m_mat;

	(*dev_proc(mdev, fill_rectangle))
	    ((gx_device *) mdev, 0, 0, mdev->width, mdev->height,
	     (gx_color_index) 0);
	color_set_pure(&dcolor, 1);
	/*
	 * Adjust the translation for rendering the mask to include a
	 * negative translation by origin.{x,y} in device space.
	 */
	m_mat = *pmat;
	m_mat.tx -= origin.x;
	m_mat.ty -= origin.y;
	/*
	 * Note that pis = NULL here, since we don't want to have to
	 * create another imager state with default log_op, etc.
	 */
	code = gx_device_begin_typed_image((gx_device *)mdev, NULL, &m_mat,
					   (const gs_image_common_t *)&i_mask,
					   prect, &dcolor, NULL, mem,
					   &penum->mask_info);
	if (code < 0)
	    goto out5;
    }
    gs_image_t_init(&i_pixel, pim->ColorSpace);
    {
	const gx_image_type_t *type1 = i_pixel.type;

	*(gs_pixel_image_t *)&i_pixel = *(const gs_pixel_image_t *)pim;
	i_pixel.type = type1;
    }
    code = gx_device_begin_typed_image((gx_device *) pcdev, pis, pmat,
				       (const gs_image_common_t *)&i_pixel,
			   prect, pdcolor, pcpath, mem, &penum->pixel_info);
    if (code < 0)
	goto out6;
    /*
     * Compute num_planes and plane_depths from the values in the
     * enumerators for the mask and the image data.
     */
    if (pim->InterleaveType == interleave_chunky) {
	/* Add the mask data to the depth of the image data. */
	penum->num_planes = 1;
	penum->plane_depths[0] =
	    penum->pixel_info->plane_depths[0] *
	    (penum->num_components + 1) / penum->num_components;
    } else {
	/* Insert the mask data as a separate plane before the image data. */
	penum->num_planes = penum->pixel_info->num_planes + 1;
	penum->plane_depths[0] = 1;
	memcpy(&penum->plane_depths[1], &penum->pixel_info->plane_depths[0],
	       (penum->num_planes - 1) * sizeof(penum->plane_depths[0]));
    }
    *pinfo = (gx_image_enum_common_t *) penum;
    return 0;
  out6:gx_image_end(penum->mask_info, false);
  out5:gs_closedevice((gx_device *) pcdev);
  out4:gs_closedevice((gx_device *) mdev);
  out3:gs_free_object(mem, pcdev, "gx_begin_image3(pcdev)");
  out2:gs_free_object(mem, mdev, "gx_begin_image3(mdev)");
  out1:gs_free_object(mem, penum->mask_data, "gx_begin_image3(mask_data)");
    gs_free_object(mem, penum->pixel_data, "gx_begin_image3(pixel_data)");
    gs_free_object(mem, penum, "gx_begin_image3");
    code = gs_note_error(gs_error_VMerror);
  out:return code;
}

/* Process the next piece of an ImageType 3 image. */
private int
gx_image3_plane_data(gx_device * dev,
 gx_image_enum_common_t * info, const gx_image_plane_t * planes, int height)
{
    gx_image3_enum_t *penum = (gx_image3_enum_t *) info;
    int pixel_height = penum->pixel_height;
    int mask_height = penum->mask_height;
    int image_height = max(pixel_height, mask_height);
    int h = min(height, image_height - penum->y);
    const gx_image_plane_t *pixel_planes;
    gx_image_plane_t pixel_plane, mask_plane;

    switch (penum->InterleaveType) {
	case interleave_chunky:
	    if (h <= 0)
		return 0;
	    if (h > 1) {
		/* Do the operation one row at a time. */
		mask_plane = planes[0];
		do {
		    int code = gx_image3_plane_data(dev, info, &mask_plane, 1);

		    if (code < 0)
			return code;
		    mask_plane.data += mask_plane.raster;
		} while (--h);
		return 0;
	    } {
		/* Pull apart the source data and the mask data. */
		int bpc = penum->bpc;
		int num_components = penum->num_components;
		int width = penum->pixel_width;

		/* We do this in the simplest (not fastest) way for now. */
		uint bit_x = bpc * (num_components + 1) * planes[0].data_x;

		sample_load_declare_setup(sptr, sbit,
					  planes[0].data + (bit_x >> 3),
					  bit_x & 7, bpc);
		sample_store_declare_setup(mptr, mbit, mbbyte, penum->mask_data,
					   0, 1);
		sample_store_declare_setup(pptr, pbit, pbbyte, penum->pixel_data,
					   0, bpc);
		int x;

		mask_plane.data = mptr;
		mask_plane.data_x = 0;
		/* raster doesn't matter */
		pixel_plane.data = pptr;
		pixel_plane.data_x = 0;
		/* raster doesn't matter */
		pixel_planes = &pixel_plane;
		for (x = 0; x < width; ++x) {
		    uint value;
		    int i;

		    sample_load_next12(value, sptr, sbit, bpc);
		    sample_store_next12(value != 0, mptr, mbit, 1, mbbyte);
		    for (i = 0; i < num_components; ++i) {
			sample_load_next12(value, sptr, sbit, bpc);
			sample_store_next12(value, pptr, pbit, bpc, pbbyte);
		    }
		}
		sample_store_flush(mptr, mbit, 1, mbbyte);
		sample_store_flush(pptr, pbit, bpc, pbbyte);
	    }
	    break;
	case interleave_scan_lines:
	case interleave_separate_source:
	    mask_plane = planes[0];
	    pixel_planes = planes + 1;
	    break;
	default:		/* not possible */
	    return_error(gs_error_rangecheck);
    }
    /*
     * Process the mask data first, so it will set up the mask
     * device for clipping the pixel data.
     */
    if (mask_plane.data) {
	int code = gx_image_plane_data(penum->mask_info, &mask_plane, h);

	if (code < 0)
	    return code;
    }
    if (pixel_planes[0].data) {
	int code;

	/*
	 * If necessary, flush any buffered mask data to the mask clipping
	 * device.
	 */
	if (penum->mask_info->procs->flush)
	    (*penum->mask_info->procs->flush)(penum->mask_info);
	code = gx_image_plane_data(penum->pixel_info, pixel_planes, h);
	if (code < 0)
	    return code;
	penum->y += h;
    }
    return penum->y >= image_height;
}

/* Clean up after processing an ImageType 3 image. */
private int
gx_image3_end_image(gx_device * dev, gx_image_enum_common_t * info,
		    bool draw_last)
{
    gx_image3_enum_t *penum = (gx_image3_enum_t *) info;
    gs_memory_t *mem = penum->memory;
    gx_device_memory *mdev = penum->mdev;
    int mcode = gx_image_end(penum->mask_info, draw_last);
    gx_device_mask_clip *pcdev = penum->pcdev;
    int pcode = gx_image_end(penum->pixel_info, draw_last);

    gs_closedevice((gx_device *) pcdev);
    gs_closedevice((gx_device *) mdev);
    gs_free_object(mem, penum->mask_data,
		   "gx_image3_end_image(mask_data)");
    gs_free_object(mem, penum->pixel_data,
		   "gx_image3_end_image(pixel_data)");
    gs_free_object(mem, pcdev, "gx_image3_end_image(pcdev)");
    gs_free_object(mem, mdev, "gx_image3_end_image(mdev)");
    gs_free_object(mem, penum, "gx_image3_end_image");
    return (pcode < 0 ? pcode : mcode);
}
