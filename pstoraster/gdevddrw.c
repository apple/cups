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

/* gdevddrw.c */
/* Default polygon and image drawing device procedures */
#include "math_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxdcolor.h"
#include "gxdevice.h"

/* ---------------- Polygon and line drawing ---------------- */

/* Define the 'remainder' analogue of fixed_mult_quo. */
private fixed
fixed_mult_rem(fixed a, fixed b, fixed c)
{	double prod = (double)a * b;
	return (fixed)(prod - floor(prod / c) * c);
}

/*
 * Fill a trapezoid.  Requires:
 *	{left,right}->start.y <= ybot <= ytop <= {left,right}->end.y.
 * Lines where left.x >= right.x will not be drawn.  Thanks to Paul Haeberli
 * for an early floating point version of this algorithm.
 */
typedef struct trap_line_s {
	int di; fixed df;	/* dx/dy ratio = di + df/h */
	fixed ldi, ldf;		/* increment per scan line = ldi + ldf/h */
	fixed x, xf;		/* current value */
	fixed h;
} trap_line;
int
gx_default_fill_trapezoid(gx_device *dev, const gs_fixed_edge *left,
  const gs_fixed_edge *right, fixed ybot, fixed ytop, bool swap_axes,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	const fixed ymin = fixed_pixround(ybot) + fixed_half;
	const fixed ymax = fixed_pixround(ytop);
	if ( ymin >= ymax ) return 0;	/* no scan lines to sample */
   {	int iy = fixed2int_var(ymin);
	const int iy1 = fixed2int_var(ymax);
	trap_line l, r;
	int rxl, rxr, ry;
	const fixed
	  x0l = left->start.x, x1l = left->end.x,
	  x0r = right->start.x, x1r = right->end.x,
	  dxl = x1l - x0l, dxr = x1r - x0r;
	const fixed	/* partial pixel offset to first line to sample */
	  ysl = ymin - left->start.y,
	  ysr = ymin - right->start.y;
	fixed fxl;
	bool fill_direct = color_writes_pure(pdevc, lop);
	gx_color_index cindex;
	dev_proc_fill_rectangle((*fill_rect));
	int max_rect_height = 1;  /* max height to do fill as rectangle */
	int code;
	
	if_debug2('z', "[z]y=[%d,%d]\n", iy, iy1);

	if ( fill_direct )
	  cindex = pdevc->colors.pure,
	  fill_rect = dev_proc(dev, fill_rectangle);
	l.h = left->end.y - left->start.y;
	r.h = right->end.y - right->start.y;
	l.x = x0l + (fixed_half - fixed_epsilon);
	r.x = x0r + (fixed_half - fixed_epsilon);
	ry = iy;

#define fill_trap_rect(x,y,w,h)\
  (fill_direct ?\
    (swap_axes ? (*fill_rect)(dev, y, x, h, w, cindex) :\
     (*fill_rect)(dev, x, y, w, h, cindex)) :\
   swap_axes ? gx_fill_rectangle_device_rop(y, x, h, w, pdevc, dev, lop) :\
   gx_fill_rectangle_device_rop(x, y, w, h, pdevc, dev, lop))

	/* Compute the dx/dy ratios. */
	/* dx# = dx#i + (dx#f / h#). */
#define compute_dx(tl, d, ys)\
  if ( d >= 0 )\
   { if ( d < tl.h ) tl.di = 0, tl.df = d;\
     else tl.di = (int)(d / tl.h), tl.df = d - tl.di * tl.h,\
       tl.x += ys * tl.di;\
   }\
  else\
   { if ( (tl.df = d + tl.h) >= 0 /* d >= -tl.h */ ) tl.di = -1, tl.x -= ys;\
     else tl.di = (int)-((tl.h - 1 - d) / tl.h), tl.df = d - tl.di * tl.h,\
       tl.x += ys * tl.di;\
   }

	/* Compute the x offsets at the first scan line to sample. */
	/* We need to be careful in computing ys# * dx#f {/,%} h# */
	/* because the multiplication may overflow.  We know that */
	/* all the quantities involved are non-negative, and that */
	/* ys# is usually than 1 (as a fixed, of course); this gives us */
	/* a cheap conservative check for overflow in the multiplication. */
#define ymult_limit (max_fixed / fixed_1)
#define ymult_quo(ys, tl)\
  (ys < fixed_1 && tl.df < ymult_limit ? ys * tl.df / tl.h :\
   fixed_mult_quo(ys, tl.df, tl.h))

	/*
	 * It's worth checking for dxl == dxr, since this is the case
	 * for parallelograms (including stroked lines).
	 * Also check for left or right vertical edges.
	 */
	if ( fixed_floor(l.x) == fixed_pixround(x1l) )
	  { /* Left edge is vertical, we don't need to increment. */
	    l.di = 0, l.df = 0;
	    fxl = 0;
	  }
	else
	  { compute_dx(l, dxl, ysl);
	    fxl = ymult_quo(ysl, l);
	    l.x += fxl;
	  }
	if ( fixed_floor(r.x) == fixed_pixround(x1r) )
	  { /* Right edge is vertical.  If both are vertical, */
	    /* we have a rectangle. */
	    if ( l.di == 0 && l.df == 0 )
	      max_rect_height = max_int;
	    else
	      r.di = 0, r.df = 0;
	  }
	/* The test for fxl != 0 is required because the right edge */
	/* might cross some pixel centers even if the left edge doesn't. */
	else if ( dxr == dxl && fxl != 0 )
	   {	if ( l.di == 0 )
		  r.di = 0, r.df = l.df;
		else			/* too hard to do adjustments right */
		  compute_dx(r, dxr, ysr);
		if ( ysr == ysl && r.h == l.h )
		  r.x += fxl;
		else
		  r.x += ymult_quo(ysr, r);
	   }
	else
	   {	compute_dx(r, dxr, ysr);
		r.x += ymult_quo(ysr, r);
	   }
	rxl = fixed2int_var(l.x);
	rxr = fixed2int_var(r.x);

	/*
	 * Take a shortcut if we're only sampling a single scan line,
	 * or if we have a rectangle.
	 */
	if ( iy1 - iy <= max_rect_height )
	   {	iy = iy1;
		if_debug2('z', "[z]rectangle, x=[%d,%d]\n", rxl, rxr);
		goto last;
	   }

	/* Compute one line's worth of dx/dy. */
	/* dx# * fixed_1 = ld#i + (ld#f / h#). */
#define compute_ldx(tl, ys)\
  if ( tl.df < ymult_limit )\
    { if ( tl.df == 0 )	/* vertical edge, worth checking for */\
	tl.ldi = int2fixed(tl.di),\
	tl.ldf = 0,\
	tl.xf = -tl.h;\
      else\
	tl.ldi = int2fixed(tl.di) + int2fixed(tl.df) / tl.h,\
	tl.ldf = int2fixed(tl.df) % tl.h,\
	tl.xf = (ys < fixed_1 ? ys * tl.df % tl.h :\
		 fixed_mult_rem(ys, tl.df, tl.h)) - tl.h;\
    }\
  else\
    tl.ldi = int2fixed(tl.di) + fixed_mult_quo(fixed_1, tl.df, tl.h),\
    tl.ldf = fixed_mult_rem(fixed_1, tl.df, tl.h),\
    tl.xf = fixed_mult_rem(ys, tl.df, tl.h) - tl.h
	compute_ldx(l, ysl);
	if ( dxr == dxl && ysr == ysl && r.h == l.h )
	  r.ldi = l.ldi, r.ldf = l.ldf, r.xf = l.xf;
	else
	  { compute_ldx(r, ysr);
	  }
#undef compute_ldx

	while ( ++iy != iy1 )
	   {	int ixl, ixr;
#define step_line(tl)\
  tl.x += tl.ldi;\
  if ( (tl.xf += tl.ldf) >= 0 ) tl.xf -= tl.h, tl.x++;
		step_line(l);
		step_line(r);
#undef step_line
		ixl = fixed2int_var(l.x);
		ixr = fixed2int_var(r.x);
		if ( ixl != rxl || ixr != rxr )
		   {	code = fill_trap_rect(rxl, ry, rxr - rxl, iy - ry);
			if ( code < 0 ) goto xit;
			rxl = ixl, rxr = ixr, ry = iy;
		   }	
	   }
last:	code = fill_trap_rect(rxl, ry, rxr - rxl, iy - ry);
xit:	if ( code < 0 && fill_direct )
	  return_error(code);
	return_if_interrupt();
	return code;
   }
}

