/*
 * "$Id: rastertopcl.c,v 1.1 1999/05/15 03:39:34 mike Exp $"
 *
 *   Hewlett-Packard Page Control Language and Raster Transfer Language
 *   filter for ESP Print.
 *
 *   Copyright 1993-1999 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law. They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/raster.h>
#include <cups/string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * Globals...
 */

unsigned char	*Planes[4],		/* Output buffers */
		*CompBuffer;		/* Compression buffer */
int		NumPlanes,		/* Number of color planes */
		Feed;			/* Number of lines to skip */


/*
 * Prototypes...
 */

void	Setup(void);
void	StartPage(cups_page_header_t *header);
void	EndPage(cups_page_header_t *header);
void	Shutdown(void);

void	CompressData(unsigned char *line, int length, int plane, int type);
void	OutputLine(cups_page_header_t *header);


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(void)
{
 /*
  * Send a PCL reset sequence.
  */

  putchar(0x1b);
  putchar('E');
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(cups_page_header_t *header)	/* I - Page header */
{
  int	plane;				/* Looping var */


 /*
  * Set the media type, position, and size...
  */

  printf("\033&l6D\033&k12H");			/* Set 6 LPI, 10 CPI */
  printf("\033&l%.2fP",				/* Set page length */
         header->PageSize[1] / 12.0);
  printf("\033&l%dX", header->NumCopies);	/* Set number copies */
  if (header->MediaPosition)
    printf("\033&l%dH", header->MediaPosition);	/* Set media position */
  if (header->cupsMediaType)
    printf("\033&l%dM",				/* Set media type */
           header->cupsMediaType);

 /*
  * Set graphics mode...
  */

  if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
  {
    NumPlanes = 4;
    printf("\033*r-4U");			/* Set KCMY graphics */
  }
  else
    NumPlanes = 1;

  printf("\033*t%dR", header->HWResolution[0]);	/* Set resolution */
  printf("\033*r%dS", header->cupsWidth);	/* Set width */
  printf("\033*r%dT", header->cupsHeight);	/* Set height */
  printf("\033*r0A");				/* Start graphics */

  if (header->cupsCompression)
    printf("\033*b%dM",				/* Set compression */
           header->cupsCompression);

  Feed = 0;					/* No blank lines yet */

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

  if (NumPlanes > 1)
  {
     printf("\033*rC");			/* End color GFX */
     printf("\033&l0H");		/* Eject current page */
  }
  else
  {
     printf("\033*r0B");		/* End GFX */
     printf("\014");			/* Eject currnet page */
  }

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
  * Send a PCL reset sequence.
  */

  putchar(0x1b);
  putchar('E');
}


/*
 * 'CompressData()' - Compress a line of graphics.
 */

void
CompressData(unsigned char *line,	/* I - Data to compress */
             int           length,	/* I - Number of bytes */
	     int           plane,	/* I - Color plane */
	     int           type)	/* I - Type of compression */
{
  unsigned char	*line_ptr,		/* Current byte pointer */
        	*line_end,		/* End-of-line byte pointer */
        	*comp_ptr,		/* Pointer into compression buffer */
        	*start;			/* Start of compression sequence */
  int           count;			/* Count of bytes for output */


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
        * Do run-length encoding...
        */

	line_end = line + length;
	for (line_ptr = line, comp_ptr = CompBuffer;
	     line_ptr < line_end;
	     comp_ptr += 2, line_ptr += count)
	{
	  for (count = 1;
               (line_ptr + count) < line_end &&
	           line_ptr[0] == line_ptr[count] &&
        	   count < 256;
               count ++);

	  comp_ptr[0] = count - 1;
	  comp_ptr[1] = line_ptr[0];
	}

        line_ptr = CompBuffer;
        line_end = comp_ptr;
	break;

    case 2 :
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
  * Set the length of the data and write a raster plane...
  */

  printf("\033*b%d%c", line_end - line_ptr, plane);
  fwrite(line_ptr, line_end - line_ptr, 1, stdout);
}


/*
 * 'OutputLine()' - Output a line of graphics.
 */

void
OutputLine(cups_page_header_t *header)	/* I - Page header */
{
  int	plane;	/* Current plane */


 /*
  * Output whitespace as needed...
  */

  if (Feed > 0)
  {
    printf("\033*b%dY", Feed);
    Feed = 0;
  }

 /*
  * Write bitmap data as needed...
  */

  for (plane = 0; plane < NumPlanes; plane ++)
    CompressData(Planes[plane], header->cupsBytesPerLine / NumPlanes,
		 plane < (NumPlanes - 1) ? 'V' : 'W',
		 header->cupsCompression);
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
  int			page;	/* Current page */
  int			y;	/* Current line */


 /*
  * Check for valid arguments...
  */

  if (argc < 6 || argc > 7)
  {
   /*
    * We don't have the correct number of arguments; write an error message
    * and then sleep for 1 second to give the scheduler a chance to read
    * the message.
    */

    fputs("ERROR: rastertopcl job-id user title copies options [file]\n", stderr);
    sleep(1);
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

    StartPage(&header);

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

 /*
  * Sleep for 1 second and return...
  */

  sleep(1);
  return (page == 0);
}


/*
 * End of "$Id: rastertopcl.c,v 1.1 1999/05/15 03:39:34 mike Exp $".
 */
