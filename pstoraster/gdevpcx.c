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

/* gdevpcx.c */
/* PCX file format drivers */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gxlum.h"

/* Thanks to Phil Conrad for donating the original version */
/* of these drivers to Aladdin Enterprises. */

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

/* Monochrome. */

private dev_proc_print_page(pcxmono_print_page);

/* Use the default RGB->color map, so we get black=0, white=1. */
private gx_device_procs pcxmono_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gx_default_map_rgb_color, gx_default_map_color_rgb);
gx_device_printer far_data gs_pcxmono_device =
  prn_device(pcxmono_procs, "pcxmono",
	     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	     X_DPI, Y_DPI,
	     0,0,0,0,			/* margins */
	     1, pcxmono_print_page);

/* Chunky 8-bit gray scale. */

private dev_proc_print_page(pcx256_print_page);

private gx_device_procs pcxgray_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb);
gx_device_printer far_data gs_pcxgray_device =
{  prn_device_body(gx_device_printer, pcxgray_procs, "pcxgray",
		   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		   X_DPI, Y_DPI,
		   0,0,0,0,			/* margins */
		   1,8,255,0,256,0, pcx256_print_page)
};

/* 4-bit planar (EGA/VGA-style) color. */

private dev_proc_print_page(pcx16_print_page);

private gx_device_procs pcx16_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    pc_4bit_map_rgb_color, pc_4bit_map_color_rgb);
gx_device_printer far_data gs_pcx16_device =
{  prn_device_body(gx_device_printer, pcx16_procs, "pcx16",
		   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		   X_DPI, Y_DPI,
		   0,0,0,0,			/* margins */
		   3,4,3,2,4,3, pcx16_print_page)
};

/* Chunky 8-bit (SuperVGA-style) color. */
/* (Uses a fixed palette of 3,3,2 bits.) */

private gx_device_procs pcx256_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);
gx_device_printer far_data gs_pcx256_device =
{  prn_device_body(gx_device_printer, pcx256_procs, "pcx256",
		   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		   X_DPI, Y_DPI,
		   0,0,0,0,			/* margins */
		   3,8,6,6,7,7, pcx256_print_page)
};

/* 24-bit color, 3 8-bit planes. */

private dev_proc_print_page(pcx24b_print_page);

private gx_device_procs pcx24b_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb);
gx_device_printer far_data gs_pcx24b_device =
  prn_device(pcx24b_procs, "pcx24b",
	     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	     X_DPI, Y_DPI,
	     0,0,0,0,			/* margins */
	     24, pcx24b_print_page);

/* ------ Private definitions ------ */

/* All two-byte quantities are stored LSB-first! */
#if arch_is_big_endian
#  define assign_ushort(a,v) a = ((v) >> 8) + ((v) << 8)
#else
#  define assign_ushort(a,v) a = (v)
#endif

typedef struct pcx_header_s {
	byte 	manuf;		/* always 0x0a */
	byte	version;	/* version info = 0,2,3,5 */
	byte	encoding;	/* 1=RLE */
	byte	bpp;		/* bits per pixel per plane */
	ushort	x1;		/* X of upper left corner */
	ushort	y1;		/* Y of upper left corner */
	ushort	x2;		/* x1 + width - 1 */
	ushort	y2;		/* y1 + height - 1 */
	ushort	hres;		/* horz. resolution (dots per inch) */
	ushort	vres;		/* vert. resolution (dots per inch) */
	byte	palette[16*3];	/* color palette */
	byte	reserved;
	byte	nplanes;	/* number of color planes */
	ushort	bpl;		/* number of bytes per line (uncompressed) */
	ushort	palinfo;	/* palette info 1=color, 2=grey */
	byte	xtra[58];	/* fill out header to 128 bytes */
} pcx_header;
/* Define the prototype header. */
private const pcx_header far_data pcx_header_prototype = {
	10,			/* manuf */
	5,			/* version */
	1,			/* encoding */
	0,			/* bpp (variable) */
	00, 00,			/* x1, y1 */
	00, 00,			/* x2, y2 (variable) */
	00, 00,			/* hres, vres (variable) */
	  {0,0,0, 0,0,0, 0,0,0, 0,0,0,	/* palette (variable) */
	   0,0,0, 0,0,0, 0,0,0, 0,0,0,
	   0,0,0, 0,0,0, 0,0,0, 0,0,0,
	   0,0,0, 0,0,0, 0,0,0, 0,0,0},
	0,			/* reserved */
	0,			/* nplanes (variable) */
	00,			/* bpl (variable) */
	00,			/* palinfo (variable) */
	  {            0,0, 0,0,0,0,0,0,0,0,	/* xtra */
	   0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	   0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
	   0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0}
};

