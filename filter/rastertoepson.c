/*
 * "$Id: rastertoepson.c,v 1.1 2000/01/20 22:33:11 mike Exp $"
 *
 *   EPSON ESC/P and ESC/P2 filter for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 1993-2000 by Easy Software Products.
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
 * Contents:
 *
 *   Setup()        - Prepare the printer for printing.
 *   StartPage()    - Start a page of graphics.
 *   EndPage()      - Finish a page of graphics.
 *   Shutdown()     - Shutdown the printer.
 *   CompressData() - Compress a line of graphics.
 *   OutputLine()   - Output a line of graphics.
 *   main()         - Main entry and processing of driver.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/string.h>
#include "raster.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * Macros...
 */

#define pwrite(s,n) fwrite((s), 1, (n), stdout)


/*
 * Globals...
 */

unsigned char	*Planes[6],		/* Output buffers */
		*CompBuffer;		/* Compression buffer */
int		NumPlanes,		/* Number of color planes */
		Feed;			/* Number of lines to skip */


/*
 * Prototypes...
 */

void	Setup(void);
void	StartPage(cups_page_header_t *header, ppd_file_t *ppd);
void	EndPage(cups_page_header_t *header);
void	Shutdown(void);

void	CompressData(unsigned char *line, int length, int plane, int type,
	              int xstep, int ystep);
