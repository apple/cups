/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gximage3.c */
/* Color image and multiple-source unpacking procedures */
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

/* ------ Unpacking procedures ------ */

void
image_unpack_1_spread(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, register int spread, uint inpos)
{	register byte *bufp = bptr + (inpos << 3) * spread;
	int left = dsize;
	register const byte *map = &pmap->table.lookup8[0];
	while ( left-- )
	   {	register uint b = *data++;
		*bufp = map[b >> 7]; bufp += spread;
		*bufp = map[(b >> 6) & 1]; bufp += spread;
		*bufp = map[(b >> 5) & 1]; bufp += spread;
		*bufp = map[(b >> 4) & 1]; bufp += spread;
		*bufp = map[(b >> 3) & 1]; bufp += spread;
		*bufp = map[(b >> 2) & 1]; bufp += spread;
		*bufp = map[(b >> 1) & 1]; bufp += spread;
		*bufp = map[b & 1]; bufp += spread;
	   }
}

void
image_unpack_2_spread(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, register int spread, uint inpos)
{	register byte *bufp = bptr + (inpos << 2) * spread;
	int left = dsize;
	register const byte *map = &pmap->table.lookup8[0];
	while ( left-- )
	   {	register unsigned b = *data++;
		*bufp = map[b >> 6]; bufp += spread;
		*bufp = map[(b >> 4) & 3]; bufp += spread;
		*bufp = map[(b >> 2) & 3]; bufp += spread;
		*bufp = map[b & 3]; bufp += spread;
	   }
}

void
image_unpack_8_spread(byte *bptr, register const byte *data, uint dsize,
  const sample_map *pmap, register int spread, uint inpos)
{	register byte *bufp = bptr + inpos * spread;
	register int left = dsize;
	register const byte *map = &pmap->table.lookup8[0];
	while ( left-- )
	   {	*bufp = map[*data++]; bufp += spread;
	   }
}

/* ------ Rendering procedures ------ */

