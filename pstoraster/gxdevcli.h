/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxdevcli.h,v 1.2 2000/06/23 14:48:50 mike Exp $ */
/* Definitions for device clients */

#ifndef gxdevcli_INCLUDED
#  define gxdevcli_INCLUDED

#include "std.h"		/* for FILE */
#include "gscompt.h"
#include "gsdcolor.h"
#include "gsmatrix.h"
#include "gsiparam.h"		/* requires gsmatrix.h */
#include "gsrefct.h"
#include "gsropt.h"
#include "gsstruct.h"
#include "gsxfont.h"
#include "gxbitmap.h"
#include "gxcindex.h"
#include "gxcvalue.h"
#include "gxfixed.h"
#include "gxtext.h"

/* See Drivers.htm for documentation of the driver interface. */

#ifndef gx_device_DEFINED
#  define gx_device_DEFINED
typedef struct gx_device_s gx_device;

#endif

/* ---------------- Auxiliary types and structures ---------------- */

/* We need at least an abstract type for a graphics state, */
/* which is passed to the page device procedures. */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;

#endif

/* We need abstract types for paths and fill/stroke parameters, */
/* for the path-oriented device procedures. */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;

#endif
#ifndef gx_clip_path_DEFINED
#  define gx_clip_path_DEFINED
typedef struct gx_clip_path_s gx_clip_path;

#endif
#ifndef gx_fill_params_DEFINED
#  define gx_fill_params_DEFINED
typedef struct gx_fill_params_s gx_fill_params;

#endif
#ifndef gx_stroke_params_DEFINED
#  define gx_stroke_params_DEFINED
typedef struct gx_stroke_params_s gx_stroke_params;

#endif
#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;

#endif
/* We need an abstract type for the image enumeration state, */
/* for begin[_typed]_image. */
#ifndef gx_image_enum_common_t_DEFINED
#  define gx_image_enum_common_t_DEFINED
typedef struct gx_image_enum_common_s gx_image_enum_common_t;

#endif

/* Define the type for colors passed to the higher-level procedures. */
typedef gx_device_color gx_drawing_color;

/* Define a type for telling get_alpha_bits what kind of object */
/* is being rendered. */
typedef enum {
    go_text,
    go_graphics
} graphics_object_type;

/* Define an edge of a trapezoid.  Requirement: end.y >= start.y. */
typedef struct gs_fixed_edge_s {
    gs_fixed_point start;
    gs_fixed_point end;
} gs_fixed_edge;

/* Define the parameters passed to get_bits_rectangle. */
#ifndef gs_get_bits_params_DEFINED
#  define gs_get_bits_params_DEFINED
typedef struct gs_get_bits_params_s gs_get_bits_params_t;
#endif

/* Define the structure for device color capabilities. */
typedef struct gx_device_color_info_s {
    int num_components;		/* doesn't include alpha: */
    /* 0 = alpha only, 1 = gray only, */
    /* 3 = RGB, 4 = CMYK */
    int depth;			/* # of bits per pixel */
    gx_color_value max_gray;	/* # of distinct gray levels -1 */
    gx_color_value max_color;	/* # of distinct color levels -1 */
    /* (only relevant if num_comp. > 1) */
    gx_color_value dither_grays;	/* size of gray ramp for dithering */
    gx_color_value dither_colors;	/* size of color cube ditto */
    /* (only relevant if num_comp. > 1) */
} gx_device_color_info;

#define dci_values(nc,depth,mg,mc,dg,dc) { nc, depth, mg, mc, dg, dc }
#define dci_std_color(color_bits)\
  dci_values(\
    (color_bits == 32 ? 4 : color_bits > 1 ? 3 : 1),\
    ((color_bits > 1) & (color_bits < 8) ? 8 : color_bits),\
    (color_bits >= 8 ? 255 : 1),\
    (color_bits >= 8 ? 255 : color_bits > 1 ? 1 : 0),\
    (color_bits >= 8 ? 5 : 2),\
    (color_bits >= 8 ? 5 : color_bits > 1 ? 2 : 0)\
  )
#define dci_black_and_white dci_std_color(1)
#define dci_black_and_white_() dci_black_and_white
#define dci_color(depth,maxv,dither)\
  dci_values(3, depth, maxv, maxv, dither, dither)
#define gx_device_has_color(dev) ((dev)->color_info.num_components > 1)

/* Structure for device procedures. */
typedef struct gx_device_procs_s gx_device_procs;

