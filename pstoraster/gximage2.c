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

/* gximage2.c */
/* General monochrome image rendering */
#include "gx.h"
#include "memory_.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxarith.h"
#include "gxmatrix.h"
#include "gsccolor.h"
#include "gspaint.h"
#include "gsutil.h"
#include "gzstate.h"
#include "gxcmap.h"
#include "gzpath.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gdevmem.h"			/* for mem_mono_device */
#include "gxcpath.h"
#include "gximage.h"
#include "gzht.h"

/* Rendering procedure for the general case of displaying a */
/* monochrome image, dealing with multiple bit-per-sample images, */
/* general transformations, and arbitrary single-component */
/* color spaces (DeviceGray, CIEBasedA, Separation, Indexed). */
/* This procedure handles a single scan line. */
int
image_render_mono(gx_image_enum *penum, byte *buffer, uint w, int h,
  gx_device *dev)
{	gs_state *pgs = penum->pgs;
	const gs_imager_state *pis = penum->pis;
	gs_logical_operation_t lop = pis->log_op;
	const int masked = penum->masked;
	fixed xt = penum->xcur;
	const gs_color_space *pcs;		/* only set for non-masks */
	cs_proc_remap_color((*remap_color));	/* ditto */
	gs_client_color cc;
	const gx_color_map_procs *cmap_procs = gx_device_cmap_procs(dev);
	cmap_proc_gray((*map_gray)) = cmap_procs->map_gray;
	gx_device_color *pdevc = pgs->dev_color;
	/* Make sure the cache setup matches the graphics state. */
	/* Also determine whether all tiles fit in the cache. */
	int tiles_fit = gx_check_tile_cache(pgs);
#define image_set_gray(sample_value)\
   { pdevc = &penum->clues[sample_value].dev_color;\
     if ( !color_is_set(pdevc) )\
      { if ( penum->device_color )\
	  (*map_gray)(byte2frac(sample_value), pdevc, pgs);\
	else\
	  { decode_sample(sample_value, cc, 0);\
	    (*remap_color)(&cc, pcs, pdevc, pgs);\
	  }\
      }\
     else if ( !color_is_pure(pdevc) )\
      { if ( !tiles_fit )\
         { code = gx_color_load(pdevc, pgs);\
	   if ( code < 0 ) return code;\
	 }\
      }\
   }
	fixed xl = xt;
	gx_dda_fixed next_x;
	register const byte *psrc = buffer;
	byte *endp = buffer + w;
	fixed xrun = xt;		/* x at start of run */
	register byte run;		/* run value */
	int htrun =			/* halftone run value */
	  (masked ? 255 : -2);
	int code;

	if ( h == 0 )
	  return 0;
	if ( !masked )
	  { pcs = penum->pcs;		/* (may not be set for masks) */
	    remap_color = pcs->type->remap_color;
	  }
	run = *psrc;
	*endp = ~endp[-1];	/* force end of run */
	if ( penum->slow_loop || penum->posture != image_portrait )
	  { /* Skewed, rotated, or imagemask with a halftone. */
	    gx_dda_fixed next_y;
	    fixed ytf = penum->ycur;
	    fixed yrun = ytf;
	    const fixed pdyx = dda_current(penum->next_x) - xl;
	    const fixed pdyy = dda_current(penum->next_y) - ytf;
	    dev_proc_fill_parallelogram((*fill_pgram)) =
	      dev_proc(dev, fill_parallelogram);

	    dda_init(next_x, xl, penum->row_extent.x, penum->width);
#define xl dda_current(next_x)
	    dda_init(next_y, ytf, penum->row_extent.y, penum->width);
#define ytf dda_current(next_y)
	    if ( masked )
	      { code = gx_color_load(pdevc, pgs);
	        if ( code < 0 )
		  return code;
		for ( ; ; )
		  { /* To avoid rounding errors, we must run dda_next */
		    /* for each pixel. */
		    for ( ; !*psrc; ++psrc )
		      { dda_next(next_x);
		        dda_next(next_y);
		      }
		    if ( psrc >= endp )
		      break;
		    yrun = ytf;
		    xrun = xl;
		    for ( ; *psrc; ++psrc )
		      { dda_next(next_x);
			dda_next(next_y);
		       }
		    code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
					 ytf - yrun, pdyx, pdyy, pdevc, lop);
		    if ( code < 0 )
		      return code;
		    if ( psrc >= endp )
		      break;
		  }
	      }
	    else		/* not masked */
	      { for ( ; ; )
		  { /* We can't skip large constant regions quickly, */
		    /* because this leads to rounding errors. */
		    /* Just fill the region between xrun and xl. */
		    dda_next(next_x);
		    dda_next(next_y);		/* harmless if no skew */
		    psrc++;
		    if ( run != htrun )
		      { htrun = run;
		        image_set_gray(run);
		      }
		    code = (*fill_pgram)(dev, xrun, yrun, xl - xrun,
					 ytf - yrun, pdyx, pdyy, pdevc, lop);
		    if ( code < 0 )
		      return code;
		    if ( psrc >= endp )
		      break;
		    yrun = ytf;
		    xrun = xl;
		    run = psrc[-1];
		  }
	      }
#undef xl
#undef ytf
	  }
	else			/* fast loop */
	  { /* No skew, and not imagemask with a halftone. */
	    const fixed adjust = penum->adjust;
	    const fixed dxx =
	      float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);
	    gx_dda_step_fixed dxx2, dxx3, dxx4;
	    fixed xa = (dxx >= 0 ? adjust : -adjust);
	    const int yt = penum->yci, iht = penum->hci;
	    dev_proc_fill_rectangle((*fill_proc)) =
	      dev_proc(dev, fill_rectangle);
	    dev_proc_strip_tile_rectangle((*tile_proc)) =
	      dev_proc(dev, strip_tile_rectangle);
	    dev_proc_copy_mono((*copy_mono_proc)) =
	      dev_proc(dev, copy_mono);
	    /*
	     * If each pixel is likely to fit in a single halftone tile,
	     * determine that now (tile_offset = offset of row within tile).
	     * Don't do this for band devices; they handle halftone fills
	     * more efficiently than copy_mono.
	     */
	    int bstart;
	    int phase_x;
	    int tile_offset =
	      ((*dev_proc(dev, get_band))(dev, yt, &bstart) == 0 ?
	       gx_check_tile_size(pgs,
				  fixed2int_ceiling(any_abs(dxx) + (xa << 1)),
				  yt, iht, &phase_x) :
	       -1);
	    int xmin = fixed2int_pixround(penum->clip_outer.p.x);
	    int xmax = fixed2int_pixround(penum->clip_outer.q.x);

	    /* Fold the adjustment into xrun and xl, */
	    /* including the +0.5-epsilon for rounding. */
	    xrun = xrun - xa + (fixed_half - fixed_epsilon);
	    dda_init(next_x, xl + xa + (fixed_half - fixed_epsilon),
		     penum->row_extent.x, penum->width);
