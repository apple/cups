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

/* gxhint3.c */
/* Apply hints for Type 1 fonts. */
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
#include "gzpath.h"

/* Define whether we are using new algorithms here. */
#define NEW

/* ------ Path hints ------ */

/* Forward references */
private void
  apply_hstem_hints(P3(gs_type1_state *, int, gs_fixed_point *)),
  apply_vstem_hints(P3(gs_type1_state *, int, gs_fixed_point *));


/*
 * Apply hints along a newly added tail of a subpath.
 * Path segments require hints as follows:
 *	Nearly vertical line: vstem hints at both ends.
 *	Nearly horizontal line: hstem hints at both ends.
 *	Curve with nearly vertical/horizontal start/end:
 *	  vstem/hstem hints at start/end.
 * We also must take care to handle the implicit closing line for
 * subpaths that aren't explicitly closed.
 */
#define hint_vert 001
#define hint_vert_lower 003
#define hint_vert_upper 005
#define hint_horz 010
#define hint_horz_lower 030
#define hint_horz_upper 050
#define nearly_axial(dmajor, dminor)\
  ((dminor) <= (dmajor) >> 4 /* float2fixed(0.25) */)

/* Determine which types of hints, if any, are applicable to a given */
/* line segment. */
private int near
line_hints(const gs_type1_state *pcis, const gs_fixed_point *p0,
  const gs_fixed_point *p1)
{	fixed dx = p1->x - p0->x, adx;
	fixed dy = p1->y - p0->y, ady;
	int hints;

	/* Map the deltas back into character space. */
	if ( pcis->fh.axes_swapped )
	  {	fixed t = dx; dx = dy; dy = t;
	  }
	if ( pcis->fh.x_inverted )
	  dx = -dx;
	if ( pcis->fh.y_inverted )
	  dy = -dy;
	adx = any_abs(dx);
	ady = any_abs(dy);
	if ( dy != 0 && nearly_axial(ady, adx) )
	  hints = (dy > 0 ? hint_vert_upper : hint_vert_lower);
	else if ( dx != 0 && nearly_axial(adx, ady) )
	  hints = (dx < 0 ? hint_horz_upper : hint_horz_lower);
	else
	  hints = 0;
	if_debug7('y', "[y]hint from 0x%lx(%g,%g) to 0x%lx(%g,%g) = %d\n",
		  (ulong)p0, fixed2float(p0->x), fixed2float(p0->y),
		  (ulong)p1, fixed2float(p1->x), fixed2float(p1->y),
		  hints);
	return hints;
}

/* Apply hints at a point.  Optionally return the amount of adjustment. */
private void near
apply_hints_at(gs_type1_state *pcis, int hints, gs_fixed_point *ppt,
  gs_fixed_point *pdiff)
{	fixed ptx = ppt->x, pty = ppt->y;
	if_debug4('y', "[y]applying hints %d to 0x%lx(%g,%g) ...\n",
		  hints, (ulong)ppt, fixed2float(ptx), fixed2float(pty));
	if ( (hints & hint_vert) != 0 &&
	     (pcis->vstem_hints.count & pcis->dotsection_flag) != 0
	   )
	  {	/* "Upper" vertical edges move in +Y. */
		  apply_vstem_hints(pcis, (hints & hint_vert_upper) -
				    (hints & hint_vert_lower), ppt);
	  }
	if ( (hints & hint_horz) != 0 &&
	     (pcis->hstem_hints.count & pcis->dotsection_flag) != 0
	   )
	  {	/* "Upper" horizontal edges move in -X. */
		  apply_hstem_hints(pcis, (hints & hint_horz_lower) -
				    (hints & hint_horz_upper), ppt);
	  }
	if ( pdiff != NULL )
	  pdiff->x = ppt->x - ptx,
	  pdiff->y = ppt->y - pty;
	/* Here is where we would round *ppt to the nearest quarter-pixel */
	/* if we wanted to. */
	if_debug2('y', "[y] ... => (%g,%g)\n",
		  fixed2float(ppt->x), fixed2float(ppt->y));
}

#ifdef DEBUG
private void near
add_hint_diff_proc(gs_fixed_point *ppt, fixed dx, fixed dy)
{	if_debug7('y', "[y]adding diff (%g,%g) to 0x%lx(%g,%g) => (%g,%g)\n",
		  fixed2float(dx), fixed2float(dy), (ulong)ppt,
		  fixed2float(ppt->x), fixed2float(ppt->y),
		  fixed2float(ppt->x + dx),
		  fixed2float(ppt->y + dy));
	ppt->x += dx;
	ppt->y += dy;
}
#define add_hint_dxdy(pt, dx,dy)\
  add_hint_diff_proc(&(pt), dx, dy)
