/* Copyright (C) 1989, 1995 Aladdin Enterprises.  All rights reserved.
  
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
 * This is a modification of Mark Wedel's Apple DMP and 
 * Jonathan Luckey's Imagewriter II driver to
 * support the Imagewriter LQ's higher resolution (320x216):
 *      appledmp:  120dpi x  72dpi is still supported (yuck)
 *	iwlo:	   160dpi x  72dpi
 *	iwhi:	   160dpi x 144dpi
 *      iwlq:      320dpi x 216dpi
 *
 * This is also my first attempt to work with gs. I have not included the LQ's
 * ability to print in colour. Perhaps at a later date I will tackle that.
 *
 * BTW, to get your Imagewriter LQ serial printer to work with a PC, attach it
 * with a nullmodem serial cable.
 *
 * Scott Barker (barkers@cuug.ab.ca)
 */

/*
 * This is a modification of Mark Wedel's Apple DMP driver to
 * support 2 higher resolutions:
 *      appledmp:  120dpi x  72dpi is still supported (yuck)
 *	iwlo:	   160dpi x  72dpi
 *	iwhi:	   160dpi x 144dpi
 *
 * The Imagewriter II is a bit odd.  In pinfeed mode, it thinks its
 * First line is 1 inch from the top of the page. If you set the top
 * form so that it starts printing at the top of the page, and print
 * to near the bottom, it thinks it has run onto the next page and
 * the formfeed will skip a whole page.  As a work around, I reverse
 * the paper about a 1.5 inches at the end of the page before the
 * formfeed to make it think its on the 'right' page.  bah. hack!
 * 
 * This is  my first attempt to work with gs, so your milage may vary
 *
 * Jonathan Luckey (luckey@rtfm.mlb.fl.us)
 */

/* This is a bare bones driver I developed for my apple Dot Matrix Printer.
 * This code originally was from the epson driver, but I removed a lot
 * of stuff that was not needed.
 *
 * The Dot Matrix Printer was a predecessor to the apple Imagewriter.  Its
 * main difference being that it was parallel.
 *
 * This code should work fine on Imagewriters, as they have a superset
 * of commands compared to the DMP printer.
 *
 * This driver does not produce the smalles output files possible.  To
 * do that, it should look through the output strings and find repeat
 * occurances of characters, and use the escape sequence that allows
 * printing repeat sequences.  However, as I see it, this the limiting
 * factor in printing is not transmission speed to the printer itself,
 * but rather, how fast the print head can move.  This is assuming the
 * printer is set up with a reasonable speed (9600 bps)
 *
 * WHAT THE CODE DOES AND DOES NOT DO:
 *
 * To print out images, it sets the printer for unidirection printing
 * and 15 cpi (120 dpi). IT sets line feed to 1/9 of an inch (72 dpi).
 * When finished, it sets things back to bidirection print, 1/8" line
 * feeds, and 12 cpi.  There does not appear to be a way to reset
 * things to initial values.
 *
 * This code does not set for 8 bit characters (which is required). It
 * also assumes that carriage return/newline is needed, and not just
 * carriage return.  These are all switch settings on the DMP, and
 * I have configured them for 8 bit data and cr only.
 *
 * You can search for the strings Init and Reset to find the strings
 * that set up the printer and clear things when finished, and change
 * them to meet your needs.
 *
 * Also, you need to make sure that the printer daemon (assuming unix)
 * doesn't change the data as it is being printed.  I have set my
 * printcap file (sunos 4.1.1) with the string:
 * ms=pass8,-opost
 * and it works fine.
 *
 * Feel free to improve this code if you want.  However, please make
 * sure that the old DMP will still be supported by any changes.  This
 * may mean making an imagewriter device, and just copying this file
 * to something like gdevimage.c.
 *
 * The limiting factor of the DMP is the vertical resolution.  However, I
 * see no way to do anything about this.  Horizontal resolution could
 * be increased by using 17 cpi (136 dpi).  I believe the Imagewriter
 * supports 24 cpi (192 dpi).  However, the higher dpi, the slower
 * the printing.
 *
 * Dot Matrix Code by Mark Wedel (master@cats.ucsc.edu)
 */


#include "gdevprn.h"

/* The device descriptors */
private dev_proc_print_page(dmp_print_page);