/* Structure for page device procedures. */
/* Note that these take the graphics state as a parameter. */
typedef struct gx_page_device_procs_s {

#define dev_page_proc_install(proc)\
  int proc(P2(gx_device *dev, gs_state *pgs))
    dev_page_proc_install((*install));

#define dev_page_proc_begin_page(proc)\
  int proc(P2(gx_device *dev, gs_state *pgs))
    dev_page_proc_begin_page((*begin_page));

#define dev_page_proc_end_page(proc)\
  int proc(P3(gx_device *dev, int reason, gs_state *pgs))
    dev_page_proc_end_page((*end_page));

} gx_page_device_procs;

/* Default procedures */
dev_page_proc_install(gx_default_install);
dev_page_proc_begin_page(gx_default_begin_page);
dev_page_proc_end_page(gx_default_end_page);

/* ---------------- Device structure ---------------- */

/*
 * Define the generic device structure.  The device procedures can
 * have two different configurations:
 * 
 *      - Statically initialized devices predating release 2.8.1
 *      set the static_procs pointer to point to a separate procedure record,
 *      and do not initialize procs.
 *
 *      - Statically initialized devices starting with release 2.8.1,
 *      and all dynamically created device instances,
 *      set the static_procs pointer to 0, and initialize procs.
 *
 * The gx_device_set_procs procedure converts the first of these to
 * the second, which is what all client code starting in 2.8.1 expects
 * (using the procs record, not the static_procs pointer, to call the
 * driver procedures).
 *
 * The choice of the name Margins (rather than, say, HWOffset), and the
 * specification in terms of a default device resolution rather than
 * 1/72" units, are due to Adobe.
 *
 * ****** NOTE: If you define any subclasses of gx_device, you *must* define
 * ****** the finalization procedure as gx_device_finalize.  Finalization
 * ****** procedures are not automatically inherited.
 */
#define gx_device_common\
	int params_size;		/* OBSOLETE if stype != 0: */\
					/* size of this structure */\
	const gx_device_procs *static_procs;	/* OBSOLETE */\
					/* pointer to procs */\
	const char *dname;		/* the device name */\
	gs_memory_t *memory;		/* (0 iff static prototype) */\
	gs_memory_type_ptr_t stype;	/* memory manager structure type, */\
					/* 0 iff static prototype */\
	rc_header rc;			/* reference count from gstates, */\
					/* +1 if not internal device */\
	bool is_open;			/* true if device has been opened */\
	int max_fill_band;		/* limit on band size for fill, */\
					/* must be 0 or a power of 2 */\
					/* (see gdevabuf.c for more info) */\
	gx_device_color_info color_info;	/* color information */\
	int width;			/* width in pixels */\
	int height;			/* height in pixels */\
	float PageSize[2];		/* media dimensions in points */\
	float ImagingBBox[4];		/* imageable region in points */\
	  bool ImagingBBox_set;\
	float HWResolution[2];		/* resolution, dots per inch */\
	float MarginsHWResolution[2];	/* resolution for Margins */\
	float Margins[2];		/* offset of physical page corner */\
					/* from device coordinate (0,0), */\
					/* in units given by MarginsHWResolution */\
	float HWMargins[4];		/* margins around imageable area, */\
					/* in default user units ("points") */\
	long PageCount;			/* number of pages written */\
	long ShowpageCount;		/* number of calls on showpage */\
	int NumCopies;\
	  bool NumCopies_set;\
	bool IgnoreNumCopies;		/* if true, force num_copies = 1 */\
	gx_page_device_procs page_procs;	/* must be last */\
		/* end of std_device_body */\
	gx_device_procs procs	/* object procedures */
#define x_pixels_per_inch HWResolution[0]
#define y_pixels_per_inch HWResolution[1]
#define offset_margin_values(x, y, left, bot, right, top)\
  {x, y}, {left, bot, right, top}
#define margin_values(left, bot, right, top)\
  offset_margin_values(0, 0, left, bot, right, top)
