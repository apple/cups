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

/* gdevrun.c */
/* Run-length encoded "device" */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"

/*
 * The pseudo-device in this file stores 8-bit "pixels" with run-length
 * encoding.  Since it may allocate less space than is required to
 * store all possible values, it may have to discard some update requests.
 */

/*
 * Define the representation of each run.  We store runs in a doubly-
 * linked list using the old trick of storing only a single pointer which
 * is the xor of the successor and predecessor indices.
 * Run 0 is a dummy end-of-line run; run 1 is a dummy start-of-line run.
 * The dummy runs have length 255 to prevent merging.
 */
typedef byte run_length;
typedef byte run_value;
typedef ushort run_index;
typedef struct run_s {
	run_length length;
	run_value value;
	run_index nix;	/* for allocated runs, xor of successor and */
			/* predecessor indices; for free runs, */
			/* index of next free run */
} run;
/*
 * Define a pointer into a run list.  The xor trick requires that we
 * store both the current index and the next (or previous) one.
 * For speed, we keep both the index of and the pointer to the current run.
 */
typedef struct run_ptr_s {
	run *ptr;
	run_index index;	/* index of current run */
	run_index next;		/* index of next run */
} run_ptr;
typedef struct const_run_ptr_s {
	const run *ptr;
	run_index index;	/* index of current run */
	run_index next;	/* index of next run */
} const_run_ptr;
/* Accessors */
#define rp_length(rp) ((rp).ptr->length)
#define rp_value(rp) ((rp).ptr->value)
#define rp_nix(rp) ((rp).ptr->nix)
/* Traversers */
#define rp_at_start(rp) ((rp).index == 1)
#define rp_at_end(rp) ((rp).index == 0)
#define rp_start(rp, data)\
  ((rp).index = (data)[1].nix,\
   (rp).ptr = (data) + (rp).index,\
   (rp).next = rp_nix(rp) ^ 1)
/* Note that rp_next and rp_prev allow rpn == rpc. */
#define rp_next(rpc, data, rpn, itemp)\
  (itemp = (rpc).index,\
   (rpn).ptr = (data) + ((rpn).index = (rpc).next),\
   (rpn).next = itemp ^ rp_nix(rpn))
#define rp_prev(rpc, data, rpp, itemp)\
  (itemp = (rpc).next ^ rp_nix(rpc),\
   (rpp).next = (rpc).index,\
   (rpp).ptr = (data) + ((rpp).index = itemp))
/* Insert/delete */
#define rp_delete_next(rpc, data, line, rpn, rpn2, itemp)\
  (rp_next(rpc, data, rpn, itemp),\
   rp_next(rpn, data, rpn2, itemp),\
   rp_nix(rpc) ^= (rpn).index ^ (rpn2).index,\
   rp_nix(rpn2) ^= (rpn).index ^ (rpc).index,\
   rp_nix(rpn) = (line)->free,\
   (line)->free = (rpn).index)
#define rp_insert_next(rpc, data, line, rpn, itemp)\
  (rp_next(rpc, data, rpn, itemp),\
   itemp = (line)->free,\
   rp_nix(rpc) ^= (rpn).index ^ itemp,\
   rp_nix(rpn) ^= (rpc).index ^ itemp,\
   (rpn).next = (rpn).index,\
   (rpn).index = itemp,\
   (rpn).ptr = (data) + itemp,\
   (line)->free = rp_nix(rpn),\
   rp_nix(rpn) = (rpc).index ^ (rpn).next)
#define rp_insert_prev(rpc, data, line, rpp, itemp)\
  (rp_prev(rpc, data, rpp, itemp),\
   itemp = (line)->free,\
   rp_nix(rpc) ^= (rpp).index ^ itemp,\
   rp_nix(rpp) ^= (rpc).index ^ itemp,\
   (rpp).ptr = (data) + itemp,\
   rp_nix(rpp) = (rpp).index ^ (rpc).index,\
   (rpp).index = itemp,\
   (line)->free = rp_nix(rpp))

/*
 * Define the state of a single scan line.
 *
 * We maintain the following invariant: if two adjacent runs have the
 * same value, the sum of their lengths is at least 256.  This may miss
 * optimality by nearly a factor of 2, but it's far easier to maintain
 * than a true optimal representation.
 *
 * For speed in the common case where nothing other than 0 is ever stored,
 * we initially don't bother to construct the runs (or the free run list)
 * for a line at all.
 */
typedef struct run_line_s {
	run *data;	/* base of runs */
	int zero;	/* 0 if line not initialized, -1 if initialized */
	uint xcur;	/* x value at cursor position */
	run_ptr rpcur;	/* cursor */
	run_index free;	/* head of free list */
} run_line;

/*
 * Define the device, built on an 8-bit memory device.
 */
