/* Copyright (C) 1990, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxhint2.c */
/* Character level hints for Type 1 fonts. */
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxchar.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gxop1.h"

/* Define the tolerance for testing whether a point is in a zone, */
/* in device pixels.  (Maybe this should be variable?) */
#define stem_tolerance float2fixed(0.05)

/* Forward references */

private stem_hint *near type1_stem(P3(stem_hint_table *, fixed, fixed));
private fixed near find_snap(P3(fixed, const stem_snap_table *, const pixel_scale *));
private alignment_zone *near find_zone(P3(gs_type1_state *, fixed, fixed));

/* Reset the stem hints. */
void
reset_stem_hints(register gs_type1_state *pcis)
{	pcis->hstem_hints.count = 0;
	pcis->vstem_hints.count = 0;
	update_stem_hints(pcis);
}

/* Update the internal stem hint pointers after moving or copying the state. */
void
update_stem_hints(register gs_type1_state *pcis)
{	pcis->hstem_hints.current = 0;
	pcis->vstem_hints.current = 0;
}

/* ------ Add hints ------ */

#define c_fixed(d, c) m_fixed(d, c, pcis->fc, max_coeff_bits)

/* Add a horizontal stem hint. */
void
type1_do_hstem(register gs_type1_state *pcis, fixed y, fixed dy,
  const gs_matrix_fixed *pmat)
{	stem_hint *psh;
	alignment_zone *pz;
	const pixel_scale *psp;
	fixed v, dv, adj_dv;
	fixed vtop, vbot;

	if ( !pcis->fh.use_y_hints || !pmat->txy_fixed_valid )
	  return;
	y += pcis->lsb.y + pcis->adxy.y;
	if ( pcis->fh.axes_swapped )
		psp = &pcis->scale.x,
		v = pcis->vs_offset.x + c_fixed(y, yx) + pmat->tx_fixed,
		dv = c_fixed(dy, yx);
	else
		psp = &pcis->scale.y,
		v = pcis->vs_offset.y + c_fixed(y, yy) + pmat->ty_fixed,
		dv = c_fixed(dy, yy);
	if ( dy < 0 )
		vbot = v + dv, vtop = v;
	else
		vbot = v, vtop = v + dv;
	if ( dv < 0 ) v += dv, dv = -dv;
	psh = type1_stem(&pcis->hstem_hints, v, dv);
	if ( psh == 0 ) return;
	adj_dv = find_snap(dv, &pcis->fh.snap_h, psp);
	pz = find_zone(pcis, vbot, vtop);
	if ( pz != 0 )
	{	/* Use the alignment zone to align the outer stem edge. */
		int inverted =
		  (pcis->fh.axes_swapped ? pcis->fh.x_inverted : pcis->fh.y_inverted);
		int adjust_v1 =
		  (inverted ? !pz->is_top_zone : pz->is_top_zone);
		fixed flat_v = pz->flat;
		fixed overshoot =
			(pz->is_top_zone ? vtop - flat_v : flat_v - vbot);
		fixed pos_over =
			(inverted ? -overshoot : overshoot);
		fixed ddv = adj_dv - dv;
		fixed shift = scaled_rounded(flat_v, psp) - flat_v;
		if ( pos_over > 0 )
		{
		  if ( pos_over < pcis->fh.blue_shift || pcis->fh.suppress_overshoot )
		  {	/* Character is small, suppress overshoot. */
			if_debug0('y', "[y]suppress overshoot\n");
			if ( pz->is_top_zone )
				shift -= overshoot;
			else
				shift += overshoot;
		  }
		  else
		  if ( pos_over < psp->unit )
		  {	/* Enforce overshoot. */
			if_debug0('y', "[y]enforce overshoot\n");
			if ( overshoot < 0 )
				overshoot = -psp->unit - overshoot;
			else
				overshoot = psp->unit - overshoot;
			if ( pz->is_top_zone )
				shift += overshoot;
			else
				shift -= overshoot;
		  }
		}
		if ( adjust_v1 )
			psh->dv1 = shift, psh->dv0 = shift - ddv;
		else
			psh->dv0 = shift, psh->dv1 = shift + ddv;
		if_debug2('y', "[y]flat_v = %g, overshoot = %g for:\n",
			  fixed2float(flat_v), fixed2float(overshoot));
	}
	else
	  {	/* Align the stem so its edges fall on pixel boundaries. */
		fixed diff2_dv = arith_rshift_1(adj_dv - dv);
		fixed edge = v - diff2_dv;
		fixed diff_v = scaled_rounded(edge, psp) - edge;
		psh->dv0 = diff_v - diff2_dv;
		psh->dv1 = diff_v + diff2_dv;
	  }
	if_debug9('y', "[y]hstem %d/%d: %g,%g -> %g(%g)%g ; d = %g,%g\n",
		  (int)(psh - &pcis->hstem_hints.data[0]),
		  pcis->hstem_hints.count,
		  fixed2float(y), fixed2float(dy),
		  fixed2float(v), fixed2float(dv), fixed2float(v + dv),
		  fixed2float(psh->dv0), fixed2float(psh->dv1));
}