#define no_margins margin_values(0, 0, 0, 0)
#define no_margins_() no_margins
/* Define macros that give the page offset ("Margins") in inches. */
#define dev_x_offset(dev) ((dev)->Margins[0] / (dev)->MarginsHWResolution[0])
#define dev_y_offset(dev) ((dev)->Margins[1] / (dev)->MarginsHWResolution[1])
#define dev_y_offset_points(dev) (dev_y_offset(dev) * 72.0)
/* Note that left/right/top/bottom are defined relative to */
/* the physical paper, not the coordinate system. */
/* For backward compatibility, we define macros that give */
/* the margins in inches. */
#define dev_l_margin(dev) ((dev)->HWMargins[0] / 72.0)
#define dev_b_margin(dev) ((dev)->HWMargins[1] / 72.0)
#define dev_b_margin_points(dev) ((dev)->HWMargins[1])
#define dev_r_margin(dev) ((dev)->HWMargins[2] / 72.0)
#define dev_t_margin(dev) ((dev)->HWMargins[3] / 72.0)
#define dev_t_margin_points(dev) ((dev)->HWMargins[3])
/* The extra () are to prevent premature expansion. */
#define open_init_closed() 0 /*false*/, 0	/* max_fill_band */
#define open_init_open() 1 /*true*/, 0	/* max_fill_band */
/* Accessors for device procedures */
#define dev_proc(dev, p) ((dev)->procs.p)
#define set_dev_proc(dev, p, proc) ((dev)->procs.p = (proc))
#define fill_dev_proc(dev, p, dproc)\
  if ( dev_proc(dev, p) == 0 ) set_dev_proc(dev, p, dproc)
#define assign_dev_procs(todev, fromdev)\
  ((todev)->procs = (fromdev)->procs)

/* ---------------- Device procedures ---------------- */

/* Define an opaque type for parameter lists. */
#ifndef gs_param_list_DEFINED
#  define gs_param_list_DEFINED
typedef struct gs_param_list_s gs_param_list;

#endif

/*
 * Definition of device procedures.
 * Note that the gx_device * argument is not declared const,
 * because many drivers maintain dynamic state in the device structure.
 * Note also that the structure is defined as a template, so that
 * we can instantiate it with device subclasses.
 * Because C doesn't have real templates, we must do this with macros.
 */

/* Define macros for declaring device procedures. */

#define dev_t_proc_open_device(proc, dev_t)\
  int proc(P1(dev_t *dev))
#define dev_proc_open_device(proc)\
  dev_t_proc_open_device(proc, gx_device)

#define dev_t_proc_get_initial_matrix(proc, dev_t)\
  void proc(P2(dev_t *dev, gs_matrix *pmat))
#define dev_proc_get_initial_matrix(proc)\
  dev_t_proc_get_initial_matrix(proc, gx_device)

#define dev_t_proc_sync_output(proc, dev_t)\
  int proc(P1(dev_t *dev))
#define dev_proc_sync_output(proc)\
  dev_t_proc_sync_output(proc, gx_device)

#define dev_t_proc_output_page(proc, dev_t)\
  int proc(P3(dev_t *dev, int num_copies, int flush))
#define dev_proc_output_page(proc)\
  dev_t_proc_output_page(proc, gx_device)

#define dev_t_proc_close_device(proc, dev_t)\
  int proc(P1(dev_t *dev))
#define dev_proc_close_device(proc)\
  dev_t_proc_close_device(proc, gx_device)

#define dev_t_proc_map_rgb_color(proc, dev_t)\
  gx_color_index proc(P4(dev_t *dev,\
    gx_color_value red, gx_color_value green, gx_color_value blue))
#define dev_proc_map_rgb_color(proc)\
  dev_t_proc_map_rgb_color(proc, gx_device)

#define dev_t_proc_map_color_rgb(proc, dev_t)\
  int proc(P3(dev_t *dev,\
    gx_color_index color, gx_color_value rgb[3]))
#define dev_proc_map_color_rgb(proc)\
  dev_t_proc_map_color_rgb(proc, gx_device)

#define dev_t_proc_fill_rectangle(proc, dev_t)\
  int proc(P6(dev_t *dev,\
    int x, int y, int width, int height, gx_color_index color))
#define dev_proc_fill_rectangle(proc)\
  dev_t_proc_fill_rectangle(proc, gx_device)

#define dev_t_proc_tile_rectangle(proc, dev_t)\
  int proc(P10(dev_t *dev,\
    const gx_tile_bitmap *tile, int x, int y, int width, int height,\
    gx_color_index color0, gx_color_index color1,\
    int phase_x, int phase_y))
#define dev_proc_tile_rectangle(proc)\
  dev_t_proc_tile_rectangle(proc, gx_device)

#define dev_t_proc_copy_mono(proc, dev_t)\
  int proc(P11(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height,\
    gx_color_index color0, gx_color_index color1))
#define dev_proc_copy_mono(proc)\
  dev_t_proc_copy_mono(proc, gx_device)

#define dev_t_proc_copy_color(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height))
#define dev_proc_copy_color(proc)\
  dev_t_proc_copy_color(proc, gx_device)

		/* OBSOLETED in release 3.66 */

#define dev_t_proc_draw_line(proc, dev_t)\
  int proc(P6(dev_t *dev,\
    int x0, int y0, int x1, int y1, gx_color_index color))
