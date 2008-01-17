/*
 * "$Id: rastertoepson.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   EPSON ESC/P and ESC/P2 filter for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
#include <cups/i18n.h>
#include "raster.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


/*
 * Model numbers...
 */

#define EPSON_9PIN	0
#define EPSON_24PIN	1
#define EPSON_COLOR	2
#define EPSON_PHOTO	3
#define EPSON_ICOLOR	4
#define EPSON_IPHOTO	5


/*
 * Macros...
 */

#define pwrite(s,n) fwrite((s), 1, (n), stdout)


/*
 * Globals...
 */

unsigned char	*Planes[6],		/* Output buffers */
		*CompBuffer,		/* Compression buffer */
		*LineBuffers[2];	/* Line bitmap buffers */
int		Model,			/* Model number */
		NumPlanes,		/* Number of color planes */
		Feed,			/* Number of lines to skip */
		EjectPage;		/* Eject the page when done? */
int		DotBit,			/* Bit in buffers */
		DotBytes,		/* # bytes in a dot column */
		DotColumns,		/* # columns in 1/60 inch */
		LineCount,		/* # of lines processed */
		EvenOffset,		/* Offset into 'even' buffers */
		OddOffset,		/* Offset into 'odd' buffers */
		Shingling;		/* Shingle output? */


/*
 * Prototypes...
 */

void	Setup(void);
void	StartPage(const ppd_file_t *ppd, const cups_page_header_t *header);
void	EndPage(const cups_page_header_t *header);
void	Shutdown(void);

void	CancelJob(int sig);
void	CompressData(const unsigned char *line, int length, int plane,
	             int type, int xstep, int ystep);
