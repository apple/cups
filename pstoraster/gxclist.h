/* Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxclist.h,v 1.3 2001/03/16 20:42:06 mike Exp $ */
/* Command list definitions for Ghostscript. */
/* Requires gxdevice.h and gxdevmem.h */

#ifndef gxclist_INCLUDED
#  define gxclist_INCLUDED

#include "gscspace.h"
#include "gxband.h"
#include "gxbcache.h"
#include "gxclio.h"
#include "gxistate.h"

/*
 * A command list is essentially a compressed list of driver calls.
 * Command lists are used to record an image that must be rendered in bands
 * for high-resolution and/or limited-memory printers.
 *
 * Command lists work in two phases.  The first phase records driver calls,
 * sorting them according to the band(s) they affect.  The second phase
 * reads back the commands band-by-band to create the bitmap images.
 * When opened, a command list is in the recording state; it switches
 * automatically from recording to reading when its get_bits procedure
 * is called.  Currently, there is a hack to reopen the device after
 * each page is processed, in order to switch back to recording.
 */

/*
 * The command list contains both commands for particular bands (the vast
 * majority) and commands that apply to a range of bands.  In order to
 * synchronize the two, we maintain the following invariant for buffered
 * commands:
 *
 *      If there are any band-range commands in the buffer, they are the
 *      first commands in the buffer, before any specific-band commands.
 *
 * To maintain this invariant, whenever we are about to put an band-range
 * command in the buffer, we check to see if the buffer already has any
 * band-range commands in it, and if so, whether they are the last commands
 * in the buffer and are for the same range; if the answer to any of these
 * questions is negative, we flush the buffer.
 */

/* ---------------- Public structures ---------------- */

/*
 * Define a saved page object.  This consists of a snapshot of the device
 * structure, information about the page per se, and the num_copies
 * parameter of output_page.
 */
typedef struct gx_saved_page_s {
    gx_device device;
    char dname[8 + 1];		/* device name for checking */
    gx_band_page_info info;
    int num_copies;
} gx_saved_page;

/*
 * Define a saved page placed at a particular (X,Y) offset for rendering.
 */
typedef struct gx_placed_page_s {
    gx_saved_page *page;
    gs_int_point offset;
} gx_placed_page;
  
/*
 * Define a procedure to cause some bandlist memory to be freed up,
 * probably by rendering current bandlist contents.
 */
#define proc_free_up_bandlist_memory(proc)\
  int proc(P2(gx_device *dev, bool flush_current))

/* ---------------- Internal structures ---------------- */

/*
 * Currently, halftoning occurs during the first phase, producing calls
 * to tile_rectangle.  Both phases keep a cache of recently seen bitmaps
 * (halftone cells and characters), which allows writing only a short cache
 * index in the command list rather than the entire bitmap.
 *
 * We keep only a single cache for all bands, but since the second phase
 * reads the command lists for each band separately, we have to keep track
 * for each cache entry E and band B whether the definition of E has been
 * written into B's list.  We do this with a bit mask in each entry.
 *
 * Eventually, we will replace this entire arrangement with one in which
 * we pass the actual halftone screen (whitening order) to all bands
 * through the command list, and halftoning occurs on the second phase.
 * This not only will shrink the command list, but will allow us to apply
 * other rendering algorithms such as error diffusion in the second phase.
 */
typedef struct {
    ulong offset;		/* writing: offset from cdev->data, */
    /*   0 means unused */
    /* reading: offset from cdev->chunk.data */
} tile_hash;
typedef struct {
    gx_cached_bits_common;
    /* To save space, instead of storing rep_width and rep_height, */
    /* we store width / rep_width and height / rep_height. */
    byte x_reps, y_reps;
    ushort rep_shift;
    ushort index;		/* index in table (hash table when writing) */
    ushort num_bands;		/* # of 1-bits in the band mask */
    /* byte band_mask[]; */
#define ts_mask(pts) (byte *)((pts) + 1)
    /* byte bits[]; */
#define ts_bits(cldev,pts) (ts_mask(pts) + (cldev)->tile_band_mask_size)
} tile_slot;


