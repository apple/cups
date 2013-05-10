/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gspath1.c */
/* Additional PostScript Level 1 path routines for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gxfixed.h"
#include "gxfarith.h"
#include "gxmatrix.h"
#include "gzstate.h"
#include "gspath.h"
#include "gzpath.h"
#include "gscoord.h"            /* gs_itransform prototype */

/* ------ Arcs ------ */

/* Conversion parameters */
#define degrees_to_radians (M_PI / 180.0)

/* Forward declarations */
/*
 * Because of an obscure bug in the IBM RS/6000 compiler, the int argument
 * for arc_add must come before the floatp arguments.
 */
typedef enum {
  arc_nothing,
  arc_moveto,
  arc_lineto
} arc_action;
private int arc_add(P9(gs_state *, arc_action,
  floatp, floatp, floatp, floatp, floatp, floatp, floatp));

int
gs_arc(gs_state *pgs,
  floatp xc, floatp yc, floatp r, floatp ang1, floatp ang2)
{	return gs_arc_add(pgs, false, xc, yc, r, ang1, ang2, true);
}

int
gs_arcn(gs_state *pgs,
  floatp xc, floatp yc, floatp r, floatp ang1, floatp ang2)
{	return gs_arc_add(pgs, true, xc, yc, r, ang1, ang2, true);
}

int
gs_arc_add(gs_state *pgs, bool clockwise, floatp axc, floatp ayc, floatp arad,
  floatp aang1, floatp aang2, bool add_line)
{	float ar = arad;
	fixed ang1 = float2fixed(aang1), ang2 = float2fixed(aang2), adiff;
	float ang1r;		/* reduced angle */
	gs_sincos_t sincos;
	float x0, y0, sin0, cos0;
	float x3r, y3r;
	arc_action action = (add_line ? arc_lineto : arc_moveto);
	int code;

#define fixed_90 int2fixed(90)
#define fixed_180 int2fixed(180)
#define fixed_360 int2fixed(360)
	if ( ar < 0 )
	   {	ang1 += fixed_180;
		ang2 += fixed_180;
		ar = - ar;
	   }
	ang1r = fixed2float(ang1 % fixed_360);
	gs_sincos_degrees(ang1r, &sincos);
	sin0 = ar * sincos.sin, cos0 = ar * sincos.cos;
	x0 = axc + cos0, y0 = ayc + sin0;
	if ( clockwise )
	   {	/* Quadrant reduction */
		while ( ang1 < ang2 ) ang2 -= fixed_360;
		while ( (adiff = ang2 - ang1) < -fixed_90 )
		   {	float w = sin0; sin0 = -cos0; cos0 = w;
			x3r = axc + cos0, y3r = ayc + sin0;
			code = arc_add(pgs, action, ar, x0, y0, x3r, y3r,
				(x0 + cos0),
				(y0 + sin0));
			if ( code < 0 ) return code;
			x0 = x3r, y0 = y3r;
			ang1 -= fixed_90;
			action = arc_nothing;
		   }
	   }
	else
	   {	/* Quadrant reduction */
		while ( ang2 < ang1 ) ang2 += fixed_360;
		while ( (adiff = ang2 - ang1) > fixed_90 )
		   {	float w = cos0; cos0 = -sin0; sin0 = w;
			x3r = axc + cos0, y3r = ayc + sin0;
			code = arc_add(pgs, action, ar, x0, y0, x3r, y3r,
				(x0 + cos0),
				(y0 + sin0));
			if ( code < 0 ) return code;
			x0 = x3r, y0 = y3r;
			ang1 += fixed_90;
			action = arc_nothing;
		   }
	   }
	/* Compute the intersection of the tangents. */
	/* We define xt and yt as separate variables to work around */
	/* a floating point bug in one of the SPARC compilers. */
	/* We know that -fixed_90 <= adiff <= fixed_90. */
	   {	double trad =
		  tan(fixed2float(adiff) * (degrees_to_radians / 2));
		double xt = x0 - trad * sin0, yt = y0 + trad * cos0;
		gs_sincos_degrees(fixed2float(ang2), &sincos);
		code = arc_add(pgs, action, ar, x0, y0,
			       (axc + ar * sincos.cos),
			       (ayc + ar * sincos.sin),
			       xt, yt);
	   }
	return code;
}

