/* Copyright (C) 1992, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gxicolor.c,v 1.1 2000/03/08 23:15:00 mike Exp $ */
/* Color image rendering */
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
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxdcconv.h"
#include "gxdcolor.h"
#include "gxistate.h"
#include "gzpath.h"
#include "gxdevmem.h"
#include "gxcpath.h"
#include "gximage.h"

/* ------ Strategy procedure ------ */

private irender_proc(image_render_color);
private irender_proc_t
image_strategy_color(gx_image_enum * penum)
{
    return image_render_color;
}

void
gs_gxicolor_init(gs_memory_t * mem)
{
    image_strategies.color = image_strategy_color;
}

/* ------ Rendering procedures ------ */

/* Render a color image with 8 or fewer bits per sample. */
typedef union {
    byte v[4];
    bits32 all;			/* for fast comparison & clearing */
} color_samples;
private int
image_render_color(gx_image_enum * penum, const byte * buffer, int data_x,
		   uint w, int h, gx_device * dev)
{
    const gs_imager_state *pis = penum->pis;
    gs_logical_operation_t lop = penum->log_op;
    gx_dda_fixed_point pnext;
    image_posture posture = penum->posture;
    fixed xprev, yprev;
    fixed pdyx, pdyy;		/* edge of parallelogram */
    int vci, vdi;
    const gs_color_space *pcs = penum->pcs;
    cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
    gs_client_color cc;
    bool device_color = penum->device_color;
    const gx_color_map_procs *cmap_procs = gx_device_cmap_procs(dev);
    cmap_proc_rgb((*map_3)) = cmap_procs->map_rgb;
    cmap_proc_cmyk((*map_4)) =
	(penum->alpha ? cmap_procs->map_rgb_alpha : cmap_procs->map_cmyk);
    gx_image_clue *pic = &penum->clues[0];
#define pdevc (&pic->dev_color)
    gx_image_clue *pic_next = &penum->clues[1];
#define pdevc_next (&pic_next->dev_color)
    gx_image_clue empty_clue;
    gx_image_clue clue_temp;
    int spp = penum->spp;
    const byte *psrc = buffer + data_x * spp;
    fixed xrun;			/* x at start of run */
    fixed yrun;			/* y ditto */
    int irun;			/* int x/rrun */
    color_samples run;		/* run value */
    color_samples next;		/* next sample value */
    const byte *bufend = psrc + w;
    bool use_cache = spp * penum->bps <= 12;
    int code = 0;

    if (h == 0)
	return 0;
    pnext = penum->dda.pixel0;
    xrun = xprev = dda_current(pnext.x);
    yrun = yprev = dda_current(pnext.y);
    pdyx = dda_current(penum->dda.row.x) - penum->cur.x;
    pdyy = dda_current(penum->dda.row.y) - penum->cur.y;
    switch (posture) {
	case image_portrait:
	    vci = penum->yci, vdi = penum->hci;
	    irun = fixed2int_var_rounded(xrun);
	    break;
	case image_landscape:
	    vci = penum->xci, vdi = penum->wci;
	    irun = fixed2int_var_rounded(yrun);
	    break;
	default:
	    break;
    }

    if_debug4('b', "[b]y=%d w=%d xt=%f yt=%f\n",
	      penum->y, w, fixed2float(xprev), fixed2float(yprev));
    run.all = 0;
    next.all = 0;
    /* Ensure that we don't get any false dev_color_eq hits. */
    if (use_cache) {
	color_set_pure(&empty_clue.dev_color, gx_no_color_index);
	pic = &empty_clue;
    }
    cc.paint.values[0] = cc.paint.values[1] =
	cc.paint.values[2] = cc.paint.values[3] = 0;
    cc.pattern = 0;
    run.v[0] = ~psrc[0];	/* force remap */
    while (psrc < bufend) {
	dda_next(pnext.x);
	dda_next(pnext.y);
#define CLUE_HASH3(penum, next)\
  &penum->clues[(next.v[0] + (next.v[1] << 2) + (next.v[2] << 4)) & 255];
#define CLUE_HASH4(penum, next)\
  &penum->clues[(next.v[0] + (next.v[1] << 2) + (next.v[2] << 4) +\
		 (next.v[3] << 6)) & 255]

	if (spp == 4) {		/* cmyk or rgba */
	    next.v[0] = psrc[0];
	    next.v[1] = psrc[1];
	    next.v[2] = psrc[2];
	    next.v[3] = psrc[3];
	    psrc += 4;
map4:	    if (next.all == run.all)
		goto inc;
	    if (use_cache) {
		pic_next = CLUE_HASH4(penum, next);
		if (pic_next->key == next.all)
		    goto f;
		/*
		 * If we are really unlucky, pic_next == pic,
		 * so mapping this color would clobber the one
		 * we're about to use for filling the run.
		 */
		if (pic_next == pic) {
		    clue_temp = *pic;
		    pic = &clue_temp;
		}
		pic_next->key = next.all;
	    }
	    if (device_color) {
		(*map_4)(byte2frac(next.v[0]), byte2frac(next.v[1]),
			 byte2frac(next.v[2]), byte2frac(next.v[3]),
			 pdevc_next, pis, dev,
			 gs_color_select_source);
		goto mapped;
	    }
	    decode_sample(next.v[3], cc, 3);
	    if_debug1('B', "[B]cc[3]=%g\n", cc.paint.values[3]);
	} else if (spp == 3) {	    /* rgb */
	    next.v[0] = psrc[0];
	    next.v[1] = psrc[1];
	    next.v[2] = psrc[2];
	    psrc += 3;
	    if (next.all == run.all)
		goto inc;
	    if (use_cache) {
		pic_next = CLUE_HASH3(penum, next);
		if (pic_next->key == next.all)
		    goto f;
		/* See above re the following check. */
		if (pic_next == pic) {
		    clue_temp = *pic;
		    pic = &clue_temp;
		}
		pic_next->key = next.all;
	    }
	    if (device_color) {
		(*map_3)(byte2frac(next.v[0]), byte2frac(next.v[1]),
			 byte2frac(next.v[2]),
			 pdevc_next, pis, dev,
			 gs_color_select_source);
		goto mapped;
	    }
	} else if (spp == 2) {	/* gray+alpha */
	    next.v[2] = next.v[1] = next.v[0] = psrc[0];
	    next.v[3] = psrc[1];
	    psrc += 2;
	    goto map4;
	} else {		/* spp == 5, cmyk+alpha */
	    /* Convert CMYK to RGB. */
	    frac rgb[3];

	    color_cmyk_to_rgb(byte2frac(psrc[0]), byte2frac(psrc[1]),
			      byte2frac(psrc[2]), byte2frac(psrc[3]),
			      pis, rgb);
	    /*
	     * It seems silly to do all this converting between
	     * fracs and bytes, but that's what the current
	     * APIs require.
	     */
	    next.v[0] = frac2byte(rgb[0]);
	    next.v[1] = frac2byte(rgb[1]);
	    next.v[2] = frac2byte(rgb[2]);
	    next.v[3] = psrc[4];
	    psrc += 5;
	    goto map4;
	}
	decode_sample(next.v[0], cc, 0);
	decode_sample(next.v[1], cc, 1);
	decode_sample(next.v[2], cc, 2);
	if_debug3('B', "[B]cc[0..2]=%g,%g,%g\n",
		  cc.paint.values[0], cc.paint.values[1],
		  cc.paint.values[2]);
	(*remap_color) (&cc, pcs, pdevc_next, pis, dev,
			gs_color_select_source);
mapped:	if (pic == pic_next)
	    goto fill;
f:	if_debug7('B', "[B]0x%x,0x%x,0x%x,0x%x -> %ld,%ld,0x%lx\n",
		  next.v[0], next.v[1], next.v[2], next.v[3],
		  pdevc_next->colors.binary.color[0],
		  pdevc_next->colors.binary.color[1],
		  (ulong) pdevc_next->type);
	/* Even though the supplied colors don't match, */
	/* the device colors might. */
	if (dev_color_eq(*pdevc, *pdevc_next))
	    goto set;
fill:	/* Fill the region between */
	/* xrun/irun and xprev */
	switch (posture) {
	case image_portrait:
	    {		/* Rectangle */
		int xi = irun;
		int wi =
		    (irun = fixed2int_var_rounded(xprev)) - xi;

		if (wi < 0)
		    xi += wi, wi = -wi;
		if (wi > 0)
		    code = gx_fill_rectangle_device_rop(xi, vci, wi, vdi,
							pdevc, dev, lop);
		xrun = xprev;	/* for sake of final run */
	    }
	    break;
	case image_landscape:
	    {		/* 90 degree rotated rectangle */
		int yi = irun;
		int hi =
		    (irun = fixed2int_var_rounded(yprev)) - yi;

		if (hi < 0)
		    yi += hi, hi = -hi;
		if (hi > 0)
		    code = gx_fill_rectangle_device_rop(vci, yi, vdi, hi,
							pdevc, dev, lop);
		yrun = yprev;	/* for sake of final run */
	    }
	    break;
	default:
	    {		/* Parallelogram */
		code = (*dev_proc(dev, fill_parallelogram))
		    (dev, xrun, yrun,
		     xprev - xrun, yprev - yrun, pdyx, pdyy,
		     pdevc, lop);
		xrun = xprev;
		yrun = yprev;
	    }
	}
	if (code < 0)
	    return code;
	if (use_cache)
	    pic = pic_next;
	else {
	    gx_image_clue *ptemp = pic;

	    pic = pic_next;
	    pic_next = ptemp;
	}
set:	run.all = next.all;
inc:	xprev = dda_current(pnext.x);
	yprev = dda_current(pnext.y);	/* harmless if no skew */
    }
    /* Fill the last run. */
    code = (*dev_proc(dev, fill_parallelogram))
	(dev, xrun, yrun, xprev - xrun, yprev - yrun, pdyx, pdyy, pdevc, lop);
    return (code < 0 ? code : 1);
}
