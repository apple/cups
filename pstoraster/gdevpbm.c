/* Copyright (C) 1992, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevpbm.c */
/* Portable Bit/Gray/PixMap drivers */
#include "gdevprn.h"
#include "gscdefs.h"
#include "gxlum.h"

/* Thanks are due to Jos Vos (jos@bull.nl) for an earlier P*M driver, */
/* on which this one is based. */

/*
 * There are 6 (pairs of) drivers here:
 *	pbm[raw] - outputs PBM (black and white).
 *	pgm[raw] - outputs PGM (gray-scale).
 *	pgnm[raw] - outputs PBM if the page contains only black and white,
 *	  otherwise PGM.
 *	ppm[raw] - outputs PPM (RGB).
 *	pnm[raw] - outputs PBM if the page contains only black and white,
 *	  otherwise PGM if the page contains only gray shades,
 *	  otherwise PPM.
 *	pkm[raw] - computes internally in CMYK, outputs PPM (RGB).
 */

/*
 * The code here is designed to work with variable depths for PGM and PPM.
 * The code will work with any of the values in brackets, but the
 * Ghostscript imager requires that depth be a power of 2 or be 24,
 * so the actual allowed values are more limited.
 *	pgm,pgnm: 1, 2, 4, 8, 16.  [1-16]
 *	pgmraw,pgnmraw: 1, 2, 4, 8.  [1-8]
 *	ppm,pnm: 4(3x1), 8(3x2), 16(3x5), 24(3x8), 32(3x10).  [3-32]
 *	ppmraw,pnmraw: 4(3x1), 8(3x2), 16(3x5), 24(3x8).  [3-24]
 *	pkm, pkmraw: 4(4x1), 8(4x2), 16(4x4), 32(4x8).  [4-32]
 */

/* Structure for P*M devices, which extend the generic printer device. */

#define MAX_COMMENT 70			/* max user-supplied comment */
struct gx_device_pbm_s {
	gx_device_common;
	gx_prn_device_common;
	/* Additional state for P*M devices */
	char magic;			/* n for "Pn" */
	char comment[MAX_COMMENT + 1];	/* comment for head of file */
	byte is_raw;			/* 1 if raw format, 0 if plain */
	byte optimize;			/* 1 if optimization OK, 0 if not */
	byte uses_color;		/* 0 if image is black and white, */
					/* 1 if gray (PGM or PPM only), */
					/* 2 or 3 if colored (PPM only) */
	int alpha_text;			/* # of alpha bits for text (1,2,4) */
	int alpha_graphics;		/* ditto for graphics (1,2,4) */
};
typedef struct gx_device_pbm_s gx_device_pbm;

#define bdev ((gx_device_pbm *)pdev)

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

/* Macro for generating P*M device descriptors. */
#define pbm_prn_device(procs, dev_name, magic, is_raw, num_comp, depth, max_gray, max_rgb, optimize, print_page)\
{	prn_device_body(gx_device_pbm, procs, dev_name,\
	  DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI,\
	  0, 0, 0, 0,\
	  num_comp, depth, max_gray, max_rgb, max_gray + 1, max_rgb + 1,\
	  print_page),\
	magic,\
	 { 0 },\
	is_raw,\
	optimize,\
	0, 1, 1\
}

/* For all but PBM, we need our own color mapping and alpha procedures. */
private dev_proc_map_rgb_color(pgm_map_rgb_color);
private dev_proc_map_rgb_color(ppm_map_rgb_color);
private dev_proc_map_color_rgb(pgm_map_color_rgb);
private dev_proc_map_color_rgb(ppm_map_color_rgb);
private dev_proc_map_cmyk_color(pkm_map_cmyk_color);
private dev_proc_map_color_rgb(pkm_map_color_rgb);
private dev_proc_put_params(ppm_put_params);
private dev_proc_get_alpha_bits(ppm_get_alpha_bits);

/* We need to initialize uses_color when opening the device, */
/* and after each showpage. */
private dev_proc_open_device(ppm_open);
private dev_proc_output_page(ppm_output_page);

/* And of course we need our own print-page routines. */
private dev_proc_print_page(pbm_print_page);
private dev_proc_print_page(pgm_print_page);
private dev_proc_print_page(ppm_print_page);
private dev_proc_print_page(pkm_print_page);

