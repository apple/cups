/* Copyright (C) 1993, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxdcolor.h,v 1.2 2000/03/08 23:14:56 mike Exp $ */
/* Device color representation for Ghostscript */

#ifndef gxdcolor_INCLUDED
#  define gxdcolor_INCLUDED

#include "gscsel.h"
#include "gsdcolor.h"
#include "gsropt.h"
#include "gsstruct.h"		/* for extern_st, GC procs */

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

/*
 * Note that the following definition depends on the gx_color_index for
 * black, which may not be 0.  Clients must check this and construct
 * a different null source if necessary.
 */
#define gx_rop_no_source_body(black_pixel)\
  NULL, 0, 0, gx_no_bitmap_id, {black_pixel, black_pixel}, true
#define gx_rop_source_set_color(prs, pixel)\
  ((prs)->scolors[0] = (prs)->scolors[1] = (pixel))
void gx_set_rop_no_source(P3(const gx_rop_source_t **psource,
			     gx_rop_source_t *pno_source, gx_device *dev));
#define set_rop_no_source(source, no_source, dev)\
  gx_set_rop_no_source(&(source), &(no_source), dev)

/*
 * Define the device color structure per se.
 */

/* The typedef is in gsdcolor.h. */
/*typedef struct gx_device_color_type_s gx_device_color_type_t; */
struct gx_device_color_type_s {

    /*
     * In order to simplify memory management, we use a union, but since
     * different variants may have different pointer tracing procedures,
     * we have to define a separate GC structure type for each variant.
     */

    gs_memory_type_ptr_t stype;

    /*
     * If necessary and possible, load the halftone or Pattern cache
     * with the rendering of this color.
     */

#define dev_color_proc_load(proc)\
  int proc(P4(gx_device_color *pdevc, const gs_imager_state *pis,\
    gx_device *dev, gs_color_select_t select))
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
     * Fill a masked region with a color.  Nearly all device colors
     * use the default implementation, which simply parses the mask
     * into rectangles and calls fill_rectangle.  Note that in this
     * case there is no RasterOp source: the mask is the source.
     */

#define dev_color_proc_fill_masked(proc)\
  int proc(P12(const gx_device_color *pdevc, const byte *data, int data_x,\
    int raster, gx_bitmap_id id, int x, int y, int w, int h,\
    gx_device *dev, gs_logical_operation_t lop, bool invert))
                         dev_color_proc_fill_masked((*fill_masked));

    /*
     * Test whether this color is equal to another.
     */

#define dev_color_proc_equal(proc)\
  bool proc(P2(const gx_device_color *pdevc1, const gx_device_color *pdevc2))
                         dev_color_proc_equal((*equal));

};

/* Define the default implementation of fill_masked. */
dev_color_proc_fill_masked(gx_dc_default_fill_masked);

extern_st(st_device_color);
/* public_st_device_color() is defined in gsdcolor.h */

/* Define the standard device color types. */
/* See gsdcolor.h for details. */
extern const gx_device_color_type_t
#define gx_dc_type_none (&gx_dc_type_data_none)
      gx_dc_type_data_none,	/* gxdcolor.c */
#define gx_dc_type_null (&gx_dc_type_data_null)
      gx_dc_type_data_null,	/* gxdcolor.c */
#define gx_dc_type_pure (&gx_dc_type_data_pure)
      gx_dc_type_data_pure,	/* gxdcolor.c */
/*#define gx_dc_type_pattern (&gx_dc_type_data_pattern) */
						/*gx_dc_type_data_pattern, *//* gspcolor.c */
#define gx_dc_type_ht_binary (&gx_dc_type_data_ht_binary)
      gx_dc_type_data_ht_binary,	/* gxht.c */
#define gx_dc_type_ht_colored (&gx_dc_type_data_ht_colored)
      gx_dc_type_data_ht_colored;	/* gxcht.c */

#define gs_color_writes_pure(pgs)\
  color_writes_pure((pgs)->dev_color, (pgs)->log_op)

/* Set up device color 1 for writing into a mask cache */
/* (e.g., the character cache). */
void gx_set_device_color_1(P1(gs_state * pgs));

/* Get the black and white pixel values of a device. */
gx_color_index gx_device_black(P1(gx_device *dev));
gx_color_index gx_device_white(P1(gx_device *dev));

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
#define gx_color_load_select(pdevc, pis, dev, select)\
  (*(pdevc)->type->load)(pdevc, pis, dev, select)
#define gx_color_load(pdevc, pis, dev)\
  gx_color_load_select(pdevc, pis, dev, gs_color_select_texture)
#define gs_state_color_load(pgs)\
  gx_color_load((pgs)->dev_color, (const gs_imager_state *)(pgs),\
		(pgs)->device)

/* Fill a rectangle with a color. */
#define gx_device_color_fill_rectangle(pdevc, x, y, w, h, dev, lop, source)\
  (*(pdevc)->type->fill_rectangle)(pdevc, x, y, w, h, dev, lop, source)
#define gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop)\
  gx_device_color_fill_rectangle(pdevc, x, y, w, h, dev, lop, NULL)
#define gx_fill_rectangle_rop(x, y, w, h, pdevc, lop, pgs)\
  gx_fill_rectangle_device_rop(x, y, w, h, pdevc, (pgs)->device, lop)
#define gx_fill_rectangle(x, y, w, h, pdevc, pgs)\
  gx_fill_rectangle_rop(x, y, w, h, pdevc, (pgs)->log_op, pgs)

/* Test device colors for equality. */
#define gx_device_color_equal(pdevc1, pdevc2)\
  (((pdevc1)->type->equal)(pdevc1, pdevc2))

#endif /* gxdcolor_INCLUDED */
