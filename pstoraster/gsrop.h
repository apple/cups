/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gsrop.h,v 1.2 2000/03/08 23:14:47 mike Exp $ */
/* RasterOp / transparency procedure interface */

#ifndef gsrop_INCLUDED
#  define gsrop_INCLUDED

#include "gsropt.h"

/* Procedural interface */

int gs_setrasterop(P2(gs_state *, gs_rop3_t));
gs_rop3_t gs_currentrasterop(P1(const gs_state *));
int gs_setsourcetransparent(P2(gs_state *, bool));
bool gs_currentsourcetransparent(P1(const gs_state *));
int gs_settexturetransparent(P2(gs_state *, bool));
bool gs_currenttexturetransparent(P1(const gs_state *));

/* Save/restore the combined logical operation. */
gs_logical_operation_t gs_current_logical_op(P1(const gs_state *));
int gs_set_logical_op(P2(gs_state *, gs_logical_operation_t));

#endif /* gsrop_INCLUDED */
