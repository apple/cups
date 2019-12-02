/*
 * Raster test program routines for CUPS.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <cups/raster-private.h>
#include <math.h>


/*
 * Local functions...
 */

static int	do_ras_file(const char *filename);
static int	do_raster_tests(cups_mode_t mode);
static void	print_changes(cups_page_header2_t *header, cups_page_header2_t *expected);


/*
 * 'main()' - Test the raster functions.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int	errors = 0;			/* Number of errors */


  if (argc == 1)
  {
    errors += do_raster_tests(CUPS_RASTER_WRITE);
    errors += do_raster_tests(CUPS_RASTER_WRITE_COMPRESSED);
    errors += do_raster_tests(CUPS_RASTER_WRITE_PWG);
    errors += do_raster_tests(CUPS_RASTER_WRITE_APPLE);
  }
  else
  {
    int			i;		/* Looping var */

    for (i = 1; i < argc; i ++)
      errors += do_ras_file(argv[i]);
  }

  return (errors);
}


/*
 * 'do_ras_file()' - Test reading of a raster file.
 */

static int				/* O - Number of errors */
do_ras_file(const char *filename)	/* I - Filename */
{
  unsigned		y;		/* Looping vars */
  int			fd;		/* File descriptor */
  cups_raster_t		*ras;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  unsigned char		*data;		/* Raster data */
  int			errors = 0;	/* Number of errors */
  unsigned		pages = 0;	/* Number of pages */


  if ((fd = open(filename, O_RDONLY)) < 0)
  {
    printf("%s: %s\n", filename, strerror(errno));
    return (1);
  }

  if ((ras = cupsRasterOpen(fd, CUPS_RASTER_READ)) == NULL)
  {
    printf("%s: cupsRasterOpen failed.\n", filename);
    close(fd);
    return (1);
  }

  printf("%s:\n", filename);

  while (cupsRasterReadHeader2(ras, &header))
  {
    pages ++;
    data = malloc(header.cupsBytesPerLine);

    printf("    Page %u: %ux%ux%u@%ux%udpi", pages,
           header.cupsWidth, header.cupsHeight, header.cupsBitsPerPixel,
           header.HWResolution[0], header.HWResolution[1]);
    fflush(stdout);

    for (y = 0; y < header.cupsHeight; y ++)
      if (cupsRasterReadPixels(ras, data, header.cupsBytesPerLine) <
              header.cupsBytesPerLine)
        break;

    if (y < header.cupsHeight)
      printf(" ERROR AT LINE %d\n", y);
    else
      putchar('\n');

    free(data);
  }

  printf("EOF at %ld\n", (long)lseek(fd, SEEK_CUR, 0));

  cupsRasterClose(ras);
  close(fd);

  return (errors);
}


/*
 * 'do_raster_tests()' - Test reading and writing of raster data.
 */