/* The device procedures */

private gx_device_procs pbm_procs =
    prn_procs(gdev_prn_open, ppm_output_page, gdev_prn_close);

/* See gdevprn.h for the template for the following. */
#define pgpm_procs(p_map_rgb_color, p_map_color_rgb, p_map_cmyk_color) {\
	ppm_open, NULL, NULL, ppm_output_page, gdev_prn_close,\
	p_map_rgb_color, p_map_color_rgb, NULL, NULL, NULL, NULL, NULL, NULL,\
	gdev_prn_get_params, ppm_put_params,\
	p_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device,\
	ppm_get_alpha_bits\
}

private gx_device_procs pgm_procs =
    pgpm_procs(pgm_map_rgb_color, pgm_map_color_rgb, NULL);
private gx_device_procs ppm_procs =
    pgpm_procs(ppm_map_rgb_color, ppm_map_color_rgb, NULL);
private gx_device_procs pkm_procs =
    pgpm_procs(NULL, pkm_map_color_rgb, pkm_map_cmyk_color);

/* The device descriptors themselves */
gx_device_pbm far_data gs_pbm_device =
  pbm_prn_device(pbm_procs, "pbm", '1', 0, 1, 1, 1, 0, 0,
		 pbm_print_page);
gx_device_pbm far_data gs_pbmraw_device =
  pbm_prn_device(pbm_procs, "pbmraw", '4', 1, 1, 1, 1, 1, 0,
		 pbm_print_page);
gx_device_pbm far_data gs_pgm_device =
  pbm_prn_device(pgm_procs, "pgm", '2', 0, 1, 8, 255, 0, 0,
		 pgm_print_page);
gx_device_pbm far_data gs_pgmraw_device =
  pbm_prn_device(pgm_procs, "pgmraw", '5', 1, 1, 8, 255, 0, 0,
		 pgm_print_page);
gx_device_pbm far_data gs_pgnm_device =
  pbm_prn_device(pgm_procs, "pgnm", '2', 0, 1, 8, 255, 0, 1,
		 pgm_print_page);
gx_device_pbm far_data gs_pgnmraw_device =
  pbm_prn_device(pgm_procs, "pgnmraw", '5', 1, 1, 8, 255, 0, 1,
		 pgm_print_page);
gx_device_pbm far_data gs_ppm_device =
  pbm_prn_device(ppm_procs, "ppm", '3', 0, 3, 24, 255, 255, 0,
		 ppm_print_page);
gx_device_pbm far_data gs_ppmraw_device =
  pbm_prn_device(ppm_procs, "ppmraw", '6', 1, 3, 24, 255, 255, 0,
		 ppm_print_page);
gx_device_pbm far_data gs_pnm_device =
  pbm_prn_device(ppm_procs, "pnm", '3', 0, 3, 24, 255, 255, 1,
		 ppm_print_page);
gx_device_pbm far_data gs_pnmraw_device =
  pbm_prn_device(ppm_procs, "pnmraw", '6', 1, 3, 24, 255, 255, 1,
		 ppm_print_page);
gx_device_pbm far_data gs_pkm_device =
  pbm_prn_device(pkm_procs, "pkm", '3', 0, 4, 4, 1, 1, 0,
		 pkm_print_page);
gx_device_pbm far_data gs_pkmraw_device =
  pbm_prn_device(pkm_procs, "pkmraw", '6', 1, 4, 4, 1, 1, 0,
		 pkm_print_page);

/* ------ Initialization ------ */

private int
ppm_open(gx_device *pdev)
{	bdev->uses_color = 0;
	return gdev_prn_open(pdev);
}

/* Print a page, and reset uses_color if this is a showpage. */
private int
ppm_output_page(gx_device *pdev, int num_copies, int flush)
{	int code = gdev_prn_output_page(pdev, num_copies, flush);
	if ( code < 0 )
	  return code;
	if ( flush )
	  bdev->uses_color = 0;
	return code;
}

/* ------ Color mapping routines ------ */

