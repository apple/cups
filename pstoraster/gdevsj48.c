/* Copyright (C) 1990, 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
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

/*
 * gdevsj48.c --- derived from gdevbj10.c 1993-10-07
 *                by Mats kerblom (f86ma@dd.chalmers.se).
 * 
 *
 * StarJet SJ48 printer driver.
 *
 */
 
#include "gdevprn.h"


/*
 * The only available resolutions (in the program) are (180,360)x(180,360).
 *
 * Used control codes:
 *   <Esc>@	Printer reset
 *   <Esc>J<n>	Make a n/180 inch linefeed
 *   <Esc>\<a><b>	Move the print position (a+256b)/180 inch to the right
 *   <Esc>*<m><a><b>... Print graphics; m=39: 180*180 dpi
 *					m=40: 360*180 dpi
 *					m=71: 180*360 dpi
 *					m=72: 360*360 dpi
 *			a+256b columns is printed.
 */

/* The device descriptor */
private dev_proc_print_page(sj48_print_page);
gx_device_printer far_data gs_sj48_device =
  prn_device(prn_std_procs, "sj48",
	80,				/* width_10ths, 8" */
	105,				/* height_10ths, 10.5" */
	360,				/* x_dpi */
	360,				/* y_dpi */
	0,0,0,0,			/* margins */
	1, sj48_print_page);


/*   This comes from the bj10/bj200 source. I don't know how it applies
 *   for a StarJet.  --- Mats kerblom.
 *
 *
 * The following is taken from the BJ200 Programmer's manual.  The top
 * margin is 3mm (0.12"), and the bottom margin is 6.4mm (0.25").  The
 * left and right margin depend on the type of paper -- US letter or
 * A4 -- but ultimately rest on a print width of 203.2mm (8").  For letter
 * paper, the left margin (and hence the right) is 6.4mm (0.25"), while
 * for A4 paper, both are 3.4mm (0.13").
 *
 * The bottom margin requires a bit of care.  The image is printed
 * as strips, each about 3.4mm wide.  We can only attain the bottom 
 * margin if the final strip coincides with it.  Note that each strip
 * is generated using only 48 of the available 64 jets, and the absence
 * of those bottom 16 jets makes our bottom margin, in effect, about
 * 1.1mm (0.04") larger.
 *
 * The bj200 behaves, in effect, as though the origin were at the first
 * printable position, rather than the top left corner of the page, so
 * we add a translation to the initial matrix to compensate for this.
 *
 * Except for the details of getting the margins correct, the bj200 is
 * no different from the bj10e, and uses the same routine to print each
 * page.
 *
 */


