/* Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.  All rights reserved.

This file is part of Ghostscript.

Ghostscript is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves any
particular purpose or works at all, unless he says so in writing.  Refer
to the Ghostscript General Public License for full details.

Everyone is granted permission to copy, modify and redistribute
Ghostscript, but only under the conditions described in the Ghostscript
General Public License.  A copy of this license is supposed to have been
given to you along with Ghostscript so you can know your rights and
responsibilities.  It should be in a file named COPYLEFT.  Among other
things, the copyright notice and this notice must be preserved on all
copies.  */

/*
gdevln03.c
Ghostscript driver for DEC LN03 printer

Ulrich Mueller, Div. PPE, CERN, CH-1211 Geneva 23 <ulm@vsnhd1.cern.ch>
This code is subject to the GNU General Public License

ulm 91-02-13 created as driver for gs 2.1.1
ulm 91-07-23 adapted to gs 2.2
ulm 91-08-21 changed memory allocation to gs_malloc,
	     ported to VMS (contributed by Martin Stiftinger, TU Vienna)
lpd 91-11-24 sped up by removing multiplies from inner loop
ijmp 92-04-14 add support for la75/la50 (macphed@dvinci.usask.ca)
ulm 92-09-25 support letter size paper (8.5" x 11")
bbl 93-06-10 added la70 mode (bruce@csugrad.cs.vt.edu)
lpd/ab 94-02-04 added la75plus mode (Andre_Beck@IRS.Inf.TU-Dresden.de)
pbk 94-02-28 keep lines less than 80 chars for systems where files
             typed to terminal don't work otherwise; define separate
             eject string for each device for flexibility;
             add support for CRT sixels (keegstra@tonga.gsfc.nasa.gov)
*/

#include "gdevprn.h"

/* Forward references */
private int sixel_print_page(P4(gx_device_printer *pdev, FILE *prn_stream,
                                const char *init, const char *eject));

/* The device descriptor */
private dev_proc_output_page(sixel_output_page);
private dev_proc_print_page(ln03_print_page);
/* We have to supply our own procs, since we have to intercept */
/* output_page so we can open the printer in text mode. */
private gx_device_procs sixel_procs =
  prn_procs(gdev_prn_open, sixel_output_page, gdev_prn_close);

#ifdef A4
#  define BOTTOM_MARGIN 0.5
#else
#  define BOTTOM_MARGIN 0.4
#endif
gx_device_printer gs_ln03_device =
    prn_device(sixel_procs, "ln03",
	       DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	       300, 300,		/* x_dpi, y_dpi */
	       0, BOTTOM_MARGIN, 0, 0,	/* left, bottom, right, top margin */
	       1, ln03_print_page);

/*
 * Initialization string: switch to graphics mode, 300 dpi
 * <ESC>[!p	    DECSTR	soft terminal reset
 * <ESC>[11h	    PUM		select unit of measurement
 * <ESC>[7 I	    SSU		select pixel as size unit
 * <ESC>[?52h	    DECOPM	origin is upper-left corner
 * <ESC>[0t	    DECSLPP	set maximum form length
 * <ESC>[1;2475s    DECSLRM	set left and right margins
 * <ESC>P0;0;1q			select sixel graphics mode
 * "1;1		    DECGRA	aspect ratio (1:1)
 */

#define LN03_INIT \
 "\033[!p\033[11h\033[7 I\033[?52h\033[0t\033[1;2475s\033P0;0;1q\"1;1"

/* leave sixel graphics mode, eject page
      <ESC>\		ST	string terminator
      <FF>		FF	form feed */
#define LN03_EJECT "\033\\\f"

private int
ln03_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,LN03_INIT,LN03_EJECT));
}

/*
 * LA50 dot matrix printer device.
 * This uses North American 8.5 x 11 inch paper size.
 */
