/* Copyright (C) 1994, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage4.c */
/* 12-bit image procedures */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxfrac.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gzstate.h"
#include "gxcmap.h"
#include "gzpath.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"

/* ---------------- Unpacking procedures ---------------- */

void
image_unpack_12(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, register int spread, uint inpos)
{	register frac *bufp = (frac *)(bptr + inpos * 2 / 3 * spread);
#define inc_bufp(bp, n) bp = (frac *)((byte *)(bp) + (n))
	register uint sample;
	register int left = dsize;
	static const frac bits2frac_4[16] = {
#define frac15(n) ((frac_1 / 15) * (n))
		frac15(0), frac15(1), frac15(2), frac15(3),
		frac15(4), frac15(5), frac15(6), frac15(7),
		frac15(8), frac15(9), frac15(10), frac15(11),
		frac15(12), frac15(13), frac15(14), frac15(15)
#undef frac15
	};
	/* We have to deal with the 3 cases of inpos % 3 individually. */
	/* Let N = inpos / 3. */
	switch ( inpos % 3 )
	{
	case 1:
		/* bufp points to frac N, which was already filled */
		/* with the leftover byte from the previous call. */
		sample = (frac2byte(*bufp) << 4) + (*data >> 4);
		*bufp = bits2frac(sample, 12);
		inc_bufp(bufp, spread);
		*bufp = bits2frac_4[*data++ & 0xf];
		if ( !--left ) return;
	case 2:
		/* bufp points to frac N+1, which was half-filled */
		/* with the second leftover byte from the previous call. */
		sample = (frac2bits(*bufp, 4) << 8) + *data++;
		*bufp = bits2frac(sample, 12);
		inc_bufp(bufp, spread);
		--left;
	case 0:
		/* Nothing special to do. */
		;
	}
	while ( left >= 3 )
	{	sample = ((uint)*data << 4) + (data[1] >> 4);
		*bufp = bits2frac(sample, 12);
		inc_bufp(bufp, spread);
		sample = ((uint)(data[1] & 0xf) << 8) + data[2];
		*bufp = bits2frac(sample, 12);
		inc_bufp(bufp, spread);
		data += 3;
		left -= 3;
	}
	/* Handle trailing bytes. */
	switch ( left )
	{
	case 2:				/* dddddddd ddddxxxx */
		sample = ((uint)*data << 4) + (data[1] >> 4);
		*bufp = bits2frac(sample, 12);
		inc_bufp(bufp, spread);
		*bufp = bits2frac_4[data[1] & 0xf];
		break;
	case 1:				/* dddddddd */
		sample = (uint)*data << 4;
		*bufp = bits2frac(sample, 12);
		break;
	case 0:				/* Nothing more to do. */
		;
	}
}

/* ---------------- Rendering procedures ---------------- */

/* ------ Rendering for 12-bit samples ------ */

/* Render an image with more than 8 bits per sample. */
/* The samples have been expanded into fracs. */
#define longs_per_4_fracs (arch_sizeof_frac * 4 / arch_sizeof_long)
typedef union {
	frac v[4];
	long all[longs_per_4_fracs];		/* for fast comparison */
} color_fracs;
#if longs_per_4_fracs == 1
#  define color_frac_eq(f1, f2)\
     ((f1).all[0] == (f2).all[0])
#else
#if longs_per_4_fracs == 2
#  define color_frac_eq(f1, f2)\
     ((f1).all[0] == (f2).all[0] && (f1).all[1] == (f2).all[1])