#else
#define add_hint_dxdy(pt, dx, dy)\
  (pt).x += (dx), (pt).y += (dy)
#endif
#define add_hint_diff(pt, diff)\
  add_hint_dxdy(pt, (diff).x, (diff).y)

/* Test whether a line is null. */
#define line_is_null(p0, p1)\
 (any_abs((p1).x - (p0).x) + any_abs((p1).y - (p0).y) < fixed_epsilon * 4)

/* Adjust the control points of a curve when moving one end. */
private void
adjust_curve_start(curve_segment *pcseg, const gs_fixed_point *pdiff)
{	fixed dx = pdiff->x, dy = pdiff->y;
	fixed dx2 = arith_rshift(dx, 2), dy2 = arith_rshift(dy, 2);
	add_hint_dxdy(pcseg->p1, dx, dy);
	add_hint_dxdy(pcseg->p2, dx2, dy2);
}
private void
adjust_curve_end(curve_segment *pcseg, const gs_fixed_point *pdiff)
{	fixed dx = pdiff->x, dy = pdiff->y;
	fixed dx2 = arith_rshift(dx, 2), dy2 = arith_rshift(dy, 2);
	add_hint_dxdy(pcseg->p1, dx2, dy2);
	add_hint_dxdy(pcseg->p2, dx, dy);
}

/*
 * Propagate a final wraparound hint back through any null line segments
 * to a possible curve.  pseg_last.pt has already been adjusted.
 */
private void
apply_final_hint(segment *pseg_last, const gs_fixed_point *pdiff)
{	segment *pseg;
	for ( pseg = pseg_last; ; pseg = pseg->prev )
	  { segment *prev = pseg->prev;
	    switch ( pseg->type )
	      {
	      case s_curve:
		adjust_curve_end(((curve_segment *)pseg), pdiff);
		return;
	      case s_line:
	      case s_line_close:
		if ( !line_is_null(prev->pt, pseg->pt) )
		  return;
		add_hint_diff(prev->pt, *pdiff);
		break;
	      default:		/* s_start */
		return;
	      }
	  }
}

/*
 * Apply hints along a subpath.  If closing is true, consider the subpath
 * closed; if not, we may add more to the subpath later.  In the latter case,
 * don't do anything if the subpath is closed, because we already applied
 * the hints.
 */