/* Standard DMP device */
gx_device_printer far_data gs_appledmp_device =
prn_device(prn_std_procs, "appledmp",
	85,				/* width_10ths, 8.5" */
	110,				/* height_10ths, 11" */
	120, 72,			/* X_DPI, Y_DPI */
	0, 0.5, 0.5, 0,		/* margins */
	1, dmp_print_page);


/*  lowrez Imagewriter device */
gx_device_printer far_data gs_iwlo_device =
prn_device(prn_std_procs, "iwlo",
	85,				/* width_10ths, 8.5" */
	110,				/* height_10ths, 11" */
	160, 72,			/* X_DPI, Y_DPI */
	0, 0.5, 0.5, 0,		/* margins */
	1, dmp_print_page);


/*  hirez Imagewriter device */
gx_device_printer far_data gs_iwhi_device =
prn_device(prn_std_procs, "iwhi",
	85,				/* width_10ths, 8.5" */
	110,				/* height_10ths, 11" */
	160, 144,			/* X_DPI, Y_DPI */
	0, 0.5, 0.5, 0,		/* margins */
	1, dmp_print_page);


/* LQ hirez Imagewriter device */
gx_device_printer far_data gs_iwlq_device =
prn_device(prn_std_procs, "iwlq",
	85,				/* width_10ths, 8.5" */
	110,				/* height_10ths, 11" */
	320, 216,
	0, 0, 0.5, 0,		/* margins */
	1, dmp_print_page);


/* ------ Internal routines ------ */

#define DMP 1
#define IWLO 2
#define IWHI 3
#define IWLQ 4

