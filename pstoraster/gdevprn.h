/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevprn.h */
/* Common header file for memory-buffered printers */

#ifndef gdevprn_INCLUDED
#  define gdevprn_INCLUDED

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"			/* for gxdevice.h */
#include "gsutil.h"			/* for memflip8x8 */
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxclist.h"
#include "gsparam.h"

/* Define the page size parameters. */
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

/*
 * Define the parameters for the printer rendering method.
 * If the entire bitmap fits in PRN_MAX_BITMAP, and there is at least
 * PRN_MIN_MEMORY_LEFT memory left after allocating it, render in RAM,
 * otherwise use a command list with a size of PRN_BUFFER_SPACE.
 * (These are parameters that can be changed by a client program.)
 */
/* Define parameters for machines with little dinky RAMs.... */
#define PRN_MAX_BITMAP_SMALL 32000
#define PRN_BUFFER_SPACE_SMALL 25000
#define PRN_MIN_MEMORY_LEFT_SMALL 32000
/* Define parameters for machines with great big hulking RAMs.... */
#define PRN_MAX_BITMAP_LARGE 10000000L
#define PRN_BUFFER_SPACE_LARGE 1000000L
#define PRN_MIN_MEMORY_LEFT_LARGE 500000L
/* Define parameters valid on all machines. */
#define PRN_MIN_BUFFER_SPACE 10000	/* give up if less than this */
/* Now define conditional parameters. */
#if arch_small_memory
#  define PRN_MAX_BITMAP PRN_MAX_BITMAP_SMALL
#  define PRN_BUFFER_SPACE PRN_BUFFER_SPACE_SMALL
#  define PRN_MIN_MEMORY_LEFT PRN_MIN_MEMORY_LEFT_SMALL
#else
/****** These should really be conditional on gs_debug_c('.') if
 ****** DEBUG is defined, but they're used in static initializers,
 ****** so we can't do it.
 ******/
#  if 0 /****** #  ifdef DEBUG ******/
#    define PRN_MAX_BITMAP\
       (gs_debug_c('.') ? PRN_MAX_BITMAP_SMALL : PRN_MAX_BITMAP_LARGE)
#    define PRN_BUFFER_SPACE\
       (gs_debug_c('.') ? PRN_BUFFER_SPACE_SMALL : PRN_BUFFER_SPACE_LARGE)
#    define PRN_MIN_MEMORY_LEFT\
       (gs_debug_c('.') ? PRN_MIN_MEMORY_LEFT_SMALL : PRN_MIN_MEMORY_LEFT_LARGE)
#  else
#    define PRN_MAX_BITMAP PRN_MAX_BITMAP_LARGE
#    define PRN_BUFFER_SPACE PRN_BUFFER_SPACE_LARGE
#    define PRN_MIN_MEMORY_LEFT PRN_MIN_MEMORY_LEFT_LARGE
#  endif
#endif

/* Define the abstract type for a printer device. */
typedef struct gx_device_printer_s gx_device_printer;

/*
 * Define the special procedures for band devices.
 */
typedef struct gx_printer_device_procs_s {

	/*
	 * Print the page on the output file.  Required only for devices
	 * where output_page is gdev_prn_output_page; ignored for other
	 * devices.
	 */

#define dev_proc_print_page(proc)\
  int proc(P2(gx_device_printer *, FILE *))
	dev_proc_print_page((*print_page));

	/* Print the page on the output file, with a given # of copies. */

#define dev_proc_print_page_copies(proc)\
  int proc(P3(gx_device_printer *, FILE *, int))
	dev_proc_print_page_copies((*print_page_copies));

	/* Initialize the memory device for a page or a band. */
	/* (The macro definition is in gxdevice.h.) */

	dev_proc_make_buffer_device((*make_buffer_device));

} gx_printer_device_procs;

/* ------ Printer device definition ------ */

