/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevmrop.h */
/* Interfaces to RasterOp implementation */
/* Requires gxdevmem.h, gsropt.h */

/* Define the table of RasterOp implementation procedures. */
extern const far_data rop_proc rop_proc_tab[256];

/*
 * PostScript colors normally act as the texture for RasterOp, with a null
 * (all zeros) source.  For images with CombineWithColor = true, we need
 * a way to use the image data as the source.  We implement this with a
 * device that applies RasterOp with a specified texture to drawing
 * operations, treating the drawing color as source rather than texture.
 * The texture is a gx_device_color; it may be any type of PostScript color,
 * even a Pattern.
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
	const gx_device_color *texture;
};
extern_st(st_device_rop_texture);
#define public_st_device_rop_texture()\
  gs_public_st_suffix_add1(st_device_rop_texture, gx_device_rop_texture,\
    "gx_device_rop_texture", device_rop_texture_enum_ptrs, device_rop_texture_reloc_ptrs,\
    st_device_forward, texture)

/* Initialize a RasterOp source device. */
void gx_make_rop_texture_device(P4(gx_device_rop_texture *rsdev,
				   gx_device *target,
				   gs_logical_operation_t lop,
				   const gx_device_color *texture));
