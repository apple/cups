/* Copyright (C) 1989, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevdjet.c */
/* HP LaserJet/DeskJet driver for Ghostscript */
#include "gdevprn.h"
#include "gdevpcl.h"

/*
 * Thanks for various improvements to:
 *	Jim Mayer (mayer@wrc.xerox.com)
 *	Jan-Mark Wams (jms@cs.vu.nl)
 *	Frans van Hoesel (hoesel@chem.rug.nl)
 *	George Cameron (g.cameron@biomed.abdn.ac.uk)
 *	Nick Duffek (nsd@bbc.com)
 * Thanks for the LJIIID duplex capability to:
 *	PDP (Philip) Brown (phil@3soft-uk.com)
 * Thanks for the OCE 9050 driver to:
 *	William Bader (wbader@EECS.Lehigh.Edu)
 */

/*
 * You may select a default resolution of 75, 100, 150, 300, or
 * (LJ4 only) 600 DPI in the makefile, or an actual resolution on
 * the gs command line.
 *
 * If the preprocessor symbol A4 is defined, the default paper size is
 * the European A4 size; otherwise it is the U.S. letter size (8.5"x11").
 *
 * To determine the proper "margin" settings for your printer, see the
 * file align.ps.
 */

/* Define the default, maximum resolutions. */
#ifdef X_DPI
#  define X_DPI2 X_DPI
#else
#  define X_DPI 300
#  define X_DPI2 600
#endif
#ifdef Y_DPI
#  define Y_DPI2 Y_DPI
#else
#  define Y_DPI 300
#  define Y_DPI2 600
#endif

/*
 * For all DeskJet Printers:
 *
 *  Maximum printing width               = 2400 dots = 8"
 *  Maximum recommended printing height  = 3100 dots = 10 1/3"
 *
 * All Deskjets have 1/2" unprintable bottom margin.
 * The recommendation comes from the HP Software Developer's Guide for
 * the DeskJet 500, DeskJet PLUS, and DeskJet printers, version C.01.00
 * of 12/1/90.
 *
 * Note that the margins defined just below here apply only to the DeskJet;
 * the paper size, width and height apply to the LaserJet as well.
 */

/* Margins are left, bottom, right, top. */
/* from Frans van Hoesel hoesel@rugr86.rug.nl. */
/* A4 has a left margin of 1/8 inch and at a printing width of
 * 8 inch this give a right margin of 0.143. The 0.09 top margin is
 * not the actual margin - which is 0.07 - but compensates for the
 * inexact paperlength which is set to 117 10ths.
 * Somebody should check for letter sized paper. I left it at 0.07".
 */
#define DESKJET_MARGINS_LETTER  0.2, 0.45, 0.3, 0.05
#define DESKJET_MARGINS_A4	0.125, 0.5, 0.143, 0.09
/* Similar margins for the LaserJet, */
/* from Eddy Andrews eeandrew@pyr.swan.ac.uk: */
#define LASERJET_MARGINS_A4	0.25, 0.20, 0.25, 0.00
/* the following from Bob Atkins bob@digilink.net are WRONG: */
/*#define LASERJET_MARGINS_A4	0.25, 0.0, 0.25, 0.0*/
/* from Dale Atems atems@igor.physics.wayne.edu: */
/*#define LASERJET_MARGINS_LETTER	0.275, -0.06, 0.425, 0.26*/
/* ... superseded by the following from Bob Atkins bob@digilink.net: */
#define LASERJET_MARGINS_LETTER	0.25, 0.0, 0.25, 0.0

/* The number of blank lines that make it worthwhile to reposition */
/* the cursor. */
#define MIN_SKIP_LINES 7

/* We round up the LINE_SIZE to a multiple of a ulong for faster scanning. */
#define W sizeof(word)

/* Printer types */
#define LJ	0
#define LJplus	1
#define LJ2p	2
#define LJ3	3
#define DJ	4
#define DJ500	5
#define LJ4	6
#define LP2563B	7
#define LJ3D	8
#define	OCE9050	9

/*
 * The notion that there is such a thing as a "PCL printer" is a fiction:
 * no two "PCL" printers, even at the same PCL level, have identical
 * command sets.  The command strings below were established by hearsay
 * and by trial and error.  (The H-P documentation isn't fully accurate
 * either; for example, it doesn't reveal that the DeskJet printers
 * implement anything beyond PCL 3.)
 */