typedef struct gx_device_run_s {
	gx_device_memory md;
	uint runs_per_line;
	run_line *lines;
	int umin, umax1;	/* some range of uninitialized lines */
} gx_device_run;

#define rdev ((gx_device_run *)dev)

/* Open the device. */
private int
run_open(gx_device *dev)
{	run_line *line = rdev->lines;
	run *data = (run *)rdev->md.base;
	int i;

	/*
	 * We need ceil(width / 255) runs to represent a line where all
	 * elements have the same value, +2 for the start and end runs,
	 * +2 for the check for 2 free runs when doing a replacement.
	 */
	if ( rdev->runs_per_line < (dev->width + 254) / 255 + 4 )
	  return_error(gs_error_rangecheck);
	for ( i = 0; i < dev->height; ++line, data += rdev->runs_per_line, ++i )
	{	line->data = data;
		line->zero = 0;
	}
	rdev->umin = 0;
	rdev->umax1 = dev->height;
	return 0;
}

/* Finish initializing a line.  This is a separate procedure only */
/* for readability. */
private void
run_line_initialize(gx_device *dev, int y)
{	run_line *line = &rdev->lines[y];
	run *data = line->data;
	int left = dev->width;
	run_index index = 2;
	run *rcur;

	line->zero = -1;
	data[0].length = 255;		/* see above */
	data[0].value = 0;		/* shouldn't matter */
	data[1].length = 255;
	data[1].value = 0;
	data[1].nix = 2;
	rcur = data + index;
	for ( ; left > 0; index++, rcur++, left -= 255 )
	{	rcur->length = min(left, 255);
		rcur->value = 0;
		rcur->nix = (index - 1) ^ (index + 1);
	}
	rcur->nix = index - 2;
	data[0].nix = index - 1;
	line->xcur = 0;
	line->rpcur.ptr = data + 2;
	line->rpcur.index = 2;
	line->rpcur.next = data[2].nix ^ 1;
	line->free = index;
	for ( ; index < rdev->runs_per_line; ++index )
	  data[index].nix = index + 1;
	data[index - 1].nix = 0;
	if ( y >= rdev->umin && y < rdev->umax1 )
	{	if ( y > (rdev->umin + rdev->umax1) >> 1 )
		  rdev->umax1 = y;
		else
		  rdev->umin = y + 1;
	}
}

/* Replace an interval of a line with a new value.  This is the procedure */
/* that does all the interesting work.  We assume the line has been */
/* initialized, and that 0 <= xo < xe <= dev->width. */
private int
run_fill_interval(run_line *line, int xo, int xe, run_value new)
{	run *data = line->data;
	int xc = line->xcur;
	run_ptr rpc;
	run_index itemp;
	int x0, x1;
	run_ptr rp0;

	rpc = line->rpcur;

	/* Find the run that contains xo. */

	if ( xo < xc )
	  { while ( xo < xc )
	      rp_prev(rpc, data, rpc, itemp), xc -= rp_length(rpc);
	  }
	else
	  { while ( xo >= xc + rp_length(rpc) )
	      xc += rp_length(rpc), rp_next(rpc, data, rpc, itemp);
	  }

	/*
	 * Skip runs above xo that already contain the new value.
	 * If the entire interval already has the correct value, exit.
	 * If we skip any such runs, set xo to just above them.
	 */

	for ( ; !rp_at_end(rpc) && rp_value(rpc) == new;
	      rp_next(rpc, data, rpc, itemp)
	    )
	  if ( (xo = xc += rp_length(rpc)) >= xe )
	    return 0;
	x0 = xc, rp0 = rpc;

	/* Find the run that contains xe-1. */

	while ( xe > xc + rp_length(rpc) )
	  xc += rp_length(rpc), rp_next(rpc, data, rpc, itemp);

	/*
	 * Skip runs below xe that already contain the new value.
	 * (We know that some run between xo and xe doesn't.)
	 * If we skip any such runs, set xe to just below them.
	 */

	while ( rp_prev(rpc, data, rpc, itemp), rp_value(rpc) == new )
	  xe = xc -= rp_length(rpc);
	rp_next(rpc, data, rpc, itemp);

	/*
	 * At this point, we know the following:
	 *	x0 <= xo < x0 + rp_length(rp0).
	 *	rp_value(rp0) != new.
	 *	xc <= xe-1 < xc + rp_length(rpc).
	 *	rp_value(rpc) != new.
	 * Note that rp0 and rpc may point to the same run.
	 */

	/*
	 * Check that we have enough free runs to do the replacement.
	 * In the worst case, where we have to split existing runs
	 * at both ends of the interval, two new runs are required.
	 * We just check for having at least two free runs, since this
	 * is simple and wastes at most 2 runs.
	 */

	if ( line->free == 0 || data[line->free].nix == 0 )
	  return -1;

	/* Split off any unaffected prefix of the run at rp0. */

	if ( x0 < xo )
	{	uint diff = xo - x0;
		run_value v0 = rp_value(rp0);
		run_ptr rpp;

		rp_prev(rp0, data, rpp, itemp);
		if ( rp_value(rpp) == v0 && rp_length(rpp) + diff <= 255 )
		  rp_length(rpp) += diff;
		else
		  { rp_insert_prev(rp0, data, line, rpp, itemp);
		    rp_length(rpp) = diff;
		    rp_value(rpp) = v0;
		  }
	}

	/* Split off any unaffected suffix of the run at rpc. */

	x1 = xc + rp_length(rpc);
	if ( x1 > xe )
	{	uint diff = x1 - xe;
		run_value vc = rp_value(rpc);
		run_ptr rpn;

		rp_next(rpc, data, rpn, itemp);
		if ( rp_value(rpn) == vc && rp_length(rpn) + diff <= 255 )
		  rp_length(rpn) += diff;
		else
		  { rp_insert_next(rpc, data, line, rpn, itemp);
		    rp_length(rpn) = diff;
		    rp_value(rpn) = vc;
		  }
	}

	/* Delete all runs from rp0 through rpc. */

	rp_prev(rp0, data, rp0, itemp);
	{	run_ptr rpn, rpn2;
		while ( rp0.next != rpc.next )
		  rp_delete_next(rp0, data, line, rpn, rpn2, itemp);
	}

	/*
	 * Finally, insert new runs with the new value.
	 * We need to check for one boundary case, namely,
	 * xo == x0 and the next lower run has the new value.
	 * (There's probably a way to structure the code just slightly
	 * differently to avoid this test.)
	 */

	{	uint left = xe - xo;
		if ( xo == x0 && rp_value(rp0) == new &&
		     rp_length(rp0) + left <= 255
		   )
		  rp_length(rp0) += left;
		else
		{ /*
		   * If we need more than one run, we probably should
		   * divide up the length to create more runs with length
		   * less than 255 in order to improve the chances of
		   * a later merge, but we won't bother right now.
		   */
		  do
		    {	run_ptr rpn;
			rp_insert_next(rp0, data, line, rpn, itemp);
			rp_length(rpn) = min(left, 255);
			rp_value(rpn) = new;
		    }
		  while ( (left -= 255) > 0 );
		}
	}

	return 0;
}