void
type1_apply_path_hints(gs_type1_state *pcis, bool closing, gx_path *ppath)
{	segment *pseg = pcis->hint_next;
#define pseg_curve ((curve_segment *)pseg)
	segment *pnext;
#define pnext_curve ((curve_segment *)pnext)
	subpath *psub = ppath->current_subpath;
	/*
	 * hints holds the set of hints that have already been applied (if
	 * applicable) to pseg->pt, and hence should not be applied again.
	 */
	int hints = pcis->hints_pending;
	gs_fixed_point diff;

	if ( pseg == 0 )
	  {	/* Start at the beginning of the subpath. */
		if ( psub == 0 )
		  return;
		if ( psub->is_closed && !closing )
		  return;
		pseg = (segment *)psub;
		if ( pseg->next == 0 )
		  return;
		hints = 0;
		pcis->unmoved_start = psub->pt;
		pcis->unmoved_end = psub->pt;
	  }
	else
	  hints = pcis->hints_pending;
	diff.x = diff.y = 0;
	for ( ; (pnext = pseg->next) != 0; pseg = pnext )
	  {	int hints_next;
		/* Apply hints to the end of the previous segment (pseg) */
		/* and the beginning of this one (pnext). */
		gs_fixed_point dseg;
		switch ( pnext->type )
		  {
		  case s_curve:
		    {	int hints_first =
			  line_hints(pcis, &pcis->unmoved_end,
				     &pnext_curve->p1) & ~hints;
			gs_fixed_point diff2;
			if ( pseg == (segment *)psub )
			  pcis->hints_initial = hints_first;
			apply_hints_at(pcis, hints_first, &pseg->pt, &dseg);
			diff2.x = pseg->pt.x - pcis->unmoved_end.x;
			diff2.y = pseg->pt.y - pcis->unmoved_end.y;
			hints_next = line_hints(pcis, &pnext_curve->p2,
						&pnext->pt);
			adjust_curve_start(pnext_curve, &diff2);
			apply_hints_at(pcis, hints_next, &pnext_curve->p2,
				       &diff);
			pcis->unmoved_end = pnext->pt;
			add_hint_diff(pnext->pt, diff);
		    }	break;
#ifdef NEW
		  case s_line_close:
			/* Undo any initial hints propagated to the end. */
			pnext->pt = pcis->unmoved_start;
#endif
		  default:		/* s_line, s_line_close */
			if ( line_is_null(pnext->pt, pcis->unmoved_end) )
			  { /* This is a null line, just move it. */
			    hints_next = hints;
			    dseg.x = dseg.y = 0;  /* don't move p2 again */
			  }
			else
			  { hints_next =
			      line_hints(pcis, &pcis->unmoved_end, &pnext->pt);
			    apply_hints_at(pcis, hints_next & ~hints,
					   &pseg->pt, &dseg);
			  }
			if ( pseg == (segment *)psub )
			  pcis->hints_initial = hints_next;
			pcis->unmoved_end = pnext->pt;
			apply_hints_at(pcis, hints_next, &pnext->pt, NULL);
		  }
		if ( pseg->type == s_curve )
		  adjust_curve_end(pseg_curve, &dseg);
		hints = hints_next;
	  }
	if ( closing )
	  {	/* Handle the end of the subpath wrapping around to the start. */
		/* This is ugly, messy code that we can surely improve. */
		fixed ctemp;
		/* Some fonts don't use closepath when they should.... */
		bool closed =
		  (pseg->type == s_line_close ||
		   (ctemp = pseg->pt.x - psub->pt.x,
		    any_abs(ctemp) < float2fixed(0.1)) ||
		   (ctemp = pseg->pt.y - psub->pt.y,
		    any_abs(ctemp) < float2fixed(0.1)));
		segment *pfirst = psub->next;
#define pfirst_curve ((curve_segment *)pfirst)
		int hints_first = pcis->hints_initial;
		if ( closed )
		  {	/*
			 * Apply the union of the hints at both the end
			 * (pseg) and the start (psub) of the subpath.  Note
			 * that we have already applied hints at the end,
			 * and hints_first at the start.
			 */
#ifdef NEW
			int do_x, do_y;
			gs_fixed_point diff2;

			if ( pcis->fh.axes_swapped )
			  do_x = hint_horz, do_y = hint_vert;
			else
			  do_x = hint_vert, do_y = hint_horz;

			{ /* Apply hints_first - hints to the end. */
			  int hints_end = hints_first & ~hints;
			  diff2.x =
			    (hints_end & do_x ?
			     psub->pt.x - pcis->unmoved_start.x : 0);
			  diff2.y =
			    (hints_end & do_y ?
			     psub->pt.y - pcis->unmoved_start.y : 0);
			}

			{ /* Apply hints - hints_first to the start. */
			  int hints_start = hints & ~hints_first;
			  diff.x =
			    (hints_start & do_x ?
			     pseg->pt.x - pcis->unmoved_end.x : 0);
			  diff.y =
			    (hints_start & do_y ?
			     pseg->pt.y - pcis->unmoved_end.y : 0);
			}

			add_hint_diff(pseg->pt, diff2);
			apply_final_hint(pseg, &diff2);
			add_hint_diff(psub->pt, diff);
#else
			hints &= ~hints_first;
			{ gs_fixed_point end, diff2;
			  end = pseg->pt;
			  apply_hints_at(pcis, hints, &end, &diff);
			  add_hint_diff(psub->pt, diff);
			  diff2.x = psub->pt.x - pcis->unmoved_start.x -
			    (pseg->pt.x - pcis->unmoved_end.x);
			  diff2.y = psub->pt.y - pcis->unmoved_start.y -
			    (pseg->pt.y - pcis->unmoved_end.y);
			  /* If the last segment closes the path, */
			  /* make sure we don't apply the last hint twice. */
			  /* (Thanks to Hans-Gerd Straeter for this fix.) */
			  if ( !line_is_null(psub->pt, pseg->pt) )
			    add_hint_diff(pseg->pt, diff2);
			  apply_final_hint(pseg, &diff2);
			}
#endif
		  }
		else
		  {	int hints_close =
			  line_hints(pcis, &pcis->unmoved_end,
				     &pcis->unmoved_start);
			hints_close &= ~(hints | hints_first);
			apply_hints_at(pcis, hints_close, &pseg->pt, &diff);
			apply_final_hint(pseg, &diff);
			apply_hints_at(pcis, hints_close, &psub->pt, &diff);
		  }
		if ( pfirst->type == s_curve )
		  adjust_curve_start(pfirst_curve, &diff);
		pcis->hint_next = 0;
		pcis->hints_pending = 0;
#undef pfirst_curve
	  }
	else
	  {	pcis->hint_next = pseg;
		pcis->hints_pending = hints;
	  }
#undef pseg_curve
#undef pnext_curve
}