/* Map an RGB color to a PGM gray value. */
/* Keep track of whether the image is black-and-white or gray. */
private gx_color_index
pgm_map_rgb_color(gx_device *pdev, ushort r, ushort g, ushort b)
{	/* We round the value rather than truncating it. */
	gx_color_value gray =
		((r * (ulong)lum_red_weight) +
		 (g * (ulong)lum_green_weight) +
		 (b * (ulong)lum_blue_weight) +
		 (lum_all_weights / 2)) / lum_all_weights
		* pdev->color_info.max_gray / gx_max_color_value;
	if ( !(gray == 0 || gray == pdev->color_info.max_gray) )
	  bdev->uses_color = 1;
	return gray;
}

/* Map a PGM gray value back to an RGB color. */
private int
pgm_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{	gx_color_value gray =
	  color * gx_max_color_value / dev->color_info.max_gray;
	prgb[0] = gray;
	prgb[1] = gray;
	prgb[2] = gray;
	return 0;
}

/* Map an RGB color to a PPM color tuple. */
/* Keep track of whether the image is black-and-white, gray, or colored. */
private gx_color_index
ppm_map_rgb_color(gx_device *pdev, ushort r, ushort g, ushort b)
{	uint bitspercolor = pdev->color_info.depth / 3;
	ulong max_value = pdev->color_info.max_color;
	gx_color_value rc = r * max_value / gx_max_color_value;
	gx_color_value gc = g * max_value / gx_max_color_value;
	gx_color_value bc = b * max_value / gx_max_color_value;
	if ( rc == gc && gc == bc )		/* black-and-white or gray */
	{	if ( !(rc == 0 || rc == max_value) )
		  bdev->uses_color |= 1;		/* gray */
	}
	else						/* color */
	  bdev->uses_color = 2;
	return ((((ulong)rc << bitspercolor) + gc) << bitspercolor) + bc;
}

/* Map a PPM color tuple back to an RGB color. */
private int
ppm_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{	uint bitspercolor = dev->color_info.depth / 3;
	uint colormask = (1 << bitspercolor) - 1;
	uint max_rgb = dev->color_info.max_color;

	prgb[0] = ((color >> (bitspercolor * 2)) & colormask) *
	  (ulong)gx_max_color_value / max_rgb;
	prgb[1] = ((color >> bitspercolor) & colormask) *
	  (ulong)gx_max_color_value / max_rgb;
	prgb[2] = (color & colormask) *
	  (ulong)gx_max_color_value / max_rgb;
	return 0;
}

