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

/*$Id: gzht.h,v 1.2 2000/03/08 23:15:07 mike Exp $ */
/* Requires gxdevice.h, gxdcolor.h */

#ifndef gzht_INCLUDED
#  define gzht_INCLUDED

#include "gscsel.h"
#include "gxht.h"
#include "gxfmap.h"
#include "gxdht.h"
#include "gxhttile.h"

/* Sort a sampled halftone order by sample value. */
void gx_sort_ht_order(P2(gx_ht_bit *, uint));

/* (Internal) procedures for constructing halftone orders. */
int gx_ht_alloc_order(P6(gx_ht_order * porder, uint width, uint height,
		     uint strip_shift, uint num_levels, gs_memory_t * mem));
int gx_ht_alloc_client_order(P6(gx_ht_order * porder, uint width, uint height,
			uint num_levels, uint num_bits, gs_memory_t * mem));
void gx_ht_construct_spot_order(P1(gx_ht_order *));
void gx_ht_construct_threshold_order(P2(gx_ht_order *, const byte *));
void gx_ht_construct_bit(P3(gx_ht_bit * bit, int width, int bit_num));
void gx_ht_construct_bits(P1(gx_ht_order *));

/* Halftone enumeration structure */
struct gs_screen_enum_s {
    gs_halftone halftone;	/* supplied by client */
    gx_ht_order order;
    gs_matrix mat;		/* for mapping device x,y to rotated cell */
    int x, y;
    int strip, shift;
    gs_state *pgs;
};

#define private_st_gs_screen_enum() /* in gshtscr.c */\
  gs_private_st_composite(st_gs_screen_enum, gs_screen_enum,\
    "gs_screen_enum", screen_enum_enum_ptrs, screen_enum_reloc_ptrs)
/*    order.levels, order.bits, pgs) */

/* Prepare a device halftone for installation, but don't install it. */
int gs_sethalftone_prepare(P3(gs_state *, gs_halftone *,
			      gx_device_halftone *));

/* Allocate and initialize a spot screen. */
/* This is the first half of gs_screen_init_accurate/memory. */
int gs_screen_order_init_memory(P5(gx_ht_order *, const gs_state *,
				gs_screen_halftone *, bool, gs_memory_t *));

#define gs_screen_order_init(porder, pgs, phsp, accurate)\
  gs_screen_order_init_memory(porder, pgs, phsp, accurate, pgs->memory)

/* Prepare to sample a spot screen. */
/* This is the second half of gs_screen_init_accurate/memory. */
int gs_screen_enum_init_memory(P5(gs_screen_enum *, const gx_ht_order *,
				  gs_state *, gs_screen_halftone *,
				  gs_memory_t *));

#define gs_screen_enum_init(penum, porder, pgs, phsp)\
  gs_screen_enum_init_memory(penum, porder, pgs, phsp, pgs->memory)

/* Process an entire screen plane. */
int gx_ht_process_screen_memory(P5(gs_screen_enum * penum, gs_state * pgs,
				   gs_screen_halftone * phsp, bool accurate,
				   gs_memory_t * mem));

#define gx_ht_process_screen(penum, pgs, phsp, accurate)\
  gx_ht_process_screen_memory(penum, pgs, phsp, accurate, pgs->memory)

/*
 * We don't want to remember all the values of the halftone screen,
 * because they would take up space proportional to P^3, where P is
 * the number of pixels in a cell.  Instead, we pick some number N of
 * patterns to cache.  Each cache slot covers a range of (P+1)/N
 * different gray levels: we "slide" the contents of the slot back and
 * forth within this range by incrementally adding and dropping 1-bits.
 * N>=0 (obviously); N<=P+1 (likewise); also, so that we can simplify things
 * by preallocating the bookkeeping information for the cache, we define
 * a constant max_cached_tiles which is an a priori maximum value for N.
 *
 * Note that the raster for each tile must be a multiple of bitmap_align_mod,
 * to satisfy the copy_mono device routine, even though a multiple of
 * sizeof(ht_mask_t) would otherwise be sufficient.
 */

