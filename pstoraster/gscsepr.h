/* Copyright (C) 1992, 1993, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gscsepr.h,v 1.1 2000/03/13 18:57:55 mike Exp $ */
/* Client interface to Separation color */

#ifndef gscsepr_INCLUDED
#  define gscsepr_INCLUDED

#include "gscspace.h"

/* Graphics state */
bool gs_currentoverprint(P1(const gs_state *));
void gs_setoverprint(P2(gs_state *, bool));

/*
 * Separation color spaces.
 *
 * The API for creating Separation color space objects exposes the fact that
 * they normally cache the results of sampling the tint_transform procedure,
 * and use the cache to convert colors when necessary.  When a language
 * interpreter sets up a Separation space, it may either provide a
 * tint_tranform procedure that will be called each time (specifying the
 * cache size as 0), or it may fill in the cache directly and provide a
 * dummy procedure.
 *
 * By default, the tint transformation procedure will simple return the
 * entries in the cache. If this function is called when the cache size is
 * 0, all color components in the alternative color space will be set to 0.
 */
extern int gs_cspace_build_Separation(
					 gs_color_space ** ppcspace,
					 gs_separation_name sname,
					 const gs_color_space * palt_cspace,
					 int cache_size,
					 gs_memory_t * pmem
);

/* Get the cached value array for a Separation color space. */
extern float *gs_separation_value_array(P1(
					      const gs_color_space * pcspace
					));

/* Set the tint transformation procedure for a separation color space. */
extern int gs_cspace_set_tint_transform_proc(P2(
						   gs_color_space * pcspace,
			       int (*proc) (P3(const gs_separation_params *,
					       floatp,
					       float *
					    ))
					     ));

#endif /* gscsepr_INCLUDED */
