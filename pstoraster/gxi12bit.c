/* Copyright (C) 1994, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxi12bit.c,v 1.1 2000/03/08 23:15:00 mike Exp $ */
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
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gzpath.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"

/* ---------------- Unpacking procedures ---------------- */

private const byte *
sample_unpack_12(byte * bptr, int *pdata_x, const byte * data,
		 int data_x, uint dsize, const sample_lookup_t * ignore_ptab,
		 int spread)
{
    register frac *bufp = (frac *) bptr;
    uint dskip = (data_x >> 1) * 3;
    const byte *psrc = data + dskip;
#define inc_bufp(bp, n) bp = (frac *)((byte *)(bp) + (n))
    uint sample;
    int left = dsize - dskip;

    if ((data_x & 1) && left > 0)
	switch (left) {
	    default:
		sample = ((uint) (psrc[1] & 0xf) << 8) + psrc[2];
		*bufp = bits2frac(sample, 12);
		inc_bufp(bufp, spread);
		psrc += 3;
		left -= 3;
		break;
	    case 2:		/* xxxxxxxx xxxxdddd */
		*bufp = (psrc[1] & 0xf) * (frac_1 / 15);
	    case 1:		/* xxxxxxxx */
		left = 0;
	}
    while (left >= 3) {
	sample = ((uint) * psrc << 4) + (psrc[1] >> 4);
	*bufp = bits2frac(sample, 12);
	inc_bufp(bufp, spread);
	sample = ((uint) (psrc[1] & 0xf) << 8) + psrc[2];
	*bufp = bits2frac(sample, 12);
	inc_bufp(bufp, spread);
	psrc += 3;
	left -= 3;
    }
    /* Handle trailing bytes. */
    switch (left) {
	case 2:		/* dddddddd ddddxxxx */
	    sample = ((uint) * psrc << 4) + (psrc[1] >> 4);
	    *bufp = bits2frac(sample, 12);
	    inc_bufp(bufp, spread);
	    *bufp = (psrc[1] & 0xf) * (frac_1 / 15);
	    break;
	case 1:		/* dddddddd */
	    sample = (uint) * psrc << 4;
	    *bufp = bits2frac(sample, 12);
	    break;
	case 0:		/* Nothing more to do. */
	    ;
    }
    *pdata_x = 0;
    return bptr;
}

/* ------ Strategy procedure ------ */

/* Use special (slow) logic for 12-bit source values. */
private irender_proc(image_render_frac);
private irender_proc_t
image_strategy_frac(gx_image_enum * penum)
{
    if (penum->bps > 8) {
	if_debug0('b', "[b]render=frac\n");
	return image_render_frac;
    }
    return 0;
}

void
gs_gxi12bit_init(gs_memory_t * mem)
{
    image_strategies.fracs = image_strategy_frac;
    sample_unpack_12_proc = sample_unpack_12;
}

/* ---------------- Rendering procedures ---------------- */

/* ------ Rendering for 12-bit samples ------ */

