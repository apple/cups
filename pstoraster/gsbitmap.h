/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsbitmap.h,v 1.1 2000/03/13 18:57:54 mike Exp $ */
/* Library "client" bitmap structures */

#ifndef gsbitmap_INCLUDED
#define gsbitmap_INCLUDED

#include "gsstruct.h"		/* for extern_st */

/*
 * The Ghostscript library stores all bitmaps bit-big-endian (i.e., the 0x80
 * bit of the first byte corresponds to x=0), as a sequence of bytes (i.e.,
 * you can't do word-oriented operations on them if you're on a
 * little-endian platform like the Intel 80x86 or VAX).  The first scan line
 * corresponds to y=0 in whatever coordinate system is relevant.
 *
 * The structures defined here are for APIs that don't impose any alignment
 * restrictions on either the starting address or the raster (distance
 * between scan lines) of bitmap data.  The structures defined in gxbitmap.h
 * do impose alignment restrictions, so that the library can use more
 * efficient algorithms; they are declared with identical contents to the
 * ones defined here, so that one can cast between them under appropriate
 * circumstances (aligned to unaligned is always safe; unaligned to
 * aligned is safe if one knows somehow that the data are actually aligned.)
 *
 * In this file we also provide structures that include depth information.
 * It probably was a design mistake not to include this information in the
 * gx structures as well.
 */

/*
 * Drivers such as the X driver and the command list (band list) driver
 * benefit greatly by being able to cache bitmaps (tiles and characters)
 * and refer to them later.  To help them recognize when a bitmap is the
 * same as one that they have seen before, the core code passes an optional
 * ID with the property that if two bitmaps have the same ID, they are
 * guaranteed to have the same contents.  (The converse is *not* true,
 * however: two bitmaps may have different IDs and still be the same.)
 */
typedef gs_id gs_bitmap_id;

/* Define a special value to indicate "no identifier". */
#define gs_no_bitmap_id     gs_no_id

/*
 * In its simplest form, the client bitmap structure does not specify a
 * depth, expecting it to be implicit in the context of use.  In many cases
 * it is possible to guess this by comparing size.x and raster, but of
 * course code should not rely on this.  See also gs_depth_bitmap below.
 * Requirements:
 *      size.x > 0, size.y > 0
 *      If size.y > 1,
 *          raster >= (size.x * depth + 7) / 8
 */
#define gs_bitmap_common                                                    \
    byte *          data;       /* pointer to the data */                   \
    int             raster;     /* increment between scanlines, bytes */    \
    gs_int_point    size;       /* width and height */                      \
    gs_bitmap_id    id		/* usually unused */

typedef struct gs_bitmap_s {
    gs_bitmap_common;
} gs_bitmap;

/*
 * For bitmaps used as halftone tiles, we may replicate the tile in
 * X and/or Y, but it is still valuable to know the true tile dimensions
 * (i.e., the dimensions prior to replication).  Requirements:
 *      size.x % rep_width = 0
 *      size.y % rep_height = 0
 * Unaligned bitmaps are not very likely to be used as tiles (replicated),
 * since most of the library procedures that replicate tiles expect them
 * to be aligned.
 */
#define gs_tile_bitmap_common                                   \
    gs_bitmap_common;                                           \
    ushort      rep_width, rep_height	/* true size of tile */

typedef struct gs_tile_bitmap_s {
    gs_tile_bitmap_common;
} gs_tile_bitmap;

/*
 * There is no "strip" version for client bitmaps, as the strip structure is
 * primarily used to efficiently store bitmaps rendered at an angle, and
 * there is little reason to do so with client bitmaps.
 *
 * For client bitmaps it is not always apparent from context what the intended
 * depth per sample value is. To provide for this, an extended version of the
 * bitmap structure is provided, that handles both variable depth and
 * interleaved color components. This structure is provided in both the
 * normal and tiled version.
 *
 * Extending this line of thinking, one could also add color space information
 * to a client bitmap structure. We have chosen not to do so, because color
 * space is almost always derived from context, and to provide such a feature
 * would involve additional memory-management complexity.
 */
#define gs_depth_bitmap_common                                      \
    gs_bitmap_common;                                               \
    byte     pix_depth;      /* bits per sample */                  \
    byte     num_comps      /* number of interleaved components */  \

typedef struct gs_depth_bitmap_s {
    gs_depth_bitmap_common;
} gs_depth_bitmap;

#define gs_tile_depth_bitmap_common                                 \
    gs_tile_bitmap_common;                                          \
    byte     pix_depth;     /* bits per sample */                   \
    byte     num_comps      /* number of interleaved components */  \

typedef struct gs_tile_depth_bitmap_s {
    gs_tile_depth_bitmap_common;
} gs_tile_depth_bitmap;

/*
 * For reasons that are no entirely clear, no memory management routines were
 * provided for the aligned bitmap structures provided in gxbitmap.h. Since
 * client bitmaps will, by nature, be created by different clients, so public
 * memory management procedures are provided. Note that the memory management
 * structure names retain the "gs_" prefix, to distinguish these structures
 * from those that may be provided for the gx_*_bitmap structures.
 *
 * For historical reasons of no particular validity (this was where the client
 * bitmap structure was first provided), the memory managment procedures for
 * client bitmap structures are included in gspcolor.c.
 */
extern_st(st_gs_bitmap);
extern_st(st_gs_tile_bitmap);
extern_st(st_gs_depth_bitmap);
extern_st(st_gs_tile_depth_bitmap);

#define public_st_gs_bitmap()   /* in gspcolor.c */ \
    gs_public_st_ptrs1( st_gs_bitmap,               \
                        gs_bitmap,                  \
                        "client bitmap",            \
                        bitmap_enum_ptrs,           \
                        bitmap_reloc_ptrs,          \
                        data                        \
                        )

#define public_st_gs_tile_bitmap()  /* in gspcolor.c */ \
    gs_public_st_suffix_add0_local( st_gs_tile_bitmap,        \
				    gs_tile_bitmap,           \
				    "client tile bitmap",     \
				    bitmap_enum_ptrs,    \
				    bitmap_reloc_ptrs,   \
				    st_gs_bitmap              \
				    )

#define public_st_gs_depth_bitmap() /* in gspcolor.c */ \
    gs_public_st_suffix_add0_local( st_gs_depth_bitmap,       \
				    gs_depth_bitmap,          \
				    "client depth bitmap",    \
				    bitmap_enum_ptrs,   \
				    bitmap_reloc_ptrs,  \
				    st_gs_bitmap              \
				    )

#define public_st_gs_tile_depth_bitmap()/* in gspcolor.c */ \
    gs_public_st_suffix_add0_local( st_gs_tile_depth_bitmap,      \
				    gs_tile_depth_bitmap,         \
				    "client tile_depth bitmap",   \
				    bitmap_enum_ptrs,  \
				    bitmap_reloc_ptrs, \
				    st_gs_tile_bitmap             \
				    )

#endif /* gsbitmap_INCLUDED */
