/* Copyright (C) 1991, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevbit.c */
/* "Plain bits" devices to measure rendering time. */
#include "gdevprn.h"
#include "gsparam.h"
#include "gxlum.h"

/*
 * When debugging problems on large CMYK devices, we have to be able to
 * modify the output at this stage.  These parameters are not set in any
 * normal configuration.
 */
/* Define whether to trim off top and bottom white space. */
/*#define TRIM_TOP_BOTTOM*/
/* Define left and right trimming margins. */
/* Note that this is only approximate: we trim to byte boundaries. */
/*#define TRIM_LEFT 400*/
/*#define TRIM_RIGHT 400*/
/* Define whether to expand each bit to a byte. */
/* Also convert black-and-white to proper gray (inverting if monobit). */
/*#define EXPAND_BITS_TO_BYTES*/

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
private dev_proc_map_rgb_color(bit_mono_map_rgb_color);
private dev_proc_map_rgb_color(bit_map_rgb_color);
private dev_proc_map_color_rgb(bit_map_color_rgb);
private dev_proc_map_cmyk_color(bit_map_cmyk_color);
private dev_proc_put_params(bit_put_params);
private dev_proc_print_page(bit_print_page);
#define bit_procs(map_rgb_color, map_cmyk_color)\
{	gdev_prn_open,\
	gx_default_get_initial_matrix,\
	NULL,	/* sync_output */\
	gdev_prn_output_page,\
	gdev_prn_close,\
	map_rgb_color,\
	bit_map_color_rgb,\
	NULL,	/* fill_rectangle */\
	NULL,	/* tile_rectangle */\
	NULL,	/* copy_mono */\
	NULL,	/* copy_color */\
	NULL,	/* draw_line */\
	NULL,	/* get_bits */\
	gdev_prn_get_params,\
	bit_put_params,\
	map_cmyk_color,\
	NULL,	/* get_xfont_procs */\
	NULL,	/* get_xfont_device */\
	NULL,	/* map_rgb_alpha_color */\
	gx_page_device_get_page_device	/* get_page_device */\
}

private gx_device_procs bitmono_procs =
  bit_procs(bit_mono_map_rgb_color, NULL);
gx_device_printer far_data gs_bit_device =
{ prn_device_body(gx_device_printer, bitmono_procs, "bit",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1,1,1,0,2,1, bit_print_page)
};

private gx_device_procs bitrgb_procs =
  bit_procs(bit_map_rgb_color, NULL);
gx_device_printer far_data gs_bitrgb_device =
{ prn_device_body(gx_device_printer, bitrgb_procs, "bitrgb",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	3,4,1,1,2,2, bit_print_page)
};

private gx_device_procs bitcmyk_procs =
  bit_procs(NULL, bit_map_cmyk_color);
gx_device_printer far_data gs_bitcmyk_device =
{ prn_device_body(gx_device_printer, bitcmyk_procs, "bitcmyk",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	4,4,1,1,2,2, bit_print_page)
};

/* Map gray to color. */
/* Note that 1-bit monochrome is a special case. */
private gx_color_index
bit_mono_map_rgb_color(gx_device *dev, gx_color_value red,
  gx_color_value green, gx_color_value blue)
{	int bpc = dev->color_info.depth;
	int drop = sizeof(gx_color_value) * 8 - bpc;
	gx_color_value gray = 
	  (red * (unsigned long)lum_red_weight +
	   green * (unsigned long)lum_green_weight +
	   blue * (unsigned long)lum_blue_weight +
	   (lum_all_weights / 2))
	    / lum_all_weights;
	return (bpc == 1 ? gx_max_color_value - gray : gray) >> drop;
}

/* Map RGB to color. */
private gx_color_index
bit_map_rgb_color(gx_device *dev, gx_color_value red,
  gx_color_value green, gx_color_value blue)
{	int bpc = dev->color_info.depth / 3;
	int drop = sizeof(gx_color_value) * 8 - bpc;
	return ((((red >> drop) << bpc) + (green >> drop)) << bpc) +
	  (blue >> drop);
}

