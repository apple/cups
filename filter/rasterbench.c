/*
 * "$Id: rasterbench.c 7376 2008-03-19 21:07:45Z mike $"
 *
 *   Raster benchmark program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   main()           - Benchmark the raster read/write functions.
 *   compute_median() - Compute the median time for a test.
 *   read_test()      - Benchmark the raster read functions.
 *   write_test()     - Benchmark the raster write functions.
 */

/*
 * Include necessary headers...
 */

#include <config.h>
#include <cups/raster.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>


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
static double	get_time(void);
static void	read_test(int fd);
static int	run_read_test(void);
static void	write_test(int fd, cups_mode_t mode);


/*
 * 'main()' - Benchmark the raster read/write functions.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		ras_fd,			/* File descriptor for read process */
		status;			/* Exit status of read process */
  double	start_secs,		/* Start time */
		write_secs,		/* Write time */
		read_secs,		/* Read time */
		pass_secs[TEST_PASSES];	/* Total test times */
  cups_mode_t	mode;			/* Write mode */


 /*
  * See if we have anything on the command-line...
  */

  if (argc > 2 || (argc == 2 && strcmp(argv[1], "-z")))
  {
    puts("Usage: rasterbench [-z]");
    return (1);
  }

  mode = argc > 1 ? CUPS_RASTER_WRITE_COMPRESSED : CUPS_RASTER_WRITE;

 /*
  * Ignore SIGPIPE...
  */

  signal(SIGPIPE, SIG_IGN);

 /*
  * Run the tests several times to get a good average...
  */

  printf("Test read/write speed of %d pages, %dx%d pixels...\n\n",
         TEST_PAGES, TEST_WIDTH, TEST_HEIGHT);
  for (i = 0; i < TEST_PASSES; i ++)
  {
    printf("PASS %2d: ", i + 1);
    fflush(stdout);

    ras_fd     = run_read_test();
    start_secs = get_time();

    write_test(ras_fd, mode);

    write_secs = get_time();
    printf(" %.3f write,", write_secs - start_secs);
    fflush(stdout);

    close(ras_fd);
    wait(&status);

    read_secs    = get_time();
    pass_secs[i] = read_secs - start_secs;
    printf(" %.3f read, %.3f total\n", read_secs - write_secs, pass_secs[i]);
  }

  printf("\nMedian Total Time: %.3f seconds per document\n",
         compute_median(pass_secs));

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
 * 'get_time()' - Get the current time in seconds.
 */

static double				/* O - Time in seconds */
get_time(void)
{
  struct timeval	curtime;	/* Current time */


  gettimeofday(&curtime, NULL);
  return (curtime.tv_sec + 0.000001 * curtime.tv_usec);
}


/*
 * 'read_test()' - Benchmark the raster read functions.
 */

static void
read_test(int fd)			/* I - File descriptor to read from */
{
  int			y;		/* Looping var */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  unsigned char		buffer[8 * TEST_WIDTH];
					/* Read buffer */


 /*
  * Test read speed...
  */

  if ((r = cupsRasterOpen(fd, CUPS_RASTER_READ)) == NULL)
  {
    perror("Unable to create raster input stream");
    return;
  }

  while (cupsRasterReadHeader2(r, &header))
  {
    for (y = 0; y < header.cupsHeight; y ++)
      cupsRasterReadPixels(r, buffer, header.cupsBytesPerLine);
  }

  cupsRasterClose(r);
}


/*
 * 'run_read_test()' - Run the read test as a child process via pipes.
 */

static int				/* O - Standard input of child */
run_read_test(void)
{
  int	ras_pipes[2];			/* Raster data pipes */
  int	pid;				/* Child process ID */


  if (pipe(ras_pipes))
    return (-1);

  if ((pid = fork()) < 0)
  {
   /*
    * Fork error - return -1 on error...
    */

    close(ras_pipes[0]);
    close(ras_pipes[1]);

    return (-1);
  }
  else if (pid == 0)
  {
   /*
    * Child comes here - read data from the input pipe...
    */

    close(ras_pipes[1]);
    read_test(ras_pipes[0]);
    exit(0);
  }
  else
  {
   /*
    * Parent comes here - return the output pipe...
    */

    close(ras_pipes[0]);
    return (ras_pipes[1]);
  }
}


/*
 * 'write_test()' - Benchmark the raster write functions.
 */

static void
write_test(int         fd,		/* I - File descriptor to write to */
           cups_mode_t mode)		/* I - Write mode */
{
  int			page, x, y;	/* Looping vars */
  int			count;		/* Number of bytes to set */
  cups_raster_t		*r;		/* Raster stream */
  cups_page_header2_t	header;		/* Page header */
  unsigned char		data[32][8 * TEST_WIDTH];
					/* Raster data to write */


 /*
  * Create a combination of random data and repeated data to simulate
  * text with some whitespace.
  */

  CUPS_SRAND(time(NULL));

  memset(data, 0, sizeof(data));

  for (y = 0; y < 28; y ++)
  {
    for (x = CUPS_RAND() & 127, count = (CUPS_RAND() & 15) + 1;
         x < sizeof(data[0]);
         x ++, count --)
    {
      if (count <= 0)
      {
	x     += (CUPS_RAND() & 15) + 1;
	count = (CUPS_RAND() & 15) + 1;

        if (x >= sizeof(data[0]))
	  break;
      }

      data[y][x] = CUPS_RAND();
    }
  }

 /*
  * Test write speed...
  */

  if ((r = cupsRasterOpen(fd, mode)) == NULL)
  {
    perror("Unable to create raster output stream");
    return;
  }

  for (page = 0; page < TEST_PAGES; page ++)
  {
    memset(&header, 0, sizeof(header));
    header.cupsWidth        = TEST_WIDTH;
    header.cupsHeight       = TEST_HEIGHT;
    header.cupsBytesPerLine = TEST_WIDTH;

    if (page & 1)
    {
      header.cupsBytesPerLine *= 4;
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

    for (y = 0; y < TEST_HEIGHT; y ++)
      cupsRasterWritePixels(r, data[y & 31], header.cupsBytesPerLine);
  }

  cupsRasterClose(r);
}


/*
 * End of "$Id: rasterbench.c 7376 2008-03-19 21:07:45Z mike $".
 */
