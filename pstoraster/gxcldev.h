/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxcldev.h,v 1.3 2001/03/16 20:42:06 mike Exp $ */
/* Internal definitions for Ghostscript command lists. */

#ifndef gxcldev_INCLUDED
#  define gxcldev_INCLUDED

#include "gsropt.h"
#include "gxht.h"		/* for gxdht.h */
#include "gxtmap.h"		/* ditto */
#include "gxdht.h"		/* for halftones */
#include "strimpl.h"		/* for compressed bitmaps */
#include "scfx.h"		/* ditto */
#include "srlx.h"		/* ditto */

/* ---------------- Commands ---------------- */

/* Define the compression modes for bitmaps. */
/*#define cmd_compress_none 0 *//* (implicit) */
#define cmd_compress_rle 1
#define clist_rle_init(ss)\
  BEGIN\
    s_RLE_set_defaults_inline(ss);\
    s_RLE_init_inline(ss);\
  END
#define clist_rld_init(ss)\
  BEGIN\
    s_RLD_set_defaults_inline(ss);\
    s_RLD_init_inline(ss);\
  END
#define cmd_compress_cfe 2
#define clist_cf_init(ss, width, mem)\
  BEGIN\
    (ss)->memory = (mem);\
    (ss)->K = -1;\
    (ss)->Columns = (width);\
    (ss)->EndOfBlock = false;\
    (ss)->BlackIs1 = true;\
    (ss)->DecodedByteAlign = align_bitmap_mod;\
  END
#define clist_cfe_init(ss, width, mem)\
  BEGIN\
    s_CFE_set_defaults_inline(ss);\
    clist_cf_init(ss, width, mem);\
    (*s_CFE_template.init)((stream_state *)(ss));\
  END
#define clist_cfd_init(ss, width, height, mem)\
  BEGIN\
    (*s_CFD_template.set_defaults)((stream_state *)ss);\
    clist_cf_init(ss, width, mem);\
    (ss)->Rows = (height);\
    (*s_CFD_template.init)((stream_state *)(ss));\
  END
#define cmd_mask_compress_any\
  ((1 << cmd_compress_rle) | (1 << cmd_compress_cfe))

/*
 * A command always consists of an operation followed by operands;
 * the syntax of the operands depends on the operation.
 * In the operation definitions below:
 *      + (prefixed) means the operand is in the low 4 bits of the opcode.
 *      # means a variable-size operand encoded with the variable-size
 *         integer encoding.
 *      % means a variable-size operand encoded with the variable-size
 *         fixed coordinate encoding.
 *      $ means a color sized according to the device depth.
 *      <> means the operand size depends on other state information
 *         and/or previous operands.
 */