/* Map color to RGB.  This has 3 separate cases, but since it is rarely */
/* used, we do a case test rather than providing 3 separate routines. */
private int
bit_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{	int depth = dev->color_info.depth;
	int ncomp = dev->color_info.num_components;
	int bpc = depth / ncomp;
	uint mask = (1 << bpc) - 1;
#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

	switch ( ncomp )
	{
	case 1:				/* gray */
		rgb[0] = rgb[1] = rgb[2] =
		  (depth == 1 ? (color ? 0 : gx_max_color_value) :
		   cvalue(color));
		break;
	case 3:				/* RGB */
		{ gx_color_index cshift = color;
		  rgb[2] = cvalue(cshift & mask);  cshift >>= bpc;
		  rgb[1] = cvalue(cshift & mask);
		  rgb[0] = cvalue(cshift >> bpc);
		}
		break;
	case 4:				/* CMYK */
		/* Map CMYK back to RGB. */
		{ gx_color_index cshift = color;
		  uint c, m, y, k;
		  
		  k = cshift & mask;  cshift >>= bpc;
		  y = cshift & mask;  cshift >>= bpc;
		  m = cshift & mask;
		  c = cshift >> bpc;
		  /* We use our improved conversion rule.... */
		  rgb[0] = cvalue((mask - c) * (mask - k) / mask);
		  rgb[1] = cvalue((mask - m) * (mask - k) / mask);
		  rgb[2] = cvalue((mask - y) * (mask - k) / mask);
		}
		break;
	}
	return 0;
#undef cvalue
}

