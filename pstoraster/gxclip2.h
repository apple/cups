/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclip2.h */
/* Mask clipping device and interface */
/* Requires gxdevice.h, gxdevmem.h */

/*
 * Patterns that don't completely fill their bounding boxes require
 * the ability to clip against a tiled mask.  For now, we only support
 * tiling parallel to the axes.
 */

#define tile_clip_buffer_request 128
#define tile_clip_buffer_size\
  ((tile_clip_buffer_request / arch_sizeof_long) * arch_sizeof_long)
typedef struct gx_device_tile_clip_s {
	gx_device_forward_common;	/* target is set by client */
	gx_strip_bitmap tiles;
	gx_device_memory mdev;		/* for tile buffer for copy_mono */
	gs_int_point phase;		/* device space origin relative */
				/* to tile (backwards from gstate phase) */
	/* Ensure that the buffer is long-aligned. */
	union _b {
		byte bytes[tile_clip_buffer_size];
		ulong longs[tile_clip_buffer_size / arch_sizeof_long];
	} buffer;
} gx_device_tile_clip;
#define private_st_device_tile_clip() /* in gxclip2.c */\
  gs_private_st_simple(st_device_tile_clip, gx_device_tile_clip,\
    "gx_device_tile_clip")

/* Initialize a tile clipping device from a mask. */
/* We supply an explicit phase. */
int tile_clip_initialize(P5(gx_device_tile_clip *, const gx_strip_bitmap *,
  gx_device *, int, int));

/* Set the phase of the tile -- used in the tiling loop when */
/* the tile doesn't simply fill the plane. */
void tile_clip_set_phase(P3(gx_device_tile_clip *, int, int));
