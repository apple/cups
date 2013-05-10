/* Copyright (C) 1993, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxdcolor.h */
/* Device color representation for Ghostscript */

#ifndef gxdcolor_INCLUDED
#  define gxdcolor_INCLUDED

#include "gsdcolor.h"
#include "gsropt.h"
#include "gsstruct.h"			/* for extern_st, GC procs */

/* Define opaque types. */

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;
#endif

/*
 * Define a source structure for RasterOp.
 */
typedef struct gx_rop_source_s {
	const byte *sdata;
	int sourcex;
	uint sraster;
	gx_bitmap_id id;
	gx_color_index scolors[2];
	bool use_scolors;
} gx_rop_source_t;
#define gx_rop_no_source_body\
  NULL, 0, 0, gx_no_bitmap_id, {0, 0}, true
extern const gx_rop_source_t gx_rop_no_source;

/*
 * Device colors are 'objects' (with very few procedures).  In order to
 * simplify memory management, we use a union, but since different variants
 * may have different pointer tracing procedures, we have to include those
 * procedures in the type.
 */

struct gx_device_color_procs_s {

	/*
	 * If necessary and possible, load the halftone or Pattern cache
	 * with the rendering of this color.
	 */

#define dev_color_proc_load(proc)\
  int proc(P2(gx_device_color *pdevc, const gs_state *pgs))
	dev_color_proc_load((*load));

	/*
	 * Fill a rectangle with the color.
	 * We pass the device separately so that pattern fills can
	 * substitute a tiled mask clipping device.
	 */

#define dev_color_proc_fill_rectangle(proc)\
  int proc(P8(const gx_device_color *pdevc, int x, int y, int w, int h,\
    gx_device *dev, gs_logical_operation_t lop, const gx_rop_source_t *source))
	dev_color_proc_fill_rectangle((*fill_rectangle));

	/*
	 * Trace the pointers for the garbage collector.
	 */

	struct_proc_enum_ptrs((*enum_ptrs));
	struct_proc_reloc_ptrs((*reloc_ptrs));

};

extern_st(st_device_color);
/* public_st_device_color() is defined in gsdcolor.h */

/* Define the standard device color types. */
/* See gsdcolor.h for details. */
extern const gx_device_color_procs
#define gx_dc_type_none (&gx_dc_procs_none)
	gx_dc_procs_none,			/* gxdcolor.c */
#define gx_dc_type_pure (&gx_dc_procs_pure)
	gx_dc_procs_pure,			/* gxdcolor.c */
/*#define gx_dc_type_pattern (&gx_dc_procs_pattern)*/
	/*gx_dc_procs_pattern,*/		/* gspcolor.c */
#define gx_dc_type_ht_binary (&gx_dc_procs_ht_binary)
	gx_dc_procs_ht_binary,			/* gxht.c */
#define gx_dc_type_ht_colored (&gx_dc_procs_ht_colored)
	gx_dc_procs_ht_colored;			/* gxcht.c */

#define gs_color_writes_pure(pgs)\
  color_writes_pure((pgs)->dev_color, (pgs)->log_op)

/* Set up device color 1 for writing into a mask cache */
/* (e.g., the character cache). */
void gx_set_device_color_1(P1(gs_state *pgs));

/* Remap the color if necessary. */
int gx_remap_color(P1(gs_state *));
#define gx_set_dev_color(pgs)\
  if ( !color_is_set((pgs)->dev_color) )\
   { int code_dc = gx_remap_color(pgs);\
     if ( code_dc != 0 ) return code_dc;\
   }

/* Indicate that the device color needs remapping. */
#define gx_unset_dev_color(pgs)\
  color_unset((pgs)->dev_color)

/* Load the halftone cache in preparation for drawing. */
#define gx_color_load(pdevc, pgs)\
  (*(pdevc)->type->load)(pdevc, pgs)

/* Fill a rectangle with a color. */
#define gx_device_color_fill_rectangle(pdevc, x, y, w, h, dev, lop, source)\
  (*(pdevc)->type->fill_rectangle)(pdevc, x, y, w, h, dev, lop, source)
#define gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop)\
  gx_device_color_fill_rectangle(pdevc, x, y, w, h, dev, lop, NULL)
#define gx_fill_rectangle_rop(x, y, w, h, pdevc, lop, pgs)\
  gx_fill_rectangle_device_rop(x, y, w, h, pdevc, (pgs)->device, lop)
#define gx_fill_rectangle(x, y, w, h, pdevc, pgs)\
  gx_fill_rectangle_rop(x, y, w, h, pdevc, (pgs)->log_op, pgs)

#endif					/* gxdcolor_INCLUDED */