/* ------ Individual hints ------ */

private stem_hint *near search_hints(P2(stem_hint_table *, fixed));

/*
 * Adjust a point according to the relevant hints.
 * x and y are the current point in device space after moving;
 * dx or dy is the delta component in character space.
 * The caller is responsible for checking use_hstem_hints or use_vstem_hints
 * and not calling the find_xxx_hints routine if this is false.
 * Note that if use_x/y_hints is false, no entries ever get made
 * in the stem hint tables, so these routines will not get called.
 */

/*
 * To figure out which side of the stem we are on, we assume that
 * the inside of the filled area is always to the left of the edge, i.e.,
 * edges moving in -X or +Y in character space are on the "upper" side of
 * the stem, while edges moving by +X or -Y are on the "lower" side.
 * (See section 3.5 of the Adobe Type 1 Font Format book.)
 * However, since dv0 and dv1 correspond to the lesser and greater
 * values in *device* space, we have to take the inversion of the other axis
 * into account when selecting between them.
 */

private void
apply_vstem_hints(gs_type1_state *pcis, int dy, gs_fixed_point *ppt)
{	fixed *pv = (pcis->fh.axes_swapped ? &ppt->y : &ppt->x);
	stem_hint *ph = search_hints(&pcis->vstem_hints, *pv);
	if ( ph != 0 )
	{
#define vstem_upper(pcis, dy)\
  (pcis->fh.x_inverted ? dy < 0 : dy > 0)
		if_debug3('Y', "[Y]use vstem %d: %g (%s)",
			  (int)(ph - &pcis->vstem_hints.data[0]),
			  fixed2float(*pv),
			  (dy == 0 ? "middle" :
			   vstem_upper(pcis, dy) ? "upper" : "lower"));
		*pv += (dy == 0 ? arith_rshift_1(ph->dv0 + ph->dv1) :
			vstem_upper(pcis, dy) ? ph->dv1 : ph->dv0);
		if_debug1('Y', " -> %g\n", fixed2float(*pv));
#undef vstem_upper
	}
}

private void
apply_hstem_hints(gs_type1_state *pcis, int dx, gs_fixed_point *ppt)
{	fixed *pv = (pcis->fh.axes_swapped ? &ppt->x : &ppt->y);
	stem_hint *ph = search_hints(&pcis->hstem_hints, *pv);
	if ( ph != 0 )
	{
#define hstem_upper(pcis, dx)\
  (pcis->fh.y_inverted ? dx > 0 : dx < 0)
		if_debug3('Y', "[Y]use hstem %d: %g (%s)",
			  (int)(ph - &pcis->hstem_hints.data[0]),
			  fixed2float(*pv),
			  (dx == 0 ? "middle" :
			   hstem_upper(pcis, dx) ? "upper" : "lower"));
		*pv += (dx == 0 ? arith_rshift_1(ph->dv0 + ph->dv1) :
			hstem_upper(pcis, dx) ? ph->dv1 : ph->dv0);
		if_debug1('Y', " -> %g\n", fixed2float(*pv));
#undef hstem_upper
	}
}

/* Search one hint table for an adjustment. */
private stem_hint *near
search_hints(stem_hint_table *psht, fixed v)
{	stem_hint *table = &psht->data[0];
	stem_hint *ph = table + psht->current;
	if ( v >= ph->v0 && v <= ph->v1 )
	  return ph;
	/* We don't bother with binary or even up/down search, */
	/* because there won't be very many hints. */
	for ( ph = &table[psht->count]; --ph >= table; )
	  if ( v >= ph->v0 && v <= ph->v1 )
	    {	psht->current = ph - table;
		return ph;
	    }
	return 0;
}

