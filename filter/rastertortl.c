/*
 * "$Id: rastertortl.c,v 1.1.2.1 2002/09/26 01:11:10 mike Exp $"
 *
 *   Hewlett-Packard Raster Transfer Language filter for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1993-2002 by Easy Software Products.
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
 * Model capability bits for this driver...
 */

#define MODEL_PJL	1		/* Supports PJL commands */
#define MODEL_PJL_EXT	2		/* Supports extended PJL commands */
#define MODEL_END_COLOR	4		/* Supports ESC * r C */
#define MODEL_CID	8		/* Supports CID command */
#define MODEL_CRD	16		/* Supports CRD command */


/*
 * Macros for common PJL/PCL commands...
 */

					/* Enter PJL mode */
#define pjl_escape()\
	printf("\033%%-12345X")
					/* Enter the specified language */
#define pjl_set_language(lang)\
	printf("@PJL ENTER LANGUAGE=%s\r\n", (lang))
					/* Reset the printer */
#define pcl_reset()\
	printf("\033E")


/*
 * Globals...
 */

unsigned char	*PixelBuffer,		/* Input pixel buffer */
		*CompBuffer,		/* Output compression buffer */
		*SeedBuffer;		/* Seed buffer */
int		NumPlanes,		/* Number of color planes */
		Page,			/* Page number */
		ModelNumber;		/* Model number bits */


/*
 * Prototypes...
 */

void	Setup(int job_id, const char *user, const char *title);
void	StartPage(cups_page_header_t *header);
void	EndPage(void);
void	Shutdown(void);