/* Printer capabilities */
typedef enum {
	mode_0,		/* PCL 3, use <ESC>*p+<n>Y for vertical spacing */
	mode_0ns,	/* PCL 3 but no vertical spacing */
	mode_2,		/* PCL 4, use <ESC>*b<n>Y for vertical spacing */
	mode_2p,	/* PCL 4 but no vertical spacing */
	mode_3,		/* PCL 5, use <ESC>*b<n>Y and clear seed row */
			/* (includes mode 2) */
	mode_3ns	/* PCL 5 but no vertical spacing */
} compression_modes;

/* The device descriptors */
private dev_proc_open_device(hpjet_open);
private dev_proc_close_device(hpjet_close);
private dev_proc_print_page(djet_print_page);
private dev_proc_print_page(djet500_print_page);
private dev_proc_print_page(ljet_print_page);
private dev_proc_print_page(ljetplus_print_page);
private dev_proc_print_page(ljet2p_print_page);
private dev_proc_print_page(ljet3_print_page);
private dev_proc_print_page(ljet3d_print_page);
private dev_proc_print_page(ljet4_print_page);
private dev_proc_print_page(lp2563_print_page);
private dev_proc_print_page(oce9050_print_page);

private gx_device_procs prn_hp_procs =
  prn_params_procs(hpjet_open, gdev_prn_output_page, hpjet_close,
		   gdev_prn_get_params, gdev_prn_put_params);

gx_device_printer far_data gs_deskjet_device =
  prn_device(prn_hp_procs, "deskjet",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0, 0, 0, 0,		/* margins filled in by hpjet_open */
	1, djet_print_page);

gx_device_printer far_data gs_djet500_device =
  prn_device(prn_hp_procs, "djet500",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0, 0, 0, 0,		/* margins filled in by hpjet_open */
	1, djet500_print_page);

gx_device_printer far_data gs_laserjet_device =
  prn_device(prn_hp_procs, "laserjet",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0.05, 0.25, 0.55, 0.25,		/* margins */
	1, ljet_print_page);

gx_device_printer far_data gs_ljetplus_device =
  prn_device(prn_hp_procs, "ljetplus",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0.05, 0.25, 0.55, 0.25,		/* margins */
	1, ljetplus_print_page);

gx_device_printer far_data gs_ljet2p_device =
  prn_device(prn_hp_procs, "ljet2p",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0.20, 0.25, 0.25, 0.25,		/* margins */
	1, ljet2p_print_page);

gx_device_printer far_data gs_ljet3_device =
  prn_device(prn_hp_procs, "ljet3",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0.20, 0.25, 0.25, 0.25,		/* margins */
	1, ljet3_print_page);

gx_device_printer far_data gs_ljet3d_device =
  prn_device(prn_hp_procs, "ljet3d",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0.20, 0.25, 0.25, 0.25,		/* margins */
	1, ljet3d_print_page);

gx_device_printer far_data gs_ljet4_device =
  prn_device(prn_hp_procs, "ljet4",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI2, Y_DPI2,
	0, 0, 0, 0,			/* margins */
	1, ljet4_print_page);
  
gx_device_printer far_data gs_lp2563_device =
  prn_device(prn_hp_procs, "lp2563",
	DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	X_DPI, Y_DPI,
	0, 0, 0, 0,			/* margins */
	1, lp2563_print_page);

gx_device_printer far_data gs_oce9050_device =
  prn_device(prn_hp_procs, "oce9050",
	24 * 10, 24 * 10,		/* 24 inch roll (can print 32" also) */
	400, 400,			/* 400 dpi */
	0, 0, 0, 0,			/* margins */
	1, oce9050_print_page);

/* Forward references */
private int hpjet_print_page(P6(gx_device_printer *, FILE *, int, int, compression_modes, const char *));

#define ppdev ((gx_device_printer *)pdev)