typedef enum {
    cmd_op_misc = 0x00,		/* (see below) */
    cmd_opv_end_run = 0x00,	/* (nothing) */
    cmd_opv_set_tile_size = 0x01,	/* rs?(1)nry?(1)nrx?(1)depth-1(5), */
				/* rep_width#, rep_height#, */
				/* [, nreps_x#][, nreps_y #] */
				/* [, rep_shift#] */
    cmd_opv_set_tile_phase = 0x02,	/* x#, y# */
    cmd_opv_set_tile_bits = 0x03,	/* index#, offset#, <bits> */
    cmd_opv_set_bits = 0x04,	/* depth*4+compress, width#, height#, */
				/* index#, offset#, <bits> */
    cmd_opv_set_tile_color = 0x05,	/* (nothing; next set/delta_color */
				/* refers to tile) */
    cmd_opv_set_misc = 0x06,
#define cmd_set_misc_lop (0 << 6)	/* 00: lop_lsb(6), lop_msb# */
#define cmd_set_misc_data_x (1 << 6)	/* 01: more(1)dx_lsb(5)[, dx_msb#] */
#define cmd_set_misc_map (2 << 6)	/* 10: non-0(1)map_index(5) */
				/*   [, n x frac] */
#define cmd_set_misc_halftone (3 << 6)	/* 11: type(6), num_comp# */
    cmd_opv_enable_lop = 0x07,	/* (nothing) */
    cmd_opv_disable_lop = 0x08,	/* (nothing) */
    cmd_opv_set_ht_order = 0x09,	/* component+1#[, cname#], */
				/* width#, height#, raster#, */
				/* shift#, num_levels#, num_bits# */
    cmd_opv_set_ht_data = 0x0a,	/* n, n x (uint|gx_ht_bit) */
    cmd_opv_end_page = 0x0b,	/* (nothing) */
    cmd_opv_delta2_color0 = 0x0c,	/* dr5dg6db5 or dc4dm4dy4dk4 */
#define cmd_delta2_24_bias 0x00102010
#define cmd_delta2_24_mask 0x001f3f1f
#define cmd_delta2_32_bias 0x08080808
#define cmd_delta2_32_mask 0x0f0f0f0f
    cmd_opv_delta2_color1 = 0x0d,	/* <<same as color0>> */
    cmd_opv_set_copy_color = 0x0e,	/* (nothing) */
    cmd_opv_set_copy_alpha = 0x0f,	/* (nothing) */
    cmd_op_set_color0 = 0x10,	/* +15 = transparent | */
				/* +0, color$ | +dcolor+8 | */
				/* +dr4, dg4db4 | */
				/* +dc3dm1, dm2dy3dk3 */
    cmd_op_set_color1 = 0x20,	/* <<same as color0>> */
#define cmd_delta1_24_bias 0x00080808
#define cmd_delta1_24_mask 0x000f0f0f
#define cmd_delta1_32_bias 0x04040404
#define cmd_delta1_32_mask 0x07070707
    cmd_op_fill_rect = 0x30,	/* +dy2dh2, x#, w# | +0, rect# */
    cmd_op_fill_rect_short = 0x40,	/* +dh, dx, dw | +0, rect_short */
    cmd_op_fill_rect_tiny = 0x50,	/* +dw+0, rect_tiny | +dw+8 */
    cmd_op_tile_rect = 0x60,	/* +dy2dh2, x#, w# | +0, rect# */
    cmd_op_tile_rect_short = 0x70,	/* +dh, dx, dw | +0, rect_short */
    cmd_op_tile_rect_tiny = 0x80,	/* +dw+0, rect_tiny | +dw+8 */
    cmd_op_copy_mono = 0x90,	/* +compress, x#, y#, (w+data_x)#, */
				/* h#, <bits> | */
#define cmd_copy_ht_color 4
				/* +4+compress, x#, y#, (w+data_x)#, */
				/* h#, <bits> | */
#define cmd_copy_use_tile 8
				/* +8 (use tile), x#, y# | */
				/* +12 (use tile), x#, y# */
    cmd_op_copy_color_alpha = 0xa0,	/* (same as copy_mono, except: */
				/* if color, ignore ht_color; */
				/* if alpha & !use_tile, depth is */
				/*   first operand) */
    cmd_op_delta_tile_index = 0xb0,	/* +delta+8 */
    cmd_op_set_tile_index = 0xc0	/* +index[11:8], index[7:0] */
} gx_cmd_op;

#define cmd_op_name_strings\
  "(misc)", "set_color[0]", "set_color[1]", "fill_rect",\
  "fill_rect_short", "fill_rect_tiny", "tile_rect", "tile_rect_short",\
  "tile_rect_tiny", "copy_mono", "copy_color_alpha", "delta_tile_index",\
  "set_tile_index", "(misc2)", "(segment)", "(path)"

#define cmd_misc_op_name_strings\
  "end_run", "set_tile_size", "set_tile_phase", "set_tile_bits",\
  "set_bits", "set_tile_color", "set_misc", "enable_lop",\
  "disable_lop", "set_ht_order", "set_ht_data", "end_page",\
  "delta2_color0", "delta2_color1", "set_copy_color", "set_copy_alpha",

#ifdef DEBUG
extern const char *const cmd_op_names[16];
extern const char *const *const cmd_sub_op_names[16];
#endif

