/* Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxdht.h,v 1.2 2000/03/08 23:14:57 mike Exp $ */
/* Definition of device halftones */

#ifndef gxdht_INCLUDED
#  define gxdht_INCLUDED

#include "gsrefct.h"
#include "gscsepnm.h"
#include "gxarith.h"		/* for igcd */
#include "gxhttype.h"

/*
 * We represent a halftone tile as a rectangular super-cell consisting of
 * multiple copies of a multi-cell whose corners lie on integral
 * coordinates, which in turn is a parallelogram (normally square) array of
 * basic parallelogram (normally square) cells whose corners lie on rational
 * coordinates.
 *
 * Let T be the aspect ratio (ratio of physical pixel height to physical
 * pixel width), which is abs(xx/yy) for portrait devices and abs(yx/xy) for
 * landscape devices.  We characterize the basic cell by four rational
 * numbers U(') = M(')/R(') and V(') = N(')/R(') where R(') is positive, at
 * least one of U and V (and the corresponding one of U' and V') is
 * non-zero, and U' is approximately U/T and V' is approximately V*T; these
 * numbers define the vertices of the basic cell at device space coordinates
 * (0,0), (U,V), (U-V',U'+V), and (-V',U'); then the multi-cell is defined
 * similarly by M(') and N(').  From these definitions, the basic cell has
 * an area of B = U*U' + V*V' = (M*M' + N*N') / R*R' pixels, and the
 * multi-cell has an area of C = B * R*R' = M*M' + N*N' pixels.
 *
 * If the coefficients of the default device transformation matrix are xx,
 * xy, yx, and yy, then U and V are related to the frequency F and the angle
 * A by:
 *      P = 72 / F;
 *      U = P * (xx * cos(A) + yx * sin(A));
 *      V = P * (xy * cos(A) + yy * sin(A)).
 *
 * We can tile the plane with any rectangular super-cell that consists of
 * repetitions of the multi-cell and whose corners coincide with multi-cell
 * coordinates (0,0).  We observe that for any integers i, j such that i*N -
 * j*M' = 0, a multi-cell corner lies on the X axis at W = i*M + j*N';
 * similarly, if i'*M - j'*N' = 0, a corner lies on the Y axis at W' = i'*N
 * + j'*M'.  Then the super-cell occupies Z = W * W' pixels, consisting of Z
 * / C multi-cells or Z / B basic cells.  The trick in all this is to find
 * values of F and A that aren't too far from the requested ones, and that
 * yield a manageably small value for Z.
 *
 * Note that the super-cell only has to be so large because we want to use
 * it directly to tile the plane.  In fact, we can decompose it into W' / D
 * horizontal strips of width W and height D, shifted horizontally with
 * respect to each other by S pixels, where we compute S by finding h and k
 * such that h*N - k*M' = D and then S = h*M + k*N'.  The halftone setup
 * routines only generate a single strip of samples, and let
 * gx_ht_construct_spot_order construct the rest.  If W' is large, we
 * actually keep only one strip, and let the strip_tile_rectangle routines
 * do the shifting at rendering time.
 */
typedef struct gx_ht_cell_params_s {
    /* Defining values.  M * M1 != 0 or N * N1 != 0; R > 0, R1 > 0. */
    /* R and D are short rather than ushort so that we don't get */
    /* unsigned results from arithmetic. */
    short M, N, R;
    short M1, N1, R1;
    /* Derived values. */
    ulong C;
    short D, D1;
    uint W, W1;
    int S;
} gx_ht_cell_params_t;

/* Compute the derived values from the defining values. */
void gx_compute_cell_values(P1(gx_ht_cell_params_t *));

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
 *      width > 0, height > 0, multiple > 0
 *      raster >= bitmap_raster(width)
 *      0 <= shift < width
 *      num_bits = width * height
 * For complete orders:
 *      full_height = height
 * For strip orders:
 *      full_height = height * width / gcd(width, shift)
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

#ifndef gx_ht_order_DEFINED
#  define gx_ht_order_DEFINED
typedef struct gx_ht_order_s gx_ht_order;

#endif
struct gx_ht_order_s {
    gx_ht_cell_params_t params;	/* parameters defining the cells */
    ushort width;
    ushort height;
    ushort raster;
    ushort shift;
    ushort orig_height;
    ushort orig_shift;
    uint full_height;
    uint num_levels;		/* = levels size */
    uint num_bits;		/* = bits size = width * height */
    uint *levels;
    gx_ht_bit *bits;
    gx_ht_cache *cache;		/* cache to use */
    gx_transfer_map *transfer;	/* TransferFunction or 0 */
};

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
 *
 * Multi-component halftone orders may be required even in Level 1 systems,
 * because they are needed for setcolorscreen.
 *
 * NOTE: it is assumed that all subsidiary structures of device halftones
 * (the components array, and the bits, levels, cache, and transfer members
 * of any gx_ht_orders, both the default order and any component orders) are
 * allocated with the same allocator as the device halftone itself.
 */
typedef struct gx_ht_order_component_s {
    gx_ht_order corder;
    gs_ht_separation_name cname;
} gx_ht_order_component;

#define private_st_ht_order_component()	/* in gsht.c */\
  gs_private_st_ptrs_add0(st_ht_order_component, gx_ht_order_component,\
    "gx_ht_order_component", ht_order_component_enum_ptrs,\
     ht_order_component_reloc_ptrs, st_ht_order, corder)
#define st_ht_order_component_max_ptrs st_ht_order_max_ptrs
/* We only export st_ht_order_component_element for use in banding. */
extern_st(st_ht_order_component_element);
#define public_st_ht_order_comp_element() /* in gsht.c */\
  gs_public_st_element(st_ht_order_component_element, gx_ht_order_component,\
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
 *      -,-,-,W(gray)
 *      R,G,B,-
 *      C,M,Y,K
 */
struct gx_device_halftone_s {
    gx_ht_order order;		/* must be first, for subclassing */
    rc_header rc;
    gs_id id;			/* the id changes whenever the data change */
    /*
     * We have to keep the halftone type so that we can pass it
     * through the band list for gx_imager_dev_ht_install.
     */
    gs_halftone_type type;
    gx_ht_order_component *components;
    uint num_comp;
    /* The following are computed from the above. */
    uint color_indices[4];
    int lcm_width, lcm_height;	/* LCM of primary color tile sizes, */
    /* max_int if overflowed */
};

extern_st(st_device_halftone);
#define public_st_device_halftone() /* in gsht.c */\
  gs_public_st_ptrs_add1(st_device_halftone, gx_device_halftone,\
    "gx_device_halftone", device_halftone_enum_ptrs,\
    device_halftone_reloc_ptrs, st_ht_order, order, components)
#define st_device_halftone_max_ptrs (st_ht_order_max_ptrs + 1)

/* Release a gx_device_halftone by freeing its components. */
/* (Don't free the gx_device_halftone itself.) */
void gx_device_halftone_release(P2(gx_device_halftone * pdht,
				   gs_memory_t * mem));

#endif /* gxdht_INCLUDED */