#define dev_proc_draw_line(proc)\
  dev_t_proc_draw_line(proc, gx_device)

		/* Added in release 2.4 */

#define dev_t_proc_get_bits(proc, dev_t)\
  int proc(P4(dev_t *dev,\
    int y, byte *data, byte **actual_data))
#define dev_proc_get_bits(proc)\
  dev_t_proc_get_bits(proc, gx_device)

		/* Added in release 2.4, changed in 2.8, */
		/* renamed in 2.9.6 */

#define dev_t_proc_get_params(proc, dev_t)\
  int proc(P2(dev_t *dev, gs_param_list *plist))
#define dev_proc_get_params(proc)\
  dev_t_proc_get_params(proc, gx_device)

#define dev_t_proc_put_params(proc, dev_t)\
  int proc(P2(dev_t *dev, gs_param_list *plist))
#define dev_proc_put_params(proc)\
  dev_t_proc_put_params(proc, gx_device)

		/* Added in release 2.6 */

#define dev_t_proc_map_cmyk_color(proc, dev_t)\
  gx_color_index proc(P5(dev_t *dev,\
    gx_color_value cyan, gx_color_value magenta, gx_color_value yellow,\
    gx_color_value black))
#define dev_proc_map_cmyk_color(proc)\
  dev_t_proc_map_cmyk_color(proc, gx_device)

#define dev_t_proc_get_xfont_procs(proc, dev_t)\
  const gx_xfont_procs *proc(P1(dev_t *dev))
#define dev_proc_get_xfont_procs(proc)\
  dev_t_proc_get_xfont_procs(proc, gx_device)

		/* Added in release 2.6.1 */

#define dev_t_proc_get_xfont_device(proc, dev_t)\
  gx_device *proc(P1(dev_t *dev))
#define dev_proc_get_xfont_device(proc)\
  dev_t_proc_get_xfont_device(proc, gx_device)

		/* Added in release 2.7.1 */

#define dev_t_proc_map_rgb_alpha_color(proc, dev_t)\
  gx_color_index proc(P5(dev_t *dev,\
    gx_color_value red, gx_color_value green, gx_color_value blue,\
    gx_color_value alpha))
#define dev_proc_map_rgb_alpha_color(proc)\
  dev_t_proc_map_rgb_alpha_color(proc, gx_device)

		/* Added in release 2.8.1 */

#define dev_t_proc_get_page_device(proc, dev_t)\
  gx_device *proc(P1(dev_t *dev))
#define dev_proc_get_page_device(proc)\
  dev_t_proc_get_page_device(proc, gx_device)

		/* Added in release 3.20 */

#define dev_t_proc_get_alpha_bits(proc, dev_t)\
  int proc(P2(dev_t *dev, graphics_object_type type))
#define dev_proc_get_alpha_bits(proc)\
  dev_t_proc_get_alpha_bits(proc, gx_device)

#define dev_t_proc_copy_alpha(proc, dev_t)\
  int proc(P11(dev_t *dev, const byte *data, int data_x,\
    int raster, gx_bitmap_id id, int x, int y, int width, int height,\
    gx_color_index color, int depth))
#define dev_proc_copy_alpha(proc)\
  dev_t_proc_copy_alpha(proc, gx_device)

		/* Added in release 3.38 */

#define dev_t_proc_get_band(proc, dev_t)\
  int proc(P3(dev_t *dev, int y, int *band_start))
#define dev_proc_get_band(proc)\
  dev_t_proc_get_band(proc, gx_device)

		/* Added in release 3.44 */

#define dev_t_proc_copy_rop(proc, dev_t)\
  int proc(P15(dev_t *dev,\
    const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,\
    const gx_color_index *scolors,\
    const gx_tile_bitmap *texture, const gx_color_index *tcolors,\
    int x, int y, int width, int height,\
    int phase_x, int phase_y, gs_logical_operation_t lop))
#define dev_proc_copy_rop(proc)\
  dev_t_proc_copy_rop(proc, gx_device)

		/* Added in release 3.60, changed in 3.68. */

#define dev_t_proc_fill_path(proc, dev_t)\
  int proc(P6(dev_t *dev,\
    const gs_imager_state *pis, gx_path *ppath,\
    const gx_fill_params *params,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath))
#define dev_proc_fill_path(proc)\
  dev_t_proc_fill_path(proc, gx_device)

#define dev_t_proc_stroke_path(proc, dev_t)\
  int proc(P6(dev_t *dev,\
    const gs_imager_state *pis, gx_path *ppath,\
    const gx_stroke_params *params,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath))