/*
 * Define the size of the largest command, not counting any bitmap or
 * similar variable-length operands.
 * The variable-size integer encoding is little-endian.  The low 7 bits
 * of each byte contain data; the top bit is 1 for all but the last byte.
 */
#define cmd_max_intsize(siz)\
  (((siz) * 8 + 6) / 7)
#define cmd_largest_size\
  (2 + (1 + cmd_max_dash) * sizeof(float))

/* ---------------- Command parameters ---------------- */

/* Rectangle */
typedef struct {
    int x, y, width, height;
} gx_cmd_rect;

/* Short rectangle */
typedef struct {
    byte dx, dwidth, dy, dheight;	/* dy and dheight are optional */
} gx_cmd_rect_short;

#define cmd_min_short (-128)
#define cmd_max_short 127
/* Tiny rectangle */
#define cmd_min_dw_tiny (-4)
#define cmd_max_dw_tiny 3
typedef struct {
    unsigned dx:4;
    unsigned dy:4;
} gx_cmd_rect_tiny;

#define cmd_min_dxy_tiny (-8)
#define cmd_max_dxy_tiny 7

/*
 * When we write bitmaps, we remove raster padding selectively:
 *      - If the bitmap is compressed, we don't remove any padding;
 *      - If the width is <= 6 bytes, we remove all the padding;
 *      - If the bitmap is only 1 scan line high, we remove the padding;
 *      - If the bitmap is going to be replicated horizontally (see the
 *      definition of decompress_spread below), we remove the padding;
 *      - Otherwise, we remove the padding only from the last scan line.
 */
#define cmd_max_short_width_bytes 6
#define cmd_max_short_width_bits (cmd_max_short_width_bytes * 8)
/*
 * Determine the (possibly unpadded) width in bytes for writing a bitmap,
 * per the algorithm just outlined.  If compression_mask has any of the
 * cmd_mask_compress_any bits set, we assume the bitmap will be compressed.
 * Return the total size of the bitmap.
 */
uint clist_bitmap_bytes(P5(uint width_bits, uint height,
			   int compression_mask,
			   uint * width_bytes, uint * raster));

/*
 * For halftone cells, we always write an unreplicated bitmap, but we
 * reserve cache space for the reading pass based on the replicated size.
 * See the clist_change_tile procedure for the algorithm that chooses the
 * replication factors.
 */

/* ---------------- Block file entries ---------------- */

typedef struct cmd_block_s {
    int band_min, band_max;
#define cmd_band_end (-1)	/* end of band file */
    long pos;			/* starting position in cfile */
} cmd_block;

/* ---------------- Band state ---------------- */

/* Define the prefix on each command run in the writing buffer. */
typedef struct cmd_prefix_s cmd_prefix;
struct cmd_prefix_s {
    cmd_prefix *next;
    uint size;
};

/* Define the pointers for managing a list of command runs in the buffer. */
/* There is one of these for each band, plus one for band-range commands. */
typedef struct cmd_list_s {
    cmd_prefix *head, *tail;	/* list of commands for band */
} cmd_list;

/* Remember the current state of one band when writing or reading. */
struct gx_clist_state_s {
    gx_color_index colors[2];	/* most recent colors */
    uint tile_index;		/* most recent tile index */
    gx_bitmap_id tile_id;	/* most recent tile id */
/* Since tile table entries may be deleted and/or moved at any time, */
/* the following is the only reliable way to check whether tile_index */
/* references a particular tile id: */
#define cls_has_tile_id(cldev, pcls, tid, offset_temp)\
  ((pcls)->tile_id == (tid) &&\
   (offset_temp = cldev->tile_table[(pcls)->tile_index].offset) != 0 &&\
   ((tile_slot *)(cldev->data + offset_temp))->id == (tid))
    gs_int_point tile_phase;	/* most recent tile phase */
    gx_color_index tile_colors[2];	/* most recent tile colors */
    gx_cmd_rect rect;		/* most recent rectangle */
    gs_logical_operation_t lop;	/* most recent logical op */
    short lop_enabled;		/* 0 = don't use lop, 1 = use lop, */
    /* -1 is used internally */
    short clip_enabled;		/* 0 = don't clip, 1 = do clip, */
    /* -1 is used internally */
    ushort color_is_alpha;	/* (Boolean) for copy_color_alpha */
    ushort known;		/* flags for whether this band */
				/* knows various misc. parameters */
    /* We assign 'known' flags here from the high end; */
    /* gxclpath.h assigns them from the low end. */
#define tile_params_known (1<<15)
#define begin_image_known (1<<14) /* gxclimag.c */
#define initial_known 0x3fff	/* exclude tile & image params */
    /* Following are only used when writing */
    cmd_list list;		/* list of commands for band */
    /* Following is set when writing, read when reading */
    ulong cost;			/* cost of rendering the band */
};

