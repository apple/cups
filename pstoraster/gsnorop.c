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

/*$Id: gsnorop.c,v 1.1 2000/03/08 23:14:45 mike Exp $ */
/* Stubs for unimplemented RasterOp */
#include "gx.h"
#include "gserrors.h"
#include "gsrop.h"
#include "gxdevcli.h"
#include "gdevmrop.h"

/* Stub accessors to logical operation in graphics state. */

gs_logical_operation_t
gs_current_logical_op(const gs_state * pgs)
{
    return lop_default;
}

int
gs_set_logical_op(gs_state * pgs, gs_logical_operation_t lop)
{
    return (lop == lop_default ? 0 : gs_note_error(gs_error_rangecheck));
}

/* Stub RasterOp implementations for memory devices. */

int
mem_mono_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			int x, int y, int width, int height,
			int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return_error(gs_error_rangecheck);
}

int
mem_gray_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			int x, int y, int width, int height,
			int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return_error(gs_error_rangecheck);
}

int
mem_gray8_rgb24_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			       const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			       int x, int y, int width, int height,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return_error(gs_error_rangecheck);
}

/* Stub default implementations of device procedures. */

int
gx_default_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
		    const gx_color_index * scolors,
	     const gx_tile_bitmap * texture, const gx_color_index * tcolors,
		    int x, int y, int width, int height,
		    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return_error(gs_error_unknownerror);	/* not implemented */
}

int
gx_default_strip_copy_rop(gx_device * dev,
	     const byte * sdata, int sourcex, uint sraster, gx_bitmap_id id,
			  const gx_color_index * scolors,
	   const gx_strip_bitmap * textures, const gx_color_index * tcolors,
			  int x, int y, int width, int height,
		       int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return_error(gs_error_unknownerror);	/* not implemented */
}

/* Stub RasterOp source devices. */

int
gx_alloc_rop_texture_device(gx_device_rop_texture ** prsdev, gs_memory_t * mem,
			    client_name_t cname)
{
    return_error(gs_error_rangecheck);
}

void
gx_make_rop_texture_device(gx_device_rop_texture * dev, gx_device * target,
	     gs_logical_operation_t log_op, const gx_device_color * texture)
{				/* Never called. */
}