/* Render an image with more than 8 bits per sample. */
/* The samples have been expanded into fracs. */
#define longs_per_4_fracs (arch_sizeof_frac * 4 / arch_sizeof_long)
typedef union {
    frac v[4];
    long all[longs_per_4_fracs];	/* for fast comparison */
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
private int
image_render_frac(gx_image_enum * penum, const byte * buffer, int data_x,
		  uint w, int h, gx_device * dev)
{
    const gs_imager_state *pis = penum->pis;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xl, ytf;
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
    gx_device_color *pdevc = &devc1;
    gx_device_color *pdevc_next = &devc2;
    int spp = penum->spp;
    const frac *psrc = (const frac *)buffer + data_x * spp;
    fixed xrun;			/* x at start of run */
    int irun;			/* int xrun */
    fixed yrun;			/* y ditto */
    color_fracs run;		/* run value */
    color_fracs next;		/* next sample value */
    const frac *bufend = psrc + w;
    int code;

    if (h == 0)
	return 0;
    pnext = penum->dda.pixel0;
    xrun = xl = dda_current(pnext.x);
    irun = fixed2int_var_rounded(xrun);
    yrun = ytf = dda_current(pnext.y);
    pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
    pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
    if_debug4('b', "[b]y=%d w=%d xt=%f yt=%f\n",
	      penum->y, w, fixed2float(xl), fixed2float(ytf));
    run.v[0] = run.v[1] = run.v[2] = run.v[3] = 0;
    next.v[0] = next.v[1] = next.v[2] = next.v[3] = 0;
    cc.paint.values[0] = cc.paint.values[1] =
	cc.paint.values[2] = cc.paint.values[3] = 0;
    cc.pattern = 0;
    (*remap_color) (&cc, pcs, pdevc, pis, dev, gs_color_select_source);
    run.v[0] = ~psrc[0];	/* force remap */

    while (psrc < bufend) {
	next.v[0] = psrc[0];
	switch (spp) {
	    case 4:		/* cmyk */
		next.v[1] = psrc[1];
		next.v[2] = psrc[2];
		next.v[3] = psrc[3];
		psrc += 4;
		if (color_frac_eq(next, run))
		    goto inc;
		if (device_color) {
		    (*map_cmyk) (next.v[0], next.v[1],
				 next.v[2], next.v[3],
				 pdevc_next, pis, dev,
				 gs_color_select_source);
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
	    case 3:		/* rgb */
		next.v[1] = psrc[1];
		next.v[2] = psrc[2];
		psrc += 3;
		if (color_frac_eq(next, run))
		    goto inc;
		if (device_color) {
		    (*map_rgb) (next.v[0], next.v[1],
				next.v[2], pdevc_next, pis, dev,
				gs_color_select_source);
		    goto f;
		}
		decode_frac(next.v[0], cc, 0);
		decode_frac(next.v[1], cc, 1);
		decode_frac(next.v[2], cc, 2);
		if_debug3('B', "[B]cc[0..2]=%g,%g,%g\n",
			  cc.paint.values[0], cc.paint.values[1],
			  cc.paint.values[2]);
		break;
	    case 1:		/* gray */
		psrc++;
		if (next.v[0] == run.v[0])
		    goto inc;
		if (device_color) {
		    (*map_rgb) (next.v[0], next.v[0],
				next.v[0], pdevc_next, pis, dev,
				gs_color_select_source);
		    goto f;
		}
		decode_frac(next.v[0], cc, 0);
		if_debug1('B', "[B]cc[0]=%g\n",
			  cc.paint.values[0]);
		break;
	}
	(*remap_color) (&cc, pcs, pdevc_next, pis, dev,
			gs_color_select_source);
f:
	if_debug7('B', "[B]0x%x,0x%x,0x%x,0x%x -> %ld,%ld,0x%lx\n",
		  next.v[0], next.v[1], next.v[2], next.v[3],
		  pdevc_next->colors.binary.color[0],
		  pdevc_next->colors.binary.color[1],
		  (ulong) pdevc_next->type);
	/* Even though the supplied colors don't match, */
	/* the device colors might. */
	if (!dev_color_eq(devc1, devc2)) {
	    /* Fill the region between xrun/irun and xl */
	    gx_device_color *ptemp;

	    if (posture != image_portrait) {	/* Parallelogram */
		code = (*dev_proc(dev, fill_parallelogram))
		    (dev, xrun, yrun,
		     xl - xrun, ytf - yrun, pdyx, pdyy,
		     pdevc, lop);
	    } else {		/* Rectangle */
		int xi = irun;
		int wi = (irun = fixed2int_var_rounded(xl)) - xi;

		if (wi < 0)
		    xi += wi, wi = -wi;
		code = gx_fill_rectangle_device_rop(xi, yt,
						  wi, iht, pdevc, dev, lop);
	    }
	    if (code < 0)
		return code;
	    ptemp = pdevc;
	    pdevc = pdevc_next;
	    pdevc_next = ptemp;
	    xrun = xl;
	    yrun = ytf;
	}
	run = next;
inc:
	xl = dda_next(pnext.x);
	ytf = dda_next(pnext.y);
    }
    /* Fill the final run. */
    code = (*dev_proc(dev, fill_parallelogram))
	(dev, xrun, yrun, xl - xrun, ytf - yrun, pdyx, pdyy, pdevc, lop);
    return (code < 0 ? code : 1);
}