/*
 * In order to keep the per-band state down to a reasonable size,
 * we store only a single set of the imager state parameters;
 * for each parameter, each band has a flag that says whether that band
 * 'knows' the current value of the parameters.
 */
extern const gs_imager_state clist_imager_state_initial;

/*
 * Define the main structure for holding command list state.
 * Unless otherwise noted, all elements are used in both the writing (first)
 * and reading (second) phase.
 */
typedef struct gx_clist_state_s gx_clist_state;

#define gx_device_clist_common_members\
	gx_device_forward_common;	/* (see gxdevice.h) */\
		/* Following must be set before writing or reading. */\
		/* See gx_device_clist_writer, below, for more that must be init'd */\
	/* gx_device *target; */	/* device for which commands */\
					/* are being buffered */\
	dev_proc_make_buffer_device((*make_buffer_device));\
	gs_memory_t *bandlist_memory;	/* allocator for in-memory bandlist files */\
	byte *data;			/* buffer area */\
	uint data_size;			/* size of buffer */\
	gx_band_params band_params;	/* band buffering parameters */\
	bool do_not_open_or_close_bandfiles;	/* if true, do not open/close bandfiles */\
		/* Following are used for both writing and reading. */\
	gx_bits_cache_chunk chunk;	/* the only chunk of bits */\
	gx_bits_cache bits;\
	uint tile_hash_mask;		/* size of tile hash table -1 */\
	uint tile_band_mask_size;	/* size of band mask preceding */\
					/* each tile in the cache */\
	tile_hash *tile_table;		/* table for tile cache: */\
					/* see tile_hash above */\
					/* (a hash table when writing) */\
	int ymin, ymax;			/* current band, <0 when writing */\
		/* Following are set when writing, read when reading. */\
	gx_band_page_info page_info;	/* page information */\
	int nbands		/* # of bands */

typedef struct gx_device_clist_common_s {
    gx_device_clist_common_members;
} gx_device_clist_common;

#define clist_band_height(cldev) ((cldev)->page_info.band_height)
#define clist_cfname(cldev) ((cldev)->page_info.cfname)
#define clist_cfile(cldev) ((cldev)->page_info.cfile)
#define clist_bfname(cldev) ((cldev)->page_info.bfname)
#define clist_bfile(cldev) ((cldev)->page_info.bfile)

/* Define the length of the longest dash pattern we are willing to store. */
/* (Strokes with longer patterns are converted to fills.) */
#define cmd_max_dash 11

/* Define the state of a band list when writing. */
typedef struct gx_device_clist_writer_s {
    gx_device_clist_common_members;	/* (must be first) */
    int error_code;		/* error returned by cmd_put_op */
    gx_clist_state *states;	/* current state of each band */
    byte *cbuf;			/* start of command buffer */
    byte *cnext;		/* next slot in command buffer */
    byte *cend;			/* end of command buffer */
    cmd_list *ccl;	/* &clist_state.list of last command */
    cmd_list band_range_list;	/* list of band-range commands */
    int band_range_min, band_range_max;	/* range for list */
    uint tile_max_size;		/* max size of a single tile (bytes) */
    uint tile_max_count;	/* max # of hash table entries */
    gx_strip_bitmap tile_params;	/* current tile parameters */
    int tile_depth;		/* current tile depth */
    int tile_known_min, tile_known_max;
    /* range of bands that knows the */
    /* current tile parameters */
    gs_imager_state imager_state;	/* current values of imager params */
    float dash_pattern[cmd_max_dash];	/* current dash pattern */
    const gx_clip_path *clip_path;	/* current clip path */
    gs_id clip_path_id;		/* id of current clip path */
    byte color_space;		/* current color space identifier */
    /* (only used for images) */
    gs_indexed_params indexed_params;	/* current indexed space parameters */
    /* (ditto) */
    gs_id transfer_ids[4];	/* ids of transfer maps */
    gs_id black_generation_id;	/* id of black generation map */
    gs_id undercolor_removal_id;	/* id of u.c.r. map */
    gs_id device_halftone_id;	/* id of device halftone */
    gs_id image_enum_id;	/* non-0 if we are inside an image */
				/* that we are passing through */
	int error_is_retryable;		/* Extra status used to distinguish hard VMerrors */
	                           /* from warnings upgraded to VMerrors. */
	                           /* T if err ret'd by cmd_put_op et al can be retried */
	int permanent_error;		/* if < 0, error only cleared by clist_reset() */
	int driver_call_nesting;	/* nesting level of non-retryable driver calls */
	int ignore_lo_mem_warnings;	/* ignore warnings from clist file/mem */
		/* Following must be set before writing */
	proc_free_up_bandlist_memory((*free_up_bandlist_memory)); /* if nz, proc to free some bandlist memory */
	int disable_mask;		/* mask of routines to disable clist_disable_xxx */
} gx_device_clist_writer;

