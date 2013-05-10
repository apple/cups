/*
 * "$Id$"
 *
 *   Test the CMYK color separation code for ESP Print Pro.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2006 by Easy Software Products, All Rights Reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   test_gray() - Test grayscale separations...
 *   test_rgb()  - Test color separations...
 *   main()      - Do color separation tests.
 */

/*
 * Include necessary headers.
 */

#include <cups/string.h>
#include "driver.h"
#include <sys/stat.h>


void	test_gray(int num_comps, const char *basename);
void	test_rgb(int num_comps, const char *basename);


/*
 * 'main()' - Do color separation tests.
 */

int						/* O - Exit status */
main(int  argc,					/* I - Number of command-line arguments */
     char *argv[])				/* I - Command-line arguments */
{
 /*
  * Make the test directory...
  */

  mkdir("test", 0755);

 /*
  * Run tests for K, Kk, CMY, CMYK, CcMmYK, and CcMmYKk separations...
  */

  test_rgb(1, "test/K-rgb");
  test_rgb(2, "test/Kk-rgb");
  test_rgb(3, "test/CMY-rgb");
  test_rgb(4, "test/CMYK-rgb");
  test_rgb(6, "test/CcMmYK-rgb");
  test_rgb(7, "test/CcMmYKk-rgb");

  test_gray(1, "test/K-gray");
  test_gray(2, "test/Kk-gray");
  test_gray(3, "test/CMY-gray");
  test_gray(4, "test/CMYK-gray");
  test_gray(6, "test/CcMmYK-gray");
  test_gray(7, "test/CcMmYKk-gray");

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'test_gray()' - Test grayscale separations...
 */

void
test_gray(int        num_comps,		/* I - Number of components */
	  const char *basename)		/* I - Base filename of output */
{
  int			i;		/* Looping var */
  char			filename[255];	/* Output filename */
  char			line[255];	/* Line from PGM file */
  int			width, height;	/* Width and height of test image */
  int			x, y;		/* Current coordinate in image */
  int			r, g, b;	/* Current RGB color */
  unsigned char		input[7000];	/* Line to separate */
  short			output[48000],	/* Output separation data */
			*outptr;	/* Pointer in output */
  FILE			*in;		/* Input PPM file */
  FILE			*out[CUPS_MAX_CHAN];
					/* Output PGM files */
  FILE			*comp;		/* Composite output */
  cups_cmyk_t		*cmyk;		/* Color separation */


 /*
  * Open the test image...
  */

  in = fopen("image.pgm", "rb");
  while (fgets(line, sizeof(line), in) != NULL)
    if (isdigit(line[0]))
      break;

  sscanf(line, "%d%d", &width, &height);

  fgets(line, sizeof(line), in);

 /*
  * Create the color separation...
  */

  cmyk = cupsCMYKNew(num_comps);

  switch (num_comps)
  {
    case 2 : /* Kk */
        cupsCMYKSetLtDk(cmyk, 0, 0.5, 1.0);
	break;

    case 4 :
	cupsCMYKSetGamma(cmyk, 2, 1.0, 0.9);
        cupsCMYKSetBlack(cmyk, 0.5, 1.0);
	break;

    case 6 : /* CcMmYK */
        cupsCMYKSetLtDk(cmyk, 0, 0.5, 1.0);
        cupsCMYKSetLtDk(cmyk, 2, 0.5, 1.0);
	cupsCMYKSetGamma(cmyk, 4, 1.0, 0.9);
        cupsCMYKSetBlack(cmyk, 0.5, 1.0);
	break;

    case 7 : /* CcMmYKk */
        cupsCMYKSetLtDk(cmyk, 0, 0.5, 1.0);
        cupsCMYKSetLtDk(cmyk, 2, 0.5, 1.0);
	cupsCMYKSetGamma(cmyk, 4, 1.0, 0.9);
        cupsCMYKSetLtDk(cmyk, 5, 0.5, 1.0);
	break;
  }

 /*
  * Open the color separation files...
  */

  for (i = 0; i < num_comps; i ++)
  {
    sprintf(filename, "%s%d.pgm", basename, i);
    out[i] = fopen(filename, "wb");

    fprintf(out[i], "P5\n%d %d 255\n", width, height);
  }

  sprintf(filename, "%s.ppm", basename);
  comp = fopen(filename, "wb");

  fprintf(comp, "P6\n%d %d 255\n", width, height);

 /*
  * Read the image and do the separations...
  */

  for (y = 0; y < height; y ++)
  {
    fread(input, width, 1, in);

    cupsCMYKDoGray(cmyk, input, output, width);

    for (x = 0, outptr = output; x < width; x ++, outptr += num_comps)
    {
      for (i = 0; i < num_comps; i ++)
        putc(255 - 255 * outptr[i] / 4095, out[i]);

      r = 4095;
      g = 4095;
      b = 4095;

      switch (num_comps)
      {
        case 1 :
	    r -= outptr[0];
	    g -= outptr[0];
	    b -= outptr[0];
	    break;
	case 2 :
	    r -= outptr[0];
	    g -= outptr[0];
	    b -= outptr[0];

	    r -= outptr[1] / 2;
	    g -= outptr[1] / 2;
	    b -= outptr[1] / 2;
	    break;
	case 3 :
	    r -= outptr[0];
	    g -= outptr[1];
	    b -= outptr[2];
	    break;
	case 4 :
	    r -= outptr[0];
	    g -= outptr[1];
	    b -= outptr[2];

	    r -= outptr[3];
	    g -= outptr[3];
	    b -= outptr[3];
	    break;
	case 6 :
	    r -= outptr[0] + outptr[1] / 2;
	    g -= outptr[2] + outptr[3] / 3;
	    b -= outptr[4];

	    r -= outptr[5];
	    g -= outptr[5];
	    b -= outptr[5];
	    break;
	case 7 :
	    r -= outptr[0] + outptr[1] / 2;
	    g -= outptr[2] + outptr[3] / 3;
	    b -= outptr[4];

	    r -= outptr[5] + outptr[6] / 2;
	    g -= outptr[5] + outptr[6] / 2;
	    b -= outptr[5] + outptr[6] / 2;
	    break;
      }

      if (r < 0)
        putc(0, comp);
      else
        putc(255 * r / 4095, comp);

      if (g < 0)
        putc(0, comp);
      else
        putc(255 * g / 4095, comp);

      if (b < 0)
        putc(0, comp);
      else
        putc(255 * b / 4095, comp);
    }
  }

  for (i = 0; i < num_comps; i ++)
    fclose(out[i]);

  fclose(comp);
  fclose(in);

  cupsCMYKDelete(cmyk);
}


/*
 * 'test_rgb()' - Test color separations...
 */

void
test_rgb(int        num_comps,		/* I - Number of components */
	 const char *basename)		/* I - Base filename of output */
{
  int			i;		/* Looping var */
  char			filename[255];	/* Output filename */
  char			line[255];	/* Line from PPM file */
  int			width, height;	/* Width and height of test image */
  int			x, y;		/* Current coordinate in image */
  int			r, g, b;	/* Current RGB color */
  unsigned char		input[7000];	/* Line to separate */
  short			output[48000],	/* Output separation data */
			*outptr;	/* Pointer in output */
  FILE			*in;		/* Input PPM file */
  FILE			*out[CUPS_MAX_CHAN];
					/* Output PGM files */
  FILE			*comp;		/* Composite output */
  cups_cmyk_t		*cmyk;		/* Color separation */


 /*
  * Open the test image...
  */

  in = fopen("image.ppm", "rb");
  while (fgets(line, sizeof(line), in) != NULL)
    if (isdigit(line[0]))
      break;

  sscanf(line, "%d%d", &width, &height);

  fgets(line, sizeof(line), in);

 /*
  * Create the color separation...
  */

  cmyk = cupsCMYKNew(num_comps);

  cupsCMYKSetBlack(cmyk, 0.5, 1.0);

  switch (num_comps)
  {
    case 2 : /* Kk */
        cupsCMYKSetLtDk(cmyk, 0, 0.5, 1.0);
	break;
    case 6 : /* CcMmYK */
	cupsCMYKSetGamma(cmyk, 0, 1.0, 0.8);
        cupsCMYKSetLtDk(cmyk, 0, 0.5, 1.0);
	cupsCMYKSetGamma(cmyk, 2, 1.0, 0.8);
        cupsCMYKSetLtDk(cmyk, 2, 0.5, 1.0);
	break;
    case 7 : /* CcMmYKk */
	cupsCMYKSetGamma(cmyk, 0, 1.0, 0.8);
        cupsCMYKSetLtDk(cmyk, 0, 0.5, 1.0);
	cupsCMYKSetGamma(cmyk, 2, 1.0, 0.8);
        cupsCMYKSetLtDk(cmyk, 2, 0.5, 1.0);
        cupsCMYKSetLtDk(cmyk, 5, 0.5, 1.0);
	break;
  }

 /*
  * Open the color separation files...
  */

  for (i = 0; i < num_comps; i ++)
  {
    sprintf(filename, "%s%d.pgm", basename, i);
    out[i] = fopen(filename, "wb");

    fprintf(out[i], "P5\n%d %d 255\n", width, height);
  }

  sprintf(filename, "%s.ppm", basename);
  comp = fopen(filename, "wb");

  fprintf(comp, "P6\n%d %d 255\n", width, height);

 /*
  * Read the image and do the separations...
  */

  for (y = 0; y < height; y ++)
  {
    fread(input, width, 3, in);

    cupsCMYKDoRGB(cmyk, input, output, width);

    for (x = 0, outptr = output; x < width; x ++, outptr += num_comps)
    {
      for (i = 0; i < num_comps; i ++)
        putc(255 - 255 * outptr[i] / 4095, out[i]);

      r = 4095;
      g = 4095;
      b = 4095;

      switch (num_comps)
      {
        case 1 :
	    r -= outptr[0];
	    g -= outptr[0];
	    b -= outptr[0];
	    break;
	case 2 :
	    r -= outptr[0];
	    g -= outptr[0];
	    b -= outptr[0];

	    r -= outptr[1] / 2;
	    g -= outptr[1] / 2;
	    b -= outptr[1] / 2;
	    break;
	case 3 :
	    r -= outptr[0];
	    g -= outptr[1];
	    b -= outptr[2];
	    break;
	case 4 :
	    r -= outptr[0];
	    g -= outptr[1];
	    b -= outptr[2];

	    r -= outptr[3];
	    g -= outptr[3];
	    b -= outptr[3];
	    break;
	case 6 :
	    r -= outptr[0] + outptr[1] / 2;
	    g -= outptr[2] + outptr[3] / 3;
	    b -= outptr[4];

	    r -= outptr[5];
	    g -= outptr[5];
	    b -= outptr[5];
	    break;
	case 7 :
	    r -= outptr[0] + outptr[1] / 2;
	    g -= outptr[2] + outptr[3] / 3;
	    b -= outptr[4];

	    r -= outptr[5] + outptr[6] / 2;
	    g -= outptr[5] + outptr[6] / 2;
	    b -= outptr[5] + outptr[6] / 2;
	    break;
      }

      if (r < 0)
        putc(0, comp);
      else
        putc(255 * r / 4095, comp);

      if (g < 0)
        putc(0, comp);
      else
        putc(255 * g / 4095, comp);

      if (b < 0)
        putc(0, comp);
      else
        putc(255 * b / 4095, comp);
    }
  }

  for (i = 0; i < num_comps; i ++)
    fclose(out[i]);

  fclose(comp);
  fclose(in);

  cupsCMYKDelete(cmyk);
}


/*
 * End of "$Id$".
 */
