/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxctable.h,v 1.2 2000/03/08 23:14:56 mike Exp $ */
/* Interface to color table lookup and interpolation */

#ifndef gxctable_INCLUDED
#  define gxctable_INCLUDED

#include "gxfixed.h"
#include "gxfrac.h"

/*
 * Define a 3- or 4-D color lookup table.
 * n is the number of dimensions (input indices), 3 or 4.
 * dims[0..n-1] are the table dimensions.
 * m is the number of output values, 3 or 4.
 * For n = 3:
 *   table[i], 0 <= i < dims[0], point to strings of length
 *     dims[1] x dims[2] x m.
 * For n = 4:
 *   table[i], 0 <= i < dims[0] x dims[1], points to strings of length
 *     dims[2] x dims[3] x m.
 * It isn't really necessary to store the size of each string, since
 * they're all the same size, but it makes things a lot easier for the GC.
 */
typedef struct gx_color_lookup_table_s {
    int n;
    int dims[4];		/* [ndims] */
    int m;
    const gs_const_string *table;
} gx_color_lookup_table;

/*
 * Interpolate in a 3- or 4-D color lookup table.
 * pi[0..n-1] are the table indices, guaranteed to be in the ranges
 * [0..dims[n]-1] respectively.
 * Return interpolated values in pv[0..m-1].
 */

/* Return the nearest value without interpolation. */
void gx_color_interpolate_nearest(P3(const fixed * pi,
			    const gx_color_lookup_table * pclt, frac * pv));

/* Use trilinear interpolation. */
void gx_color_interpolate_linear(P3(const fixed * pi,
			    const gx_color_lookup_table * pclt, frac * pv));

#endif /* gxctable_INCLUDED */
