/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gzstate.h */
/* Private graphics state definition for Ghostscript library */
#include "gxdcolor.h"
#include "gxistate.h"
#include "gsstate.h"
#include "gxstate.h"
#include "gxcvalue.h"
#include "gxtmap.h"

/* Opaque types referenced by the graphics state. */
#ifndef gs_font_DEFINED
#  define gs_font_DEFINED
typedef struct gs_font_s gs_font;
#endif
#ifndef gs_halftone_DEFINED
#  define gs_halftone_DEFINED
typedef struct gs_halftone_s gs_halftone;
#endif
#ifndef gx_device_halftone_DEFINED
#  define gx_device_halftone_DEFINED
typedef struct gx_device_halftone_s gx_device_halftone;
#endif

/* Composite components of the graphics state. */
typedef struct gx_transfer_colored_s {
	/* The components must be in this order: */
	gx_transfer_map *red;
	gx_transfer_map *green;
	gx_transfer_map *blue;
	gx_transfer_map *gray;
} gx_transfer_colored;
typedef union gx_transfer_s {
	gx_transfer_map *indexed[4];
	gx_transfer_colored colored;
} gx_transfer;

/*
 * We allocate a large subset of the graphics state as a single object.
 * Its type is opaque here, defined in gsstate.c.
 * The components marked with @ must be allocated in the contents.
 * (Consult gsstate.c for more details about gstate storage management.)
 */
typedef struct gs_state_contents_s gs_state_contents;

/* Graphics state structure. */

struct gs_state_s {
	gs_imager_state_common;		/* imager state, must be first */
	gs_memory_t *memory;
	gs_state *saved;		/* previous state from gsave */
	gs_state_contents *contents;

		/* Transformation: */

	gs_matrix ctm_inverse;
	bool ctm_inverse_valid;		/* true if ctm_inverse = ctm^-1 */
	gs_matrix ctm_default;
	bool ctm_default_set;		/* if true, use ctm_default; */
					/* if false, ask device */

		/* Paths: */

	struct gx_path_s *path;
	struct gx_clip_path_s *clip_path;	/* @ */
	int clip_rule;

		/* Halftone screen: */

	gs_halftone *halftone;
	gx_device_halftone *dev_ht;
	gs_int_point ht_phase;
	struct gx_ht_cache_s *ht_cache;	/* shared by all gstates */

		/* Color (device-independent): */

	struct gs_color_space_s *color_space;
	struct gs_client_color_s *ccolor;
	gx_color_value alpha;

		/* Color (device-dependent): */

	struct gs_cie_render_s *cie_render;
	gx_transfer_map *black_generation;	/* may be 0 */
	gx_transfer_map *undercolor_removal;	/* may be 0 */
		/* set_transfer holds the transfer functions specified by */
		/* set[color]transfer; effective_transfer includes the */
		/* effects of overrides by TransferFunctions in halftone */
		/* dictionaries.  (In Level 1 systems, set_transfer and */
		/* effective_transfer are always the same.) */
	gx_transfer set_transfer;
	gx_transfer effective_transfer;

		/* Color caches: */

	gx_device_color *dev_color;
	struct gx_cie_joint_caches_s *cie_joint_caches;
	const struct gx_color_map_procs_s *cmap_procs;
	struct gx_pattern_cache_s *pattern_cache; /* shared by all GCs */

		/* Font: */

	gs_font *font;
	gs_font *root_font;
	gs_matrix_fixed char_tm;	/* font matrix * ctm */
#define char_tm_only(pgs) *(gs_matrix *)&(pgs)->char_tm
	bool char_tm_valid;		/* true if char_tm is valid */
	byte in_cachedevice;		/* 0 if not in setcachedevice, */
					/* 1 if in setcachdevice but not */
					/* actually caching, */
					/* 2 if in setcachdevice and */
					/* actually caching */
	byte /*gs_char_path_mode*/ in_charpath;	/* (see gscpm.h) */
	gs_state *show_gstate;		/* gstate when show was invoked */
					/* (so charpath can append to path) */

		/* Other stuff: */

	int level;			/* incremented by 1 per gsave */
	gx_device *device;
#undef gs_currentdevice_inline
#define gs_currentdevice_inline(pgs) ((pgs)->device)

		/* Client data: */

	void *client_data;
#define gs_state_client_data(pgs) ((pgs)->client_data)
	gs_state_client_procs client_procs;
};
#define private_st_gs_state()	/* in gsstate.c */\
  gs_private_st_composite(st_gs_state, gs_state, "gs_state",\
    gs_state_enum_ptrs, gs_state_reloc_ptrs)
