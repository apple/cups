/* Copyright (C) 1993, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxpcolor.h,v 1.2 2000/03/08 23:15:04 mike Exp $ */
/* Requires gsmatrix.h, gxdevice.h, gxdevmem.h, gxcolor2.h, gxdcolor.h */

#ifndef gxpcolor_INCLUDED
#  define gxpcolor_INCLUDED

#include "gxpcache.h"

/*
 * Define the Pattern device color types.  There is one type for
 * colored patterns, and one uncolored pattern type for each non-Pattern
 * device color type.
 */
extern const gx_device_color_type_t
      gx_dc_pattern, gx_dc_pure_masked, gx_dc_binary_masked, gx_dc_colored_masked;

#define gx_dc_type_pattern (&gx_dc_pattern)

/*
 * Define a color tile, an entry in the rendered Pattern cache (and
 * eventually in the colored halftone cache).  Note that the depth is
 * not sufficient to ensure that the rendering matches a given device;
 * however, we don't currently have an object that represents the
 * abstraction of a 'color representation'.
 */
struct gx_color_tile_s {
    /* ------ The following are the 'key' in the cache. ------ */
    /* Note that the id is a generated instance ID, */
    /* and has no relation to the template's gs_uid. */
    gx_bitmap_id id;
    int depth;
    /* We do, however, copy the template's gs_uid, */
    /* for use in selective cache purging. */
    gs_uid uid;
    /* ------ The following are the cache 'value'. ------ */
    /* Note that if tbits and tmask both have data != 0, */
    /* both must have the same rep_shift. */
/****** NON-ZERO shift VALUES ARE NOT SUPPORTED YET. ******/
    int tiling_type;		/* TilingType */
    gs_matrix step_matrix;	/* tiling space -> device space, */
    /* see gxcolor2.h for details */
    gs_rect bbox;		/* bbox of tile in tiling space */
    gx_strip_bitmap tbits;	/* data = 0 if uncolored */
    gx_strip_bitmap tmask;	/* data = 0 if no mask */
    /* (i.e., the mask is all 1's) */
    bool is_simple;		/* true if xstep/ystep = tile size */
    /* The following is neither key nor value. */
    uint index;			/* the index of the tile within */
    /* the cache (for GC) */
};

#define private_st_color_tile()	/* in gxpcmap.c */\
  gs_private_st_ptrs2(st_color_tile, gx_color_tile, "gx_color_tile",\
    color_tile_enum_ptrs, color_tile_reloc_ptrs, tbits.data, tmask.data)
#define private_st_color_tile_element()	/* in gxpcmap.c */\
  gs_private_st_element(st_color_tile_element, gx_color_tile,\
    "gx_color_tile[]", color_tile_elt_enum_ptrs, color_tile_elt_reloc_ptrs,\
    st_color_tile)

/* Define the Pattern cache. */
				/*#include "gxpcache.h" *//* (above) */

/* Allocate a Pattern cache. */
/* We shorten the procedure names because some VMS compilers */
/* truncate names to 23 characters. */
uint gx_pat_cache_default_tiles(P0());
ulong gx_pat_cache_default_bits(P0());
gx_pattern_cache *gx_pattern_alloc_cache(P3(gs_memory_t *, uint, ulong));

/* Get or set the Pattern cache in a gstate. */
gx_pattern_cache *gstate_pattern_cache(P1(gs_state *));
void gstate_set_pattern_cache(P2(gs_state *, gx_pattern_cache *));

/*
 * Define a device for accumulating the rendering of a Pattern.
 * This is actually a wrapper for two other devices: one that accumulates
 * the actual pattern image (if this is a colored pattern), and one that
 * accumulates a mask defining which pixels in the image are set.
 */
typedef struct gx_device_pattern_accum_s {
    gx_device_forward_common;
    /* Client sets these before opening */
    gs_memory_t *bitmap_memory;
    const gs_pattern_instance *instance;
    /* open sets these */
    gx_device_memory *bits;	/* target also points to bits */
    gx_device_memory *mask;
} gx_device_pattern_accum;

#define private_st_device_pattern_accum() /* in gxpcmap.c */\
  gs_private_st_suffix_add3(st_device_pattern_accum, gx_device_pattern_accum,\
    "pattern accumulator", pattern_accum_enum, pattern_accum_reloc,\
    st_device_forward, instance, bits, mask)

/* Allocate a pattern accumulator. */
gx_device_pattern_accum *gx_pattern_accum_alloc(P2(gs_memory_t * memory, client_name_t));

/* Add an accumulated pattern to the cache. */
/* Note that this does not free any of the data in the accumulator */
/* device, but it may zero out the bitmap_memory pointers to prevent */
/* the accumulated bitmaps from being freed when the device is closed. */
int gx_pattern_cache_add_entry(P3(gs_imager_state *, gx_device_pattern_accum *,
				  gx_color_tile **));

/* Look up a pattern color in the cache. */
bool gx_pattern_cache_lookup(P4(gx_device_color *, const gs_imager_state *,
				gx_device *, gs_color_select_t));

/* Purge selected entries from the pattern cache. */
void gx_pattern_cache_winnow(P3(gx_pattern_cache *,
				bool (*)(P2(gx_color_tile *, void *)),
				void *));

#endif /* gxpcolor_INCLUDED */
