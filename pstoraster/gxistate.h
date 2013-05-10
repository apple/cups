/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxistate.h */
/* Imager state definition */

#ifndef gxistate_INCLUDED
#  define gxistate_INCLUDED

#include "gsropt.h"
#include "gxfixed.h"
#include "gxline.h"
#include "gxmatrix.h"

/*
 * Define the subset of the PostScript graphics state that the imager
 * library API needs.  The definition of this subset is subject to change
 * as we come to understand better the boundary between the imager and
 * the interpreter.  In particular, the imager state currently INCLUDES
 * the following:
 *	line parameters: cap, join, miter limit, dash pattern
 *	transformation matrix (CTM)
 *	logical operation: RasterOp, transparency, rendering algorithm
 *	overprint flag
 *	rendering tweaks: flatness, fill adjustment, stroke adjust flag
 * The imager state currently EXCLUDES the following:
 *	graphics state stack
 *	default CTM
 *	path
 *	clipping path
 *	color specification: color, color space, alpha
 *	color rendering information: halftone, halftone phase,
 *	   transfer functions, black generation, undercolor removal,
 *	   CIE rendering tables
 *	font
 *	device
 *	caches for many of the above
 */

#define gs_imager_state_common\
	gx_line_params line_params;\
	gs_matrix_fixed ctm;\
	gs_logical_operation_t log_op;\
	bool overprint;\
	float flatness;\
	gs_fixed_point fill_adjust;	/* fattening for fill */\
	bool stroke_adjust
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
   { gx_line_params_initial },\
   { scale, 0.0, 0.0, -(scale), 0.0, 0.0 },\
  lop_default, 0/*false*/, 1.0, { fixed_half, fixed_half }, 0/*false*/

#define private_st_imager_state()	/* in gsstate.c */\
  gs_private_st_ptrs_add0(st_imager_state, gs_imager_state, "gs_imager_state",\
    imager_state_enum, imager_state_reloc, st_line_params, line_params)

#endif					/* gxistate_INCLUDED */
