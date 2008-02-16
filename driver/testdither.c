/*
 * "$Id$"
 *
 *   Dither test program for ESP Print Pro.
 *
 *   Try the following:
 *
 *       testdither 0 255 > filename.ppm
 *       testdither 0 127 255 > filename.ppm
 *       testdither 0 85 170 255 > filename.ppm
 *       testdither 0 63 127 170 198 227 255 > filename.ppm
 *       testdither 0 210 383 > filename.ppm
 *       testdither 0 82 255 > filename.ppm
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()  - Test dithering and output a PPM file.
 *   usage() - Show program usage...
 */

/*
 * Include necessary headers.
 */

#include "driver.h"
#include <cups/string.h>


/*
 * Local functions...
 */

void	usage(void);


/*
 * 'main()' - Test dithering and output a PPM file.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		x, y;		/* Current coordinate in image */
  short		line[512];	/* Line to dither */
  unsigned char	pixels[512],	/* Dither pixels */
		*pixptr;	/* Pointer in line */
  int		output;		/* Output pixel */
  cups_lut_t	*lut;		/* Dither lookup table */
  cups_dither_t	*dither;	/* Dither state */
  int		nlutvals;	/* Number of lookup values */
  float		lutvals[16];	/* Lookup values */
  int		pixvals[16];	/* Pixel values */


 /*
  * See if we have lookup table values on the command-line...
  */

  if (argc > 1)
  {
   /*
    * Yes, collect them...
    */

    nlutvals = 0;

    for (x = 1; x < argc; x ++)
      if (isdigit(argv[x][0]) && nlutvals < 16)
      {
        pixvals[nlutvals] = atoi(argv[x]);
        lutvals[nlutvals] = atof(argv[x]) / 255.0;
	nlutvals ++;
      }
      else
        usage();

   /*
    * See if we have at least 2 values...
    */

    if (nlutvals < 2)
      usage();
  }
  else
  {
   /*
    * Otherwise use the default 2-entry LUT with values of 0 and 255...
    */

    nlutvals   = 2;
    lutvals[0] = 0.0;
    lutvals[1] = 1.0;
    pixvals[0] = 0;
    pixvals[1] = 255;
  }

 /*
  * Create the lookup table and dither state...
  */

  lut    = cupsLutNew(nlutvals, lutvals);
  dither = cupsDitherNew(512);

 /*
  * Put out the PGM header for a raw 256x256x8-bit grayscale file...
  */

  puts("P5\n512\n512\n255");

 /*
  * Dither 512 lines, which are written out in 256 image lines...
  */

  for (y = 0; y < 512; y ++)
  {
   /*
    * Create the grayscale data for the current line...
    */

    for (x = 0; x < 512; x ++)
      line[x] = 4095 * ((y / 32) * 16 + x / 32) / 255;

   /*
    * Dither the line...
    */

    cupsDitherLine(dither, lut, line, 1, pixels);

    if (y == 0)
    {
      fputs("DEBUG: pixels =", stderr);
      for (x = 0; x < 512; x ++)
        fprintf(stderr, " %d", pixels[x]);
      fputs("\n", stderr);
    }

   /*
    * Add or set the output pixel values...
    */

    for (x = 0, pixptr = pixels; x < 512; x ++, pixptr ++)
    {
      output = 255 - pixvals[*pixptr];

      if (output < 0)
	putchar(0);
      else
	putchar(output);
    }
  }

 /*
  * Free the dither state and lookup table...
  */

  cupsDitherDelete(dither);
  cupsLutDelete(lut);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'usage()' - Show program usage...
 */

void
usage(void)
{
  puts("Usage: testdither [val1 val2 [... val16]] >filename.ppm");
  exit(1);
}


/*
 * End of "$Id$".
 */
