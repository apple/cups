/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxmclip.c,v 1.1 2000/03/08 23:15:02 mike Exp $ */
/* Mask clipping support */
#include "gx.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxmclip.h"

/* Structure descriptor */
public_st_device_mask_clip();

/* GC procedures */
#define mcdev ((gx_device_mask_clip *)vptr)

private ENUM_PTRS_BEGIN(device_mask_clip_enum_ptrs)
{
    if (index < st_gx_strip_bitmap_max_ptrs) {
	return ENUM_USING(st_gx_strip_bitmap, &mcdev->tiles,
			  sizeof(mcdev->tiles), index);
    }
    index -= st_gx_strip_bitmap_max_ptrs;
    if (index < st_device_memory_max_ptrs) {
	return ENUM_USING(st_device_memory, &mcdev->mdev,
			  sizeof(mcdev->mdev), index);
    }
    ENUM_PREFIX(st_device_forward, st_device_memory_max_ptrs);
}
ENUM_PTRS_END

private RELOC_PTRS_BEGIN(device_mask_clip_reloc_ptrs)
{
    RELOC_PREFIX(st_device_forward);
    RELOC_USING(st_gx_strip_bitmap, &mcdev->tiles, sizeof(mcdev->tiles));
    RELOC_USING(st_device_memory, &mcdev->mdev, sizeof(mcdev->mdev));
    if (mcdev->mdev.base != 0) {
	/*
	 * Update the line pointers specially, since they point into the
	 * buffer that is part of the mask clipping device itself.
	 */
	long diff = (char *)RELOC_OBJ(mcdev) - (char *)mcdev;
	int i;

	for (i = 0; i < mcdev->mdev.height; ++i)
	    mcdev->mdev.line_ptrs[i] += diff;
	mcdev->mdev.base = mcdev->mdev.line_ptrs[0];
	mcdev->mdev.line_ptrs =
	    (void *)((char *)(mcdev->mdev.line_ptrs) + diff);
    }
}
RELOC_PTRS_END

#undef mcdev

/* Initialize a mask clipping device. */
int
gx_mask_clip_initialize(gx_device_mask_clip * cdev,
			const gx_device_mask_clip * proto,
			const gx_bitmap * bits, gx_device * tdev,
			int tx, int ty)
{
    int buffer_width = bits->size.x;
    int buffer_height =
	tile_clip_buffer_size / (bits->raster + sizeof(byte *));

    gx_device_init((gx_device *)cdev, (const gx_device *)proto,
		   NULL, true);
    cdev->width = tdev->width;
    cdev->height = tdev->height;
    cdev->color_info = tdev->color_info;
    cdev->target = tdev;
    cdev->phase.x = -tx;
    cdev->phase.y = -ty;
    if (buffer_height > bits->size.y)
	buffer_height = bits->size.y;
    gs_make_mem_mono_device(&cdev->mdev, 0, 0);
    for (;;) {
	if (buffer_height <= 0) {
	    /*
	     * The tile is too wide to buffer even one scan line.
	     * We could do copy_mono in chunks, but for now, we punt.
	     */
	    cdev->mdev.base = 0;
	    return 0;
	}
	cdev->mdev.width = buffer_width;
	cdev->mdev.height = buffer_height;
	if (gdev_mem_bitmap_size(&cdev->mdev) <= tile_clip_buffer_size)
	    break;
	buffer_height--;
    }
    cdev->mdev.base = cdev->buffer.bytes;
    return (*dev_proc(&cdev->mdev, open_device))((gx_device *)&cdev->mdev);
}
