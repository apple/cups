/* Copyright (C) 1991, 1992, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gsdps1.c */
/* Display PostScript graphics additions for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"			/* for gscoord.h */
#include "gscoord.h"
#include "gspaint.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gspath.h"
#include "gspath2.h"			/* defines interface */
#include "gzpath.h"
#include "gzstate.h"

/*
 * Define how much rounding slop setbbox should leave,
 * in device coordinates.  Because of rounding in transforming
 * path coordinates to fixed point, the minimum realistic value is:
 *
 *	#define box_rounding_slop_fixed (fixed_epsilon)
 *
 * But even this isn't enough to compensate for cumulative rounding error
 * in rmoveto or rcurveto.  Instead, we somewhat arbitrarily use:
 */
#define box_rounding_slop_fixed (fixed_epsilon * 3)

/* ------ Graphics state ------ */

/* Set the bounding box for the current path. */
int
gs_setbbox(gs_state *pgs, floatp llx, floatp lly, floatp urx, floatp ury)
{	gs_rect ubox, dbox;
	gs_fixed_rect obox, bbox;
	gx_path *ppath = pgs->path;
	int code;
	if ( llx > urx || lly > ury )
		return_error(gs_error_rangecheck);
	/* Transform box to device coordinates. */
	ubox.p.x = llx;
	ubox.p.y = lly;
	ubox.q.x = urx;
	ubox.q.y = ury;
	if ( (code = gs_bbox_transform(&ubox, &ctm_only(pgs), &dbox)) < 0 )
		return code;
	/* Round the corners in opposite directions. */
	/* Because we can't predict the magnitude of the dbox values, */
	/* we add/subtract the slop after fixing. */
	bbox.p.x =
	  (fixed)floor(dbox.p.x * fixed_scale) - box_rounding_slop_fixed;
	bbox.p.y =
	  (fixed)floor(dbox.p.y * fixed_scale) - box_rounding_slop_fixed;
	bbox.q.x =
	  (fixed)ceil(dbox.q.x * fixed_scale) + box_rounding_slop_fixed;
	bbox.q.y =
	  (fixed)ceil(dbox.q.y * fixed_scale) + box_rounding_slop_fixed;
	if ( gx_path_bbox(ppath, &obox) >= 0 )
	{	/* Take the union of the bboxes. */
		ppath->bbox.p.x = min(obox.p.x, bbox.p.x);
		ppath->bbox.p.y = min(obox.p.y, bbox.p.y);
		ppath->bbox.q.x = max(obox.q.x, bbox.q.x);
		ppath->bbox.q.y = max(obox.q.y, bbox.q.y);
	}
	else		/* empty path */
	{	/* Just set the bbox. */
		ppath->bbox.p.x = bbox.p.x;
		ppath->bbox.p.y = bbox.p.y;
		ppath->bbox.q.x = bbox.q.x;
		ppath->bbox.q.y = bbox.q.y;
	}
	ppath->bbox_set = 1;
	return 0;
}

/* ------ Rectangles ------ */

/* Append a list of rectangles to a path. */
int
gs_rectappend(gs_state *pgs, const gs_rect *pr, uint count)
{	for ( ; count != 0; count--, pr++ )
	   {	floatp px = pr->p.x, py = pr->p.y, qx = pr->q.x, qy = pr->q.y;
		int code;
		/* Ensure counter-clockwise drawing. */
		if ( (qx >= px) != (qy >= py) )
			qx = px, px = pr->q.x;	/* swap x values */
		if ( (code = gs_moveto(pgs, px, py)) < 0 ||
		     (code = gs_lineto(pgs, qx, py)) < 0 ||
		     (code = gs_lineto(pgs, qx, qy)) < 0 ||
		     (code = gs_lineto(pgs, px, qy)) < 0 ||
		     (code = gs_closepath(pgs)) < 0
		   )
			return code;
	   }
	return 0;
}

/* Clip to a list of rectangles. */
int
gs_rectclip(gs_state *pgs, const gs_rect *pr, uint count)
{	int code;
	gx_path old_path;
	old_path = *pgs->path;
	gx_path_reset(pgs->path);
	if ( (code = gs_rectappend(pgs, pr, count)) < 0 ||
	     (code = gs_clip(pgs)) < 0
	   )
	  {	gx_path_release(pgs->path);
		*pgs->path = old_path;
		return code;
	  }
	gs_newpath(pgs);
	gx_path_release(&old_path);
	return 0;
}

/* Fill a list of rectangles. */
/* (We could do this a lot more efficiently.) */
int
gs_rectfill(gs_state *pgs, const gs_rect *pr, uint count)
{	int code;
	if ( (code = gs_gsave(pgs)) < 0 ) return code;
	if ( (code = gs_newpath(pgs)) < 0 ||
	     (code = gs_rectappend(pgs, pr, count)) < 0 ||
	     (code = gs_fill(pgs)) < 0
	   )
	  DO_NOTHING;
	gs_grestore(pgs);
	return code;
}

/* Stroke a list of rectangles. */
/* (We could do this a lot more efficiently.) */
int
gs_rectstroke(gs_state *pgs, const gs_rect *pr, uint count,
  const gs_matrix *pmat)
{	int code;
	if ( (code = gs_gsave(pgs)) < 0 ) return code;
	if ( (code = gs_newpath(pgs)) < 0 ||
	     (code = gs_rectappend(pgs, pr, count)) < 0 ||
	     (pmat != NULL && (code = gs_concat(pgs, pmat)) < 0) ||
	     (code = gs_stroke(pgs)) < 0
	   )
	  DO_NOTHING;
	gs_grestore(pgs);
	return code;
}
