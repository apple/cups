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

/*$Id: gxdevice.h,v 1.2 2000/03/08 23:14:57 mike Exp $ */
/* Definitions for device implementors */

#ifndef gxdevice_INCLUDED
#  define gxdevice_INCLUDED

#include "stdio_.h"		/* for FILE */
#include "gxdevcli.h"
#include "gsparam.h"
/*
 * Drivers still use gs_malloc and gs_free, so include the interface for
 * these.  (Eventually they should go away.)
 */
#include "gsmalloc.h"

/* ---------------- Auxiliary types and structures ---------------- */

/* Define default pages sizes. */
/* U.S. letter paper (8.5" x 11"). */
#define DEFAULT_WIDTH_10THS_US_LETTER 85
#define DEFAULT_HEIGHT_10THS_US_LETTER 110
/* A4 paper (210mm x 297mm).  The dimensions are off by a few mm.... */
#define DEFAULT_WIDTH_10THS_A4 83
#define DEFAULT_HEIGHT_10THS_A4 117
/* Choose a default.  A4 may be set in the makefile. */
#ifdef A4
#  define DEFAULT_WIDTH_10THS DEFAULT_WIDTH_10THS_A4
#  define DEFAULT_HEIGHT_10THS DEFAULT_HEIGHT_10THS_A4
#else
#  define DEFAULT_WIDTH_10THS DEFAULT_WIDTH_10THS_US_LETTER
#  define DEFAULT_HEIGHT_10THS DEFAULT_HEIGHT_10THS_US_LETTER
#endif

/* ---------------- Device structure ---------------- */

/*
 * To insulate statically defined device templates from the
 * consequences of changes in the device structure, the following macros
 * must be used for generating initialized device structures.
 *
 * The computations of page width and height in pixels should really be
 *      ((int)(page_width_inches*x_dpi))
 * but some compilers (the Ultrix 3.X pcc compiler and the HPUX compiler)
 * can't cast a computed float to an int.  That's why we specify
 * the page width and height in inches/10 instead of inches.
 *
 * Note that the macro is broken up so as to be usable for devices that
 * add further initialized state to the generic device.
 * Note also that the macro does not initialize procs, which is
 * the next element of the structure.
 */
#define std_device_part1_(devtype, ptr_procs, dev_name, stype, open_init)\
	sizeof(devtype), ptr_procs, dev_name,\
	0 /*memory*/, stype, { 0 } /*rc*/,\
	open_init()		/*is_open, max_fill_band */
/* color_info goes here */
/*
 * The MetroWerks compiler has some bizarre bug that produces a spurious
 * error message if the width and/or height are defined as 0 below,
 * unless we use the +/- workaround in the next macro.
 */
#define std_device_part2_(width, height, x_dpi, y_dpi)\
	width, height,\
	{ (((width) * 72.0 + 0.5) - 0.5) / (x_dpi),\
	  (((height) * 72.0 + 0.5) - 0.5) / (y_dpi) },\
	{ 0, 0, 0, 0 }, 0/*false*/, { x_dpi, y_dpi }, { x_dpi, y_dpi }
/* offsets and margins go here */
#define std_device_part3_()\
	0, 0, 1, 0/*false*/, 0/*false*/,\
	{ gx_default_install, gx_default_begin_page, gx_default_end_page }
/*
 * We need a number of different variants of the std_device_ macro simply
 * because we can't pass the color_info or offsets/margins
 * as macro arguments, which in turn is because of the early macro
 * expansion issue noted in stdpre.h.  The basic variants are:
 *      ...body_with_macros_, which uses 0-argument macros to supply
 *        open_init, color_info, and offsets/margins;
 *      ...full_body, which takes 12 values (6 for dci_values,
 *        6 for offsets/margins);
 *      ...color_full_body, which takes 9 values (3 for dci_color,
 *        6 for margins/offset).
 *      ...std_color_full_body, which takes 7 values (1 for dci_std_color,
 *        6 for margins/offset).
 *      
 */
#define std_device_body_with_macros_(dtype, pprocs, dname, stype, w, h, xdpi, ydpi, open_init, dci_macro, margins_macro)\
	std_device_part1_(dtype, pprocs, dname, stype, open_init),\
	dci_macro(),\
	std_device_part2_(w, h, xdpi, ydpi),\
	margins_macro(),\
	std_device_part3_()

