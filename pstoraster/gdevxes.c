/* Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.  All rights reserved.

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
gdevxes.c
Ghostscript driver for Xerox XES printer
  (2700, 3700, 4045, etc.)

Peter Flass - NYS LBDC <flass@lbdrscs.bitnet>
New York State Legislative Bill Drafting Commission
1450 Western Avenue, 3rd floor
Albany, NY  12203

This code is subject to the GNU General Public License

Operation: The page bitmap is scanned to determined the
actual margins.  A "graphics rectangle" is defined to
contain the included data and positioned on the page.
The bitmap is then re-read and "sixellized" by converting
each three bytes to four six-bit chunks (zero padding on
the right if necessary) and adding x'3F' to generate a
printable code.  Runs of up to 32767 identical characters
are compressed to an ascii count and a single character.

*/

#include "gdevprn.h"

/* Forward references */
private int sixel_print_page(P3(gx_device_printer *pdev,
			       FILE *prn_stream, const char *init));

/* The device descriptor */
private dev_proc_output_page(sixel_output_page);
private dev_proc_print_page(xes_print_page);
private gx_device_procs xes_procs =
  prn_procs(gdev_prn_open, sixel_output_page, gdev_prn_close);

#ifdef A4
#  define BOTTOM_MARGIN 0.5
#  define PAGE_LENGTH_PELS 3300
#else
#  define BOTTOM_MARGIN 0.4
#  define PAGE_LENGTH_PELS 3300
#endif

gx_device_printer gs_xes_device =
    prn_device(xes_procs, "xes",
	       DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	       300, 300,		/* x_dpi, y_dpi */
	       0, BOTTOM_MARGIN, 0, 0,	/* left, bottom, right, top margin */
	       1, xes_print_page);

/*
 * Initialization string: switch to graphics mode, 300 dpi
 * <ESC>+X	    Reset	soft terminal reset
 * <ESC>gw1;0,0,		Graphic window
 */

#define XES_RESET \
 "\033+X\n"
#define XES_GRAPHICS \
 "\033gw1;"

private int
xes_print_page(gx_device_printer *pdev, FILE *prn_stream)
{
    return (sixel_print_page(pdev,prn_stream,XES_RESET));
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
private int
sixel_print_page(gx_device_printer *pdev, FILE *prn_stream, const char *init)
{
    byte *buf, *b, *end;
    byte last = '\0';
    byte tmp[4];
    char run[8], *t;
    int lnum, l, line_size;
    int width, height;
    int top, bottom, left, right;
    int count = 0;

    line_size = gdev_mem_bytes_per_scan_line(pdev);
    height = pdev->height; 
    /* Default page rectangle */
    top    = pdev->height;
    left   = line_size;
    right  = 0;
    bottom = 0;

    buf = (byte *)gs_malloc(line_size, 1, "sixel_print_page");
    end = buf + line_size - 1;

    /* Check allocation */
    if (!buf) 
      return_error(gs_error_VMerror);

    /* Compute required window size */
    for (lnum=0; lnum < pdev->height; lnum++ ) {
	gdev_prn_copy_scan_lines(pdev, lnum, buf, line_size);
	for( b=buf; b<=end; b++ ) if(*b) break;
	if ( b<=end ) {
	  top  = min( top, lnum );
	  left = min( left, (int)(b-buf));
	  bottom = max( bottom, lnum );
	  for( b=end; b>=buf; b-- ) if(*b) break;
	  if ( b>=buf ) right = max( right, (int)(b-buf) );
	  } /* endif */
	} /* endfor */
	width = right - left + 1;	/* width in bytes */
	height= bottom- top  + 1;	/* height in pels */
	/* round width to multiple of 3 bytes */
	width = ( (width+2) / 3 ) * 3;
	right = min( line_size-1, left+width-1 );
	end = buf + right;		/* recompute EOL  */

    fputs( init, prn_stream );

    /* Position and size graphics window */
    fprintf( prn_stream, "%s%d,%d,%d,%d\n", 
             XES_GRAPHICS, 
             left*8, PAGE_LENGTH_PELS-top, 
             width*8, height );

    /* Print lines of graphics                 */
    for (lnum = top; lnum <= bottom; lnum++ ) {
	gdev_prn_copy_scan_lines(pdev, lnum, buf, line_size);
	for ( b=buf+left; b<=end ; ) {
	  /* grab data in 3-byte chunks   */
	  /* with zero pad at end-of-line */
	  tmp[0]=tmp[1]=tmp[2]='\0';
	  tmp[0]=*b++;
	  if (b<=end) tmp[1]=*b++;
	  if (b<=end) tmp[2]=*b++;
	  /* sixellize data */
	  tmp[3] = ( tmp[2] & 0x3F) + 0x3F;
	  tmp[2] = ( tmp[2] >> 6 |
	            (tmp[1] & 0x0F) << 2 ) + 0x3F;
	  tmp[1] = ( tmp[1] >> 4 |
	            (tmp[0] & 0x03) << 4 ) + 0x3F;
	  tmp[0] = ( tmp[0] >> 2 ) + 0x3F;
	  /* build runs of identical characters */
	  /* longest run length is 32767 bytes  */
	  for ( l=0; l<4; l++) {
	      if ( tmp[l] == last ) {
		count++;
		if (count==32767) {
		  run[sprintf(run, "%d", count)]='\0';
		  for (t=run; *t; t++)fputc( *t, prn_stream ); 
		  fputc( last, prn_stream ); 
		  last = '\0';
		  count = 0;
		  } /* end if count */
		} /* end if tmp[l] */
	      else {
		  /* emit single character or run */
		  switch (count) {
		    case 0: break;
		    case 1: fputc( last, prn_stream );
			    break;
		    default:run[sprintf(run, "%d", count)]='\0';
			    for (t=run; *t; t++) fputc( *t, prn_stream );
			    fputc( last, prn_stream );
			    break;
		    } /* end switch */
		  last = tmp[l];
		  count = 1;
		  } /* end else */
	  } /* end for l */
	} /* end for b */
    } /* end for lnum */

    /* Write final run */
    switch (count) {
      case 0: break;
      case 1: fputc( last, prn_stream );
	      break;
      default:run[sprintf(run, "%d", count)]='\0';
	      for (t=run; *t; t++) fputc( *t, prn_stream );
	      fputc( last, prn_stream );
	      break;
      } /* end switch */

    /* Eject page and reset */
    fprintf( prn_stream, "\f%s", XES_RESET );
    fflush(prn_stream);

    gs_free((char *)buf, line_size, 1, "sixel_print_page");

    return(0);
}