#define xl dda_current(next_x)
	    xa <<= 1;
	    /* Calculate multiples of the DDA step. */
	    dxx2 = next_x.step;
	    dda_step_add(dxx2, next_x.step);
	    dxx3 = dxx2;
	    dda_step_add(dxx3, next_x.step);
	    dxx4 = dxx3;
	    dda_step_add(dxx4, next_x.step);
	    for ( ; ; )
	      {	/* Skip large constant regions quickly, */
		/* but don't slow down transitions too much. */
skf:		if ( psrc[0] == run )
		  { if ( psrc[1] == run )
		      { if ( psrc[2] == run )
			  { if ( psrc[3] == run )
			      { psrc += 4;
				dda_state_next(next_x.state, dxx4);
				goto skf;
			      }
			    else
			      { psrc += 4;
				dda_state_next(next_x.state, dxx3);
			      }
			  }
		        else
			  { psrc += 3;
			    dda_state_next(next_x.state, dxx2);
			  }
		      }
		    else
		      { psrc += 2;
			dda_next(next_x);
		      }
		  }
		else
		  psrc++;
		{ /* Now fill the region between xrun and xl. */
		  int xi = fixed2int_var(xrun);
		  int wi = fixed2int_var(xl) - xi;
		  int xei, tsx;
		  const gx_strip_bitmap *tile;

		  if ( wi <= 0 )
		    { if ( wi == 0 ) goto mt;
		      xi += wi, wi = -wi;
		    }
		  if ( (xei = xi + wi) > xmax || xi < xmin )
		    { /* Do X clipping */
		      if ( xi < xmin )
			wi -= xmin - xi, xi = xmin;
		      if ( xei > xmax )
			wi -= xei - xmax;
		      if ( wi <= 0 )
			goto mt;
		    }
		  switch ( run )
		    {
		    case 0:
		      if ( masked ) goto mt;
		      if ( !color_is_pure(&penum->icolor0) ) goto ht;
		      code = (*fill_proc)(dev, xi, yt, wi, iht,
					  penum->icolor0.colors.pure);
		      break;
		    case 255:		/* just for speed */
		      if ( !color_is_pure(&penum->icolor1) ) goto ht;
		      code = (*fill_proc)(dev, xi, yt, wi, iht,
					  penum->icolor1.colors.pure);
		      break;
		    default:
ht:		      /* Use halftone if needed */
		      if ( run != htrun )
			{ image_set_gray(run);
			  htrun = run;
			}
		      /* We open-code gx_fill_rectangle, */
		      /* because we've done some of the work for */
		      /* halftone tiles in advance. */
		      if ( color_is_pure(pdevc) )
			{ code = (*fill_proc)(dev, xi, yt, wi, iht,
					      pdevc->colors.pure);
			}
		      else if ( !color_is_binary_halftone(pdevc) )
			{ code =
			    gx_fill_rectangle_device_rop(xi, yt, wi, iht,
							 pdevc, dev, lop);
			}
		      else if ( tile_offset >= 0 &&
			        (tile = &pdevc->colors.binary.b_tile->tiles,
				 (tsx = (xi + phase_x) % tile->rep_width) + wi <= tile->size.x)
			      )
			{ /* The pixel(s) fit(s) in a single (binary) tile. */
			  byte *row = tile->data + tile_offset;
			  code = (*copy_mono_proc)
			    (dev, row, tsx, tile->raster, gx_no_bitmap_id,
			     xi, yt, wi, iht,
			     pdevc->colors.binary.color[0],
			     pdevc->colors.binary.color[1]);
			}
		      else
			{ code = (*tile_proc)(dev,
					      &pdevc->colors.binary.b_tile->tiles,
					      xi, yt, wi, iht,
					      pdevc->colors.binary.color[0],
					      pdevc->colors.binary.color[1],
					      pdevc->phase.x, pdevc->phase.y);
			}
		    }
		    if ( code < 0 ) return code;
mt:		    if ( psrc >= endp ) break;
		    xrun = xl - xa;	/* original xa << 1 */
		    run = psrc[-1];
		}
		dda_next(next_x);
	      }
	  }
#undef xl
	return 1;
}