/* Send the page to the printer. */
private int
dmp_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	
	int dev_type;

	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	/* Note that in_size is a multiple of 8. */
	int in_size = line_size * 8;
  
	byte *buf1 = (byte *)gs_malloc(in_size, 1, "dmp_print_page(buf1)");
	byte *buf2 = (byte *)gs_malloc(in_size, 1, "dmp_print_page(buf2)");
	byte *prn = (byte *)gs_malloc(3*in_size, 1, "dmp_print_page(prn)");
  
	byte *in = buf1;
	byte *out = buf2;
	int lnum = 0;

	/* Check allocations */
	if ( buf1 == 0 || buf2 == 0 || prn == 0 )
	{
		if ( buf1 ) 
			gs_free((char *)buf1, in_size, 1,
			"dmp_print_page(buf1)");
		if ( buf2 ) 
			gs_free((char *)buf2, in_size, 1,
			"dmp_print_page(buf2)");
		if ( prn ) 
			gs_free((char *)prn, in_size, 1,
			"dmp_print_page(prn)");
		return_error(gs_error_VMerror);
	}

	if ( pdev->y_pixels_per_inch == 216 )
		dev_type = IWLQ;
	else if ( pdev->y_pixels_per_inch == 144 )
		dev_type = IWHI;
	else if ( pdev->x_pixels_per_inch == 160 )
		dev_type = IWLO;
	else
		dev_type = DMP;

	/* Initialize the printer and reset the margins. */

	fputs("\r\n\033>\033T16", prn_stream);

	switch(dev_type)
	{
	case IWLQ:
		fputs("\033P\033a3", prn_stream);
		break;
	case IWHI:
	case IWLO:
		fputs("\033P", prn_stream);
		break;
	case DMP: 
	default:
		fputs("\033q", prn_stream);
		break;
	}

	/* Print lines of graphics */
	while ( lnum < pdev->height )
	{	
		byte *inp;
		byte *in_end;
		byte *out_end;
		int lcnt,ltmp;
		int count, passes;
		byte *prn_blk, *prn_end, *prn_tmp;

/* The apple DMP printer seems to be odd in that the bit order on
 * each line is reverse what might be expected.  Meaning, an
 * underscore would be done as a series of 0x80, while on overscore
 * would be done as a series of 0x01.  So we get each
 * scan line in reverse order.
 */

		switch (dev_type)
		{
		case IWLQ: passes = 3; break;
		case IWHI: passes = 2; break;
		case IWLO:
		case DMP:
		default: passes = 1; break;
		}

		for (count = 0; count < passes; count++)
		{
			for (lcnt=0; lcnt<8; lcnt++)
			{
				switch(dev_type)
				{
				case IWLQ: ltmp = lcnt + 8*count; break;
				case IWHI: ltmp = 2*lcnt + count; break;
				case IWLO:
				case DMP:
				default: ltmp = lcnt; break;
				}

				if ((lnum+ltmp)>pdev->height) 
					memset(in+lcnt*line_size,0,line_size);
				else
					gdev_prn_copy_scan_lines(pdev,
					lnum+ltmp, in + line_size*(7 - lcnt),
					line_size);
			}

			out_end = out;
			inp = in;
			in_end = inp + line_size;
			for ( ; inp < in_end; inp++, out_end += 8 )
			{
				gdev_prn_transpose_8x8(inp, line_size,
				out_end, 1);
			}

			out_end = out;

			switch (dev_type)
			{
			case IWLQ: prn_end = prn + count; break;
			case IWHI: prn_end = prn + in_size*count; break;
			case IWLO:
			case DMP:
			default: prn_end = prn; break;
			}

			while ( (int)(out_end-out) < in_size)
			{
				*prn_end = *(out_end++);
				if ((dev_type) == IWLQ) prn_end += 3;
				else prn_end++;
			}
		}
      
		switch (dev_type)
		{
		case IWLQ:
			prn_blk = prn;
			prn_end = prn_blk + in_size * 3;
			while (prn_end > prn && prn_end[-1] == 0 &&
				prn_end[-2] == 0 && prn_end[-3] == 0)
			{
				prn_end -= 3;
			}
			while (prn_blk < prn_end && prn_blk[0] == 0 &&
				prn_blk[1] == 0 && prn_blk[2] == 0)
			{
				prn_blk += 3;
			}
			if (prn_end != prn_blk)
			{
				if ((prn_blk - prn) > 7)
					fprintf(prn_stream,"\033U%04d%c%c%c",
						(int)((prn_blk - prn)/3),
						0, 0, 0);
				else
					prn_blk = prn;
				fprintf(prn_stream,"\033C%04d",
					(int)((prn_end - prn_blk)/3));
				fwrite(prn_blk, 1, (int)(prn_end - prn_blk),
					prn_stream);
		        }
			break;
		case IWHI:
			for (count = 0; count < 2; count++)
			{
				prn_blk = prn_tmp = prn + in_size*count;
				prn_end = prn_blk + in_size;
				while (prn_end > prn_blk && prn_end[-1] == 0)
					prn_end--;
				while (prn_blk < prn_end && prn_blk[0] == 0)
					prn_blk++;
				if (prn_end != prn_blk)
				{
					if ((prn_blk - prn_tmp) > 7)
						fprintf(prn_stream,
							"\033V%04d%c",
							(int)(prn_blk-prn_tmp),
							 0);
					else
						prn_blk = prn_tmp;
					fprintf(prn_stream,"\033G%04d",
						(int)(prn_end - prn_blk));
					fwrite(prn_blk, 1,
						(int)(prn_end - prn_blk),
						prn_stream);
				}
				if (!count) fputs("\033T01\r\n",prn_stream);
			}
			fputs("\033T15",prn_stream);
			break;
		case IWLO:
		case DMP:
		default:
			prn_blk = prn;
			prn_end = prn_blk + in_size;
			while (prn_end > prn_blk && prn_end[-1] == 0)
				prn_end--;
			while (prn_blk < prn_end && prn_blk[0] == 0)
				prn_blk++;
			if (prn_end != prn_blk)
			{
				if ((prn_blk - prn) > 7)
					fprintf(prn_stream,"\033V%04d%c",
						(int)(prn_blk - prn), 0);
				else
					prn_blk = prn;
				fprintf(prn_stream,"\033G%04d",
					(int)(prn_end - prn_blk));
				fwrite(prn_blk, 1, (int)(prn_end - prn_blk),
					prn_stream);
			}
			break;
		}

		fputs("\r\n",prn_stream);

		switch (dev_type)
		{
			case IWLQ: lnum += 24 ; break;
			case IWHI: lnum += 16 ; break;
			case IWLO:
			case DMP:
			default: lnum += 8 ; break;
		}
	}

	/* ImageWriter will skip a whole page if too close to end */
	/* so skip back more than an inch */
	if ( !(dev_type == DMP) )
		fputs("\033T99\n\n\033r\n\n\n\n\033f", prn_stream);
  
	/* Formfeed and Reset printer */
	fputs("\033T16\f\033<\033B\033E", prn_stream);
	fflush(prn_stream);

	gs_free((char *)prn, in_size, 1, "dmp_print_page(prn)");
	gs_free((char *)buf2, in_size, 1, "dmp_print_page(buf2)");
	gs_free((char *)buf1, in_size, 1, "dmp_print_page(buf1)");
	return 0;
}