#define dev_proc_stroke_path(proc)\
  dev_t_proc_stroke_path(proc, gx_device)

		/* Added in release 3.60 */

#define dev_t_proc_fill_mask(proc, dev_t)\
  int proc(P13(dev_t *dev,\
    const byte *data, int data_x, int raster, gx_bitmap_id id,\
    int x, int y, int width, int height,\
    const gx_drawing_color *pdcolor, int depth,\
    gs_logical_operation_t lop, const gx_clip_path *pcpath))
#define dev_proc_fill_mask(proc)\
  dev_t_proc_fill_mask(proc, gx_device)

		/* Added in release 3.66, changed in 3.69 */

#define dev_t_proc_fill_trapezoid(proc, dev_t)\
  int proc(P8(dev_t *dev,\
    const gs_fixed_edge *left, const gs_fixed_edge *right,\
    fixed ybot, fixed ytop, bool swap_axes,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop))
#define dev_proc_fill_trapezoid(proc)\
  dev_t_proc_fill_trapezoid(proc, gx_device)

#define dev_t_proc_fill_parallelogram(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop))
#define dev_proc_fill_parallelogram(proc)\
  dev_t_proc_fill_parallelogram(proc, gx_device)

#define dev_t_proc_fill_triangle(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop))
#define dev_proc_fill_triangle(proc)\
  dev_t_proc_fill_triangle(proc, gx_device)

#define dev_t_proc_draw_thin_line(proc, dev_t)\
  int proc(P7(dev_t *dev,\
    fixed fx0, fixed fy0, fixed fx1, fixed fy1,\
    const gx_drawing_color *pdcolor, gs_logical_operation_t lop))
#define dev_proc_draw_thin_line(proc)\
  dev_t_proc_draw_thin_line(proc, gx_device)

		/* Added in release 3.66 (as stubs); */
		/* changed in 3.68; */
		/* begin_image and image_data changed in 4.30, */
		/* begin_image changed in 5.23. */

#define dev_t_proc_begin_image(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    const gs_imager_state *pis, const gs_image_t *pim,\
    gs_image_format_t format, const gs_int_rect *prect,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,\
    gs_memory_t *memory, gx_image_enum_common_t **pinfo))
#define dev_proc_begin_image(proc)\
  dev_t_proc_begin_image(proc, gx_device)

		/* OBSOLETED in release 5.23 */

#define dev_t_proc_image_data(proc, dev_t)\
  int proc(P6(dev_t *dev,\
    gx_image_enum_common_t *info, const byte **planes, int data_x,\
    uint raster, int height))
#define dev_proc_image_data(proc)\
  dev_t_proc_image_data(proc, gx_device)

		/* OBSOLETED in release 5.23 */

#define dev_t_proc_end_image(proc, dev_t)\
  int proc(P3(dev_t *dev,\
    gx_image_enum_common_t *info, bool draw_last))
#define dev_proc_end_image(proc)\
  dev_t_proc_end_image(proc, gx_device)

		/* Added in release 3.68 */

#define dev_t_proc_strip_tile_rectangle(proc, dev_t)\
  int proc(P10(dev_t *dev,\
    const gx_strip_bitmap *tiles, int x, int y, int width, int height,\
    gx_color_index color0, gx_color_index color1,\
    int phase_x, int phase_y))
#define dev_proc_strip_tile_rectangle(proc)\
  dev_t_proc_strip_tile_rectangle(proc, gx_device)

#define dev_t_proc_strip_copy_rop(proc, dev_t)\
  int proc(P15(dev_t *dev,\
    const byte *sdata, int sourcex, uint sraster, gx_bitmap_id id,\
    const gx_color_index *scolors,\
    const gx_strip_bitmap *textures, const gx_color_index *tcolors,\
    int x, int y, int width, int height,\
    int phase_x, int phase_y, gs_logical_operation_t lop))
#define dev_proc_strip_copy_rop(proc)\
  dev_t_proc_strip_copy_rop(proc, gx_device)

		/* Added in release 4.20 */

#define dev_t_proc_get_clipping_box(proc, dev_t)\
  void proc(P2(dev_t *dev, gs_fixed_rect *pbox))
#define dev_proc_get_clipping_box(proc)\
  dev_t_proc_get_clipping_box(proc, gx_device)

		/* Added in release 5.20, changed in 5.23 */

#define dev_t_proc_begin_typed_image(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    const gs_imager_state *pis, const gs_matrix *pmat,\
    const gs_image_common_t *pim, const gs_int_rect *prect,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,\
    gs_memory_t *memory, gx_image_enum_common_t **pinfo))
