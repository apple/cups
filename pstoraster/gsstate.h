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

/* gsstate.h */
/* Public graphics state API */

#ifndef gsstate_INCLUDED
#  define gsstate_INCLUDED

/* Opaque type for a graphics state */
#ifndef gs_state_DEFINED
#  define gs_state_DEFINED
typedef struct gs_state_s gs_state;
#endif

/* Initial allocation and freeing */
gs_state *gs_state_alloc(P1(gs_memory_t *)); /* 0 if fails */
int gs_state_free(P1(gs_state *));

/* Initialization, saving, restoring, and copying */
int	gs_gsave(P1(gs_state *)),
	gs_grestore(P1(gs_state *)),
	gs_grestoreall(P1(gs_state *));
gs_state *gs_gstate(P1(gs_state *));
int	gs_copygstate(P2(gs_state * /*to*/, const gs_state * /*from*/)),
	gs_currentgstate(P2(gs_state * /*to*/, const gs_state * /*from*/)),
	gs_setgstate(P2(gs_state * /*to*/, const gs_state * /*from*/));
int	gs_initgraphics(P1(gs_state *));

/* Device control */
#include "gsdevice.h"

/* Line parameters and quality */
#include "gsline.h"

/* Color and gray */
#include "gscolor.h"

/* Halftone screen */
#include "gsht.h"
int	gs_sethalftonephase(P3(gs_state *, int, int));
int	gs_currenthalftonephase(P2(const gs_state *, gs_int_point *));

/* Miscellaneous */
int	gs_setfilladjust(P3(gs_state *, floatp, floatp));
int	gs_currentfilladjust(P2(const gs_state *, gs_point *));

#endif					/* gsstate_INCLUDED */