/**** MRS: We need to include this here instead of the top to avoid lots of
 ****      pointer errors.  Why the f**k does GS have to define so many
 ****      interdependent structures?!?
 ****/
#include "gxclist.h"

/* The initial values for a band state */
/*static const gx_clist_state cls_initial */
#define cls_initial_values\
	 { gx_no_color_index, gx_no_color_index },\
	0, gx_no_bitmap_id,\
	 { 0, 0 }, { gx_no_color_index, gx_no_color_index },\
	 { 0, 0, 0, 0 }, lop_default, 0, 0, 0, initial_known,\
	 { 0, 0 }, 0

/* Define the size of the command buffer used for reading. */
/* This is needed to split up operations with a large amount of data, */
/* primarily large copy_ operations. */
#define cbuf_size 800

/* ---------------- Driver procedure support ---------------- */

/*
 * The procedures and macros defined here are used when writing
 * (gxclist.c, gxclbits.c, gxclimag.c, gxclpath.c, gxclrect.c).
 * Note that none of the cmd_put_xxx procedures do VMerror recovery;
 * they convert low-memory warnings to VMerror errors.
 */

/* ------ Exported by gxclist.c ------ */

/*
 * Error recovery procedures for writer-side VMerrors, for async rendering
 * support.  This logic assumes that the command list file and/or the
 * renderer allocate memory from the same pool as the writer. Hence, when
 * the writer runs out of memory, it tries to pause and let the renderer run
 * for a while in hope that enough memory will be freed by it to allow the
 * writer to allocate enough memory to proceed. Once a VMerror is detected,
 * error recovery proceeds in two escalating stages:
 *
 *  1) The recovery logic repeatedly calls clist_VMerror_recover(), which
 *     waits until the next page has finished rendering. The recovery logic
 *     keeps calling clist_VMerror_recover() until enough memory is freed,
 *     or until clist_VMerror_recover() signals that no more pages
 *     remain to be rendered.
 *
 *  2) If enough memory is not free, the recovery logic calls
 *     clist_VMerror_recover_flush() once. This routine terminates and
 *     flushes out the partially-completed page that the writer is currently
 *     writing to the command file, then waits for the partial page to finish
 *     rendering. It then opens up a new command list "file" and resets the
 *     state of the command list machinery to an initial state as if a new
 *     page were beginning.
 *
 * If insufficient memory is available after the 2nd step, the situation
 * is the same as if it ocurred in a non-async setup: the writer program
 * simply used up too much memory and cannot continue.
 *
 * The first stage of error recovery (no flush) is performed without
 * flushing out the current page, so failing commands can simply be
 * restarted after such recovery. This is not true of 2nd stage recovery
 * (flush): as part of its operation, the flush resets the state of both
 * writer and renderer to initial values.  In this event, the recovery logic
 * which called clist_try_recover_VMerror_flush() must force any pertinent
 * state information to be re-emitted before re-issuing the failing command.
 *
 * In case of a VMerror, the internal procedures that support the driver
 * procedures simply return the error code: they do not attempt recovery.
 * Note that all such procedures must take care that (1) they don't update
 * any writer state to reflect information written to the band list unless
 * the write actually succeeds, and (2) they are idempotent, since they may
 * be re-executed after first-stage VMerror recovery.
 *
 * Error recovery is only performed by the driver procedures themselves
 * (fill_rectangle, copy_mono, fill_path, etc.) and a few other procedures
 * at the same level of control.  The implementation of error recovery is
 * packaged up in the FOR_RECTS et al macros defined below, but -- as noted
 * above -- recovery is not fully transparent.  Other routines which perform
 * error recovery are those which open the device, begin a new page, or
 * reopen the device (put_params).
 */
