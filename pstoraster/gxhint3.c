/* Copyright (C) 1994, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxhint3.c,v 1.2 2000/03/08 23:15:00 mike Exp $ */
/* Apply hints for Type 1 fonts. */
#include "math_.h"		/* for floor in fixed_mult_quo */
#include "gx.h"
#include "gserrors.h"
#include "gxarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "gxtype1.h"
#include "gzpath.h"

/* ------ Path hints ------ */

/* Forward references */
private void
     apply_hstem_hints(P3(gs_type1_state *, int, gs_fixed_point *)), apply_vstem_hints(P3(gs_type1_state *, int, gs_fixed_point *));


/*
 * Apply hints along a newly added tail of a subpath.
 * Path segments require hints as follows:
 *      Nearly vertical line: vstem hints at both ends.
 *      Nearly horizontal line: hstem hints at both ends.
 *      Curve with nearly vertical/horizontal start/end:
 *        vstem/hstem hints at start/end.
 * We also must take care to handle the implicit closing line for
 * subpaths that aren't explicitly closed.
 *
 * Note that "upper" and "lower" refer to device coordinates, which are
 * what we use throughout the Type 1 code; however, "horizontal" and
 * "vertical" refer to the character space coordinate system.
 */
#define HINT_VERT_LOWER 1
#define HINT_VERT_UPPER 2	/* must be > lower */
#define HINT_VERT (HINT_VERT_LOWER | HINT_VERT_UPPER)
#define HINT_HORZ_LOWER 4
#define HINT_HORZ_UPPER 8	/* must be > lower */
#define HINT_HORZ (HINT_HORZ_LOWER | HINT_HORZ_UPPER)
#define NEARLY_AXIAL(dmajor, dminor)\
  ((dminor) <= (dmajor) >> 4)

/*
 * Determine which types of hints, if any, are applicable to a given
 * line segment.
 */
private int
line_hints(const gs_type1_state * pcis, const gs_fixed_point * p0,
	   const gs_fixed_point * p1)
{
    fixed dx = p1->x - p0->x;
    fixed dy = p1->y - p0->y;
    fixed adx, ady;
    bool xi = pcis->fh.x_inverted, yi = pcis->fh.y_inverted;
    int hints;

    /*
     * To figure out which side of the stem we are on, we assume that the
     * inside of the filled area is always to the left of the edge, i.e.,
     * edges moving in -X or +Y in character space are on the "upper" side
     * of the stem, while edges moving by +X or -Y are on the "lower" side.
     * (See section 3.5 of the Adobe Type 1 Font Format book.)
     */

    /*
     * Map the deltas back into character space.  This is essentially an
     * inverse-distance-transform with the combined matrix, but we don't
     * bother to undo the scaling, since it only matters for the axiality
     * test and we don't care about situations where X and Y scaling are
     * radically different.
     */
    if (xi)
	dx = -dx;
    if (yi)
	dy = -dy;
    if (pcis->fh.axes_swapped) {
	fixed t = dx;
	int ti = xi;

	dx = dy, xi = yi;
	dy = t, yi = ti;
    }
    adx = any_abs(dx);
    ady = any_abs(dy);
    /*
     * Note that since upper/lower refer to device space, we must
     * interchange them if the corresponding axis is inverted.
     */
    if (dy != 0 && NEARLY_AXIAL(ady, adx)) {
	hints = (dy > 0 ? HINT_VERT_UPPER : HINT_VERT_LOWER);
	if (xi)
	    hints ^= (HINT_VERT_LOWER | HINT_VERT_UPPER);
    } else if (dx != 0 && NEARLY_AXIAL(adx, ady)) {
	hints = (dx < 0 ? HINT_HORZ_UPPER : HINT_HORZ_LOWER);
	if (yi)
	    hints ^= (HINT_HORZ_LOWER | HINT_HORZ_UPPER);
    } else
	hints = 0;
    if_debug7('y', "[y]hint from 0x%lx(%1.4f,%1.4f) to 0x%lx(%1.4f,%1.4f) = %d\n",
	      (ulong) p0, fixed2float(p0->x), fixed2float(p0->y),
	      (ulong) p1, fixed2float(p1->x), fixed2float(p1->y),
	      hints);
    return hints;
}

