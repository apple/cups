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

/* gxcindex.h */
/* Define the device color index type and macros */
/* Requires gxbitmap.h. */

#ifndef gxcindex_INCLUDED
#  define gxcindex_INCLUDED

/*
 * Internally, a (pure) device color is represented by opaque values of
 * type gx_color_index, which are tied to the specific device.  The driver
 * maps between these values and RGB[alpha] or CMYK values.  In this way,
 * the driver can convert RGB values to its most natural color representation,
 * and have the graphics library cache the result.
 */

/* Define the type for device color indices. */
typedef unsigned long gx_color_index;
#define arch_log2_sizeof_color_index arch_log2_sizeof_long
#define arch_sizeof_color_index arch_sizeof_long

/* Define the 'transparent' color index. */
#define gx_no_color_index_value (-1)	/* no cast -> can be used in #if */

/* The SGI C compiler provided with Irix 5.2 gives error messages */
/* if we use the proper definition of gx_no_color_index: */
/*#define gx_no_color_index ((gx_color_index)gx_no_color_index_value)*/
/* Instead, we must spell out the typedef: */
#define gx_no_color_index ((unsigned long)gx_no_color_index_value)

/*
 * Define macros for accumulating a scan line of a colored image.
 * The usage is as follows:
 *	declare_line_accum(line, bpp, xo);
 *	for ( x = xo; x < xe; ++x )
 *	  {	<< compute color at x >>
 *		line_accum(color, bpp);
 *	  }
 *	line_accum_copy(dev, line, bpp, xo, xe, raster, y);
 * This code must be enclosed in { }, since declare_line_accum declares
 * variables.
 *
 * Note that declare_line_accum declares the variables l_dst, l_bits, l_shift,
 * and l_xprev.  Other code in the loop may use these variables.
 */
#define declare_line_accum(line, bpp, xo)\
	byte *l_dst = (line);\
	uint l_bits = 0;\
	int l_shift = 8 - (bpp);\
	int l_xprev = (xo)
#define line_accum(color, bpp)\
	switch ( (bpp) >> 3 )\
	  {\
	  case 0:\
	    l_bits += (uint)((color) << l_shift);\
	    if ( (l_shift -= (bpp)) < 0 )\
	      *l_dst++ = (byte)l_bits, l_bits = 0,\
	      l_shift += 8;\
	    break;\
	  case 4: *l_dst++ = (byte)((color) >> 24);\
	  case 3: *l_dst++ = (byte)((color) >> 16);\
	  case 2: *l_dst++ = (byte)((color) >> 8);\
	  case 1: *l_dst++ = (byte)(color);\
	  }
#define line_accum_store(bpp)\
	if ( l_shift != 8 - (bpp) )\
	  *l_dst = (byte)l_bits
#define line_accum_copy(dev, line, bpp, xo, xe, raster, y)\
	if ( (xe) > l_xprev )\
	  { int code;\
	    line_accum_store(bpp);\
	    code = (*dev_proc(dev, copy_color))\
	      (dev, line, l_xprev - (xo), raster,\
	       gx_no_bitmap_id, l_xprev, y, (xe) - l_xprev, 1);\
	    if ( code < 0 )\
	      return code;\
	  }

#endif					/* gxcindex_INCLUDED */
