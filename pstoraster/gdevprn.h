/* Copyright (C) 1989, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevprn.h,v 1.5 2001/03/16 20:42:06 mike Exp $ */
/* Common header file for memory-buffered printers */

#ifndef gdevprn_INCLUDED
#  define gdevprn_INCLUDED

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"			/* for gp_file_name_sizeof */
#include "gserrors.h"
#include "gsmatrix.h"		/* for gxdevice.h */
#include "gsutil.h"		/* for memflip8x8 */
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxcldev.h"
#include "gsparam.h"

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
#  if 0				/****** #  ifdef DEBUG ***** */
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
#ifndef gx_device_printer_DEFINED
#  define gx_device_printer_DEFINED
typedef struct gx_device_printer_s gx_device_printer;
#endif

/* Define the abstract type for some band device procedures' arguments. */
typedef struct gdev_prn_start_render_params_s gdev_prn_start_render_params;

/* Define the abstract type for a page queue for async rendering. */
#ifndef gx_page_queue_DEFINED
#  define gx_page_queue_DEFINED
typedef struct gx_page_queue_s gx_page_queue;
#endif

/* Define the abstract type for parameters describing buffer space. */
#ifndef gdev_prn_space_params_DEFINED
#  define gdev_prn_space_params_DEFINED
typedef struct gdev_prn_space_params_s gdev_prn_space_params;
#endif

/*
 * Define the special procedures for band devices.
 */