/* 
** version info for PCX is as follows 
**
** 0 == 2.5
** 2 == 2.8 w/palette info
** 3 == 2.8 without palette info
** 5 == 3.0 (includes palette)
**
*/

/*
 * Define the DCX header.  We don't actually use this yet.
 * All quantities are stored little-endian!
	bytes 0-3: ID = 987654321
	bytes 4-7: file offset of page 1
	[... up to 1023 entries ...]
	bytes N-N+3: 0 to mark end of page list
 * This is followed by the pages in order, each of which is a PCX file.
 */
#define dcx_magic 987654321
#define dcx_max_pages 1023

/* Forward declarations */
private void pcx_write_rle(P4(const byte *, const byte *, int, FILE *));
private int pcx_write_page(P4(gx_device_printer *, FILE *, pcx_header _ss *, bool));

/* Write a monochrome PCX page. */
private int
pcxmono_print_page(gx_device_printer *pdev, FILE *file)
{	pcx_header header;

	header = pcx_header_prototype;
	header.version = 2;
	header.bpp = 1;
	header.nplanes = 1;
	/* Set the first two entries of the short palette. */
	memcpy((byte *)header.palette, "\000\000\000\377\377\377", 6);
	return pcx_write_page(pdev, file, &header, false);
}

/* Write an "old" PCX page. */
static const byte pcx_ega_palette[16*3] = {
  0x00,0x00,0x00,  0x00,0x00,0xaa,  0x00,0xaa,0x00,  0x00,0xaa,0xaa,
  0xaa,0x00,0x00,  0xaa,0x00,0xaa,  0xaa,0xaa,0x00,  0xaa,0xaa,0xaa,
  0x55,0x55,0x55,  0x55,0x55,0xff,  0x55,0xff,0x55,  0x55,0xff,0xff,
  0xff,0x55,0x55,  0xff,0x55,0xff,  0xff,0xff,0x55,  0xff,0xff,0xff
};
private int
pcx16_print_page(gx_device_printer *pdev, FILE *file)
{	pcx_header header;

	header = pcx_header_prototype;
	header.version = 2;
	header.bpp = 1;
	header.nplanes = 4;
	/* Fill the EGA palette appropriately. */
	memcpy((byte *)header.palette, pcx_ega_palette,
	       sizeof(pcx_ega_palette));
	return pcx_write_page(pdev, file, &header, true);
}

/* Write a "new" PCX page. */
private int
pcx256_print_page(gx_device_printer *pdev, FILE *file)
{	pcx_header header;
	int code;

	header = pcx_header_prototype;
	header.bpp = 8;
	header.nplanes = 1;
	code = pcx_write_page(pdev, file, &header, false);
	if ( code >= 0 )
	{	/* Write out the palette. */
		fputc(0x0c, file);
		code = pc_write_palette((gx_device *)pdev, 256, file);
	}
	return code;
}

/* Write a 24-bit color PCX page. */
private int
pcx24b_print_page(gx_device_printer *pdev, FILE *file)
{	pcx_header header;

	header = pcx_header_prototype;
	header.bpp = 8;
	header.nplanes = 3;
	return pcx_write_page(pdev, file, &header, true);
}

