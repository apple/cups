/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* siscale.c */
/* Image scaling filters */
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"
#include <assert.h>
#include "strimpl.h"
#include "siscale.h"

/*
 *	Image scaling code is based on public domain code from
 *	Graphics Gems III (pp. 414-424), Academic Press, 1992.
 */

/* ---------------- ImageScaleEncode/Decode ---------------- */

public_st_IScale_state();	/* public so clients can allocate */

/* ------ Digital filter definition ------ */

/* Mitchell filter definition */
#define Mitchell_support 2.0
#define B (1.0 / 3.0)
#define C (1.0 / 3.0)
private double
Mitchell_filter(double t)
{	double t2 = t * t;

	if ( t < 0 )
	  t = -t;

	if ( t < 1 )
	  return
	    ((12 - 9 * B - 6 * C) * (t * t2) +
	     (-18 + 12 * B + 6 * C) * t2 +
	     (6 - 2 * B)) / 6;
	else if ( t < 2 )
	  return
	    ((-1 * B - 6 * C) * (t * t2) +
	     (6 * B + 30 * C) * t2 +
	     (-12 * B - 48 * C) * t +
	     (8 * B + 24 * C)) / 6;
	else
	  return 0;
}

#define filter_support Mitchell_support
#define filter_proc Mitchell_filter
#define fproc(t) filter_proc(t)
#define fWidthIn filter_support

/*
 * The environment provides the following definitions:
 *	typedef PixelTmp, PixelTmp2
 *	double fproc(double t)
 *	double fWidthIn
 *	PixelTmp {min,max,unit}PixelTmp
 */
#define CLAMP(v, mn, mx)\
  (v < mn ? mn : v > mx ? mx : v)

/* ------ Auxiliary procedures ------ */

/* Define the minimum scale. */
#define min_scale ((fWidthIn * 2) / (max_support - 1.01))

/* Calculate the support for a given scale. */
/* The value is always in the range 1 .. max_support. */
private int
contrib_pixels(double scale)
{	return (int)(fWidthIn / (scale >= 1.0 ? 1.0 : max(scale, min_scale))
		     * 2 + 1);
}

/* Pre-calculate filter contributions for a row or a column. */
/* Return the highest input pixel index used. */
private int
calculate_contrib(
	/* Return weight list parameters in contrib[0 .. size-1]. */
  CLIST *contrib,
	/* Store weights in items[0 .. contrib_pixels(scale)*size-1]. */
	/* (Less space than this may actually be needed.) */
  CONTRIB *items,
	/* The output image is scaled by 'scale' relative to the input. */
  double scale,
	/* Start generating weights for input pixel 'input_index'. */
  int input_index,
	/* Generate 'size' weight lists. */
  int size,
	/* Limit pixel indices to 'limit', for clamping at the edges */
	/* of the image. */
  int limit,
	/* Wrap pixel indices modulo 'modulus'. */
  int modulus,
	/* Successive pixel values are 'stride' distance apart -- */
	/* normally, the number of color components. */
  int stride,
	/* The unit of output is 'rescale_factor' times the unit of input. */
  double rescale_factor
  )
{
	double scaled_factor = scale_PixelWeight(rescale_factor);
	double WidthIn, fscale;
	bool squeeze;
	int npixels;
	int i, j;
	int last_index = -1;

	if ( scale < 1.0 )
	  {	double clamped_scale = max(scale, min_scale);
		WidthIn = fWidthIn / clamped_scale;
		fscale = 1.0 / clamped_scale;
		squeeze = true;
	  }
	else
	  {	WidthIn = fWidthIn;
		fscale = 1.0;
		squeeze = false;
	  }
	npixels = (int)(WidthIn * 2 + 1);

	for ( i = 0; i < size; ++i )
	  {	double center = (input_index + i) / scale;
		int left = (int)ceil(center - WidthIn);
		int right = (int)floor(center + WidthIn);
#define clamp_pixel(j)\
  (j < 0 ? -j : j >= limit ? (limit - j) + limit - 1 : j)
		int lmin = (left < 0 ? 0 : left);
		int lmax = (left < 0 ? -left : left);
		int rmin =
		  (right >= limit ? (limit - right) + limit - 1 : right);
		int rmax = (right >= limit ? limit - 1 : right);
		int first_pixel = min(lmin, rmin);
		int last_pixel = max(lmax, rmax);
		CONTRIB *p;

		if ( last_pixel > last_index )
		  last_index = last_pixel;
		contrib[i].first_pixel = (first_pixel % modulus) * stride;
		contrib[i].n = last_pixel - first_pixel + 1;
		contrib[i].index = i * npixels;
		p = items + contrib[i].index;
		for ( j = 0; j < npixels; ++j )
		  p[j].weight = 0;
		if ( squeeze )
		  {	for ( j = left; j <= right; ++j )
			  {	double weight =
				  fproc((center - j) / fscale) / fscale;
				int n = clamp_pixel(j);
				int k = n - first_pixel;
				p[k].weight +=
				  (PixelWeight)(weight * scaled_factor);
			  }
		  }
		else
		  {	for ( j = left; j <= right; ++j )
			  {	double weight = fproc(center - j);
				int n = clamp_pixel(j);
				int k = n - first_pixel;
				p[k].weight +=
				  (PixelWeight)(weight * scaled_factor);
			  }
		  }
	  }
	return last_index;
}


