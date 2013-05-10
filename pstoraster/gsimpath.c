/* Copyright (C) 1989, 1992 Aladdin Enterprises.  All rights reserved.
  
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

/* gsimpath.c */
/* Image to outline conversion for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gspath.h"

/* Define the state of the conversion process. */
typedef struct {
	/* The following are set at the beginning of the conversion. */
	gs_state *pgs;
	const byte *data;		/* image data */
	int width, height, raster;
	/* The following are updated dynamically. */
	int dx, dy;			/* X/Y increment of current run */
	int count;			/* # of steps in current run */
} status;

/* Define the scaling for the path tracer. */
/* It must be even. */
#define outline_scale 4
/* Define the length of the short strokes for turning corners. */
#define step 1

/* Forward declarations */
private int near get_pixel(P3(const status *, int, int));
private int near trace_from(P4(status *, int, int, int));
private int near add_dxdy(P4(status *, int, int, int));
#define add_deltas(s, dx, dy, n)\
  if ( (code = add_dxdy(s, dx, dy, n)) < 0 ) return code
/* Append an outline derived from an image to the current path. */
int
gs_imagepath(gs_state *pgs, int width, int height, const byte *data)
{	status stat;
	status *out = &stat;
	int code, x, y;
	/* Initialize the state. */
	stat.pgs = pgs;
	stat.data = data;
	stat.width = width;
	stat.height = height;
	stat.raster = (width + 7) / 8;
	/* Trace the cells to form an outline.  The trace goes in clockwise */
	/* order, always starting by going west along a bottom edge. */
	for ( y = height - 1; y >= 0; y-- )
	  for ( x = width - 1; x >= 0; x-- )
	   {	if ( get_pixel(out, x, y) && !get_pixel(out, x, y - 1) &&
		     (!get_pixel(out, x + 1, y) || get_pixel(out, x + 1, y - 1)) &&
		     !trace_from(out, x, y, 1)
		   )
		   {	/* Found a starting point */
			stat.count = 0;
			stat.dx = stat.dy = 0;
			if ( (code = trace_from(out, x, y, 0)) < 0 )
				return code;
			add_deltas(out, 0, 0, 1); /* force out last segment */
			if ( (code = gs_closepath(pgs)) < 0 )
				return code;
		   }
	   }
	return 0;
}

/* Get a pixel from the data.  Return 0 if outside the image. */
private int near
get_pixel(register const status *out, int x, int y)
{	if ( x < 0 || x >= out->width || y < 0 || y >= out->height )
		return 0;
	return (out->data[y * out->raster + (x >> 3)] >> (~x & 7)) & 1;
}

/* Trace a path.  If detect is true, don't draw, just return 1 if we ever */
/* encounter a starting point whose x,y follows that of the initial point */
/* in x-then-y scan order; if detect is false, actually draw the outline. */
private int near
trace_from(register status *out, int x0, int y0, int detect)
{	int x = x0, y = y0;
	int dx = -1, dy = 0;		/* initially going west */
	int part = 0;			/* how far along edge we are; */
					/* initialized only to pacify gcc */
	int code;
	if ( !detect )
	{	part = (get_pixel(out, x + 1, y - 1) ?
			outline_scale - step : step);
		code = gs_moveto(out->pgs,
				 x + 1 - part / (float)outline_scale,
				 (float)y);
		if ( code < 0 ) return code;
	}
	while ( 1 )
	   {	/* Relative to the current direction, */
		/* -dy,dx is at +90 degrees (counter-clockwise); */
		/* tx,ty is at +45 degrees; */
		/* ty,-tx is at -45 degrees (clockwise); */
		/* dy,-dx is at -90 degrees. */
		int tx = dx - dy, ty = dy + dx;
		if ( get_pixel(out, x + tx, y + ty) )
		{	/* Cell at 45 degrees is full, */
			/* go counter-clockwise. */
			if ( !detect )
			{	/* If this is a 90 degree corner set at a */
				/* 45 degree angle, avoid backtracking. */
				if ( out->dx == ty && out->dy == -tx )
				{
#define half_scale (outline_scale / 2 - step)
					out->count -= half_scale;
					add_deltas(out, tx, ty, outline_scale / 2);
#undef half_scale
				}
				else
				{	add_deltas(out, dx, dy, step - part);
					add_deltas(out, tx, ty, outline_scale - step);
				}
				part = outline_scale - step;
			}
			x += tx, y += ty;
			dx = -dy, dy += tx;
		}
		else if ( !get_pixel(out, x + dx, y + dy) )
		{	/* Cell straight ahead is empty, go clockwise. */
			if ( !detect )
			{	add_deltas(out, dx, dy, outline_scale - step - part);
				add_deltas(out, ty, -tx, step);
				part = step;
			}
			dx = dy, dy -= ty;
		}
		else
		{	/* Neither of the above, go in same direction. */
			if ( !detect )
			{	add_deltas(out, dx, dy, outline_scale);
			}
			x += dx, y += dy;
		}
		if ( dx == -step && dy == 0 && !(tx == -step && ty == -step) )
		{	/* We just turned a corner and are going west, */
			/* so the previous pixel is a starting point pixel. */
			if ( x == x0 && y == y0 ) return 0;
			if ( detect && (y > y0 || (y == y0 && x > x0)) )
				return 1;
		}
	   }
}

/* Add a (dx, dy) pair to the path being formed. */
/* Accumulate successive segments in the same direction. */
private int near
add_dxdy(register status *out, int dx, int dy, int count)
{	if ( count != 0 )
	   {	if ( dx == out->dx && dy == out->dy )
			out->count += count;
		else
		   {	if ( out->count != 0 )
			   {	int code = gs_rlineto(out->pgs,
				   out->dx * out->count / (float)outline_scale,
				   out->dy * out->count / (float)outline_scale);
				if ( code < 0 ) return code;
			   }
			out->dx = dx, out->dy = dy;
			out->count = count;
		   }
	   }
	return 0;
}