#define std_device_std_body(dtype, pprocs, dname, w, h, xdpi, ydpi)\
	std_device_body_with_macros_(dtype, pprocs, dname, 0,\
	  w, h, xdpi, ydpi,\
	  open_init_closed, dci_black_and_white_, no_margins_)

#define std_device_std_body_type_open(dtype, pprocs, dname, stype, w, h, xdpi, ydpi)\
	std_device_body_with_macros_(dtype, pprocs, dname, stype,\
	  w, h, xdpi, ydpi,\
	  open_init_open, dci_black_and_white_, no_margins_)

#define std_device_std_body_open(dtype, pprocs, dname, w, h, xdpi, ydpi)\
	std_device_std_body_type_open(dtype, pprocs, dname, 0, w, h, xdpi, ydpi)

#define std_device_full_body(dtype, pprocs, dname, w, h, xdpi, ydpi, ncomp, depth, mg, mc, dg, dc, xoff, yoff, lm, bm, rm, tm)\
	std_device_part1_(dtype, pprocs, dname, 0, open_init_closed),\
	dci_values(ncomp, depth, mg, mc, dg, dc),\
	std_device_part2_(w, h, xdpi, ydpi),\
	offset_margin_values(xoff, yoff, lm, bm, rm, tm),\
	std_device_part3_()

#define std_device_dci_type_body(dtype, pprocs, dname, stype, w, h, xdpi, ydpi, ncomp, depth, mg, mc, dg, dc)\
	std_device_part1_(dtype, pprocs, dname, stype, open_init_closed),\
	dci_values(ncomp, depth, mg, mc, dg, dc),\
	std_device_part2_(w, h, xdpi, ydpi),\
	offset_margin_values(0, 0, 0, 0, 0, 0),\
	std_device_part3_()

#define std_device_dci_body(dtype, pprocs, dname, w, h, xdpi, ydpi, ncomp, depth, mg, mc, dg, dc)\
	std_device_dci_type_body(dtype, pprocs, dname, 0,\
	  w, h, xdpi, ydpi, ncomp, depth, mg, mc, dg, dc)

#define std_device_color_full_body(dtype, pprocs, dname, w, h, xdpi, ydpi, depth, max_value, dither, xoff, yoff, lm, bm, rm, tm)\
	std_device_part1_(dtype, pprocs, dname, 0, open_init_closed),\
	dci_color(depth, max_value, dither),\
	std_device_part2_(w, h, xdpi, ydpi),\
	offset_margin_values(xoff, yoff, lm, bm, rm, tm),\
	std_device_part3_()

#define std_device_color_body(dtype, pprocs, dname, w, h, xdpi, ydpi, depth, max_value, dither)\
	std_device_color_full_body(dtype, pprocs, dname,\
	  w, h, xdpi, ydpi,\
	  depth, max_value, dither,\
	  0, 0, 0, 0, 0, 0)

#define std_device_color_stype_body(dtype, pprocs, dname, stype, w, h, xdpi, ydpi, depth, max_value, dither)\
	std_device_part1_(dtype, pprocs, dname, stype, open_init_closed),\
	dci_color(depth, max_value, dither),\
	std_device_part2_(w, h, xdpi, ydpi),\
	offset_margin_values(0, 0, 0, 0, 0, 0),\
	std_device_part3_()

#define std_device_std_color_full_body(dtype, pprocs, dname, w, h, xdpi, ydpi, depth, xoff, yoff, lm, bm, rm, tm)\
	std_device_part1_(dtype, pprocs, dname, 0, open_init_closed),\
	dci_std_color(depth),\
	std_device_part2_(w, h, xdpi, ydpi),\
	offset_margin_values(xoff, yoff, lm, bm, rm, tm),\
	std_device_part3_()

/* ---------------- Default implementations ---------------- */

