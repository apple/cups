/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscolor1.h,v 1.2 2000/03/08 23:14:37 mike Exp $ */
/* Client interface to Level 1 extended color facilities */
/* Requires gscolor.h */

#ifndef gscolor1_INCLUDED
#  define gscolor1_INCLUDED

/* Color and gray interface */
int gs_setcmykcolor(P5(gs_state *, floatp, floatp, floatp, floatp)), gs_currentcmykcolor(P2(const gs_state *, float[4])),
      gs_setblackgeneration(P2(gs_state *, gs_mapping_proc)), gs_setblackgeneration_remap(P3(gs_state *, gs_mapping_proc, bool));
gs_mapping_proc gs_currentblackgeneration(P1(const gs_state *));
int gs_setundercolorremoval(P2(gs_state *, gs_mapping_proc)), gs_setundercolorremoval_remap(P3(gs_state *, gs_mapping_proc, bool));
gs_mapping_proc gs_currentundercolorremoval(P1(const gs_state *));

/* Transfer function */
int gs_setcolortransfer(P5(gs_state *, gs_mapping_proc /*red */ ,
		    gs_mapping_proc /*green */ , gs_mapping_proc /*blue */ ,
			   gs_mapping_proc /*gray */ )), gs_setcolortransfer_remap(P6(gs_state *, gs_mapping_proc /*red */ ,
		    gs_mapping_proc /*green */ , gs_mapping_proc /*blue */ ,
					 gs_mapping_proc /*gray */ , bool));
void gs_currentcolortransfer(P2(const gs_state *, gs_mapping_proc[4]));

#endif /* gscolor1_INCLUDED */