/* Write out a page in PCX format. */
/* This routine is used for all formats. */
/* The caller has set header->bpp, nplanes, and palette. */
private int
pcx_write_page(gx_device_printer *pdev, FILE *file, pcx_header _ss *phdr,
  bool planar)
{	int raster = gdev_prn_raster(pdev);
	uint rsize = round_up((pdev->width * phdr->bpp + 7) >> 3, 2);	/* PCX format requires even */
	int height = pdev->height;
	int depth = pdev->color_info.depth;
	uint lsize = raster + rsize;
	byte *line = (byte *)gs_malloc(lsize, 1, "pcx file buffer");
	byte *plane = line + raster;
	int y;
	int code = 0;			/* return code */
	if ( line == 0 )		/* can't allocate line buffer */
	  return_error(gs_error_VMerror);

	/* Fill in the variable entries in the header struct. */

	assign_ushort(phdr->x2, pdev->width-1);
	assign_ushort(phdr->y2, height-1);
	assign_ushort(phdr->hres, (int)pdev->x_pixels_per_inch);
	assign_ushort(phdr->vres, (int)pdev->y_pixels_per_inch);
	assign_ushort(phdr->bpl, (planar || depth == 1 ? rsize :
				  raster + (raster & 1)));
	assign_ushort(phdr->palinfo, (depth > 1 ? 1 : 2));

	/* Write the header. */

	if ( fwrite((const char *)phdr, 1, 128, file) < 128 )
	{	code = gs_error_ioerror;
		goto pcx_done;
	}

	/* Write the contents of the image. */
	for ( y = 0; y < height; y++ )
	{	byte *row;
		byte *end;

		code = gdev_prn_get_bits(pdev, y, line, &row);
		if ( code < 0 ) break;
		end = row + raster;
		if ( !planar )
		{	/* Just write the bits. */
			if ( raster & 1 )
			{	/* Round to even, with predictable padding. */
				*end = end[-1];
				++end;
			}
			pcx_write_rle(row, end, 1, file);
		}
		else
		  switch ( depth )
		{

		case 4:
		{	byte *pend = plane + rsize;
			int shift;

			for ( shift = 0; shift < 4; shift++ )
			{	register byte *from, *to;
				register int bright = 1 << shift;
				register int bleft = bright << 4;

				for ( from = row, to = plane;
				      from < end; from += 4
				    )
				{	*to++ =
					  (from[0] & bleft ? 0x80 : 0) |
					  (from[0] & bright ? 0x40 : 0) |
					  (from[1] & bleft ? 0x20 : 0) |
					  (from[1] & bright ? 0x10 : 0) |
					  (from[2] & bleft ? 0x08 : 0) |
					  (from[2] & bright ? 0x04 : 0) |
					  (from[3] & bleft ? 0x02 : 0) |
					  (from[3] & bright ? 0x01 : 0);
				}
				/* We might be one byte short of rsize. */
				if ( to < pend )
				  *to = to[-1];
				pcx_write_rle(plane, pend, 1, file);
			}
		}
			break;

		case 24:
		{	int pnum;
			for ( pnum = 0; pnum < 3; ++pnum )
			  { pcx_write_rle(row + pnum, row + raster, 3, file);
			    if ( pdev->width & 1 )
			      fputc(0, file);		/* pad to even */
			  }
		}
			break;

		default:
			  code = gs_note_error(gs_error_rangecheck);
			  goto pcx_done;

		}
	}

pcx_done:
	gs_free((char *)line, lsize, 1, "pcx file buffer");

	return code;
}

/* ------ Internal routines ------ */

/* Write one line in PCX run-length-encoded format. */
private void
pcx_write_rle(const byte *from, const byte *end, int step, FILE *file)
{	int max_run = step * 63;
	while ( from < end )
	{	byte data = *from;
		from += step;
		if ( data != *from || from == end )
		  {	if ( data >= 0xc0 )
			  putc(0xc1, file);
		  }
		else
		  {	const byte *start = from;
			while ( (from < end) && (*from == data) )
			  from += step;
			/* Now (from - start) / step + 1 is the run length. */
			while ( from - start >= max_run )
			  { putc(0xff, file);
			    putc(data, file);
			    start += max_run;
			  }
			if ( from > start || data >= 0xc0 )
			  putc((from - start) / step + 0xc1, file);
		  }
		putc(data, file);
	}
}