/* Default implementations of optional procedures. */
/* Note that the default map_xxx_color routines assume white_on_black. */
dev_proc_open_device(gx_default_open_device);
dev_proc_get_initial_matrix(gx_default_get_initial_matrix);
dev_proc_get_initial_matrix(gx_upright_get_initial_matrix);
dev_proc_sync_output(gx_default_sync_output);
dev_proc_output_page(gx_default_output_page);
dev_proc_close_device(gx_default_close_device);
dev_proc_map_rgb_color(gx_default_w_b_map_rgb_color);
dev_proc_map_color_rgb(gx_default_w_b_map_color_rgb);
#define gx_default_map_rgb_color gx_default_w_b_map_rgb_color
#define gx_default_map_color_rgb gx_default_w_b_map_color_rgb
dev_proc_tile_rectangle(gx_default_tile_rectangle);
dev_proc_copy_mono(gx_default_copy_mono);
dev_proc_copy_color(gx_default_copy_color);
dev_proc_draw_line(gx_default_draw_line);
dev_proc_get_bits(gx_no_get_bits);	/* gives error */
dev_proc_get_bits(gx_default_get_bits);
dev_proc_get_params(gx_default_get_params);
dev_proc_put_params(gx_default_put_params);
dev_proc_map_cmyk_color(gx_default_map_cmyk_color);
dev_proc_get_xfont_procs(gx_default_get_xfont_procs);
dev_proc_get_xfont_device(gx_default_get_xfont_device);
dev_proc_map_rgb_alpha_color(gx_default_map_rgb_alpha_color);
dev_proc_get_page_device(gx_default_get_page_device);	/* returns NULL */
dev_proc_get_page_device(gx_page_device_get_page_device);	/* returns dev */
dev_proc_get_alpha_bits(gx_default_get_alpha_bits);
dev_proc_copy_alpha(gx_no_copy_alpha);	/* gives error */
dev_proc_copy_alpha(gx_default_copy_alpha);
dev_proc_get_band(gx_default_get_band);
dev_proc_copy_rop(gx_no_copy_rop);	/* gives error */
dev_proc_copy_rop(gx_default_copy_rop);
dev_proc_fill_path(gx_default_fill_path);
dev_proc_stroke_path(gx_default_stroke_path);
dev_proc_fill_mask(gx_default_fill_mask);
dev_proc_fill_trapezoid(gx_default_fill_trapezoid);
dev_proc_fill_parallelogram(gx_default_fill_parallelogram);
dev_proc_fill_triangle(gx_default_fill_triangle);
dev_proc_draw_thin_line(gx_default_draw_thin_line);
dev_proc_begin_image(gx_default_begin_image);
dev_proc_image_data(gx_default_image_data);
dev_proc_end_image(gx_default_end_image);
dev_proc_strip_tile_rectangle(gx_default_strip_tile_rectangle);
dev_proc_strip_copy_rop(gx_no_strip_copy_rop);	/* gives error */
dev_proc_strip_copy_rop(gx_default_strip_copy_rop);
dev_proc_get_clipping_box(gx_default_get_clipping_box);
dev_proc_get_clipping_box(gx_get_largest_clipping_box);
dev_proc_begin_typed_image(gx_default_begin_typed_image);
dev_proc_get_bits_rectangle(gx_no_get_bits_rectangle);	/* gives error */
dev_proc_get_bits_rectangle(gx_default_get_bits_rectangle);
dev_proc_map_color_rgb_alpha(gx_default_map_color_rgb_alpha);
dev_proc_create_compositor(gx_no_create_compositor);
/* default is for ordinary "leaf" devices, non_imaging is for */
/* devices that only care about coverage and not contents. */
dev_proc_create_compositor(gx_default_create_compositor);
dev_proc_create_compositor(gx_non_imaging_create_compositor);
dev_proc_get_hardware_params(gx_default_get_hardware_params);
dev_proc_text_begin(gx_default_text_begin);

/* Color mapping routines for black-on-white, gray scale, true RGB, */
/* and true CMYK color. */
dev_proc_map_rgb_color(gx_default_b_w_map_rgb_color);
dev_proc_map_color_rgb(gx_default_b_w_map_color_rgb);
dev_proc_map_rgb_color(gx_default_gray_map_rgb_color);
dev_proc_map_color_rgb(gx_default_gray_map_color_rgb);
dev_proc_map_rgb_color(gx_default_rgb_map_rgb_color);
dev_proc_map_color_rgb(gx_default_rgb_map_color_rgb);
dev_proc_map_cmyk_color(gx_default_cmyk_map_cmyk_color);

