/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevmiff.c */
/* MIFF file format driver */
#include "gdevprn.h"

/* ------ The device descriptor ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

private dev_proc_print_page(miff24_print_page);

private gx_device_procs miff24_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
    gx_default_rgb_map_rgb_color, gx_default_rgb_map_color_rgb);
gx_device_printer far_data gs_miff24_device =
  prn_device(miff24_procs, "miff24",
	     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	     X_DPI, Y_DPI,
	     0,0,0,0,			/* margins */
	     24, miff24_print_page);

/* Print one page in 24-bit RLE direct color format. */
private int
miff24_print_page(gx_device_printer *pdev, FILE *file)
{	int raster = gx_device_raster((gx_device *)pdev, true);
	byte *line = (byte *)gs_malloc(raster, 1, "miff line buffer");
	int y;
	int code = 0;			/* return code */

	if ( line == 0 )		/* can't allocate line buffer */
	  return_error(gs_error_VMerror);
	fputs("id=ImageMagick\n", file);
	fputs("class=DirectClass\n", file);
	fprintf(file, "columns=%d\n", pdev->width);
	fputs("compression=RunlengthEncoded\n", file);
	fprintf(file, "rows=%d\n", pdev->height);
	fputs(":\n", file);
	for ( y = 0; y < pdev->height; ++y )
	  {	byte *row;
		byte *end;

		code = gdev_prn_get_bits(pdev, y, line, &row);
		if ( code < 0 )
		  break;
		end = row + pdev->width * 3;
		while ( row < end )
		  { int count = 0;
		    while ( count < 255 && row < end - 3 &&
			    row[0] == row[3] && row[1] == row[4] &&
			    row[2] == row[5]
			  )
		      ++count, row += 3;
		    putc(row[0], file);
		    putc(row[1], file);
		    putc(row[2], file);
		    putc(count, file);
		    row += 3;
		  }
	  }
	gs_free((char *)line, lsize, 1, "miff line buffer");

	return code;
}