/* Fill a parallelogram whose points are p, p+a, p+b, and p+a+b. */
/* We should swap axes to get best accuracy, but we don't. */
/* We must be very careful to follow the center-of-pixel rule in all cases. */
int
gx_default_fill_parallelogram(gx_device *dev,
  fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	fixed t;
	fixed qx, qy, ym;
	dev_proc_fill_trapezoid((*fill_trapezoid));
	gs_fixed_edge left, right;
	int code;

	/* Ensure ay >= 0, by >= 0. */
	if ( ay < 0 )
	  px += ax, py += ay, ax = -ax, ay = -ay;
	if ( by < 0 )
	  px += bx, py += by, bx = -bx, by = -by;
	qx = px + ax + bx;
	/* Make a special fast check for rectangles. */
	if ( (ay | bx) == 0 || (by | ax) == 0 )
	{	/* If a point falls exactly on the middle of a pixel, */
		/* we must round it down, not up. */
		int rx = fixed2int_pixround(px);
		int ry = fixed2int_pixround(py);
		/* Exactly one of (ax,bx) and one of (ay,by) is non-zero. */
		int w = fixed2int_pixround(qx) - rx;
		if ( w < 0 ) rx += w, w = -w;
		return gx_fill_rectangle_device_rop(rx, ry, w,
			fixed2int_pixround(py + ay + by) - ry,
			pdevc, dev, lop);
	}
	/* Not a rectangle.  Ensure ax <= bx. */
#define swap(r, s) (t = r, r = s, s = t)
	if ( ax > bx )
	  swap(ax, bx), swap(ay, by);
	fill_trapezoid = dev_proc(dev, fill_trapezoid);
	qy = py + ay + by;
	left.start.x = right.start.x = px;
	left.start.y = right.start.y = py;
	left.end.x = px + ax;
	left.end.y = py + ay;
	right.end.x = px + bx;
	right.end.y = py + by;
#define rounded_same(p1, p2)\
  (fixed_pixround(p1) == fixed_pixround(p2))
	if ( ay < by )
	  { if ( !rounded_same(py, left.end.y) )
	      { code = (*fill_trapezoid)(dev, &left, &right, py, left.end.y,
					 false, pdevc, lop);
	        if ( code < 0 )
		  return code;
	      }
	    left.start = left.end;
	    left.end.x = qx, left.end.y = qy;
	    ym = right.end.y;
	    if ( !rounded_same(left.start.y, ym) )
	      { code = (*fill_trapezoid)(dev, &left, &right, left.start.y, ym,
					 false, pdevc, lop);
	        if ( code < 0 )
		  return code;
	      }
	    right.start = right.end;
	    right.end.x = qx, right.end.y = qy;
	  }
	else
	  { if ( !rounded_same(py, right.end.y) )
	      { code = (*fill_trapezoid)(dev, &left, &right, py, right.end.y,
					 false, pdevc, lop);
	        if ( code < 0 )
		  return code;
	      }
	    right.start = right.end;
	    right.end.x = qx, right.end.y = qy;
	    ym = left.end.y;
	    if ( !rounded_same(right.start.y, ym) )
	      { code = (*fill_trapezoid)(dev, &left, &right, right.start.y, ym,
					 false, pdevc, lop);
	        if ( code < 0 )
		  return code;
	      }
	    left.start = left.end;
	    left.end.x = qx, left.end.y = qy;
	  }
	if ( !rounded_same(ym, qy) )
	  return (*fill_trapezoid)(dev, &left, &right, ym, qy,
				   false, pdevc, lop);
	else
	  return 0;
#undef rounded_same
#undef swap
}