/* Structure for generic printer devices. */
/* This must be preceded by gx_device_common. */
/* Printer devices are actually a union of a memory device */
/* and a clist device, plus some additional state. */
#define prn_fname_sizeof 80
#define gx_prn_device_common\
	byte skip[max(sizeof(gx_device_memory), sizeof(gx_device_clist)) -\
		  sizeof(gx_device) + sizeof(double) /* padding */];\
	gx_printer_device_procs printer_procs;\
		/* ------ The following items must be set before ------ */\
		/* ------ calling the device open routine. ------ */\
	long max_bitmap;		/* max size of non-buffered bitmap */\
	long use_buffer_space;		/* space to use for buffer */\
	char fname[prn_fname_sizeof];	/* output file name */\
		/* ------ End of preset items ------ */\
	int NumCopies;\
	  bool NumCopies_set;\
	bool OpenOutputFile;\
	bool Duplex;\
	  int Duplex_set;		/* -1 = not supported */\
	bool file_is_new;		/* true iff file just opened */\
	FILE *file;			/* output file */\
	char ccfname[60];		/* clist file name */\
	clist_file_ptr ccfile;		/* command list scratch file */\
	char cbfname[60];		/* clist block file name */\
	clist_file_ptr cbfile;		/* command list block scratch file */\
	long buffer_space;	/* amount of space for clist buffer, */\
					/* 0 means not using clist */\
	byte *buf;			/* buffer for rendering */\
	gx_device_procs orig_procs	/* original (std_)procs */

/* The device descriptor */
struct gx_device_printer_s {
	gx_device_common;
	gx_prn_device_common;
};

/* Macro for casting gx_device argument */
#define prn_dev ((gx_device_printer *)dev)

/* Define a typedef for the sake of ansi2knr. */
typedef dev_proc_print_page((*dev_proc_print_page_t));

/* Standard device procedures for printers */
dev_proc_open_device(gdev_prn_open);
dev_proc_output_page(gdev_prn_output_page);
dev_proc_close_device(gdev_prn_close);
#define gdev_prn_map_rgb_color gx_default_b_w_map_rgb_color
#define gdev_prn_map_color_rgb gx_default_b_w_map_color_rgb
dev_proc_get_params(gdev_prn_get_params);
dev_proc_put_params(gdev_prn_put_params);

/* Macro for generating procedure table */
#define prn_procs(p_open, p_output_page, p_close)\
  prn_color_procs(p_open, p_output_page, p_close, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb)
#define prn_params_procs(p_open, p_output_page, p_close, p_get_params, p_put_params)\
  prn_color_params_procs(p_open, p_output_page, p_close, gdev_prn_map_rgb_color, gdev_prn_map_color_rgb, p_get_params, p_put_params)
#define prn_color_procs(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb)\
  prn_color_params_procs(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, gdev_prn_get_params, gdev_prn_put_params)
/* See gdev_prn_open for explanation of the NULLs below. */
#define prn_color_params_procs(p_open, p_output_page, p_close, p_map_rgb_color, p_map_color_rgb, p_get_params, p_put_params) {\
	p_open,\
	NULL,	/* get_initial_matrix */\
	NULL,	/* sync_output */\
	p_output_page,\
	p_close,\
	p_map_rgb_color,\
	p_map_color_rgb,\
	NULL,	/* fill_rectangle */\
	NULL,	/* tile_rectangle */\
	NULL,	/* copy_mono */\
	NULL,	/* copy_color */\
	NULL,	/* draw_line */\
	NULL,	/* get_bits */\
	p_get_params,\
	p_put_params,\
	NULL,	/* map_cmyk_color */\
	NULL,	/* get_xfont_procs */\
	NULL,	/* get_xfont_device */\
	NULL,	/* map_rgb_alpha_color */\
	gx_page_device_get_page_device,\
	NULL,	/* get_alpha_bits */\
	NULL,	/* copy_alpha */\
	NULL,	/* get_band */\
	NULL,	/* copy_rop */\
	NULL,	/* fill_path */\
	NULL,	/* stroke_path */\
	NULL,	/* fill_mask */\
	NULL,	/* fill_trapezoid */\
	NULL,	/* fill_parallelogram */\
	NULL,	/* fill_triangle */\
	NULL,	/* draw_thin_line */\
	NULL,	/* begin_image */\
	NULL,	/* image_data */\
	NULL,	/* end_image */\
	NULL,	/* strip_tile_rectangle */\
	NULL	/* strip_copy_rop */\
}