/* Default implementations for forwarding devices */
dev_proc_get_initial_matrix(gx_forward_get_initial_matrix);
dev_proc_sync_output(gx_forward_sync_output);
dev_proc_output_page(gx_forward_output_page);
dev_proc_map_rgb_color(gx_forward_map_rgb_color);
dev_proc_map_color_rgb(gx_forward_map_color_rgb);
dev_proc_fill_rectangle(gx_forward_fill_rectangle);
dev_proc_tile_rectangle(gx_forward_tile_rectangle);
dev_proc_copy_mono(gx_forward_copy_mono);
dev_proc_copy_color(gx_forward_copy_color);
dev_proc_get_bits(gx_forward_get_bits);
dev_proc_get_params(gx_forward_get_params);
dev_proc_put_params(gx_forward_put_params);
dev_proc_map_cmyk_color(gx_forward_map_cmyk_color);
dev_proc_get_xfont_procs(gx_forward_get_xfont_procs);
dev_proc_get_xfont_device(gx_forward_get_xfont_device);
dev_proc_map_rgb_alpha_color(gx_forward_map_rgb_alpha_color);
dev_proc_get_page_device(gx_forward_get_page_device);
dev_proc_get_alpha_bits(gx_forward_get_alpha_bits);
dev_proc_copy_alpha(gx_forward_copy_alpha);
dev_proc_get_band(gx_forward_get_band);
dev_proc_copy_rop(gx_forward_copy_rop);
dev_proc_fill_path(gx_forward_fill_path);
dev_proc_stroke_path(gx_forward_stroke_path);
dev_proc_fill_mask(gx_forward_fill_mask);
dev_proc_fill_trapezoid(gx_forward_fill_trapezoid);
dev_proc_fill_parallelogram(gx_forward_fill_parallelogram);
dev_proc_fill_triangle(gx_forward_fill_triangle);
dev_proc_draw_thin_line(gx_forward_draw_thin_line);
dev_proc_begin_image(gx_forward_begin_image);
#define gx_forward_image_data gx_default_image_data
#define gx_forward_end_image gx_default_end_image
dev_proc_strip_tile_rectangle(gx_forward_strip_tile_rectangle);
dev_proc_strip_copy_rop(gx_forward_strip_copy_rop);
dev_proc_get_clipping_box(gx_forward_get_clipping_box);
dev_proc_begin_typed_image(gx_forward_begin_typed_image);
dev_proc_get_bits_rectangle(gx_forward_get_bits_rectangle);
dev_proc_map_color_rgb_alpha(gx_forward_map_color_rgb_alpha);
/* There is no forward_create_compositor (see Drivers.htm). */
dev_proc_get_hardware_params(gx_forward_get_hardware_params);
dev_proc_text_begin(gx_forward_text_begin);

/* ---------------- Implementation utilities ---------------- */

/* Fill in the GC structure descriptor for a device. */
/* This is only called during initialization. */
void gx_device_make_struct_type(P2(gs_memory_struct_type_t *,
				   const gx_device *));

/* Convert the device procedures to the proper form (see above). */
void gx_device_set_procs(P1(gx_device *));

/* Fill in defaulted procedures in a device procedure record. */
void gx_device_fill_in_procs(P1(gx_device *));
void gx_device_forward_fill_in_procs(P1(gx_device_forward *));

/* Forward the color mapping procedures from a device to its target. */
void gx_device_forward_color_procs(P1(gx_device_forward *));

/*
 * Copy device parameters back from a target.  This copies all standard
 * parameters related to page size and resolution, plus color_info.
 */
void gx_device_copy_params(P2(gx_device *to, const gx_device *from));

/* Open the output file for a device. */
int gx_device_open_output_file(P5(const gx_device * dev, const char *fname,
				  bool binary, bool positionable,
				  FILE ** pfile));

/*
 * Determine whether a given device needs to halftone.  Eventually this
 * should take an imager state as an additional argument.
 */
#define gx_device_must_halftone(dev)\
  ((gx_device_has_color(dev) ? (dev)->color_info.max_color :\
    (dev)->color_info.max_gray) < 31)
#define gx_color_device_must_halftone(dev)\
  ((dev)->color_info.max_gray < 31)

/*
 * Device procedures that draw into rectangles need to clip the coordinates
 * to the rectangle ((0,0),(dev->width,dev->height)).  The following macros
 * do the clipping.  They assume that the arguments of the procedure are
 * named dev, x, y, w, and h, and may modify the arguments (other than dev).
 *
 * For procedures that fill a region, dev, x, y, w, and h are the only
 * relevant arguments.  For procedures that copy bitmaps, see below.
 *
 * The following group of macros for region-filling procedures clips
 * specific edges of the supplied rectangle, as indicated by the macro name.
 */