/* Open the printer, adjusting the margins if necessary. */
private int
hpjet_open(gx_device *pdev)
{	/* Change the margins if necessary. */
	const float _ds *m = 0;

	if ( ppdev->printer_procs.print_page == djet_print_page ||
	     ppdev->printer_procs.print_page == djet500_print_page
	   )
	{	static const float m_a4[4] = { DESKJET_MARGINS_A4 };
		static const float m_letter[4] = { DESKJET_MARGINS_LETTER };
		m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? m_a4 :
		     m_letter);
	}
	else if ( ppdev->printer_procs.print_page == oce9050_print_page ||
		  ppdev->printer_procs.print_page == lp2563_print_page
		)
	  ;
	else		/* LaserJet */
	{	static const float m_a4[4] = { LASERJET_MARGINS_A4 };
		static const float m_letter[4] = { LASERJET_MARGINS_LETTER };
		m = (gdev_pcl_paper_size(pdev) == PAPER_SIZE_A4 ? m_a4 :
		     m_letter);
	}
	if ( m != 0 )
	  gx_device_set_margins(pdev, m, true);
	/* If this is a LJIIID, enable Duplex. */
	if ( ppdev->printer_procs.print_page == ljet3d_print_page )
	  ppdev->Duplex = true, ppdev->Duplex_set = 0;
	return gdev_prn_open(pdev);
}

/* hpjet_close is only here to eject odd numbered pages in duplex mode. */
private int
hpjet_close(gx_device *pdev)
{	if ( ppdev->Duplex_set >= 0 && ppdev->Duplex )
	  {	gdev_prn_open_printer(pdev, 1);
		fputs("\033&l0H", ppdev->file) ;
	  }
	return gdev_prn_close(pdev);
}

#undef ppdev

/* ------ Internal routines ------ */

/* The DeskJet can compress (mode 2) */
private int
djet_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, DJ, 300, mode_2,
		"\033&k1W\033*b2M");
}
/* The DeskJet500 can compress (modes 2&3) */
private int
djet500_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, DJ500, 300, mode_3,
		"\033&k1W");
}
/* The LaserJet series II can't compress */
private int
ljet_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, LJ, 300, mode_0,
		"\033*b0M");
}
/* The LaserJet Plus can't compress */
private int
ljetplus_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, LJplus, 300, mode_0,
		"\033*b0M");
}
/* LaserJet series IIp & IId compress (mode 2) */
/* but don't support *p+ or *b vertical spacing. */
private int
ljet2p_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, LJ2p, 300, mode_2p,
		"\033*r0F\033*b2M");
}
/* All LaserJet series IIIs (III,IIId,IIIp,IIIsi) compress (modes 2&3) */
private int
ljet3_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, LJ3, 300, mode_3,
		"\033*r0F");
}
/* LaserJet IIId is same as LaserJet III, except for duplex */
private int
ljet3d_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, LJ3D, 300, mode_3,
		"\033*r0F");
}
/* LaserJet 4 series compresses, and it needs a special sequence to */
/* allow it to specify coordinates at 600 dpi. */
private int
ljet4_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	int dots_per_inch = (int)pdev->y_pixels_per_inch;
	char real_init[50];
	sprintf(real_init, "\033*r0F\033&u%dD", dots_per_inch);
	return hpjet_print_page(pdev, prn_stream, LJ4, dots_per_inch, mode_3,
		real_init);
}
/* The 2563B line printer can't compress */
/* and doesn't support *p+ or *b vertical spacing. */
private int
lp2563_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	return hpjet_print_page(pdev, prn_stream, LP2563B, 300, mode_0ns,
		"\033*b0M");
}
/* The Oce line printer has TIFF compression */
/* and doesn't support *p+ or *b vertical spacing. */
private int
oce9050_print_page(gx_device_printer *pdev, FILE *prn_stream)
{	int code;

	/* Switch to HP_RTL. */
	fputs("\033%1B", prn_stream);	/* Enter HPGL/2 mode */
	fputs("BP", prn_stream);	/* Begin Plot */
	fputs("IN;", prn_stream);	/* Initialize (start plot) */
	fputs("\033%1A", prn_stream);	/* Enter PCL mode */

	code = hpjet_print_page(pdev, prn_stream, OCE9050, 400, mode_3ns,
				"\033*b3M");

	/* Return to HPGL/2 mode. */
	fputs("\033%1B", prn_stream);	/* Enter HPGL/2 mode */
	if ( code == 0 )
	  {	fputs("PU", prn_stream);	/* Pen Up */
		fputs("SP0", prn_stream);	/* Pen Select */
		fputs("PG;", prn_stream);	/* Advance Full Page */
		fputs("\033E", prn_stream);	/* Reset */
	  }

	return code;
}