/* Add a vertical stem hint. */
void
type1_do_vstem(register gs_type1_state *pcis, fixed x, fixed dx,
  const gs_matrix_fixed *pmat)
{	stem_hint *psh;
	const pixel_scale *psp;
	fixed v, dv, adj_dv;
	fixed edge, diff_v, diff2_dv;
	if ( !pcis->fh.use_x_hints ) return;
	x += pcis->lsb.x + pcis->adxy.x;
	if ( pcis->fh.axes_swapped )
		psp = &pcis->scale.y,
		v = pcis->vs_offset.y + c_fixed(x, xy) + pmat->ty_fixed,
		dv = c_fixed(dx, xy);
	else
		psp = &pcis->scale.x,
		v = pcis->vs_offset.x + c_fixed(x, xx) + pmat->tx_fixed,
		dv = c_fixed(dx, xx);
	if ( dv < 0 ) v += dv, dv = -dv;
	psh = type1_stem(&pcis->vstem_hints, v, dv);
	if ( psh == 0 ) return;
	adj_dv = find_snap(dv, &pcis->fh.snap_v, psp);
	if ( pcis->pfont->data.ForceBold && adj_dv < psp->unit )
		adj_dv = psp->unit;
	/* Align the stem so its edges fall on pixel boundaries. */
	diff2_dv = arith_rshift_1(adj_dv - dv);
	edge = v - diff2_dv;
	diff_v = scaled_rounded(edge, psp) - edge;
	psh->dv0 = diff_v - diff2_dv;
	psh->dv1 = diff_v + diff2_dv;
	if_debug9('y', "[y]vstem %d/%d: %g,%g -> %g(%g)%g ; d = %g,%g\n",
		  (int)(psh - &pcis->vstem_hints.data[0]),
		  pcis->vstem_hints.count,
		  fixed2float(x), fixed2float(dx),
		  fixed2float(v), fixed2float(dv), fixed2float(v + dv),
		  fixed2float(psh->dv0), fixed2float(psh->dv1));
}