static int				/* O - Number of errors */
do_raster_tests(cups_mode_t mode)	/* O - Write mode */
{
  unsigned		page, x, y, count;/* Looping vars */
  FILE			*fp;		/* Raster file */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header2_t	header,		/* Page header */
			expected;	/* Expected page header */
  unsigned char		data[2048];	/* Raster data */
  int			errors = 0;	/* Number of errors */


 /*
  * Test writing...
  */

  printf("cupsRasterOpen(%s): ",
         mode == CUPS_RASTER_WRITE ? "CUPS_RASTER_WRITE" :
	     mode == CUPS_RASTER_WRITE_COMPRESSED ? "CUPS_RASTER_WRITE_COMPRESSED" :
	     mode == CUPS_RASTER_WRITE_PWG ? "CUPS_RASTER_WRITE_PWG" :
				             "CUPS_RASTER_WRITE_APPLE");
  fflush(stdout);

  if ((fp = fopen("test.raster", "wb")) == NULL)
  {
    printf("FAIL (%s)\n", strerror(errno));
    return (1);
  }

  if ((r = cupsRasterOpen(fileno(fp), mode)) == NULL)
  {
    printf("FAIL (%s)\n", strerror(errno));
    fclose(fp);
    return (1);
  }

  puts("PASS");

  for (page = 0; page < 4; page ++)
  {
    memset(&header, 0, sizeof(header));
    header.cupsWidth        = 256;
    header.cupsHeight       = 256;
    header.cupsBytesPerLine = 256;
    header.HWResolution[0]  = 64;
    header.HWResolution[1]  = 64;
    header.PageSize[0]      = 288;
    header.PageSize[1]      = 288;
    header.cupsPageSize[0]  = 288.0f;
    header.cupsPageSize[1]  = 288.0f;

    strlcpy(header.MediaType, "auto", sizeof(header.MediaType));

    if (page & 1)
    {
      header.cupsBytesPerLine *= 4;
      header.cupsColorSpace = CUPS_CSPACE_CMYK;
      header.cupsColorOrder = CUPS_ORDER_CHUNKED;
      header.cupsNumColors  = 4;
    }
    else
    {
      header.cupsColorSpace = CUPS_CSPACE_W;
      header.cupsColorOrder = CUPS_ORDER_CHUNKED;
      header.cupsNumColors  = 1;
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

    printf("cupsRasterWriteHeader2(page %d): ", page + 1);

    if (cupsRasterWriteHeader2(r, &header))
    {
      puts("PASS");
    }
    else
    {
      puts("FAIL");
      errors ++;
    }

    fputs("cupsRasterWritePixels: ", stdout);
    fflush(stdout);

    memset(data, 0, header.cupsBytesPerLine);
    for (y = 0; y < 64; y ++)
      if (!cupsRasterWritePixels(r, data, header.cupsBytesPerLine))
        break;

    if (y < 64)
    {
      puts("FAIL");
      errors ++;
    }
    else
    {
      for (x = 0; x < header.cupsBytesPerLine; x ++)
	data[x] = (unsigned char)x;

      for (y = 0; y < 64; y ++)
	if (!cupsRasterWritePixels(r, data, header.cupsBytesPerLine))
	  break;

      if (y < 64)
      {
	puts("FAIL");
	errors ++;
      }
      else
      {
	memset(data, 255, header.cupsBytesPerLine);
	for (y = 0; y < 64; y ++)
	  if (!cupsRasterWritePixels(r, data, header.cupsBytesPerLine))
	    break;

	if (y < 64)
	{
	  puts("FAIL");
	  errors ++;
	}
	else
	{
	  for (x = 0; x < header.cupsBytesPerLine; x ++)
	    data[x] = (unsigned char)(x / 4);

	  for (y = 0; y < 64; y ++)
	    if (!cupsRasterWritePixels(r, data, header.cupsBytesPerLine))
	      break;

	  if (y < 64)
	  {
	    puts("FAIL");
	    errors ++;
	  }
	  else
	    puts("PASS");
        }
      }
    }
  }

  cupsRasterClose(r);
  fclose(fp);

 /*
  * Test reading...
  */

  fputs("cupsRasterOpen(CUPS_RASTER_READ): ", stdout);
  fflush(stdout);

  if ((fp = fopen("test.raster", "rb")) == NULL)
  {
    printf("FAIL (%s)\n", strerror(errno));
    return (1);
  }

  if ((r = cupsRasterOpen(fileno(fp), CUPS_RASTER_READ)) == NULL)
  {
    printf("FAIL (%s)\n", strerror(errno));
    fclose(fp);
    return (1);
  }

  puts("PASS");

  for (page = 0; page < 4; page ++)
  {
    memset(&expected, 0, sizeof(expected));
    expected.cupsWidth        = 256;
    expected.cupsHeight       = 256;
    expected.cupsBytesPerLine = 256;
    expected.HWResolution[0]  = 64;
    expected.HWResolution[1]  = 64;
    expected.PageSize[0]      = 288;
    expected.PageSize[1]      = 288;

    strlcpy(expected.MediaType, "auto", sizeof(expected.MediaType));

    if (mode != CUPS_RASTER_WRITE_PWG)
    {
      expected.cupsPageSize[0] = 288.0f;
      expected.cupsPageSize[1] = 288.0f;
    }

    if (mode >= CUPS_RASTER_WRITE_PWG)
    {
      strlcpy(expected.MediaClass, "PwgRaster", sizeof(expected.MediaClass));
      expected.cupsInteger[7] = 0xffffff;
    }

    if (page & 1)
    {
      expected.cupsBytesPerLine *= 4;
      expected.cupsColorSpace = CUPS_CSPACE_CMYK;
      expected.cupsColorOrder = CUPS_ORDER_CHUNKED;
      expected.cupsNumColors  = 4;
    }
    else
    {
      expected.cupsColorSpace = CUPS_CSPACE_W;
      expected.cupsColorOrder = CUPS_ORDER_CHUNKED;
      expected.cupsNumColors  = 1;
    }

    if (page & 2)
    {
      expected.cupsBytesPerLine *= 2;
      expected.cupsBitsPerColor = 16;
      expected.cupsBitsPerPixel = (page & 1) ? 64 : 16;
    }
    else
    {
      expected.cupsBitsPerColor = 8;
      expected.cupsBitsPerPixel = (page & 1) ? 32 : 8;
    }

    printf("cupsRasterReadHeader2(page %d): ", page + 1);
    fflush(stdout);

    if (!cupsRasterReadHeader2(r, &header))
    {
      puts("FAIL (read error)");
      errors ++;
      break;
    }
    else if (memcmp(&header, &expected, sizeof(header)))
    {
      puts("FAIL (bad page header)");
      errors ++;
      print_changes(&header, &expected);
    }
    else
      puts("PASS");

    fputs("cupsRasterReadPixels: ", stdout);
    fflush(stdout);

    for (y = 0; y < 64; y ++)
    {
      if (!cupsRasterReadPixels(r, data, header.cupsBytesPerLine))
      {
        puts("FAIL (read error)");
	errors ++;
	break;
      }

      if (data[0] != 0 || memcmp(data, data + 1, header.cupsBytesPerLine - 1))
      {
        printf("FAIL (raster line %d corrupt)\n", y);

	for (x = 0, count = 0; x < header.cupsBytesPerLine && count < 10; x ++)
        {
	  if (data[x])
	  {
	    count ++;

	    if (count == 10)
	      puts("   ...");
	    else
	      printf("  %4u %02X (expected %02X)\n", x, data[x], 0);
	  }
	}

	errors ++;
	break;
      }
    }

    if (y == 64)
    {
      for (y = 0; y < 64; y ++)
      {
	if (!cupsRasterReadPixels(r, data, header.cupsBytesPerLine))
	{
	  puts("FAIL (read error)");
	  errors ++;
	  break;
	}

	for (x = 0; x < header.cupsBytesPerLine; x ++)
          if (data[x] != (x & 255))
	    break;

	if (x < header.cupsBytesPerLine)
	{
	  printf("FAIL (raster line %d corrupt)\n", y + 64);

	  for (x = 0, count = 0; x < header.cupsBytesPerLine && count < 10; x ++)
	  {
	    if (data[x] != (x & 255))
	    {
	      count ++;

	      if (count == 10)
		puts("   ...");
	      else
		printf("  %4u %02X (expected %02X)\n", x, data[x], x & 255);
	    }
	  }

	  errors ++;
	  break;
	}
      }

      if (y == 64)
      {
	for (y = 0; y < 64; y ++)
	{
	  if (!cupsRasterReadPixels(r, data, header.cupsBytesPerLine))
	  {
	    puts("FAIL (read error)");
	    errors ++;
	    break;
	  }

	  if (data[0] != 255 || memcmp(data, data + 1, header.cupsBytesPerLine - 1))
          {
	    printf("fail (raster line %d corrupt)\n", y + 128);

	    for (x = 0, count = 0; x < header.cupsBytesPerLine && count < 10; x ++)
	    {
	      if (data[x] != 255)
	      {
		count ++;

		if (count == 10)
		  puts("   ...");
		else
		  printf("  %4u %02X (expected %02X)\n", x, data[x], 255);
	      }
	    }

	    errors ++;
	    break;
	  }
	}

        if (y == 64)
	{
	  for (y = 0; y < 64; y ++)
	  {
	    if (!cupsRasterReadPixels(r, data, header.cupsBytesPerLine))
	    {
	      puts("FAIL (read error)");
	      errors ++;
	      break;
	    }

	    for (x = 0; x < header.cupsBytesPerLine; x ++)
              if (data[x] != ((x / 4) & 255))
		break;

	    if (x < header.cupsBytesPerLine)
            {
	      printf("FAIL (raster line %d corrupt)\n", y + 192);

	      for (x = 0, count = 0; x < header.cupsBytesPerLine && count < 10; x ++)
	      {
		if (data[x] != ((x / 4) & 255))
		{
		  count ++;

		  if (count == 10)
		    puts("   ...");
		  else
		    printf("  %4u %02X (expected %02X)\n", x, data[x], (x / 4) & 255);
		}
	      }

	      errors ++;
	      break;
	    }
	  }

	  if (y == 64)
	    puts("PASS");
	}
      }
    }
  }

  cupsRasterClose(r);
  fclose(fp);

  return (errors);
}


/*
 * 'print_changes()' - Print differences in the page header.
 */

static void
print_changes(
    cups_page_header2_t *header,	/* I - Actual page header */
    cups_page_header2_t *expected)	/* I - Expected page header */
{
  int	i;				/* Looping var */


  if (strcmp(header->MediaClass, expected->MediaClass))
    printf("    MediaClass (%s), expected (%s)\n", header->MediaClass,
           expected->MediaClass);

  if (strcmp(header->MediaColor, expected->MediaColor))
    printf("    MediaColor (%s), expected (%s)\n", header->MediaColor,
           expected->MediaColor);

  if (strcmp(header->MediaType, expected->MediaType))
    printf("    MediaType (%s), expected (%s)\n", header->MediaType,
           expected->MediaType);

  if (strcmp(header->OutputType, expected->OutputType))
    printf("    OutputType (%s), expected (%s)\n", header->OutputType,
           expected->OutputType);

  if (header->AdvanceDistance != expected->AdvanceDistance)
    printf("    AdvanceDistance %d, expected %d\n", header->AdvanceDistance,
           expected->AdvanceDistance);

  if (header->AdvanceMedia != expected->AdvanceMedia)
    printf("    AdvanceMedia %d, expected %d\n", header->AdvanceMedia,
           expected->AdvanceMedia);

  if (header->Collate != expected->Collate)
    printf("    Collate %d, expected %d\n", header->Collate,
           expected->Collate);

  if (header->CutMedia != expected->CutMedia)
    printf("    CutMedia %d, expected %d\n", header->CutMedia,
           expected->CutMedia);

  if (header->Duplex != expected->Duplex)
    printf("    Duplex %d, expected %d\n", header->Duplex,
           expected->Duplex);

  if (header->HWResolution[0] != expected->HWResolution[0] ||
      header->HWResolution[1] != expected->HWResolution[1])
    printf("    HWResolution [%d %d], expected [%d %d]\n",
           header->HWResolution[0], header->HWResolution[1],
           expected->HWResolution[0], expected->HWResolution[1]);

  if (memcmp(header->ImagingBoundingBox, expected->ImagingBoundingBox,
             sizeof(header->ImagingBoundingBox)))
    printf("    ImagingBoundingBox [%d %d %d %d], expected [%d %d %d %d]\n",
           header->ImagingBoundingBox[0],
           header->ImagingBoundingBox[1],
           header->ImagingBoundingBox[2],
           header->ImagingBoundingBox[3],
           expected->ImagingBoundingBox[0],
           expected->ImagingBoundingBox[1],
           expected->ImagingBoundingBox[2],
           expected->ImagingBoundingBox[3]);

  if (header->InsertSheet != expected->InsertSheet)
    printf("    InsertSheet %d, expected %d\n", header->InsertSheet,
           expected->InsertSheet);

  if (header->Jog != expected->Jog)
    printf("    Jog %d, expected %d\n", header->Jog,
           expected->Jog);

  if (header->LeadingEdge != expected->LeadingEdge)
    printf("    LeadingEdge %d, expected %d\n", header->LeadingEdge,
           expected->LeadingEdge);

  if (header->Margins[0] != expected->Margins[0] ||
      header->Margins[1] != expected->Margins[1])
    printf("    Margins [%d %d], expected [%d %d]\n",
           header->Margins[0], header->Margins[1],
           expected->Margins[0], expected->Margins[1]);

  if (header->ManualFeed != expected->ManualFeed)
    printf("    ManualFeed %d, expected %d\n", header->ManualFeed,
           expected->ManualFeed);

  if (header->MediaPosition != expected->MediaPosition)
    printf("    MediaPosition %d, expected %d\n", header->MediaPosition,
           expected->MediaPosition);

  if (header->MediaWeight != expected->MediaWeight)
    printf("    MediaWeight %d, expected %d\n", header->MediaWeight,
           expected->MediaWeight);

  if (header->MirrorPrint != expected->MirrorPrint)
    printf("    MirrorPrint %d, expected %d\n", header->MirrorPrint,
           expected->MirrorPrint);

  if (header->NegativePrint != expected->NegativePrint)
    printf("    NegativePrint %d, expected %d\n", header->NegativePrint,
           expected->NegativePrint);

  if (header->NumCopies != expected->NumCopies)
    printf("    NumCopies %d, expected %d\n", header->NumCopies,
           expected->NumCopies);

  if (header->Orientation != expected->Orientation)
    printf("    Orientation %d, expected %d\n", header->Orientation,
           expected->Orientation);

  if (header->OutputFaceUp != expected->OutputFaceUp)
    printf("    OutputFaceUp %d, expected %d\n", header->OutputFaceUp,
           expected->OutputFaceUp);

  if (header->PageSize[0] != expected->PageSize[0] ||
      header->PageSize[1] != expected->PageSize[1])
    printf("    PageSize [%d %d], expected [%d %d]\n",
           header->PageSize[0], header->PageSize[1],
           expected->PageSize[0], expected->PageSize[1]);

  if (header->Separations != expected->Separations)
    printf("    Separations %d, expected %d\n", header->Separations,
           expected->Separations);

  if (header->TraySwitch != expected->TraySwitch)
    printf("    TraySwitch %d, expected %d\n", header->TraySwitch,
           expected->TraySwitch);

  if (header->Tumble != expected->Tumble)
    printf("    Tumble %d, expected %d\n", header->Tumble,
           expected->Tumble);

  if (header->cupsWidth != expected->cupsWidth)
    printf("    cupsWidth %d, expected %d\n", header->cupsWidth,
           expected->cupsWidth);

  if (header->cupsHeight != expected->cupsHeight)
    printf("    cupsHeight %d, expected %d\n", header->cupsHeight,
           expected->cupsHeight);

  if (header->cupsMediaType != expected->cupsMediaType)
    printf("    cupsMediaType %d, expected %d\n", header->cupsMediaType,
           expected->cupsMediaType);

  if (header->cupsBitsPerColor != expected->cupsBitsPerColor)
    printf("    cupsBitsPerColor %d, expected %d\n", header->cupsBitsPerColor,
           expected->cupsBitsPerColor);

  if (header->cupsBitsPerPixel != expected->cupsBitsPerPixel)
    printf("    cupsBitsPerPixel %d, expected %d\n", header->cupsBitsPerPixel,
           expected->cupsBitsPerPixel);

  if (header->cupsBytesPerLine != expected->cupsBytesPerLine)
    printf("    cupsBytesPerLine %d, expected %d\n", header->cupsBytesPerLine,
           expected->cupsBytesPerLine);

  if (header->cupsColorOrder != expected->cupsColorOrder)
    printf("    cupsColorOrder %d, expected %d\n", header->cupsColorOrder,
           expected->cupsColorOrder);

  if (header->cupsColorSpace != expected->cupsColorSpace)
    printf("    cupsColorSpace %d, expected %d\n", header->cupsColorSpace,
           expected->cupsColorSpace);

  if (header->cupsCompression != expected->cupsCompression)
    printf("    cupsCompression %d, expected %d\n", header->cupsCompression,
           expected->cupsCompression);

  if (header->cupsRowCount != expected->cupsRowCount)
    printf("    cupsRowCount %d, expected %d\n", header->cupsRowCount,
           expected->cupsRowCount);

  if (header->cupsRowFeed != expected->cupsRowFeed)
    printf("    cupsRowFeed %d, expected %d\n", header->cupsRowFeed,
           expected->cupsRowFeed);

  if (header->cupsRowStep != expected->cupsRowStep)
    printf("    cupsRowStep %d, expected %d\n", header->cupsRowStep,
           expected->cupsRowStep);

  if (header->cupsNumColors != expected->cupsNumColors)
    printf("    cupsNumColors %d, expected %d\n", header->cupsNumColors,
           expected->cupsNumColors);

  if (fabs(header->cupsBorderlessScalingFactor - expected->cupsBorderlessScalingFactor) > 0.001)
    printf("    cupsBorderlessScalingFactor %g, expected %g\n",
           header->cupsBorderlessScalingFactor,
           expected->cupsBorderlessScalingFactor);

  if (fabs(header->cupsPageSize[0] - expected->cupsPageSize[0]) > 0.001 ||
      fabs(header->cupsPageSize[1] - expected->cupsPageSize[1]) > 0.001)
    printf("    cupsPageSize [%g %g], expected [%g %g]\n",
           header->cupsPageSize[0], header->cupsPageSize[1],
           expected->cupsPageSize[0], expected->cupsPageSize[1]);

  if (fabs(header->cupsImagingBBox[0] - expected->cupsImagingBBox[0]) > 0.001 ||
      fabs(header->cupsImagingBBox[1] - expected->cupsImagingBBox[1]) > 0.001 ||
      fabs(header->cupsImagingBBox[2] - expected->cupsImagingBBox[2]) > 0.001 ||
      fabs(header->cupsImagingBBox[3] - expected->cupsImagingBBox[3]) > 0.001)
    printf("    cupsImagingBBox [%g %g %g %g], expected [%g %g %g %g]\n",
           header->cupsImagingBBox[0], header->cupsImagingBBox[1],
           header->cupsImagingBBox[2], header->cupsImagingBBox[3],
           expected->cupsImagingBBox[0], expected->cupsImagingBBox[1],
           expected->cupsImagingBBox[2], expected->cupsImagingBBox[3]);

  for (i = 0; i < 16; i ++)
    if (header->cupsInteger[i] != expected->cupsInteger[i])
      printf("    cupsInteger%d %d, expected %d\n", i, header->cupsInteger[i],
             expected->cupsInteger[i]);

  for (i = 0; i < 16; i ++)
    if (fabs(header->cupsReal[i] - expected->cupsReal[i]) > 0.001)
      printf("    cupsReal%d %g, expected %g\n", i, header->cupsReal[i],
             expected->cupsReal[i]);

  for (i = 0; i < 16; i ++)
    if (strcmp(header->cupsString[i], expected->cupsString[i]))
      printf("    cupsString%d (%s), expected (%s)\n", i,
	     header->cupsString[i], expected->cupsString[i]);

  if (strcmp(header->cupsMarkerType, expected->cupsMarkerType))
    printf("    cupsMarkerType (%s), expected (%s)\n", header->cupsMarkerType,
           expected->cupsMarkerType);

  if (strcmp(header->cupsRenderingIntent, expected->cupsRenderingIntent))
    printf("    cupsRenderingIntent (%s), expected (%s)\n",
           header->cupsRenderingIntent,
           expected->cupsRenderingIntent);

  if (strcmp(header->cupsPageSizeName, expected->cupsPageSizeName))
    printf("    cupsPageSizeName (%s), expected (%s)\n",
           header->cupsPageSizeName,
           expected->cupsPageSizeName);
}
