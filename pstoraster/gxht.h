/* Copyright (C) 1993, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxht.h,v 1.2 2000/03/08 23:15:00 mike Exp $ */
/* Rest of (client) halftone definitions */

#ifndef gxht_INCLUDED
#  define gxht_INCLUDED

#include "gscsepnm.h"
#include "gsht1.h"
#include "gsrefct.h"
#include "gxhttype.h"
#include "gxtmap.h"

/*
 * Halftone types. Note that for this implementation there are only
 * spot functions, thresholds, and multi-component halftones; the peculiar
 * colored halftones supported by PostScript (HalftoneType's 2 and 4) are
 * not supported.
 *
 * NB1: While this code supports relocation of the client data, it will not
 *      free that data when the halftone is released. The client must handle
 *      that task directly.
 *
 * NB2: The garbage collection code will deal with the user provided data as
 *      a structure pointer allocated on the heap. The client must make
 *      certain this is the case.
 *
 * There is, somewhat unfortunately, no identifier applied to these
 * halftones. This reflects the origin of this graphics library as a set
 * of routines for use by a PostScript interpreter.
 *
 * In PostScript, halftone objects do not exist in an identified form outside
 * of the graphic state. Though Level 2 and PostScript 3 support halftone
 * dictionaries, these are neither read-only structures nor tagged
 * by a unique identifier. Hence, they are not suitable for use as cache keys.
 * Caching of halftones for PostScript is confined to the graphic state,
 * and this holds true for the graphic library as well.
 *
 * Note also that implementing a generalized halftone cache is not trivial,
 * as the device-specific representation of spot halftones depends on the 
 * default transformation for the device, and more generally the device
 * specific representation of halftones may depend on the sense of the device
 * (additive or subtract). Hence, a halftone cache would need to be keyed
 * by device. (This is not an issue when caching halftones in the graphic
 * state as the device is also a component of the graphic state).
 */

/*
 * Note that the transfer_closure members will replace transfer sometime
 * in the future.  For the moment, transfer_closure is only used if
 * transfer = 0.
 */

/* Type 1 halftone.  This is just a Level 1 halftone with */
/* a few extra members. */
typedef struct gs_spot_halftone_s {
    gs_screen_halftone screen;
    bool accurate_screens;
    gs_mapping_proc transfer;	/* OBSOLETE */
    gs_mapping_closure_t transfer_closure;
} gs_spot_halftone;

#define st_spot_halftone_max_ptrs st_screen_halftone_max_ptrs + 1

/* Type 3 halftone. */
typedef struct gs_threshold_halftone_s {
    int width;
    int height;
    gs_const_string thresholds;
    gs_mapping_proc transfer;	/* OBSOLETE */
    gs_mapping_closure_t transfer_closure;
} gs_threshold_halftone;

#define st_threshold_halftone_max_ptrs 2

/* Client-defined halftone that generates a halftone order. */
typedef struct gs_client_order_halftone_s gs_client_order_halftone;

#ifndef gx_ht_order_DEFINED
#  define gx_ht_order_DEFINED
typedef struct gx_ht_order_s gx_ht_order;

#endif
typedef struct gs_client_order_ht_procs_s {

    /*
     * Allocate and fill in the order.  gx_ht_alloc_client_order
     * (see gzht.h) does everything but fill in the actual data.
     */

    int (*create_order) (P4(gx_ht_order * porder,
			    gs_state * pgs,
			    const gs_client_order_halftone * phcop,
			    gs_memory_t * mem));

} gs_client_order_ht_procs_t;
struct gs_client_order_halftone_s {
    int width;
    int height;
    int num_levels;
    const gs_client_order_ht_procs_t *procs;
    const void *client_data;
    gs_mapping_closure_t transfer_closure;
};

#define st_client_order_halftone_max_ptrs 2

/* Define the elements of a Type 5 halftone. */
typedef struct gs_halftone_component_s {
    gs_ht_separation_name cname;
    gs_halftone_type type;
    union {
	gs_spot_halftone spot;	/* Type 1 */
	gs_threshold_halftone threshold;	/* Type 3 */
	gs_client_order_halftone client_order;	/* client order */
    } params;
} gs_halftone_component;

extern_st(st_halftone_component);
#define public_st_halftone_component()	/* in gsht1.c */\
  gs_public_st_composite(st_halftone_component, gs_halftone_component,\
    "gs_halftone_component", halftone_component_enum_ptrs,\
    halftone_component_reloc_ptrs)
extern_st(st_ht_component_element);
#define public_st_ht_component_element() /* in gsht1.c */\
  gs_public_st_element(st_ht_component_element, gs_halftone_component,\
    "gs_halftone_component[]", ht_comp_elt_enum_ptrs, ht_comp_elt_reloc_ptrs,\
    st_halftone_component)
#define st_halftone_component_max_ptrs\
  max(max(st_spot_halftone_max_ptrs, st_threshold_halftone_max_ptrs),\
      st_client_order_halftone_max_ptrs)

/* Define the Type 5 halftone itself. */
typedef struct gs_multiple_halftone_s {
    gs_halftone_component *components;
    uint num_comp;
} gs_multiple_halftone;

#define st_multiple_halftone_max_ptrs 1

/*
 * The halftone stored in the graphics state is the union of
 * setscreen, setcolorscreen, Type 1, Type 3, and Type 5.
 *
 * NOTE: it is assumed that all subsidiary structures of halftones (the
 * threshold array(s) for Type 3 halftones or halftone components, and the
 * components array for Type 5 halftones) are allocated with the same
 * allocator as the halftone structure itself.
 */
struct gs_halftone_s {
    gs_halftone_type type;
    rc_header rc;
    union {
	gs_screen_halftone screen;	/* setscreen */
	gs_colorscreen_halftone colorscreen;	/* setcolorscreen */
	gs_spot_halftone spot;	/* Type 1 */
	gs_threshold_halftone threshold;	/* Type 3 */
	gs_client_order_halftone client_order;	/* client order */
	gs_multiple_halftone multiple;	/* Type 5 */
    } params;
};

extern_st(st_halftone);
#define public_st_halftone()	/* in gsht.c */\
  gs_public_st_composite(st_halftone, gs_halftone, "gs_halftone",\
    halftone_enum_ptrs, halftone_reloc_ptrs)
#define st_halftone_max_ptrs\
  max(max(st_screen_halftone_max_ptrs, st_colorscreen_halftone_max_ptrs),\
      max(max(st_spot_halftone_max_ptrs, st_threshold_halftone_max_ptrs),\
	  max(st_client_order_halftone_max_ptrs,\
	      st_multiple_halftone_max_ptrs)))

/* Procedural interface for AccurateScreens */

/*
 * Set/get the default AccurateScreens value (for set[color]screen).
 * Note that this value is stored in a static variable.
 */
void gs_setaccuratescreens(P1(bool));
bool gs_currentaccuratescreens(P0());

/* Initiate screen sampling with optional AccurateScreens. */
int gs_screen_init_memory(P5(gs_screen_enum *, gs_state *,
			     gs_screen_halftone *, bool, gs_memory_t *));

#define gs_screen_init_accurate(penum, pgs, phsp, accurate)\
  gs_screen_init_memory(penum, pgs, phsp, accurate, pgs->memory)

/* Procedural interface for MinScreenLevels (a Ghostscript extension) */

/*
 * Set/get the MinScreenLevels value.
 *
 * Note that this value is stored in a static variable.
 */
void gs_setminscreenlevels(P1(uint));
uint gs_currentminscreenlevels(P0());

#endif /* gxht_INCLUDED */