/* Send the page to the printer. */
private int
sj48_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	int line_size = gx_device_raster((gx_device *)pdev, 0);
	int xres = pdev->x_pixels_per_inch;
	int yres = pdev->y_pixels_per_inch;
	int mode = (yres == 180 ?
			(xres == 180 ? 39 : 40) :
			(xres == 180 ? 71 : 72));
	int bytes_per_column = (yres == 180) ? 3 : 6;
	int bits_per_column = bytes_per_column * 8;
	int skip_unit = bytes_per_column * (xres == 180 ? 1 : 2); /* Skips in step of 1/180" */
	byte *in = (byte *)gs_malloc(8, line_size, "sj48_print_page(in)");
	byte *out = (byte *)gs_malloc(bits_per_column, line_size, "sj48_print_page(out)");
	int lnum = 0;
	int skip = 0;
	int skips;
	int code = 0;
	int last_row = dev_print_scan_lines(pdev);
	int limit = last_row - bits_per_column;

	if ( in == 0 || out == 0 )
	{	code = gs_error_VMerror;
		gs_note_error(code);
		goto fin;
	}

	/* Abort if the requested resolution is unsupported. */
	if ((xres !=180 && xres != 360) || (yres !=180 && yres != 360))
	{	code = gs_error_rangecheck;
		gs_note_error(code);
		goto fin;
	}

	/* Initialize the printer. */
	fwrite("\033@\000\000", 1, 4, prn_stream);  /* <Printer reset>, <0>, <0>. */

	/* Transfer pixels to printer.  The last row we can print is defined
	   by "last_row".  Only the bottom of the print head can print at the
	   bottom margin, and so we align the final printing pass.  The print
	   head is kept from moving below "limit", which is exactly one pass
	   above the bottom margin.  Once it reaches this limit, we make our
	   final printing pass of a full "bits_per_column" rows. */
	while ( lnum < last_row )
	   {	
		byte *in_data;
		byte *in_end = in + line_size;
		byte *out_beg = out;
		byte *out_end = out + bytes_per_column * pdev->width;
		byte *outl = out;
		int count, bnum;

		/* Copy 1 scan line and test for all zero. */
		code = gdev_prn_get_bits(pdev, lnum, in, &in_data);
		if ( code < 0 ) goto xit;
		/* The mem... or str... functions should be faster than */
		/* the following code, but all systems seem to implement */
		/* them so badly that this code is faster. */
		   {	register const long *zip = (const long *)in_data;
			register int zcnt = line_size;
			register const byte *zipb;
			for ( ; zcnt >= 4 * sizeof(long); zip += 4, zcnt -= 4 * sizeof(long) )
			   {	if ( zip[0] | zip[1] | zip[2] | zip[3] )
					goto notz;
			   }
			zipb = (const byte *)zip;
			while ( --zcnt >= 0 )
			   {
				if ( *zipb++ )
					goto notz;
			   }
			/* Line is all zero, skip */
			lnum++;
			skip++;
			continue;
notz:			;
		   }

		/* Vertical tab to the appropriate position.  Note here that
		   we make sure we don't move below limit. */
		if ( lnum > limit )
		    {	skip -= (limit - lnum);
			lnum = limit;
		    }

		/* The SJ48 can only skip in steps of 1/180" */
		if (yres == 180) {
		  skips = skip;
		} else {
		  if (skip & 1) {
		    skip--; /* Makes skip even. */
		    lnum--;
		  }
                  skips = skip/2;
		} 
		    
		while ( skips > 255 )
		   {	fputs("\033J\377", prn_stream);
			skips -= 255;
		   }
		if ( skips )
			fprintf(prn_stream, "\033J%c", skips);

		/* If we've printed as far as "limit", then reset "limit"
		   to "last_row" for the final printing pass. */
		if ( lnum == limit )
			limit = last_row;
		skip = 0;

		/* Transpose in blocks of 8 scan lines. */
		for ( bnum = 0; bnum < bits_per_column; bnum += 8 )
		   {	int lcnt = min(8, limit - lnum);
			byte *inp = in;
			byte *outp = outl;
		   	lcnt = gdev_prn_copy_scan_lines(pdev,
				lnum, in, lcnt * line_size);
			if ( lcnt < 0 )
			   {	code = lcnt;
				goto xit;
			   }
			if ( lcnt < 8 )
				memset(in + lcnt * line_size, 0,
				       (8 - lcnt) * line_size);
			for ( ; inp < in_end; inp++, outp += bits_per_column )
			   {	gdev_prn_transpose_8x8(inp, line_size,
					outp, bytes_per_column);
			   }
			outl++;
			lnum += lcnt;
			skip += lcnt;
		   }

		/* Send the bits to the printer.  We alternate horizontal
		   skips with the data.  The horizontal skips are in units
		   of 1/180 inches, so we look at the data in groups of
		   1 or 2 columns depending on resolution (controlled
                   by skip_unit).  */
		outl = out;
		do
		   {	int count;
			int n;
			byte *out_ptr;

			/* First look for blank groups of columns. */
			while(outl < out_end)
			   {	n = count = min(out_end - outl, skip_unit);
				out_ptr = outl;
				while ( --count >= 0 )
				   {	if ( *out_ptr++ )
						break;
				   }
				if ( count >= 0 )
					break;
				else
					outl = out_ptr;
			   }
			if (outl >= out_end)
				break;
			if (outl > out_beg)
			   {	count = (outl - out_beg) / skip_unit;
				fprintf(prn_stream, "\033\\%c%c",
					count & 0xff, count >> 8);
			   }

			/* Next look for non-blank groups of columns. */
			out_beg = outl;
			outl += n;
			while(outl < out_end)
			   {	n = count = min(out_end - outl, skip_unit);
				out_ptr = outl;
				while ( --count >= 0 )
				   {	if ( *out_ptr++ )
						break;
				   }
				if ( count < 0 )
					break;
				else
					outl += n;
			   }
			count = outl - out_beg;
			{
			  /* What to transmit is the number of columns in the row.
			     Compare this with the <Esc>|*-command wich expects the
			     total number of bytes in the graphic row! */
			  int count1 = count/bytes_per_column;
			  fprintf(prn_stream, "\033*%c%c%c",
				  mode, count1 & 0xff, count1 >> 8);
			}
			fwrite(out_beg, 1, count, prn_stream);
			out_beg = outl;
			outl += n;
		   }
		while ( out_beg < out_end );

		fputc('\r', prn_stream);
		skip = bits_per_column;  /* <CR> only moves to the beginning of the row. */
	   }

	/* Eject the page */
xit:	fputc(014, prn_stream);	/* form feed */
	fflush(prn_stream);
fin:	if ( out != 0 )
		gs_free((char *)out, bits_per_column, line_size,
			"sj48_print_page(out)");
	if ( in != 0 )
		gs_free((char *)in, 8, line_size, "sj48_print_page(in)");
	return code;
}