#define dev_proc_begin_typed_image(proc)\
  dev_t_proc_begin_typed_image(proc, gx_device)

		/* Added in release 5.20 */

#define dev_t_proc_get_bits_rectangle(proc, dev_t)\
  int proc(P4(dev_t *dev, const gs_int_rect *prect,\
    gs_get_bits_params_t *params, gs_int_rect **unread))
#define dev_proc_get_bits_rectangle(proc)\
  dev_t_proc_get_bits_rectangle(proc, gx_device)

#define dev_t_proc_map_color_rgb_alpha(proc, dev_t)\
  int proc(P3(dev_t *dev,\
    gx_color_index color, gx_color_value rgba[4]))
#define dev_proc_map_color_rgb_alpha(proc)\
  dev_t_proc_map_color_rgb_alpha(proc, gx_device)

#define dev_t_proc_create_compositor(proc, dev_t)\
  int proc(P5(dev_t *dev,\
    gx_device **pcdev, const gs_composite_t *pcte,\
    const gs_imager_state *pis, gs_memory_t *memory))
#define dev_proc_create_compositor(proc)\
  dev_t_proc_create_compositor(proc, gx_device)\

		/* Added in release 5.23 */

#define dev_t_proc_get_hardware_params(proc, dev_t)\
  int proc(P2(dev_t *dev, gs_param_list *plist))
#define dev_proc_get_hardware_params(proc)\
  dev_t_proc_get_hardware_params(proc, gx_device)

		/* Added in release 5.24 */

     /* ... text_begin ... see gxtext.h for definition */

/* Define the device procedure vector template proper. */

#define gx_device_proc_struct(dev_t)\
{	dev_t_proc_open_device((*open_device), dev_t);\
	dev_t_proc_get_initial_matrix((*get_initial_matrix), dev_t);\
	dev_t_proc_sync_output((*sync_output), dev_t);\
	dev_t_proc_output_page((*output_page), dev_t);\
	dev_t_proc_close_device((*close_device), dev_t);\
	dev_t_proc_map_rgb_color((*map_rgb_color), dev_t);\
	dev_t_proc_map_color_rgb((*map_color_rgb), dev_t);\
	dev_t_proc_fill_rectangle((*fill_rectangle), dev_t);\
	dev_t_proc_tile_rectangle((*tile_rectangle), dev_t);\
	dev_t_proc_copy_mono((*copy_mono), dev_t);\
	dev_t_proc_copy_color((*copy_color), dev_t);\
	dev_t_proc_draw_line((*obsolete_draw_line), dev_t);\
	dev_t_proc_get_bits((*get_bits), dev_t);\
	dev_t_proc_get_params((*get_params), dev_t);\
	dev_t_proc_put_params((*put_params), dev_t);\
	dev_t_proc_map_cmyk_color((*map_cmyk_color), dev_t);\
	dev_t_proc_get_xfont_procs((*get_xfont_procs), dev_t);\
	dev_t_proc_get_xfont_device((*get_xfont_device), dev_t);\
	dev_t_proc_map_rgb_alpha_color((*map_rgb_alpha_color), dev_t);\
	dev_t_proc_get_page_device((*get_page_device), dev_t);\
	dev_t_proc_get_alpha_bits((*get_alpha_bits), dev_t);\
	dev_t_proc_copy_alpha((*copy_alpha), dev_t);\
	dev_t_proc_get_band((*get_band), dev_t);\
	dev_t_proc_copy_rop((*copy_rop), dev_t);\
	dev_t_proc_fill_path((*fill_path), dev_t);\
	dev_t_proc_stroke_path((*stroke_path), dev_t);\
	dev_t_proc_fill_mask((*fill_mask), dev_t);\
	dev_t_proc_fill_trapezoid((*fill_trapezoid), dev_t);\
	dev_t_proc_fill_parallelogram((*fill_parallelogram), dev_t);\
	dev_t_proc_fill_triangle((*fill_triangle), dev_t);\
	dev_t_proc_draw_thin_line((*draw_thin_line), dev_t);\
	dev_t_proc_begin_image((*begin_image), dev_t);\
	dev_t_proc_image_data((*image_data), dev_t);\
	dev_t_proc_end_image((*end_image), dev_t);\
	dev_t_proc_strip_tile_rectangle((*strip_tile_rectangle), dev_t);\
	dev_t_proc_strip_copy_rop((*strip_copy_rop), dev_t);\
	dev_t_proc_get_clipping_box((*get_clipping_box), dev_t);\
	dev_t_proc_begin_typed_image((*begin_typed_image), dev_t);\
	dev_t_proc_get_bits_rectangle((*get_bits_rectangle), dev_t);\
	dev_t_proc_map_color_rgb_alpha((*map_color_rgb_alpha), dev_t);\
	dev_t_proc_create_compositor((*create_compositor), dev_t);\
	dev_t_proc_get_hardware_params((*get_hardware_params), dev_t);\
	dev_t_proc_text_begin((*text_begin), dev_t);\
}
/*
 * Provide procedures for passing image data.  image_data and end_image
 * are the equivalents of the obsolete driver procedures.  image_plane_data
 * was originally planned as a driver procedure, but is now associated with
 * the image enumerator, like the other two.
 */

