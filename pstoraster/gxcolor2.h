/* Copyright (C) 1993, 1995, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxcolor2.h,v 1.2 2000/03/08 23:14:55 mike Exp $ */
/* Internal definitions for Level 2 color routines */
/* Requires gsstruct.h, gxfixed.h */

#ifndef gxcolor2_INCLUDED
#  define gxcolor2_INCLUDED

#include "gscolor2.h"
#include "gsrefct.h"
#include "gxbitmap.h"

/* Cache for Indexed color with procedure, or Separation color. */
struct gs_indexed_map_s {
    rc_header rc;
    union {
	int (*lookup_index) (P3(const gs_indexed_params *, int, float *));
	int (*tint_transform) (P3(const gs_separation_params *, floatp, float *));
    } proc;
    uint num_values;		/* base_space->type->num_components * (hival + 1) */
    float *values;		/* actually [num_values] */
};

extern_st(st_indexed_map);
#define public_st_indexed_map() /* in gscolor2.c */\
  gs_public_st_ptrs1(st_indexed_map, gs_indexed_map, "gs_indexed_map",\
    indexed_map_enum_ptrs, indexed_map_reloc_ptrs, values)

/* Allocate an indexed map and its values. */
int alloc_indexed_map(P4(gs_indexed_map ** ppmap, int num_values,
			 gs_memory_t * mem, client_name_t cname));

/* Free an indexed map and its values when the reference count goes to 0. */
rc_free_proc(free_indexed_map);

/*
 * We define 'tiling space' as the space in which (0,0) is the origin of
 * the key pattern cell and in which coordinate (i,j) is displaced by
 * i * XStep + j * YStep from the origin.  In this space, it is easy to
 * compute a (rectangular) set of tile copies that cover a (rectangular)
 * region to be tiled.  Note that since all we care about is that the
 * stepping matrix (the transformation from tiling space to device space)
 * yield the right set of coordinates for integral X and Y values, we can
 * adjust it to make the tiling computation easier; in particular, we can
 * arrange it so that all 4 transformation factors are non-negative.
 */

/* Implementation of Pattern instances. */
struct gs_pattern_instance_s {
    rc_header rc;
    gs_client_pattern template;
    /* Following are created by makepattern */
    gs_state *saved;
    gs_matrix step_matrix;	/* tiling space -> device space */
    gs_rect bbox;		/* bbox of tile in tiling space */
    bool is_simple;		/* true if xstep/ystep = tile size */
    /*
     * uses_mask is always true for PostScript patterns, but is false
     * for bitmap patterns that don't have explicit transparent pixels.
     */
    bool uses_mask;	        /* if true, pattern mask must be created */
    gs_int_point size;		/* in device coordinates */
    gx_bitmap_id id;		/* key for cached bitmap */
    /* (= id of mask) */
};

/* The following is only public for a type test in the interpreter */
/* (.buildpattern operator). */
extern_st(st_pattern_instance);
#define public_st_pattern_instance() /* in gspcolor.c */\
  gs_public_st_ptrs_add1(st_pattern_instance, gs_pattern_instance,\
    "pattern instance", pattern_instance_enum_ptrs,\
    pattern_instance_reloc_ptrs, st_pattern1_template, template, saved)

#endif /* gxcolor2_INCLUDED */
