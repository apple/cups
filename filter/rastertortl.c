/*
 * "$Id: rastertortl.c,v 1.1.2.2 2002/09/26 09:56:17 mike Exp $"
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
#define MODEL_CMYK	32		/* Supports CMYK graphics */
#define MODEL_ENCAD	64		/* Supports ENCAD quality modes */


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
					/* Switch to PCL mode */
#define pcl_set_pcl_mode(m)\
	printf("\033%%%dA", (m))
					/* Switch to HP-GL/2 mode */
#define pcl_set_hpgl_mode(m)\
	printf("\033%%%dB", (m))


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

  if (ModelNumber & MODEL_PJL)
  {
   /*
    * Send PJL setup commands...
    */

    pjl_escape();

    if (header->cupsColorSpace == CUPS_CSPACE_K)
      puts("@PJL SET RENDERMODE = GRAYSCALE\r");
    else
      puts("@PJL SET RENDERMODE = COLOR\r");

    if (strcmp(header->OutputType, "Best") == 0)
      puts("@PJL SET MAXDETAIL = ON\r");
    else
      puts("@PJL SET MAXDETAIL = OFF\r");

    printf("@PJL SET RESOLUTION = %d\r\n", header->HWResolution[0]);

    if (ModelNumber & MODEL_PJL_EXT)
    {
      puts("@PJL SET COLORSPACE = SRGB\r");
      puts("@PJL SET RENDERINTENT = PERCEPTUAL\r");

      printf("@PJL SET PAPERLENGTH = %d\r\n", header->PageSize[1] * 10);
      printf("@PJL SET PAPERWIDTH = %d\r\n", header->PageSize[0] * 10);
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
  * Set graphics mode...
  */

  if (ModelNumber & MODEL_CRD)
  {
   /*
    * Set the print quality...
    */

    if (strcmp(header->OutputType, "Draft") == 0)
      printf("\033*o-1M");
    else if (strcmp(header->OutputType, "Normal") == 0)
      printf("\033*o0M");
    else if (strcmp(header->OutputType, "Best") == 0)
      printf("\033*o1M");

   /*
    * Send 12-byte configure raster data command with horizontal and
    * vertical resolutions as well as a color count...
    */

    printf("\033&u%dD", header->HWResolution[0]);
    printf("\033*p0Y\033*p0X");			/* Set top of form */

    printf("\033*g12W");
    putchar(6);					/* Format 6 */
    putchar(0x1f);				/* SP ???? */
    putchar(0x00);				/* Number components */
    putchar(0x01);				/* (1 for RGB) */

    putchar(header->HWResolution[0] >> 8);	/* Horizontal resolution */
    putchar(header->HWResolution[0]);
    putchar(header->HWResolution[1] >> 8);	/* Vertical resolution */
    putchar(header->HWResolution[1]);

    putchar(header->cupsCompression);		/* Compression mode 3 or 10 */
    putchar(0x01);				/* Portrait orientation */
    putchar(0x20);				/* Bits per pixel (32 = RGB) */
    putchar(0x01);				/* Planes per pixel (1 = chunky RGB) */

    NumPlanes = 1;
  }
  else
  {
   /*
    * Set the print quality...
    */

    if (ModelNumber & MODEL_ENCAD)
    {
      if (strcmp(header->OutputType, "Draft") == 0)
	printf("QM,5698,25,1;");
      else if (strcmp(header->OutputType, "Normal") == 0)
	printf("QM,5698,25,2;");
      else if (strcmp(header->OutputType, "Best") == 0)
	printf("QM,5698,25,4;");

      printf("QM,5698,30,%d,0;", header->cupsMediaType);
    }
    else
    {
      if (strcmp(header->OutputType, "Draft") == 0)
	printf("QM0;");
      else if (strcmp(header->OutputType, "Normal") == 0)
	printf("QM50;");
      else if (strcmp(header->OutputType, "Best") == 0)
	printf("QM100;");
    }

   /*
    * Set media size, position, type, etc...
    */

    printf("BP5,0;");
    printf("PS%.0f,%.0f;",
	   header->cupsHeight * 1016.0 / header->HWResolution[1],
	   header->cupsWidth * 1016.0 / header->HWResolution[0]);
    printf("PU;");
    printf("PA0,0;");

    printf("MT%d;", header->cupsMediaType);

    if (header->CutMedia == CUPS_CUT_PAGE)
      printf("EC;");
    else
      printf("EC0;");

   /*
    * Set the appropriate graphics mode...
    */

    if (ModelNumber & MODEL_ENCAD)
      pcl_set_pcl_mode(2);
    else
      pcl_set_pcl_mode(0);

    printf("\033&a1N");				/* Set negative motion */

    printf("\033*t%dR", header->HWResolution[0]);/* Set resolution */

    if (header->cupsColorSpace == CUPS_CSPACE_RGB)
    {
      NumPlanes = 3;

      fwrite("\033*v6W\0\3\0\10\10\10", 11, 1, stdout);
    }
    else if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
    {
      NumPlanes = 4;

      if (ModelNumber & MODEL_CMYK)
        printf("\033*r-4U");			/* Set KCMY graphics */
    }
    else
      NumPlanes = 1;
  }

 /*
  * Set size and position of graphics...
  */

  printf("\033*r1A");				/* Start graphics */

  if (!(ModelNumber & MODEL_CRD))
  {
    printf("\033*r%dS", header->cupsWidth);	/* Set width */
    printf("\033*r%dT", header->cupsHeight);	/* Set height */
  }

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
  {
    SeedBuffer = calloc(header->cupsBytesPerLine, 1);

    if (header->cupsCompression == 10)
      memset(SeedBuffer, 0xff, header->cupsBytesPerLine);
  }
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
  * End graphics...
  */

  if (ModelNumber & MODEL_END_COLOR)
     printf("\033*rC");			/* End color GFX */
  else
     printf("\033*r0B");		/* End GFX */

 /*
  * Eject the current page...
  */

  if (ModelNumber & MODEL_CRD)
    putchar(12);
  else
  {
    pcl_set_hpgl_mode(0);
    printf("PG;");
  }

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
  * Reset and end the job...
  */

  pcl_reset();

  if (ModelNumber & MODEL_PJL)
  {
    pjl_escape();
    puts("@PJL EOJ\r");
  }
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

  for (i = NumPlanes * 8000; i > 0; i --)
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
        	*start,			/* Start of compression sequence */
		*seed;			/* Seed buffer pointer */
  int           count,			/* Count of bytes for output */
		offset,			/* Offset of bytes for output */
		temp;			/* Temporary count */
  int		r, g, b;		/* RGB deltas for mode 10 compression */


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

    case 3 :
       /*
	* Do delta-row compression...
	*/

	line_ptr = line;
	line_end = line + length;

	comp_ptr = CompBuffer;
	seed     = SeedBuffer + plane * length;

	while (line_ptr < line_end)
        {
         /*
          * Find the next non-matching sequence...
          */

          start = line_ptr;
          while (*line_ptr == *seed &&
                 line_ptr < line_end)
          {
            line_ptr ++;
            seed ++;
          }

          if (line_ptr == line_end)
            break;

          offset = line_ptr - start;

         /*
          * Find up to 8 non-matching bytes...
          */

          start = line_ptr;
          count = 0;
          while (*line_ptr != *seed &&
                 line_ptr < line_end &&
                 count < 8)
          {
            line_ptr ++;
            seed ++;
            count ++;
          }

         /*
          * Place mode 3 compression data in the buffer; see HP manuals
          * for details...
          */

          if (offset >= 31)
          {
           /*
            * Output multi-byte offset...
            */

            *comp_ptr++ = ((count - 1) << 5) | 31;

            offset -= 31;
            while (offset >= 255)
            {
              *comp_ptr++ = 255;
              offset    -= 255;
            }

            *comp_ptr++ = offset;
          }
          else
          {
           /*
            * Output single-byte offset...
            */

            *comp_ptr++ = ((count - 1) << 5) | offset;
          }

          memcpy(comp_ptr, start, count);
          comp_ptr += count;
        }

	line_ptr = CompBuffer;
	line_end = comp_ptr;

        memcpy(SeedBuffer + plane * length, line, length);
	break;

    case 10 :
       /*
        * Mode 10 "near lossless" RGB compression...
	*/

	line_ptr = line;
	line_end = line + length;

	comp_ptr = CompBuffer;
	seed     = SeedBuffer;

	while (line_ptr < line_end)
        {
         /*
          * Find the next non-matching sequence...
          */

          start = line_ptr;
          while (line_ptr[0] == seed[0] &&
                 line_ptr[1] == seed[1] &&
                 line_ptr[2] == seed[2] &&
                 (line_ptr + 2) < line_end)
          {
            line_ptr += 3;
            seed += 3;
          }

          if (line_ptr == line_end)
            break;

          offset = (line_ptr - start) / 3;

         /*
          * Find up to 8 non-matching RGB tuples...
          */

          start = line_ptr;
          while ((line_ptr[0] != seed[0] ||
                  line_ptr[1] != seed[1] ||
                  line_ptr[2] != seed[2]) &&
                 (line_ptr + 2) < line_end)
          {
            line_ptr += 3;
            seed += 3;
          }

          count = (line_ptr - start) / 3;

         /*
          * Place mode 10 compression data in the buffer; each sequence
	  * starts with a command byte that looks like:
	  *
	  *     CMD SRC SRC OFF OFF CNT CNT CNT
	  *
	  * For the purpose of this driver, CMD and SRC are always 0.
	  *
	  * If the offset >= 3 then additional offset bytes follow the
	  * first command byte, each byte == 255 until the last one.
	  *
	  * If the count >= 7, then additional count bytes follow each
	  * group of pixels, each byte == 255 until the last one.
	  *
	  * The offset and count are in RGB tuples (not bytes, as for
	  * Mode 3 and 9)...
          */

          if (offset >= 3)
          {
           /*
            * Output multi-byte offset...
            */

            if (count > 7)
	      *comp_ptr++ = 0x1f;
	    else
	      *comp_ptr++ = 0x18 | (count - 1);

            offset -= 3;
            while (offset >= 255)
            {
              *comp_ptr++ = 255;
              offset      -= 255;
            }

            *comp_ptr++ = offset;
          }
          else
          {
           /*
            * Output single-byte offset...
            */

            if (count > 7)
	      *comp_ptr++ = (offset << 3) | 0x07;
	    else
	      *comp_ptr++ = (offset << 3) | (count - 1);
          }

	  temp = count - 8;
	  seed -= count * 3;

          while (count > 0)
	  {
	    if (count <= temp)
	    {
	     /*
	      * This is exceedingly lame...  The replacement counts
	      * are intermingled with the data...
	      */

              if (temp >= 255)
        	*comp_ptr++ = 255;
              else
        	*comp_ptr++ = temp;

              temp -= 255;
	    }

           /*
	    * Get difference between current and see pixels...
	    */

            r = start[0] - seed[0];
	    g = start[1] - seed[1];
	    b = ((start[2] & 0xfe) - (seed[2] & 0xfe)) / 2;

            if (r < -16 || r > 15 || g < -16 || g > 15 || b < -16 || b > 15)
	    {
	     /*
	      * Pack 24-bit RGB into 23 bits...  Lame...
	      */

	      *comp_ptr++ = start[0] >> 1;

	      if (start[0] & 1)
		*comp_ptr++ = 0x80 | (start[1] >> 1);
	      else
		*comp_ptr++ = start[1] >> 1;

	      if (start[1] & 1)
		*comp_ptr++ = 0x80 | (start[2] >> 1);
	      else
		*comp_ptr++ = start[2] >> 1;
            }
	    else
	    {
	     /*
	      * Pack 15-bit RGB difference...
	      */

              *comp_ptr++ = 0x80 | ((r << 2) & 0x7c) | ((g >> 3) & 0x03);
	      *comp_ptr++ = ((g << 5) & 0xe0) | (b & 0x1f);
	    }

            count --;
	    start += 3;
	    seed += 3;
          }

         /*
	  * Make sure we have the ending count if the replacement count
	  * was exactly 8 + 255n...
	  */

	  if (temp == 0)
	    *comp_ptr++ = 0;
        }

	line_ptr = CompBuffer;
	line_end = comp_ptr;

        memcpy(SeedBuffer, line, length);
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
		bytes;			/* Bytes per plane */
  unsigned char	*ptr;			/* Pointer into pixel buffer */


 /*
  * Write bitmap data as needed...
  */

  bytes = header->cupsBytesPerLine / NumPlanes;
  ptr   = PixelBuffer;

  for (plane = 0; plane < NumPlanes; plane ++, ptr += bytes)
    CompressData(ptr, bytes, plane < (NumPlanes - 1) ? 'V' : 'W',
		 header->cupsCompression);

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

  if ((ppd = ppdOpenFile(getenv("PPD"))) == NULL)
    ModelNumber = MODEL_CID;
  else
  {
    ModelNumber = ppd->model_number;

    ppdClose(ppd);
  }

  Setup(atoi(argv[1]), argv[2], argv[3]);

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
 * End of "$Id: rastertortl.c,v 1.1.2.2 2002/09/26 09:56:17 mike Exp $".
 */