/* Render a color image with 8 or fewer bits per sample. */
typedef union {
	byte v[4];
	bits32 all;		/* for fast comparison & clearing */
} color_samples;
int
image_render_color(gx_image_enum *penum, byte *buffer, uint w, int h,
  gx_device *dev)
{	gs_state *pgs = penum->pgs;
	const gs_imager_state *pis = penum->pis;
	gs_logical_operation_t lop = pis->log_op;
	gx_dda_fixed next_x, next_y;
	image_posture posture = penum->posture;
	fixed xl = penum->xcur;
	fixed ytf = penum->ycur;
	fixed pdyx, pdyy;		/* edge of parallelogram */
	int vci, vdi;
	const gs_color_space *pcs = penum->pcs;
	cs_proc_remap_color((*remap_color)) = pcs->type->remap_color;
	gs_client_color cc;
	bool device_color = penum->device_color;
	const gx_color_map_procs *cmap_procs = gx_device_cmap_procs(dev);
	cmap_proc_rgb((*map_rgb)) = cmap_procs->map_rgb;
	cmap_proc_cmyk((*map_cmyk)) = cmap_procs->map_cmyk;
	gx_image_clue *pic = &penum->clues[0];
#define pdevc (&pic->dev_color)
	gx_image_clue *pic_next = &penum->clues[1];
#define pdevc_next (&pic_next->dev_color)
	gx_image_clue empty_clue;
	int spp = penum->spp;
	const byte *psrc = buffer;
	fixed xrun = xl;		/* x at start of run */
	fixed yrun = ytf;		/* y ditto */
	int irun;			/* int x/rrun */
	color_samples run;		/* run value */
	color_samples next;		/* next sample value */
	bool small =
	  fixed2int(any_abs(penum->row_extent.x)) < penum->width &&
	  fixed2int(any_abs(penum->row_extent.y)) < penum->width;
	byte *bufend = buffer + w;
	bool use_cache = spp * penum->bps <= 12;

	if ( h == 0 )
	  return 0;
	switch ( posture )
	  {
	  case image_portrait:
	    vci = penum->yci, vdi = penum->hci;
	    irun = fixed2int_var_rounded(xrun);
	    break;
	  case image_landscape:
	    vci = penum->xci, vdi = penum->wci;
	    irun = fixed2int_var_rounded(yrun);
	    break;
	  case image_skewed:
	    pdyx = dda_current(penum->next_x) - xl;
	    pdyy = dda_current(penum->next_y) - ytf;
	  }

	dda_init(next_x, xl, penum->row_extent.x, penum->width);
	dda_init(next_y, ytf, penum->row_extent.y, penum->width);
	bufend[0] = ~bufend[-spp];	/* force end of run */
	if_debug4('b', "[b]y=%d w=%d xt=%f yt=%f\n",
		  penum->y, w, fixed2float(xl), fixed2float(ytf));
	run.all = 0;
	next.all = 0;
	/* Ensure that we don't get any false dev_color_eq hits. */
	if ( use_cache )
	  { color_set_pure(&empty_clue.dev_color, gx_no_color_index);
	    pic = &empty_clue;
	  }
	cc.paint.values[0] = cc.paint.values[1] =
	  cc.paint.values[2] = cc.paint.values[3] = 0;
	cc.pattern = 0;
	run.v[0] = ~psrc[0];		/* force remap */
	while ( psrc <= bufend )	/* 1 extra iteration */
				/* to handle final run */
	{	dda_next(next_x);
#define xn dda_current(next_x)
		dda_next(next_y);
#define yn dda_current(next_y)
#define includes_pixel_center(a, b)\
  (fixed_floor(a < b ? (a - (fixed_half + fixed_epsilon)) ^ (b - fixed_half) :\
	       (b - (fixed_half + fixed_epsilon)) ^ (a - fixed_half)) != 0)
#define paint_no_pixels()\
  (small && !includes_pixel_center(xl, xn) &&\
   !includes_pixel_center(ytf, yn) && psrc <= bufend)
#define clue_hash3(next)\
  &penum->clues[(next.v[0] + (next.v[1] << 2) + (next.v[2] << 4)) & 255];
#define clue_hash4(next)\
  &penum->clues[(next.v[0] + (next.v[1] << 2) + (next.v[2] << 4) +\
		 (next.v[3] << 6)) & 255]

		next.v[0] = psrc[0];
		next.v[1] = psrc[1];
		next.v[2] = psrc[2];
		if ( spp == 4 )		/* cmyk */
		{	next.v[3] = psrc[3];
			psrc += 4;
			if ( next.all == run.all || paint_no_pixels() )
			  goto inc;
			if ( use_cache )
			  { pic_next = clue_hash4(next);
			    if ( pic_next->key == next.all )
			      goto f;
			    pic_next->key = next.all;
			  }
			if ( device_color )
			{	(*map_cmyk)(byte2frac(next.v[0]),
					byte2frac(next.v[1]),
					byte2frac(next.v[2]),
					byte2frac(next.v[3]),
					pdevc_next, pgs);
				goto mapped;
			}
			decode_sample(next.v[3], cc, 3);
			if_debug1('B', "[B]cc[3]=%g\n", cc.paint.values[3]);
		}
		else			/* rgb */
		{	psrc += 3;
			if ( next.all == run.all || paint_no_pixels() )
			  goto inc;
			if ( use_cache )
			  { pic_next = clue_hash3(next);
			    if ( pic_next->key == next.all )
			      goto f;
			    pic_next->key = next.all;
			  }
			if ( device_color )
			{	(*map_rgb)(byte2frac(next.v[0]),
					byte2frac(next.v[1]),
					byte2frac(next.v[2]),
					pdevc_next, pgs);
				goto mapped;
			}
		}
		decode_sample(next.v[0], cc, 0);
		decode_sample(next.v[1], cc, 1);
		decode_sample(next.v[2], cc, 2);
		if_debug3('B', "[B]cc[0..2]=%g,%g,%g\n",
			  cc.paint.values[0], cc.paint.values[1],
			  cc.paint.values[2]);
		(*remap_color)(&cc, pcs, pdevc_next, pgs);
mapped:		if ( pic == pic_next )
		  goto fill;
f:		if_debug7('B', "[B]0x%x,0x%x,0x%x,0x%x -> %ld,%ld,0x%lx\n",
			next.v[0], next.v[1], next.v[2], next.v[3],
			pdevc_next->colors.binary.color[0],
			pdevc_next->colors.binary.color[1],
			(ulong)pdevc_next->type);
		/* Even though the supplied colors don't match, */
		/* the device colors might. */
		if ( dev_color_eq(*pdevc, *pdevc_next) &&
		     psrc <= bufend	/* force end of last run */
		   )
		  goto set;
fill:		{	/* Fill the region between */
			/* xrun/irun and xl */
			int code;
			switch ( posture )
			  {
			  case image_portrait:
			    {	/* Rectangle */
				int xi = irun;
				int wi =
				  (irun = fixed2int_var_rounded(xl)) - xi;
				if ( wi < 0 )
				  xi += wi, wi = -wi;
				code =
				  gx_fill_rectangle_device_rop(xi, vci,
					       wi, vdi, pdevc, dev, lop);
			    }	break;
			  case image_landscape:
			    {	/* 90 degree rotated rectangle */
				int yi = irun;
				int hi =
				  (irun = fixed2int_var_rounded(ytf)) - yi;
				if ( hi < 0 )
				  yi += hi, hi = -hi;
				code = gx_fill_rectangle_device_rop(vci, yi,
						vdi, hi, pdevc, dev, lop);
			    }	break;
			  default:
			    {	/* Parallelogram */
				code = (*dev_proc(dev, fill_parallelogram))
				  (dev, xrun, yrun,
				   xl - xrun, ytf - yrun, pdyx, pdyy,
				   pdevc, lop);
				xrun = xl;
				yrun = ytf;
			    }
			  }
			if ( code < 0 )
				return code;
			if ( use_cache )
			  pic = pic_next;
			else
			  { gx_image_clue *ptemp = pic;
			    pic = pic_next;
			    pic_next = ptemp;
			  }
		}
set:		run.all = next.all;
inc:		xl = xn;
		ytf = yn;		/* harmless if no skew */
#undef xn
#undef yn
	}
	return 1;
}
