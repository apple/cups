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

/* gxdht.h */
/* Definition of device halftones */

#ifndef gxdht_INCLUDED
#  define gxdht_INCLUDED

#include "gxarith.h"		/* for igcd */

/*
 * The whitening order is represented by a pair of arrays.
 * The levels array contains an integer (an index into the bits array)
 * for each distinct halftone level, indicating how many pixels should be
 * whitened for that level; levels[0] = 0, levels[i] <= levels[i+1], and
 * levels[num_levels-1] <= num_bits.
 * The bits array contains an (offset,mask) pair for each pixel in the tile.
 * bits[i].offset is the (properly aligned) byte index of a pixel
 * in the tile; bits[i].mask is the mask to be or'ed into this byte and
 * following ones.  This is arranged so it will work properly on
 * either big- or little-endian machines, and with different mask widths.
 */
/* The mask width must be at least as wide as uint, */
/* and must not be wider than the width implied by align_bitmap_mod. */
typedef uint ht_mask_t;
#define ht_mask_bits (sizeof(ht_mask_t) * 8)
typedef struct gx_ht_bit_s {
	uint offset;
	ht_mask_t mask;
} gx_ht_bit;
/* During sampling, bits[i].mask is used to hold a normalized sample value. */
typedef ht_mask_t ht_sample_t;
/* The following awkward expression avoids integer overflow. */
#define max_ht_sample (ht_sample_t)(((1 << (ht_mask_bits - 2)) - 1) * 2 + 1)

/*
 * Define the internal representation of a halftone order.
 * Note that it may include a cached transfer function.
 *
 * Halftone orders exist in two slightly different configurations, strip and
 * complete.  In a complete order, shift = 0 and full_height = height; in a
 * strip order, shift != 0 and full_height is the height of a fully expanded
 * halftone made up of enough shifted strip copies to get back to a zero
 * shift.  In other words, full_height is a cached value, but it is an
 * important one, since it is the modulus used for computing the
 * tile-relative phase.  Requirements:
 *	width > 0, height > 0, multiple > 0
 *	raster >= bitmap_raster(width)
 *	0 <= shift < width
 *	num_bits = width * height
 * For complete orders:
 *	full_height = height
 * For strip orders:
 *	full_height = height * width / gcd(width, shift)
 * Note that during the sampling of a complete spot halftone, these
 * invariants may be violated; in particular, it is possible that shift != 0
 * and height < full_height, even though num_bits and num_levels reflect the
 * full height.  In this case, the invariant is restored (by resetting
 * shift and height) when sampling is finished.  However, we must save the
 * original height and shift values used for sampling, since sometimes we
 * run the "finishing" routines more than once.  (This is ugly, but it's
 * too hard to fix.)
 *
 * See gxbitmap.h for more details about strip halftones.
 */
typedef struct gx_ht_cache_s gx_ht_cache;
typedef struct gx_ht_order_s {
	ushort width;
	ushort height;
	ushort raster;
	ushort shift;
	ushort orig_height;
	ushort orig_shift;
	ushort full_height;
	ushort multiple;	/* square root of number of basic cells */
				/* per multi-cell, see gshtscr.c */
	uint num_levels;		/* = levels size */
	uint num_bits;			/* = bits size = width * height */
	uint *levels;
	gx_ht_bit *bits;
	gx_ht_cache *cache;		/* cache to use */
	gx_transfer_map *transfer;	/* TransferFunction or 0 */
} gx_ht_order;
#define ht_order_is_complete(porder)\
  ((porder)->shift == 0)
#define ht_order_full_height(porder)\
  ((porder)->shift == 0 ? (porder)->height :\
   (porder)->width / igcd((porder)->width, (porder)->shift) *\
     (porder)->height)

/* We only export st_ht_order for use in st_screen_enum. */
extern_st(st_ht_order);
#define public_st_ht_order()	/* in gsht.c */\
  gs_public_st_ptrs4(st_ht_order, gx_ht_order, "gx_ht_order",\
    ht_order_enum_ptrs, ht_order_reloc_ptrs, levels, bits, cache, transfer)
#define st_ht_order_max_ptrs 4

/*
 * Define a device halftone.  This consists of one or more orders.
 * If components = 0, then order is the only current halftone screen
 * (set by setscreen, Type 1 sethalftone, Type 3 sethalftone, or
 * Type 5 sethalftone with only a Default).  Otherwise, order is the
 * gray or black screen (for gray/RGB or CMYK devices respectively),
 * and components is an array of gx_ht_order_components parallel to
 * the components of the client halftone (set by setcolorscreen or
 * Type 5 sethalftone).
 */
typedef struct gx_ht_order_component_s {
	gx_ht_order corder;
	gs_ht_separation_name cname;
} gx_ht_order_component;
#define private_st_ht_order_component()	/* in gsht1.c */\
  gs_private_st_ptrs_add0(st_ht_order_component, gx_ht_order_component,\
    "gx_ht_order_component", ht_order_component_enum_ptrs,\
     ht_order_component_reloc_ptrs, st_ht_order, corder)
#define st_ht_order_component_max_ptrs st_ht_order_max_ptrs
#define private_st_ht_order_comp_element() /* in gsht1.c */\
  gs_private_st_element(st_ht_order_component_element, gx_ht_order_component,\
    "gx_ht_order_component[]", ht_order_element_enum_ptrs,\
    ht_order_element_reloc_ptrs, st_ht_order_component)

#ifndef gx_device_halftone_DEFINED
#  define gx_device_halftone_DEFINED
typedef struct gx_device_halftone_s gx_device_halftone;
#endif

/*
 * color_indices is a cache that gives the indices in components of
 * the screens for the 1, 3, or 4 primary color(s).  These indices are
 * always in the same order, namely:
 *	-,-,-,W(gray)
 *	R,G,B,-
 *	C,M,Y,K
 */
struct gx_device_halftone_s {
	gx_ht_order order;
	uint color_indices[4];
	gx_ht_order_component *components;
	uint num_comp;
		/* The following are computed from the above. */
	int lcm_width, lcm_height;	/* LCM of primary color tile sizes, */
					/* max_int if overflowed */
};
extern_st(st_device_halftone);
#define public_st_device_halftone() /* in gsht.c */\
  gs_public_st_ptrs_add1(st_device_halftone, gx_device_halftone,\
    "gx_device_halftone", device_halftone_enum_ptrs,\
    device_halftone_reloc_ptrs, st_ht_order, order, components)
#define st_device_halftone_max_ptrs (st_ht_order_max_ptrs + 1)

#endif					/* gxdht_INCLUDED */
