/* Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxistate.h,v 1.2 2000/03/08 23:15:02 mike Exp $ */
/* Imager state definition */

#ifndef gxistate_INCLUDED
#  define gxistate_INCLUDED

#include "gscsel.h"
#include "gsrefct.h"
#include "gsropt.h"
#include "gxcvalue.h"
#include "gxfixed.h"
#include "gxline.h"
#include "gxmatrix.h"
#include "gxtmap.h"

/*
 * Define the subset of the PostScript graphics state that the imager
 * library API needs.  The definition of this subset is subject to change
 * as we come to understand better the boundary between the imager and
 * the interpreter.  In particular, the imager state currently INCLUDES
 * the following:
 *      line parameters: cap, join, miter limit, dash pattern
 *      transformation matrix (CTM)
 *      logical operation: RasterOp, transparency
 *      color modification: alpha, rendering algorithm
 *      overprint flag
 *      rendering tweaks: flatness, fill adjustment, stroke adjust flag,
 *        accurate curves flag, shading smoothness
 *      color rendering information:
 *          halftone, halftone phases
 *          transfer functions
 *          black generation, undercolor removal
 *          CIE rendering tables
 *          halftone and pattern caches
 * The imager state currently EXCLUDES the following:
 *      graphics state stack
 *      default CTM
 *      path
 *      clipping path
 *      color specification: color, color space
 *      font
 *      device
 *      caches for many of the above
 */

/*
 * Define the color rendering state information.
 * This should be a separate object (or at least a substructure),
 * but doing this would require editing too much code.
 */

/* Opaque types referenced by the color rendering state. */
#ifndef gs_halftone_DEFINED
#  define gs_halftone_DEFINED
typedef struct gs_halftone_s gs_halftone;

#endif
#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;

#endif
#ifndef gx_device_halftone_DEFINED
#  define gx_device_halftone_DEFINED
typedef struct gx_device_halftone_s gx_device_halftone;

#endif

/*
 * We need some special memory management for the components of a
 * c.r. state, as indicated by the following notations on the elements:
 *      (RC) means the element is reference-counted.
 *      (Shared) means the element is shared among an arbitrary number of
 *        c.r. states and is never freed.
 *      (Owned) means exactly one c.r. state references the element,
 *        and it is guaranteed that no references to it will outlive
 *        the c.r. state itself.
 */

/* Define the interior structure of a transfer function. */
typedef struct gx_transfer_colored_s {
    /* The components must be in this order: */
    gx_transfer_map *red;	/* (RC) */
    gx_transfer_map *green;	/* (RC) */
    gx_transfer_map *blue;	/* (RC) */
    gx_transfer_map *gray;	/* (RC) */
} gx_transfer_colored;
typedef union gx_transfer_s {
    gx_transfer_map *indexed[4];	/* (RC) */
    gx_transfer_colored colored;
} gx_transfer;

#define gs_color_rendering_state_common\
\
		/* Halftone screen: */\
\
	gs_halftone *halftone;			/* (RC) */\
	gs_int_point screen_phase[gs_color_select_count];\
		/* dev_ht depends on halftone and device resolution. */\
	gx_device_halftone *dev_ht;		/* (Owned) */\
		/* The contents of ht_cache depend on dev_ht. */\
	struct gx_ht_cache_s *ht_cache;		/* (Shared) by all gstates */\
\
		/* Color (device-dependent): */\
\
	struct gs_cie_render_s *cie_render;	/* (RC) may be 0 */\
	gx_transfer_map *black_generation;	/* (RC) may be 0 */\
	gx_transfer_map *undercolor_removal;	/* (RC) may be 0 */\
		/* set_transfer holds the transfer functions specified by */\
		/* set[color]transfer; effective_transfer includes the */\
		/* effects of overrides by TransferFunctions in halftone */\
		/* dictionaries.  (In Level 1 systems, set_transfer and */\
		/* effective_transfer are always the same.) */\
	gx_transfer set_transfer;		/* members are (RC) */\
	gx_transfer effective_transfer;		/* see below */\
\
		/* Color caches: */\
\
		/* cie_joint_caches depend on cie_render and */\
		/* the color space. */\
	struct gx_cie_joint_caches_s *cie_joint_caches;		/* (RC) */\
		/* cmap_procs depend on the device's color_info. */\
	const struct gx_color_map_procs_s *cmap_procs;		/* static */\
		/* The contents of pattern_cache depend on the */\
		/* the color space and the device's color_info and */\
		/* resolution. */\
	struct gx_pattern_cache_s *pattern_cache	/* (Shared) by all gstates */

