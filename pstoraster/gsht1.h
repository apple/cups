/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gsht1.h */
/* Extended public interface to halftones */

#ifndef gsht1_INCLUDED
#  define gsht1_INCLUDED

#include "gsht.h"

/* Procedural interface */
int	gs_setcolorscreen(P2(gs_state *, gs_colorscreen_halftone *));
int	gs_currentcolorscreen(P2(gs_state *, gs_colorscreen_halftone *));

/* We include sethalftone here, even though it is a Level 2 feature, */
/* because it turns out to be convenient to define setcolorscreen */
/* using sethalftone. */
int	gs_sethalftone(P2(gs_state *, gs_halftone *));
int	gs_currenthalftone(P2(gs_state *, gs_halftone *));

#endif					/* gsht1_INCLUDED */