int clist_VMerror_recover(P2(gx_device_clist_writer *, int));
int clist_VMerror_recover_flush(P2(gx_device_clist_writer *, int));

/* Write out device parameters. */
int cmd_put_params(P2(gx_device_clist_writer *, gs_param_list *));

/* Conditionally keep command statistics. */
#ifdef DEBUG
int cmd_count_op(P2(int op, uint size));
void cmd_uncount_op(P2(int op, uint size));
#  define cmd_count_add1(v) (v++)
#else
#  define cmd_count_op(op, size) (op)
#  define cmd_uncount_op(op, size) DO_NOTHING
#  define cmd_count_add1(v) DO_NOTHING
#endif

/* Add a command to the appropriate band list, */
/* and allocate space for its data. */
byte *cmd_put_list_op(P3(gx_device_clist_writer * cldev, cmd_list * pcl, uint size));

#ifdef DEBUG
byte *cmd_put_op(P3(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size));
#else
#  define cmd_put_op(cldev, pcls, size)\
     cmd_put_list_op(cldev, &(pcls)->list, size)
#endif
/* Call cmd_put_op and update stats if no error occurs. */
#define set_cmd_put_op(dp, cldev, pcls, op, csize)\
  ( (dp = cmd_put_op(cldev, pcls, csize)) == 0 ?\
      (cldev)->error_code :\
    (*dp = cmd_count_op(op, csize), 0) )

/* Add a command for all bands or a range of bands. */
byte *cmd_put_range_op(P4(gx_device_clist_writer * cldev, int band_min,
			  int band_max, uint size));

#define cmd_put_all_op(cldev, size)\
  cmd_put_range_op(cldev, 0, (cldev)->nbands - 1, size)
/* Call cmd_put_all/range_op and update stats if no error occurs. */
#define set_cmd_put_range_op(dp, cldev, op, bmin, bmax, csize)\
  ( (dp = cmd_put_range_op(cldev, bmin, bmax, csize)) == 0 ?\
      (cldev)->error_code :\
    (*dp = cmd_count_op(op, csize), 0) )
#define set_cmd_put_all_op(dp, cldev, op, csize)\
  set_cmd_put_range_op(dp, cldev, op, 0, (cldev)->nbands - 1, csize)

/* Shorten the last allocated command. */
/* Note that this does not adjust the statistics. */
#define cmd_shorten_list_op(cldev, pcls, delta)\
  ((pcls)->tail->size -= (delta), (cldev)->cnext -= (delta))
#define cmd_shorten_op(cldev, pcls, delta)\
  cmd_shorten_list_op(cldev, &(pcls)->list, delta)

/* Write out the buffered commands, and reset the buffer. */
/* Return 0 if OK, 1 if OK with low-memory warning, */
/* or the usual negative error code. */
int cmd_write_buffer(P2(gx_device_clist_writer * cldev, byte cmd_end));

/* End a page by flushing the buffer and terminating the command list. */
int clist_end_page(P1(gx_device_clist_writer *));

/* Compute the # of bytes required to represent a variable-size integer. */
/* (This works for negative integers also; they are written as though */
/* they were unsigned.) */
int cmd_size_w(P1(uint));

#define w1byte(w) (!((w) & ~0x7f))
#define w2byte(w) (!((w) & ~0x3fff))
#define cmd_sizew(w)\
  (w1byte(w) ? 1 : w2byte(w) ? 2 : cmd_size_w((uint)(w)))
#define cmd_size2w(wx,wy)\
  (w1byte((wx) | (wy)) ? 2 :\
   cmd_size_w((uint)(wx)) + cmd_size_w((uint)(wy)))