/* Map CMYK to color. */
private gx_color_index
bit_map_cmyk_color(gx_device *dev, gx_color_value cyan,
  gx_color_value magenta, gx_color_value yellow, gx_color_value black)
{	int bpc = dev->color_info.depth / 4;
	int drop = sizeof(gx_color_value) * 8 - bpc;
	gx_color_index color =
	  ((((((cyan >> drop) << bpc) +
	      (magenta >> drop)) << bpc) +
	    (yellow >> drop)) << bpc) +
	  (black >> drop);

	return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Set parameters.  We allow setting the number of bits per component. */
private int
bit_put_params(gx_device *pdev, gs_param_list *plist)
{	gx_device_color_info save_info;
	int ncomps = pdev->color_info.num_components;
	int bpc = pdev->color_info.depth / ncomps;
	int v;
	int ecode = 0;
	int code;
	static const byte depths[4][8] = {
	  { 1, 2, 0, 4, 8, 0, 0, 8 },
	  { 0 },
	  { 4, 8, 0, 16, 16, 0, 0, 24 },
	  { 4, 8, 0, 16, 32, 0, 0, 32 }
	};
	const char _ds *vname;

	if ( (code = param_read_int(plist, (vname = "GrayValues"), &v)) != 1 ||
	     (code = param_read_int(plist, (vname = "RedValues"), &v)) != 1 ||
	     (code = param_read_int(plist, (vname = "GreenValues"), &v)) != 1 ||
	     (code = param_read_int(plist, (vname = "BlueValues"), &v)) != 1
	   )
	{	if ( code < 0 )
		  ecode = code;
		else switch ( v )
		{
		case 2: bpc = 1; break;
		case 4: bpc = 2; break;
		case 16: bpc = 4; break;
		case 32: bpc = 5; break;
		case 256: bpc = 8; break;
		default: param_signal_error(plist, vname,
					    ecode = gs_error_rangecheck);
		}
	}

	if ( ecode < 0 )
	  return ecode;
	/* Temporarily reset the color_info so that gdev_prn_put_params */
	/* won't complain. */
	save_info = pdev->color_info;
	if ( code != 1 )
	  {	pdev->color_info.depth = depths[ncomps - 1][bpc - 1];
		pdev->color_info.max_gray = pdev->color_info.max_color =
		  (pdev->color_info.dither_grays =
		   pdev->color_info.dither_colors =
		     (1 << bpc)) - 1;
	  }
	ecode = gdev_prn_put_params(pdev, plist);
	if ( ecode < 0 )
	  {	pdev->color_info = save_info;
		return ecode;
	  }
	if ( code != 1 )
	  {	if ( pdev->is_open )
		  gs_closedevice(pdev);
	  }
	return 0;
}

/* Send the page to the printer. */
private int
bit_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	/* Just dump the bits on the file. */
	/* If the file is 'nul', don't even do the writes. */
	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	byte *in = (byte *)gs_malloc(line_size, 1, "bit_print_page(in)");
	byte *data;
	int nul = !strcmp(pdev->fname, "nul");
	int lnum = 0, bottom = pdev->height;

	if ( in == 0 )
	  return_error(gs_error_VMerror);
#ifdef TRIM_TOP_BOTTOM
	{ gx_color_index white =
	    (pdev->color_info.num_components == 4 ? 0 :
	     (*dev_proc(pdev, map_rgb_color))
	       ((gx_device *)pdev, gx_max_color_value, gx_max_color_value,
		gx_max_color_value));
#define color_index_bits (sizeof(gx_color_index) * 8)
	  const gx_color_index *p;
	  const gx_color_index *end;
	  int depth = pdev->color_info.depth;
	  int end_bits = ((long)line_size * depth) % color_index_bits;
	  gx_color_index end_mask =
	    (end_bits == 0 ? 0 : -1 << (color_index_bits - end_bits));
	  int i;

	  for ( i = depth; i < color_index_bits; i += depth )
	    white |= white << depth;
	  /* Remove bottom white space. */
	  for ( ; bottom - lnum > 1; --bottom )
	    { gdev_prn_get_bits(pdev, bottom - 1, in, &data);
	      p = (const gx_color_index *)data;
	      end = p + line_size / sizeof(gx_color_index);
	      for ( ; p < end; ++p )
		if ( *p != white )
		  goto bx;
	      if ( end_mask != 0 && ((*end ^ white) & end_mask) != 0 )
		goto bx;
	    }
	  /* Remove top white space. */
bx:	  for ( ; lnum < bottom; ++lnum )
	    { gdev_prn_get_bits(pdev, lnum, in, &data);
	      p = (const gx_color_index *)data;
	      end = p + line_size / sizeof(gx_color_index);
	      for ( ; p < end; ++p )
		if ( *p != white )
		  goto tx;
	      if ( end_mask != 0 && ((*end ^ white) & end_mask) != 0 )
		goto tx;
	    }
tx:	  ;
	}
#endif
	for ( ; lnum < bottom; ++lnum )
	  {	gdev_prn_get_bits(pdev, lnum, in, &data);
		if ( !nul )
		  { const byte *row = data;
		    uint len = line_size;
#ifdef TRIM_LEFT
		    row += (TRIM_LEFT * pdev->color_info.depth) >> 3;
#endif
#ifdef TRIM_RIGHT
		    len = data + ((TRIM_RIGHT * pdev->color_info.depth + 7) >> 3) - row;
#endif
#ifdef EXPAND_BITS_TO_BYTES
		    { uint i;
		      byte invert = (pdev->color_info.depth == 1 ? 0xff : 0);
		      for ( i = 0; i < len; ++i )
			{ byte b = row[i] ^ invert;
			  putc(-(b >> 7) & 0xff, prn_stream);
			  putc(-((b >> 6) & 1) & 0xff, prn_stream);
			  putc(-((b >> 5) & 1) & 0xff, prn_stream);
			  putc(-((b >> 4) & 1) & 0xff, prn_stream);
			  putc(-((b >> 3) & 1) & 0xff, prn_stream);
			  putc(-((b >> 2) & 1) & 0xff, prn_stream);
			  putc(-((b >> 1) & 1) & 0xff, prn_stream);
			  putc(-(b & 1) & 0xff, prn_stream);
			}
		    }
#else
		    fwrite(row, 1, len, prn_stream);
#endif
		  }
	  }
	gs_free((char *)in, line_size, 1, "bit_print_page(in)");
	return 0;
}
