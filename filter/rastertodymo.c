/*
 * "$Id: rastertodymo.c,v 1.4.2.8 2003/11/05 19:20:43 mike Exp $"
 *
 *   DYMO label printer filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2001 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   Setup()        - Prepare the printer for printing.
 *   StartPage()    - Start a page of graphics.
 *   EndPage()      - Finish a page of graphics.
 *   Shutdown()     - Shutdown the printer.
 *   CancelJob()    - Cancel the current job...
 *   CompressData() - Compress a line of graphics.
 *   OutputLine()   - Output a line of graphics.
 *   main()         - Main entry and processing of driver.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include "raster.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


/*
 * Globals...
 */

unsigned char	*Buffer;		/* Output buffer */
int		Page,			/* Current page */
		Feed;			/* Number of lines to skip */


/*
 * Prototypes...
 */

void	Setup(void);
void	StartPage(cups_page_header_t *header);
void	EndPage(void);

void	CancelJob(int sig);

/**** MRS - supported resolutions = 136, 203, 300 ****/


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(void)
{
  int	i;		/* Looping var */


 /*
  * Clear any remaining data...
  */

  for (i = 0; i < 100; i ++)
    putchar(0x1b);

 /*
  * Reset the printer...
  */

  printf("\033@");
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(cups_page_header_t *header)	/* I - Page header */
{
  int	length;				/* Actual label length */


#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Register a signal handler to eject the current page if the
  * job is cancelled.
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, CancelJob);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = CancelJob;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, CancelJob);
#endif /* HAVE_SIGSET */

 /*
  * Setup printer/job attributes...
  */

  length = header->PageSize[1] * header->HWResolution[1] / 72;

  printf("\033L%c%c", length >> 8, length);
  printf("\033D%c", header->cupsBytesPerLine);

  printf("\033%c", header->cupsCompression + 'c'); /* Darkness */

 /*
  * Allocate memory for a line of graphics...
  */

  Buffer = malloc(header->cupsBytesPerLine);
  Feed   = 0;
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */

void
EndPage(void)
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Eject the current page...
  */

  printf("\033E");

  fflush(stdout);

 /*
  * Unregister the signal handler...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = SIG_IGN;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Free memory...
  */

  free(Buffer);
}


/*
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
  int	i;				/* Looping var */


  (void)sig;

 /*
  * Send out lots of ESC bytes to clear out any pending raster data...
  */

  for (i = 0; i < 100; i ++)
    putchar(0x1b);

 /*
  * End the current page and exit...
  */

  EndPage();

  exit(0);
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int			fd;	/* Raster data file */
  cups_raster_t		*ras;	/* Raster stream for printing */
  cups_page_header_t	header;	/* Page header from file */
  int			y;	/* Current line */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
   /*
    * We don't have the correct number of arguments; write an error message
    * and return.
    */

    fputs("ERROR: rastertodymo job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: Unable to open raster file - ");
      sleep(1);
      return (1);
    }
  }
  else
    fd = 0;

  ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

 /*
  * Initialize the print device...
  */

  Setup();

 /*
  * Process pages as needed...
  */

  Page = 0;

  while (cupsRasterReadHeader(ras, &header))
  {
   /*
    * Write a status message with the page number and number of copies.
    */

    Page ++;

    fprintf(stderr, "PAGE: %d 1\n", Page);

   /*
    * Start the page...
    */

    StartPage(&header);

   /*
    * Loop for each line on the page...
    */

    for (y = 0; y < header.cupsHeight; y ++)
    {
     /*
      * Let the user know how far we have progressed...
      */

      if ((y & 15) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", Page,
	        100 * y / header.cupsHeight);

     /*
      * Read a line of graphics...
      */

      if (cupsRasterReadPixels(ras, Buffer, header.cupsBytesPerLine) < 1)
        break;

     /*
      * See if the line is blank; if not, write it to the printer...
      */

      if (Buffer[0] ||
          memcmp(Buffer, Buffer + 1, header.cupsBytesPerLine - 1))
      {
        if (Feed)
	{
	  while (Feed > 255)
	  {
	    printf("\033f\001%c", 255);
	    Feed -= 255;
	  }

	  printf("\033f\001%c", Feed);
	  Feed = 0;
        }

        putchar(0x16);
	fwrite(Buffer, header.cupsBytesPerLine, 1, stdout);
	fflush(stdout);

#ifdef __sgi
       /*
        * This hack works around a bug in the IRIX serial port driver when
	* run at high baud rates (e.g. 115200 baud)...  This results in
	* slightly slower label printing, but at least the labels come
	* out properly.
	*/

	sginap(1);
#endif /* __sgi */
      }
      else
        Feed ++;
    }

   /*
    * Eject the page...
    */

    EndPage();
  }

 /*
  * Close the raster stream...
  */

  cupsRasterClose(ras);
  if (fd)
    close(fd);

 /*
  * If no pages were printed, send an error message...
  */

  if (Page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: " CUPS_SVERSION " is ready to print.\n", stderr);

  return (Page == 0);
}


/*
 * End of "$Id: rastertodymo.c,v 1.4.2.8 2003/11/05 19:20:43 mike Exp $".
 */