/* Fill a triangle whose points are p, p+a, and p+b. */
/* We should swap axes to get best accuracy, but we don't. */
int
gx_default_fill_triangle(gx_device *dev,
  fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	fixed t;
	fixed ym;
	dev_proc_fill_trapezoid((*fill_trapezoid)) =
	  dev_proc(dev, fill_trapezoid);
	gs_fixed_edge left, right;
	int code;

	/* Ensure ay >= 0, by >= 0. */
	if ( ay < 0 )
	  px += ax, py += ay, bx -= ax, by -= ay, ax = -ax, ay = -ay;
	if ( by < 0 )
	  px += bx, py += by, ax -= bx, ay -= by, bx = -bx, by = -by;
	/* Ensure ax <= bx. */
#define swap(r, s) (t = r, r = s, s = t)
	if ( ax > bx )
	  swap(ax, bx), swap(ay, by);
#undef swap
	left.start.y = right.start.y = py;
	/* Make a special check for a flat bottom or top, */
	/* which we can handle with a single call on fill_trapezoid. */
	if ( ay < by )
	  { right.end.x = px + bx, right.end.y = py + by;
	    if ( ay == 0 )
	      { if ( ax < 0 )
		  left.start.x = px + ax, right.start.x = px;
	        else
		  left.start.x = px, right.start.x = px + ax;
		left.end.x = right.end.x, left.end.y = right.end.y;
		ym = py;
	      }
	    else
	      { left.start.x = right.start.x = px;
		left.end.x = px + ax, left.end.y = py + ay;
		code = (*fill_trapezoid)(dev, &left, &right, py, left.end.y,
					 false, pdevc, lop);
		if ( code < 0 )
		  return code;
		left.start = left.end;
		left.end = right.end;
		ym = left.start.x;
	      }
	  }
	else if ( by < ay )
	  { left.end.x = px + ax, left.end.y = py + by;
	    if ( by == 0 )
	      { if ( bx < 0 )
		  left.start.x = px + bx, right.start.x = px;
	        else
		  left.start.x = px, right.start.x = px + bx;
		right.end.x = left.end.x, right.end.y = left.end.y;
		ym = py;
	      }
	    else
	      { left.start.x = right.start.x = px;
		right.end.x = px + bx, right.end.y = py + by;
		code = (*fill_trapezoid)(dev, &left, &right, py, right.end.y,
					 false, pdevc, lop);
		if ( code < 0 )
		  return code;
		right.start = right.end;
		right.end = left.end;
		ym = right.start.x;
	      }
	  }
	else		/* ay == by */
	  { left.start.x = right.start.x = px;
	    left.end.x = px + ax, left.end.y = py + ay;
	    right.end.x = px + bx, right.end.y = py + by;
	    ym = py;
	  }
	return (*fill_trapezoid)(dev, &left, &right, ym, right.end.y,
				 false, pdevc, lop);
}