/* Apply filter to zoom horizontally from src to tmp. */
private void
zoom_x(PixelTmp *tmp, const void /*PixelIn*/ *src, int sizeofPixelIn,
  int tmp_width, int WidthIn, int Colors, const CLIST *contrib,
  const CONTRIB *items)
{	int c, i;
	for ( c = 0; c < Colors; ++c )
	  {	PixelTmp *tp = tmp + c;
		const CLIST *clp = contrib;
#define zoom_x_loop(PixelIn, PixelIn2)\
		const PixelIn *raster = (const PixelIn *)src + c;\
		for ( i = 0; i < tmp_width; tp += Colors, ++clp, ++i )\
		  {	AccumTmp weight = 0;\
			{ int j = clp->n;\
			  const PixelIn *pp = raster + clp->first_pixel;\
			  const CONTRIB *cp = items + clp->index;\
			  switch ( Colors )\
			  {\
			  case 1:\
			    for ( ; j > 0; pp += 1, ++cp, --j )\
			      weight += *pp * cp->weight;\
			    break;\
			  case 3:\
			    for ( ; j > 0; pp += 3, ++cp, --j )\
			      weight += *pp * cp->weight;\
			    break;\
			  default:\
			    for ( ; j > 0; pp += Colors, ++cp, --j )\
			      weight += *pp * cp->weight;\
			  }\
			}\
			{ PixelIn2 pixel = unscale_AccumTmp(weight);\
			  *tp =\
			    (PixelTmp)CLAMP(pixel, minPixelTmp, maxPixelTmp);\
			}\
		  }

		if ( sizeofPixelIn == 1 )
		  {	zoom_x_loop(byte, int)
		  }
		else			/* sizeofPixelIn == 2 */
		  {
#if arch_ints_are_short
			zoom_x_loop(bits16, long)
#else
			zoom_x_loop(bits16, int)
#endif
		  }
	  }
}

/* Apply filter to zoom vertically from tmp to dst. */
/* This is simpler because we can treat all columns identically */
/* without regard to the number of samples per pixel. */
private void
zoom_y(void /*PixelOut*/ *dst, int sizeofPixelOut, uint MaxValueOut,
  const PixelTmp *tmp, int WidthOut, int tmp_width,
  int Colors, const CLIST *contrib, const CONTRIB *items)
{	int kn = WidthOut * Colors;
	int cn = contrib->n;
	int first_pixel = contrib->first_pixel;
	const CONTRIB *cbp = items + contrib->index;
	int kc;
	PixelTmp2 max_weight = MaxValueOut;

#define zoom_y_loop(PixelOut)\
	for ( kc = 0; kc < kn; ++kc )\
	  {	AccumTmp weight = 0;\
		{ const PixelTmp *pp = &tmp[kc + first_pixel];\
		  int j = cn;\
		  const CONTRIB *cp = cbp;\
		  for ( ; j > 0; pp += kn, ++cp, --j )\
		    weight += *pp * cp->weight;\
		}\
		{ PixelTmp2 pixel = unscale_AccumTmp(weight);\
		  ((PixelOut *)dst)[kc] =\
		    (PixelOut)CLAMP(pixel, 0, max_weight);\
		}\
	  }

	if ( sizeofPixelOut == 1 )
	  {	zoom_y_loop(byte)
	  }
	else			/* sizeofPixelOut == 2 */
	  {	zoom_y_loop(bits16)
	  }
}