/* Apply hints at a point.  Optionally return the amount of adjustment. */
private void
apply_hints_at(gs_type1_state * pcis, int hints, gs_fixed_point * ppt,
	       gs_fixed_point * pdiff)
{
    fixed px = ppt->x, py = ppt->y;

    if_debug4('y', "[y]applying hints %d to 0x%lx(%1.4f,%1.4f) ...\n",
	      hints, (ulong) ppt, fixed2float(px), fixed2float(py));
    if ((hints & HINT_VERT) != 0 &&
	(pcis->vstem_hints.count & pcis->dotsection_flag) != 0
	)
	apply_vstem_hints(pcis, (hints & HINT_VERT_UPPER) -
			  (hints & HINT_VERT_LOWER), ppt);
    if ((hints & HINT_HORZ) != 0 &&
	(pcis->hstem_hints.count & pcis->dotsection_flag) != 0
	)
	apply_hstem_hints(pcis, (hints & HINT_HORZ_UPPER) -
			  (hints & HINT_HORZ_LOWER), ppt);
    if (pdiff != NULL)
	pdiff->x = ppt->x - px,
	    pdiff->y = ppt->y - py;
    /* Here is where we would round *ppt to the nearest quarter-pixel */
    /* if we wanted to. */
    if_debug2('y', "[y] ... => (%1.4f,%1.4f)\n",
	      fixed2float(ppt->x), fixed2float(ppt->y));
}

/* Add a hint delta to a point. */
#ifndef DEBUG
inline
#endif
private void
add_hint_diff(gs_fixed_point * ppt, gs_fixed_point delta)
{
    if_debug7('y', "[y]adding diff (%1.4f,%1.4f) to 0x%lx(%1.4f,%1.4f) => (%1.4f,%1.4f)\n",
	      fixed2float(delta.x), fixed2float(delta.y), (ulong) ppt,
	      fixed2float(ppt->x), fixed2float(ppt->y),
	      fixed2float(ppt->x + delta.x), fixed2float(ppt->y + delta.y));
    ppt->x += delta.x;
    ppt->y += delta.y;
}

/* Test whether a line is null. */
inline private bool
line_is_null(gs_fixed_point p0, gs_fixed_point p1)
{
    return (any_abs(p1.x - p0.x) + any_abs(p1.y - p0.y) < fixed_epsilon * 4);
}

/*
 * Adjust the other control points of a curve proportionately when moving
 * one end.  The Boolean argument indicates whether the point being
 * adjusted is the one nearer the point that was moved.
 */
private fixed
scale_delta(fixed diff, fixed dv, fixed lv, bool nearer)
{
    if (dv == 0)
	return 0;
    /*
     * fixed_mult_quo requires non-negative 2nd and 3rd arguments,
     * and also 2nd argument < 3rd argument.
     * If it weren't for that, we would just use it directly.
     *
     * lv = 0 is implausible, but we have to allow for it.
     */
    if (lv == 0)
	return (nearer ? diff : fixed_0);
    if (lv < 0)
	lv = -lv, dv = -dv;
    if (dv < 0)
	dv = -dv, diff = -diff;
    /*
     * If dv > lv, there has been some kind of anomaly similar to
     * the lv = 0 case.
     */
    if (dv >= lv)
	return (nearer ? diff : fixed_0);
    else
	return fixed_mult_quo(diff, dv, lv);
}
private void
adjust_curve_start(curve_segment * pcseg, const gs_fixed_point * pdiff)
{
    fixed dx = pdiff->x, dy = pdiff->y;
    fixed end_x = pcseg->pt.x, end_y = pcseg->pt.y;
    const segment *prev = pcseg->prev;
    fixed lx = end_x - (prev->pt.x - dx), ly = end_y - (prev->pt.y - dy);
    gs_fixed_point delta;

    delta.x = scale_delta(end_x - pcseg->p1.x, dx, lx, true);
    delta.y = scale_delta(end_y - pcseg->p1.y, dy, ly, true);
    add_hint_diff(&pcseg->p1, delta);
    delta.x = scale_delta(end_x - pcseg->p2.x, dx, lx, false);
    delta.y = scale_delta(end_y - pcseg->p2.y, dy, ly, false);
    add_hint_diff(&pcseg->p2, delta);
}
private void
adjust_curve_end(curve_segment * pcseg, const gs_fixed_point * pdiff)
{
    fixed dx = pdiff->x, dy = pdiff->y;
    const segment *prev = pcseg->prev;
    fixed start_x = prev->pt.x, start_y = prev->pt.y;
    fixed lx = pcseg->pt.x - dx - start_x, ly = pcseg->pt.y - dy - start_y;
    gs_fixed_point delta;

    delta.x = scale_delta(pcseg->p1.x - start_x, dx, lx, false);
    delta.y = scale_delta(pcseg->p1.y - start_y, dy, ly, false);
    add_hint_diff(&pcseg->p1, delta);
    delta.x = scale_delta(pcseg->p2.x - start_x, dx, lx, true);
    delta.y = scale_delta(pcseg->p2.y - start_y, dy, ly, true);
    add_hint_diff(&pcseg->p2, delta);
}

