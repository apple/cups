/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gxpaint.c */
/* Graphics-state-aware fill and stroke procedures */
#include "gx.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxhttile.h"
#include "gxpaint.h"
#include "gxpath.h"

/* Fill a path. */
int
gx_fill_path(gx_path *ppath, gx_device_color *pdevc, gs_state *pgs,
  int rule, fixed adjust_x, fixed adjust_y)
{	gx_device *dev = gs_currentdevice_inline(pgs);
	gx_fill_params params;

	params.rule = rule;
	params.adjust.x = adjust_x;
	params.adjust.y = adjust_y;
	params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
	params.fill_zero_width = true;
	return (*dev_proc(dev, fill_path))
	  (dev, (const gs_imager_state *)pgs, ppath, &params, pdevc,
	   pgs->clip_path);
}

/* Stroke a path for drawing or saving. */
int
gx_stroke_fill(gx_path *ppath, gs_state *pgs)
{	const gx_clip_path *pcpath = pgs->clip_path;
	gx_device *dev = gs_currentdevice_inline(pgs);
	gx_stroke_params params;
	gx_device_color *pdevc = pgs->dev_color;
	int code;

	params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
	if ( dev->std_procs.stroke_path != gx_default_stroke_path &&
	     pgs->log_op == lop_default
	   )
	  { /* Give the device a chance to handle it. */
	    code = (*dev_proc(dev, stroke_path))
	      (dev, (const gs_imager_state *)pgs, ppath, &params, pdevc,
	       pcpath);
	    if ( code >= 0 )
	      return code;
	  }
	return gx_stroke_path_only(ppath, (gx_path *)0, dev,
				   (const gs_imager_state *)pgs,
				   &params, pdevc, pcpath);
}

int
gx_stroke_add(gx_path *ppath, gx_path *to_path, gs_state *pgs)
{	gx_stroke_params params;
	int code;

	params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
	code = gx_stroke_path_only(ppath, to_path, pgs->device,
				   (const gs_imager_state *)pgs,
				   &params, NULL, NULL);
	/* I don't understand why this code used to be here: */
#if 0
	if ( code < 0 )
	  return code;
	if ( ppath->subpath_open <= 0 && ppath->position_valid )
	  code = gx_path_add_point(to_path, ppath->position.x,
				   ppath->position.y);
#endif
	return code;
}
