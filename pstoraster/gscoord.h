/* Copyright (C) 1989, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gscoord.h */
/* Client interface to coordinate system operations */
/* Requires gsmatrix.h and gsstate.h */

/* Coordinate system modification */
int	gs_initmatrix(P1(gs_state *)),
	gs_defaultmatrix(P2(const gs_state *, gs_matrix *)),
	gs_currentmatrix(P2(const gs_state *, gs_matrix *)),
	gs_setmatrix(P2(gs_state *, const gs_matrix *)),
	gs_translate(P3(gs_state *, floatp, floatp)),
	gs_scale(P3(gs_state *, floatp, floatp)),
	gs_rotate(P2(gs_state *, floatp)),
	gs_concat(P2(gs_state *, const gs_matrix *));
/* Extensions */
int	gs_setdefaultmatrix(P2(gs_state *, const gs_matrix *)),
	gs_currentcharmatrix(P3(gs_state *, gs_matrix *, bool)),
	gs_setcharmatrix(P2(gs_state *, const gs_matrix *)),
	gs_settocharmatrix(P1(gs_state *));

/* Coordinate transformation */
int	gs_transform(P4(gs_state *, floatp, floatp, gs_point *)),
	gs_dtransform(P4(gs_state *, floatp, floatp, gs_point *)),
	gs_itransform(P4(gs_state *, floatp, floatp, gs_point *)),
	gs_idtransform(P4(gs_state *, floatp, floatp, gs_point *));

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

int	gs_imager_idtransform(P4(const gs_imager_state *, floatp, floatp,
				 gs_point *));
