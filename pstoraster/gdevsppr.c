/* Copyright (C) 1992, 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevsppr.c */
/* SPARCprinter driver for Ghostscript */
#include "gdevprn.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioccom.h>
#include <unbdev/lpviio.h>

/*
   Thanks to Martin Schulte (schulte@thp.Uni-Koeln.DE) for contributing
   this driver to Ghostscript.  He supplied the following notes.

The device-driver (normally) returns two differnt types of Error-Conditions,
FATALS and WARNINGS. In case of a fatal, the print routine returns -1, in
case of a warning (such as paper out), a string describing the error is
printed to stdout and the output-operation is repeated after five seconds.

A problem is that not all possible errors seem to return the correct error,
under some circumstance I get the same response as if an error repeated,
that's why there is this the strange code about guessing the error.

I didn't implement asynchronous IO (yet), because "`normal"' multipage-
printings like TEX-Output seem to be printed with the maximum speed whereas
drawings normally occur as one-page outputs, where asynchronous IO doesn't
help anyway.
*/

private dev_proc_open_device(sparc_open);
private dev_proc_print_page(sparc_print_page);

#define SPARC_MARGINS_A4	0.15, 0.12, 0.12, 0.15
#define SPARC_MARGINS_LETTER	0.15, 0.12, 0.12, 0.15

gx_device_procs prn_sparc_procs =
  prn_procs(sparc_open, gdev_prn_output_page, gdev_prn_close);

gx_device_printer far_data gs_sparc_device =
prn_device(prn_sparc_procs,
  "sparc",
  DEFAULT_WIDTH_10THS,DEFAULT_HEIGHT_10THS,
  400,400,
  0,0,0,0,
  1,
  sparc_print_page);

/* Open the printer, and set the margins. */
private int
sparc_open(gx_device *pdev)
{	/* Change the margins according to the paper size. */
	const float _ds *m;
	static const float m_a4[4] = { SPARC_MARGINS_A4 };
	static const float m_letter[4] = { SPARC_MARGINS_LETTER };

	m = (pdev->height / pdev->y_pixels_per_inch >= 11.1 ? m_a4 : m_letter);
	gx_device_set_margins(pdev, m, true);
	return gdev_prn_open(pdev);
}

char *errmsg[]={
  "EMOTOR",
  "EROS",
  "EFUSER",
  "XEROFAIL",
  "ILCKOPEN",
  "NOTRAY",
  "NOPAPR",
  "XITJAM",
  "MISFEED",
  "WDRUMX",
  "WDEVEX",
  "NODRUM",
  "NODEVE",
  "EDRUMX",
  "EDEVEX",
  "ENGCOLD",
  "TIMEOUT",
  "EDMA",
  "ESERIAL"
  };

private char *
err_code_string(int err_code)
  {
  if ((err_code<EMOTOR)||(err_code>ESERIAL))
    {
    char buffer[80];
    sprintf(buffer,"err_code out of range: %d",err_code);
    return buffer;
    }
  return errmsg[err_code];
  }

int warning=0;

private int
sparc_print_page(gx_device_printer *pdev, FILE *prn)
  {
  struct lpvi_page lpvipage;
  struct lpvi_err lpvierr;
  char *out_buf;
  int out_size;
  if (ioctl(fileno(prn),LPVIIOC_GETPAGE,&lpvipage)!=0)
    {
    fprintf(stderr,"sparc_print_page: LPVIIOC_GETPAGE failed\n");
    return -1;
    }
  lpvipage.bitmap_width=gdev_mem_bytes_per_scan_line((gx_device *)pdev);
  lpvipage.page_width=lpvipage.bitmap_width*8;
  lpvipage.page_length=pdev->height;
  lpvipage.resolution= (pdev->x_pixels_per_inch == 300 ? 300 : 400);
  if (ioctl(fileno(prn),LPVIIOC_SETPAGE,&lpvipage)!=0)
    {
    fprintf(stderr,"sparc_print_page: LPVIIOC_SETPAGE failed\n");
    return -1;
    }
  out_size=lpvipage.bitmap_width*lpvipage.page_length;
  out_buf=gs_malloc(out_size,1,"sparc_print_page: out_buf");
  gdev_prn_copy_scan_lines(pdev,0,out_buf,out_size);
  while (write(fileno(prn),out_buf,out_size)!=out_size)
    {
    if (ioctl(fileno(prn),LPVIIOC_GETERR,&lpvierr)!=0)
      {
      fprintf(stderr,"sparc_print_page: LPVIIOC_GETERR failed\n");
      return -1;
      }
    switch (lpvierr.err_type)
      {
      case 0:
	if (warning==0)
          {
          fprintf(stderr,
            "sparc_print_page: Printer Problem with unknown reason...");
          fflush(stderr);
          warning=1;
          }
	sleep(5);
	break;
      case ENGWARN:
	fprintf(stderr,
          "sparc_print_page: Printer-Warning: %s...",
          err_code_string(lpvierr.err_code));
	fflush(stderr);
	warning=1;
	sleep(5);
	break;
      case ENGFATL:
	fprintf(stderr,
          "sparc_print_page: Printer-Fatal: %s\n",
          err_code_string(lpvierr.err_code));
	return -1;
      case EDRVR:
	fprintf(stderr,
          "sparc_print_page: Interface/driver error: %s\n",
          err_code_string(lpvierr.err_code));
	return -1;
      default:
	fprintf(stderr,
          "sparc_print_page: Unknown err_type=%d(err_code=%d)\n",
          lpvierr.err_type,lpvierr.err_code);
	return -1;
      }
    }
  if (warning==1)
    {
    fprintf(stderr,"OK.\n");
    warning=0;
    }
  gs_free(out_buf,out_size,1,"sparc_print_page: out_buf");
  return 0;
  }
