/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsimage.c,v 1.3 2000/03/08 23:14:42 mike Exp $ */
/* Image setup procedures for Ghostscript library */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gscspace.h"
#include "gsmatrix.h"		/* for gsiparam.h */
#include "gsimage.h"
#include "gxarith.h"		/* for igcd */
#include "gxdevice.h"
#include "gxiparam.h"
#include "gxpath.h"		/* for gx_effective_clip_path */
#include "gzstate.h"

/* Define the enumeration state for this interface layer. */
							/*typedef struct gs_image_enum_s gs_image_enum; *//* in gsimage.h */
struct gs_image_enum_s {
    /* The following are set at initialization time. */
    gs_memory_t *memory;
    gx_device *dev;		/* if 0, just skip over the data */
    gx_image_enum_common_t *info;	/* driver bookkeeping structure */
    int num_planes;
    int width, height;
    uint raster;		/* bytes per row (per plane), no padding */
    /* The following are updated dynamically. */
    int plane_index;		/* index of next plane of data */
    int y;
    uint pos;			/* byte position within the scan line */
    gs_const_string sources[gs_image_max_components];	/* source data */
    gs_string rows[gs_image_max_components];	/* row buffers */
    bool error;
};

gs_private_st_composite(st_gs_image_enum, gs_image_enum, "gs_image_enum",
			gs_image_enum_enum_ptrs, gs_image_enum_reloc_ptrs);
#define gs_image_enum_num_ptrs 2

/* GC procedures */
#define eptr ((gs_image_enum *)vptr)
private 
ENUM_PTRS_BEGIN(gs_image_enum_enum_ptrs)
{
    /* Enumerate the data planes. */
    index -= gs_image_enum_num_ptrs;
    if (index < eptr->plane_index)
	ENUM_RETURN_STRING_PTR(gs_image_enum, sources[index]);
    index -= eptr->plane_index;
    if (index < eptr->num_planes)
	ENUM_RETURN_STRING_PTR(gs_image_enum, rows[index]);
    return 0;
}
ENUM_PTR(0, gs_image_enum, dev);
ENUM_PTR(1, gs_image_enum, info);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(gs_image_enum_reloc_ptrs)
{
    int i;

    RELOC_PTR(gs_image_enum, dev);
    RELOC_PTR(gs_image_enum, info);
    for (i = 0; i < eptr->plane_index; i++)
	RELOC_CONST_STRING_PTR(gs_image_enum, sources[i]);
    for (i = 0; i < eptr->num_planes; i++)
	RELOC_STRING_PTR(gs_image_enum, rows[i]);
}
RELOC_PTRS_END
#undef eptr

/* Create an image enumerator given image parameters and a graphics state. */
int
gs_image_begin_typed(const gs_image_common_t * pic, gs_state * pgs,
		     bool uses_color, gx_image_enum_common_t ** ppie)
{
    gx_device *dev = gs_currentdevice(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);

    if (code < 0)
	return code;
    if (uses_color)
	gx_set_dev_color(pgs);
    return gx_device_begin_typed_image(dev, (const gs_imager_state *)pgs,
		NULL, pic, NULL, pgs->dev_color, pcpath, pgs->memory, ppie);
}

/* Allocate an image enumerator. */
private void
image_enum_init(gs_image_enum * penum)
{				/* Clean pointers for GC. */
    int i;

    penum->info = 0;
    penum->dev = 0;
    for (i = 0; i < countof(penum->sources); ++i) {
	penum->sources[i].data = 0, penum->sources[i].size = 0;
	penum->rows[i].data = 0, penum->rows[i].size = 0;
    }
}
gs_image_enum *
gs_image_enum_alloc(gs_memory_t * mem, client_name_t cname)
{
    gs_image_enum *penum =
    gs_alloc_struct(mem, gs_image_enum, &st_gs_image_enum, cname);

    if (penum != 0) {
	penum->memory = mem;
	image_enum_init(penum);
    }
    return penum;
}

/* Start processing an ImageType 1 image. */
int
gs_image_init(gs_image_enum * penum, const gs_image_t * pim, bool multi,
	      gs_state * pgs)
{
    gs_image_t image;
    gx_image_enum_common_t *pie;
    int code;

    image = *pim;
    if (image.ImageMask) {
	image.ColorSpace = NULL;
	if (pgs->in_cachedevice <= 1)
	    image.adjust = false;
    } else {
	if (pgs->in_cachedevice)
	    return_error(gs_error_undefined);
	if (image.ColorSpace == NULL)
	    image.ColorSpace =
		gs_cspace_DeviceGray((const gs_imager_state *)pgs);
    }
    code = gs_image_begin_typed((const gs_image_common_t *)&image, pgs,
				image.ImageMask | image.CombineWithColor,
				&pie);
    if (code < 0)
	return code;
    return gs_image_common_init(penum, pie,
				(const gs_data_image_t *)&image,
				pgs->memory,
				(pgs->in_charpath ? NULL :
				 gs_currentdevice_inline(pgs)));
}
/* Start processing a general image. */
int
gs_image_common_init(gs_image_enum * penum, gx_image_enum_common_t * pie,
	    const gs_data_image_t * pim, gs_memory_t * mem, gx_device * dev)
{
    if (pim->Width == 0 || pim->Height == 0) {
	gx_image_end(pie, false);
	return 1;
    }
    image_enum_init(penum);
    penum->memory = mem;
    penum->dev = dev;
    penum->info = pie;
    penum->num_planes = pie->num_planes;
    penum->width = pim->Width;
    penum->height = pim->Height;
/****** ALL PLANES MUST HAVE SAME DEPTH FOR NOW ******/
    penum->raster = (pim->Width * pie->plane_depths[0] + 7) >> 3;
    /* Initialize the dynamic part of the state. */
    penum->plane_index = 0;
    penum->y = 0;
    penum->pos = 0;
    penum->error = false;
    return 0;
}