typedef struct gx_image_plane_s {
    const byte *data;
    int data_x;
    uint raster;
} gx_image_plane_t;

#define image_enum_proc_plane_data(proc)\
  int proc(P4(gx_device *dev,\
    gx_image_enum_common_t *info, const gx_image_plane_t *planes,\
    int height))
#define gx_device_begin_image(dev, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo)\
  ((*dev_proc(dev, begin_image))\
   (dev, pis, pim, format, prect, pdcolor, pcpath, memory, pinfo))
#define gx_device_begin_typed_image(dev, pis, pmat, pim, prect, pdcolor, pcpath, memory, pinfo)\
  ((*dev_proc(dev, begin_typed_image))\
   (dev, pis, pmat, pim, prect, pdcolor, pcpath, memory, pinfo))

/*
 * The driver-like procedures gx_device_{image_data, image_plane_data,
 * end_image} are now DEPRECATED and will eventually be removed.
 * Their replacements no longer take an ignored dev argument.
 */
int gx_image_data(P5(gx_image_enum_common_t *info, const byte **planes,
		     int data_x, uint raster, int height));
int gx_image_plane_data(P3(gx_image_enum_common_t *info,
			   const gx_image_plane_t *planes, int height));
int gx_image_end(P2(gx_image_enum_common_t *info, bool draw_last));

#define gx_device_image_data(dev, info, planes, data_x, raster, height)\
  gx_image_data(info, planes, data_x, raster, height)
#define gx_device_image_plane_data(dev, info, planes, height)\
  gx_image_plane_data(info, planes, height)
#define gx_device_end_image(dev, info, draw_last)\
  gx_image_end(info, draw_last)

/* A generic device procedure record. */
struct gx_device_procs_s gx_device_proc_struct(gx_device);

/*
 * Define a procedure for setting up a memory device for buffering output
 * for a given device.  This is only used by band devices, but we define it
 * here for convenience.  The default implementation just calls
 * gs_make_mem_device.  Possibly this should be a generic device
 * procedure....
 */
#ifndef gx_device_memory_DEFINED
#  define gx_device_memory_DEFINED
typedef struct gx_device_memory_s gx_device_memory;

#endif
#define dev_proc_make_buffer_device(proc)\
  int proc(P4(gx_device_memory *, gx_device *, gs_memory_t *, bool))
dev_proc_make_buffer_device(gx_default_make_buffer_device);

/*
 * Define unaligned analogues of the copy_xxx procedures.
 * These are slower than the standard procedures, which require
 * aligned bitmaps, and also are not portable to non-byte-addressed machines.
 *
 * We allow both unaligned data and unaligned scan line widths;
 * however, we do require that both of these be aligned modulo the largest
 * power of 2 bytes that divides the data depth, i.e.:
 *      depth   alignment
 *      <= 8    1
 *      16      2
 *      24      1
 *      32      4
 */
dev_proc_copy_mono(gx_copy_mono_unaligned);
dev_proc_copy_color(gx_copy_color_unaligned);
dev_proc_copy_alpha(gx_copy_alpha_unaligned);

/* A generic device */
struct gx_device_s {
    gx_device_common;
};

extern_st(st_device);
struct_proc_finalize(gx_device_finalize);	/* public for subclasses */
/* We use vacuous enum/reloc procedures, rather than 0, so that */
/* gx_device can have subclasses. */
#define public_st_device()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_device, gx_device, "gx_device",\
    0, gs_no_struct_enum_ptrs, gs_no_struct_reloc_ptrs, gx_device_finalize)
#define st_device_max_ptrs 0

/* Enumerate or relocate a pointer to a device. */
/* These take the containing space into account properly. */
gx_device *gx_device_enum_ptr(P1(gx_device *));
gx_device *gx_device_reloc_ptr(P2(gx_device *, gc_state_t *));

