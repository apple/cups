/* Copyright (C) 1990, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevbj10.c */
/* Canon Bubble Jet BJ-10e and BJ200 printer driver */
#include "gdevprn.h"

/*
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
 * NOTE:  The bj200 has a DIP switch called "Text scale mode" and if
 * set, it allows the printer to get 66 lines on a letter-sized page
 * by reducing the line spacing by a factor of 14/15.  If this DIP
 * switch is set, the page image printed by ghostscript will also be
 * similarly squeezed.  Thus text scale mode is something ghostscript
 * would like to disable.
 *
 * According to the bj200 manual, which describes the bj10 commands,
 * the printer can be reset either to the settings determined by the
 * DIP switches, or to the factory defaults, and then some of those
 * settings can be specifically overriden.  Unfortunately, the text
 * scale mode and horizontal print position (for letter vs A4 paper)
 * can not be overriden.  On my bj200, the factory settings are for
 * no text scaling and letter paper, thus using the factory defaults
 * also implies letter paper.  I don't know if this is necessarily
 * true for bj200's sold elsewhere, or for other printers that use
 * the same command set.
 *
 * If your factory defaults are in fact the same, you can compile
 * the driver with USE_FACTORY_DEFAULTS defined, in which case the
 * printer will be reset to the factory defaults for letter paper,
 * and reset to the DIP switch settings for A4 paper.  In this case,
 * with letter-sized paper, the text scale mode will be disabled.
 * Further, by leaving the horizontal print position DIP switch set
 * for A4 paper, gs will be able to print on either A4 or letter
 * paper without changing the DIP switch.  Since it's not clear that
 * the factory defaults are universal, the default behaviour is not
 * to define USE_FACTORY_DEFAULTS, and the printer will always be
 * reset to the DIP switch defaults.
 */

#define BJ200_TOP_MARGIN		0.12
#define BJ200_BOTTOM_MARGIN		0.29
#define BJ200_LETTER_SIDE_MARGIN	0.25
#define BJ200_A4_SIDE_MARGIN		0.13

private dev_proc_open_device(bj200_open);

private dev_proc_print_page(bj10e_print_page);

private gx_device_procs prn_bj200_procs =
  prn_procs(bj200_open, gdev_prn_output_page, gdev_prn_close);

gx_device_printer far_data gs_bj200_device =
  prn_device(prn_bj200_procs, "bj200",
	DEFAULT_WIDTH_10THS,
	DEFAULT_HEIGHT_10THS,
	360,				/* x_dpi */
	360,				/* y_dpi */
	0, 0, 0, 0,			/* margins filled in by bj200_open */
	1, bj10e_print_page);

/*
 * (<simon@pogner.demon.co.uk>, aka <sjwright@cix.compulink.co.uk>):
 * My bj10ex, which as far as I can tell is just like a bj10e, works
 * fine with the bj200 setup here.
 */

gx_device_printer far_data gs_bj10e_device =
  prn_device(prn_bj200_procs, "bj10e",
	DEFAULT_WIDTH_10THS,
	DEFAULT_HEIGHT_10THS,
	360,				/* x_dpi */
	360,				/* y_dpi */
	0,0,0,0,			/* margins */
	1, bj10e_print_page);

/*
 * Notes on the BJ10e/BJ200 command set.
 *

According to the BJ200 manual, the "set initial condition" sequence (ESC [
K) has 2 bytes which can override the DIP switches -- these are the last 2
bytes.  Several bits are listed as "reserved" -- one or more may possibly
affect the sheet feeder.  The first is referred to as <P1>, with the
following meaning:
				1		0
bit 7	ignore/process P1	ignore		process
bit 6	reserved
bit 5	alarm			disabled	enabled
bit 4	automatic CR		CR+LF		CR
bit 3	automatic LF		CR+LF		LF
bit 2	page length		12 inches	11 inches
bit 1	style for zero		slashed		not slashed
bit 0	character set		set 2		set 1

The last byte is <P2>, with the following meaning:
				1		0
bit 7	ignore/process P2	ignore		process
bit 6	code page		850		437
bit 5	reserved
bit 4	reserved
bit 3	reserved
bit 2	reserved
bit 1	reserved
bit 0	reserved

The automatic CR setting is important to gs, but the rest shouldn't matter
(gs doesn't print characters or send LF, and it explicitly sets the page
length).  The sequence ESC 5 <n> controls automatic CR -- if <n> is 0x00,
it is turned off (CR only) and if <n> is 0x01, it is turned on (CR + LF).
So we do following: Change the initialization string to so that the last 2
of the 9 bytes are \200 rather than \000.  Then add
	|* Turn off automatic carriage return, otherwise we get line feeds. *|
	fwrite("\0335\000", 1, 3, prn_stream);
after the initialization.  (Actually, instead of setting the last 2 bytes
to \200, we suppress them altogether by changing the byte count from \004
to \002 (the byte count is the 4th (low 8 bits) and 5th (high 8 bits) bytes
in the initialization sequence).)

*/

