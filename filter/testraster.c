/*
 * "$Id: testraster.c,v 1.1.2.3 2003/01/07 18:27:01 mike Exp $"
 *
 *   Raster test program routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights for the CUPS Raster source
 *   files are outlined in the GNU Library General Public License, located
 *   in the "pstoraster" directory.  If this file is missing or damaged
 *   please contact Easy Software Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "raster.h"
#include <stdlib.h>
#include <cups/string.h>


/*
 * 'main()' - Test the raster read/write functions.
 */

int					/* O - Exit status */
main(void)
{
  int			page, x, y;	/* Looping vars */
  FILE			*fp;		/* Raster file */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  unsigned char		data[2048];	/* Raster data */


  if ((fp = fopen("test.raster", "wb")) == NULL)
  {
    perror("Unable to create test.raster");
    return (1);
  }

  if ((r = cupsRasterOpen(fp, CUPS_RASTER_WRITE)) == NULL)
  {
    perror("Unable to create raster output stream");
    fclose(fp);
    return (1);
  }

  for (page = 0; page < 4; page ++)
  {
    memset(&header, 0, sizeof(header));
    header.cupsWidth        = 256;
    header.cupsHeight       = 256;
    header.cupsBytesPerLine = 256;

    if (page & 1)
    {
      header.cupsBytesPerLine *= 2;
      header.cupsColorSpace = CUPS_CSPACE_CMYK;
      header.cupsColorOrder = CUPS_ORDER_CHUNKED;
    }
    else
    {
      header.cupsColorSpace = CUPS_CSPACE_K;
      header.cupsColorOrder = CUPS_ORDER_BANDED;
    }

    if (page & 2)
    {
      header.cupsBytesPerLine *= 2;
      header.cupsBitsPerColor = 16;
      header.cupsBitsPerPixel = (page & 1) ? 64 : 16;
    }
    else
    {
      header.cupsBitsPerColor = 8;
      header.cupsBitsPerPixel = (page & 1) ? 32 : 8;
    }

    cupsRasterWriteHeader2(r, &header);

    memset(data, 0, header.cupsBytesPerLine);
    for (y = 0; y < 64; y ++)
      cupsRasterWritePixels(r, data, header.cupsBytesPerLine);

    for (x = 0; x < header.cupsBytesPerLine; x ++)
      data[x] = x;

    for (y = 0; y < 64; y ++)
      cupsRasterWritePixels(r, data, header.cupsBytesPerLine);

    memset(data, 255, header.cupsBytesPerLine);
    for (y = 0; y < 64; y ++)
      cupsRasterWritePixels(r, data, header.cupsBytesPerLine);

    for (x = 0; x < header.cupsBytesPerLine; x ++)
      data[x] = x / 4;

    for (y = 0; y < 64; y ++)
      cupsRasterWritePixels(r, data, header.cupsBytesPerLine);
  }

  cupsRasterClose(r);
  fclose(fp);

  if ((fp = fopen("test.raster", "rb")) == NULL)
  {
    perror("Unable to open test.raster");
    return (1);
  }

  if ((r = cupsRasterOpen(fp, CUPS_RASTER_READ)) == NULL)
  {
    perror("Unable to create raster input stream");
    fclose(fp);
    return (1);
  }

  for (page = 0; page < 4; page ++)
  {
    cupsRasterReadHeader2(r, &header);

    printf("Page %d:\n", page + 1);
    printf("    cupsWidth        = %d\n", header.cupsWidth);
    printf("    cupsHeight       = %d\n", header.cupsHeight);
    printf("    cupsBitsPerColor = %d\n", header.cupsBitsPerColor);
    printf("    cupsBitsPerPixel = %d\n", header.cupsBitsPerPixel);
    printf("    cupsColorSpace   = %d\n", header.cupsColorSpace);
    printf("    cupsColorOrder   = %d\n", header.cupsColorOrder);
    printf("    cupsBytesPerLine = %d\n", header.cupsBytesPerLine);

    for (y = 0; y < 64; y ++)
    {
      cupsRasterReadPixels(r, data, header.cupsBytesPerLine);

      if (data[0] != 0 || memcmp(data, data + 1, header.cupsBytesPerLine - 1))
        printf("    RASTER LINE %d CORRUPT AT %d (%02X instead of 00!)\n",
	       y, x, data[x]);
    }

    for (y = 0; y < 64; y ++)
    {
      cupsRasterReadPixels(r, data, header.cupsBytesPerLine);

      for (x = 0; x < header.cupsBytesPerLine; x ++)
        if (data[x] != (x & 255))
	  break;

      if (x < header.cupsBytesPerLine)
        printf("    RASTER LINE %d CORRUPT AT %d (%02X instead of %02X!)\n",
	       y + 64, x, data[x], x & 255);
    }

    for (y = 0; y < 64; y ++)
    {
      cupsRasterReadPixels(r, data, header.cupsBytesPerLine);

      if (data[0] != 255 || memcmp(data, data + 1, header.cupsBytesPerLine - 1))
        printf("    RASTER LINE %d CORRUPT AT %d (%02X instead of FF!)\n",
	       y + 128, x, data[x]);
    }

    for (y = 0; y < 64; y ++)
    {
      cupsRasterReadPixels(r, data, header.cupsBytesPerLine);

      for (x = 0; x < header.cupsBytesPerLine; x ++)
        if (data[x] != ((x / 4) & 255))
	  break;

      if (x < header.cupsBytesPerLine)
        printf("    RASTER LINE %d CORRUPT AT %d (%02X instead of %02X!)\n",
	       y + 192, x, data[x], (x / 4) & 255);
    }
  }

  cupsRasterClose(r);
  fclose(fp);

  return (0);
}


/*
 * End of "$Id: testraster.c,v 1.1.2.3 2003/01/07 18:27:01 mike Exp $".
 */