private dev_proc_print_page(la50_print_page);
gx_device_printer gs_la50_device =
    prn_device(sixel_procs, "la50",
	       85,
	       110,
	       144, 72,
	       0, 0, 0.5, 0,
	       1, la50_print_page);
/* LA50's use a very primitive form of initialization */

#define LA50_INIT "\033Pq"

/* leave sixel graphics mode, eject page
      <ESC>\		ST	string terminator
      <FF>		FF	form feed */
#define LA50_EJECT "\033\\\f"

private int
la50_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,LA50_INIT,LA50_EJECT));
}

/*
 * LA70 dot matrix printer device.
 * This uses North American 8.5 x 11 inch paper size.
 */
private dev_proc_print_page(la70_print_page);
gx_device_printer gs_la70_device =
    prn_device(sixel_procs, "la70",
	       85,
	       110,
	       144, 144,
	       0, 0, 0.5, 0,
	       1, la70_print_page);

#define LA70_INIT "\033P0;0;0q\"1;1"

/* leave sixel graphics mode, eject page
      <ESC>\		ST	string terminator
      <FF>		FF	form feed */
#define LA70_EJECT "\033\\\f"

private int
la70_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,LA70_INIT,LA70_EJECT));
}

/*
 * LA75 dot matrix printer device.
 * This uses North American 8.5 x 11 inch paper size.
 */
private dev_proc_print_page(la75_print_page);
gx_device_printer gs_la75_device =
    prn_device(sixel_procs, "la75",
	       85,
	       110,
	       144, 72,
	       0, 0, 0.5, 0,
	       1, la75_print_page);

#define LA75_INIT "\033P0;0;0q"

/* leave sixel graphics mode, eject page
      <ESC>\		ST	string terminator
      <FF>		FF	form feed */
#define LA75_EJECT "\033\\\f"

private int
la75_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,LA75_INIT,LA75_EJECT));
}

/*
 * LA75+ dot matrix printer device (24 needles).
 * This uses either A4 or US paper size.
 * Last changed: 03.02.94 -abp
 */
private dev_proc_print_page(la75plus_print_page);
gx_device_printer gs_la75plus_device =
    prn_device(sixel_procs, "la75plus",
               85,
               110,
               180, 180,
               0, 0, BOTTOM_MARGIN, 0,
               1, la75plus_print_page);

/*
 * Init String:
 * <ESC>c               full reset
 * <DCS>0;0;1q          start sixel printing at max resolution
 * "1;1                 aspect ratio 1:1
 */

#define LA75PLUS_INIT "\033c\033P0;0;1q\"1;1"

/* leave sixel graphics mode, eject page
      <ESC>\		ST	string terminator
      <FF>		FF	form feed */
#define LA75PLUS_EJECT "\033\\\f"

private int
la75plus_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,LA75PLUS_INIT,LA75PLUS_EJECT));
}

/*
 * CRT sixels, e.g. for display by VT240-like terminals or MSKERMIT.
 * Parameters set so MSKERMIT using sixels matches native EGA.
 * COBE/DMR prefers (145, 100, 56.8, 28.5) to match its program DPSI.
 */
private dev_proc_print_page(sxlcrt_print_page);
gx_device_printer gs_sxlcrt_device =
    prn_device(sixel_procs,
               "sxlcrt",
               180,
               110,
               42.6667, 32.0,
               0, 0, 0, 0,
               1, sxlcrt_print_page);

/* Use init and eject strings similar to COBE/DMR program DQUSIXEL */
/* Add an exit Tek emulation sequence so kermit displays properly  */
#define SXLCRT_INIT "\033[?38l\033P0q"
/* leave sixel graphics mode, home cursor */
#define SXLCRT_EJECT "\033\\\033[23;0H"

private int
sxlcrt_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,SXLCRT_INIT,SXLCRT_EJECT));
}

/* ------ Internal routines ------ */