/* The standard printer device procedures */
/* (using gdev_prn_open/output_page/close). */
extern gx_device_procs prn_std_procs;

/*
 * Define macros for generating the device structure,
 * analogous to the std_device_body macros in gxdevice.h
 * Note that the macros are broken up so as to be usable for devices that
 * add further initialized state to the printer device.
 *
 * The 'margin' values provided here specify the unimageable region
 * around the edges of the page (in inches), and the left and top margins
 * also specify the displacement of the device (0,0) point from the
 * upper left corner.  We should provide macros that allow specifying
 * all 6 values independently, but we don't yet.
 */
#define prn_device_body_rest_(print_page)\
	 { 0 },		/* std_procs */\
	 { 0 },		/* skip */\
	 { print_page,\
	   gx_default_print_page_copies,\
	   gx_default_make_buffer_device\
	 },\
	PRN_MAX_BITMAP,\
	PRN_BUFFER_SPACE,\
	 { 0 },		/* fname */\
	1, 0/*false*/,	/* NumCopies[_set] */\
	0/*false*/,	/* OpenOutputFile */\
	0/*false*/, -1,	/* Duplex[_set] */\
	0/*false*/, 0, { 0 }, 0, { 0 }, 0, 0, 0, { 0 }	/* ... orig_procs */

/* The Sun cc compiler won't allow \ within a macro argument list. */
/* This accounts for the short parameter names here and below. */
#define prn_device_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
	std_device_full_body(dtype, &procs, dname,\
	  (int)((long)w10 * xdpi / 10),\
	  (int)((long)h10 * ydpi / 10),\
	  xdpi, ydpi,\
	  ncomp, depth, mg, mc, dg, dc,\
	  -(lo) * (xdpi), -(to) * (ydpi),\
	  (lm) * 72.0, (bm) * 72.0,\
	  (rm) * 72.0, (tm) * 72.0\
	),\
	prn_device_body_rest_(print_page)

#define prn_device_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
  prn_device_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)

#define prn_device_body_copies(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_pages)\
	std_device_full_body(dtype, &procs, dname,\
	  (int)((long)w10 * xdpi / 10),\
	  (int)((long)h10 * ydpi / 10),\
	  xdpi, ydpi,\
	  ncomp, depth, mg, mc, dg, dc,\
	  -(lm) * (xdpi), -(tm) * (ydpi),\
	  (lm) * 72.0, (bm) * 72.0,\
	  (rm) * 72.0, (tm) * 72.0\
	),\
	 { 0 },		/* std_procs */\
	 { 0 },		/* skip */\
	 { NULL,\
	   print_pages,\
	   gx_default_make_buffer_device\
	 },\
	PRN_MAX_BITMAP,\
	PRN_BUFFER_SPACE,\
	 { 0 },		/* fname */\
	1, 0/*false*/,	/* NumCopies[_set] */\
	0/*false*/,	/* OpenOutputFile */\
	0/*false*/, -1,	/* Duplex[_set] */\
	0/*false*/, 0, { 0 }, 0, { 0 }, 0, 0, 0, { 0 }	/* ... orig_procs */

#define prn_device_std_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
	std_device_std_color_full_body(dtype, &procs, dname,\
	  (int)((long)w10 * xdpi / 10),\
	  (int)((long)h10 * ydpi / 10),\
	  xdpi, ydpi, color_bits,\
	  -(lo) * (xdpi), -(to) * (ydpi),\
	  (lm) * 72.0, (bm) * 72.0,\
	  (rm) * 72.0, (tm) * 72.0\
	),\
	prn_device_body_rest_(print_page)

#define prn_device_std_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, color_bits, print_page)\
  prn_device_std_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, color_bits, print_page)