/* Map a CMYK color to a pixel value. */
private gx_color_index
pkm_map_cmyk_color(gx_device *pdev, ushort c, ushort m, ushort y, ushort k)
{	uint bitspercolor = pdev->color_info.depth >> 2;
	ulong max_value = pdev->color_info.max_color;
	uint cc = c * max_value / gx_max_color_value;
	uint mc = m * max_value / gx_max_color_value;
	uint yc = y * max_value / gx_max_color_value;
	uint kc = k * max_value / gx_max_color_value;
	gx_color_index color =
	  ((((((ulong)cc << bitspercolor) + mc) << bitspercolor) + yc)
	   << bitspercolor) + kc;

	return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Map a CMYK pixel value to RGB. */
private int
pkm_map_color_rgb(gx_device *dev, gx_color_index color, gx_color_value rgb[3])
{	gx_color_index cshift = color;
	int bpc = dev->color_info.depth >> 2;
	uint mask = (1 << bpc) - 1;
	uint max_value = dev->color_info.max_color;
	uint c, m, y, k;

	k = cshift & mask;  cshift >>= bpc;
	y = cshift & mask;  cshift >>= bpc;
	m = cshift & mask;
	c = cshift >> bpc;
#define cvalue(c)\
  ((gx_color_value)((ulong)(c) * gx_max_color_value / max_value))
	/* We use our improved conversion rule.... */
	rgb[0] = cvalue((max_value - c) * (max_value - k) / max_value);
	rgb[1] = cvalue((max_value - m) * (max_value - k) / max_value);
	rgb[2] = cvalue((max_value - y) * (max_value - k) / max_value);
#undef cvalue
	return 0;
}

/* ------ Alpha capability ------ */

/* Put parameters. */
private int
ppm_put_alpha_param(gs_param_list *plist, gs_param_name param_name, int *pa,
  bool alpha_ok)
{	int code = param_read_int(plist, param_name, pa);
	switch ( code )
	{
	case 0:
		switch ( *pa )
		  {
		  case 1:
			return 0;
		  case 2: case 4:
			if ( alpha_ok )
			  return 0;
		  default:
			code = gs_error_rangecheck;
		  }
	default:
		param_signal_error(plist, param_name, code);
	case 1:
		;
	}
	return code;
}
private int
ppm_put_params(gx_device *pdev, gs_param_list *plist)
{	gx_device_color_info save_info;
	int ncomps = pdev->color_info.num_components;
	int bpc = pdev->color_info.depth / ncomps;
	int ecode = 0;
	int code;
	int atext = bdev->alpha_text, agraphics = bdev->alpha_graphics;
	bool alpha_ok;
	long v;
	const char _ds *vname;

	save_info = pdev->color_info;
	if ( (code = param_read_long(plist, (vname = "GrayValues"), &v)) != 1 ||
	     (code = param_read_long(plist, (vname = "RedValues"), &v)) != 1 ||
	     (code = param_read_long(plist, (vname = "GreenValues"), &v)) != 1 ||
	     (code = param_read_long(plist, (vname = "BlueValues"), &v)) != 1
	   )
	  {	if ( code < 0 )
		  ecode = code;
		else if ( v < 2 || v > (bdev->is_raw || ncomps > 1 ? 256 : 65536L) )
		  param_signal_error(plist, vname,
				     ecode = gs_error_rangecheck);
		else if ( v == 2 )
		  bpc = 1;
		else if ( v <= 4 )
		  bpc = 2;
		else if ( v <= 16 )
		  bpc = 4;
		else if ( v <= 32 && ncomps == 3 )
		  bpc = 5;
		else if ( v <= 256 )
		  bpc = 8;
	  	else
		  bpc = 16;
	  	if ( ecode >= 0 )
		  { static const byte depths[4][16] = {
		      { 1, 2, 0, 4, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 16 },
		      { 0 },
		      { 4, 8, 0, 16, 16, 0, 0, 24 },
		      { 4, 8, 0, 16, 0, 0, 0, 24 },
		    };
		    pdev->color_info.depth = depths[ncomps - 1][bpc - 1];
		    pdev->color_info.max_gray = pdev->color_info.max_color =
		      (pdev->color_info.dither_grays =
		       pdev->color_info.dither_colors = (int)v) - 1;
		  }
	  }
	alpha_ok = bpc >= 5;
	if ( (code = ppm_put_alpha_param(plist, "TextAlphaBits", &bdev->alpha_text, alpha_ok)) < 0 )
	  ecode = code;
	if ( (code = ppm_put_alpha_param(plist, "GraphicsAlphaBits", &bdev->alpha_graphics, alpha_ok)) < 0 )
	  ecode = code;
	if ( (code = ecode) < 0 ||
	     (code = gdev_prn_put_params(pdev, plist)) < 0
	   )
	  { bdev->alpha_text = atext;
	    bdev->alpha_graphics = agraphics;
	    pdev->color_info = save_info;
	  }
	return code;
}

/* Get the number of alpha bits. */
private int
ppm_get_alpha_bits(gx_device *pdev, graphics_object_type type)
{	return (type == go_text ? bdev->alpha_text : bdev->alpha_graphics);
}

/* ------ Internal routines ------ */

/* Print a page using a given row printing routine. */
private int
pbm_print_page_loop(gx_device_printer *pdev, char magic, FILE *pstream,
  int (*row_proc)(P4(gx_device_printer *, byte *, int, FILE *)))
{	uint raster = gdev_prn_raster(pdev);
	byte *data = (byte *)gs_malloc(raster, 1, "pbm_begin_page");
	int lnum = 0;
	int code = 0;
	if ( data == 0 )
	  return_error(gs_error_VMerror);
	fprintf(pstream, "P%c\n", magic);
	if ( bdev->comment[0] )
	  fprintf(pstream, "# %s\n", bdev->comment);
	else
	  fprintf(pstream, "# Image generated by %s (device=%s)\n",
		  gs_product, pdev->dname);
	fprintf(pstream, "%d %d\n", pdev->width, pdev->height);
	switch ( magic )
	{
	case '1':		/* pbm */
	case '4':		/* pbmraw */
		break;
	default:
		fprintf(pstream, "%d\n", pdev->color_info.max_gray);
	}
	for ( ; lnum < pdev->height; lnum++ )
	{	byte *row;
		code = gdev_prn_get_bits(pdev, lnum, data, &row);
		if ( code < 0 ) break;
		code = (*row_proc)(pdev, row, pdev->color_info.depth, pstream);
		if ( code < 0 ) break;
	}
	gs_free((char *)data, raster, 1, "pbm_print_page_loop");
	return (code < 0 ? code : 0);
}

/* ------ Individual page printing routines ------ */

/* Print a monobit page. */
private int
pbm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream)
{	if ( bdev->is_raw )
	  fwrite(data, 1, (pdev->width + 7) >> 3, pstream);
	else
	{	byte *bp;
		uint x, mask;
		for ( bp = data, x = 0, mask = 0x80; x < pdev->width; )
		{	putc((*bp & mask ? '1' : '0'), pstream);
			if ( ++x == pdev->width || !(x & 63) )
			  putc('\n', pstream);
			if ( (mask >>= 1) == 0 )
			  bp++, mask = 0x80;
		}
	}
	return 0;
}
private int
pbm_print_page(gx_device_printer *pdev, FILE *pstream)
{	return pbm_print_page_loop(pdev, bdev->magic, pstream, pbm_print_row);
}

