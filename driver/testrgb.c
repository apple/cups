/*
 * "$Id$"
 *
 *   Test the new RGB color separation code for ESP Print Pro.
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
 *   main()       - Do color rgb tests.
 *   test_gray()  - Test grayscale rgbs...
 *   test_rgb()   - Test color rgbs...
 */

/*
 * Include necessary headers.
 */

#include <cups/string.h>
#include "driver.h"
#include <sys/stat.h>

#ifdef HAVE_LIBLCMS
#  include <lcms/lcms.h>
#endif /* HAVE_LIBLCMS */


void	test_gray(cups_sample_t *samples, int num_samples,
	          int cube_size, int num_comps, const char *basename);
void	test_rgb(cups_sample_t *samples, int num_samples,
		 int cube_size, int num_comps,
		 const char *basename);


/*
 * 'main()' - Do color rgb tests.
 */

int						/* O - Exit status */
main(int  argc,					/* I - Number of command-line arguments */
     char *argv[])				/* I - Command-line arguments */
{
  static cups_sample_t	CMYK[] =		/* Basic 4-color sep */
			{
			  /*{ r,   g,   b   }, { C,   M,   Y,   K   }*/
			  { { 0,   0,   0   }, { 0,   0,   0,   255 } },
			  { { 255, 0,   0   }, { 0,   255, 240, 0   } },
			  { { 0,   255, 0   }, { 200, 0,   200, 0   } },
			  { { 255, 255, 0   }, { 0,   0,   240, 0   } },
			  { { 0,   0,   255 }, { 200, 200, 0,   0   } },
			  { { 255, 0,   255 }, { 0,   200, 0,   0   } },
			  { { 0,   255, 255 }, { 200, 0,   0,   0   } },
			  { { 255, 255, 255 }, { 0,   0,   0,   0   } }
			};


 /*
  * Make the "images" test directory...
  */

  mkdir("images", 0700);

 /*
  * Run tests for CMYK and CMYK separations...
  */

  test_rgb(CMYK, 8, 2, 4, "images/rgb-cmyk");

  test_gray(CMYK, 8, 2, 4, "images/gray-cmyk");

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'test_gray()' - Test grayscale rgbs...
 */

void
test_gray(cups_sample_t *samples,	/* I - Sample values */
          int           num_samples,	/* I - Number of samples */
	  int           cube_size,	/* I - Cube size */
          int           num_comps,	/* I - Number of components */
	  const char    *basename)	/* I - Base filename of output */
{
  int			i;		/* Looping var */
  char			filename[255];	/* Output filename */
  char			line[255];	/* Line from PPM file */
  int			width, height;	/* Width and height of test image */
  int			x, y;		/* Current coordinate in image */
  int			r, g, b;	/* Current RGB color */
  unsigned char		input[7000];	/* Line to rgbarate */
  unsigned char		output[48000],	/* Output rgb data */
			*outptr;	/* Pointer in output */
  FILE			*in;		/* Input PPM file */
  FILE			*out[CUPS_MAX_CHAN];
					/* Output PGM files */
  FILE			*comp;		/* Composite output */
  cups_rgb_t		*rgb;		/* Color separation */


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
  * Create the color rgb...
  */

  rgb = cupsRGBNew(num_samples, samples, cube_size, num_comps);

 /*
  * Open the color rgb files...
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
  * Read the image and do the rgbs...
  */

  for (y = 0; y < height; y ++)
  {
    fread(input, width, 1, in);

    cupsRGBDoGray(rgb, input, output, width);

    for (x = 0, outptr = output; x < width; x ++, outptr += num_comps)
    {
      for (i = 0; i < num_comps; i ++)
        putc(255 - outptr[i], out[i]);

      r = 255;
      g = 255;
      b = 255;

      r -= outptr[0];
      g -= outptr[1];
      b -= outptr[2];

      r -= outptr[3];
      g -= outptr[3];
      b -= outptr[3];

      if (num_comps > 4)
      {
        r -= outptr[4] / 2;
	g -= outptr[5] / 2;
      }

      if (num_comps > 6)
      {
        r -= outptr[6] / 2;
	g -= outptr[6] / 2;
	b -= outptr[6] / 2;
      }

      if (r < 0)
        putc(0, comp);
      else
        putc(r, comp);

      if (g < 0)
        putc(0, comp);
      else
        putc(g, comp);

      if (b < 0)
        putc(0, comp);
      else
        putc(b, comp);
    }
  }

  for (i = 0; i < num_comps; i ++)
    fclose(out[i]);

  fclose(comp);
  fclose(in);

  cupsRGBDelete(rgb);
}


/*
 * 'test_rgb()' - Test color rgbs...
 */

void
test_rgb(cups_sample_t *samples,	/* I - Sample values */
         int           num_samples,	/* I - Number of samples */
	 int           cube_size,	/* I - Cube size */
         int           num_comps,	/* I - Number of components */
	 const char    *basename)	/* I - Base filename of output */
{
  int			i;		/* Looping var */
  char			filename[255];	/* Output filename */
  char			line[255];	/* Line from PPM file */
  int			width, height;	/* Width and height of test image */
  int			x, y;		/* Current coordinate in image */
  int			r, g, b;	/* Current RGB color */
  unsigned char		input[7000];	/* Line to rgbarate */
  unsigned char		output[48000],	/* Output rgb data */
			*outptr;	/* Pointer in output */
  FILE			*in;		/* Input PPM file */
  FILE			*out[CUPS_MAX_CHAN];
					/* Output PGM files */
  FILE			*comp;		/* Composite output */
  cups_rgb_t		*rgb;		/* Color separation */


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
  * Create the color rgb...
  */

  rgb = cupsRGBNew(num_samples, samples, cube_size, num_comps);

 /*
  * Open the color rgb files...
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
  * Read the image and do the rgbs...
  */

  for (y = 0; y < height; y ++)
  {
    fread(input, width, 3, in);

    cupsRGBDoRGB(rgb, input, output, width);

    for (x = 0, outptr = output; x < width; x ++, outptr += num_comps)
    {
      for (i = 0; i < num_comps; i ++)
        putc(255 - outptr[i], out[i]);

      r = 255;
      g = 255;
      b = 255;

      r -= outptr[0];
      g -= outptr[1];
      b -= outptr[2];

      r -= outptr[3];
      g -= outptr[3];
      b -= outptr[3];

      if (num_comps > 4)
      {
        r -= outptr[4] / 2;
	g -= outptr[5] / 2;
      }

      if (num_comps > 6)
      {
        r -= outptr[6] / 2;
	g -= outptr[6] / 2;
	b -= outptr[6] / 2;
      }

      if (r < 0)
        putc(0, comp);
      else
        putc(r, comp);

      if (g < 0)
        putc(0, comp);
      else
        putc(g, comp);

      if (b < 0)
        putc(0, comp);
      else
        putc(b, comp);
    }
  }

  for (i = 0; i < num_comps; i ++)
    fclose(out[i]);

  fclose(comp);
  fclose(in);

  cupsRGBDelete(rgb);
}


/*
 * End of "$Id$".
 */
