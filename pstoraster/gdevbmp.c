/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevbmp.c */
/* .BMP file format output drivers */
#include "gdevprn.h"
#include "gdevpccm.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

private dev_proc_print_page(bmp_print_page);

/* Monochrome. */

gx_device_printer far_data gs_bmpmono_device =
  prn_device(prn_std_procs, "bmpmono",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, bmp_print_page);

/* 4-bit planar (EGA/VGA-style) color. */

private gx_device_procs bmp16_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    pc_4bit_map_rgb_color, pc_4bit_map_color_rgb);
gx_device_printer far_data gs_bmp16_device =
  prn_device(bmp16_procs, "bmp16",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	4, bmp_print_page);

/* 8-bit (SuperVGA-style) color. */
/* (Uses a fixed palette of 3,3,2 bits.) */

private gx_device_procs bmp256_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);
gx_device_printer far_data gs_bmp256_device =
  prn_device(bmp256_procs, "bmp256",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	8, bmp_print_page);

/* 24-bit color. */

private dev_proc_map_rgb_color(map_16m_rgb_color);
private dev_proc_map_color_rgb(map_16m_color_rgb);
private gx_device_procs bmp16m_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    map_16m_rgb_color, map_16m_color_rgb);
gx_device_printer far_data gs_bmp16m_device =
  prn_device(bmp16m_procs, "bmp16m",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	24, bmp_print_page);

/* ------ Private definitions ------ */

/* All multi-byte quantities are stored LSB-first! */
typedef ushort word;
#if arch_sizeof_int == 4
typedef uint dword;
#else
#  if arch_sizeof_long == 4
typedef ulong dword;
#  endif
#endif
#if arch_is_big_endian
#  define assign_word(a,v) a = ((v) >> 8) + ((v) << 8)
#  define assign_dword(a,v)\
     a = ((v) >> 24) + (((v) >> 8) & 0xff00L) +\
	 (((dword)(v) << 8) & 0xff0000L) + ((dword)(v) << 24)
#else
#  define assign_word(a,v) a = (v)
#  define assign_dword(a,v) a = (v)
#endif

typedef struct bmp_file_header_s {

	/* BITMAPFILEHEADER */

	/*
	 * This structure actually begins with two bytes
	 * containing the characters 'BM', but we must omit them,
	 * because some compilers would insert padding to force
	 * the size member to a 32- or 64-bit boundary.
	 */

	/*byte	typeB, typeM;*/	/* always 'BM' */
	dword	size;		/* total size of file */
	word	reserved1;
	word	reserved2;
	dword	offBits;	/* offset of bits from start of file */
	
} bmp_file_header;
#define sizeof_bmp_file_header (2 + sizeof(bmp_file_header))

typedef struct bmp_info_header_s {

	/* BITMAPINFOHEADER */

	dword	size;		/* size of info header in bytes */
	dword	width;		/* width in pixels */
	dword	height;		/* height in pixels */
	word	planes;		/* # of planes, always 1 */
	word	bitCount;	/* bits per pixel */
	dword	compression;	/* compression scheme, always 0 */
	dword	sizeImage;	/* size of bits */
	dword	xPelsPerMeter;	/* X pixels per meter */
	dword	yPelsPerMeter;	/* Y pixels per meter */
	dword	clrUsed;	/* # of colors used */
	dword	clrImportant;	/* # of important colors */

	/* This is followed by (1 << bitCount) bmp_quad structures, */
	/* unless bitCount == 24. */

} bmp_info_header;

typedef struct bmp_quad_s {

	/* RGBQUAD */

	byte	blue, green, red, reserved;

} bmp_quad;