#endif
#endif
int
image_render_frac(gx_image_enum *penum, byte *buffer, uint w, int h,
  gx_device *dev)
{	gs_state *pgs = penum->pgs;
	const gs_imager_state *pis = penum->pis;
	gs_logical_operation_t lop = pis->log_op;
	gx_dda_fixed next_x, next_y;
	image_posture posture = penum->posture;
	fixed xl = penum->xcur;
	fixed ytf = penum->ycur;
	fixed pdyx, pdyy;		/* edge of parallelogram */
	int yt = penum->yci, iht = penum->hci;
	const gs_color_space *pcs = penum->pcs;
	cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
	gs_client_color cc;
	int device_color = penum->device_color;
	const gx_color_map_procs *cmap_procs = gx_device_cmap_procs(dev);
	cmap_proc_rgb((*map_rgb)) = cmap_procs->map_rgb;
	cmap_proc_cmyk((*map_cmyk)) = cmap_procs->map_cmyk;
	gx_device_color devc1, devc2;
	gx_device_color _ss *spdevc = &devc1;
	gx_device_color _ss *spdevc_next = &devc2;
#define pdevc ((gx_device_color *)spdevc)
#define pdevc_next ((gx_device_color *)spdevc_next)
	int spp = penum->spp;
	const frac *psrc = (frac *)buffer;
	fixed xrun = xl;		/* x at start of run */
	int irun = fixed2int_var_rounded(xrun);	/* int xrun */
	fixed yrun = ytf;		/* y ditto */
	color_fracs run;		/* run value */
	color_fracs next;		/* next sample value */
	frac *bufend = (frac *)buffer + w;

	if ( h == 0 )
	  return 0;
	dda_init(next_x, xl, penum->row_extent.x, penum->width);
	dda_init(next_y, ytf, penum->row_extent.y, penum->width);
	pdyx = dda_current(penum->next_x) - xl;
	pdyy = dda_current(penum->next_y) - ytf;
	bufend[0] = ~bufend[-spp];	/* force end of run */
	if_debug4('b', "[b]y=%d w=%d xt=%f yt=%f\n",
		  penum->y, w, fixed2float(xl), fixed2float(ytf));
	run.v[0] = run.v[1] = run.v[2] = run.v[3] = 0;
	next.v[0] = next.v[1] = next.v[2] = next.v[3] = 0;
	cc.paint.values[0] = cc.paint.values[1] =
	  cc.paint.values[2] = cc.paint.values[3] = 0;
	cc.pattern = 0;
	(*remap_color)(&cc, pcs, pdevc, pgs);
	run.v[0] = ~psrc[0];		/* force remap */

	while ( psrc <= bufend )	/* 1 extra iteration */
				/* to handle final run */
	{	next.v[0] = psrc[0];
		switch ( spp )
		{
		case 4:			/* cmyk */
			next.v[1] = psrc[1];
			next.v[2] = psrc[2];
			next.v[3] = psrc[3];
			psrc += 4;
			if ( color_frac_eq(next, run) ) goto inc;
			if ( device_color )
			{	(*map_cmyk)(next.v[0], next.v[1],
					    next.v[2], next.v[3],
					    pdevc_next, pgs);
				goto f;
			}
			decode_frac(next.v[0], cc, 0);
			decode_frac(next.v[1], cc, 1);
			decode_frac(next.v[2], cc, 2);
			decode_frac(next.v[3], cc, 3);
			if_debug4('B', "[B]cc[0..3]=%g,%g,%g,%g\n",
				  cc.paint.values[0], cc.paint.values[1],
				  cc.paint.values[2], cc.paint.values[3]);
			if_debug1('B', "[B]cc[3]=%g\n",
				  cc.paint.values[3]);
			break;
		case 3:			/* rgb */
			next.v[1] = psrc[1];
			next.v[2] = psrc[2];
			psrc += 3;
			if ( color_frac_eq(next, run) ) goto inc;
			if ( device_color )
			{	(*map_rgb)(next.v[0], next.v[1],
					   next.v[2], pdevc_next, pgs);
				goto f;
			}
			decode_frac(next.v[0], cc, 0);
			decode_frac(next.v[1], cc, 1);
			decode_frac(next.v[2], cc, 2);
			if_debug3('B', "[B]cc[0..2]=%g,%g,%g\n",
				  cc.paint.values[0], cc.paint.values[1],
				  cc.paint.values[2]);
			break;
		case 1:			/* gray */
			psrc++;
			if ( next.v[0] == run.v[0] ) goto inc;
			if ( device_color )
			{	(*map_rgb)(next.v[0], next.v[0],
					   next.v[0], pdevc_next, pgs);
				goto f;
			}
			decode_frac(next.v[0], cc, 0);
			if_debug1('B', "[B]cc[0]=%g\n",
				  cc.paint.values[0]);
			break;
		}
		(*remap_color)(&cc, pcs, pdevc_next, pgs);
f:		if_debug7('B', "[B]0x%x,0x%x,0x%x,0x%x -> %ld,%ld,0x%lx\n",
			next.v[0], next.v[1], next.v[2], next.v[3],
			pdevc_next->colors.binary.color[0],
			pdevc_next->colors.binary.color[1],
			(ulong)pdevc_next->type);
		/* Even though the supplied colors don't match, */
		/* the device colors might. */
		if ( !dev_color_eq(devc1, devc2) ||
		     psrc > bufend	/* force end of last run */
		   )
		{	/* Fill the region between */
			/* xrun/irun and xl */
			gx_device_color _ss *sptemp;
			int code;
			if ( posture != image_portrait )
			{	/* Parallelogram */
				code = (*dev_proc(dev, fill_parallelogram))
				  (dev, xrun, yrun,
				   xl - xrun, ytf - yrun, pdyx, pdyy,
				   pdevc, lop);
				xrun = xl;
				yrun = ytf;
			}
				else
			{	/* Rectangle */
				int xi = irun;
				int wi = (irun = fixed2int_var_rounded(xl)) - xi;
				if ( wi < 0 ) xi += wi, wi = -wi;
				code = gx_fill_rectangle_device_rop(xi, yt,
						wi, iht, pdevc, dev, lop);
			}
			if ( code < 0 )
			  return code;
			sptemp = spdevc;
			spdevc = spdevc_next;
			spdevc_next = sptemp;
		}
		run = next;
inc:		xl = dda_next(next_x);
		ytf = dda_next(next_y);
	}
	return 1;
}