int
gs_arcto(gs_state *pgs,
  floatp ax1, floatp ay1, floatp ax2, floatp ay2, floatp arad, float retxy[4])
{	float xt0, yt0, xt2, yt2;
	gs_point up0;
#define ax0 up0.x
#define ay0 up0.y
	int code;
	if ( arad < 0 )
		return_error(gs_error_undefinedresult);
	/* Transform the current point back into user coordinates */
	if ( (code = gs_currentpoint(pgs, &up0)) < 0 ) return code;
	   {	/* Now we have to compute the tangent points. */
		/* Basically, the idea is to compute the tangent */
		/* of the bisector by using tan(x+y) and tan(z/2) */
		/* formulas, without ever using any trig. */
		float dx0 = ax0 - ax1, dy0 = ay0 - ay1;
		float dx2 = ax2 - ax1, dy2 = ay2 - ay1;
		/* Compute the squared lengths from p1 to p0 and p2. */
		double sql0 = dx0 * dx0 + dy0 * dy0;
		double sql2 = dx2 * dx2 + dy2 * dy2;
		/* Compute the distance from p1 to the tangent points. */
		/* This is the only hairy part. */
		double num = dy0 * dx2 - dy2 * dx0;
		double denom = sqrt(sql0 * sql2) - (dx0 * dx2 + dy0 * dy2);
		/* Check for collinear points. */
		if ( fabs(num) < 1.0e-6 || fabs(denom) < 1.0e-6 )
		   {	gs_fixed_point pt;
			code = gs_point_transform2fixed(&pgs->ctm, ax1, ay1, &pt);
			if ( code >= 0 ) code = gx_path_add_line(pgs->path, pt.x, pt.y);
			xt0 = xt2 = ax1;
			yt0 = yt2 = ay1;
		   }
		else		/* not collinear */
		   {	double dist = fabs(arad * num / denom);
			double l0 = dist / sqrt(sql0), l2 = dist / sqrt(sql2);
			xt0 = ax1 + dx0 * l0;
			yt0 = ay1 + dy0 * l0;
			xt2 = ax1 + dx2 * l2;
			yt2 = ay1 + dy2 * l2;
			code = arc_add(pgs, arc_lineto, arad, xt0, yt0, xt2, yt2, ax1, ay1);
		   }
	   }
	if ( retxy != 0 )
	   {	retxy[0] = xt0;
		retxy[1] = yt0;
		retxy[2] = xt2;
		retxy[3] = yt2;
	   }
	return code;
}

/* Internal routine for adding an arc to the path. */
private int
arc_add(gs_state *pgs, arc_action action,
  floatp r, floatp x0, floatp y0, floatp x3, floatp y3, floatp xt, floatp yt)
{	gx_path *path = pgs->path;
	floatp dx = xt - x0, dy = yt - y0;
	double dist = dx * dx + dy * dy;
	double r2 = r * r;
	floatp fraction;
	gs_fixed_point p0, p3, pt, cpt;
	int code;
	/* Compute the fraction coefficient for the curve. */
	/* See gx_path_add_partial_arc for details. */
	if ( dist >= r2 * 1.0e8 )	/* almost zero radius; */
				/* the >= catches dist == r == 0 */
	  fraction = 0.0;
	else
	  fraction = (4.0/3.0) / (1 + sqrt(1 + dist / r2));
	if_debug8('r',
		  "[r]Arc f=%f p0=(%f,%f) pt=(%f,%f) p3=(%f,%f) action=%d\n",
		  fraction, x0, y0, xt, yt, x3, y3, (int)action);
	if (	(code = gs_point_transform2fixed(&pgs->ctm, x0, y0, &p0)) < 0 ||
		(code = gs_point_transform2fixed(&pgs->ctm, x3, y3, &p3)) < 0 ||
		(code = gs_point_transform2fixed(&pgs->ctm, xt, yt, &pt)) < 0 ||
		(code =
		 (action == arc_nothing ? 0 :
		  action == arc_lineto &&
		   gx_path_current_point(path, &cpt) >= 0 ?
		  gx_path_add_line(path, p0.x, p0.y) :
		  /* action == arc_moveto */
		  gx_path_add_point(path, p0.x, p0.y))) < 0
	   )
	  return code;
	return gx_path_add_partial_arc(path, p3.x, p3.y, pt.x, pt.y, fraction);
}

/* ------ Path transformers ------ */

int
gs_dashpath(gs_state *pgs)
{	gx_path fpath;
	int code;

	if ( gs_currentdash_length == 0 )
	  return 0;			/* no dash pattern */
	code = gs_flattenpath(pgs);
	if ( code < 0 )
	  return code;
	code = gx_path_expand_dashes(pgs->path, &fpath,
				     (const gs_imager_state *)pgs);
	if ( code < 0 )
	  return code;
	gx_path_release(pgs->path);
	*pgs->path = fpath;
	return 0;
}

