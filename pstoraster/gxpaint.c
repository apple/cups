/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxpaint.c,v 1.2 2000/03/08 23:15:03 mike Exp $ */
/* Graphics-state-aware fill and stroke procedures */
#include "gx.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxhttile.h"
#include "gxpaint.h"
#include "gxpath.h"

/* Fill a path. */
int
gx_fill_path(gx_path * ppath, gx_device_color * pdevc, gs_state * pgs,
	     int rule, fixed adjust_x, fixed adjust_y)
{
    gx_device *dev = gs_currentdevice_inline(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);
    gx_fill_params params;

    if (code < 0)
	return code;
    params.rule = rule;
    params.adjust.x = adjust_x;
    params.adjust.y = adjust_y;
    params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
    params.fill_zero_width = (adjust_x | adjust_y) != 0;
    return (*dev_proc(dev, fill_path))
	(dev, (const gs_imager_state *)pgs, ppath, &params, pdevc, pcpath);
}

/* Stroke a path for drawing or saving. */
int
gx_stroke_fill(gx_path * ppath, gs_state * pgs)
{
    gx_device *dev = gs_currentdevice_inline(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);
    gx_stroke_params params;

    if (code < 0)
	return code;
    params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
    return (*dev_proc(dev, stroke_path))
	(dev, (const gs_imager_state *)pgs, ppath, &params,
	 pgs->dev_color, pcpath);
}

int
gx_stroke_add(gx_path * ppath, gx_path * to_path, gs_state * pgs)
{
    gx_stroke_params params;

    params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
    return gx_stroke_path_only(ppath, to_path, pgs->device,
			       (const gs_imager_state *)pgs,
			       &params, NULL, NULL);
}
