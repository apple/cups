/* Copyright (C) 1991, 1994, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevcp50.c */
/* Mitsubishi CP50 color printer driver */
#include "gdevprn.h"
#define ppdev ((gx_device_printer *)pdev)

/***
 *** Note: this driver was contributed by a user.  Please contact
 ***       Michael Hu (michael@ximage.com) if you have questions.
 ***/

/* The value of X_PIXEL and Y_PIXEL is gained by experiment */
#define X_PIXEL   474
#define Y_PIXEL   800

/* The value of FIRST_LINE and LAST_LINE is gained by experiment */
/* Note: LAST-LINE - FIRST_LINE + 1 should close to Y_PIXEL */
#define FIRST_LINE 140
#define LAST_LINE  933

/* The value of FIRST is gained by experiment */
/* There are 60 pixel(RGB) in the right clipped margin */
#define FIRST_COLUMN    180

/* The value of X_DPI and Y_DPI is gained by experiment */
#define X_DPI 154		/* pixels per inch */ 
#define Y_DPI 187		/* pixels per inch */

/* The device descriptor */
private dev_proc_print_page(cp50_print_page);
private dev_proc_output_page(cp50_output_page);

private dev_proc_map_rgb_color(cp50_rgb_color);
private dev_proc_map_color_rgb(cp50_color_rgb);

private gx_device_procs cp50_procs =
  prn_color_procs(gdev_prn_open, cp50_output_page, gdev_prn_close,
    cp50_rgb_color, cp50_color_rgb);

gx_device_printer far_data gs_cp50_device =
  prn_device(cp50_procs, "cp50",
	39,				/* width_10ths, 100mm */
	59,				/* height_10ths,150mm  */
	X_DPI, Y_DPI,
	0.39, 0.91, 0.43, 0.75,		/* margins */
	24, cp50_print_page);

int copies;

/* ------ Internal routines ------ */


/* Send the page to the printer. */
private int
cp50_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	
	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	byte *out = (byte *)gs_malloc(line_size, 1, "cp50_print_page(out)");
    byte *r_plane = (byte *)gs_malloc(X_PIXEL*Y_PIXEL, 1, "cp50_print_page(r_plane)");
    byte *g_plane = (byte *)gs_malloc(X_PIXEL*Y_PIXEL, 1, "cp50_print_page(g_plane)");
    byte *b_plane = (byte *)gs_malloc(X_PIXEL*Y_PIXEL, 1, "cp50_print_page(b_plane)");
    byte *t_plane = (byte *)gs_malloc(X_PIXEL*Y_PIXEL, 1, "cp50_print_page(t_plane)");
	int lnum = FIRST_LINE;
	int last = LAST_LINE;
    int lines = X_PIXEL;
    byte hi_lines, lo_lines;
    byte num_copies;
    int i,j;


