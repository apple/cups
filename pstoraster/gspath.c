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

/* gspath.c */
/* Basic path routines for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gscoord.h"			/* requires gsmatrix.h */
#include "gzstate.h"
#include "gzpath.h"
#include "gxdevice.h"			/* for gxcpath.h */
#include "gzcpath.h"

/* ------ Miscellaneous ------ */

int
gs_newpath(gs_state *pgs)
{	gx_path_release(pgs->path);
	gx_path_init(pgs->path, pgs->memory);
	return 0;
}

int
gs_closepath(gs_state *pgs)
{	return gx_path_close_subpath(pgs->path);
}

int
gs_upmergepath(gs_state *pgs)
{	return gx_path_add_path(pgs->saved->path, pgs->path);
}

/* ------ Points and lines ------ */

int
gs_currentpoint(const gs_state *pgs, gs_point *ppt)
{	gs_fixed_point pt;
	int code = gx_path_current_point(pgs->path, &pt);
	if ( code < 0 ) return code;
	return gs_itransform((gs_state *)pgs,
			     fixed2float(pt.x), fixed2float(pt.y), ppt);
}

int
gs_moveto(gs_state *pgs, floatp x, floatp y)
{	int code;
	gs_fixed_point pt;
	if ( (code = gs_point_transform2fixed(&pgs->ctm, x, y, &pt)) >= 0 )
		code = gx_path_add_point(pgs->path, pt.x, pt.y);
	return code;
}

int
gs_rmoveto(gs_state *pgs, floatp x, floatp y)
{	int code;
	gs_fixed_point dpt;
	if ( (code = gs_distance_transform2fixed(&pgs->ctm, x, y, &dpt)) >= 0 )
		code = gx_path_add_relative_point(pgs->path, dpt.x, dpt.y);
	return code;
}

int
gs_lineto(gs_state *pgs, floatp x, floatp y)
{	int code;
	gs_fixed_point pt;
	if ( (code = gs_point_transform2fixed(&pgs->ctm, x, y, &pt)) >= 0 )
		code = gx_path_add_line(pgs->path, pt.x, pt.y);
	return code;
}

int
gs_rlineto(gs_state *pgs, floatp x, floatp y)
{	gs_fixed_point cpt, dpt;
	int code = gx_path_current_point(pgs->path, &cpt);
	if ( code < 0 ) return code;
	if ( (code = gs_distance_transform2fixed(&pgs->ctm, x, y, &dpt)) >= 0 )
		code = gx_path_add_line(pgs->path, cpt.x + dpt.x, cpt.y + dpt.y);
	return code;
}

/* ------ Curves ------ */