typedef struct gx_printer_device_procs_s {

    /*
     * Print the page on the output file.  Required only for devices
     * where output_page is gdev_prn_output_page; ignored for other
     * devices.
     */

#define prn_dev_proc_print_page(proc)\
  int proc(P2(gx_device_printer *, FILE *))
    prn_dev_proc_print_page((*print_page));
/* BACKWARD COMPATIBILITY */
#define dev_proc_print_page(proc) prn_dev_proc_print_page(proc)

    /* Print the page on the output file, with a given # of copies. */

#define prn_dev_proc_print_page_copies(proc)\
  int proc(P3(gx_device_printer *, FILE *, int))
    prn_dev_proc_print_page_copies((*print_page_copies));
/* BACKWARD COMPATIBILITY */
#define dev_proc_print_page_copies(proc) prn_dev_proc_print_page_copies(proc)

    /* Initialize the memory device for a page or a band. */
    /* (The macro definition is in gxdevcli.h.) */

    dev_proc_make_buffer_device((*make_buffer_device));

    /*
     * Compute effective space params. These results effectively override
     * the space_params in the device, but does not replace them; that is to
     * say that computed space params are temps used for computation.
     * Procedure must fill in only those space_params that it wishes to
     * override, using curr width, height, margins, etc.
     *
     * Caller is gdevprn.open & gdevprn.put_params, calls driver or
     * default.
     */

#define prn_dev_proc_get_space_params(proc)\
  void proc(P2(const gx_device_printer *, gdev_prn_space_params *))
    prn_dev_proc_get_space_params((*get_space_params));

    /*
     * Only for gx_device_printer devices that overlap interpreting and
     * rasterizing. Since there are 2 instances of the device (1 for writing
     * the cmd list & 1 for rasterizing it), and each device is associated
     * with an different thread, this function is called to start the
     * rasterizer's thread. Once started, the rasterizer thread must call
     * down to gdev_prn_asnyc_render_thread, which will only return after
     * device closes.
     *
     * Caller is gdevprna.open, calls driver implementation or default.
     */

#define prn_dev_proc_start_render_thread(proc)\
  int proc(P1(gdev_prn_start_render_params *))
    prn_dev_proc_start_render_thread((*start_render_thread));

    /*
     * Only for gx_device_printer devices that overlap interpreting and
     * rasterizing. Since there are 2 instances of the device (1 for writing
     * the cmd list & 1 for rasterizing it), these fns are called to
     * open/close the rasterizer's instance, once the writer's instance has
     * been created & init'd. These procs must cascade down to
     * gdev_prn_async_render_open/close.
     *
     * Caller is gdevprna, calls driver implementation or default.
     */

#define prn_dev_proc_open_render_device(proc)\
  int proc(P1(gx_device_printer *))
    prn_dev_proc_open_render_device((*open_render_device));

#define prn_dev_proc_close_render_device(proc)\
  int proc(P1(gx_device_printer *))
    prn_dev_proc_close_render_device((*close_render_device));

    /*
     * Buffer a page on the output device. A page may or may not have been
     * fully rendered, but the rasterizer needs to realize the page to free
     * up resources or support copypage. Printing a page may involve zero or
     * more buffer_pages. All buffer_page output is overlaid in the buffer
     * until a terminating print_page or print_page_copies clears the
     * buffer. Note that, after the first buffer_page, the driver must use
     * the get_overlay_bits function instead of get_bits. The difference is
     * that get_overlay_bits requires the caller to supply the same buffered
     * bitmap that was computed as a result of a previous buffer_page, so
     * that get_overlay_bits can add further marks to the existing buffered
     * image. NB that output must be accumulated in buffer even if
     * num_copies == 0.
     *
     * Caller is expected to be gdevprn, calls driver implementation or
     * default.
     */

#define prn_dev_proc_buffer_page(proc)\
  int proc(P3(gx_device_printer *, FILE *, int))
    prn_dev_proc_buffer_page((*buffer_page));

    /*
     * Transform a given set of bits by marking it per the current page
     * description. This is a different version of get_bits, where this
     * procedure accepts a bitmap and merely adds further marks, without
     * clearing the bits.
     *
     * Driver implementation is expected to be the caller.
     */

#define prn_dev_proc_get_overlay_bits(proc)\
  int proc(P4(gx_device_printer *, int, int, byte *))
    prn_dev_proc_get_overlay_bits((*get_overlay_bits));

    /*
     * Find out where the band buffer for a given line is going to fall on
     * the next call to get_bits. This is an alternative to get_overlay_bits
     * in cases where the client doesn't own a suitably formatted buffer to
     * deposit bits into. When using this function, do a
     * locate_overlay_buffer, copy the background data into the returned
     * buffer, then do get_bits to get the transformed data.  IMPORTANT: the
     * locate_overlay_buffer for a specific range of lines must immediately
     * be followed by one or more get_bits for the same line range with no
     * other intervening driver calls. If this condition is violated,
     * results are undefined.
     */

#define prn_dev_proc_locate_overlay_buffer(proc)\
  int proc(P3(gx_device_printer *, int, byte **))
    prn_dev_proc_locate_overlay_buffer((*locate_overlay_buffer));

} gx_printer_device_procs;

/* ------ Printer device definition ------ */

/* Structure for generic printer devices. */
/* This must be preceded by gx_device_common. */
/* Printer devices are actually a union of a memory device */
/* and a clist device, plus some additional state. */
#define prn_fname_sizeof gp_file_name_sizeof
typedef enum {
    BandingAuto = 0,
    BandingAlways,
    BandingNever
} gdev_prn_banding_type;
struct gdev_prn_space_params_s {
    long MaxBitmap;		/* max size of non-buffered bitmap */
    long BufferSpace;		/* space to use for buffer */
    gx_band_params band;	/* see gxclist.h */
    bool params_are_read_only;	/* true if put_params may not modify this struct */
    gdev_prn_banding_type banding_type;	/* used to force banding or bitmap */
};