struct gx_ht_cache_s {
    /* The following are set when the cache is created. */
    byte *bits;			/* the base of the bits */
    uint bits_size;		/* the space available for bits */
    gx_ht_tile *ht_tiles;	/* the base of the tiles */
    uint num_tiles;		/* the number of tiles allocated */
    /* The following are reset each time the cache is initialized */
    /* for a new screen. */
    gx_ht_order order;		/* the cached order vector */
    int num_cached;		/* actual # of cached tiles */
    int levels_per_tile;	/* # of levels per cached tile */
    gx_bitmap_id base_id;	/* the base id, to which */
    /* we add the halftone level */
};

/* We don't mark from the tiles pointer, and we relocate the tiles en masse. */
#define private_st_ht_tiles()	/* in gxht.c */\
  gs_private_st_composite(st_ht_tiles, gx_ht_tile, "ht tiles",\
    ht_tiles_enum_ptrs, ht_tiles_reloc_ptrs)
#define private_st_ht_cache()	/* in gxht.c */\
  gs_private_st_ptrs_add2(st_ht_cache, gx_ht_cache, "ht cache",\
    ht_cache_enum_ptrs, ht_cache_reloc_ptrs,\
    st_ht_order, order, bits, ht_tiles)

/* Compute a fractional color for dithering, the correctly rounded */
/* quotient f * max_gx_color_value / maxv. */
#define frac_color_(f, maxv)\
  (gx_color_value)(((f) * (0xffffL * 2) + maxv) / (maxv * 2))
extern const gx_color_value *const fc_color_quo[8];

#define fractional_color(f, maxv)\
  ((maxv) <= 7 ? fc_color_quo[maxv][f] : frac_color_(f, maxv))

/* ------ Halftone cache procedures ------ */

/* Allocate/free a halftone cache. */
uint gx_ht_cache_default_tiles(P0());
uint gx_ht_cache_default_bits(P0());
gx_ht_cache *gx_ht_alloc_cache(P3(gs_memory_t *, uint, uint));
void gx_ht_free_cache(P2(gs_memory_t *, gx_ht_cache *));

/* Clear a halftone cache. */
#define gx_ht_clear_cache(pcache)\
  ((pcache)->order.levels = 0, (pcache)->order.bits = 0,\
   (pcache)->ht_tiles[0].tiles.data = 0)

/* Initialize a halftone cache with a given order. */
void gx_ht_init_cache(P2(gx_ht_cache *, const gx_ht_order *));

/* Make the cache order current, and return whether */
/* there is room for all possible tiles in the cache. */
bool gx_check_tile_cache(P1(const gs_imager_state *));

/* Determine whether a given (width, y, height) might fit into a */
/* single tile. If so, return the byte offset of the appropriate row */
/* from the beginning of the tile, and set *ppx to the x phase offset */
/* within the tile; if not, return -1. */
int gx_check_tile_size(P6(const gs_imager_state * pis, int w, int y, int h,
			  gs_color_select_t select, int *ppx));

/* Make a given level current in a halftone cache. */
gx_ht_tile *gx_render_ht(P2(gx_ht_cache *, int));

/* ------ Device halftone management ------ */

/* Release a gx_ht_order by freeing its components. */
/* (Don't free the gx_device_halftone itself.) */
void gx_ht_order_release(P3(gx_ht_order * porder, gs_memory_t * mem, bool free_cache));

/*
 * Install a device halftone in an imager state.
 * Note that this does not read or update the client halftone.
 */
int gx_imager_dev_ht_install(P4(gs_imager_state * pis,
				const gx_device_halftone * pdht,
				gs_halftone_type type,
				const gx_device * dev));

/*
 * Install a new halftone in the graphics state.  Note that we copy the top
 * level of the gs_halftone and the gx_device_halftone, and take ownership
 * of any substructures.
 */
int gx_ht_install(P3(gs_state *, const gs_halftone *,
		     const gx_device_halftone *));

/* Reestablish the effective transfer functions, taking into account */
/* any overrides from halftone dictionaries. */
/* Some compilers object to names longer than 31 characters.... */
void gx_imager_set_effective_xfer(P1(gs_imager_state * pis));
void gx_set_effective_transfer(P1(gs_state * pgs));


#endif /* gzht_INCLUDED */