#define cmd_sizexy(xy) cmd_size2w((xy).x, (xy).y)
#define cmd_sizew_max ((sizeof(uint) * 8 + 6) / 7)

/* Put a variable-size integer in the buffer. */
byte *cmd_put_w(P2(uint, byte *));

#define cmd_putw(w,dp)\
  (w1byte(w) ? (*dp = w, ++dp) :\
   w2byte(w) ? (*dp = (w) | 0x80, dp[1] = (w) >> 7, dp += 2) :\
   (dp = cmd_put_w((uint)(w), dp)))
#define cmd_put2w(wx,wy,dp)\
  (w1byte((wx) | (wy)) ? (dp[0] = (wx), dp[1] = (wy), dp += 2) :\
   (dp = cmd_put_w((uint)(wy), cmd_put_w((uint)(wx), dp))))
#define cmd_putxy(xy,dp) cmd_put2w((xy).x, (xy).y, dp)

/* Put out a command to set a color. */
typedef struct {
    byte set_op;
    byte delta2_op;
    bool tile_color;
} clist_select_color_t;
extern const clist_select_color_t
      clist_select_color0, clist_select_color1, clist_select_tile_color0,
      clist_select_tile_color1;
int cmd_put_color(P5(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		     const clist_select_color_t * select,
		     gx_color_index color, gx_color_index * pcolor));

#define cmd_set_color0(dev, pcls, color0)\
  cmd_put_color(dev, pcls, &clist_select_color0, color0, &(pcls)->colors[0])
#define cmd_set_color1(dev, pcls, color1)\
  cmd_put_color(dev, pcls, &clist_select_color1, color1, &(pcls)->colors[1])

/* Put out a command to set the tile colors. */
int cmd_set_tile_colors(P4(gx_device_clist_writer *cldev,
			   gx_clist_state * pcls,
			   gx_color_index color0, gx_color_index color1));

/* Put out a command to set the tile phase. */
int cmd_set_tile_phase(P4(gx_device_clist_writer *cldev,
			  gx_clist_state * pcls,
			  int px, int py));

/* Enable or disable the logical operation. */
int cmd_put_enable_lop(P3(gx_device_clist_writer *, gx_clist_state *, int));
#define cmd_do_enable_lop(cldev, pcls, enable)\
  ( (pcls)->lop_enabled == ((enable) ^ 1) &&\
    cmd_put_enable_lop(cldev, pcls, enable) < 0 ?\
      (cldev)->error_code : 0 )
#define cmd_enable_lop(cldev, pcls)\
  cmd_do_enable_lop(cldev, pcls, 1)
#define cmd_disable_lop(cldev, pcls)\
  cmd_do_enable_lop(cldev, pcls, 0)

/* Enable or disable clipping. */
int cmd_put_enable_clip(P3(gx_device_clist_writer *, gx_clist_state *, int));

#define cmd_do_enable_clip(cldev, pcls, enable)\
  ( (pcls)->clip_enabled == ((enable) ^ 1) &&\
    cmd_put_enable_clip(cldev, pcls, enable) < 0 ?\
      (cldev)->error_code : 0 )
#define cmd_enable_clip(cldev, pcls)\
  cmd_do_enable_clip(cldev, pcls, 1)
#define cmd_disable_clip(cldev, pcls)\
  cmd_do_enable_clip(cldev, pcls, 0)

/* Write a command to set the logical operation. */
int cmd_set_lop(P3(gx_device_clist_writer *, gx_clist_state *,
		   gs_logical_operation_t));

/* Disable (if default) or enable the logical operation, setting it if */
/* needed. */
int cmd_update_lop(P3(gx_device_clist_writer *, gx_clist_state *,
		      gs_logical_operation_t));

