/*
 * "$Id$"
 *
 *   Raster test program routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
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

#include "image-private.h"
#include <stdlib.h>
#include <cups/string.h>


/*
 * Test PS commands and header...
 */

static const char *test_code =
"<<"
"/MediaClass(Media Class)"
"/MediaColor((Media Color))"
"/MediaType(Media\\\\Type)"
"/OutputType<416263>"
"/AdvanceDistance 1000"
"/AdvanceMedia 1"
"/Collate false"
"/CutMedia 2"
"/Duplex true"
"/HWResolution[100 200]"
"/InsertSheet true"
"/Jog 3"
"/LeadingEdge 1"
"/ManualFeed true"
"/MediaPosition 8#777"
"/MediaWeight 16#fe01"
"/MirrorPrint true"
"/NegativePrint true"
"/NumCopies 1"
"/Orientation 1"
"/OutputFaceUp true"
"/PageSize[612 792.1]"
"/Separations true"
"/TraySwitch true"
"/Tumble true"
"/cupsMediaType 2"
"/cupsColorOrder 1"
"/cupsColorSpace 1"
"/cupsCompression 1"
"/cupsRowCount 1"
"/cupsRowFeed 1"
"/cupsRowStep 1"
"/cupsBorderlessScalingFactor 1.001"
"/cupsInteger0 1"
"/cupsInteger1 2"
"/cupsInteger2 3"
"/cupsInteger3 4"
"/cupsInteger4 5"
"/cupsInteger5 6"
"/cupsInteger6 7"
"/cupsInteger7 8"
"/cupsInteger8 9"
"/cupsInteger9 10"
"/cupsInteger10 11"
"/cupsInteger11 12"
"/cupsInteger12 13"
"/cupsInteger13 14"
"/cupsInteger14 15"
"/cupsInteger15 16"
"/cupsReal0 1.1"
"/cupsReal1 2.1"
"/cupsReal2 3.1"
"/cupsReal3 4.1"
"/cupsReal4 5.1"
"/cupsReal5 6.1"
"/cupsReal6 7.1"
"/cupsReal7 8.1"
"/cupsReal8 9.1"
"/cupsReal9 10.1"
"/cupsReal10 11.1"
"/cupsReal11 12.1"
"/cupsReal12 13.1"
"/cupsReal13 14.1"
"/cupsReal14 15.1"
"/cupsReal15 16.1"
"/cupsString0(1)"
"/cupsString1(2)"
"/cupsString2(3)"
"/cupsString3(4)"
"/cupsString4(5)"
"/cupsString5(6)"
"/cupsString6(7)"
"/cupsString7(8)"
"/cupsString8(9)"
"/cupsString9(10)"
"/cupsString10(11)"
"/cupsString11(12)"
"/cupsString12(13)"
"/cupsString13(14)"
"/cupsString14(15)"
"/cupsString15(16)"
"/cupsMarkerType(Marker Type)"
"/cupsRenderingIntent(Rendering Intent)"
"/cupsPageSizeName(Letter)"
"/cupsPreferredBitsPerColor 17"
">> setpagedevice";

static cups_page_header2_t test_header =
{
  "Media Class",			/* MediaClass */
  "(Media Color)",			/* MediaColor */
  "Media\\Type",			/* MediaType */
  "Abc",				/* OutputType */
  1000,					/* AdvanceDistance */
  CUPS_ADVANCE_FILE,			/* AdvanceMedia */
  CUPS_FALSE,				/* Collate */
  CUPS_CUT_JOB,				/* CutMedia */
  CUPS_TRUE,				/* Duplex */
  { 100, 200 },				/* HWResolution */
  { 0, 0, 0, 0 },			/* ImagingBoundingBox */
  CUPS_TRUE,				/* InsertSheet */
  CUPS_JOG_SET,				/* Jog */
  CUPS_EDGE_RIGHT,			/* LeadingEdge */
  { 0, 0 },				/* Margins */
  CUPS_TRUE,				/* ManualFeed */
  0777,					/* MediaPosition */
  0xfe01,				/* MediaWeight */
  CUPS_TRUE,				/* MirrorPrint */
  CUPS_TRUE,				/* NegativePrint */
  1,					/* NumCopies */
  CUPS_ORIENT_90,			/* Orientation */
  CUPS_TRUE,				/* OutputFaceUp */
  { 612, 792 },				/* PageSize */
  CUPS_TRUE,				/* Separations */
  CUPS_TRUE,				/* TraySwitch */
  CUPS_TRUE,				/* Tumble */
  0,					/* cupsWidth */
  0,					/* cupsHeight */
  2,					/* cupsMediaType */
  0,					/* cupsBitsPerColor */
  0,					/* cupsBitsPerPixel */
  0,					/* cupsBytesPerLine */
  CUPS_ORDER_BANDED,			/* cupsColorOrder */
  CUPS_CSPACE_RGB,			/* cupsColorSpace */
  1,					/* cupsCompression */
  1,					/* cupsRowCount */
  1,					/* cupsRowFeed */
  1,					/* cupsRowStep */
  0,					/* cupsNumColors */
  1.001,				/* cupsBorderlessScalingFactor */
  { 612.0, 792.1 },			/* cupsPageSize */
  { 0.0, 0.0, 0.0, 0.0 },		/* cupsImagingBBox */
  { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 },
					/* cupsInteger[16] */
  { 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1, 9.1, 10.1, 11.1, 12.1, 13.1,
    14.1, 15.1, 16.1 },			/* cupsReal[16] */
  { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
    "14", "15", "16" },			/* cupsString[16] */
  "Marker Type",			/* cupsMarkerType */
  "Rendering Intent",			/* cupsRenderingIntent */
  "Letter"				/* cupsPageSizeName */
};


