/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxcldev.h */
/* Internal definitions for Ghostscript command lists. */
#include "gxclist.h"
#include "gsropt.h"
#include "gxht.h"			/* for gxdht.h */
#include "gxtmap.h"			/* ditto */
#include "gxdht.h"			/* for halftones */
#include "strimpl.h"			/* for compressed bitmaps */
#include "scfx.h"			/* ditto */
#include "srlx.h"			/* ditto */

/* The implementation files define cdev as either crdev or cwdev. */
#define ccdev (&((gx_device_clist *)dev)->common)
#define cwdev (&((gx_device_clist *)dev)->writer)
#define crdev (&((gx_device_clist *)dev)->reader)

/* ---------------- Commands ---------------- */

/* Define the compression modes for bitmaps. */
/*#define cmd_compress_none 0*/		/* (implicit) */
#define cmd_compress_rle 1
#define clist_rle_init(ss)\
  { s_RLE_set_defaults_inline(ss);\
    s_RLE_init_inline(ss);\
  }
#define clist_rld_init(ss)\
  { s_RLD_set_defaults_inline(ss);\
    s_RLD_init_inline(ss);\
  }
#define cmd_compress_cfe 2
#define clist_cf_init(ss, width)\
  { (ss)->memory = &gs_memory_default;\
    (ss)->K = -1;\
    (ss)->Columns = (width);\
    (ss)->EndOfBlock = false;\
    (ss)->BlackIs1 = true;\
    (ss)->DecodedByteAlign = align_bitmap_mod;\
  }
#define clist_cfe_init(ss, width)\
  { s_CFE_set_defaults_inline(ss);\
    clist_cf_init(ss, width);\
    (*s_CFE_template.init)((stream_state *)(ss));\
  }
#define clist_cfd_init(ss, width, height)\
  { (*s_CFD_template.set_defaults)((stream_state *)ss);\
    clist_cf_init(ss, width);\
    (ss)->Rows = (height);\
    (*s_CFD_template.init)((stream_state *)(ss));\
  }
#define cmd_mask_compress_any\
  ((1 << cmd_compress_rle) | (1 << cmd_compress_cfe))

/*
 * A command always consists of an operation followed by operands;
 * the syntax of the operands depends on the operation.
 * In the operation definitions below:
 *	+ (prefixed) means the operand is in the low 4 bits of the opcode.
 *	# means a variable-size operand encoded with the variable-size
 *	   integer encoding.
 *	% means a variable-size operand encoded with the variable-size
 *	   fixed coordinate encoding.
 *	$ means a color sized according to the device depth.
 *	<> means the operand size depends on other state information
 *	   and/or previous operands.
 */
typedef enum {
	cmd_op_misc = 0x00,		/* (see below) */
	  cmd_opv_end_run = 0x00,	/* (nothing) */
	  cmd_opv_set_tile_size = 0x01,	/* depth, width#, height#, */
					/* rep_width#, rep_height#, */
					/* rep_shift# */
	  cmd_opv_set_tile_phase = 0x02, /* x#, y# */
	  cmd_opv_set_tile_bits = 0x03, /* index#, offset#, <bits> */
	  cmd_opv_set_bits = 0x04,	/* depth*4+compress, width#, height#, */
					/* index#, offset#, <bits> */
	  cmd_opv_set_tile_color = 0x05,  /* (nothing; next set/delta_color */
					/* refers to tile) */
	  cmd_opv_set_lop = 0x06,	/* lop# */
	  cmd_opv_enable_lop = 0x07,	/* (nothing) */
	  cmd_opv_disable_lop = 0x08,	/* (nothing) */
	  cmd_opv_set_ht_size = 0x09,	/* offset#, */
					/* width#, height#, raster#, */
					/* shift#, num_levels#, num_bits# */
	  cmd_opv_set_ht_data = 0x0a,	/* n, n x (uint|gx_ht_bit) */
	  cmd_opv_next_data_x = 0x0b,	/* data_x */
	  cmd_opv_delta2_color0 = 0x0c,	/* dr5dg6db5 or dc4dm4dy4dk4 */
#define cmd_delta2_24_bias 0x00102010
#define cmd_delta2_24_mask 0x001f3f1f
#define cmd_delta2_32_bias 0x08080808
#define cmd_delta2_32_mask 0x0f0f0f0f
	  cmd_opv_delta2_color1 = 0x0d,	/* <<same as color0>> */
	  cmd_opv_set_copy_color = 0x0e,  /* (nothing) */
	  cmd_opv_set_copy_alpha = 0x0f,  /* (nothing) */
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
	cmd_op_copy_mono = 0x90,	/* +compress, x#, y#, w#, h#, */
					/*   <bits> | */
#define cmd_copy_ht_color 4
					/* +4+compress, x#, y#, w#, h#, */
					/*   <bits> | */
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
  "set_tile_index", "?dx?", "?ex?", "?fx?"

#define cmd_misc_op_name_strings\
  "end_run", "set_tile_size", "set_tile_phase", "set_tile_bits",\
  "set_bits", "set_tile_color", "set_lop", "enable_lop",\
  "disable_lop", "set_ht_size", "set_ht_data", "next_data_x",\
  "delta2_color0", "delta2_color1", "set_copy_color", "set_copy_alpha",

#ifdef DEBUG
extern const char *cmd_op_names[16];
extern const char **cmd_sub_op_names[16];
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
	unsigned dx : 4;
	unsigned dy : 4;
} gx_cmd_rect_tiny;
#define cmd_min_dxy_tiny (-8)
#define cmd_max_dxy_tiny 7

