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

/*$Id: gxmclip.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Mask clipping device and interface */
/* Requires gxdevice.h, gxdevmem.h */

#ifndef gxmclip_INCLUDED
#  define gxmclip_INCLUDED

#include "gxclip.h"

/*
 * ImageType 3 images and Patterns that don't completely fill their
 * bounding box require the ability to clip against a mask.
 * The interface declared here doesn't take a position on whether
 * the mask will be used only in one position (ImageType 3) or in
 * multiple positions for tiling (Patterns).
 *
 * All the information in this file is logically private, but we must expose
 * the structure definition so that clients can allocate instances in the
 * stack frame.
 */

#define tile_clip_buffer_request 300
#define tile_clip_buffer_size\
  ((tile_clip_buffer_request / arch_sizeof_long) * arch_sizeof_long)
typedef struct gx_device_mask_clip_s {
    gx_device_forward_common;	/* target is set by client */
    gx_strip_bitmap tiles;
    gx_device_memory mdev;	/* for tile buffer for copy_mono */
    gs_int_point phase;		/* device space origin relative */
				/* to tile (backwards from gstate phase) */
    /* Ensure that the buffer is long-aligned. */
    union _b {
	byte bytes[tile_clip_buffer_size];
	ulong longs[tile_clip_buffer_size / arch_sizeof_long];
    } buffer;
} gx_device_mask_clip;

extern_st(st_device_mask_clip);
#define public_st_device_mask_clip()	/* in gxmclip.c */\
  gs_public_st_composite(st_device_mask_clip, gx_device_mask_clip,\
    "gx_device_mask_clip", device_mask_clip_enum_ptrs,\
    device_mask_clip_reloc_ptrs)

/*
 * Internal routine to initialize a mask clipping device.
 * We supply an explicit device space origin or phase.
 * Note that this procedure does not set cdev->tiles.
 */
int gx_mask_clip_initialize(P6(gx_device_mask_clip * cdev,
			       const gx_device_mask_clip * proto,
			       const gx_bitmap * bits, gx_device * tdev,
			       int tx, int ty));

/*
 * Prepare colors for a copy_mono operation.
 * The arguments of copy_mono are free variables:
 *   dev, data, sourcex, raster, id, x, y, w, y, color0, color1.
 */
#define setup_mask_copy_mono(cdev, color, mcolor0, mcolor1)\
	BEGIN\
	  if ( cdev->mdev.base == 0 ) {\
	    /*\
	     * The tile was too large for us to buffer even one scan line.\
	     * Punt to the very, very slow default implementation of\
	     * copy_mono.\
	     */\
	    return gx_default_copy_mono(dev, data, sourcex, raster, id,\
					x, y, w, h, color0, color1);\
	  }\
	  if ( color1 != gx_no_color_index ) {\
	    if ( color0 != gx_no_color_index ) {\
	      /* Pre-fill with color0. */\
	      code =\
		(*dev_proc(dev, fill_rectangle))(dev, x, y, w, h, color0);\
	      if ( code < 0 )\
		return code;\
	    }\
	    color = color1;\
	    mcolor0 = 0, mcolor1 = gx_no_color_index;\
	  } else if ( color0 != gx_no_color_index ) {\
	    color = color0;\
	    mcolor0 = gx_no_color_index, mcolor1 = 0;\
	  } else\
	    return 0;\
	END

#endif /* gxmclip_INCLUDED */
