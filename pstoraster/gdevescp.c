/* Copyright (C) 1993, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevescp.c */
/*
 * Epson 'ESC/P 2' language printer driver.
 *
 * This driver uses the ESC/P2 language raster graphics commands with
 * compression. The driver skips vertical white space, provided that
 * the white space is >= 24/band_size (<~ 1.7mm @ 360dpi!) high. There
 * is no attempt to skip horizontal white space, but the compression
 * greatly reduces the significance of this (a nearly blank line would
 * take about 45 bytes). The driver compresses the data one scan line at
 * a time, even though this is not enforced by the hardware. The reason
 * I have done this is that, since the driver skips data outside the
 * margins, we would have to set up a extra pointers to keep track of
 * the data from the previous scan line. Doing this would add extra
 * complexity at a small saving of disk space.
 *
 * These are the only possible optimisations that remain, and would
 * greatly increase the complexity of the driver. At this point, I don't
 * consider them necessary, but I might consider implementing them if
 * enough people encourage me to do so.
 *
 * Richard Brown (rab@tauon.ph.unimelb.edu.au)
 *
 */

#include "gdevprn.h"

/*
 * Valid values for X_DPI and Y_DPI: 180, 360
 *
 * The value specified at compile time is the default value used if the
 * user does not specify a resolution at runtime.
 */
#ifndef X_DPI
#  define X_DPI 360
#endif

#ifndef Y_DPI
#  define Y_DPI 360
#endif

/*
 * Margin definitions: Stylus 800 printer driver:
 *
 * The commented margins are from the User's Manual.
 *
 * The values actually used here are more accurate for my printer.
 * The Stylus paper handling is quite sensitive to these settings.
 * If you find that the printer uses an extra page after every real
 * page, you'll need to increase the top and/or bottom margin.
 */

#define STYLUS_L_MARGIN 0.13	/*0.12*/
#define STYLUS_B_MARGIN 0.56	/*0.51*/
#define STYLUS_T_MARGIN 0.34	/*0.12*/
#ifdef A4
#   define STYLUS_R_MARGIN 0.18 /*0.15*/
#else
#   define STYLUS_R_MARGIN 0.38
#endif

/*
 * Epson AP3250 Margins:
 */

#define AP3250_L_MARGIN 0.18
#define AP3250_B_MARGIN 0.51
#define AP3250_T_MARGIN 0.34
#define AP3250_R_MARGIN 0.28  /* US paper */

/* The device descriptor */
private dev_proc_print_page(escp2_print_page);

/* Stylus 800 device */
gx_device_printer far_data gs_st800_device =
  prn_device(prn_std_procs, "st800",
	DEFAULT_WIDTH_10THS,
	DEFAULT_HEIGHT_10THS,			
	X_DPI, Y_DPI,
	STYLUS_L_MARGIN, STYLUS_B_MARGIN, STYLUS_R_MARGIN, STYLUS_T_MARGIN,
	1, escp2_print_page);

/* AP3250 device */
gx_device_printer far_data gs_ap3250_device =
  prn_device(prn_std_procs, "ap3250",
	DEFAULT_WIDTH_10THS,
	DEFAULT_HEIGHT_10THS,			
	X_DPI, Y_DPI,
	AP3250_L_MARGIN, AP3250_B_MARGIN, AP3250_R_MARGIN, AP3250_T_MARGIN,
	1, escp2_print_page);

/* ------ Internal routines ------ */