/*
 * When we write bitmaps, we remove raster padding selectively:
 *	- If the width is <= 6 bytes, we remove all the padding;
 *	- If the bitmap is only 1 scan line high, we remove the padding;
 *	- If the bitmap is uncompressed, we remove the padding from the
 *	last scan line;
 *	- Otherwise, we don't remove any padding.
 * Currently, this information is embodied in the code that writes bitmaps
 * (cmd_put_bits) and in the code that reads bitmaps (set_[tile_]bits and
 * copy_xxx opcodes).
 *
 * For halftone cells, we always write an unreplicated bitmap, but we
 * reserve cache space for the reading pass based on the replicated size.
 * See the clist_change_tile procedure for the algorithm that chooses the
 * replication factors.
 */
#define cmd_max_short_width_bytes 6
#define cmd_max_short_width_bits (cmd_max_short_width_bytes * 8)

/* ---------------- Block file entries ---------------- */

typedef struct cmd_block_s {
	int band;
#define cmd_band_all (-1)		/* do in all bands */
#define cmd_band_end (-2)		/* end of band file */
	long pos;			/* starting position in cfile */
} cmd_block;

/* ---------------- Band state ---------------- */

/* Remember the current state of one band when writing or reading. */
struct gx_clist_state_s {
	gx_color_index colors[2];	/* most recent colors */
	uint tile_index;		/* most recent tile index */
	gx_bitmap_id tile_id;		/* most recent tile id */
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
	ushort color_is_alpha;		/* (Boolean) for copy_color_alpha */
	ushort known;			/* flags for whether this band */
					/* knows various misc. parameters */
/****** THESE SHOULD BE MOVED TO gxclpath.h ******/
#define flatness_known		(1<<0)
#define fill_adjust_known	(1<<1)
#define ctm_known		(1<<2)
#define line_width_known	(1<<3)
#define miter_limit_known	(1<<4)
#define misc_known		(1<<5)
#define dash_known		(1<<6)
#define clip_path_known		(1<<7)
#define stroke_all_known	((1<<8)-1)
#define color_space_known	(1<<8)
#define all_known		((1<<9)-1)
		/* Following are only used when writing */
	cmd_list list;			/* list of commands for band */
};

/* The initial values for a band state */
/*static const gx_clist_state cls_initial*/
#define cls_initial_values\
	 { gx_no_color_index, gx_no_color_index },\
	0, gx_no_bitmap_id,\
	 { 0, 0 }, { gx_no_color_index, gx_no_color_index },\
	 { 0, 0, 0, 0 }, lop_default, 0, 0, 0, all_known,\
	 { 0, 0 }

/* Define the size of the command buffer used for reading. */
/* This is needed to split up operations with a large amount of data, */
/* primarily large copy_ operations. */
#define cbuf_size 800

/* ---------------- Driver procedure support ---------------- */

/* The procedures and macros defined here are used when writing */
/* (gxclist.c, gxclbits.c, gxclpath.c). */

/* ------ Exported by gxclist.c ------ */

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
byte *cmd_put_list_op(P3(gx_device_clist_writer *cldev, cmd_list *pcl, uint size));
#ifdef DEBUG
byte *cmd_put_op(P3(gx_device_clist_writer *cldev, gx_clist_state *pcls, uint size));
#else
#  define cmd_put_op(cldev, pcls, size)\
     cmd_put_list_op(cldev, &(pcls)->list, size)
#endif
/* Call cmd_put_op and return properly if an error occurs. */
#define set_cmd_put_op(dp, cldev, pcls, op, csize)\
  do {\
    if ( (dp = cmd_put_op(cldev, pcls, csize)) == 0 )\
      return (cldev)->error_code;\
    *dp = cmd_count_op(op, csize);\
  } while ( 0 )

/* Add a command for all bands. */
byte *cmd_put_all_op(P2(gx_device_clist_writer *cldev, uint size));
/* Call cmd_put_all_op and return properly if an error occurs. */
#define set_cmd_put_all_op(dp, cldev, op, csize)\
  do {\
    if ( (dp = cmd_put_all_op(cldev, csize)) == 0 )\
      return (cldev)->error_code;\
    *dp = cmd_count_op(op, csize);\
  } while ( 0 )

