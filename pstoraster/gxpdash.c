/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gxpdash.c */
/* Dash expansion for paths */
#include "math_.h"
#include "gx.h"
#include "gsmatrix.h"			/* for gscoord.h */
#include "gscoord.h"
#include "gxfixed.h"
#include "gsline.h"
#include "gzline.h"
#include "gzpath.h"

/* Expand a dashed path into explicit segments. */
/* The path contains no curves. */
private int subpath_expand_dashes(P3(const subpath *, gx_path *,
				     const gs_imager_state *));
int
gx_path_expand_dashes(const gx_path *ppath_old, gx_path *ppath,
  const gs_imager_state *pis)
{	const subpath *psub;
	if ( gs_currentlineparams(pis)->dash.pattern_size == 0 )
	  return gx_path_copy(ppath_old, ppath, true);
	gx_path_init(ppath, ppath_old->memory);
	for ( psub = ppath_old->first_subpath; psub != 0;
	      psub = (const subpath *)psub->last->next
	    )
	  {	int code = subpath_expand_dashes(psub, ppath, pis);
		if ( code < 0 )
		  {	gx_path_release(ppath);
			return code;
		  }
	  }
	return 0;
}
private int
subpath_expand_dashes(const subpath *psub, gx_path *ppath,
  const gs_imager_state *pis)
{	const gx_dash_params *dash = &gs_currentlineparams(pis)->dash;
	const float *pattern = dash->pattern;
	int count, ink_on, index;
	float dist_left;
	fixed x0 = psub->pt.x, y0 = psub->pt.y;
	fixed x, y;
	const segment *pseg;
	int wrap = (dash->init_ink_on && psub->is_closed ? -1 : 0);
	int drawing = wrap;
	int code;
	if ( (code = gx_path_add_point(ppath, x0, y0)) < 0 )
		return code;
	/* To do the right thing at the beginning of a closed path, */
	/* we have to skip any initial line, and then redo it at */
	/* the end of the path.  Drawing = -1 while skipping, */
	/* 0 while drawing normally, and 1 on the second round. */
top:	count = dash->pattern_size;
	ink_on = dash->init_ink_on;
	index = dash->init_index;
	dist_left = dash->init_dist_left;
	x = x0, y = y0;
	pseg = (const segment *)psub;
	while ( (pseg = pseg->next) != 0 && pseg->type != s_start )
	   {	fixed sx = pseg->pt.x, sy = pseg->pt.y;
		fixed udx = sx - x, udy = sy - y;
		float length, dx, dy;
		float dist;
		if ( !(udx | udy) )	/* degenerate */
			dx = 0, dy = 0, length = 0;
		else
		   {	gs_point d;
			dx = udx, dy = udy;	/* scaled as fixed */
			gs_imager_idtransform(pis, dx, dy, &d);
			length = hypot(d.x, d.y) * (1 / (float)int2fixed(1));
		   }
		dist = length;
		while ( dist > dist_left )
		   {	/* We are using up the dash element */
			float fraction = dist_left / length;
			fixed nx = x + (fixed)(dx * fraction);
			fixed ny = y + (fixed)(dy * fraction);
			if ( ink_on )
			   {	if ( drawing >= 0 )
				  code = gx_path_add_line(ppath, nx, ny);
			   }
			else
			   {	if ( drawing > 0 ) return 0;	/* done */
				code = gx_path_add_point(ppath, nx, ny);
				drawing = 0;
			   }
			if ( code < 0 ) return code;
			dist -= dist_left;
			ink_on = !ink_on;
			if ( ++index == count ) index = 0;
			dist_left = pattern[index];
			x = nx, y = ny;
		   }
		dist_left -= dist;
		/* Handle the last dash of a segment. */
		if ( ink_on )
		   {	if ( drawing >= 0 )
			  code =
			    (pseg->type == s_line_close && drawing > 0 ?
			     gx_path_close_subpath(ppath) :
			     gx_path_add_line(ppath, sx, sy));
		   }
		else
		   {	if ( drawing > 0 ) return 0;	/* done */
			code = gx_path_add_point(ppath, sx, sy);
			drawing = 0;
		   }
		if ( code < 0 ) return code;
		x = sx, y = sy;
	   }
	/* Check for wraparound. */
	if ( wrap && drawing <= 0 )
	   {	/* We skipped some initial lines. */
		/* Go back and do them now. */
		drawing = 1;
		goto top;
	   }
	return 0;
}

