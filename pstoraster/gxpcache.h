/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxpcache.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Definition of Pattern cache */

#ifndef gxpcache_INCLUDED
#  define gxpcache_INCLUDED

/*
 * Define a cache for rendered Patterns.  This is currently an open
 * hash table with single probing (no reprobing) and round-robin
 * replacement.  Obviously, we can do better in both areas.
 */
#ifndef gx_pattern_cache_DEFINED
#  define gx_pattern_cache_DEFINED
typedef struct gx_pattern_cache_s gx_pattern_cache;

#endif
#ifndef gx_color_tile_DEFINED
#  define gx_color_tile_DEFINED
typedef struct gx_color_tile_s gx_color_tile;

#endif
struct gx_pattern_cache_s {
    gs_memory_t *memory;
    gx_color_tile *tiles;
    uint num_tiles;
    uint tiles_used;
    uint next;			/* round-robin index */
    ulong bits_used;
    ulong max_bits;
    void (*free_all) (P1(gx_pattern_cache *));
};

#define private_st_pattern_cache() /* in gxpcmap.c */\
  gs_private_st_ptrs1(st_pattern_cache, gx_pattern_cache,\
    "gx_pattern_cache", pattern_cache_enum, pattern_cache_reloc, tiles)

#endif /* gxpcache_INCLUDED */