void	OutputLine(const cups_page_header_t *header);
void	OutputRows(const cups_page_header_t *header, int row);


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(void)
{
  const char	*device_uri;	/* The device for the printer... */


 /*
  * EPSON USB printers need an additional command issued at the
  * beginning of each job to exit from "packet" mode...
  */

  if ((device_uri = getenv("DEVICE_URI")) != NULL &&
      strncmp(device_uri, "usb:", 4) == 0 && Model >= EPSON_ICOLOR)
    pwrite("\000\000\000\033\001@EJL 1284.4\n@EJL     \n\033@", 29);
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(const ppd_file_t         *ppd,	/* I - PPD file */
          const cups_page_header_t *header)	/* I - Page header */
{
  int	n, t;					/* Numbers */
  int	plane;					/* Looping var */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;			/* Actions for POSIX signals */
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
  * Send a reset sequence.
  */

  if (ppd && ppd->nickname && strstr(ppd->nickname, "OKIDATA") != NULL)
    printf("\033{A");	/* Set EPSON emulation mode */

  printf("\033@");

 /*
  * See which type of printer we are using...
  */

  EjectPage = header->Margins[0] || header->Margins[1];
    
  switch (Model)
  {
    case EPSON_9PIN :
    case EPSON_24PIN :
        printf("\033P\022");		/* Set 10 CPI */

	if (header->HWResolution[0] == 360 || header->HWResolution[0] == 240)
	{
	  printf("\033x1");		/* LQ printing */
	  printf("\033U1");		/* Unidirectional */
	}
	else
	{
	  printf("\033x0");		/* Draft printing */
	  printf("\033U0");		/* Bidirectional */
	}

	printf("\033l%c\033Q%c", 0,	/* Side margins */
                      (int)(10.0 * header->PageSize[0] / 72.0 + 0.5));
	printf("\033C%c%c", 0,		/* Page length */
                      (int)(header->PageSize[1] / 72.0 + 0.5));
	printf("\033N%c", 0);		/* Bottom margin */
        printf("\033O");		/* No perforation skip */

       /*
	* Setup various buffer limits...
	*/

        DotBytes   = header->cupsRowCount / 8;
	DotColumns = header->HWResolution[0] / 60;
        Shingling  = 0;

        if (Model == EPSON_9PIN)
	  printf("\033\063\030");	/* Set line feed */
	else
	  switch (header->HWResolution[0])
	  {
	    case 60:
	    case 120 :
	    case 240 :
        	printf("\033\063\030");	/* Set line feed */
		break;

	    case 180 :
	    case 360 :
        	Shingling = 1;

        	if (header->HWResolution[1] == 180)
        	  printf("\033\063\010");/* Set line feed */
		else
        	  printf("\033+\010");	/* Set line feed */
        	break;
	  }
        break;

    default :
       /*
	* Set graphics mode...
	*/

	pwrite("\033(G\001\000\001", 6);	/* Graphics mode */

       /*
	* Set the media size...
	*/

        if (Model < EPSON_ICOLOR)
	{
	  pwrite("\033(U\001\000", 5);		/* Resolution/units */
	  putchar(3600 / header->HWResolution[1]);
        }
	else
	{
	  pwrite("\033(U\005\000", 5);
	  putchar(1440 / header->HWResolution[1]);
	  putchar(1440 / header->HWResolution[1]);
	  putchar(1440 / header->HWResolution[0]);
	  putchar(0xa0);	/* n/1440ths... */
	  putchar(0x05);
	}

	n = header->PageSize[1] * header->HWResolution[1] / 72.0;

	pwrite("\033(C\002\000", 5);		/* Page length */
	putchar(n);
	putchar(n >> 8);

        if (ppd)
	  t = (ppd->sizes[1].length - ppd->sizes[1].top) *
	      header->HWResolution[1] / 72.0;
        else
	  t = 0;

	pwrite("\033(c\004\000", 5);		/* Top & bottom margins */
	putchar(t);
	putchar(t >> 8);
	putchar(n);
	putchar(n >> 8);

	if (header->HWResolution[1] == 720)
	{
	  pwrite("\033(i\001\000\001", 6);	/* Microweave */
	  pwrite("\033(e\002\000\000\001", 7);	/* Small dots */
	}

	pwrite("\033(V\002\000\000\000", 7);	/* Set absolute position 0 */

        DotBytes   = 0;
	DotColumns = 0;
        Shingling  = 0;
        break;
  }

 /*
  * Set other stuff...
  */

  if (header->cupsColorSpace == CUPS_CSPACE_CMY)
    NumPlanes = 3;
  else if (header->cupsColorSpace == CUPS_CSPACE_KCMY)
    NumPlanes = 4;
  else if (header->cupsColorSpace == CUPS_CSPACE_KCMYcm)
    NumPlanes = 6;
  else
    NumPlanes = 1;

  Feed = 0;				/* No blank lines yet */

 /*
  * Allocate memory for a line/row of graphics...
  */

  if ((Planes[0] = malloc(header->cupsBytesPerLine)) == NULL)
  {
    fputs("ERROR: Unable to allocate memory!\n", stderr);
    exit(1);
  }

  for (plane = 1; plane < NumPlanes; plane ++)
    Planes[plane] = Planes[0] + plane * header->cupsBytesPerLine / NumPlanes;

  if (header->cupsCompression || DotBytes)
  {
    if ((CompBuffer = calloc(2, header->cupsWidth)) == NULL)
    {
      fputs("ERROR: Unable to allocate memory!\n", stderr);
      exit(1);
    }
  }
  else
    CompBuffer = NULL;

  if (DotBytes)
  {
    if ((LineBuffers[0] = calloc(DotBytes,
                                 header->cupsWidth * (Shingling + 1))) == NULL)
    {
      fputs("ERROR: Unable to allocate memory!\n", stderr);
      exit(1);
    }

    LineBuffers[1] = LineBuffers[0] + DotBytes * header->cupsWidth;
    DotBit         = 128;
    LineCount      = 0;
    EvenOffset     = 0;
    OddOffset      = 0;
  }
}


/*
 * 'EndPage()' - Finish a page of graphics.
 */

void
EndPage(const cups_page_header_t *header)	/* I - Page header */
{
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;			/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  if (DotBytes && header)
  {
   /*
    * Flush remaining graphics as needed...
    */

    if (!Shingling)
    {
      if (DotBit < 128 || EvenOffset)
        OutputRows(header, 0);
    }
    else if (OddOffset > EvenOffset)
    {
      OutputRows(header, 1);
      OutputRows(header, 0);
    }
    else
    {
      OutputRows(header, 0);
      OutputRows(header, 1);
    }
  }

 /*
  * Eject the current page...
  */

  if (EjectPage)
    putchar(12);		/* Form feed */
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

  free(Planes[0]);

  if (CompBuffer)
    free(CompBuffer);

  if (DotBytes)
    free(LineBuffers[0]);
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

  if (DotBytes)
    i = DotBytes * 360 * 8;
  else
    i = 720;

  for (; i > 0; i --)
    putchar(0);

 /*
  * End the current page and exit...
  */

  EndPage(NULL);
  Shutdown();

  exit(0);
}


/*
 * 'CompressData()' - Compress a line of graphics.
 */

void
CompressData(const unsigned char *line,	/* I - Data to compress */
             int                 length,/* I - Number of bytes */
	     int                 plane,	/* I - Color plane */
	     int                 type,	/* I - Type of compression */
	     int                 xstep,	/* I - X resolution */
	     int                 ystep)	/* I - Y resolution */
{
  const unsigned char	*line_ptr,	/* Current byte pointer */
        		*line_end,	/* End-of-line byte pointer */
        		*start;		/* Start of compression sequence */
  unsigned char      	*comp_ptr,	/* Pointer into compression buffer */
			temp;		/* Current byte */
  int   	        count;		/* Count of bytes for output */
  static int		ctable[6] = { 0, 2, 1, 4, 18, 17 };
					/* KCMYcm color values */


 /*
  * Setup pointers...
  */

  line_ptr = line;
  line_end = line + length;

 /*
  * Do depletion for 720 DPI printing...
  */

  if (ystep == 5)
  {
    for (comp_ptr = (unsigned char *)line; comp_ptr < line_end;)
    {
     /*
      * Grab the current byte...
      */

      temp = *comp_ptr;

     /*
      * Check adjacent bits...
      */

      if ((temp & 0xc0) == 0xc0)
        temp &= 0xbf;
      if ((temp & 0x60) == 0x60)
        temp &= 0xdf;
      if ((temp & 0x30) == 0x30)
        temp &= 0xef;
      if ((temp & 0x18) == 0x18)
        temp &= 0xf7;
      if ((temp & 0x0c) == 0x0c)
        temp &= 0xfb;
      if ((temp & 0x06) == 0x06)
        temp &= 0xfd;
      if ((temp & 0x03) == 0x03)
        temp &= 0xfe;

      *comp_ptr++ = temp;

     /*
      * Check the last bit in the current byte and the first bit in the
      * next byte...
      */

      if ((temp & 0x01) && comp_ptr < line_end && *comp_ptr & 0x80)
        *comp_ptr &= 0x7f;
    }
  }

  switch (type)
  {
    case 0 :
       /*
	* Do no compression...
	*/
	break;

    case 1 :
       /*
        * Do TIFF pack-bits encoding...
        */

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

  putchar(0x0d);			/* Move print head to left margin */

  if (Model < EPSON_ICOLOR)
  {
   /*
    * Do graphics the "old" way...
    */

    if (NumPlanes > 1)
    {
     /*
      * Set the color...
      */

      if (plane > 3)
	printf("\033(r%c%c%c%c", 2, 0, 1, ctable[plane] & 15);
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

    length *= 8;
    printf("\033.");			/* Raster graphics */
    putchar(type);
    putchar(ystep);
    putchar(xstep);
    putchar(1);
    putchar(length);
    putchar(length >> 8);
  }
  else
  {
   /*
    * Do graphics the "new" way...
    */

    printf("\033i");
    putchar(ctable[plane]);
    putchar(type);
    putchar(1);
    putchar(length & 255);
    putchar(length >> 8);
    putchar(1);
    putchar(0);
  }

  pwrite(line_ptr, line_end - line_ptr);
  fflush(stdout);
}


/*
 * 'OutputLine()' - Output a line of graphics.
 */

void
OutputLine(const cups_page_header_t *header)	/* I - Page header */
{
  if (header->cupsRowCount)
  {
    int			width;
    unsigned char	*tempptr,
			*evenptr,
			*oddptr;
    register int	x;
    unsigned char	bit;
    const unsigned char	*pixel;
    unsigned char 	*temp;


   /*
    * Collect bitmap data in the line buffers and write after each buffer.
    */

    for (x = header->cupsWidth, bit = 128, pixel = Planes[0],
             temp = CompBuffer;
	 x > 0;
	 x --, temp ++)
    {
      if (*pixel & bit)
        *temp |= DotBit;

      if (bit > 1)
	bit >>= 1;
      else
      {
	bit = 128;
	pixel ++;
      }
    }

    if (DotBit > 1)
      DotBit >>= 1;
    else
    {
     /*
      * Copy the holding buffer to the output buffer, shingling as necessary...
      */

      if (Shingling && LineCount != 0)
      {
       /*
        * Shingle the output...
        */

        if (LineCount & 1)
        {
          evenptr = LineBuffers[1] + OddOffset;
          oddptr  = LineBuffers[0] + EvenOffset + DotBytes;
        }
        else
        {
          evenptr = LineBuffers[0] + EvenOffset;
          oddptr  = LineBuffers[1] + OddOffset + DotBytes;
        }

        for (width = header->cupsWidth, tempptr = CompBuffer;
             width > 0;
             width -= 2, tempptr += 2, oddptr += DotBytes * 2,
	         evenptr += DotBytes * 2)
        {
          evenptr[0] = tempptr[0];
          oddptr[0]  = tempptr[1];
        }
      }
      else
      {
       /*
        * Don't shingle the output...
        */

        for (width = header->cupsWidth, tempptr = CompBuffer,
                 evenptr = LineBuffers[0] + EvenOffset;
             width > 0;
             width --, tempptr ++, evenptr += DotBytes)
          *evenptr = tempptr[0];
      }

      if (Shingling && LineCount != 0)
      {
	EvenOffset ++;
	OddOffset ++;

	if (EvenOffset == DotBytes)
	{
	  EvenOffset = 0;
	  OutputRows(header, 0);
	}

	if (OddOffset == DotBytes)
	{
          OddOffset = 0;
	  OutputRows(header, 1);
	}
      }
      else
      {
	EvenOffset ++;

	if (EvenOffset == DotBytes)
	{
          EvenOffset = 0;
	  OutputRows(header, 0);
	}
      }

      DotBit = 128;
      LineCount ++;

      memset(CompBuffer, 0, header->cupsWidth);
    }
  }
  else
  {
    int	plane;		/* Current plane */
    int	bytes;		/* Bytes per plane */
    int	xstep, ystep;	/* X & Y resolutions */


   /*
    * Write a single line of bitmap data as needed...
    */

    xstep = 3600 / header->HWResolution[0];
    ystep = 3600 / header->HWResolution[1];
    bytes = header->cupsBytesPerLine / NumPlanes;

    for (plane = 0; plane < NumPlanes; plane ++)
    {
     /*
      * Skip blank data...
      */

      if (!Planes[plane][0] &&
          memcmp(Planes[plane], Planes[plane] + 1, bytes - 1) == 0)
	continue;

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

      CompressData(Planes[plane], bytes, plane, header->cupsCompression, xstep,
                   ystep);
    }

    Feed ++;
  }
}


/*
 * 'OutputRows()' - Output 8, 24, or 48 rows.
 */

void
OutputRows(const cups_page_header_t *header,	/* I - Page image header */
           int                      row)	/* I - Row number (0 or 1) */
{
  unsigned	i, n;				/* Looping vars */
  int		dot_count,			/* Number of bytes to print */
                dot_min;			/* Minimum number of bytes */
  unsigned char *dot_ptr,			/* Pointer to print data */
		*ptr;				/* Current data */


  dot_min = DotBytes * DotColumns;

  if (LineBuffers[row][0] != 0 ||
      memcmp(LineBuffers[row], LineBuffers[row] + 1,
             header->cupsWidth * DotBytes - 1))
  {
   /*
    * Skip leading space...
    */

    i         = 0;
    dot_count = header->cupsWidth * DotBytes;
    dot_ptr   = LineBuffers[row];

    while (dot_count >= dot_min && dot_ptr[0] == 0 &&
           memcmp(dot_ptr, dot_ptr + 1, dot_min - 1) == 0)
    {
      i         ++;
      dot_ptr   += dot_min;
      dot_count -= dot_min;
    }

   /*
    * Skip trailing space...
    */

    while (dot_count >= dot_min && dot_ptr[dot_count - dot_min] == 0 &&
           memcmp(dot_ptr + dot_count - dot_min,
	          dot_ptr + dot_count - dot_min + 1, dot_min - 1) == 0)
      dot_count -= dot_min;

   /*
    * Position print head for printing...
    */

    if (i == 0)
      putchar('\r');
    else
    {
      putchar(0x1b);
      putchar('$');
      putchar(i & 255);
      putchar(i >> 8);
    }

   /*
    * Start bitmap graphics for this line...
    */

    printf("\033*");			/* Select bit image */
    switch (header->HWResolution[0])
    {
      case 60 : /* 60x60/72 DPI gfx */
          putchar(0);
          break;
      case 120 : /* 120x60/72 DPI gfx */
          putchar(1);
          break;
      case 180 : /* 180 DPI gfx */
          putchar(39);
          break;
      case 240 : /* 240x72 DPI gfx */
          putchar(3);
          break;
      case 360 : /* 360x180/360 DPI gfx */
	  if (header->HWResolution[1] == 180)
	  {
            if (Shingling && LineCount != 0)
              putchar(40);		/* 360x180 fast */
            else
              putchar(41);		/* 360x180 slow */
	  }
	  else
          {
	    if (Shingling && LineCount != 0)
              putchar(72);		/* 360x360 fast */
            else
              putchar(73);		/* 360x360 slow */
          }
          break;
    }

    n = (unsigned)dot_count / DotBytes;
    putchar(n & 255);
    putchar(n / 256);

   /*
    * Write the graphics data...
    */

    if (header->HWResolution[0] == 120 ||
        header->HWResolution[0] == 240)
    {
     /*
      * Need to interleave the dots to avoid hosing the print head...
      */

      for (n = dot_count / 2, ptr = dot_ptr; n > 0; n --, ptr += 2)
      {
        putchar(*ptr);
	putchar(0);
      }

     /*
      * Move the head back and print the odd bytes...
      */

      if (i == 0)
	putchar('\r');
      else
      {
	putchar(0x1b);
	putchar('$');
	putchar(i & 255);
	putchar(i >> 8);
      }

      if (header->HWResolution[0] == 120)
      	printf("\033*\001");		/* Select bit image */
      else
      	printf("\033*\003");		/* Select bit image */

      n = (unsigned)dot_count / DotBytes;
      putchar(n & 255);
      putchar(n / 256);

      for (n = dot_count / 2, ptr = dot_ptr + 1; n > 0; n --, ptr += 2)
      {
	putchar(0);
        putchar(*ptr);
      }
    }
    else
      pwrite(dot_ptr, dot_count);
  }

 /*
  * Feed the paper...
  */

  putchar('\n');

  if (Shingling && row == 1)
  {
    if (header->HWResolution[1] == 360)
      printf("\n\n\n\n");
    else
      printf("\n");
  }

  fflush(stdout);

 /*
  * Clear the buffer...
  */

  memset(LineBuffers[row], 0, header->cupsWidth * DotBytes);
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int			fd;	/* File descriptor */
  cups_raster_t		*ras;	/* Raster stream for printing */
  cups_page_header_t	header;	/* Page header from file */
  ppd_file_t		*ppd;	/* PPD file */
  int			page;	/* Current page */
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

    fprintf(stderr, _("Usage: %s job-id user title copies options [file]\n"),
            argv[0]);
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
  if (ppd)
    Model = ppd->model_number;

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
        fprintf(stderr, _("INFO: Printing page %d, %d%% complete...\n"), page,
	        100 * y / header.cupsHeight);

     /*
      * Read a line of graphics...
      */

      if (cupsRasterReadPixels(ras, Planes[0], header.cupsBytesPerLine) < 1)
        break;

     /*
      * Write it to the printer...
      */

      OutputLine(&header);
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
    fputs(_("ERROR: No pages found!\n"), stderr);
  else
    fputs(_("INFO: Ready to print.\n"), stderr);

  return (page == 0);
}


/*
 * End of "$Id: rastertoepson.c 6649 2007-07-11 21:46:42Z mike $".
 */