/*
 * Define macros for dividing up an operation into bands, per the
 * template

    FOR_RECTS {
	... process rectangle x, y, width, height in band pcls ...
    } END_RECTS;

 * Note that FOR_RECTS resets y and height.  It is OK for the code that
 * processes each band to reset height to a smaller (positive) value; the
 * vertical subdivision code in copy_mono, copy_color, and copy_alpha makes
 * use of this.  The band processing code may `continue' (to reduce nesting
 * of conditionals).
 *
 * If the processing code detects an error that may be a recoverable
 * VMerror, the code may call ERROR_RECT(), which will attempt to fix the
 * VMerror by flushing and closing the band and resetting the imager state,
 * and then restart emitting the entire band. Before flushing the file, the
 * 'on_error' clause of END_RECTS_ON_ERROR (defaults to the constant 1 if
 * END_RECT is used) is evaluated and tested.  The 'on_error' clause enables
 * mop-up actions to be executed before flushing, and/or selectively
 * inhibits the flush, close, reset and restart process.  Similarly, the
 * 'after_recovering' clause of END_RECTS_ON_ERROR allows an action to get
 * performed after successfully recovering.
 *
 * The band processing code may wrap an operation with TRY_RECT { ...  }
 * HANDLE_RECT_UNLESS(code, unless_action) (or HANDLE_RECT(code)). This will
 * perform local first-stage VMerror recovery, by waiting for some memory to
 * become free and then retrying the failed operation starting at the
 * TRY_RECT. If local recovery is unsuccessful, the local recovery code
 * calls ERROR_RECT.
 *
 * In a few cases, the band processing code calls other driver procedures
 * (e.g., clist_copy_mono calls itself recursively if it must split up the
 * operation into smaller pieces) or other procedures that may attempt
 * VMerror recovery.  In such cases, the recursive call must not attempt
 * second-stage VMerror recovery, since the caller would have no way of
 * knowing that the writer state had been reset.  Such recursive calls
 * should be wrapped in NEST_RECT { ... } UNNEST_RECT, which causes
 * ERROR_RECT simply to return the error code rather than attempting
 * recovery.  (TRY/HANDLE_RECT will still attempt local recovery, as
 * described above, but this is harmless since it is transparent.) By
 * convention, calls to cmd_put_xxx or cmd_set_xxx never attempt recovery
 * and so never require NEST_RECTs.
 *
 * If a put_params call fails, the device will be left in a closed state,
 * but higher-level code won't notice this fact.  We flag this by setting
 * permanent_error, which prevents writing to the command list.
 */

/**** MRS: Added cast to cdev->states since size of gx_clist_state is unknown ****/
#define FOR_RECTS\
    BEGIN\
	int yend = y + height;\
	int band_height = cdev->page_band_height;\
	int band_code;\
\
	if (cdev->permanent_error < 0)\
	  return (cdev->permanent_error);\
	do {\
	    int band = y / band_height;\
	    gx_clist_state *pcls = ((struct gx_clist_state_s *)cdev->states) + band;\
	    int band_end = (band + 1) * band_height;\
\
	    height = min(band_end, yend) - y;\
retry_rect:\
	    ;
#define NEST_RECT    ++cdev->driver_call_nesting;
#define UNNEST_RECT  --cdev->driver_call_nesting
#define ERROR_RECT(code_value)\
		BEGIN\
		    band_code = (code_value);\
		    goto error_in_rect;\
		END
#define TRY_RECT\
		BEGIN\
		    do
#define HANDLE_RECT_UNLESS(codevar, unless_clause)\
		    while (codevar < 0 &&\
			   !(codevar = clist_VMerror_recover(cdev, (codevar)))\
			   );\
		    if (codevar < 0 && !(unless_clause))\
			ERROR_RECT(codevar);\
		END
#define HANDLE_RECT(codevar)\
		HANDLE_RECT_UNLESS(codevar, 0)
#define END_RECTS_ON_ERROR(retry_cleanup, is_error, after_recovering)\
	    continue;\
error_in_rect:\
		if (cdev->error_is_retryable) {\
		    retry_cleanup;\
		    if ((is_error) &&\
			cdev->driver_call_nesting == 0 &&\
			(band_code =\
			 clist_VMerror_recover_flush(cdev, band_code)) >= 0 &&\
			(after_recovering)\
			)\
			goto retry_rect;\
		}\
		return band_code;\
	} while ((y += height) < yend);\
    END
