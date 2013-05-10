/*
 * "$Id$"
 *
 *   CUPS raster to PWG raster format filter for CUPS.
 *
 *   Copyright 2011 Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright law.
 *   Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry for filter.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/language-private.h>
#include <cups/raster.h>
#include <cups/string-private.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * 'main()' - Main entry for filter.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			fd;		/* Raster file */
  cups_raster_t		*inras,		/* Input raster stream */
			*outras;	/* Output raster stream */
  cups_page_header2_t	inheader,	/* Input raster page header */
			outheader;	/* Output raster page header */
  int			y;		/* Current line */
  unsigned char		*line;		/* Line buffer */
  int			page = 0,	/* Current page */
			page_width,	/* Actual page width */
			page_height,	/* Actual page height */
			page_top,	/* Top margin */
			page_bottom,	/* Bottom margin */
			page_left,	/* Left margin */
			linesize,	/* Bytes per line */
			lineoffset;	/* Offset into line */
  unsigned char		white;		/* White pixel */


  if (argc < 6 || argc > 7)
  {
    puts("Usage: rastertopwg job user title copies options [filename]");
    return (1);
  }
  else if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: Unable to open print file");
      return (1);
    }
  }
  else
    fd = 0;

  inras  = cupsRasterOpen(fd, CUPS_RASTER_READ);
  outras = cupsRasterOpen(1, CUPS_RASTER_WRITE_PWG);

  while (cupsRasterReadHeader2(inras, &inheader))
  {
   /*
    * Compute the real raster size...
    */

    page ++;

    fprintf(stderr, "PAGE: %d %d\n", page, inheader.NumCopies);

    page_width  = (int)(inheader.cupsPageSize[0] * inheader.HWResolution[0] /
                        72.0);
    page_height = (int)(inheader.cupsPageSize[1] * inheader.HWResolution[1] /
                        72.0);
    page_left   = (int)(inheader.cupsImagingBBox[0] *
                        inheader.HWResolution[0] / 72.0);
    page_bottom = (int)(inheader.cupsImagingBBox[1] *
                        inheader.HWResolution[1] / 72.0);
    page_top    = page_height - page_bottom - inheader.cupsHeight;
    linesize    = (page_width * inheader.cupsBitsPerPixel + 7) / 8;
    lineoffset  = page_left * inheader.cupsBitsPerPixel / 8; /* Round down */

    switch (inheader.cupsColorSpace)
    {
      case CUPS_CSPACE_W :
      case CUPS_CSPACE_RGB :
      case CUPS_CSPACE_SW :
      case CUPS_CSPACE_SRGB :
      case CUPS_CSPACE_ADOBERGB :
          white = 255;
	  break;

      case CUPS_CSPACE_K :
      case CUPS_CSPACE_CMYK :
      case CUPS_CSPACE_DEVICE1 :
      case CUPS_CSPACE_DEVICE2 :
      case CUPS_CSPACE_DEVICE3 :
      case CUPS_CSPACE_DEVICE4 :
      case CUPS_CSPACE_DEVICE5 :
      case CUPS_CSPACE_DEVICE6 :
      case CUPS_CSPACE_DEVICE7 :
      case CUPS_CSPACE_DEVICE8 :
      case CUPS_CSPACE_DEVICE9 :
      case CUPS_CSPACE_DEVICEA :
      case CUPS_CSPACE_DEVICEB :
      case CUPS_CSPACE_DEVICEC :
      case CUPS_CSPACE_DEVICED :
      case CUPS_CSPACE_DEVICEE :
      case CUPS_CSPACE_DEVICEF :
          white = 0;
	  break;
      default :
	  _cupsLangPrintFilter(stderr, "ERROR", _("Unsupported raster data."));
	  fprintf(stderr, "DEBUG: Unsupported cupsColorSpace %d on page %d.\n",
	          inheader.cupsColorSpace, page);
	  return (1);
    }

    if (inheader.cupsColorOrder != CUPS_ORDER_CHUNKED)
    {
      _cupsLangPrintFilter(stderr, "ERROR", _("Unsupported raster data."));
      fprintf(stderr, "DEBUG: Unsupported cupsColorOrder %d on page %d.\n",
              inheader.cupsColorOrder, page);
      return (1);
    }

    if (inheader.cupsBitsPerPixel != 1 &&
        inheader.cupsBitsPerColor != 8 && inheader.cupsBitsPerColor != 16)
    {
      _cupsLangPrintFilter(stderr, "ERROR", _("Unsupported raster data."));
      fprintf(stderr, "DEBUG: Unsupported cupsBitsPerColor %d on page %d.\n",
              inheader.cupsBitsPerColor, page);
      return (1);
    }

    memcpy(&outheader, &inheader, sizeof(outheader));
    outheader.cupsWidth  = page_width;
    outheader.cupsHeight       = page_height;
    outheader.cupsBytesPerLine = linesize;

    if (!cupsRasterWriteHeader2(outras, &outheader))
    {
      _cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
      fprintf(stderr, "DEBUG: Unable to write header for page %d.\n", page);
      return (1);
    }

   /*
    * Copy raster data...
    */

    line = malloc(linesize);

    memset(line, white, linesize);
    for (y = page_top; y > 0; y --)
      if (!cupsRasterWritePixels(outras, line, outheader.cupsBytesPerLine))
      {
	_cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
	fprintf(stderr, "DEBUG: Unable to write line %d for page %d.\n",
	        page_top - y + 1, page);
	return (1);
      }

    for (y = inheader.cupsHeight; y > 0; y --)
    {
      cupsRasterReadPixels(inras, line + lineoffset, inheader.cupsBytesPerLine);
      if (!cupsRasterWritePixels(outras, line, outheader.cupsBytesPerLine))
      {
	_cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
	fprintf(stderr, "DEBUG: Unable to write line %d for page %d.\n",
	        inheader.cupsHeight - y + page_top + 1, page);
	return (1);
      }
    }

    memset(line, white, linesize);
    for (y = page_top; y > 0; y --)
      if (!cupsRasterWritePixels(outras, line, outheader.cupsBytesPerLine))
      {
	_cupsLangPrintFilter(stderr, "ERROR", _("Error sending raster data."));
	fprintf(stderr, "DEBUG: Unable to write line %d for page %d.\n",
	        page_bottom - y + page_top + inheader.cupsHeight + 1, page);
	return (1);
      }

    free(line);
  }

  cupsRasterClose(inras);
  if (fd)
    close(fd);

  cupsRasterClose(outras);

  return (0);
}


/*
 * End of "$Id$".
 */