/*
 * 'main()' - Test the raster read/write functions.
 */

int					/* O - Exit status */
main(void)
{
  int			i, page, x, y;	/* Looping vars */
  FILE			*fp;		/* Raster file */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  unsigned char		data[2048];	/* Raster data */
  int			preferred_bits;	/* Preferred bits */


 /*
  * Test PS exec code...
  */

  fputs("_cupsRasterExecPS: ", stdout);
  fflush(stdout);

  memset(&header, 0, sizeof(header));
  header.Collate = CUPS_TRUE;
  preferred_bits = 0;

  if (_cupsRasterExecPS(&header, &preferred_bits, test_code))
    puts("FAIL (error from function)");
  else if (preferred_bits != 17 || memcmp(&header, &test_header, sizeof(header)))
  {
    puts("FAIL (bad header)");

    if (preferred_bits != 17)
      printf("    cupsPreferredBitsPerColor %d, expected 17\n",
             preferred_bits);

    if (strcmp(header.MediaClass, test_header.MediaClass))
      printf("    MediaClass (%s), expected (%s)\n", header.MediaClass,
             test_header.MediaClass);

    if (strcmp(header.MediaColor, test_header.MediaColor))
      printf("    MediaColor (%s), expected (%s)\n", header.MediaColor,
             test_header.MediaColor);

    if (strcmp(header.MediaType, test_header.MediaType))
      printf("    MediaType (%s), expected (%s)\n", header.MediaType,
             test_header.MediaType);

    if (strcmp(header.OutputType, test_header.OutputType))
      printf("    OutputType (%s), expected (%s)\n", header.OutputType,
             test_header.OutputType);

    if (header.AdvanceDistance != test_header.AdvanceDistance)
      printf("    AdvanceDistance %d, expected %d\n", header.AdvanceDistance,
             test_header.AdvanceDistance);

    if (header.AdvanceMedia != test_header.AdvanceMedia)
      printf("    AdvanceMedia %d, expected %d\n", header.AdvanceMedia,
             test_header.AdvanceMedia);

    if (header.Collate != test_header.Collate)
      printf("    Collate %d, expected %d\n", header.Collate,
             test_header.Collate);

    if (header.CutMedia != test_header.CutMedia)
      printf("    CutMedia %d, expected %d\n", header.CutMedia,
             test_header.CutMedia);

    if (header.Duplex != test_header.Duplex)
      printf("    Duplex %d, expected %d\n", header.Duplex,
             test_header.Duplex);

    if (header.HWResolution[0] != test_header.HWResolution[0] ||
        header.HWResolution[1] != test_header.HWResolution[1])
      printf("    HWResolution [%d %d], expected [%d %d]\n",
             header.HWResolution[0], header.HWResolution[1],
             test_header.HWResolution[0], test_header.HWResolution[1]);

    if (memcmp(header.ImagingBoundingBox, test_header.ImagingBoundingBox,
               sizeof(header.ImagingBoundingBox)))
      printf("    ImagingBoundingBox [%d %d %d %d], expected [%d %d %d %d]\n",
             header.ImagingBoundingBox[0],
             header.ImagingBoundingBox[1],
             header.ImagingBoundingBox[2],
             header.ImagingBoundingBox[3],
             test_header.ImagingBoundingBox[0],
             test_header.ImagingBoundingBox[1],
             test_header.ImagingBoundingBox[2],
             test_header.ImagingBoundingBox[3]);

    if (header.InsertSheet != test_header.InsertSheet)
      printf("    InsertSheet %d, expected %d\n", header.InsertSheet,
             test_header.InsertSheet);

    if (header.Jog != test_header.Jog)
      printf("    Jog %d, expected %d\n", header.Jog,
             test_header.Jog);

    if (header.LeadingEdge != test_header.LeadingEdge)
      printf("    LeadingEdge %d, expected %d\n", header.LeadingEdge,
             test_header.LeadingEdge);

    if (header.Margins[0] != test_header.Margins[0] ||
        header.Margins[1] != test_header.Margins[1])
      printf("    Margins [%d %d], expected [%d %d]\n",
             header.Margins[0], header.Margins[1],
             test_header.Margins[0], test_header.Margins[1]);

    if (header.ManualFeed != test_header.ManualFeed)
      printf("    ManualFeed %d, expected %d\n", header.ManualFeed,
             test_header.ManualFeed);

    if (header.MediaPosition != test_header.MediaPosition)
      printf("    MediaPosition %d, expected %d\n", header.MediaPosition,
             test_header.MediaPosition);

    if (header.MediaWeight != test_header.MediaWeight)
      printf("    MediaWeight %d, expected %d\n", header.MediaWeight,
             test_header.MediaWeight);

    if (header.MirrorPrint != test_header.MirrorPrint)
      printf("    MirrorPrint %d, expected %d\n", header.MirrorPrint,
             test_header.MirrorPrint);

    if (header.NegativePrint != test_header.NegativePrint)
      printf("    NegativePrint %d, expected %d\n", header.NegativePrint,
             test_header.NegativePrint);

    if (header.NumCopies != test_header.NumCopies)
      printf("    NumCopies %d, expected %d\n", header.NumCopies,
             test_header.NumCopies);

    if (header.Orientation != test_header.Orientation)
      printf("    Orientation %d, expected %d\n", header.Orientation,
             test_header.Orientation);

    if (header.OutputFaceUp != test_header.OutputFaceUp)
      printf("    OutputFaceUp %d, expected %d\n", header.OutputFaceUp,
             test_header.OutputFaceUp);

    if (header.PageSize[0] != test_header.PageSize[0] ||
        header.PageSize[1] != test_header.PageSize[1])
      printf("    PageSize [%d %d], expected [%d %d]\n",
             header.PageSize[0], header.PageSize[1],
             test_header.PageSize[0], test_header.PageSize[1]);

    if (header.Separations != test_header.Separations)
      printf("    Separations %d, expected %d\n", header.Separations,
             test_header.Separations);

    if (header.TraySwitch != test_header.TraySwitch)
      printf("    TraySwitch %d, expected %d\n", header.TraySwitch,
             test_header.TraySwitch);

    if (header.Tumble != test_header.Tumble)
      printf("    Tumble %d, expected %d\n", header.Tumble,
             test_header.Tumble);

    if (header.cupsWidth != test_header.cupsWidth)
      printf("    cupsWidth %d, expected %d\n", header.cupsWidth,
             test_header.cupsWidth);

    if (header.cupsHeight != test_header.cupsHeight)
      printf("    cupsHeight %d, expected %d\n", header.cupsHeight,
             test_header.cupsHeight);

    if (header.cupsMediaType != test_header.cupsMediaType)
      printf("    cupsMediaType %d, expected %d\n", header.cupsMediaType,
             test_header.cupsMediaType);

    if (header.cupsBitsPerColor != test_header.cupsBitsPerColor)
      printf("    cupsBitsPerColor %d, expected %d\n", header.cupsBitsPerColor,
             test_header.cupsBitsPerColor);

    if (header.cupsBitsPerPixel != test_header.cupsBitsPerPixel)
      printf("    cupsBitsPerPixel %d, expected %d\n", header.cupsBitsPerPixel,
             test_header.cupsBitsPerPixel);

    if (header.cupsBytesPerLine != test_header.cupsBytesPerLine)
      printf("    cupsBytesPerLine %d, expected %d\n", header.cupsBytesPerLine,
             test_header.cupsBytesPerLine);

    if (header.cupsColorOrder != test_header.cupsColorOrder)
      printf("    cupsColorOrder %d, expected %d\n", header.cupsColorOrder,
             test_header.cupsColorOrder);

    if (header.cupsColorSpace != test_header.cupsColorSpace)
      printf("    cupsColorSpace %d, expected %d\n", header.cupsColorSpace,
             test_header.cupsColorSpace);

    if (header.cupsCompression != test_header.cupsCompression)
      printf("    cupsCompression %d, expected %d\n", header.cupsCompression,
             test_header.cupsCompression);

    if (header.cupsRowCount != test_header.cupsRowCount)
      printf("    cupsRowCount %d, expected %d\n", header.cupsRowCount,
             test_header.cupsRowCount);

    if (header.cupsRowFeed != test_header.cupsRowFeed)
      printf("    cupsRowFeed %d, expected %d\n", header.cupsRowFeed,
             test_header.cupsRowFeed);

    if (header.cupsRowStep != test_header.cupsRowStep)
      printf("    cupsRowStep %d, expected %d\n", header.cupsRowStep,
             test_header.cupsRowStep);

    if (header.cupsNumColors != test_header.cupsNumColors)
      printf("    cupsNumColors %d, expected %d\n", header.cupsNumColors,
             test_header.cupsNumColors);

    if (header.cupsBorderlessScalingFactor !=
            test_header.cupsBorderlessScalingFactor)
      printf("    cupsBorderlessScalingFactor %g, expected %g\n",
             header.cupsBorderlessScalingFactor,
             test_header.cupsBorderlessScalingFactor);

    if (header.cupsPageSize[0] != test_header.cupsPageSize[0] ||
        header.cupsPageSize[1] != test_header.cupsPageSize[1])
      printf("    cupsPageSize [%g %g], expected [%g %g]\n",
             header.cupsPageSize[0], header.cupsPageSize[1],
             test_header.cupsPageSize[0], test_header.cupsPageSize[1]);

    if (header.cupsImagingBBox[0] != test_header.cupsImagingBBox[0] ||
        header.cupsImagingBBox[1] != test_header.cupsImagingBBox[1] ||
        header.cupsImagingBBox[2] != test_header.cupsImagingBBox[2] ||
        header.cupsImagingBBox[3] != test_header.cupsImagingBBox[3])
      printf("    cupsImagingBBox [%g %g %g %g], expected [%g %g %g %g]\n",
             header.cupsImagingBBox[0], header.cupsImagingBBox[1],
             header.cupsImagingBBox[2], header.cupsImagingBBox[3],
             test_header.cupsImagingBBox[0], test_header.cupsImagingBBox[1],
             test_header.cupsImagingBBox[2], test_header.cupsImagingBBox[3]);

    for (i = 0; i < 16; i ++)
      if (header.cupsInteger[i] != test_header.cupsInteger[i])
	printf("    cupsInteger%d %d, expected %d\n", i, header.cupsInteger[i],
               test_header.cupsInteger[i]);

    for (i = 0; i < 16; i ++)
      if (header.cupsReal[i] != test_header.cupsReal[i])
	printf("    cupsReal%d %g, expected %g\n", i, header.cupsReal[i],
               test_header.cupsReal[i]);

    for (i = 0; i < 16; i ++)
      if (strcmp(header.cupsString[i], test_header.cupsString[i]))
	printf("    cupsString%d (%s), expected (%s)\n", i,
	       header.cupsString[i], test_header.cupsString[i]);

    if (strcmp(header.cupsMarkerType, test_header.cupsMarkerType))
      printf("    cupsMarkerType (%s), expected (%s)\n", header.cupsMarkerType,
             test_header.cupsMarkerType);

    if (strcmp(header.cupsRenderingIntent, test_header.cupsRenderingIntent))
      printf("    cupsRenderingIntent (%s), expected (%s)\n",
             header.cupsRenderingIntent,
             test_header.cupsRenderingIntent);

    if (strcmp(header.cupsPageSizeName, test_header.cupsPageSizeName))
      printf("    cupsPageSizeName (%s), expected (%s)\n",
             header.cupsPageSizeName,
             test_header.cupsPageSizeName);
  }
  else
    puts("PASS");
    
 /*
  * Test writing...
  */

  if ((fp = fopen("test.raster", "wb")) == NULL)
  {
    perror("Unable to create test.raster");
    return (1);
  }

  if ((r = cupsRasterOpen(fileno(fp), CUPS_RASTER_WRITE)) == NULL)
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

 /*
  * Test reading...
  */

  if ((fp = fopen("test.raster", "rb")) == NULL)
  {
    perror("Unable to open test.raster");
    return (1);
  }

  if ((r = cupsRasterOpen(fileno(fp), CUPS_RASTER_READ)) == NULL)
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
 * End of "$Id$".
 */
