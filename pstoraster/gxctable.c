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

/*$Id: gxctable.c,v 1.2 2000/03/08 23:14:56 mike Exp $ */
/* Color table lookup and interpolation */
#include "gx.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxctable.h"

/* See gxctable.h for the API and structure definitions. */

/*
 * Define an implementation that simply picks the nearest value without
 * any interpolation.
 */
void
gx_color_interpolate_nearest(const fixed * pi,
			     const gx_color_lookup_table * pclt, frac * pv)
{
    const int *pdim = pclt->dims;
    int m = pclt->m;
    const gs_const_string *table = pclt->table;

    if (pclt->n > 3) {
	table += fixed2int_var_rounded(pi[0]) * pdim[1];
	++pi, ++pdim;
    } {
	int ic = fixed2int_var_rounded(pi[2]);
	int ib = fixed2int_var_rounded(pi[1]);
	int ia = fixed2int_var_rounded(pi[0]);
	const byte *p = pclt->table[ia].data + (ib * pdim[2] + ic) * m;
	int j;

	for (j = 0; j < m; ++j, ++p)
	    pv[j] = byte2frac(*p);
    }
}

/*
 * Define an implementation that uses trilinear interpolation.
 */
void
gx_color_interpolate_linear(const fixed * pi,
			    const gx_color_lookup_table * pclt, frac * pv)
{
    const int *pdim = pclt->dims;
    int m = pclt->m;

    if (pclt->n > 3) {		/* Do two 3-D interpolations, */
	/* and then interpolate between them. */
	gx_color_lookup_table clt3;
	frac vx[4];
	int ix = fixed2int_var(pi[0]);
	fixed fx = fixed_fraction(pi[0]);
	int j;

	clt3.n = 3;
	/*clt3.dims[0] = pdim[1]; *//* not used */
	clt3.dims[1] = pdim[2];
	clt3.dims[2] = pdim[3];
	clt3.m = m;
	clt3.table = pclt->table + ix * pdim[1];
	gx_color_interpolate_linear(pi + 1, &clt3, pv);
	if (ix == pdim[0] - 1)
	    return;
	clt3.table += pdim[1];
	gx_color_interpolate_linear(pi + 1, &clt3, vx);
	for (j = 0; j < m; ++j)
	    pv[j] += (frac) arith_rshift((long)fx * (vx[j] - pv[j]),
					 _fixed_shift);
    } else {
	int ic = fixed2int_var(pi[2]);
	fixed fc = fixed_fraction(pi[2]);
	uint dc1 = (ic == pdim[2] - 1 ? 0 : m);
	int ib = fixed2int_var(pi[1]);
	fixed fb = fixed_fraction(pi[1]);
	uint db1 = (ib == pdim[1] - 1 ? 0 : pdim[2] * m);
	uint dbc = (ib * pdim[2] + ic) * m;
	uint dbc1 = db1 + dc1;
	int ia = fixed2int_var(pi[0]);
	fixed fa = fixed_fraction(pi[0]);
	const byte *pa0 = pclt->table[ia].data + dbc;
	const byte *pa1 =
	(ia == pdim[0] - 1 ? pa0 : pclt->table[ia + 1].data + dbc);
	int j;

	/* The values to be interpolated are */
	/* pa{0,1}[{0,db1,dc1,dbc1}]. */
	for (j = 0; j < m; ++j, ++pa0, ++pa1) {
	    frac v000 = byte2frac(pa0[0]);
	    frac v001 = byte2frac(pa0[dc1]);
	    frac v010 = byte2frac(pa0[db1]);
	    frac v011 = byte2frac(pa0[dbc1]);
	    frac v100 = byte2frac(pa1[0]);
	    frac v101 = byte2frac(pa1[dc1]);
	    frac v110 = byte2frac(pa1[db1]);
	    frac v111 = byte2frac(pa1[dbc1]);

	    frac v00 = v000 +
	    (frac) arith_rshift((long)fc * (v001 - v000),
				_fixed_shift);
	    frac v01 = v010 +
	    (frac) arith_rshift((long)fc * (v011 - v010),
				_fixed_shift);
	    frac v10 = v100 +
	    (frac) arith_rshift((long)fc * (v101 - v100),
				_fixed_shift);
	    frac v11 = v110 +
	    (frac) arith_rshift((long)fc * (v111 - v110),
				_fixed_shift);

	    frac v0 = v00 +
	    (frac) arith_rshift((long)fb * (v01 - v00),
				_fixed_shift);
	    frac v1 = v10 +
	    (frac) arith_rshift((long)fb * (v11 - v10),
				_fixed_shift);

	    pv[j] = v0 +
		(frac) arith_rshift((long)fa * (v1 - v0),
				    _fixed_shift);
	}
    }
}