/* Adjust the character center for a vstem3. */
/****** NEEDS UPDATING FOR SCALE ******/
void
type1_do_center_vstem(gs_type1_state *pcis, fixed x0, fixed dx,
  const gs_matrix_fixed *pmat)
{	fixed x1 = x0 + dx;
	gs_fixed_point pt0, pt1, width;
	fixed center, int_width;
	fixed *psxy;
	if ( gs_point_transform2fixed(pmat, fixed2float(x0), 0.0, &pt0) < 0 ||
	     gs_point_transform2fixed(pmat, fixed2float(x1), 0.0, &pt1) < 0
	   )
	  {	/* Punt. */
		return;
	  }
	width.x = pt0.x - pt1.x;
	if ( width.x < 0 )
	  width.x = - width.x;
	width.y = pt0.y - pt1.y;
	if ( width.y < 0 )
	  width.y = - width.y;
	if ( width.y < float2fixed(0.05) )
	{	/* Vertical on device */
		center = arith_rshift_1(pt0.x + pt1.x);
		int_width = fixed_rounded(width.x);
		psxy = &pcis->vs_offset.x;
	}
	else
	{	/* Horizontal on device */
		center = arith_rshift_1(pt0.y + pt1.y);
		int_width = fixed_rounded(width.y);
		psxy = &pcis->vs_offset.y;
	}
	if ( int_width == fixed_0 || (int_width & fixed_1) == 0 )
	{	/* Odd width, center stem over pixel. */
		*psxy = fixed_floor(center) + fixed_half - center;
	}
	else
	{	/* Even width, center stem between pixels. */
		*psxy = fixed_rounded(center) - center;
	}
	/* We can't fix up the current point here, */
	/* but we can fix up everything else. */
	/****** TO BE COMPLETED ******/
}

/* Add a stem hint, keeping the table sorted. */
/* We know that d >= 0. */
/* Return the stem hint pointer, or 0 if the table is full. */
private stem_hint *near
type1_stem(stem_hint_table *psht, fixed v0, fixed d)
{	stem_hint *bot = &psht->data[0];
	stem_hint *top = bot + psht->count;
	if ( psht->count >= max_stems )
	  return 0;
	while ( top > bot && v0 < top[-1].v0 )
	   {	*top = top[-1];
		top--;
	   }
	/* Add a little fuzz for insideness testing. */
	top->v0 = v0 - stem_tolerance;
	top->v1 = v0 + d + stem_tolerance;
	psht->count++;
	return top;
}

/* Compute the adjusted width of a stem. */
/* The value returned is always a multiple of scale.unit. */
private fixed near
find_snap(fixed dv, const stem_snap_table *psst, const pixel_scale *pps)
{	fixed best = pps->unit;
	fixed adj_dv;
	int i;
	for ( i = 0; i < psst->count; i++ )
	{	fixed diff = psst->data[i] - dv;
		if ( any_abs(diff) < any_abs(best) )
		{	if_debug3('Y', "[Y]possibly snap %g to [%d]%g\n",
				  fixed2float(dv), i,
				  fixed2float(psst->data[i]));
			best = diff;
		}
	}
	adj_dv = scaled_rounded((any_abs(best) < pps->unit ? dv + best : dv),
				pps);
	if ( adj_dv == 0 )
	  adj_dv = pps->unit;
#ifdef DEBUG
	if ( adj_dv == dv )
	  if_debug1('Y', "[Y]no snap %g\n", fixed2float(dv));
	else
	  if_debug2('Y', "[Y]snap %g to %g\n",
		    fixed2float(dv), fixed2float(adj_dv));
#endif
	return adj_dv;
}

/* Find the applicable alignment zone for a stem, if any. */
/* vbot and vtop are the bottom and top of the stem, */
/* but without interchanging if the y axis is inverted. */
private alignment_zone *near
find_zone(gs_type1_state *pcis, fixed vbot, fixed vtop)
{	alignment_zone *pz;
	for ( pz = &pcis->fh.a_zones[pcis->fh.a_zone_count];
	      --pz >= &pcis->fh.a_zones[0];
	    )
	{	fixed v = (pz->is_top_zone ? vtop : vbot);
		if ( v >= pz->v0 && v <= pz->v1 )
		{	if_debug2('Y', "[Y]stem crosses %s-zone %d\n",
				  (pz->is_top_zone ? "top" : "bottom"),
				  (int)(pz - &pcis->fh.a_zones[0]));
			return pz;
		}
	}
	return 0;
}