#define gx_prn_device_common\
	byte skip[max(sizeof(gx_device_memory), sizeof(gx_device_clist)) -\
		  sizeof(gx_device) + sizeof(double) /* padding */];\
	gx_printer_device_procs printer_procs;\
		/* ------ Device parameters that must be set ------ */\
		/* ------ before calling the device open routine. ------ */\
	gdev_prn_space_params space_params;\
	char fname[prn_fname_sizeof];	/* OutputFile */\
		/* ------ Other device parameters ------ */\
	bool OpenOutputFile;\
	bool ReopenPerPage;\
	bool Duplex;\
	  int Duplex_set;		/* -1 = not supported */\
		/* ------ End of parameters ------ */\
	bool file_is_new;		/* true iff file just opened */\
	FILE *file;			/* output file */\
	long buffer_space;	/* amount of space for clist buffer, */\
					/* 0 means not using clist */\
	byte *buf;			/* buffer for rendering */\
		/* ---- Begin async rendering support --- */\
	gs_memory_t *buffer_memory;	/* allocator for command list */\
	gs_memory_t *bandlist_memory;	/* allocator for bandlist files */\
	proc_free_up_bandlist_memory((*free_up_bandlist_memory));  	/* if nz, proc to free some bandlist memory */\
	gx_page_queue *page_queue;	/* if <> 0,page queue for gdevprna NOT GC'd */\
	bool is_async_renderer;		/* device is only the rendering part of async device */\
	gx_device_printer *async_renderer;	/* in async writer, pointer to async renderer */\
	uint clist_disable_mask;	/* mask of clist options to disable */\
		/* ---- End async rendering support --- */\
	gx_device_procs orig_procs	/* original (std_)procs */

/* The device descriptor */
struct gx_device_printer_s {
    gx_device_common;
    gx_prn_device_common;
};

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

/* Default printer-specific procedures */
prn_dev_proc_get_space_params(gdev_prn_default_get_space_params);
prn_dev_proc_start_render_thread(gx_default_start_render_thread); /* for async rendering only, see gdevprna.c */
prn_dev_proc_open_render_device(gx_default_open_render_device);
prn_dev_proc_close_render_device(gx_default_close_render_device);
prn_dev_proc_buffer_page(gx_default_buffer_page); /* returns an error */
prn_dev_proc_get_overlay_bits(gdev_prn_get_overlay_bits);
prn_dev_proc_locate_overlay_buffer(gdev_prn_locate_overlay_buffer);

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
	NULL,	/* strip_copy_rop, */\
	NULL,	/* get_clipping_box */\
	NULL,	/* begin_typed_image */\
	NULL,	/* map_color_rgb_alpha */\
	NULL,	/* create_compositor */\
	NULL,	/* get_hardware_params */\
	NULL	/* text_begin */\
}

/* The standard printer device procedures */
/* (using gdev_prn_open/output_page/close). */
extern const gx_device_procs prn_std_procs;

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
	   gx_default_make_buffer_device,\
	   gdev_prn_default_get_space_params,\
	   gx_default_start_render_thread,\
	   gx_default_open_render_device,\
	   gx_default_close_render_device,\
	   gx_default_buffer_page,\
	   gdev_prn_get_overlay_bits,\
	   gdev_prn_locate_overlay_buffer\
	 },\
	 { PRN_MAX_BITMAP, PRN_BUFFER_SPACE,\
	     { band_params_initial_values },\
	   0/*false*/,	/* params_are_read_only */\
	   BandingAuto	/* banding_type */\
	 },\
	 { 0 },		/* fname */\
	0/*false*/,	/* OpenOutputFile */\
	0/*false*/,	/* ReopenPerPage */\
	0/*false*/, -1,	/* Duplex[_set] */\
	0/*false*/, 0, 0, 0, /* file_is_new ... buf */\
	0, 0, 0, 0, 0/*false*/, 0, 0, /* buffer_memory ... clist_dis'_mask */\
	{ 0 }	/* ... orig_procs */