/* Send the page to the printer. */
private int
escp2_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	
	int line_size = gdev_prn_raster((gx_device_printer *)pdev);
	int band_size = 24;	/* 1, 8, or 24 */
	int in_size = line_size * band_size;

	byte *buf1 = (byte *)gs_malloc(in_size, 1, "escp2_print_page(buf1)");
	byte *buf2 = (byte *)gs_malloc(in_size, 1, "escp2_print_page(buf2)");
	byte *in = buf1;
	byte *out = buf2;

	int skip, lnum, top, bottom, left, width;
	int auto_feed = 1;
	int count, i;

	/*
	** Check for valid resolution:
	**
	**	XDPI	YDPI
	**	360	360
	**	360	180
	**	180	180
	*/

	if( !( (pdev->x_pixels_per_inch == 180 &&
	        pdev->y_pixels_per_inch == 180) ||
	       (pdev->x_pixels_per_inch == 360 &&
	       (pdev->y_pixels_per_inch == 360 ||
	        pdev->y_pixels_per_inch == 180) )) )
		   return_error(gs_error_rangecheck);

	/*
	** Check buffer allocations:
	*/

	if ( buf1 == 0 || buf2 == 0 )
	{	if ( buf1 ) 
		  gs_free((char *)buf1, in_size, 1, "escp2_print_page(buf1)");
		if ( buf2 ) 
		  gs_free((char *)buf2, in_size, 1, "escp2_print_page(buf2)");
		return_error(gs_error_VMerror);
	}

	/*
	** Reset printer, enter graphics mode:
	*/

	fwrite("\033@\033(G\001\000\001", 1, 8, prn_stream);

#ifdef A4
	/*
	** After reset, the Stylus is set up for US letter paper.
	** We need to set the page size appropriately for A4 paper.
	** For some bizarre reason the ESC/P2 language wants the bottom
	** margin measured from the *top* of the page:
	*/

	fwrite("\033(U\001\0\n\033(C\002\0t\020\033(c\004\0\0\0t\020",
	                                                1, 22, prn_stream);
