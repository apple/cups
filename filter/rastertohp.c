/*
 * "$Id: rastertohp.c 7834 2008-08-04 21:02:09Z mike $"
 *
 *   Hewlett-Packard Page Control Language filter for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
 *   Copyright 1993-2007 by Easy Software Products.
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
#include <cups/i18n.h>
#include <cups/raster.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>


/*
 * Globals...
 */

unsigned char	*Planes[4],		/* Output buffers */
		*CompBuffer,		/* Compression buffer */
		*BitBuffer;		/* Buffer for output bits */
int		NumPlanes,		/* Number of color planes */
		ColorBits,		/* Number of bits per color */
		Feed,			/* Number of lines to skip */
		Duplex,			/* Current duplex mode */
		Page,			/* Current page number */
		Canceled;		/* Has the current job been canceled? */


/*
 * Prototypes...
 */

void	Setup(void);
void	StartPage(ppd_file_t *ppd, cups_page_header2_t *header);
void	EndPage(void);
void	Shutdown(void);

void	CancelJob(int sig);
void	CompressData(unsigned char *line, int length, int plane, int type);
void	OutputLine(cups_page_header2_t *header);


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
StartPage(ppd_file_t         *ppd,	/* I - PPD file */
          cups_page_header2_t *header)	/* I - Page header */
{
  int	plane;				/* Looping var */


 /*
  * Show page device dictionary...
  */

  fprintf(stderr, "DEBUG: StartPage...\n");
  fprintf(stderr, "DEBUG: MediaClass = \"%s\"\n", header->MediaClass);
  fprintf(stderr, "DEBUG: MediaColor = \"%s\"\n", header->MediaColor);
  fprintf(stderr, "DEBUG: MediaType = \"%s\"\n", header->MediaType);
  fprintf(stderr, "DEBUG: OutputType = \"%s\"\n", header->OutputType);

  fprintf(stderr, "DEBUG: AdvanceDistance = %d\n", header->AdvanceDistance);
  fprintf(stderr, "DEBUG: AdvanceMedia = %d\n", header->AdvanceMedia);
  fprintf(stderr, "DEBUG: Collate = %d\n", header->Collate);
  fprintf(stderr, "DEBUG: CutMedia = %d\n", header->CutMedia);
  fprintf(stderr, "DEBUG: Duplex = %d\n", header->Duplex);
  fprintf(stderr, "DEBUG: HWResolution = [ %d %d ]\n", header->HWResolution[0],
          header->HWResolution[1]);
  fprintf(stderr, "DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n",
          header->ImagingBoundingBox[0], header->ImagingBoundingBox[1],
          header->ImagingBoundingBox[2], header->ImagingBoundingBox[3]);
  fprintf(stderr, "DEBUG: InsertSheet = %d\n", header->InsertSheet);
  fprintf(stderr, "DEBUG: Jog = %d\n", header->Jog);
  fprintf(stderr, "DEBUG: LeadingEdge = %d\n", header->LeadingEdge);
  fprintf(stderr, "DEBUG: Margins = [ %d %d ]\n", header->Margins[0],
          header->Margins[1]);
  fprintf(stderr, "DEBUG: ManualFeed = %d\n", header->ManualFeed);
  fprintf(stderr, "DEBUG: MediaPosition = %d\n", header->MediaPosition);
  fprintf(stderr, "DEBUG: MediaWeight = %d\n", header->MediaWeight);
  fprintf(stderr, "DEBUG: MirrorPrint = %d\n", header->MirrorPrint);
  fprintf(stderr, "DEBUG: NegativePrint = %d\n", header->NegativePrint);
  fprintf(stderr, "DEBUG: NumCopies = %d\n", header->NumCopies);
  fprintf(stderr, "DEBUG: Orientation = %d\n", header->Orientation);
  fprintf(stderr, "DEBUG: OutputFaceUp = %d\n", header->OutputFaceUp);
  fprintf(stderr, "DEBUG: PageSize = [ %d %d ]\n", header->PageSize[0],
          header->PageSize[1]);
  fprintf(stderr, "DEBUG: Separations = %d\n", header->Separations);
  fprintf(stderr, "DEBUG: TraySwitch = %d\n", header->TraySwitch);
  fprintf(stderr, "DEBUG: Tumble = %d\n", header->Tumble);
  fprintf(stderr, "DEBUG: cupsWidth = %d\n", header->cupsWidth);
  fprintf(stderr, "DEBUG: cupsHeight = %d\n", header->cupsHeight);
  fprintf(stderr, "DEBUG: cupsMediaType = %d\n", header->cupsMediaType);
  fprintf(stderr, "DEBUG: cupsBitsPerColor = %d\n", header->cupsBitsPerColor);
  fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d\n", header->cupsBitsPerPixel);
  fprintf(stderr, "DEBUG: cupsBytesPerLine = %d\n", header->cupsBytesPerLine);
  fprintf(stderr, "DEBUG: cupsColorOrder = %d\n", header->cupsColorOrder);
  fprintf(stderr, "DEBUG: cupsColorSpace = %d\n", header->cupsColorSpace);
  fprintf(stderr, "DEBUG: cupsCompression = %d\n", header->cupsCompression);

 /*
  * Setup printer/job attributes...
  */

  Duplex    = header->Duplex;
  ColorBits = header->cupsBitsPerColor;

  if ((!Duplex || (Page & 1)) && header->MediaPosition)
    printf("\033&l%dH",				/* Set media position */
           header->MediaPosition);

  if (Duplex && ppd && ppd->model_number == 2)
  {
   /*
    * Handle duplexing on new DeskJet printers...
    */

    printf("\033&l-2H");			/* Load media */

    if (Page & 1)
      printf("\033&l2S");			/* Set duplex mode */
  }

  if (!Duplex || (Page & 1) || (ppd && ppd->model_number == 2))
  {
   /*
    * Set the media size...
    */

    printf("\033&l6D\033&k12H");		/* Set 6 LPI, 10 CPI */
    printf("\033&l0O");				/* Set portrait orientation */

    switch (header->PageSize[1])
    {
      case 540 : /* Monarch Envelope */
          printf("\033&l80A");			/* Set page size */
	  break;

      case 595 : /* A5 */
          printf("\033&l25A");			/* Set page size */
	  break;

      case 624 : /* DL Envelope */
          printf("\033&l90A");			/* Set page size */
	  break;

      case 649 : /* C5 Envelope */
          printf("\033&l91A");			/* Set page size */
	  break;

      case 684 : /* COM-10 Envelope */
          printf("\033&l81A");			/* Set page size */
	  break;

      case 709 : /* B5 Envelope */
          printf("\033&l100A");			/* Set page size */
	  break;

      case 756 : /* Executive */
          printf("\033&l1A");			/* Set page size */
	  break;

      case 792 : /* Letter */
          printf("\033&l2A");			/* Set page size */
	  break;

      case 842 : /* A4 */
          printf("\033&l26A");			/* Set page size */
	  break;

      case 1008 : /* Legal */
          printf("\033&l3A");			/* Set page size */
	  break;

      case 1191 : /* A3 */
          printf("\033&l27A");			/* Set page size */
	  break;

      case 1224 : /* Tabloid */
          printf("\033&l6A");			/* Set page size */
	  break;
    }

    printf("\033&l%dP",				/* Set page length */
           header->PageSize[1] / 12);
    printf("\033&l0E");				/* Set top margin to 0 */
  }

  if (!Duplex || (Page & 1))
  {
   /*
    * Set other job options...
    */

    printf("\033&l%dX", header->NumCopies);	/* Set number copies */

    if (header->cupsMediaType &&
        (!ppd || ppd->model_number != 2 || header->HWResolution[0] == 600))
      printf("\033&l%dM",			/* Set media type */
             header->cupsMediaType);

    if (!ppd || ppd->model_number != 2)
    {
      int mode = Duplex ? 1 + header->Tumble != 0 : 0;

      printf("\033&l%dS", mode);		/* Set duplex mode */
      printf("\033&l0L");			/* Turn off perforation skip */
    }
  }
  else if (!ppd || ppd->model_number != 2)
    printf("\033&a2G");				/* Set back side */

 /*
  * Set graphics mode...
  */

  if (ppd && ppd->model_number == 2)
  {
   /*
    * Figure out the number of color planes...
    */

    if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
      NumPlanes = 4;
    else
      NumPlanes = 1;

   /*
    * Set the resolution and top-of-form...
    */

    printf("\033&u%dD", header->HWResolution[0]);
						/* Resolution */
    printf("\033&l0e0L");			/* Reset top and don't skip */
    printf("\033*p0Y\033*p0X");			/* Set top of form */

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

    printf("\033&l0H");				/* Set media position */
  }
  else
  {
    printf("\033*t%dR", header->HWResolution[0]);
						/* Set resolution */

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

   /*
    * Set size and position of graphics...
    */

    printf("\033*r%dS", header->cupsWidth);	/* Set width */
    printf("\033*r%dT", header->cupsHeight);	/* Set height */

    printf("\033&a0H");				/* Set horizontal position */

    if (ppd)
      printf("\033&a%.0fV", 			/* Set vertical position */
             10.0 * (ppd->sizes[0].length - ppd->sizes[0].top));
    else
      printf("\033&a0V");			/* Set top-of-page */
  }

  printf("\033*r1A");				/* Start graphics */

  if (header->cupsCompression)
    printf("\033*b%dM",				/* Set compression */
           header->cupsCompression);

  Feed = 0;					/* No blank lines yet */

 /*
  * Allocate memory for a line of graphics...
  */

  if ((Planes[0] = malloc(header->cupsBytesPerLine)) == NULL)
  {
    fputs("ERROR: Unable to allocate memory!\n", stderr);
    exit(1);
  }

  for (plane = 1; plane < NumPlanes; plane ++)
    Planes[plane] = Planes[0] + plane * header->cupsBytesPerLine / NumPlanes;

  if (ColorBits > 1)
    BitBuffer = malloc(ColorBits * ((header->cupsWidth + 7) / 8));
  else
    BitBuffer = NULL;

  if (header->cupsCompression)
    CompBuffer = malloc(header->cupsBytesPerLine * 2);
  else
    CompBuffer = NULL;
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */

void
EndPage(void)
{
 /*
  * Eject the current page...
  */

  if (NumPlanes > 1)
  {
     printf("\033*rC");			/* End color GFX */

     if (!(Duplex && (Page & 1)))
       printf("\033&l0H");		/* Eject current page */
  }
  else
  {
     printf("\033*r0B");		/* End GFX */

     if (!(Duplex && (Page & 1)))
       printf("\014");			/* Eject current page */
  }

  fflush(stdout);

 /*
  * Free memory...
  */

  free(Planes[0]);

  if (BitBuffer)
    free(BitBuffer);

  if (CompBuffer)
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
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
  (void)sig;

  Canceled = 1;
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

  printf("\033*b%d%c", (int)(line_end - line_ptr), plane);
  fwrite(line_ptr, line_end - line_ptr, 1, stdout);
}


/*
 * 'OutputLine()' - Output a line of graphics.
 */

void
OutputLine(cups_page_header2_t *header)	/* I - Page header */
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

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			fd;		/* File descriptor */
  cups_raster_t		*ras;		/* Raster stream for printing */
  cups_page_header2_t	header;		/* Page header from file */
  int			y;		/* Current line */
  ppd_file_t		*ppd;		/* PPD file */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


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

    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]\n"),
                    "rastertohp");
    return (1);
  }

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) == -1)
    {
      _cupsLangPrintf(stderr, _("ERROR: Unable to open raster file - %s\n"),
                      strerror(errno));
      sleep(1);
      return (1);
    }
  }
  else
    fd = 0;

  ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

 /*
  * Register a signal handler to eject the current page if the
  * job is cancelled.
  */

  Canceled = 0;

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
  * Initialize the print device...
  */

  ppd = ppdOpenFile(getenv("PPD"));

  Setup();

 /*
  * Process pages as needed...
  */

  Page = 0;

  while (cupsRasterReadHeader2(ras, &header))
  {
   /*
    * Write a status message with the page number and number of copies.
    */

    if (Canceled)
      break;

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

      if (Canceled)
	break;

      if ((y & 127) == 0)
        _cupsLangPrintf(stderr, _("INFO: Printing page %d, %d%% complete...\n"),
                        Page, 100 * y / header.cupsHeight);

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

    EndPage();

    if (Canceled)
      break;
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
  if (fd != 0)
    close(fd);

 /*
  * If no pages were printed, send an error message...
  */

  if (Page == 0)
  {
    _cupsLangPuts(stderr, _("ERROR: No pages found!\n"));
    return (1);
  }
  else
  {
    _cupsLangPuts(stderr, _("INFO: Ready to print.\n"));
    return (0);
  }
}


/*
 * End of "$Id: rastertohp.c 7834 2008-08-04 21:02:09Z mike $".
 */