void	CancelJob(int sig);
void	CompressData(unsigned char *line, int length, int plane, int type);
void	OutputLine(cups_page_header_t *header);


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(int        job_id,		/* I - Job ID */
      const char *user,			/* I - User printing job */
      const char *title)		/* I - Title of job */
{
  if (ModelNumber & MODEL_PJL)
  {
   /*
    * Send PJL setup commands...
    */

    pjl_escape();

    if (ModelNumber & MODEL_PJL_EXT)
    {
      printf("@PJL SET MARGINS = SMALLER\r\n");
      printf("@PJL SET PRINTAREA = FULLSIZE\r\n");
      printf("@PJL JOB NAME = \"%d %s %s\"\r\n", job_id, user, title);
    }

   /*
    * Set the print language...
    */

    if (ModelNumber & MODEL_CRD)
      pjl_set_language("PCL3GUI");
    else
      pjl_set_language("HPGL2");
  }

 /*
  * Send reset sequences.
  */

  pcl_reset();

  if (!(ModelNumber & MODEL_CRD))
    printf("IN;");
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(cups_page_header_t *header)	/* I - Page header */
{
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


 /*
  * Set graphics mode...
  */

  printf("\033*t%dR", header->HWResolution[0]);	/* Set resolution */

  if (ppd->model_number == 2)
  {
   /*
    * Figure out the number of color planes...
    */

    if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
      NumPlanes = 4;
    else
      NumPlanes = 1;

   /*
    * Send 26-byte configure image data command with horizontal and
    * vertical resolutions as well as a color count...
    */

    printf("\033*g26W");
    putchar(2);					/* Format 2 */
    putchar(NumPlanes);				/* Output planes */

    putchar(header->HWResolution[0] >> 8);	/* Black resolution */
    putchar(header->HWResolution[0]);
    putchar(header->HWResolution[1] >> 8);
    putchar(header->HWResolution[1]);
    putchar(0);
    putchar(1 << ColorBits);			/* # of black levels */

    putchar(header->HWResolution[0] >> 8);	/* Cyan resolution */
    putchar(header->HWResolution[0]);
    putchar(header->HWResolution[1] >> 8);
    putchar(header->HWResolution[1]);
    putchar(0);
    putchar(1 << ColorBits);			/* # of cyan levels */

    putchar(header->HWResolution[0] >> 8);	/* Magenta resolution */
    putchar(header->HWResolution[0]);
    putchar(header->HWResolution[1] >> 8);
    putchar(header->HWResolution[1]);
    putchar(0);
    putchar(1 << ColorBits);			/* # of magenta levels */

    putchar(header->HWResolution[0] >> 8);	/* Yellow resolution */
    putchar(header->HWResolution[0]);
    putchar(header->HWResolution[1] >> 8);
    putchar(header->HWResolution[1]);
    putchar(0);
    putchar(1 << ColorBits);			/* # of yellow levels */
  }
  else
  {
    if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
    {
      NumPlanes = 4;
      printf("\033*r-4U");			/* Set KCMY graphics */
    }
    else if (header->cupsColorSpace == CUPS_CSPACE_CMY)
    {
      NumPlanes = 3;
      printf("\033*r-3U");			/* Set CMY graphics */
    }
    else
      NumPlanes = 1;				/* Black&white graphics */
  }

 /*
  * Set size and position of graphics...
  */

  printf("\033*r%dS", header->cupsWidth);	/* Set width */
  printf("\033*r%dT", header->cupsHeight);	/* Set height */

  printf("\033*r1A");				/* Start graphics */

  printf("\033*b%dM", header->cupsCompression);	/* Set compression */

 /*
  * Allocate memory for a line of graphics...
  */

  PixelBuffer = malloc(header->cupsBytesPerLine);

  if (header->cupsCompression)
    CompBuffer = malloc(header->cupsBytesPerLine * 2);
  else
    CompBuffer = NULL;

  if (header->cupsCompression >= 3)
    SeedBuffer = calloc(BytesPerLine, 1);
  else
    SeedBuffer = NULL;
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

  if (ModelNumber & MODEL_END_COLOR)
     printf("\033*rC");			/* End color GFX */
  else
     printf("\033*r0B");		/* End GFX */

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

  free(PixelBuffer);

  if (CompBuffer)
    free(CompBuffer);

  if (SeedBuffer)
    free(SeedBuffer);
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
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
  int	i;				/* Looping var */


  (void)sig;

 /*
  * Send out lots of NUL bytes to clear out any pending raster data...
  */

  for (i = 0; i < 600; i ++)
    putchar(0);

 /*
  * End the current page and exit...
  */

  EndPage();
  Shutdown();

  exit(0);
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
    default :
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
  int		plane,			/* Current plane */
		bytes,			/* Bytes to write */
		count;			/* Bytes to convert */
  unsigned char	bit,			/* Current plane data */
		bit0,			/* Current low bit data */
		bit1,			/* Current high bit data */
		*plane_ptr,		/* Pointer into Planes */
		*bit_ptr;		/* Pointer into BitBuffer */


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

  bytes = (header->cupsWidth + 7) / 8;

  for (plane = 0; plane < NumPlanes; plane ++)
    if (ColorBits == 1)
    {
     /*
      * Send bits as-is...
      */

      CompressData(Planes[plane], bytes, plane < (NumPlanes - 1) ? 'V' : 'W',
		   header->cupsCompression);
    }
    else
    {
     /*
      * Separate low and high bit data into separate buffers.
      */

      for (count = header->cupsBytesPerLine / NumPlanes,
               plane_ptr = Planes[plane], bit_ptr = BitBuffer;
	   count > 0;
	   count -= 2, plane_ptr += 2, bit_ptr ++)
      {
        bit = plane_ptr[0];

        bit0 = ((bit & 64) << 1) | ((bit & 16) << 2) | ((bit & 4) << 3) | ((bit & 1) << 4);
        bit1 = (bit & 128) | ((bit & 32) << 1) | ((bit & 8) << 2) | ((bit & 2) << 3);

        if (count > 1)
	{
	  bit = plane_ptr[1];

          bit0 |= (bit & 1) | ((bit & 4) >> 1) | ((bit & 16) >> 2) | ((bit & 64) >> 3);
          bit1 |= ((bit & 2) >> 1) | ((bit & 8) >> 2) | ((bit & 32) >> 3) | ((bit & 128) >> 4);
	}

        bit_ptr[0]     = bit0;
	bit_ptr[bytes] = bit1;
      }

     /*
      * Send low and high bits...
      */

      CompressData(BitBuffer, bytes, 'V', header->cupsCompression);
      CompressData(BitBuffer + bytes, bytes, plane < (NumPlanes - 1) ? 'V' : 'W',
		   header->cupsCompression);
    }

  fflush(stdout);
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  FILE			*fp;	/* Raster data file */
  cups_raster_t		*ras;	/* Raster stream for printing */
  cups_page_header_t	header;	/* Page header from file */
  int			y;	/* Current line */
  ppd_file_t		*ppd;	/* PPD file */


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

    fputs("ERROR: rastertortl job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fp = fopen(argv[6], "rb")) == NULL)
    {
      perror("ERROR: Unable to open raster file - ");
      sleep(1);
      return (1);
    }
  }
  else
    fp = stdin;

  ras = cupsRasterOpen(fp, CUPS_RASTER_READ);

 /*
  * Initialize the print device...
  */

  ppd = ppdOpenFile(getenv("PPD"));

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

    fprintf(stderr, "PAGE: %d %d\n", Page, header.NumCopies);

   /*
    * Start the page...
    */

    StartPage(ppd, &header);

   /*
    * Loop for each line on the page...
    */

    for (y = 0; y < header.cupsHeight; y ++)
    {
     /*
      * Let the user know how far we have progressed...
      */

      if ((y & 127) == 0)
        fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", Page,
	        100 * y / header.cupsHeight);

     /*
      * Read a line of graphics and write it out...
      */

      if (cupsRasterReadPixels(ras, PixelBuffer, header.cupsBytesPerLine) < 1)
        break;

      OutputLine(&header);
    }

   /*
    * Eject the page...
    */

    EndPage();
  }

 /*
  * Shutdown the printer...
  */

  Shutdown();

  if (ppd)
    ppdClose(ppd);

 /*
  * Close the raster stream...
  */

  cupsRasterClose(ras);
  if (fp != stdin)
    fclose(fp);

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
 * End of "$Id: rastertortl.c,v 1.1.2.1 2002/09/26 01:11:10 mike Exp $".
 */
