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

/*$Id: gxropc.h,v 1.1 2000/03/13 18:58:46 mike Exp $ */
/* Internals for RasterOp compositing */

#ifndef gxropc_INCLUDED
#  define gxropc_INCLUDED

#include "gsropc.h"
#include "gxcomp.h"

/* Define RasterOp-compositing objects. */
typedef struct gs_composite_rop_s {
    gs_composite_common;
    gs_composite_rop_params_t params;
} gs_composite_rop_t;

#define private_st_composite_rop() /* in gsropc.c */\
  gs_private_st_ptrs1(st_composite_rop, gs_composite_rop_t,\
    "gs_composite_rop_t", composite_rop_enum_ptrs, composite_rop_reloc_ptrs,\
    params.texture)

/*
 * Initialize a RasterOp compositing function from parameters.
 * We make this visible so that clients can allocate gs_composite_rop_t
 * objects on the stack, to reduce memory manager overhead.
 */
void gx_init_composite_rop(P2(gs_composite_rop_t * pcte,
			      const gs_composite_rop_params_t * params));

#endif /* gxropc_INCLUDED */
