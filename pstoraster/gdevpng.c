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

/* gdevpng.c */
/* PNG (Portable Network Graphics) Format.  Pronounced "ping". */
/* lpd 1996-6-24: Added #ifdef for compatibility with old libpng versions. */
/* lpd 1996-6-11: Edited to remove unnecessary color mapping code. */
/* lpd (L. Peter Deutsch) 1996-4-7: Modified for libpng 0.88. */
/* Original version by Russell Lang 1995-07-04 */

#include "gdevprn.h"
#include "gdevpccm.h"
#include "gscdefs.h"

#define PNG_INTERNAL
#include "png.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

private dev_proc_print_page(png_print_page);

/* Monochrome. */

gx_device_printer far_data gs_pngmono_device =
  prn_device(prn_std_procs, "pngmono",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	1, png_print_page);

/* 4-bit planar (EGA/VGA-style) color. */

private gx_device_procs png16_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    pc_4bit_map_rgb_color, pc_4bit_map_color_rgb);
gx_device_printer far_data gs_png16_device =
  prn_device(png16_procs, "png16",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	4, png_print_page);

/* 8-bit (SuperVGA-style) color. */
/* (Uses a fixed palette of 3,3,2 bits.) */

private gx_device_procs png256_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);
gx_device_printer far_data gs_png256_device =
  prn_device(png256_procs, "png256",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	8, png_print_page);

/* 8-bit gray */

private gx_device_procs pnggray_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gx_default_gray_map_rgb_color, gx_default_gray_map_color_rgb);
gx_device_printer far_data gs_pnggray_device =
{ prn_device_body(gx_device_printer, pnggray_procs, "pnggray",
		   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
		   X_DPI, Y_DPI,
		   0,0,0,0,			/* margins */
		   1,8,255,0,256,0, png_print_page)
};

/* 24-bit color. */

private gx_device_procs png16m_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb);
gx_device_printer far_data gs_png16m_device =
  prn_device(png16m_procs, "png16m",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0,0,0,0,			/* margins */
	24, png_print_page);


/* ------ Private definitions ------ */

/* Write out a page in PNG format. */
/* This routine is used for all formats. */
private int
png_print_page(gx_device_printer *pdev, FILE *file)
{	int raster = gdev_prn_raster(pdev);
	/* PNG structures */
	png_struct *png_ptr;
	png_info *info_ptr;
	byte *row = (byte *)gs_malloc(raster, 1, "png raster buffer");
	int height = pdev->height;
	int depth = pdev->color_info.depth;
	int y;
	int code = 0;			/* return code */
	const char *software_key = "Software";
	char software_text[256];
	png_text text_png;

	if ( row == 0 )			/* can't allocate row buffer */
	  return_error(gs_error_VMerror);

	/* allocate the necessary structures */
	png_ptr = gs_malloc(sizeof (png_struct), 1, "png structure");
	if (!png_ptr)
	  return_error(gs_error_VMerror);

	info_ptr = gs_malloc(sizeof (png_info), 1, "png info_ptr");
	if (!info_ptr)
	  {
		gs_free((char *)row, raster, 1, "png raster buffer");
		gs_free((char *)png_ptr, sizeof (png_struct), 1, "png structure");
		return_error(gs_error_VMerror);
	  }

	/* set error handling */
	if (setjmp(png_ptr->jmpbuf))
	{
		png_write_destroy(png_ptr);
		gs_free((char *)row, raster, 1, "png raster buffer");
		gs_free((char *)png_ptr, sizeof (png_struct), 1, "png structure");
		gs_free((char *)info_ptr, sizeof (png_info), 1, "png info_ptr");
		/* If we get here, we had a problem reading the file */
		return_error(gs_error_VMerror);
	}

	/* initialize the structures */
	png_info_init(info_ptr);
	png_write_init(png_ptr);

	/* set up the output control */
	png_init_io(png_ptr, file);

	/* set the file information here */
	info_ptr->width = pdev->width;
	info_ptr->height = pdev->height;
	switch(depth) {
	    case 24:
	        info_ptr->bit_depth = 8;
		info_ptr->color_type = PNG_COLOR_TYPE_RGB;
		break;
	    case 8:
		info_ptr->bit_depth = 8;
		if (gx_device_has_color(pdev))
		    info_ptr->color_type = PNG_COLOR_TYPE_PALETTE;
		else
		    info_ptr->color_type = PNG_COLOR_TYPE_GRAY;
		break;
	    case 4:
		info_ptr->bit_depth = 4;
		info_ptr->color_type = PNG_COLOR_TYPE_PALETTE;
		break;
	    case 1:
		info_ptr->bit_depth = 1;
		info_ptr->color_type = PNG_COLOR_TYPE_GRAY;
		/* invert monocrome pixels */
   		png_set_invert_mono(png_ptr);
		break;
	}

	/* set the palette if there is one */
	if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
	    int i;
	    int num_colors = 1<<depth;
	    gx_color_value rgb[3];
	    info_ptr->valid |= PNG_INFO_PLTE;
	    info_ptr->palette = gs_malloc(256 * sizeof (png_color), 1, "png palette");
	    info_ptr->num_palette = num_colors;
	    for (i=0; i<num_colors; i++) {
		(*dev_proc(pdev, map_color_rgb))((gx_device *)pdev,
				(gx_color_index)i, rgb);
		info_ptr->palette[i].red = gx_color_value_to_byte(rgb[0]);
		info_ptr->palette[i].green = gx_color_value_to_byte(rgb[1]);
		info_ptr->palette[i].blue = gx_color_value_to_byte(rgb[2]);
	    }
	}

	/* add comment */
	sprintf(software_text, "%s %d.%02d", gs_product, 
		(int)(gs_revision / 100), (int)(gs_revision % 100));
	text_png.compression = -1;	/* uncompressed */
	text_png.key = (char *)software_key;	/* not const, unfortunately */
	text_png.text = software_text;
	text_png.text_length = strlen(software_text);
	info_ptr->text = &text_png;
	info_ptr->num_text = 1;

	/* write the file information */
	png_write_info(png_ptr, info_ptr);

	/* don't write the comments twice */
	info_ptr->num_text = 0;
	info_ptr->text = NULL;

	/* Write the contents of the image. */
	for ( y = 0; y < height; y++ )
	{	gdev_prn_copy_scan_lines(pdev, y, row, raster);
         	png_write_rows(png_ptr, &row, 1);
	}

	/* write the rest of the file */
	png_write_end(png_ptr, info_ptr);

	/* clean up after the write, and free any memory allocated */
	png_write_destroy(png_ptr);

	/* if you malloced the palette, free it here */
	if (info_ptr->palette)
	     gs_free(info_ptr->palette, 256 * sizeof (png_color), 1, "png palette");

	/* free the structures */
	gs_free((char *)row, raster, 1, "png raster buffer");
	gs_free((char *)png_ptr, sizeof (png_struct), 1, "png structure");
	gs_free((char *)info_ptr, sizeof (png_info), 1, "png info_ptr");

	return code;
}

/*
 * Patch around a static reference to a never-used procedure.
 * This could be avoided if we were willing to edit pngconf.h to
 *	#undef PNG_PROGRESSIVE_READ_SUPPORTED
 */
#ifdef PNG_PROGRESSIVE_READ_SUPPORTED
void
png_push_fill_buffer(png_structp png_ptr, png_bytep buffer,
   png_uint_32 length)
{
}
#endif
