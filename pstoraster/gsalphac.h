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

/*$Id: gsalphac.h,v 1.1 2000/03/13 18:57:54 mike Exp $ */
/* Alpha-compositing interface */

#ifndef gsalphac_INCLUDED
#  define gsalphac_INCLUDED

#include "gscompt.h"

/*
 * Define the compositing operations.  These values must match the ones in
 * dpsNeXT.h.
 */
typedef enum {
    composite_Clear = 0,
    composite_Copy,
    composite_Sover,
    composite_Sin,
    composite_Sout,
    composite_Satop,
    composite_Dover,
    composite_Din,
    composite_Dout,
    composite_Datop,
    composite_Xor,
    composite_PlusD,
    composite_PlusL,
#define composite_last composite_PlusL
    composite_Highlight,	/* (only for compositerect) */
#define compositerect_last composite_Highlight
    composite_Dissolve		/* (not for PostScript composite operators) */
#define composite_op_last composite_Dissolve
} gs_composite_op_t;

/*
 * Define parameters for alpha-compositing.
 */
typedef struct gs_composite_alpha_params_s {
    gs_composite_op_t op;
    float delta;		/* only for Dissolve */
} gs_composite_alpha_params_t;

/* Create an alpha-compositing object. */
int gs_create_composite_alpha(P3(gs_composite_t ** ppcte,
				 const gs_composite_alpha_params_t * params,
				 gs_memory_t * mem));

#endif /* gsalphac_INCLUDED */