/* Define typedefs for some of the device procedures, because */
/* ansi2knr can't handle dev_proc_xxx((*xxx)) in a formal argument list. */
typedef dev_proc_map_rgb_color((*dev_proc_map_rgb_color_t));
typedef dev_proc_map_color_rgb((*dev_proc_map_color_rgb_t));

/*
 * A forwarding device forwards all non-display operations, and possibly
 * some imaging operations (possibly transformed in some way), to another
 * device called the "target".  This is used for many different purposes
 * internally, including clipping, banding, image and pattern accumulation,
 * compositing, halftoning, and the null device.
 */
#define gx_device_forward_common\
	gx_device_common;\
	gx_device *target
/* A generic forwarding device. */
typedef struct gx_device_forward_s {
    gx_device_forward_common;
} gx_device_forward;

extern_st(st_device_forward);
#define public_st_device_forward()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_device_forward, gx_device_forward,\
    "gx_device_forward", 0, device_forward_enum_ptrs,\
    device_forward_reloc_ptrs, gx_device_finalize)
#define st_device_forward_max_ptrs (st_device_max_ptrs + 1)

/* A null device.  This is used to temporarily disable output. */
#ifndef gx_device_null_DEFINED
#  define gx_device_null_DEFINED
typedef struct gx_device_null_s gx_device_null;

#endif
struct gx_device_null_s {
    gx_device_forward_common;
};
extern const gx_device_null gs_null_device;

#define gx_device_is_null(dev)\
  ((dev)->dname == gs_null_device.dname)
extern_st(st_device_null);
#define public_st_device_null()	/* in gsdevice.c */\
  gs_public_st_complex_only(st_device_null, gx_device_null,\
    "gx_device_null", 0, device_forward_enum_ptrs,\
    device_forward_reloc_ptrs, gx_device_finalize)
#define st_device_null_max_ptrs st_device_forward_max_ptrs

/*
 * Initialize a just-allocated device from a prototype.
 * internal = true means initialize the reference count to 0,
 * false means initialize to 1.
 */
void gx_device_init(P4(gx_device * dev, const gx_device * proto,
		       gs_memory_t * mem, bool internal));

/* Make a null device. */
/* The gs_memory_t argument is 0 if the device is temporary and local, */
/* or the allocator that was used to allocate it if it is a real object. */
void gs_make_null_device(P2(gx_device_null *, gs_memory_t *));

/* Calculate the raster (number of bytes in a scan line), */
/* with byte or word padding. */
uint gx_device_raster(P2(const gx_device * dev, bool pad_to_word));

/* Adjust the resolution for devices that only have a fixed set of */
/* geometries, so that the apparent size in inches remains constant. */
/* If fit=1, the resolution is adjusted so that the entire image fits; */
/* if fit=0, one dimension fits, but the other one is clipped. */
int gx_device_adjust_resolution(P4(gx_device * dev, int actual_width, int actual_height, int fit));

/* Set the HWMargins to values defined in inches. */
/* If move_origin is true, also reset the Margins. */
void gx_device_set_margins(P3(gx_device * dev, const float *margins /*[4] */ ,
			      bool move_origin));

/* Set the width and height (in pixels), updating PageSize. */
void gx_device_set_width_height(P3(gx_device * dev, int width, int height));

/* Set the resolution (in pixels per inch), updating width and height. */
void gx_device_set_resolution(P3(gx_device * dev, floatp x_dpi, floatp y_dpi));

/* Set the PageSize (in 1/72" units), updating width and height. */
void gx_device_set_media_size(P3(gx_device * dev, floatp media_width, floatp media_height));

/****** BACKWARD COMPATIBILITY ******/
#define gx_device_set_page_size(dev, w, h)\
  gx_device_set_media_size(dev, w, h)

/*
 * Temporarily install a null device, or a special device such as
 * a clipping or cache device.
 */
void gx_set_device_only(P2(gs_state *, gx_device *));

/* Close a device. */
int gs_closedevice(P1(gx_device *));

/* ------ Device types (an unused concept right now) ------ */

#define dev_type_proc_initialize(proc)\
  int proc(P1(gx_device *))

typedef struct gx_device_type_s {
    gs_memory_type_ptr_t stype;
                         dev_type_proc_initialize((*initialize));
} gx_device_type;

#define device_type(dtname, stype, initproc)\
private dev_type_proc_initialize(initproc);\
const gx_device_type dtname = { &stype, initproc }

/*dev_type_proc_initialize(gdev_initialize); */

#endif /* gxdevcli_INCLUDED */