#define prn_device_margins(procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
{ prn_device_std_margins_body(gx_device_printer, procs, dname,\
    w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
}

#define prn_device(procs, dname, w10, h10, xdpi, ydpi, lm, bm, rm, tm, color_bits, print_page)\
  prn_device_margins(procs, dname, w10, h10, xdpi, ydpi,\
    lm, tm, lm, bm, rm, tm, color_bits, print_page)\

/* ------ Utilities ------ */
/* These are defined in gdevprn.c. */

int gdev_prn_open_printer(P2(gx_device *dev, int binary_mode));
#define gdev_prn_file_is_new(pdev) ((pdev)->file_is_new)
#define gdev_prn_raster(pdev) gx_device_raster((gx_device *)(pdev), 0)
int gdev_prn_get_bits(P4(gx_device_printer *, int, byte *, byte **));
int gdev_prn_copy_scan_lines(P4(gx_device_printer *, int, byte *, uint));
int gdev_prn_close_printer(P1(gx_device *));

/* Define the InputAttributes and OutputAttributes of a device. */
/* The device get_params procedure would call these. */

typedef struct input_media_s {
	float PageSize[2];
	const char *MediaColor;
	float MediaWeight;
	const char *MediaType;
} input_media;
#define gdev_prn_begin_input_media(plist, pdict, count)\
  ((pdict)->size = (count),\
   param_begin_write_dict(plist, "InputAttributes", pdict, true))
int gdev_prn_input_page_size(P4(int index, gs_param_dict *pdict,
  floatp width_points, floatp height_points));
int gdev_prn_input_media(P3(int index, gs_param_dict *pdict,
  const input_media *pim));
#define gdev_prn_end_input_media(plist, pdict)\
  param_end_write_dict(plist, "InputAttributes", pdict)

typedef struct output_media_s {
	const char *OutputType;
} output_media;
#define gdev_prn_begin_output_media(plist, pdict, count)\
  ((pdict)->size = (count),\
   param_begin_write_dict(plist, "OutputAttributes", pdict, true))
int gdev_prn_output_media(P3(int index, gs_param_dict *pdict,
  const output_media *pom));
#define gdev_prn_end_output_media(plist, pdict)\
  param_end_write_dict(plist, "OutputAttributes", pdict)

/* The default print_page_copies procedure just calls print_page */
/* the given number of times. */
dev_proc_print_page_copies(gx_default_print_page_copies);

/* Define the number of scan lines that should actually be passed */
/* to the device. */
int gdev_prn_print_scan_lines(P1(gx_device *));

/* BACKWARD COMPATIBILITY */
#define dev_print_scan_lines(dev)\
  gdev_prn_print_scan_lines((gx_device *)(dev))
#define gdev_mem_bytes_per_scan_line(dev)\
  gdev_prn_raster((gx_device_printer *)(dev))
#define gdev_prn_transpose_8x8(inp,ils,outp,ols)\
  memflip8x8(inp,ils,outp,ols)

/* ------ Printer device types ------ */
/**************** THE FOLLOWING CODE IS NOT USED YET. ****************/

#if 0		/**************** VMS linker gets upset ****************/
extern_st(st_prn_device);
#endif
int	gdev_prn_initialize(P3(gx_device *, const char _ds *, dev_proc_print_page((*))));
void	gdev_prn_init_color(P4(gx_device *, int, dev_proc_map_rgb_color((*)), dev_proc_map_color_rgb((*))));

#define prn_device_type(dtname, initproc, pageproc)\
private dev_proc_print_page(pageproc);\
device_type(dtname, st_prn_device, initproc)

/****** FOLLOWING SHOULD CHECK __PROTOTYPES__ ******/
#define prn_device_type_mono(dtname, dname, initproc, pageproc)\
private dev_proc_print_page(pageproc);\
private int \
initproc(gx_device *dev)\
{	return gdev_prn_initialize(dev, dname, pageproc);\
}\
device_type(dtname, st_prn_device, initproc)

/****** DITTO ******/
#define prn_device_type_color(dtname, dname, depth, initproc, pageproc, rcproc, crproc)\
private dev_proc_print_page(pageproc);\
private int \
initproc(gx_device *dev)\
{	int code = gdev_prn_initialize(dev, dname, pageproc);\
	gdev_prn_init_color(dev, depth, rcproc, crproc);\
	return code;\
}\
device_type(dtname, st_prn_device, initproc)

#endif				/* gdevprn_INCLUDED */