/* Draw a one-pixel-wide line. */
int
gx_default_draw_thin_line(gx_device *dev,
  fixed fx0, fixed fy0, fixed fx1, fixed fy1,
  const gx_device_color *pdevc, gs_logical_operation_t lop)
{	int ix = fixed2int_var(fx0);
	int iy = fixed2int_var(fy0);
	int itox = fixed2int_var(fx1);
	int itoy = fixed2int_var(fy1);

	return_if_interrupt();
	if ( itoy == iy )		/* horizontal line */
	  { return (ix <= itox ?
		    gx_fill_rectangle_device_rop(ix, iy, itox - ix + 1, 1,
						 pdevc, dev, lop) :
		    gx_fill_rectangle_device_rop(itox, iy, ix - itox + 1, 1,
						 pdevc, dev, lop)
		    );
	  }
	if ( itox == ix )		/* vertical line */
	  { return (iy <= itoy ?
		    gx_fill_rectangle_device_rop(ix, iy, 1, itoy - iy + 1,
						 pdevc, dev, lop) :
		    gx_fill_rectangle_device_rop(ix, itoy, 1, iy - itoy + 1,
						 pdevc, dev, lop)
		    );
	  }
	if ( color_writes_pure(pdevc, lop) &&
	     (*dev_proc(dev, draw_line))(dev, ix, iy, itox, itoy,
					 pdevc->colors.pure) >= 0
	   )
	  return 0;
	{ fixed h = fy1 - fy0;
	  fixed w = fx1 - fx0;
	  fixed tf;
	  bool swap_axes;
	  gs_fixed_edge left, right;

#define fswap(a, b) tf = a, a = b, b = tf
	  if ( (w < 0 ? -w : w) <= (h < 0 ? -h : h) )
	    { if ( h < 0 )
		fswap(fx0, fx1), fswap(fy0, fy1),
		h = -h;
	      right.start.x = (left.start.x = fx0 - fixed_half) + fixed_1;
	      right.end.x = (left.end.x = fx1 - fixed_half) + fixed_1;
	      left.start.y = right.start.y = fy0;
	      left.end.y = right.end.y = fy1;
	      swap_axes = false;
	    }
	  else
	    { if ( w < 0 )
		fswap(fx0, fx1), fswap(fy0, fy1),
		w = -w;
	      right.start.x = (left.start.x = fy0 - fixed_half) + fixed_1;
	      right.end.x = (left.end.x = fy1 - fixed_half) + fixed_1;
	      left.start.y = right.start.y = fx0;
	      left.end.y = right.end.y = fx1;
	      swap_axes = true;
	    }
	  return (*dev_proc(dev, fill_trapezoid))(dev, &left, &right,
						  left.start.y, left.end.y,
						  swap_axes, pdevc, lop);
#undef fswap
	}
}

/* Stub out the obsolete procedure. */
int
gx_default_draw_line(gx_device *dev,
  int x0, int y0, int x1, int y1, gx_color_index color)
{	return -1;
}
