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

/* gximage1.c */
/* Fast monochrome image rendering */
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

/* Conditionally include statistics code. */
#ifdef DEBUG
#  define STATS
#endif

/* ------ Rendering procedures ------ */

/* Rendering procedure for ignoring an image.  We still need to iterate */
/* over the samples, because the procedure might have side effects. */
int
image_render_skip(gx_image_enum *penum, byte *data, uint w, int h,
  gx_device *dev)
{	return h;
}

/* Scale (and possibly reverse) one scan line of a monobit image. */
/* This is used for both portrait and landscape image processing. */
/* We pass in an x offset (0 <= line_x < align_bitmap_mod * 8) so that */
/* we can align the result with the eventual device X. */
#ifdef STATS
struct ix_s {
  long
    calls, runs,
    lbit0, byte00, byte01, byte02, byte03, byte04, rbit0,
    lbit1, byte1, rbit1,
    thin, thin2, nwide, bwide, nfill, bfill;
} ix_;
#  define incs(stat) ++ix_.stat
#  define adds(stat, n) ix_.stat += n
#else
#  define incs(stat) DO_NOTHING
#  define adds(stat, n) DO_NOTHING
#endif
private void
image_simple_expand(byte *line, int line_x, uint raster, uint line_width,
  byte *buffer, uint w, fixed xcur, fixed dxx, byte zero /* 0 or 0xff */)
{	int ix = fixed2int_pixround(xcur);
	fixed xl = xcur + fixed_half - int2fixed(ix);
	byte sbit = 0x80;
	const fixed dxx_4 = dxx << 2;
	const fixed dxx_8 = dxx_4 << 1;
	const fixed dxx_32 = dxx_8 << 2;
	register const byte *psrc = buffer;
	byte *endp = buffer + (w >> 3);
	byte endbit = 1 << (~w & 7);
	byte data;
	byte one = ~zero;

	if ( dxx < 0 )
	  {	ix -= line_width;
		xl += int2fixed(line_width);
	  }
	xl += int2fixed(line_x);

	/* Ensure that the line ends with a transition from 0 to 1. */
	if ( endbit == 1 )
	  ++endp, endp[-1] &= ~1, *endp = endbit = 0x80;
	else
	  endbit >>= 1, *endp = (*endp & ~(endbit << 1)) | endbit;

	/* Pre-clear the line. */
	memset(line + (line_x >> 3), zero, raster - (line_x >> 3));

	/*
	 * Loop invariants:
	 *	data = *psrc;
	 *	sbit = 1 << n, 0<=n<=7.
	 */
	incs(calls);
	for ( data = *psrc; ; )
	{	int x0, n, bit;
		byte *bp;
		static const byte lmasks[9] =
		 { 0xff, 0x7f, 0x3f, 0x1f, 0xf, 7, 3, 1, 0 };
		static const byte rmasks[9] =
		 { 0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

		incs(runs);

		/* Scan a run of zeros. */
		data ^= 0xff;			/* invert */
		while ( data & sbit )
		{	xl += dxx;
			sbit >>= 1;
			incs(lbit0);
		}
		if ( !sbit )
		{	/* Scan a run of zero bytes. */
sw:			if ( (data = psrc[1]) != 0 )
			  {	psrc++;
				incs(byte00);
			  }
			else if ( (data = psrc[2]) != 0 )
			  {	xl += dxx_8, psrc += 2;
				incs(byte01);
			  }
			else if ( (data =psrc[3]) != 0 )
			  {	xl += dxx_8 << 1, psrc += 3;
				incs(byte02);
			  }
			else if ( (data = psrc[4]) != 0 )
			  {	xl += dxx_32 - dxx_8, psrc += 4;
				incs(byte03);
			  }
			else
			  {	xl += dxx_32;
				psrc += 4;
				incs(byte04);
				goto sw;
			  }
			if ( data > 0xf )
			  sbit = 0x80;
			else
			  sbit = 0x08, xl += dxx_4;
			data ^= 0xff;		/* invert */
			while ( data & sbit )
			{	xl += dxx;
				sbit >>= 1;
				incs(rbit0);
			}
		}
		/* We know the data end with a transition from 0 to 1; */
		/* check for that now. */
		if ( psrc >= endp && sbit == endbit )
		  break;
		x0 = fixed2int_var(xl);

		/* Scan a run of ones. */
		/* We know the current bit is a one. */
		data ^= 0xff;			/* un-invert */
		do
		  {	xl += dxx;
			sbit >>= 1;
			incs(lbit1);
		  }
		while ( data & sbit );
		if ( !sbit )
		  {	/* Scan a run of 0xff bytes. */
			while ( (data = *++psrc) == 0xff )
			  { xl += dxx_8;
			    incs(byte1);
			  }
			if ( data < 0xf0 )
			  sbit = 0x80;
			else
			  sbit = 0x08, xl += dxx_4;
			while ( data & sbit )
			  { xl += dxx;
			    sbit >>= 1;
			    incs(rbit1);
			  }
		  }

		/* Fill the run in the scan line. */
		n = fixed2int_var(xl) - x0;
		if ( n < 0 )
		  x0 += n, n = -n;
		bp = line + (x0 >> 3);
		bit = x0 & 7;
		if ( (n += bit) <= 8 )
		  {	*bp ^= lmasks[bit] - lmasks[n];
			incs(thin);
		  }
		else if ( (n -= 8) <= 8 )
		  {	*bp ^= lmasks[bit];
			bp[1] ^= rmasks[n];
			incs(thin2);
		  }
		else
		  {	*bp++ ^= lmasks[bit];
			if ( n >= 56 )
			{	int nb = n >> 3;
				memset(bp, one, nb);
				bp += nb;
				incs(nwide);
				adds(bwide, nb);
			}
			else
			{	adds(bfill, n >> 3);
				while ( (n -= 8) >= 0 )
				  *bp++ = one;
				incs(nfill);
			}
			*bp ^= rmasks[n & 7];
		  }

	};
}

/* Rendering procedure for a monobit image with no */
/* skew or rotation and pure colors. */
int
image_render_simple(gx_image_enum *penum, byte *buffer, uint w, int h,
  gx_device *dev)
{	dev_proc_copy_mono((*copy_mono)) = dev_proc(dev, copy_mono);
	byte *line = penum->line;
	uint line_width, line_size;
	int line_x;
	fixed xcur = penum->xcur;
	int ix = fixed2int_pixround(xcur);
	const int iy = penum->yci, ih = penum->hci;
	fixed dxx =
	  float2fixed(penum->matrix.xx + fixed2float(fixed_epsilon) / 2);

	gx_color_index
		zero = penum->icolor0.colors.pure,
		one = penum->icolor1.colors.pure;
	int dy = 0;

	if ( h == 0 )
	  return 0;
	if ( penum->map[0].table.lookup4x1to32[0] != 0 )
	  zero = penum->icolor1.colors.pure,
	  one = penum->icolor0.colors.pure;

	if ( line == 0 )
	{	/* A direct BitBlt is possible. */
		line = buffer;
		line_size = (w + 7) >> 3;
		line_width = w;
		line_x = 0;
	}
	else if ( copy_mono == mem_mono_device.std_procs.copy_mono &&
		  dxx > 0 && (zero ^ one) == 1	/* must be (0,1) or (1,0) */
		)
	  {	/* Do the operation directly into the memory device bitmap. */
		int ixr = fixed2int_pixround(xcur + w * dxx) - 1;
		int line_ix;
		int ib_left = ix >> 3, ib_right = ixr >> 3;
		byte save_left, save_right, mask;

		line = scan_line_base((gx_device_memory *)dev, iy);
		line_x = ix & (align_bitmap_mod * 8 - 1);
		line_ix = ix - line_x;
		line_size = (ixr >> 3) + 1 - (line_ix >> 3);
		line_width = ixr + 1 - ix;
		/* We must save and restore any unmodified bits in */
		/* the two edge bytes. */
		save_left = line[ib_left];
		save_right = line[ib_right];
		image_simple_expand(line + (line_ix >> 3), line_x,
				    line_size, line_width,
				    buffer, w, xcur, dxx,
				    -(byte)zero);
		if ( ix & 7 )
		  mask = (byte)(0xff00 >> (ix & 7)),
		  line[ib_left] = (save_left & mask) + (line[ib_left] & ~mask);
		if ( (ixr + 1) & 7 )
		  mask = (byte)(0xff00 >> ((ixr + 1) & 7)),
		  line[ib_right] = (line[ib_right] & mask) + (save_right & ~mask);
		line += line_ix >> 3;
		dy = 1;
		/*
		 * If we're going to replicate the line, ensure that we don't
		 * attempt to change the polarity.
		 */
		zero = 0;
		one = 1;
	  }
	else
	  {	line_size = penum->line_size;
		line_width = penum->line_width;
		line_x = ix & (align_bitmap_mod * 8 - 1);
		image_simple_expand(line, line_x, line_size, line_width,
				    buffer, w, xcur, dxx, 0);
	  }

	/* Finally, transfer the scan line to the device. */
	if ( dxx < 0 )
	  ix -= line_width;
	for ( ; dy < ih; dy++ )
	  {	int code = (*copy_mono)(dev, line, line_x, line_size,
					gx_no_bitmap_id,
					ix, iy + dy, line_width, 1,
					zero, one);
		if ( code < 0 )
		  return code;
	  }

	return_check_interrupt(1);
}

/* Rendering procedure for a 90 degree rotated monobit image */
/* with pure colors.  We buffer and then flip 8 scan lines at a time. */
private int copy_landscape(P5(gx_image_enum *, int, int, bool, gx_device *));
int
image_render_landscape(gx_image_enum *penum, byte *buffer, uint w, int h,
  gx_device *dev)
{	byte *line = penum->line;
	uint raster = bitmap_raster(penum->line_width);
	int ix = penum->xci, iw = penum->wci;
	int xinc, xmod;
	byte *row;
	const byte *orig_row = 0;
	fixed fxy =
	  float2fixed(penum->matrix.xy + fixed2float(fixed_epsilon) / 2);
	bool y_neg = fxy < 0;

	if ( is_fneg(penum->matrix.yx) )
	  ix += iw - 1, iw = -iw, xinc = -1;
	else
	  xinc = 1;
	if ( h != 0 )
	  {	for ( ; iw != 0; iw -= xinc )
		  {	xmod = ix & 7;
			row = line + xmod * raster;
			if ( orig_row == 0 )
			  { image_simple_expand(row, 0, raster,
						penum->line_width,
						buffer, w,
						penum->ycur, fxy, 0);
			    orig_row = row;
			  }
			else
			  memcpy(row, orig_row, raster);
			if ( xinc > 0 )
			  {	++ix;
				if ( xmod == 7 )
				  {	int code =
					  copy_landscape(penum,
							 penum->line_xy, ix,
							 y_neg, dev);
					if ( code < 0 )
					  return code;
					orig_row = 0;
					penum->line_xy = ix;
				  }
			  }
			else
			  {	if ( xmod == 0 )
				  {	int code =
					  copy_landscape(penum, ix,
							 penum->line_xy,
							 y_neg, dev);
					if ( code < 0 )
					  return code;
					orig_row = 0;
					penum->line_xy = ix;
				  }
				--ix;
			  }
		  }
		return 0;
	  }
	else
	  {	/* Put out any left-over bits. */
		return
		  (xinc > 0 ?
		   copy_landscape(penum, penum->line_xy, ix, y_neg, dev) :
		   copy_landscape(penum, ix + 1, penum->line_xy, y_neg, dev));
	  }
}

/* Flip and copy one group of scan lines. */
private int
copy_landscape(gx_image_enum *penum, int x0, int x1, bool y_neg,
  gx_device *dev)
{	byte *line = penum->line;
	uint line_width = penum->line_width;
	uint raster = bitmap_raster(line_width);
	byte *flipped = line + raster * 8;

	/* Flip the buffered data from raster * 8 to align_bitmap_mod * */
	/* line_width. */
	if ( line_width > 0 )
	{	int i;
		for ( i = (line_width - 1) >> 3; i >= 0; --i )
		  memflip8x8(line + i, raster,
			     flipped + (i << (log2_align_bitmap_mod + 3)),
			     align_bitmap_mod);
	}

	/* Transfer the scan lines to the device. */
	{	dev_proc_copy_mono((*copy_mono)) = dev_proc(dev, copy_mono);
		gx_color_index
		  zero = penum->icolor0.colors.pure,
		  one = penum->icolor1.colors.pure;
		int w = x1 - x0;
		int y = fixed2int(penum->ycur);

		if ( penum->map[0].table.lookup4x1to32[0] != 0 )
		  zero = penum->icolor1.colors.pure,
		  one = penum->icolor0.colors.pure;
		if ( w < 0 )
		  x0 = x1, w = -w;
		if ( y_neg )
		  y -= line_width;
		return (*copy_mono)(dev, flipped, x0 & 7, align_bitmap_mod,
				    gx_no_bitmap_id,
				    x0, y, w, line_width, zero, one);
	}
}