/* Write out a page in BMP format. */
/* This routine is used for all formats. */
private int
bmp_print_page(gx_device_printer *pdev, FILE *file)
{	int raster = gdev_prn_raster(pdev);
	/* BMP scan lines are padded to 32 bits. */
	ulong bmp_raster = raster + (-raster & 3);
	int height = pdev->height;
	int depth = pdev->color_info.depth;
	int quads = (depth <= 8 ? sizeof(bmp_quad) << depth : 0);
	byte *row = (byte *)gs_malloc(bmp_raster, 1, "bmp file buffer");
	int y;
	int code = 0;			/* return code */

	if ( row == 0 )			/* can't allocate row buffer */
		return_error(gs_error_VMerror);

	/* Write the file header. */

	fputc('B', file);
	fputc('M', file);
	{	bmp_file_header fhdr;
		assign_dword(fhdr.size,
			sizeof_bmp_file_header +
			sizeof(bmp_info_header) + quads +
			bmp_raster * height);
		assign_word(fhdr.reserved1, 0);
		assign_word(fhdr.reserved2, 0);
		assign_dword(fhdr.offBits,
			sizeof_bmp_file_header +
			sizeof(bmp_info_header) + quads);
		if ( fwrite((const char *)&fhdr, 1, sizeof(fhdr), file) != sizeof(fhdr) )
		{	code = gs_error_ioerror;
			goto bmp_done;
		}
	}
	
	/* Write the info header. */

	{	bmp_info_header ihdr;
		assign_dword(ihdr.size, sizeof(ihdr));
		assign_dword(ihdr.width, pdev->width);
		assign_dword(ihdr.height, height);
		assign_word(ihdr.planes, 1);
		assign_word(ihdr.bitCount, depth);
		assign_dword(ihdr.compression, 0);
		assign_dword(ihdr.sizeImage, bmp_raster * height);
		/* Even though we could compute the resolution correctly, */
		/* the convention seems to be to leave it unspecified. */
		assign_dword(ihdr.xPelsPerMeter, 0);
		    /*(dword)(pdev->x_pixels_per_inch * (1000.0 / 30.48)));*/
		assign_dword(ihdr.yPelsPerMeter, 0);
		    /*(dword)(pdev->y_pixels_per_inch * (1000.0 / 30.48)));*/
		assign_dword(ihdr.clrUsed, 0);
		assign_dword(ihdr.clrImportant, 0);
		if ( fwrite((const char *)&ihdr, 1, sizeof(ihdr), file) != sizeof(ihdr) )
		{	code = gs_error_ioerror;
			goto bmp_done;
		}
	}

	/* Write the palette. */

	if ( depth <= 8 )
	{	int i;
		gx_color_value rgb[3];
		bmp_quad q;
		q.reserved = 0;
		for ( i = 0; i != 1 << depth; i++ )
		{	(*dev_proc(pdev, map_color_rgb))((gx_device *)pdev,
				(gx_color_index)i, rgb);
			q.red = gx_color_value_to_byte(rgb[0]);
			q.green = gx_color_value_to_byte(rgb[1]);
			q.blue = gx_color_value_to_byte(rgb[2]);
			fwrite((const char *)&q, sizeof(q), 1, file);
		}
	}

	/* Write the contents of the image. */
	/* BMP files want the image in bottom-to-top order! */

	for ( y = height - 1; y >= 0; y-- )
	{	gdev_prn_copy_scan_lines(pdev, y, row, raster);
		fwrite((const char *)row, bmp_raster, 1, file);
	}

bmp_done:
	gs_free((char *)row, bmp_raster, 1, "bmp file buffer");

	return code;
}

/* 24-bit color mappers (taken from gdevmem2.c). */
/* Note that Windows expects RGB values in the order B,G,R. */

/* Map a r-g-b color to a color index. */
private gx_color_index
map_16m_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{	return gx_color_value_to_byte(r) +
	       ((uint)gx_color_value_to_byte(g) << 8) +
	       ((ulong)gx_color_value_to_byte(b) << 16);
}

/* Map a color index to a r-g-b color. */
private int
map_16m_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	prgb[2] = gx_color_value_from_byte(color >> 16);
	prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
	prgb[0] = gx_color_value_from_byte(color & 0xff);
	return 0;
}