/* ------ Stream implementation ------ */

#define tmp_width WidthOut
#define tmp_height HeightIn

/* Forward references */
private void s_IScale_release(P1(stream_state *st));

/* Calculate the weights for an output row. */
private void
calculate_dst_contrib(stream_IScale_state *ss, int y)
{	uint row_size = ss->WidthOut * ss->Colors;
	int last_index =
	  calculate_contrib(&ss->dst_next_list, ss->dst_items, ss->yscale,
			    y, 1, ss->HeightIn, max_support, row_size,
			    (double)ss->MaxValueOut / unitPixelTmp);
	int first_index_mod = ss->dst_next_list.first_pixel / row_size;

	ss->dst_last_index = last_index;
	last_index %= max_support;
	if ( last_index < first_index_mod )
	  { /* Shuffle the indices to account for wraparound. */
	    CONTRIB shuffle[max_support];
	    int i;

	    for ( i = 0; i < max_support; ++i )
	      shuffle[i].weight =
		(i <= last_index ?
		 ss->dst_items[i + max_support - first_index_mod].weight :
		 i >= first_index_mod ?
		 ss->dst_items[i - first_index_mod].weight :
		 0);
	    memcpy(ss->dst_items, shuffle, max_support * sizeof(CONTRIB));
	    ss->dst_next_list.n = max_support;
	    ss->dst_next_list.first_pixel = 0;
	  }
}

#define ss ((stream_IScale_state *)st)

/* Initialize the filter. */
private int
s_IScale_init(stream_state *st)
{	gs_memory_t *mem = ss->memory;

	ss->sizeofPixelIn = ss->BitsPerComponentIn / 8;
	ss->sizeofPixelOut = ss->BitsPerComponentOut / 8;
	ss->xscale = (double)ss->WidthOut / (double)ss->WidthIn;
	ss->yscale = (double)ss->HeightOut / (double)ss->HeightIn;

	ss->src_y = 0;
	ss->src_size = ss->WidthIn * ss->sizeofPixelIn * ss->Colors;
	ss->src_offset = 0;
	ss->dst_y = 0;
	ss->dst_size = ss->WidthOut * ss->sizeofPixelOut * ss->Colors;
	ss->dst_offset = 0;

	/* create intermediate image to hold horizontal zoom */
	ss->tmp = (PixelTmp *)gs_alloc_byte_array(mem,
				min(ss->tmp_height, max_support),
				ss->tmp_width * ss->Colors * sizeof(PixelTmp),
				"image_scale tmp");
	ss->contrib = (CLIST *)gs_alloc_byte_array(mem,
				max(ss->WidthOut, ss->HeightOut),
				sizeof(CLIST), "image_scale contrib");
	ss->items = (CONTRIB *)gs_alloc_byte_array(mem,
				contrib_pixels(ss->xscale) * ss->WidthOut,
				sizeof(CONTRIB), "image_scale contrib[*]");
	/* Allocate buffers for 1 row of source and destination. */
	ss->dst = gs_alloc_byte_array(mem, ss->WidthOut * ss->Colors,
				ss->sizeofPixelOut, "image_scale dst");
	ss->src = gs_alloc_byte_array(mem, ss->WidthIn * ss->Colors,
				ss->sizeofPixelIn, "image_scale src");
	if ( ss->tmp == 0 || ss->contrib == 0 || ss->items == 0 ||
	     ss->dst == 0 || ss->src == 0
	   )
	  {	s_IScale_release(st);
		return ERRC;			/****** WRONG ******/
	  }

	/* Pre-calculate filter contributions for a row. */
	calculate_contrib(ss->contrib, ss->items, ss->xscale,
			  0, ss->WidthOut, ss->WidthIn, ss->WidthIn,
			  ss->Colors, (double)unitPixelTmp / ss->MaxValueIn);

	/* Prepare the weights for the first output row. */
	calculate_dst_contrib(ss, 0);

	return 0;

}