/* Print a gray-mapped page. */
private int
pgm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream)
{	/* Note that bpp <= 8 for raw format, bpp <= 16 for plain. */
	uint mask = (1 << depth) - 1;
	byte *bp;
	uint x;
	int shift;
	if ( bdev->is_raw && depth == 8 )
	  fwrite(data, 1, pdev->width, pstream);
	else
	  for ( bp = data, x = 0, shift = 8 - depth; x < pdev->width; )
	{	uint pixel;
		if ( shift < 0 )	/* bpp = 16 */
		   {	pixel = ((uint)*bp << 8) + bp[1];
			bp += 2;
		   }
		else
		   {	pixel = (*bp >> shift) & mask;
			if ( (shift -= depth) < 0 )
			  bp++, shift += 8;
		   }
		++x;
		if ( bdev->is_raw )
		  putc(pixel, pstream);
		else
		  fprintf(pstream, "%d%c", pixel,
			  (x == pdev->width || !(x & 15) ? '\n' : ' '));
	}
	return 0;
}
private int
pxm_pbm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream)
{	/* Compress a PGM or PPM row to a PBM row. */
	/* This doesn't have to be very fast. */
	/* Note that we have to invert the data as well. */
	int delta = (depth + 7) >> 3;
	byte *src = data + delta - 1;		/* always big-endian */
	byte *dest = data;
	int x;
	byte out_mask = 0x80;
	byte out = 0;
	if ( depth >= 8 )
	  {	/* One or more bytes per source pixel. */
		for ( x = 0; x < pdev->width; x++, src += delta )
		  {	if ( !(*src & 1) )
			  out |= out_mask;
			out_mask >>= 1;
			if ( !out_mask )
			  out_mask = 0x80,
			  *dest++ = out,
			  out = 0;
		  }
	  }
	else
	  {	/* Multiple source pixels per byte. */
		byte in_mask = 0x100 >> depth;
		for ( x = 0; x < pdev->width; x++ )
		  {	if ( !(*src & in_mask) )
			  out |= out_mask;
			in_mask >>= depth;
			if ( !in_mask )
			  in_mask = 0x100 >> depth,
			  src++;
			out_mask >>= 1;
			if ( !out_mask )
			  out_mask = 0x80,
			  *dest++ = out,
			  out = 0;
		  }
	}
	if ( out_mask != 0x80 )
	  *dest = out;
	return pbm_print_row(pdev, data, 1, pstream);
}
private int
pgm_print_page(gx_device_printer *pdev, FILE *pstream)
{	return (bdev->uses_color == 0 && bdev->optimize ?
		pbm_print_page_loop(pdev, bdev->magic - 1, pstream,
				    pxm_pbm_print_row) :
		pbm_print_page_loop(pdev, bdev->magic, pstream,
				    pgm_print_row) );
}

