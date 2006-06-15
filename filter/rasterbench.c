/*
 * "$Id$"
 *
 *   Raster benchmark program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   main()           - Benchmark the raster read/write functions.
 *   compute_median() - Compute the median time for a test.
 *   read_test()      - Benchmark the raster read functions.
 *   write_test()     - Benchmark the raster write functions.
 */

/*
 * Include necessary headers...
 */

#include "raster.h"
#include <stdlib.h>
#include <sys/time.h>


/*
 * Constants...
 */

#define TEST_WIDTH	1024
#define TEST_HEIGHT	1024
#define TEST_PAGES	16
#define TEST_PASSES	20


/*
 * Local functions...
 */

static double	compute_median(double *secs);
static double	read_test(void);
static double	write_test(int do_random);


/*
 * 'main()' - Benchmark the raster read/write functions.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  double	secs,			/* Test times */
		write_random_secs[TEST_PASSES],
		read_random_secs[TEST_PASSES],
		write_normal_secs[TEST_PASSES],
		read_normal_secs[TEST_PASSES],
		write_random,		/* Total write time for random */
		read_random,		/* Total read time for random */
		write_normal,		/* Total write time for normal */
		read_normal;		/* Total read time for normal */


 /*
  * Run the tests 10 times to get a good average...
  */

  printf("Test read/write speed of %d pages, %dx%d pixels...\n\n",
         TEST_PAGES, TEST_WIDTH, TEST_HEIGHT);
  for (i = 0, read_normal = 0.0, read_random = 0.0,
           write_normal = 0.0, write_random = 0.0;
       i < TEST_PASSES;
       i ++)
  {
    printf("PASS %2d: normal-write", i + 1);
    fflush(stdout);
    secs = write_normal_secs[i] = write_test(0);

    printf(" %.3f, read", secs);
    fflush(stdout);
    secs = read_normal_secs[i] = read_test();

    printf(" %.3f, random-write", secs);
    fflush(stdout);
    secs = write_random_secs[i] = write_test(1);

    printf(" %.3f, read", secs);
    fflush(stdout);
    secs = read_random_secs[i] = read_test();

    printf(" %.3f\n", secs);
  }

  write_random = compute_median(write_random_secs);
  read_random  = compute_median(read_random_secs);
  write_normal = compute_median(write_normal_secs);
  read_normal  = compute_median(read_normal_secs);

  printf("\nNormal Write Time: %.3f seconds per document\n",
         write_normal);
  printf("Random Write Time: %.3f seconds per document\n",
         write_random);
  printf("Average Write Time: %.3f seconds per document\n",
         (write_normal + write_random) / 2);

  printf("\nNormal Read Time: %.3f seconds per document\n",
         read_normal);
  printf("Random Read Time: %.3f seconds per document\n",
         read_random);
  printf("Average Read Time: %.3f seconds per document\n",
         (read_normal + read_random) / 2);

  return (0);
}


/*
 * 'compute_median()' - Compute the median time for a test.
 */

static double				/* O - Median time in seconds */
compute_median(double *secs)		/* I - Array of time samples */
{
  int		i, j;			/* Looping vars */
  double	temp;			/* Swap variable */


 /*
  * Sort the array into ascending order using a quicky bubble sort...
  */

  for (i = 0; i < (TEST_PASSES - 1); i ++)
    for (j = i + 1; j < TEST_PASSES; j ++)
      if (secs[i] > secs[j])
      {
        temp    = secs[i];
	secs[i] = secs[j];
	secs[j] = temp;
      }

 /*
  * Return the average of the middle two samples...
  */

  return (0.5 * (secs[TEST_PASSES / 2 - 1] + secs[TEST_PASSES / 2]));
}


/*
 * 'read_test()' - Benchmark the raster read functions.
 */

static double				/* O - Total time in seconds */
read_test(void)
{
  int			y;		/* Looping var */
  FILE			*fp;		/* Raster file */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header_t	header;		/* Page header */
  unsigned char		buffer[8 * TEST_WIDTH];
					/* Read buffer */
  struct timeval	start, end;	/* Start and end times */


 /*
  * Test read speed...
  */

  if ((fp = fopen("test.raster", "rb")) == NULL)
  {
    perror("Unable to open test.raster");
    return (0.0);
  }

  if ((r = cupsRasterOpen(fileno(fp), CUPS_RASTER_READ)) == NULL)
  {
    perror("Unable to create raster input stream");
    fclose(fp);
    return (0.0);
  }

  gettimeofday(&start, NULL);

  while (cupsRasterReadHeader(r, &header))
  {
    for (y = 0; y < header.cupsHeight; y ++)
      cupsRasterReadPixels(r, buffer, header.cupsBytesPerLine);
  }

  cupsRasterClose(r);

  gettimeofday(&end, NULL);

  fclose(fp);

  return (end.tv_sec - start.tv_sec +
          0.000001 * (end.tv_usec - start.tv_usec));
}


/*
 * 'write_test()' - Benchmark the raster write functions.
 */

static double				/* O - Total time in seconds */
write_test(int do_random)		/* I - Do random data? */
{
  int			page, x, y;	/* Looping vars */
  FILE			*fp;		/* Raster file */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header_t	header;		/* Page header */
  unsigned char		data[2][8 * TEST_WIDTH];
					/* Raster data to write */
  struct timeval	start, end;	/* Start and end times */


  if (do_random)
  {
   /*
    * Create some random data to test worst-case performance...
    */

    srand(time(NULL));

    for (x = 0; x < sizeof(data[0]); x ++)
    {
      data[0][x] = rand();
      data[1][x] = rand();
    }
  }
  else
  {
   /*
    * Otherwise, create some non-random data to test the best-case
    * performance...
    */

    for (x = 0; x < sizeof(data[0]); x ++)
    {
      data[0][x] = x & 255;		/* Gradients */
      data[1][x] = (x & 128) ? 255 : 0;	/* Alternating */
    }
  }

 /*
  * Test write speed...
  */

  if ((fp = fopen("test.raster", "wb")) == NULL)
  {
    perror("Unable to create test.raster");
    return (0.0);
  }

  if ((r = cupsRasterOpen(fileno(fp), CUPS_RASTER_WRITE)) == NULL)
  {
    perror("Unable to create raster output stream");
    fclose(fp);
    return (0.0);
  }

  gettimeofday(&start, NULL);

  for (page = 0; page < 16; page ++)
  {
    memset(&header, 0, sizeof(header));
    header.cupsWidth        = 1024;
    header.cupsHeight       = 1024;
    header.cupsBytesPerLine = 1024;

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

    cupsRasterWriteHeader(r, &header);

    for (y = 0; y < 1024; y ++)
    {
      if (do_random)
        cupsRasterWritePixels(r, data[y & 1], header.cupsBytesPerLine);
      else if (y & 128)
        cupsRasterWritePixels(r, data[1], header.cupsBytesPerLine);
      else
        cupsRasterWritePixels(r, data[0], header.cupsBytesPerLine);
    }
  }

  cupsRasterClose(r);

  gettimeofday(&end, NULL);

  fclose(fp);

  return (end.tv_sec - start.tv_sec +
          0.000001 * (end.tv_usec - start.tv_usec));
}


/*
 * End of "$Id$".
 */