#endif

	/*
	** Set the line spacing to match the band height:
	*/

	if( pdev->y_pixels_per_inch == 360 )
	   fwrite("\033(U\001\0\012\033+\030", 1, 9, prn_stream);
	else
	   fwrite("\033(U\001\0\024\033+\060", 1, 9, prn_stream);

        /*
        ** If the printer has automatic page feeding, then the paper
        ** will already be positioned at the top margin value, so we
        ** start printing the image from there. Similarly, we must not
        ** try to print or even line feed past the bottom margin, since
        ** the printer will automatically load a new page.
        ** Printers without this feature may actually need to be told
        ** to skip past the top margin.
        */

        if( auto_feed ) {
           top = dev_t_margin(pdev) * pdev->y_pixels_per_inch;
           bottom = pdev->height - dev_b_margin(pdev) * pdev->y_pixels_per_inch;
        } else {
           top = 0;
           bottom = pdev->height;
        }

        /*
        ** Make left margin and width sit on byte boundaries:
        */

        left  = ( (int) (dev_l_margin(pdev) * pdev->x_pixels_per_inch) ) >> 3;

        width = ((pdev->width - (int)(dev_r_margin(pdev) * pdev->x_pixels_per_inch)) >> 3) - left;

	/*
	** Print the page:
	*/

	for ( lnum = top, skip = 0 ; lnum < bottom ; )
	{	
		byte *in_data;
		byte *inp;
		byte *in_end;
		byte *outp;
		register byte *p, *q;
		int lcnt;

		/*
		** Check buffer for 0 data. We can't do this mid-band
		*/

		gdev_prn_get_bits(pdev, lnum, in, &in_data);
		while ( in_data[0] == 0 &&
		        !memcmp((char *)in_data, (char *)in_data + 1, line_size - 1) &&
		        lnum < bottom )
	    	{	
			lnum++;
			skip++;
			gdev_prn_get_bits(pdev, lnum, in, &in_data);
		}

		if(lnum == bottom ) break;	/* finished with this page */

		/*
		** Skip blank lines if we need to:
		*/

		if( skip ) {
		   fwrite("\033(v\002\000", 1, 5, prn_stream);
		   fputc(skip & 0xff, prn_stream);
		   fputc(skip >> 8,   prn_stream);
		   skip = 0;
		}

		lcnt = gdev_prn_copy_scan_lines(pdev, lnum, in, in_size);

		/*
		** Check to see if we don't have enough data to fill an entire
		** band. Padding here seems to work (the printer doesn't jump
		** to the next (blank) page), although the ideal behaviour
		** would probably be to reduce the band height.
		**
		** Pad with nulls:
		*/

		if( lcnt < band_size )
		   memset(in + lcnt * line_size, 0, in_size - lcnt * line_size);

		/*
		** Now we have a band of data: try to compress it:
		*/

		for( outp = out, i = 0 ; i < band_size ; i++ ) {

		   /*
		   ** Take margins into account:
		   */

		   inp = in + i * line_size + left;
		   in_end = inp + width;

		   /*
		   ** walk through input buffer, looking for repeated data:
		   ** Since we need more than 2 repeats to make the compression
		   ** worth it, we can compare pairs, since it doesn't matter if we
		   **
		   */

		   for( p = inp, q = inp + 1 ; q < in_end ; ) {

		   	if( *p != *q ) {

		   	   p += 2;
		   	   q += 2;

		   	} else {

		   	   /*
		   	   ** Check behind us, just in case:
		   	   */

		   	   if( p > inp && *p == *(p-1) )
		   	      p--;

			   /*
			   ** walk forward, looking for matches:
			   */

			   for( q++ ; *q == *p && q < in_end ; q++ ) {
			      if( (q-p) >= 128 ) {
			         if( p > inp ) {
			            count = p - inp;
			            while( count > 128 ) {
				       *outp++ = '\177';
				       memcpy(outp, inp, 128);	/* data */
				       inp += 128;
				       outp += 128;
				       count -= 128;
			            }
			            *outp++ = (char) (count - 1); /* count */
			            memcpy(outp, inp, count);	/* data */
			            outp += count;
			         }
				 *outp++ = '\201';	/* Repeat 128 times */
				 *outp++ = *p;
			         p += 128;
			         inp = p;
			      }
			   }

			   if( (q - p) > 2 ) {	/* output this sequence */
			      if( p > inp ) {
				 count = p - inp;
				 while( count > 128 ) {
				    *outp++ = '\177';
				    memcpy(outp, inp, 128);	/* data */
				    inp += 128;
				    outp += 128;
				    count -= 128;
				 }
				 *outp++ = (char) (count - 1);	/* byte count */
				 memcpy(outp, inp, count);	/* data */
				 outp += count;
			      }
			      count = q - p;
			      *outp++ = (char) (256 - count + 1);
			      *outp++ = *p;
			      p += count;
			      inp = p;
			   } else	/* add to non-repeating data list */
			      p = q;
			   if( q < in_end )
			      q++;
		   	}
		   }

		   /*
		   ** copy remaining part of line:
		   */

		   if( inp < in_end ) {

		      count = in_end - inp;

		      /*
		      ** If we've had a long run of varying data followed by a
		      ** sequence of repeated data and then hit the end of line,
		      ** it's possible to get data counts > 128.
		      */

		      while( count > 128 ) {
			*outp++ = '\177';
			memcpy(outp, inp, 128);	/* data */
			inp += 128;
			outp += 128;
			count -= 128;
		      }

		      *outp++ = (char) (count - 1);	/* byte count */
		      memcpy(outp, inp, count);	/* data */
		      outp += count;
		   }
		}

		/*
		** Output data:
		*/

	        fwrite("\033.\001", 1, 3, prn_stream);

	        if(pdev->y_pixels_per_inch == 360)
	           fputc('\012', prn_stream);
		else
	           fputc('\024', prn_stream);

	        if(pdev->x_pixels_per_inch == 360)
	           fputc('\012', prn_stream);
		else
	           fputc('\024', prn_stream);

		fputc(band_size, prn_stream);

	        fputc((width << 3) & 0xff, prn_stream);
		fputc( width >> 5,         prn_stream);

	        fwrite(out, 1, (outp - out), prn_stream);

        	fwrite("\r\n", 1, 2, prn_stream);
		lnum += band_size;
	}

	/* Eject the page and reinitialize the printer */

	fputs("\f\033@", prn_stream);
	fflush(prn_stream);

	gs_free((char *)buf2, in_size, 1, "escp2_print_page(buf2)");
	gs_free((char *)buf1, in_size, 1, "escp2_print_page(buf1)");
	return 0;
}
