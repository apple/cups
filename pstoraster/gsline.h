/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsline.h */
/* Line parameter and quality definitions */

#ifndef gsline_INCLUDED
#  define gsline_INCLUDED

#include "gslparam.h"

/* Procedures */
int	gs_setlinewidth(P2(gs_state *, floatp));
float	gs_currentlinewidth(P1(const gs_state *));
int	gs_setlinecap(P2(gs_state *, gs_line_cap));
gs_line_cap
	gs_currentlinecap(P1(const gs_state *));
int	gs_setlinejoin(P2(gs_state *, gs_line_join));
gs_line_join
	gs_currentlinejoin(P1(const gs_state *));
int	gs_setmiterlimit(P2(gs_state *, floatp));
float	gs_currentmiterlimit(P1(const gs_state *));
int	gs_setdash(P4(gs_state *, const float *, uint, floatp));
uint	gs_currentdash_length(P1(const gs_state *));
const float *
	gs_currentdash_pattern(P1(const gs_state *));
float	gs_currentdash_offset(P1(const gs_state *));
int	gs_setflat(P2(gs_state *, floatp));
float	gs_currentflat(P1(const gs_state *));
int	gs_setstrokeadjust(P2(gs_state *, bool));
bool	gs_currentstrokeadjust(P1(const gs_state *));

/* Imager-level procedures */
#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif
int	gs_imager_setflat(P2(gs_imager_state *, floatp));

#endif					/* gsline_INCLUDED */