int
gs_flattenpath(gs_state *pgs)
{	gx_path fpath;
	int code;
	if ( !pgs->path->curve_count ) return 0;	/* no curves */
	code = gx_path_flatten(pgs->path, &fpath, pgs->flatness);
	if ( code < 0 ) return code;
	gx_path_release(pgs->path);
	*pgs->path = fpath;
	return 0;
}

int
gs_reversepath(gs_state *pgs)
{	gx_path rpath;
	int code = gx_path_copy_reversed(pgs->path, &rpath, 1);
	if ( code < 0 ) return code;
	gx_path_release(pgs->path);
	*pgs->path = rpath;
	return 0;
}

/* ------ Accessors ------ */

int
gs_upathbbox(gs_state *pgs, gs_rect *pbox, bool include_moveto)
{	gs_fixed_rect fbox;		/* box in device coordinates */
	gs_rect dbox;
	int code = gx_path_bbox(pgs->path, &fbox);

	if ( code < 0 )
	  return code;
	/* If the path ends with a moveto and include_moveto is true, */
	/* include the moveto in the bounding box. */
	if ( pgs->path->subpath_open < 0 && include_moveto )
	  { gs_fixed_point pt;
	    gx_path_current_point_inline(pgs->path, &pt);
	    if ( pt.x < fbox.p.x )
	      fbox.p.x = pt.x;
	    if ( pt.y < fbox.p.y )
	      fbox.p.y = pt.y;
	    if ( pt.x > fbox.q.x )
	      fbox.q.x = pt.x;
	    if ( pt.y > fbox.q.y )
	      fbox.q.y = pt.y;
	  }
	/* Transform the result back to user coordinates. */
	dbox.p.x = fixed2float(fbox.p.x);
	dbox.p.y = fixed2float(fbox.p.y);
	dbox.q.x = fixed2float(fbox.q.x);
	dbox.q.y = fixed2float(fbox.q.y);
	return gs_bbox_transform_inverse(&dbox, &ctm_only(pgs), pbox);
}

/* ------ Enumerators ------ */

/* Start enumerating a path */
int
gs_path_enum_init(gs_path_enum *penum, const gs_state *pgs)
{	gx_path *copied_path = gx_path_alloc(pgs->memory, "gs_path_enum_init");
	int code;

	if ( copied_path == 0 )
	  return_error(gs_error_VMerror);
	code = gx_path_copy(pgs->path, copied_path, 1);
	if ( code < 0 )
	{	gs_free_object(pgs->memory, copied_path, "gs_path_enum_init");
		return code;
	}
	gx_path_enum_init(penum, copied_path);
	penum->pgs = pgs;
	penum->copied_path = copied_path;
	return 0;
}

/* Enumerate the next element of a path. */
/* If the path is finished, return 0; */
/* otherwise, return the element type. */
int
gs_path_enum_next(gs_path_enum *penum, gs_point ppts[3])
{	gs_fixed_point fpts[3];
	gs_state *pgs = (gs_state *)penum->pgs;		/* discard const */
	int pe_op = gx_path_enum_next(penum, fpts);
	int code;

	switch ( pe_op )
	  {
	case 0:				/* all done */
	case gs_pe_closepath:
		break;
	case gs_pe_curveto:
		if ( (code = gs_itransform(pgs,
					   fixed2float(fpts[1].x),
					   fixed2float(fpts[1].y),
					   &ppts[1])) < 0 ||
		     (code = gs_itransform(pgs,
					   fixed2float(fpts[2].x),
					   fixed2float(fpts[2].y),
					   &ppts[2])) < 0 )
		  return code;
	case gs_pe_moveto:
	case gs_pe_lineto:
		if ( (code = gs_itransform(pgs,
					   fixed2float(fpts[0].x),
					   fixed2float(fpts[0].y),
					   &ppts[0])) < 0 )
		  return code;
	default:			/* error */
		break;
	   }
	return pe_op;
}

/* Clean up after a pathforall. */
void
gs_path_enum_cleanup(gs_path_enum *penum)
{	if ( penum->copied_path != 0 )		/* don't do it twice ... */
						/* shouldn't be needed! */
	{	gx_path_release(penum->copied_path);
		gs_free_object(penum->pgs->memory, penum->copied_path,
			       "gs_path_enum_cleanup");
		penum->path = 0;
		penum->copied_path = 0;
	}
}