int
gs_curveto(gs_state *pgs,
  floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3)
{	gs_fixed_point p1, p2, p3;
	int code;
	if (	(code = gs_point_transform2fixed(&pgs->ctm, x1, y1, &p1)) < 0 ||
		(code = gs_point_transform2fixed(&pgs->ctm, x2, y2, &p2)) < 0 ||
		(code = gs_point_transform2fixed(&pgs->ctm, x3, y3, &p3)) < 0
	   ) return code;
	return gx_path_add_curve(pgs->path,
		p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

int
gs_rcurveto(gs_state *pgs,
  floatp dx1, floatp dy1, floatp dx2, floatp dy2, floatp dx3, floatp dy3)
{	gs_fixed_point pt, p1, p2, p3;
	int code = gx_path_current_point(pgs->path, &pt);
	if ( code < 0 ) return code;
	if (	(code = gs_distance_transform2fixed(&pgs->ctm, dx1, dy1, &p1)) < 0 ||
		(code = gs_distance_transform2fixed(&pgs->ctm, dx2, dy2, &p2)) < 0 ||
		(code = gs_distance_transform2fixed(&pgs->ctm, dx3, dy3, &p3)) < 0
	   ) return code;
	return gx_path_add_curve(pgs->path,
		pt.x + p1.x, pt.y + p1.y,
		pt.x + p2.x, pt.y + p2.y,
		pt.x + p3.x, pt.y + p3.y);
}

/* ------ Clipping ------ */

/* Forward references */
private int common_clip(P2(gs_state *, int));
private int set_clip_path(P3(gs_state *, gx_clip_path *, int));

int
gs_clippath(gs_state *pgs)
{	gx_path path;
	int code = gx_cpath_path(pgs->clip_path, &path);
	if ( code < 0 ) return code;
	return gx_path_copy(&path, pgs->path, 1);
}

int
gs_initclip(gs_state *pgs)
{	register gx_device *dev = gs_currentdevice(pgs);
	gs_rect bbox;
	gs_fixed_rect box;
	gs_matrix imat;

	if ( dev->ImagingBBox_set )
	{	/* Use the ImagingBBox, relative to default user space. */
		gs_defaultmatrix(pgs, &imat);
		bbox.p.x = dev->ImagingBBox[0];
		bbox.p.y = dev->ImagingBBox[1];
		bbox.q.x = dev->ImagingBBox[2];
		bbox.q.y = dev->ImagingBBox[3];
	}
	else
	{	/* Use the MediaSize indented by the HWMargins, */
		/* relative to unrotated user space adjusted by */
		/* the Margins.  (We suspect this isn't quite right, */
		/* but the whole issue of "margins" is such a mess that */
		/* we don't think we can do any better.) */
		(*dev_proc(dev, get_initial_matrix))(dev, &imat);
		/* Adjust for the Margins. */
		imat.tx += dev->Margins[0] * dev->HWResolution[0] /
		  dev->MarginsHWResolution[0];
		imat.ty += dev->Margins[1] * dev->HWResolution[1] /
		  dev->MarginsHWResolution[1];
		bbox.p.x = dev->HWMargins[0];
		bbox.p.y = dev->HWMargins[1];
		bbox.q.x = dev->MediaSize[0] - dev->HWMargins[2];
		bbox.q.y = dev->MediaSize[1] - dev->HWMargins[3];
	}
	gs_bbox_transform(&bbox, &imat, &bbox);
	/* Round the clipping box so that it doesn't get ceilinged. */
	box.p.x = fixed_rounded(float2fixed(bbox.p.x));
	box.p.y = fixed_rounded(float2fixed(bbox.p.y));
	box.q.x = fixed_rounded(float2fixed(bbox.q.x));
	box.q.y = fixed_rounded(float2fixed(bbox.q.y));
	return gx_clip_to_rectangle(pgs, &box);
}

int
gs_clip(gs_state *pgs)
{	return common_clip(pgs, gx_rule_winding_number);
}

int
gs_eoclip(gs_state *pgs)
{	return common_clip(pgs, gx_rule_even_odd);
}

private int
common_clip(gs_state *pgs, int rule)
{	gx_path fpath;
	int code = gx_path_flatten(pgs->path, &fpath, pgs->flatness);
	if ( code < 0 ) return code;
	code = gx_cpath_intersect(pgs, pgs->clip_path, &fpath, rule);
	if ( code != 1 ) gx_path_release(&fpath);
	if ( code < 0 ) return code;
	return set_clip_path(pgs, pgs->clip_path, rule);
}

int
gs_setclipoutside(gs_state *pgs, bool outside)
{	return gx_cpath_set_outside(pgs->clip_path, outside);
}

bool
gs_currentclipoutside(const gs_state *pgs)
{	return gx_cpath_is_outside(pgs->clip_path);
}

/* Establish a rectangle as the clipping path. */
/* Used by initclip and by the character and Pattern cache logic. */
int
gx_clip_to_rectangle(gs_state *pgs, gs_fixed_rect *pbox)
{	gx_clip_path cpath;
	int code = gx_cpath_from_rectangle(&cpath, pbox, pgs->memory);
	if ( code < 0 ) return code;
	gx_cpath_release(pgs->clip_path);
	return set_clip_path(pgs, &cpath, gx_rule_winding_number);
}

/* Set the clipping path to the current path, without intersecting. */
/* Currently only used by the insideness testing operators, */
/* but might be used by viewclip eventually. */
/* The algorithm is very inefficient; we'll improve it later if needed. */
int
gx_clip_to_path(gs_state *pgs)
{	gs_fixed_rect bbox;
	int code;
	if ( (code = gx_path_bbox(pgs->path, &bbox)) < 0 ||
	     (code = gx_clip_to_rectangle(pgs, &bbox)) < 0
	   )
	  return code;
	return gs_clip(pgs);
}

/* Set the clipping path (internal). */
private int
set_clip_path(gs_state *pgs, gx_clip_path *pcpath, int rule)
{	*pgs->clip_path = *pcpath;
	pgs->clip_rule = rule;
#ifdef DEBUG
if ( gs_debug_c('P') )
  {	extern void gx_cpath_print(P1(const gx_clip_path *));
	dprintf("[P]Clipping path:\n"),
	gx_cpath_print(pcpath);
  }
#endif
	return 0;
}