/* Print a color-mapped page. */
private int
ppgm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream, bool color)
{	/* If color=false, write only one value per pixel; */
	/* if color=true, write 3 values per pixel. */
	/* Note that depth <= 24 for raw format, depth <= 32 for plain. */
	uint bpe = depth / 3;	/* bits per r/g/b element */
	uint mask = (1 << bpe) - 1;
	byte *bp;
	uint x;
	uint eol_mask = (color ? 7 : 15);
	int shift;
	if ( bdev->is_raw && depth == 24 && color )
	  fwrite(data, 1, pdev->width * (depth / 8), pstream);
	else
	  for ( bp = data, x = 0, shift = 8 - depth; x < pdev->width; )
	{	bits32 pixel = 0;
		uint r, g, b;
		switch ( depth >> 3 )
		   {
		case 4:
			pixel = (bits32)*bp << 24; bp++;
			/* falls through */
		case 3:
			pixel += (bits32)*bp << 16; bp++;
			/* falls through */
		case 2:
			pixel += (uint)*bp << 8; bp++;
			/* falls through */
		case 1:
			pixel += *bp; bp++;
			break;
		case 0:			/* bpp == 4, bpe == 1 */
			pixel = *bp >> shift;
			if ( (shift -= depth) < 0 )
			  bp++, shift += 8;
			break;
		   }
		++x;
		b = pixel & mask;  pixel >>= bpe;
		g = pixel & mask;  pixel >>= bpe;
		r = pixel & mask;
		if ( bdev->is_raw )
		{	if ( color )
			{	putc(r, pstream);
				putc(g, pstream);
			}
			putc(b, pstream);
		}
		else
		{	if ( color )
			  fprintf(pstream, "%d %d ", r, g);
			fprintf(pstream, "%d%c", b,
				(x == pdev->width || !(x & eol_mask) ?
				 '\n' : ' '));
		}
	}
	return 0;
}
private int
ppm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream)
{	return ppgm_print_row(pdev, data, depth, pstream, true);
}
private int
ppm_pgm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream)
{	return ppgm_print_row(pdev, data, depth, pstream, false);
}
private int
ppm_print_page(gx_device_printer *pdev, FILE *pstream)
{	return (bdev->uses_color >= 2 || !bdev->optimize ?
		pbm_print_page_loop(pdev, bdev->magic, pstream,
				    ppm_print_row) :
		bdev->uses_color == 1 ?
		pbm_print_page_loop(pdev, bdev->magic - 1, pstream,
				    ppm_pgm_print_row) :
		pbm_print_page_loop(pdev, bdev->magic - 2, pstream,
				    pxm_pbm_print_row) );
}

/* Print a faux CMYK page. */
private int
pkm_print_row(gx_device_printer *pdev, byte *data, int depth,
  FILE *pstream)
{	byte *bp;
	uint x;
	int shift;
	ulong max_value = pdev->color_info.max_color;
	uint mask = (depth >= 8 ? 0xff : (1 << depth) - 1);

	for ( bp = data, x = 0, shift = 8 - depth; x < pdev->width; )
	{	bits32 pixel = 0;
		gx_color_value rgb[3];
		uint r, g, b;

		switch ( depth >> 3 )
		   {
		case 4:
			pixel = (bits32)*bp << 24; bp++;
			/* falls through */
		case 3:
			pixel += (bits32)*bp << 16; bp++;
			/* falls through */
		case 2:
			pixel += (uint)*bp << 8; bp++;
			/* falls through */
		case 1:
			pixel += *bp; bp++;
			break;
		case 0:			/* bpp == 4 */
			pixel = (*bp >> shift) & mask;
			if ( (shift -= depth) < 0 )
			  bp++, shift += 8;
			break;
		   }
		++x;
		pkm_map_color_rgb((gx_device *)pdev, pixel, rgb);
		r = rgb[0] * max_value / gx_max_color_value;
		g = rgb[1] * max_value / gx_max_color_value;
		b = rgb[2] * max_value / gx_max_color_value;
		if ( bdev->is_raw )
		{	putc(r, pstream);
			putc(g, pstream);
			putc(b, pstream);
		}
		else
		{	fprintf(pstream, "%d %d %d%c", r, g, b,
				(x == pdev->width || !(x & 7) ?
				 '\n' : ' '));
		}
	}
	return 0;
}
private int
pkm_print_page(gx_device_printer *pdev, FILE *pstream)
{	return pbm_print_page_loop(pdev, bdev->magic, pstream,
				   pkm_print_row);
}
