/* Copyright (C) 1991, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxclist.h */
/* Command list definitions for Ghostscript. */
/* Requires gxdevice.h and gxdevmem.h */
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
 * majority) and commands that apply to all bands.  In order to synchronize
 * the two, we maintain the following invariant for buffered commands:
 *
 *	If there are any all-band commands in the buffer, they are the
 *	first commands in the buffer, before any specific-band commands.
 *
 * To maintain this invariant, whenever we are about to put an all-band
 * command in the buffer, we check to see if the buffer already has any
 * all-band commands in it, and if so, whether they are the last commands
 * in the buffer; if the answer to either question is negative, we flush
 * the buffer.
 */

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
	ushort index;		/* index in table (hash table when writing) */
	ushort num_bands;	/* # of 1-bits in the band mask */
	/* byte band_mask[]; */
#define ts_mask(pts) (byte *)((pts) + 1)
	/* byte bits[]; */
#define ts_bits(cldev,pts) (ts_mask(pts) + (cldev)->tile_band_mask_size)
} tile_slot;

/* Define the prefix on each command run in the writing buffer. */
typedef struct cmd_prefix_s cmd_prefix;
struct cmd_prefix_s {
	cmd_prefix *next;
	uint size;
};

/* Define the pointers for managing a list of command runs in the buffer. */
/* There is one of these for each band, plus one for all-band commands. */
typedef struct cmd_list_s {
	cmd_prefix *head, *tail;	/* list of commands for band */
} cmd_list;

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
#define gx_device_clist_common\
	gx_device_forward_common;	/* (see gxdevice.h) */\
		/* Following must be set before writing or reading. */\
	/* gx_device *target; */	/* device for which commands */\
					/* are being buffered */\
	dev_proc_make_buffer_device((*make_buffer_device));\
	byte *data;			/* buffer area */\
	uint data_size;			/* size of buffer */\
	clist_file_ptr cfile;		/* command list file */\
	clist_file_ptr bfile;		/* command list block file */\
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
	byte *mdata;			/* start of memory device data */\
	int band_height;		/* height of each band */\
	int nbands;			/* # of bands */\
	long bfile_end_pos		/* ftell at end of bfile */

/* Define the length of the longest dash pattern we are willing to store. */
/* (Strokes with longer patterns are converted to fills.) */
#define cmd_max_dash 11

/* Define the state of a band list when writing. */
typedef struct gx_device_clist_writer_s {
	gx_device_clist_common;		/* (must be first) */
	int error_code;			/* error returned by cmd_put_op */
	gx_clist_state *states;		/* current state of each band */
	byte *cbuf;			/* start of command buffer */
	byte *cnext;			/* next slot in command buffer */
	byte *cend;			/* end of command buffer */
	cmd_list *ccl;			/* &clist_state.list of last command */
	cmd_list all_band_list;		/* list of all-band commands */
	uint tile_max_size;		/* max size of a single tile (bytes) */
	uint tile_max_count;		/* max # of hash table entries */
	gx_strip_bitmap tile_params;	/* current tile parameters */
	int tile_depth;			/* current tile depth */
	gs_imager_state imager_state;	/* current values of imager params */
	float dash_pattern[cmd_max_dash]; /* current dash pattern */
	const gx_clip_path *clip_path;	/* current clip path */
	gx_bitmap_id clip_path_id;	/* id of current clip path */
	byte color_space;		/* current color space identifier */
					/* (only used for images) */
	int indexed_hival;		/* current indexed space hival */
					/* (ditto) */
} gx_device_clist_writer;

/* Define the state of a band list when reading. */
typedef struct gx_device_clist_reader_s {
	gx_device_clist_common;		/* (must be first) */
} gx_device_clist_reader;

typedef union gx_device_clist_s {
	struct _clc {
	  gx_device_clist_common;
	} common;
	gx_device_clist_reader reader;
	gx_device_clist_writer writer;
} gx_device_clist;

/* The device template itself is never used, only the procs. */
extern gx_device_procs gs_clist_device_procs;