/*
 * Return the number of bytes of data per row per plane.
 */
uint
gs_image_bytes_per_plane_row(const gs_image_enum * penum, int plane)
{
/****** IGNORE PLANE FOR NOW ******/
    return penum->raster;
}

/* Process the next piece of an image. */
private int
copy_planes(gx_device * dev, gs_image_enum * penum, const byte ** planes,
	    int h)
{
    int code =
	(penum->dev == 0 ? (penum->y + h < penum->height ? 0 : 1) :
	 gx_image_data(penum->info, planes, 0, penum->raster, h));

    if (code < 0)
	penum->error = true;
    return code;
}
int
gs_image_next(gs_image_enum * penum, const byte * dbytes, uint dsize,
	      uint * pused)
{
    gx_device *dev;
    uint left;
    int num_planes;
    uint raster;
    uint pos;
    int code;

    /*
     * Handle the following differences between gs_image_next and
     * the device image_data procedure:
     *
     *      - image_data requires an array of planes; gs_image_next
     *      expects planes in successive calls.
     *
     *      - image_data requires that each call pass entire rows;
     *      gs_image_next allows arbitrary amounts of data.
     */
    if (penum->plane_index != 0)
	if (dsize != penum->sources[0].size)
	    return_error(gs_error_rangecheck);
    penum->sources[penum->plane_index].data = dbytes;
    penum->sources[penum->plane_index].size = dsize;
    if (++(penum->plane_index) != penum->num_planes)
	return 0;
    /* We have a full set of planes. */
    dev = penum->dev;
    left = dsize;
    num_planes = penum->num_planes;
    raster = penum->raster;
    pos = penum->pos;
    code = 0;
    while (left && penum->y < penum->height) {
	const byte *planes[gs_image_max_components];
	int i;

	for (i = 0; i < num_planes; ++i)
	    planes[i] = penum->sources[i].data + dsize - left;
	if (pos == 0 && left >= raster) {	/* Pass (a) row(s) directly from the source. */
	    int h = left / raster;

	    if (h > penum->height - penum->y)
		h = penum->height - penum->y;
	    code = copy_planes(dev, penum, planes, h);
	    if (code < 0)
		break;
	    left -= raster * h;
	    penum->y += h;
	} else {		/* Buffer a partial row. */
	    uint count = min(left, raster - pos);

	    if (penum->rows[0].data == 0) {	/* Allocate the row buffers. */
		for (i = 0; i < num_planes; ++i) {
		    byte *row = gs_alloc_string(penum->memory, raster,
						"gs_image_next(row)");

		    if (row == 0) {
			code = gs_note_error(gs_error_VMerror);
			while (--i >= 0) {
			    gs_free_string(penum->memory, penum->rows[i].data,
					   raster, "gs_image_next(row)");
			    penum->rows[i].data = 0;
			    penum->rows[i].size = 0;
			}
			break;
		    }
		    penum->rows[i].data = row;
		    penum->rows[i].size = raster;
		}
		if (code < 0)
		    break;
	    }
	    for (i = 0; i < num_planes; ++i)
		memcpy(penum->rows[i].data + pos, planes[i], count);
	    pos += count;
	    left -= count;
	    if (pos == raster) {
		for (i = 0; i < num_planes; ++i)
		    planes[i] = penum->rows[i].data;
		code = copy_planes(dev, penum, planes, 1);
		if (code < 0)
		    break;
		pos = 0;
		penum->y++;
	    }
	}
    }
    penum->pos = pos;
    penum->plane_index = 0;
    *pused = dsize - left;
    return code;
}

/* Clean up after processing an image. */
void
gs_image_cleanup(gs_image_enum * penum)
{
    int i;

    for (i = 0; i < penum->num_planes; ++i)
	gs_free_string(penum->memory, penum->rows[i].data,
		       penum->rows[i].size, "gs_image_cleanup(row)");
    if (penum->dev != 0)
	gx_image_end(penum->info, !penum->error);
    /* Don't free the local enumerator -- the client does that. */
}
