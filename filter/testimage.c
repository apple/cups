/*
 * "$Id$"
 *
 *   Image library test program for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1993-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Main entry...
 */

/*
 * Include necessary headers...
 */

#include "image.h"


/*
 * 'main()' - Main entry...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_image_t		*img;		/* Image to print */
  cups_icspace_t	primary;	/* Primary image colorspace */
  FILE			*out;		/* Output PPM/PGM file */
  cups_ib_t		*line;		/* Line from file */
  int			y,		/* Current line */
			width,		/* Width of image */
			height,		/* Height of image */
			depth;		/* Depth of image */


  if (argc != 3)
  {
    puts("Usage: testimage filename.ext filename.[ppm|pgm]");
    return (1);
  }

  if (strstr(argv[2], ".ppm") != NULL)
    primary = CUPS_IMAGE_RGB;
  else
    primary = CUPS_IMAGE_WHITE;

  img = cupsImageOpen(argv[1], primary, CUPS_IMAGE_WHITE, 100, 0, NULL);

  if (!img)
  {
    perror(argv[1]);
    return (1);
  }

  out = fopen(argv[2], "wb");

  if (!out)
  {
    perror(argv[2]);
    cupsImageClose(img);
    return (1);
  }

  width  = cupsImageGetWidth(img);
  height = cupsImageGetHeight(img);
  depth  = cupsImageGetDepth(img);
  line   = calloc(width, depth);

  fprintf(out, "P%d\n%d\n%d\n255\n",
          cupsImageGetColorSpace(img) == CUPS_IMAGE_WHITE ? 5 : 6,
          width, height);

  for (y = 0; y < height; y ++)
  {
    cupsImageGetRow(img, 0, y, width, line);
    fwrite(line, width, depth, out);
  }

  cupsImageClose(img);
  fclose(out);

  return (0);
}


/*
 * End of "$Id$".
 */
