/* Copyright (C) 1989, 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxbitmap.h */
/* Definitions for stored bitmaps for Ghostscript */

#ifndef gxbitmap_INCLUDED
#  define gxbitmap_INCLUDED

/*
 * Drivers such as the X driver and the command list (band list) driver
 * benefit greatly by being able to cache bitmaps (tiles and characters)
 * and refer to them later.  To help them recognize when a bitmap is the
 * same as one that they have seen before, the core code passes an optional
 * ID with the property that if two bitmaps have the same ID, they are
 * guaranteed to have the same contents.  (The converse is *not* true,
 * however: two bitmaps may have different IDs and still be the same.)
 */
typedef gs_id gx_bitmap_id;
#define gx_no_bitmap_id gs_no_id

/*
 * Bitmaps are stored bit-big-endian (i.e., the 0x80 bit of the first
 * byte corresponds to x=0), as a sequence of bytes (i.e., you can't
 * do word-oriented operations on them if you're on a little-endian
 * platform like the Intel 80x86 or VAX).  Each scan line must start on
 * a `word' (long) boundary, and hence is padded to a word boundary,
 * although this should rarely be of concern, since the raster and width
 * are specified individually.  The first scan line corresponds to y=0
 * in whatever coordinate system is relevant.
 */
/* We assume arch_align_long_mod is 1-4 or 8. */
#if arch_align_long_mod <= 4
#  define log2_align_bitmap_mod 2
#else
#if arch_align_long_mod == 8
#  define log2_align_bitmap_mod 3
#endif
#endif
#define align_bitmap_mod (1 << log2_align_bitmap_mod)
#define bitmap_raster(width_bits)\
  ((uint)(((width_bits + (align_bitmap_mod * 8 - 1))\
    >> (log2_align_bitmap_mod + 3)) << log2_align_bitmap_mod))
/*
 * The basic structure for a bitmap does not specify the depth;
 * this is implicit in the context of use.  Requirements:
 *	size.x > 0, size.y > 0
 *	raster >= (size.x * depth + 7) / 8
 */
#define gx_bitmap_common\
	byte *data;\
	int raster;			/* bytes per scan line */\
	gs_int_point size;		/* width, height */\
	gx_bitmap_id id
typedef struct gx_bitmap_s {
	gx_bitmap_common;
} gx_bitmap;
/*
 * For bitmaps used as halftone tiles, we may replicate the tile in
 * X and/or Y, but it is still valuable to know the true tile dimensions
 * (i.e., the dimensions prior to replication).  Requirements:
 *	width % rep_width = 0
 *	height % rep_height = 0
 * Note that rep_height means something slightly different if shift != 0;
 * see below.
 */
#define gx_tile_bitmap_common\
	gx_bitmap_common;\
	ushort rep_width, rep_height	/* true size of tile */
typedef struct gx_tile_bitmap_s {
	gx_tile_bitmap_common;
} gx_tile_bitmap;
/*
 * For halftones at arbitrary angles, we provide for storing the halftone
 * data as a strip that must be shifted in X for different values of Y.  For
 * an ordinary (non-shifted) halftone that has a repetition width of W and a
 * repetition height of H, the pixel at coordinate (X,Y) corresponds to
 * halftone pixel (X mod W, Y mod H), ignoring phase; for a strip halftone
 * with strip shift S and strip height H, the pixel at (X,Y) corresponds to
 * halftone pixel ((X + S * floor(Y/H)) mod W, Y mod H).  In other words,
 * each Y increment of H shifts the strip left by S pixels.
 *
 * As for non-shifted tiles, a strip bitmap may include multiple copies
 * in X or Y to reduce loop overhead.  In this case, we must distinguish:
 *	- The height of an individual strip, which is the same as
 *	the height of the bitmap being replicated (rep_height, H);
 *	- The height of the entire bitmap (size.y).
 * Similarly, we must distinguish:
 *	- The shift per strip (rep_shift, S);
 *	- The shift for the entire bitmap (shift).
 * Note that shift = (rep_shift * size.y / rep_height) mod rep_width,
 * so the shift member of the structure is only an accelerator.  It is,
 * however, an important one, since it indicates whether the overall
 * bitmap requires shifting or not.
 *
 * If the bitmap consists of a multiple of W / gcd(S, W) copies in Y, the
 * effective shift is zero, reducing it to a tile.  For simplicity, we
 * require that if shift is non-zero, the bitmap height be less than H * W /
 * gcd(S, W).  I.e., we don't allow strip bitmaps that are large enough to
 * include a complete tile but that don't include an integral number of
 * tiles.  Requirements:
 *	rep_shift < rep_width
 *	shift = (rep_shift * (size.y / rep_height)) % rep_width
 */
#define gx_strip_bitmap_common\
	gx_tile_bitmap_common;\
	ushort rep_shift;\
	ushort shift
typedef struct gx_strip_bitmap_s {
	gx_strip_bitmap_common;
} gx_strip_bitmap;

#endif					/* gxbitmap_INCLUDED */