#define fit_fill_xy(dev, x, y, w, h)\
  BEGIN\
	if ( (x | y) < 0 ) {\
	  if ( x < 0 )\
	    w += x, x = 0;\
	  if ( y < 0 )\
	    h += y, y = 0;\
	}\
  END
#define fit_fill_y(dev, y, h)\
  BEGIN\
	if ( y < 0 )\
	  h += y, y = 0;\
  END
#define fit_fill_w(dev, x, w)\
  BEGIN\
	if ( w > dev->width - x )\
	  w = dev->width - x;\
  END
#define fit_fill_h(dev, y, h)\
  BEGIN\
	if ( h > dev->height - y )\
	  h = dev->height - y;\
  END
#define fit_fill_xywh(dev, x, y, w, h)\
  BEGIN\
	fit_fill_xy(dev, x, y, w, h);\
	fit_fill_w(dev, x, w);\
	fit_fill_h(dev, y, h);\
  END
/*
 * Clip all edges, and return from the procedure if the result is empty.
 */
#define fit_fill(dev, x, y, w, h)\
  BEGIN\
	fit_fill_xywh(dev, x, y, w, h);\
	if ( w <= 0 || h <= 0 )\
	  return 0;\
  END

/*
 * For driver procedures that copy bitmaps (e.g., copy_mono, copy_color),
 * clipping the destination region also may require adjusting the pointer to
 * the source data.  In addition to dev, x, y, w, and h, the clipping macros
 * for these procedures reference data, data_x, raster, and id; they may
 * modify the values of data, data_x, and id.
 *
 * Clip the edges indicated by the macro name.
 */
#define fit_copy_xyw(dev, data, data_x, raster, id, x, y, w, h)\
  BEGIN\
	if ( (x | y) < 0 ) {\
	  if ( x < 0 )\
	    w += x, data_x -= x, x = 0;\
	  if ( y < 0 )\
	    h += y, data -= y * raster, id = gx_no_bitmap_id, y = 0;\
	}\
	if ( w > dev->width - x )\
	  w = dev->width - x;\
  END
/*
 * Clip all edges, and return from the procedure if the result is empty.
 */
#define fit_copy(dev, data, data_x, raster, id, x, y, w, h)\
  BEGIN\
	fit_copy_xyw(dev, data, data_x, raster, id, x, y, w, h);\
	if ( h > dev->height - y )\
	  h = dev->height - y;\
	if ( w <= 0 || h <= 0 )\
	  return 0;\
  END

/* ---------------- Media parameters ---------------- */

/* Define the InputAttributes and OutputAttributes of a device. */
/* The device get_params procedure would call these. */

typedef struct gdev_input_media_s {
    float PageSize[4];		/* nota bene */
    const char *MediaColor;
    float MediaWeight;
    const char *MediaType;
} gdev_input_media_t;

#define gdev_input_media_default_values { 0, 0, 0, 0 }, 0, 0, 0
extern const gdev_input_media_t gdev_input_media_default;

void gdev_input_media_init(P1(gdev_input_media_t * pim));

int gdev_begin_input_media(P3(gs_param_list * mlist, gs_param_dict * pdict,
			      int count));

int gdev_write_input_page_size(P4(int index, gs_param_dict * pdict,
				floatp width_points, floatp height_points));

int gdev_write_input_media(P3(int index, gs_param_dict * pdict,
			      const gdev_input_media_t * pim));

int gdev_end_input_media(P2(gs_param_list * mlist, gs_param_dict * pdict));

typedef struct gdev_output_media_s {
    const char *OutputType;
} gdev_output_media_t;

#define gdev_output_media_default_values 0
extern const gdev_output_media_t gdev_output_media_default;

int gdev_begin_output_media(P3(gs_param_list * mlist, gs_param_dict * pdict,
			       int count));

int gdev_write_output_media(P3(int index, gs_param_dict * pdict,
			       const gdev_output_media_t * pom));

int gdev_end_output_media(P2(gs_param_list * mlist, gs_param_dict * pdict));

#endif /* gxdevice_INCLUDED */