/* Send the page to the printer.  For speed, compress each scan line, */
/* since computer-to-printer communication time is often a bottleneck. */
private int
hpjet_print_page(gx_device_printer *pdev, FILE *prn_stream, int ptype,
  int dots_per_inch, compression_modes cmodes, const char *init_string)
{	int line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
	int line_size_words = (line_size + W - 1) / W;
	uint storage_size_words = line_size_words * 8; /* data, out_row, out_row_alt, prev_row */
	word *storage = (ulong *)gs_malloc(storage_size_words, W,
					   "hpjet_print_page");
	word
	  *data_words,
	  *out_row_words,
	  *out_row_alt_words,
	  *prev_row_words;
#define data ((byte *)data_words)
#define out_row ((byte *)out_row_words)
#define out_row_alt ((byte *)out_row_alt_words)
#define prev_row ((byte *)prev_row_words)
	byte *out_data;
	int x_dpi = pdev->x_pixels_per_inch;
	int y_dpi = pdev->y_pixels_per_inch;
	int y_dots_per_pixel = dots_per_inch / y_dpi;
	int num_rows = dev_print_scan_lines(pdev);

	int out_count;
	int compression = -1;
	static const char *from2to3 = "\033*b3M";
	static const char *from3to2 = "\033*b2M";
	int penalty_from2to3 = strlen(from2to3);
	int penalty_from3to2 = strlen(from3to2);
	int paper_size = gdev_pcl_paper_size((gx_device *)pdev);
	int code = 0;
	bool dup = pdev->Duplex;
	bool dupset = pdev->Duplex_set >= 0;

	if ( storage == 0 )	/* can't allocate working area */
		return_error(gs_error_VMerror);
	data_words = storage;
	out_row_words = data_words + (line_size_words * 2);
	out_row_alt_words = out_row_words + (line_size_words * 2);
	prev_row_words = out_row_alt_words + (line_size_words * 2);
	/* Clear temp storage */
	memset(data, 0, storage_size_words * W);

	/* Initialize printer. */
	if ( pdev->PageCount == 0 )
	  {		/* Put out init string before first page. */
		fputs("\033E", prn_stream);		/* reset printer */
		/* If the printer supports it, set the paper size */
		/* based on the actual requested size. */
		if ( !(ptype == LJ || ptype == LJplus ) )
		{	fprintf(prn_stream, "\033&l%dA", paper_size);
		}
		fputs("\033&l0o0l0E", prn_stream);
		fputs(init_string, prn_stream);
		/* If printer can duplex, set duplex mode appropriately. */
		if ( ptype == LJ3D )
		{       if ( dupset && dup )
			  fputs("\033&l1S",prn_stream);
		        else if ( dupset && !dup )
			  fputs("\033&l0S",prn_stream);
		        else	/* default to duplex for this printer */
			  fputs("\033&l1S",prn_stream);
		}
	}

		/* End raster graphics, position cursor at top. */
	fputs("\033*rB\033*p0x0Y", prn_stream);

		/* The DeskJet and DeskJet Plus reset everything upon */
		/* receiving \033*rB, so we must reinitialize graphics mode. */
	if ( ptype == DJ ) 
          fputs(init_string, prn_stream);

		/* Set resolution. */
	fprintf(prn_stream, "\033*t%dR", x_dpi);

	/* Send each scan line in turn */
	   {	int lnum;
		int num_blank_lines = 0;
		word rmask = ~(word)0 << (-pdev->width & (W * 8 - 1));

		/* Transfer raster graphics. */
		for ( lnum = 0; lnum < num_rows; lnum++ )
		   {	register word *end_data =
				data_words + line_size_words;
			code = gdev_prn_copy_scan_lines(pdev, lnum,
						 (byte *)data, line_size);
			if ( code < 0 )
				break;
		   	/* Mask off 1-bits beyond the line width. */
			end_data[-1] &= rmask;
			/* Remove trailing 0s. */
			while ( end_data > data_words && end_data[-1] == 0 )
			  end_data--;
			if ( end_data == data_words )
			   {	/* Blank line */
				num_blank_lines++;
				continue;
			   }

			/* We've reached a non-blank line. */
			/* Put out a spacing command if necessary. */
			if ( num_blank_lines == lnum )
			{	/* We're at the top of a page. */
				if ( cmodes == mode_2p || cmodes == mode_0ns )
				{	/* Start raster graphics. */
					fputs("\033*r1A", prn_stream);
					for ( ; num_blank_lines; num_blank_lines-- )
						fputs("\033*bW", prn_stream);
				}
				else if ( cmodes == mode_3ns )
				{	/* Start raster graphics. */
					fputs("\033*r1A", prn_stream);
#if 1	/* don't waste paper */
					if ( num_blank_lines > 0 )
						fputs("\033*b0W", prn_stream);
					num_blank_lines = 0;
#else
					for ( ; num_blank_lines; num_blank_lines-- )
						fputs("\033*b0W", prn_stream);
#endif
				}
				else
				{	if ( num_blank_lines > 0 )
					  fprintf(prn_stream, "\033*p+%dY",
						num_blank_lines * y_dots_per_pixel);
					/* Start raster graphics. */
					fputs("\033*r1A", prn_stream);
				}
			}
			/* Skip blank lines if any */
			else if ( num_blank_lines != 0 )
			{ /* For Canon LBP4i and some others: */
			  /* <ESC>*b<n>Y doesn't properly clear the seed */
			  /* row if we are in compression mode 3. */
			  if ( (num_blank_lines < MIN_SKIP_LINES &&
				compression != 3) ||
				cmodes == mode_3ns ||
				cmodes == mode_2p || cmodes == mode_0ns
			      )
			   {	/* Moving down from current position */
				/* causes head motion on the DeskJet, so */
				/* if the number of lines is small, */
				/* we're better off printing blanks. */
				if ( cmodes == mode_3ns && compression != 2 )
				{	/* Switch to mode 2 */
					fputs(from3to2, prn_stream);
					compression = 2;
				}
				if ( cmodes == mode_3 || cmodes == mode_3ns )
				{	/* Must clear the seed row. */
					fputs("\033*b1Y", prn_stream);
					num_blank_lines--;
				}
				if ( cmodes == mode_3ns )
				{
					for ( ; num_blank_lines; num_blank_lines-- )
						fputs("\033*b0W", prn_stream);
				}
				else
			   	{
					for ( ; num_blank_lines; num_blank_lines-- )
						fputs("\033*bW", prn_stream);
				}
			   }
			   else if ( cmodes == mode_0 )	/* PCL 3 */
			   {	fprintf(prn_stream, "\033*p+%dY",
					num_blank_lines * y_dots_per_pixel);
			   }
			   else
			   {	   fprintf(prn_stream, "\033*b%dY",
					   num_blank_lines);
			   }
			   /* Clear the seed row (only matters for */
			   /* mode 3 compression). */
			   memset(prev_row, 0, line_size);
			}
			num_blank_lines = 0;

			/* Choose the best compression mode */
			/* for this particular line. */
			switch (cmodes)
			  {
			  case mode_3:
			  case mode_3ns:
			   {	/* Compression modes 2 and 3 are both */
				/* available.  Try both and see which one */
				/* produces the least output data. */
				int count3 = gdev_pcl_mode3compress(line_size, data,
							   prev_row, out_row);
				int count2 = gdev_pcl_mode2compress(data_words, end_data,
							   out_row_alt);
				int penalty3 =
				  (compression == 3 ? 0 : penalty_from2to3);
				int penalty2 =
				  (compression == 2 ? 0 : penalty_from3to2);
				if ( count3 + penalty3 < count2 + penalty2)
				   {	if ( compression != 3 )
					    fputs(from2to3, prn_stream);
					compression = 3;
					out_data = out_row;
					out_count = count3;
				   }
				else
				   {	if ( compression != 2 )
					    fputs(from3to2, prn_stream);
					compression = 2;
					out_data = out_row_alt;
					out_count = count2;
				   }
				break;
			   }
			  case mode_2:
			  case mode_2p:
				out_data = out_row;
			   	out_count = gdev_pcl_mode2compress(data_words, end_data,
							  out_row);
				break;
			  default:
				out_data = data;
				out_count = (byte *)end_data - data;
			  }

			/* Transfer the data */
			fprintf(prn_stream, "\033*b%dW", out_count);
			fwrite(out_data, sizeof(byte), out_count,
			       prn_stream);
		   }
	}

	/* end raster graphics and eject page */
	fputs("\033*rB\f", prn_stream);
		
	/* free temporary storage */
	gs_free((char *)storage, storage_size_words, W, "hpjet_print_page");

	return code;
}