void	OutputLine(cups_page_header_t *header);


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(void)
{
 /*
  * Send a reset sequence.
  */

  printf("\033@");
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(cups_page_header_t *header,	/* I - Page header */
          ppd_file_t         *ppd)	/* I - PPD file */
{
  int	n, t;				/* Numbers */
  int	plane;				/* Looping var */


 /*
  * Send a reset sequence.
  */

  printf("\033@");

 /*
  * Set the media size...
  */

  n = header->PageSize[1] * header->HWResolution[1] / 72.0;

  pwrite("\033(C\002\000", 5);	/* Page length */
  putchar(n);
  putchar(n >> 8);

  t = (ppd->sizes[1].length - ppd->sizes[1].top) *
      header->HWResolution[1] / 72.0;

  pwrite("\033(c\004\000", 5);	/* Top & bottom margins */
  putchar(t);
  putchar(t >> 8);
  putchar(n);
  putchar(n >> 8);

 /*
  * Set graphics mode...
  */

  if (header->cupsColorSpace == CUPS_CSPACE_CMY)
    NumPlanes = 3;
  else if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
    NumPlanes = 4;
  else if (header->cupsColorSpace == CUPS_CSPACE_KCMYcm)
    NumPlanes = 6;
  else
    NumPlanes = 1;

  pwrite("\033(G\001\000\001", 6);	/* Graphics mode */
  pwrite("\033(U\001\000", 5);		/* Resolution */
  putchar(3600 / header->HWResolution[1]);

  if (header->HWResolution[1] == 720)
  {
    pwrite("\033(i\001\000\001", 6);	/* Microweave */
    pwrite("\033(e\002\000\000\001", 7);/* Small dots */
  }

  pwrite("\033(V\002\000", 5);		/* Set absolute position */
  putchar(t);
  putchar(t >> 8);

  Feed = 0;				/* No blank lines yet */

 /*
  * Allocate memory for a line of graphics...
  */

  Planes[0] = malloc(header->cupsBytesPerLine);
  for (plane = 1; plane < NumPlanes; plane ++)
    Planes[plane] = Planes[0] + plane * header->cupsBytesPerLine / NumPlanes;

  if (header->cupsCompression)
    CompBuffer = malloc(header->cupsBytesPerLine * 2);
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */

void
EndPage(cups_page_header_t *header)	/* I - Page header */
{
 /*
  * Eject the current page...
  */

  putchar(12);			/* Form feed */

 /*
  * Free memory...
  */

  free(Planes[0]);

  if (header->cupsCompression)
    free(CompBuffer);
}


/*
 * 'Shutdown()' - Shutdown the printer.
 */

void
Shutdown(void)
{
 /*
  * Send a reset sequence.
  */

  printf("\033@");
}


/*
 * 'CompressData()' - Compress a line of graphics.
 */

void
CompressData(unsigned char *line,	/* I - Data to compress */
             int           length,	/* I - Number of bytes */
	     int           plane,	/* I - Color plane */
	     int           type,	/* I - Type of compression */
	     int           xstep,	/* I - X resolution */
	     int           ystep)	/* I - Y resolution */
{
  unsigned char	*line_ptr,		/* Current byte pointer */
        	*line_end,		/* End-of-line byte pointer */
        	*comp_ptr,		/* Pointer into compression buffer */
        	*start;			/* Start of compression sequence */
  int           count;			/* Count of bytes for output */
  static int	ctable[6] = { 0, 2, 1, 4, 2, 1 };
					/* KCMYcm color values */


  switch (type)
  {
    case 0 :
       /*
	* Do no compression...
	*/

	line_ptr = line;
	line_end = line + length;
	break;

    case 1 :
       /*
        * Do TIFF pack-bits encoding...
        */

	line_ptr = line;
	line_end = line + length;
	comp_ptr = CompBuffer;

	while (line_ptr < line_end)
	{
	  if ((line_ptr + 1) >= line_end)
	  {
	   /*
	    * Single byte on the end...
	    */

	    *comp_ptr++ = 0x00;
	    *comp_ptr++ = *line_ptr++;
	  }
	  else if (line_ptr[0] == line_ptr[1])
	  {
	   /*
	    * Repeated sequence...
	    */

	    line_ptr ++;
	    count = 2;

	    while (line_ptr < (line_end - 1) &&
        	   line_ptr[0] == line_ptr[1] &&
        	   count < 127)
	    {
              line_ptr ++;
              count ++;
	    }

	    *comp_ptr++ = 257 - count;
	    *comp_ptr++ = *line_ptr++;
	  }
	  else
	  {
	   /*
	    * Non-repeated sequence...
	    */

	    start    = line_ptr;
	    line_ptr ++;
	    count    = 1;

	    while (line_ptr < (line_end - 1) &&
        	   line_ptr[0] != line_ptr[1] &&
        	   count < 127)
	    {
              line_ptr ++;
              count ++;
	    }

	    *comp_ptr++ = count - 1;

	    memcpy(comp_ptr, start, count);
	    comp_ptr += count;
	  }
	}

        line_ptr = CompBuffer;
        line_end = comp_ptr;
	break;
  }

 /*
  * Set the color if necessary...
  */

  if (NumPlanes > 1)
  {
    if (plane > 3)
      printf("\033(r%c%c%c%c", 2, 0, 1, ctable[plane]);
					/* Set extended color */
    else if (NumPlanes == 3)
      printf("\033r%c", ctable[plane + 1]);
					/* Set color */
    else
      printf("\033r%c", ctable[plane]);	/* Set color */
  }

 /*
  * Send a raster plane...
  */

  putchar(0x0d);			/* Move print head to left margin */

  length *= 8;
  printf("\033.");			/* Raster graphics */
  putchar(type);
  putchar(ystep);
  putchar(xstep);
  putchar(1);
  putchar(length);
  putchar(length >> 8);

  pwrite(line_ptr, line_end - line_ptr);
}


/*
 * 'OutputLine()' - Output a line of graphics.
 */

void
OutputLine(cups_page_header_t *header)	/* I - Page header */
{
  int	plane;				/* Current plane */
  int	xstep, ystep;			/* X & Y resolutions */

 /*
  * Output whitespace as needed...
  */

  if (Feed > 0)
  {
    pwrite("\033(v\002\000", 5);	/* Relative vertical position */
    putchar(Feed);
    putchar(Feed >> 8);

    Feed = 0;
  }

 /*
  * Write bitmap data as needed...
  */

  xstep = 3600 / header->HWResolution[0];
  ystep = 3600 / header->HWResolution[1];

  for (plane = 0; plane < NumPlanes; plane ++)
    CompressData(Planes[plane], header->cupsBytesPerLine / NumPlanes, plane,
		 header->cupsCompression, xstep, ystep);

  Feed ++;
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int			fd;	/* File descriptor */
  cups_raster_t		*ras;	/* Raster stream for printing */
  cups_page_header_t	header;	/* Page header from file */
  ppd_file_t		*ppd;	/* PPD file */
  int			page;	/* Current page */
  int			y;	/* Current line */


 /*
  * Check for valid arguments...
  */

  if (argc < 6 || argc > 7)
  {
   /*
    * We don't have the correct number of arguments; write an error message
    * and return.
    */

    fputs("ERROR: rastertoepson job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) == -1)
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

  ppd = ppdOpenFile(getenv("PPD"));

  Setup();

 /*
  * Process pages as needed...
  */

  page = 0;

  while (cupsRasterReadHeader(ras, &header))
  {
   /*
    * Write a status message with the page number and number of copies.
    */

    page ++;

    fprintf(stderr, "PAGE: %d %d\n", page, header.NumCopies);

   /*
    * Start the page...
    */

    StartPage(&header, ppd);

   /*
    * Loop for each line on the page...
    */

    for (y = 0; y < header.cupsHeight; y ++)
    {
     /*
      * Let the user know how far we have progressed...
      */

      if ((y & 127) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", page,
	        100 * y / header.cupsHeight);

     /*
      * Read a line of graphics...
      */

      if (cupsRasterReadPixels(ras, Planes[0], header.cupsBytesPerLine) < 1)
        break;

     /*
      * See if the line is blank; if not, write it to the printer...
      */

      if (Planes[0][0] ||
          memcmp(Planes[0], Planes[0] + 1, header.cupsBytesPerLine - 1))
        OutputLine(&header);
      else
        Feed ++;
    }

   /*
    * Eject the page...
    */

    EndPage(&header);
  }

 /*
  * Shutdown the printer...
  */

  Shutdown();

  ppdClose(ppd);

 /*
  * Close the raster stream...
  */

  cupsRasterClose(ras);
  if (fd != 0)
    close(fd);

 /*
  * If no pages were printed, send an error message...
  */

  if (page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: Ready to print.\n", stderr);

  return (page == 0);
}


/*
 * End of "$Id: rastertoepson.c,v 1.1 2000/01/20 22:33:11 mike Exp $".
 */