#define END_RECTS END_RECTS_ON_ERROR(DO_NOTHING, 1, 1)

/* ------ Exported by gxclrect.c ------ */

/* Put out a fill or tile rectangle command. */
int cmd_write_rect_cmd(P7(gx_device_clist_writer * cldev,
			  gx_clist_state * pcls,
			  int op, int x, int y, int width, int height));

/* ------ Exported by gxclbits.c ------ */

/*
 * Put a bitmap in the buffer, compressing if appropriate.
 * pcls == 0 means put the bitmap in all bands.
 * Return <0 if error, otherwise the compression method.
 * A return value of gs_error_limitcheck means that the bitmap was too big
 * to fit in the command reading buffer.
 * Note that this leaves room for the command and initial arguments,
 * but doesn't fill them in.
 *
 * If decompress_elsewhere is set in the compression_mask, it is OK
 * to write out a compressed bitmap whose decompressed size is too large
 * to fit in the command reading buffer.  (This is OK when reading a
 * cached bitmap, but not a bitmap for a one-time copy operation.)
 */
#define decompress_elsewhere 0x100
/*
 * If decompress_spread is set, the decompressed data will be spread out
 * for replication, so we drop all the padding even if the width is
 * greater than cmd_max_short_width_bytes (see above).
 */
#define decompress_spread 0x200

int cmd_put_bits(P10(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		     const byte * data, uint width_bits, uint height,
		     uint raster, int op_size, int compression_mask,
		     byte ** pdp, uint * psize));

/*
 * Put out commands for a color map (transfer function, black generation, or
 * undercolor removal).  If pid != 0, write the map only if its ID differs
 * from the current one, and update the saved ID in the case.
 */
typedef enum {
    cmd_map_transfer = 0,	/* all transfer functions */
    cmd_map_transfer_0,		/* transfer[0] */
    cmd_map_transfer_1,		/* transfer[1] */
    cmd_map_transfer_2,		/* transfer[2] */
    cmd_map_transfer_3,		/* transfer[3] */
    cmd_map_ht_transfer,	/* transfer fn of most recent halftone order */
    cmd_map_black_generation,
    cmd_map_undercolor_removal
} cmd_map_index;
int cmd_put_color_map(P4(gx_device_clist_writer * cldev,
			 cmd_map_index map_index,
			 const gx_transfer_map * map, gs_id * pid));

/*
 * Change tiles for clist_tile_rectangle.  (We make this a separate
 * procedure primarily for readability.)
 */
int clist_change_tile(P4(gx_device_clist_writer * cldev, gx_clist_state * pcls,
			 const gx_strip_bitmap * tiles, int depth));

/*
 * Change "tile" for clist_copy_*.  Only uses tiles->{data, id, raster,
 * rep_width, rep_height}.  tiles->[rep_]shift must be zero.
 */
int clist_change_bits(P4(gx_device_clist_writer * cldev, gx_clist_state * pcls,
			 const gx_strip_bitmap * tiles, int depth));

/* ------ Exported by gxclimag.c ------ */

/*
 * Add commands to represent a full (device) halftone.
 * (This routine should probably be in some other module.)
 * ****** Note: the type parameter is now unnecessary, because device
 * halftones record the type. ******
 */
int cmd_put_halftone(P3(gx_device_clist_writer * cldev,
		   const gx_device_halftone * pdht, gs_halftone_type type));

/* ------ Exported by gxclrast.c for gxclread.c ------ */

/*
 * Define whether we are actually rendering a band, or just executing
 * the put_params that occurs at the beginning of each page.
 */
typedef enum {
    playback_action_render,
    playback_action_setup
} clist_playback_action;

/* Play back and rasterize one band. */
int clist_playback_band(P7(clist_playback_action action,
			   gx_device_clist_reader *cdev,
			   stream *s, gx_device *target,
			   int x0, int y0, gs_memory_t *mem));

#endif /* gxcldev_INCLUDED */