/* Shorten the last allocated command. */
/* Note that this does not adjust the statistics. */
#define cmd_shorten_list_op(cldev, pcls, delta)\
  ((pcls)->tail->size -= (delta), (cldev)->cnext -= (delta))
#define cmd_shorten_op(cldev, pcls, delta)\
  cmd_shorten_list_op(cldev, &(pcls)->list, delta)

/* Flush the buffer. */
int clist_flush_buffer(P1(gx_device_clist_writer *));

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
int cmd_size_rect(P1(const gx_cmd_rect *));
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
  clist_select_color0, clist_select_color1,
  clist_select_tile_color0, clist_select_tile_color1;
int cmd_put_color(P5(gx_device_clist_writer *cldev, gx_clist_state *pcls,
		     const clist_select_color_t *select,
		     gx_color_index color, gx_color_index *pcolor));
#define cmd_set_color0(dev, pcls, color0)\
  cmd_put_color(dev, pcls, &clist_select_color0, color0, &(pcls)->colors[0])
#define cmd_set_color1(dev, pcls, color1)\
  cmd_put_color(dev, pcls, &clist_select_color1, color1, &(pcls)->colors[1])

/* Put out a command to set the tile colors. */
int cmd_set_tile_colors(P4(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  gx_color_index color0, gx_color_index color1));

/* Put out a command to set the tile phase. */
int cmd_set_tile_phase(P4(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  int px, int py));

/* Enable or disable the logical operation. */
int cmd_put_enable_lop(P3(gx_device_clist_writer *, gx_clist_state *, int));
#define cmd_do_enable_lop(cldev, pcls, enable)\
  if ( (pcls)->lop_enabled == ((enable) ^ 1) &&\
         cmd_put_enable_lop(cldev, pcls, enable) < 0\
     )\
    return (cldev)->error_code
#define cmd_enable_lop(cldev, pcls)\
  cmd_do_enable_lop(cldev, pcls, 1)
#define cmd_disable_lop(cldev, pcls)\
  cmd_do_enable_lop(cldev, pcls, 0)

/* Enable or disable clipping. */
extern byte cmd_opvar_enable_clip, cmd_opvar_disable_clip;
int cmd_put_enable_clip(P3(gx_device_clist_writer *, gx_clist_state *, int));
#define cmd_do_enable_clip(cldev, pcls, enable)\
  if ( (pcls)->clip_enabled == ((enable) ^ 1) &&\
         cmd_put_enable_clip(cldev, pcls, enable) < 0\
     )\
    return (cldev)->error_code
#define cmd_enable_clip(cldev, pcls)\
  cmd_do_enable_clip(cldev, pcls, 1)
#define cmd_disable_clip(cldev, pcls)\
  cmd_do_enable_clip(cldev, pcls, 0)

/* Write a command to set the logical operation. */
int cmd_set_lop(P3(gx_device_clist_writer *, gx_clist_state *,
  gs_logical_operation_t));

/* Put out a fill or tile rectangle command. */
int cmd_write_rect_cmd(P7(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  int op, int x, int y, int width, int height));

/*
 * Define macros for dividing up an operation into bands.
 * Note that BEGIN_RECT resets y and height.  It is OK for the code that
 * processes each band to reset height to a smaller (positive) value;
 * the vertical subdivision code in copy_mono, copy_color, and copy_alpha
 * makes use of this.  The band processing code may `continue' (to reduce
 * nesting of conditionals).
 */
#define BEGIN_RECT\
   {	int yend = y + height;\
	int band_height = cdev->band_height;\
	do\
	   {	int band = y / band_height;\
		gx_clist_state *pcls = cdev->states + band;\
		int band_end = (band + 1) * band_height;\
		height = min(band_end, yend) - y;\
		   {
#define END_RECT\
		   }\
	   }\
	while ( (y += height) < yend );\
   }

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

int cmd_put_bits(P10(gx_device_clist_writer *cldev, gx_clist_state *pcls,
		     const byte *data, uint width_bits, uint height,
		     uint raster, int op_size, int compression_mask,
		     byte **pdp, uint *psize));

/*
 * Change tiles for clist_tile_rectangle.  (We make this a separate
 * procedure primarily for readability.)
 */
int clist_change_tile(P4(gx_device_clist_writer *cldev, gx_clist_state *pcls,
			 const gx_strip_bitmap *tiles, int depth));

/*
 * Change "tile" for clist_copy_*.  Only uses tiles->{data, id, raster,
 * rep_width, rep_height}.  tiles->[rep_]shift must be zero.
 */
int clist_change_bits(P4(gx_device_clist_writer *cldev, gx_clist_state *pcls,
  const gx_strip_bitmap *tiles, int depth));