/* Open the printer in text mode before gdev_prn_output_page */
/* opens it in binary mode. */
private int
sixel_output_page(gx_device *pdev, int num_copies, int flush)
{	int code = gdev_prn_open_printer(pdev, 0);
	if ( code < 0 )
		return code;
	return gdev_prn_output_page(pdev, num_copies, flush);
}

/* Send the page to the printer. */
/* Keep all lines <= 80 chars */
private int
sixel_print_page(gx_device_printer *pdev, FILE *prn_stream,
                 const char *init, const char *eject)
{
    byte *in, *inp;
    int lnum, lcount, l, count, empty, mask, c, oldc, line_size, in_size;
    int ccount;

    line_size = gdev_mem_bytes_per_scan_line((gx_device *)pdev);
    in_size = line_size * 6;
    in = (byte *)gs_malloc(in_size, 1, "sixel_print_page");

    /* Check allocation */
    if (!in) return(-1);

    fputs(init,prn_stream);
    ccount = strlen(init);

    /* Print lines of graphics */
    for (lnum = lcount = 0; lnum < pdev->height; lnum+=6, lcount++) {
	gdev_prn_copy_scan_lines(pdev, lnum, inp = in, line_size * 6);

	mask = 0200;
	oldc = 077;
	empty = 1;

	for (l = pdev->width, count = 0; --l >= 0; count++) {
	    /* transpose 6*8 rectangle */
	    register byte *iptr = inp;
	    c = 077;
	    if (*iptr & mask)
		c += 1;
	    if (*(iptr += line_size) & mask)
		c += 2;
	    if (*(iptr += line_size) & mask)
		c += 4;
	    if (*(iptr += line_size) & mask)
		c += 010;
	    if (*(iptr += line_size) & mask)
		c += 020;
	    if (*(iptr += line_size) & mask)
		c += 040;
	    if (!(mask >>= 1)) {
		mask = 0200;
		inp++;
	    }

            if (c != oldc) {
                if (empty) {
                    while (--lcount >= 0) {

                        /* terminate record.
                           this LF is ignored by the LN03 */
                        if (ccount > 78) {
                            fputc('\n', prn_stream);
                            ccount = 0;
                        }
                        /* terminate previous line */
                        fputc('-', prn_stream);
                        ccount++;
                    }
                    empty = lcount = 0;
                }
                if (count > 3) {
                    /* use run length encoding */
                    if (ccount > 74) {
                        fputc('\n', prn_stream);
                        ccount = 0;
                    }
                    /* we know lines will not exceed 10000 pixels */
                    ccount = ccount + 3 + (count > 9)
                                        + (count > 99)
                                        + (count > 999);
                    fprintf(prn_stream, "!%d%c", count, oldc);
                }
                else {
                    while (--count >= 0) {
                        if (ccount > 78) {
                            fputc('\n', prn_stream);
                            ccount = 0;
                        }
                        fputc(oldc, prn_stream);
                        ccount++;
                    }
                }
                oldc = c;
                count = 0;
            }
        }
        if (c != 077) {
           if (count > 3) {
                /* use run length encoding */
                if (ccount > 74) {
                    fputc('\n', prn_stream);
                    ccount = 0;
                }
                /* we know lines will not exceed 10000 pixels */
                ccount = ccount + 3 + (count > 9)
                                    + (count > 99)
                                    + (count > 999);
                fprintf(prn_stream, "!%d%c", count, c);
            }
            else {
                while (--count >= 0) {
                    if (ccount > 78) {
                        fputc('\n', prn_stream);
                        ccount = 0;
                    }
                    fputc(c, prn_stream);
                    ccount++;
                }
            }
        }
    }

    /* leave sixel graphics mode, eject page */
    if (ccount + strlen(eject) > 79) fputc('\n', prn_stream);
    fputs(eject, prn_stream);
    fflush(prn_stream);

    gs_free((char *)in, in_size, 1, "sixel_print_page");

    return(0);
}
