/* Copyright (C) 1989, 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxbitmap.h,v 1.2 2000/03/08 23:14:50 mike Exp $ */
/* Definitions for stored bitmaps for Ghostscript */

#ifndef gxbitmap_INCLUDED
#  define gxbitmap_INCLUDED

#include "gstypes.h"		/* for gs_id */
#include "gsbitmap.h"

/* Define the gx version of a bitmap identifier. */
typedef gs_bitmap_id gx_bitmap_id;

/* Define the gx version of the "no identifier" value. */
#define gx_no_bitmap_id gs_no_bitmap_id

/*
 * For gx_bitmap data, each scan line must start on a `word' (long)
 * boundary, and hence is padded to a word boundary, although this should
 * rarely be of concern, since the raster and width are specified
 * individually.
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
 * Define the gx analogue of the basic bitmap structure.  Note that since
 * all scan lines must be aligned, the requirement on raster is:
 *      If size.y > 1,
 *          raster >= bitmap_raster(size.x * depth)
 *          raster % align_bitmap_mod = 0
 */
#define gx_bitmap_common gs_bitmap_common
typedef struct gx_bitmap_s {
    gx_bitmap_common;
} gx_bitmap;

/*
 * Define the gx analogue of the tile bitmap structure.  Note that if
 * shift != 0 (for strip bitmaps, see below), size.y and rep_height
 * mean something slightly different: see below for details.
 */
#define gx_tile_bitmap_common gs_tile_bitmap_common
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
 *      - The height of an individual strip, which is the same as
 *      the height of the bitmap being replicated (rep_height, H);
 *      - The height of the entire bitmap (size.y).
 * Similarly, we must distinguish:
 *      - The shift per strip (rep_shift, S);
 *      - The shift for the entire bitmap (shift).
 * Note that shift = (rep_shift * size.y / rep_height) mod rep_width,
 * so the shift member of the structure is only an accelerator.  It is,
 * however, an important one, since it indicates whether the overall
 * bitmap requires shifting or not.
 *
 * Note that for shifted tiles, size.y is the size of the stored bitmap
 * (1 or more strips), and NOT the height of the actual tile.  The latter
 * is not stored in the structure at all: it can be computed as H * W /
 * gcd(S, W).
 *
 * If the bitmap consists of a multiple of W / gcd(S, W) copies in Y, the
 * effective shift is zero, reducing it to a tile.  For simplicity, we
 * require that if shift is non-zero, the bitmap height be less than H * W /
 * gcd(S, W).  I.e., we don't allow strip bitmaps that are large enough to
 * include a complete tile but that don't include an integral number of
 * tiles.  Requirements:
 *      rep_shift < rep_width
 *      shift = (rep_shift * (size.y / rep_height)) % rep_width
 */
#define gx_strip_bitmap_common\
	gx_tile_bitmap_common;\
	ushort rep_shift;\
	ushort shift
typedef struct gx_strip_bitmap_s {
    gx_strip_bitmap_common;
} gx_strip_bitmap;

extern_st(st_gx_strip_bitmap);
#define public_st_gx_strip_bitmap()	/* in gspcolor.c */\
  gs_public_st_suffix_add0_local(st_gx_strip_bitmap, gx_strip_bitmap,\
    "gx_strip_bitmap", bitmap_enum_ptrs, bitmap_reloc_ptrs,\
    st_gs_tile_bitmap)
#define st_gx_strip_bitmap_max_ptrs 1

#endif /* gxbitmap_INCLUDED */