/*
 * Propagate a final wraparound hint back through any null line segments
 * to a possible curve.  pseg_last.pt has already been adjusted.
 */
private void
apply_final_hint(segment * pseg_last, const gs_fixed_point * pdiff)
{
    segment *pseg;

    for (pseg = pseg_last;; pseg = pseg->prev) {
	segment *prev = pseg->prev;

	switch (pseg->type) {
	    case s_curve:
		adjust_curve_end(((curve_segment *) pseg), pdiff);
		return;
	    case s_line:
	    case s_line_close:
		if (!line_is_null(prev->pt, pseg->pt))
		    return;
		add_hint_diff(&prev->pt, *pdiff);
		break;
	    default:		/* s_start */
		return;
	}
    }
}

/*
 * Handle the end of the subpath wrapping around to the start.  This is
 * ugly, messy code that we should be able to improve, but I neither see how
 * to do it nor understand how the IBM Type 1 rasterizer can produce such
 * good results without doing anything like this.
 *
 * This is a separate procedure only for readability: it is only called
 * from one place in the next procedure.
 */
private void
apply_wrapped_hints(gs_type1_state * pcis, subpath * psub, segment * pseg,
		    int hints, gs_fixed_point * pdiff)
{
    /* Some fonts don't use closepath when they should.... */
    fixed ctemp;
    bool closed =
	(pseg->type == s_line_close ||
	 ((ctemp = pseg->pt.x - psub->pt.x,
	   any_abs(ctemp) < float2fixed(0.1)) &&
	  (ctemp = pseg->pt.y - psub->pt.y,
	   any_abs(ctemp) < float2fixed(0.1))));
    segment *const pfirst = psub->next;
    int hints_first = pcis->hints_initial;

    if (closed) {
	/*
	 * Apply the union of the hints at both the end (pseg) and the start
	 * (psub) of the subpath.  Note that we have already applied hints
	 * at the end, and hints_first at the start.  However, because of
	 * hint replacement, the points might differ even if hints ==
	 * hints_first.  In this case, the initial hints take priority,
	 * because the initial segment was laid down first.
	 */
	int do_x, do_y;
	gs_fixed_point diff2;

	if_debug2('y', "[y]closing closed, hints=%d, hints_first=%d\n",
		  hints, hints_first);
	if (pcis->fh.axes_swapped)
	    do_x = HINT_HORZ, do_y = HINT_VERT;
	else
	    do_x = HINT_VERT, do_y = HINT_HORZ;
	{
	    /* Apply hints_first - hints to the end. */
	    int hints_end = hints_first & ~hints;

	    diff2.x =
		(hints_end & do_x ?
		 psub->pt.x - pcis->unmoved_start.x : 0);
	    diff2.y =
		(hints_end & do_y ?
		 psub->pt.y - pcis->unmoved_start.y : 0);
	}
	{
	    /* Apply hints - hints_first to the start. */
	    int hints_start = hints & ~hints_first;

	    pdiff->x =
		(hints_start & do_x ?
		 pseg->pt.x - pcis->unmoved_end.x : 0);
	    pdiff->y =
		(hints_start & do_y ?
		 pseg->pt.y - pcis->unmoved_end.y : 0);
	}
	add_hint_diff(&pseg->pt, diff2);
	apply_final_hint(pseg, &diff2);
	add_hint_diff(&psub->pt, *pdiff);
	/*
	 * Now align the initial and final points, to deal with hint
	 * replacement.
	 */
	diff2.x = psub->pt.x - pseg->pt.x;
	diff2.y = psub->pt.y - pseg->pt.y;
	if (diff2.x || diff2.y) {
	    /* Force the points to coincide. */
	    pseg->pt = psub->pt;
	    apply_final_hint(pseg, &diff2);
	}
    } else {
	int hints_close =
	line_hints(pcis, &pcis->unmoved_end, &pcis->unmoved_start);

	hints_close &= ~(hints | hints_first);
	if_debug3('y', "[y]closing open, hints=%d, hints_close=%d, hints_first=%d\n",
		  hints, hints_close, hints_first);
	if (hints_close) {
	    apply_hints_at(pcis, hints_close, &pseg->pt, pdiff);
	    apply_final_hint(pseg, pdiff);
	    apply_hints_at(pcis, hints_close, &psub->pt, pdiff);
	} else
	    pdiff->x = pdiff->y = 0;
    }
    if (pfirst->type == s_curve)
	adjust_curve_start((curve_segment *) pfirst, pdiff);
}