/* ------ Internal routines ------ */

/* Open the printer, and set the margins. */
private int
bj200_open(gx_device *pdev)
{
	/* Change the margins according to the paper size.
	   The top and bottom margins don't seem to depend on the
	   page length, but on the paper handling mechanism;
	   The side margins do depend on the paper width, as the
	   printer centres the 8" print line on the page. */

	static const float a4_margins[4] =
	 {	BJ200_A4_SIDE_MARGIN, BJ200_BOTTOM_MARGIN,
		BJ200_A4_SIDE_MARGIN, BJ200_TOP_MARGIN
	 };
	static const float letter_margins[4] =
	 {	BJ200_LETTER_SIDE_MARGIN, BJ200_BOTTOM_MARGIN,
		BJ200_LETTER_SIDE_MARGIN, BJ200_TOP_MARGIN
	 };

	gx_device_set_margins(pdev,
		(pdev->width / pdev->x_pixels_per_inch <= 8.4 ?
		 a4_margins : letter_margins),
		true);
	return gdev_prn_open(pdev);
}

/* Send the page to the printer. */
private int
bj10e_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	int line_size = gx_device_raster((gx_device *)pdev, 0);
	int xres = pdev->x_pixels_per_inch;
	int yres = pdev->y_pixels_per_inch;
	int mode = (yres == 180 ?
			(xres == 180 ? 11 : 12) :
			(xres == 180 ? 14 : 16));
	int bytes_per_column = (yres == 180) ? 3 : 6;
	int bits_per_column = bytes_per_column * 8;
	int skip_unit = bytes_per_column * 3;
	byte *in = (byte *)gs_malloc(8, line_size, "bj10e_print_page(in)");
	byte *out = (byte *)gs_malloc(bits_per_column, line_size, "bj10e_print_page(out)");
	int lnum = 0;
	int skip = 0;
	int code = 0;
	int last_row = dev_print_scan_lines(pdev);
	int limit = last_row - bits_per_column;

	if ( in == 0 || out == 0 )
	{	code = gs_error_VMerror;
		gs_note_error(code);
		goto fin;
	}

	/* Initialize the printer. */
#ifdef USE_FACTORY_DEFAULTS
	/* Check for U.S. letter vs. A4 paper. */
	fwrite(( pdev->width / pdev->x_pixels_per_inch <= 8.4 ?
		"\033[K\002\000\000\044"	/*A4--DIP switch defaults*/ :
		"\033[K\002\000\004\044"	/*letter--factory defaults*/ ),
	       1, 7, prn_stream);
#else
	fwrite("\033[K\002\000\000\044", 1, 7, prn_stream);
#endif

	/* Turn off automatic carriage return, otherwise we get line feeds. */
	fwrite("\0335\000", 1, 3, prn_stream);

	/* Set vertical spacing. */
	fwrite("\033[\\\004\000\000\000", 1, 7, prn_stream);
	fputc(yres & 0xff, prn_stream);
	fputc(yres >> 8, prn_stream);

	/* Set the page length.  This is the printable length, in inches. */
	fwrite("\033C\000", 1, 3, prn_stream);
	fputc((last_row + yres - 1)/yres, prn_stream);

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
		int bnum;

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
		    {	skip -= (lnum - limit);
			lnum = limit;
		    }
		while ( skip > 255 )
		   {	fputs("\033J\377", prn_stream);
			skip -= 255;
		   }
		if ( skip )
			fprintf(prn_stream, "\033J%c", skip);

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
		   of 1/120 inches, so we look at the data in groups of
		   3 columns, since 3/360 = 1/120, and 3/180 = 2/120.  */
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
				if ( xres == 180 ) count <<= 1;
				fprintf(prn_stream, "\033d%c%c",
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
			count = outl - out_beg + 1;
			fprintf(prn_stream, "\033[g%c%c%c",
				count & 0xff, count >> 8, mode);
			fwrite(out_beg, 1, count - 1, prn_stream);
			out_beg = outl;
			outl += n;
		   }
		while ( out_beg < out_end );

		fputc('\r', prn_stream);
	   }

	/* Eject the page */
xit:	fputc(014, prn_stream);	/* form feed */
	fflush(prn_stream);
fin:	if ( out != 0 )
		gs_free((char *)out, bits_per_column, line_size,
			"bj10e_print_page(out)");
	if ( in != 0 )
		gs_free((char *)in, 8, line_size, "bj10e_print_page(in)");
	return code;
}
