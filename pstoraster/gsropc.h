/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsropc.h,v 1.1 2000/03/13 18:57:56 mike Exp $ */
/* RasterOp-compositing interface */

#ifndef gsropc_INCLUDED
#  define gsropc_INCLUDED

#include "gscompt.h"
#include "gsropt.h"

/*
 * Define parameters for RasterOp-compositing.
 * There are two kinds of RasterOp compositing operations.
 * If texture == 0, the input data are the texture, and the source is
 * implicitly all 0 (black).  If texture != 0, it defines the texture,
 * and the input data are the source.  Note that in the latter case,
 * the client (the caller of gs_create_composite_rop) promises that
 * *texture will not change.
 */

#ifndef gx_device_color_DEFINED
#  define gx_device_color_DEFINED
typedef struct gx_device_color_s gx_device_color;

#endif

typedef struct gs_composite_rop_params_s {
    gs_logical_operation_t log_op;
    const gx_device_color *texture;
} gs_composite_rop_params_t;

/* Create a RasterOp-compositing object. */
int gs_create_composite_rop(P3(gs_composite_t ** ppcte,
			       const gs_composite_rop_params_t * params,
			       gs_memory_t * mem));

#endif /* gsropc_INCLUDED */
