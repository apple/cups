/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevbbox.h */
/* Interface to bounding box device */
/* Requires gxdevice.h */

/*
 * This device keeps track of the per-page bounding box,
 * and also forwards all drawing commands to a target.
 * It isn't used in any standard way right now, but it's been requested
 * by several customers.
 *
 * To ensure that the bounding box device code is linked into the
 * executable, add gdevbbox.$(OBJ) to the end of the definition of the
 * LIB3d macro in the makefile (lib.mak).
 *
 * To set up a bounding box device that doesn't do any drawing:
 *	gx_device_bbox *bdev =
 *	  gs_alloc_struct_immovable(some_memory,
 *				    gx_device_bbox, &st_device_bbox,
 *				    "some identifying string for debugging");
 *	gx_device_bbox_init(bdev, NULL);
 * Non-drawing bounding box devices have an "infinite" page size.
 *
 * To set up a bounding box device that draws to another device tdev:
 *	gx_device_bbox *bdev =
 *	  gs_alloc_struct_immovable(some_memory,
 *				    gx_device_bbox, &st_device_bbox,
 *				    "some identifying string for debugging");
 *	gx_device_bbox_init(bdev, tdev);
 * Bounding box devices that draw to a real device appear to have the
 * same page size as that device.
 *
 * To intercept the end-of-page to call a routine eop of your own:
 *	dev_proc_output_page(eop);	-- declare a prototype for eop
 *	bdev.std_procs.output_page = eop;
 *	...
 *	int eop(gx_device *dev, int num_copies, int flush)
 *	{	gs_rect bbox;
 *		gx_device_bbox_bbox((gx_device_bbox *)dev, &bbox);
 *		<< do whatever you want >>
 *		return gx_forward_output_page(dev, num_copies, flush);
 *	}
 */
typedef struct gx_device_bbox_s {
	gx_device_forward_common;
	/* The following are updated dynamically. */
	gs_fixed_rect bbox;
	gx_color_index white;
} gx_device_bbox;
extern_st(st_device_bbox);
#define public_st_device_bbox()	/* in gdevbbox.c */\
  gs_public_st_suffix_add0_final(st_device_bbox, gx_device_bbox,\
    "gx_device_bbox", device_bbox_enum_ptrs, device_bbox_reloc_ptrs,\
    gx_device_finalize, st_device_forward)

/* Initialize a bounding box device. */
void gx_device_bbox_init(P2(gx_device_bbox *dev, gx_device *target));

/* Read back the bounding box in 1/72" units. */
void gx_device_bbox_bbox(P2(gx_device_bbox *dev, gs_rect *pbbox));