/*
 * Apply hints along a subpath.  If closing is true, consider the subpath
 * closed; if not, we may add more to the subpath later.  In the latter case,
 * don't do anything if the subpath is closed, because we already applied
 * the hints.
 */
void
type1_apply_path_hints(gs_type1_state * pcis, bool closing, gx_path * ppath)
{
    segment *pseg = pcis->hint_next;
    segment *pnext;
    subpath *const psub = ppath->current_subpath;

    /*
     * hints holds the set of hints that have already been applied (if
     * applicable) to pseg->pt, and hence should not be applied again.
     */
    int hints = pcis->hints_pending;
    gs_fixed_point diff;

    /*
     * Since unknown OtherSubrs call apply_path_hints before returning
     * to the client, and since OtherSubrs may be invoked before the
     * [h]sbw is seen, it's possible that init_done < 0, i.e., the path
     * and hint members of the state haven't been set up yet.  In this
     * case, we know there are no relevant hints.
     */
    if (pcis->init_done < 0)
	return;
    if (pseg == 0) {
	/* Start at the beginning of the subpath. */
	if (psub == 0)
	    return;
	if (psub->is_closed && !closing)
	    return;
	pseg = (segment *) psub;
	if (pseg->next == 0)
	    return;
	hints = 0;
	pcis->unmoved_start = psub->pt;
	pcis->unmoved_end = psub->pt;
    } else
	hints = pcis->hints_pending;
    diff.x = diff.y = 0;
    for (; (pnext = pseg->next) != 0; pseg = pnext) {
	int hints_next;

	/*
	 * Apply hints to the end of the previous segment (pseg)
	 * and the beginning of this one (pnext).
	 */
	gs_fixed_point dseg;

	switch (pnext->type) {
	    case s_curve:{
		    curve_segment *const pnext_curve = (curve_segment *) pnext;
		    int hints_first =
		    line_hints(pcis, &pcis->unmoved_end,
			       &pnext_curve->p1) & ~hints;
		    gs_fixed_point diff2;

		    if (pseg == (segment *) psub)
			pcis->hints_initial = hints_first;
		    if (hints_first)
			apply_hints_at(pcis, hints_first, &pseg->pt, &dseg);
		    else
			dseg.x = dseg.y = 0;
		    diff2.x = pseg->pt.x - pcis->unmoved_end.x;
		    diff2.y = pseg->pt.y - pcis->unmoved_end.y;
		    hints_next = line_hints(pcis, &pnext_curve->p2, &pnext->pt);
		    adjust_curve_start(pnext_curve, &diff2);
		    if (hints_next) {
			apply_hints_at(pcis, hints_next, &pnext_curve->p2, &diff);
			pcis->unmoved_end = pnext->pt;
			add_hint_diff(&pnext->pt, diff);
		    } else
			pcis->unmoved_end = pnext->pt;
		    break;
		}
	    case s_line_close:
		/* Undo any initial hints propagated to the end. */
		pnext->pt = pcis->unmoved_start;
	    default:		/* s_line, s_line_close */
		if (line_is_null(pnext->pt, pcis->unmoved_end)) {
		    /* This is a null line, just move it. */
		    hints_next = hints;
		    dseg.x = dseg.y = 0;	/* don't move p2 again */
		} else {
		    hints_next =
			line_hints(pcis, &pcis->unmoved_end, &pnext->pt);
		    if (hints_next & ~hints)
			apply_hints_at(pcis, hints_next & ~hints,
				       &pseg->pt, &dseg);
		    else
			dseg.x = dseg.y = 0;
		}
		if (pseg == (segment *) psub)
		    pcis->hints_initial = hints_next;
		pcis->unmoved_end = pnext->pt;
		if (hints_next)
		    apply_hints_at(pcis, hints_next, &pnext->pt, NULL);
	}
	if (pseg->type == s_curve)
	    adjust_curve_end((curve_segment *) pseg, &dseg);
	hints = hints_next;
    }
    if (closing) {
	apply_wrapped_hints(pcis, psub, pseg, hints, &diff);
	pcis->hint_next = 0;
	pcis->hints_pending = 0;
    } else {
	pcis->hint_next = pseg;
	pcis->hints_pending = hints;
    }
}