/* The Sun cc compiler won't allow \ within a macro argument list. */
/* This accounts for the short parameter names here and below. */
#define prn_device_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, ncomp, depth, mg, mc, dg, dc, print_page)\
	std_device_full_body(dtype, &procs, dname,\
	  (int)((long)(w10) * (xdpi) / 10),\
	  (int)((long)(h10) * (ydpi) / 10),\
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
	   gx_default_make_buffer_device,\
	   gdev_prn_default_get_space_params,\
	   gx_default_start_render_thread,\
	   gx_default_open_render_device,\
	   gx_default_close_render_device,\
	   gx_default_buffer_page,\
	   gdev_prn_get_overlay_bits,\
	   gdev_prn_locate_overlay_buffer\
	 },\
	 { PRN_MAX_BITMAP, PRN_BUFFER_SPACE,\
	     { band_params_initial_values },\
	   0/*false*/,	/* params_are_read_only */\
	   BandingAuto	/* banding_type */\
	 },\
	 { 0 },		/* fname */\
	0/*false*/,	/* OpenOutputFile */\
	0/*false*/,	/* ReopenPerPage */\
	0/*false*/, -1,	/* Duplex[_set] */\
	0/*false*/, 0, 0, 0, /* file_is_new ... buf */\
	0, 0, 0, 0, 0/*false*/, 0, 0, /* buffer_memory ... clist_dis'_mask */\
	{ 0 }	/* ... orig_procs */
#define prn_device_std_margins_body(dtype, procs, dname, w10, h10, xdpi, ydpi, lo, to, lm, bm, rm, tm, color_bits, print_page)\
	std_device_std_color_full_body(dtype, &procs, dname,\
	  (int)((long)(w10) * (xdpi) / 10),\
	  (int)((long)(h10) * (ydpi) / 10),\
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

int gdev_prn_open_printer_positionable(P3(gx_device *dev, bool binary_mode,
					  bool positionable));
/* open_printer defaults positionable = false */
int gdev_prn_open_printer(P2(gx_device * dev, bool binary_mode));
#define gdev_prn_file_is_new(pdev) ((pdev)->file_is_new)
#define gdev_prn_raster(pdev) gx_device_raster((gx_device *)(pdev), 0)
int gdev_prn_get_bits(P4(gx_device_printer *, int, byte *, byte **));
int gdev_prn_copy_scan_lines(P4(gx_device_printer *, int, byte *, uint));
int gdev_prn_close_printer(P1(gx_device *));

/* The default print_page_copies procedure just calls print_page */
/* the given number of times. */
prn_dev_proc_print_page_copies(gx_default_print_page_copies);

/* Define the number of scan lines that should actually be passed */
/* to the device. */
int gdev_prn_print_scan_lines(P1(gx_device *));

/* Allocate / reallocate / free printer memory. */
int gdev_prn_allocate_memory(P4(gx_device *pdev,
				gdev_prn_space_params *space,
				int new_width, int new_height));
int gdev_prn_reallocate_memory(P4(gx_device *pdev,
				  gdev_prn_space_params *space,
				  int new_width, int new_height));
int gdev_prn_free_memory(P1(gx_device *pdev));

/* BACKWARD COMPATIBILITY */
#define dev_print_scan_lines(dev)\
  gdev_prn_print_scan_lines((gx_device *)(dev))
#define gdev_mem_bytes_per_scan_line(dev)\
  gdev_prn_raster((gx_device_printer *)(dev))
#define gdev_prn_transpose_8x8(inp,ils,outp,ols)\
  memflip8x8(inp,ils,outp,ols)

/* ------ Printer device types ------ */
/**************** THE FOLLOWING CODE IS NOT USED YET. ****************/

#if 0				/**************** VMS linker gets upset *************** */
extern_st(st_prn_device);
#endif
int gdev_prn_initialize(P3(gx_device *, const char *, dev_proc_print_page((*))));
void gdev_prn_init_color(P4(gx_device *, int, dev_proc_map_rgb_color((*)), dev_proc_map_color_rgb((*))));

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

#endif /* gdevprn_INCLUDED */