/* Bits for gx_device_clist_writer.disable_mask. Bit set disables behavior */
#define clist_disable_fill_path	(1 << 0)
#define clist_disable_stroke_path (1 << 1)
#define clist_disable_hl_image (1 << 2)
#define clist_disable_complex_clip (1 << 3)
#define clist_disable_nonrect_hl_image (1 << 4)
#define clist_disable_pass_thru_params (1 << 5)	/* disable EXCEPT at top of page */

/* Define the state of a band list when reading. */
/* For normal rasterizing, pages and num_pages are both 0. */
typedef struct gx_device_clist_reader_s {
    gx_device_clist_common_members;	/* (must be first) */
    const gx_placed_page *pages;
    int num_pages;
} gx_device_clist_reader;

typedef union gx_device_clist_s {
    gx_device_clist_common common;
    gx_device_clist_reader reader;
    gx_device_clist_writer writer;
} gx_device_clist;

/* setup before opening clist device */
#define clist_init_params(xclist, xdata, xdata_size, xtarget, xmake_buffer, xband_params, xexternal, xmemory, xfree_bandlist, xdisable)\
	(xclist)->common.data = (xdata);\
	(xclist)->common.data_size = (xdata_size);\
	(xclist)->common.target = (xtarget);\
	(xclist)->common.make_buffer_device = (xmake_buffer);\
	(xclist)->common.band_params = (xband_params);\
	(xclist)->common.do_not_open_or_close_bandfiles = (xexternal);\
	(xclist)->common.bandlist_memory = (xmemory);\
	(xclist)->writer.free_up_bandlist_memory = (xfree_bandlist);\
	(xclist)->writer.disable_mask = (xdisable)

/* Determine whether this clist device is able to recover VMerrors */
#define clist_test_VMerror_recoverable(cldev)\
  ((cldev)->free_up_bandlist_memory != 0)

/* The device template itself is never used, only the procedures. */
extern const gx_device_procs gs_clist_device_procs;

/* Reset (or prepare to append to) the command list after printing a page. */
int clist_finish_page(P2(gx_device * dev, bool flush));

/* Force bandfiles closed */
int clist_close_output_file(P1(gx_device *dev));

/* Define the abstract type for a printer device. */
#ifndef gx_device_printer_DEFINED
#  define gx_device_printer_DEFINED
typedef struct gx_device_printer_s gx_device_printer;
#endif

/* Do device setup from params passed in the command list. */
int clist_setup_params(P1(gx_device *dev));

/* Do more rendering to a client-supplied memory image, return results */
int clist_get_overlay_bits(P4(gx_device_printer *pdev, int y, int line_count,
			      byte *data));

/* Find out where the band buffer for a given line is going to fall on the */
/* next call to get_bits. */
int clist_locate_overlay_buffer(P3(gx_device_printer *pdev, int y,
				   byte **pdata));

#endif /* gxclist_INCLUDED */