/*fprintf(prn_stream, "%d,%d,%d,", pdev->width, pdev->height, line_size);*/

	/* Check allocations */
	if ( out == 0 || r_plane == 0 || g_plane == 0 || b_plane == 0 || 
         t_plane == 0)
	{	if ( out )
			gs_free((char *)out, line_size, 1,
				"cp50_print_page(out)");
        if (r_plane)
            gs_free((char *)r_plane, X_PIXEL*Y_PIXEL, 1,
                "cp50_print_page(r_plane)");
        if (g_plane)  
            gs_free((char *)g_plane, X_PIXEL*Y_PIXEL, 1, 
                "cp50_print_page(g_plane)");
        if (b_plane)  
            gs_free((char *)b_plane, X_PIXEL*Y_PIXEL, 1, 
                "cp50_print_page(b_plane)");
        if (t_plane)
            gs_free((char *)t_plane, X_PIXEL*Y_PIXEL, 1, 
                "cp50_print_page(t_plane)");
		return -1;
	}

    /* set each plane as white */
    memset(r_plane, -1, X_PIXEL*Y_PIXEL);
    memset(g_plane, -1, X_PIXEL*Y_PIXEL);
    memset(b_plane, -1, X_PIXEL*Y_PIXEL);
    memset(t_plane, -1, X_PIXEL*Y_PIXEL);

	/* Initialize the printer */ /* see programmer manual for CP50 */
	fprintf(prn_stream,"\033\101");
    fprintf(prn_stream,"\033\106\010\001");
    fprintf(prn_stream,"\033\106\010\003");

    /* set number of copies */
    fprintf(prn_stream,"\033\116");
    num_copies = copies & 0xFF;
    fwrite(&num_copies, sizeof(char), 1, prn_stream);

    /* download image */
    hi_lines = lines >> 8;
    lo_lines = lines & 0xFF;

    fprintf(prn_stream,"\033\123\062");
    fwrite(&hi_lines, sizeof(char), 1, prn_stream);
    fwrite(&lo_lines, sizeof(char), 1, prn_stream);
    fprintf(prn_stream,"\001"); /* dummy */

	/* Print lines of graphics */
	while ( lnum <= last )
	   {
        int i, col;
		gdev_prn_copy_scan_lines(pdev, lnum, (byte *)out, line_size);
		/*fwrite(out, sizeof(char), line_size, prn_stream);*/
        for(i=0; i<X_PIXEL; i++)
        {
          col = (lnum-FIRST_LINE) * X_PIXEL + i;
          r_plane[col] = out[i*3+FIRST_COLUMN];
          g_plane[col] = out[i*3+1+FIRST_COLUMN];
          b_plane[col] = out[i*3+2+FIRST_COLUMN];
        }
		lnum ++;
	   }

    /* rotate each plane and download it */
    for(i=0;i<X_PIXEL;i++)
      for(j=Y_PIXEL-1;j>=0;j--)
        t_plane[(Y_PIXEL-1-j)+i*Y_PIXEL] = r_plane[i+j*X_PIXEL];
    fwrite(t_plane, sizeof(char), X_PIXEL*Y_PIXEL, prn_stream);

    for(i=0;i<X_PIXEL;i++)
      for(j=Y_PIXEL-1;j>=0;j--)
        t_plane[(Y_PIXEL-1-j)+i*Y_PIXEL] = g_plane[i+j*X_PIXEL]; 
    fwrite(t_plane, sizeof(char), X_PIXEL*Y_PIXEL, prn_stream);

    for(i=0;i<X_PIXEL;i++)
      for(j=Y_PIXEL-1;j>=0;j--)
        t_plane[(Y_PIXEL-1-j)+i*Y_PIXEL] = b_plane[i+j*X_PIXEL]; 
    fwrite(t_plane, sizeof(char), X_PIXEL*Y_PIXEL, prn_stream);


	gs_free((char *)out, line_size, 1, "cp50_print_page(out)");
    gs_free((char *)r_plane, X_PIXEL*Y_PIXEL, 1, "cp50_print_page(r_plane)");
    gs_free((char *)g_plane, X_PIXEL*Y_PIXEL, 1, "cp50_print_page(g_plane)");
    gs_free((char *)b_plane, X_PIXEL*Y_PIXEL, 1, "cp50_print_page(b_plane)");
    gs_free((char *)t_plane, X_PIXEL*Y_PIXEL, 1, "cp50_print_page(t_plane)");

	return 0;
}

int private 
cp50_output_page(gx_device *pdev, int num_copies, int flush)
{   int code, outcode, closecode;

    code = gdev_prn_open_printer(pdev, 1);
    if ( code < 0 ) return code;

    copies = num_copies; /* using global variable to pass */

    /* Print the accumulated page description. */
    outcode = (*ppdev->printer_procs.print_page)(ppdev, ppdev->file);
    if ( code < 0 ) return code;

    closecode = gdev_prn_close_printer(pdev);
    if ( code < 0 ) return code;

    if ( ppdev->buffer_space ) /* reinitialize clist for writing */
      code = (*gs_clist_device_procs.output_page)(pdev, num_copies, flush);
 
    if ( outcode < 0 ) return outcode;
    if ( closecode < 0 ) return closecode;
    return code;
}


/* 24-bit color mappers (taken from gdevmem2.c). */
/* Note that Windows expects RGB values in the order B,G,R. */
 
/* Map a r-g-b color to a color index. */
private gx_color_index
cp50_rgb_color(gx_device *dev, gx_color_value r, gx_color_value g,
  gx_color_value b)
{   return ((ulong)gx_color_value_to_byte(r) << 16)+
           ((uint)gx_color_value_to_byte(g) << 8) +
           gx_color_value_to_byte(b);
}
 
/* Map a color index to a r-g-b color. */
private int
cp50_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{   prgb[2] = gx_color_value_from_byte(color & 0xff);
    prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
    prgb[0] = gx_color_value_from_byte(color >> 16);
    return 0;
}