/* Process a buffer.  Note that this handles Encode and Decode identically. */
private int
s_IScale_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *pw, bool last)
{
	/* Check whether we need to deliver any output. */

top:	while ( ss->src_y > ss->dst_last_index )
	  { /* We have enough horizontally scaled temporary rows */
	    /* to generate a vertically scaled output row. */
	    uint wleft = pw->limit - pw->ptr;
	    if ( ss->dst_y == ss->HeightOut )
	      return EOFC;
	    if ( wleft == 0 )
	      return 1;
	    if ( ss->dst_offset == 0 )
	      { byte *row;
		if ( wleft >= ss->dst_size )
		  { /* We can scale the row directly into the output. */
		    row = pw->ptr + 1;
		    pw->ptr += ss->dst_size;
		  }
		else
		  { /* We'll have to buffer the row. */
		    row = ss->dst;
		  }
		/* Apply filter to zoom vertically from tmp to dst. */
		zoom_y(row, ss->sizeofPixelOut, ss->MaxValueOut, ss->tmp,
		       ss->WidthOut, ss->tmp_width, ss->Colors,
		       &ss->dst_next_list, ss->dst_items);
		if ( row != ss->dst )	/* no buffering */
		  goto adv;
	      }
	    { /* We're delivering a buffered output row. */
	      uint wcount = ss->dst_size - ss->dst_offset;
	      uint ncopy = min(wleft, wcount);
	      memcpy(pw->ptr + 1, (byte *)ss->dst + ss->dst_offset, ncopy);
	      pw->ptr += ncopy;
	      ss->dst_offset += ncopy;
	      if ( ncopy != wcount )
		return 1;
	      ss->dst_offset = 0;
	    }
	    /* Advance to the next output row. */
adv:	    ++(ss->dst_y);
	    if ( ss->dst_y != ss->HeightOut )
	      calculate_dst_contrib(ss, ss->dst_y);
	  }

	/* Read input data and scale horizontally into tmp. */

#ifdef DEBUG
	assert(ss->src_y < ss->HeightIn);
#endif
	  { uint rleft = pr->limit - pr->ptr;
	    uint rcount = ss->src_size - ss->src_offset;
	    if ( rleft == 0 )
	      return 0;			/* need more input */
	    if ( rleft >= rcount )
	      { /* We're going to fill up a row. */
		const byte *row;
		if ( ss->src_offset == 0 )
		  { /* We have a complete row.  Read the data */
		    /* directly from the input. */
		    row = pr->ptr + 1;
		  }
		else
		  { /* We're buffering a row in src. */
		    row = ss->src;
		    memcpy((byte *)ss->src + ss->src_offset, pr->ptr + 1,
			   rcount);
		    ss->src_offset = 0;
		  }
		/* Apply filter to zoom horizontally from src to tmp. */
		zoom_x(ss->tmp + (ss->src_y % max_support) *
		         ss->tmp_width * ss->Colors, row,
		       ss->sizeofPixelIn, ss->tmp_width, ss->WidthIn,
		       ss->Colors, ss->contrib, ss->items);
		pr->ptr += rcount;
		++(ss->src_y);
		goto top;
	      }
	    else
	      { /* We don't have a complete row.  Copy data to src buffer. */
		memcpy((byte *)ss->src + ss->src_offset, pr->ptr + 1, rleft);
		ss->src_offset += rleft;
		pr->ptr += rleft;
		return 0;
	      }
	  }
}

/* Release the filter's storage. */
private void
s_IScale_release(stream_state *st)
{	gs_memory_t *mem = ss->memory;
	gs_free_object(mem, (void *)ss->src, "image_scale src"); /* no longer const */
	  ss->src = 0;
	gs_free_object(mem, ss->dst, "image_scale dst");
	  ss->dst = 0;
	gs_free_object(mem, ss->items, "image_scale contrib[*]");
	  ss->items = 0;
	gs_free_object(mem, ss->contrib, "image_scale contrib");
	  ss->contrib = 0;
	gs_free_object(mem, ss->tmp, "image_scale tmp");
	  ss->tmp = 0;
}

#undef ss

/* Stream template */
const stream_template s_IScale_template =
{	&st_IScale_state, s_IScale_init, s_IScale_process, 1, 1,
	s_IScale_release
};