/* Replace a rectangle with a new value. */
private int
run_fill_rectangle(gx_device *dev, int x, int y, int w, int h,
  gx_color_index color)
{	int xe;
	run_line *line;
	int ny;

	fit_fill(dev, x, y, w, h);
	/*
	 * If the new value is 0 and the rectangle falls entirely within
	 * the uninitialized region that we're keeping track of,
	 * we can skip the entire operation.
	 */
	if ( (byte)color == 0 && y >= rdev->umin && y + h <= rdev->umax1 )
	  return 0;

	xe = x + w;
	for ( line = &rdev->lines[y], ny = h; ny > 0; ++line, --ny )
	  if ( (byte)color != line->zero )
	    { if ( line->zero == 0 )
		run_line_initialize(dev, y + h - ny);
	      run_fill_interval(line, x, xe, (byte)color);
	    }
	return 0;
}

/* Get a fully expanded scan line. */
private int
run_get_bits(gx_device *dev, int y, byte *row, byte **actual_row)
{	const run_line *line = &rdev->lines[y];
	const run *data = line->data;
	const_run_ptr rp;
	byte *q = *actual_row = row;
	run_index itemp;

	if ( line->zero == 0 )
	{	memset(row, 0, dev->width);
		return 0;
	}
	for ( rp_start(rp, data); !rp_at_end(rp);
	      rp_next(rp, data, rp, itemp)
	    )
	{ memset(q, rp_value(rp), rp_length(rp));
	  q += rp_length(rp);
	}
	return 0;
}

/* Debugging code */

#ifdef DEBUG

void
debug_print_run(const run *data, run_index index, const char *prefix)
{	const run *pr = data + index;
	dprintf5("%s%5d: length = %3d, value = %3d, nix = %5u\n",
		 prefix, index, pr->length, pr->value, pr->nix);
}

void
debug_print_run_line(const run_line *line, const char *prefix)
{	const run *data = line->data;
	dprintf5("%sruns at 0x%lx: zero = %d, free = %u, xcur = %u,\n",
		prefix, (ulong)data, line->zero, line->free, line->xcur);
	dprintf4("%s  rpcur = {ptr = 0x%lx, index = %u, next = %u}\n",
		prefix, (ulong)line->rpcur.ptr, line->rpcur.index, line->rpcur.next);
	{ const_run_ptr rpc;
	  uint itemp;
	  rp_start(rpc, data);
	  while ( !rp_at_end(rpc) )
	    { debug_print_run(data, rpc.index, prefix);
	      rp_next(rpc, data, rpc, itemp);
	    }
	}
}

#endif					/* DEBUG */