/*
 * Enumerate the reference-counted pointers in a c.r. state.  Note that
 * effective_transfer doesn't contribute to the reference count: it points
 * either to the same objects as set_transfer, or to objects in a halftone
 * structure that someone else worries about.
 */
#define gs_cr_state_do_rc_ptrs(m)\
  m(halftone) m(cie_render) m(black_generation) m(undercolor_removal)\
  m(set_transfer.colored.red) m(set_transfer.colored.green)\
  m(set_transfer.colored.blue) m(set_transfer.colored.gray)\
  m(cie_joint_caches)

/* Enumerate the pointers in a c.r. state. */
#define gs_cr_state_do_ptrs(m)\
  m(0,halftone) m(1,dev_ht) m(2,ht_cache)\
  m(3,cie_render) m(4,black_generation) m(5,undercolor_removal)\
  m(6,set_transfer.colored.red) m(7,set_transfer.colored.green)\
  m(8,set_transfer.colored.blue) m(9,set_transfer.colored.gray)\
  m(10,effective_transfer.colored.red) m(11,effective_transfer.colored.green)\
  m(12,effective_transfer.colored.blue) m(13,effective_transfer.colored.gray)\
  m(14,cie_joint_caches) m(15,pattern_cache)
#define st_cr_state_num_ptrs 16

/*
 * Define constant values that can be allocated once and shared among
 * all imager states in an address space.
 */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;

#endif
typedef struct gs_imager_state_shared_s {
    rc_header rc;
    gs_color_space *cs_DeviceGray;
    gs_color_space *cs_DeviceRGB;
    gs_color_space *cs_DeviceCMYK;
} gs_imager_state_shared_t;

#define private_st_imager_state_shared()	/* in gsstate.c */\
  gs_private_st_ptrs3(st_imager_state_shared, gs_imager_state_shared_t,\
    "gs_imager_state_shared", imager_state_shared_enum_ptrs,\
    imager_state_shared_reloc_ptrs, cs_DeviceGray, cs_DeviceRGB, cs_DeviceCMYK)

/* Define the imager state structure itself. */
#define gs_imager_state_common\
	gs_memory_t *memory;\
	gs_imager_state_shared_t *shared;\
	gx_line_params line_params;\
	gs_matrix_fixed ctm;\
	gs_logical_operation_t log_op;\
	gx_color_value alpha;\
	bool overprint;\
	float flatness;\
	gs_fixed_point fill_adjust;	/* fattening for fill */\
	bool stroke_adjust;\
	bool accurate_curves;\
	float smoothness;\
	gs_color_rendering_state_common
#define gs_imager_state_shared(pis, elt) ((pis)->shared->elt)
#define st_imager_state_num_ptrs\
  (st_line_params_num_ptrs + st_cr_state_num_ptrs + 1)
/* Access macros */
#define ctm_only(pis) (*(const gs_matrix *)&(pis)->ctm)
#define ctm_only_writable(pis) (*(gs_matrix *)&(pis)->ctm)
#define set_ctm_only(pis, mat) (*(gs_matrix *)&(pis)->ctm = (mat))
#define gs_init_rop(pis) ((pis)->log_op = lop_default)
#define gs_currentlineparams_inline(pis) (&(pis)->line_params)

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;

#endif

struct gs_imager_state_s {
    gs_imager_state_common;
};

/* Initialization for gs_imager_state */
#define gs_imager_state_initial(scale)\
  0, 0, { gx_line_params_initial },\
   { scale, 0.0, 0.0, -(scale), 0.0, 0.0 },\
  lop_default, gx_max_color_value, 0/*false*/, 1.0,\
   { fixed_half, fixed_half }, 0/*false*/, 0/*false*/, 1.0

#define private_st_imager_state()	/* in gsstate.c */\
  gs_private_st_composite(st_imager_state, gs_imager_state, "gs_imager_state",\
    imager_state_enum_ptrs, imager_state_reloc_ptrs)

/* Initialize an imager state, other than the parts covered by */
/* gs_imager_state_initial. */
/* The halftone, dev_ht, and ht_cache elements are not set or used. */
int gs_imager_state_initialize(P2(gs_imager_state * pis, gs_memory_t * mem));

/* Release an imager state. */
void gs_imager_state_release(P1(gs_imager_state * pis));

#endif /* gxistate_INCLUDED */