/* ------ Individual hints ------ */

private const stem_hint *search_hints(P2(stem_hint_table *, fixed));

/*
 * Adjust a point according to the relevant hints.
 * dx or dy is > 0 for the upper edge, < 0 for the lower.
 * The caller is responsible for checking use_hstem_hints or use_vstem_hints
 * and not calling the find_xxx_hints routine if this is false.
 * Note that if use_x/y_hints is false, no entries ever get made
 * in the stem hint tables, so these routines will not get called.
 */

private void
apply_vstem_hints(gs_type1_state * pcis, int dy, gs_fixed_point * ppt)
{
    fixed *pv = (pcis->fh.axes_swapped ? &ppt->y : &ppt->x);
    const stem_hint *ph = search_hints(&pcis->vstem_hints, *pv);

    if (ph != 0) {
	if_debug3('Y', "[Y]use vstem %d: %1.4f (%s)",
		  (int)(ph - &pcis->vstem_hints.data[0]),
		  fixed2float(*pv),
		  (dy == 0 ? "?!" : dy > 0 ? "upper" : "lower"));
#ifdef DEBUG
	if (dy == 0) {
	    lprintf("dy == 0 in apply_vstem_hints!\n");
	    return;
	}
#endif
	*pv += (dy > 0 ? ph->dv1 : ph->dv0);
	if_debug1('Y', " -> %1.4f\n", fixed2float(*pv));
    }
}

private void
apply_hstem_hints(gs_type1_state * pcis, int dx, gs_fixed_point * ppt)
{
    fixed *pv = (pcis->fh.axes_swapped ? &ppt->x : &ppt->y);
    const stem_hint *ph = search_hints(&pcis->hstem_hints, *pv);

    if (ph != 0) {
	if_debug3('Y', "[Y]use hstem %d: %1.4f (%s)",
		  (int)(ph - &pcis->hstem_hints.data[0]),
		  fixed2float(*pv),
		  (dx == 0 ? "?!" : dx > 0 ? "upper" : "lower"));
#ifdef DEBUG
	if (dx == 0) {
	    lprintf("dx == 0 in apply_vstem_hints!\n");
	    return;
	}
#endif
	*pv += (dx > 0 ? ph->dv1 : ph->dv0);
	if_debug1('Y', " -> %1.4f\n", fixed2float(*pv));
    }
}

/* Search one hint table for an adjustment. */
private const stem_hint *
search_hints(stem_hint_table * psht, fixed v)
{
    const stem_hint *table = &psht->data[0];
    const stem_hint *ph = table + psht->current;

    if (v >= ph->v0 && v <= ph->v1 && ph->active)
	return ph;
    /* We don't bother with binary or even up/down search, */
    /* because there won't be very many hints. */
    for (ph = &table[psht->count]; --ph >= table;)
	if (v >= ph->v0 && v <= ph->v1 && ph->active) {
	    psht->current = ph - table;
	    return ph;
	}
    return 0;
}
