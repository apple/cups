/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevmrop.h,v 1.2 2000/03/08 23:14:24 mike Exp $ */
/* Definitions for device RasterOp implementations. */
/* Requires gxdevmem.h, gsropt.h */

#ifndef gdevmrop_INCLUDED
#  define gdevmrop_INCLUDED

/* Define the table of RasterOp implementation procedures. */
extern const rop_proc rop_proc_table[256];

/* Define the table of RasterOp operand usage. */
extern const byte /*rop_usage_t */ rop_usage_table[256];

/*
 * Compute the effective RasterOp for the 1-bit case,
 * taking transparency into account.
 */
gs_rop3_t gs_transparent_rop(P1(gs_logical_operation_t lop));

#ifdef DEBUG
/* Trace a [strip_]copy_rop call. */
void trace_copy_rop(P16(const char *cname, gx_device * dev,
			const byte * sdata, int sourcex, uint sraster,
			gx_bitmap_id id, const gx_color_index * scolors,
			const gx_strip_bitmap * textures,
			const gx_color_index * tcolors,
			int x, int y, int width, int height,
			int phase_x, int phase_y, gs_logical_operation_t lop));
#endif

/*
 * PostScript colors normally act as the texture for RasterOp, with a null
 * (all zeros) source.  For images with CombineWithColor = true, we need
 * a way to use the image data as the source.  We implement this with a
 * device that applies RasterOp with a specified texture to drawing
 * operations, treating the drawing color as source rather than texture.
 * The texture is a gx_device_color; it may be any type of color, even a
 * pattern.
 */
#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;

#endif

#ifndef gx_device_rop_texture_DEFINED
#  define gx_device_rop_texture_DEFINED
typedef struct gx_device_rop_texture_s gx_device_rop_texture;

#endif

struct gx_device_rop_texture_s {
    gx_device_forward_common;
    gs_logical_operation_t log_op;
    gx_device_color texture;
};

#define private_st_device_rop_texture()	/* in gdevrops.c */\
  gs_private_st_composite(st_device_rop_texture, gx_device_rop_texture,\
    "gx_device_rop_texture", device_rop_texture_enum_ptrs, device_rop_texture_reloc_ptrs)

/* Create a RasterOp source device. */
int gx_alloc_rop_texture_device(P3(gx_device_rop_texture ** prsdev,
				   gs_memory_t * mem,
				   client_name_t cname));

/* Initialize a RasterOp source device. */
void gx_make_rop_texture_device(P4(gx_device_rop_texture * rsdev,
				   gx_device * target,
				   gs_logical_operation_t lop,
				   const gx_device_color * texture));

#endif /* gdevmrop_INCLUDED */
