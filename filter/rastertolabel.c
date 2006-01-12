/*
 * "$Id$"
 *
 *   Label printer filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2001-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
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
 *   CancelJob()    - Cancel the current job...
 *   OutputLine()   - Output a line of graphics.
 *   ZPLCompress()  - Output a run-length compression sequence.
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
 * This driver filter currently supports Dymo and Zebra label printers.
 *
 * The Dymo portion of the driver has been tested with the 300, 330,
 * and 330 Turbo label printers; it may also work with older models.
 * The Dymo printers support printing at 136, 203, and 300 DPI.
 *
 * The Zebra portion of the driver has been tested with the LP-2844Z label
 * printer; it may also work with other models.  The driver supports EPL
 * line mode, EPL page mode, ZPL, and CPCL as defined in Zebra's on-line
 * developer documentation.
 */

/*
 * Model number constants...
 */

#define DYMO_3x0	0		/* Dymo Labelwriter 300/330/330 Turbo */

#define ZEBRA_EPL_LINE	0x10		/* Zebra EPL line mode printers */
#define ZEBRA_EPL_PAGE	0x11		/* Zebra EPL page mode printers */
#define ZEBRA_ZPL	0x12		/* Zebra ZPL-based printers */
#define ZEBRA_CPCL	0x13		/* Zebra CPCL-based printers */


/*
 * Globals...
 */

unsigned char	*Buffer;		/* Output buffer */
char		*CompBuffer;		/* Compression buffer */
unsigned char	*LastBuffer;		/* Last buffer */
int		LastSet;		/* Number of repeat characters */
int		ModelNumber,		/* cupsModelNumber attribute */
		Page,			/* Current page */
		Feed,			/* Number of lines to skip */
		Canceled;		/* Non-zero if job is canceled */


/*
 * Prototypes...
 */

void	Setup(ppd_file_t *ppd);
void	StartPage(ppd_file_t *ppd, cups_page_header_t *header);
void	EndPage(ppd_file_t *ppd, cups_page_header_t *header);
void	CancelJob(int sig);
void	OutputLine(ppd_file_t *ppd, cups_page_header_t *header, int y);
void	ZPLCompress(char repeat_char, int repeat_count);


/*
 * 'Setup()' - Prepare the printer for printing.
 */

void
Setup(ppd_file_t *ppd)			/* I - PPD file */
{
  int		i;			/* Looping var */


 /*
  * Get the model number from the PPD file...
  */

  if (ppd)
    ModelNumber = ppd->model_number;

 /*
  * Initialize based on the model number...
  */

  switch (ModelNumber)
  {
    case DYMO_3x0 :
       /*
	* Clear any remaining data...
	*/

	for (i = 0; i < 100; i ++)
	  putchar(0x1b);

       /*
	* Reset the printer...
	*/

	fputs("\033@", stdout);
	break;

    case ZEBRA_EPL_LINE :
	break;

    case ZEBRA_EPL_PAGE :
	break;

    case ZEBRA_ZPL :
        break;

    case ZEBRA_CPCL :
        break;
  }
}


/*
 * 'StartPage()' - Start a page of graphics.
 */

void
StartPage(ppd_file_t *ppd,		/* I - PPD file */
          cups_page_header_t *header)	/* I - Page header */
{
  ppd_choice_t	*choice;		/* Marked choice */
  int		length;			/* Actual label length */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Register a signal handler to eject the current page if the
  * job is canceled.
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

  switch (ModelNumber)
  {
    case DYMO_3x0 :
       /*
	* Setup printer/job attributes...
	*/

	length = header->PageSize[1] * header->HWResolution[1] / 72;

	printf("\033L%c%c", length >> 8, length);
	printf("\033D%c", header->cupsBytesPerLine);

	printf("\033%c", header->cupsCompression + 'c'); /* Darkness */
	break;

    case ZEBRA_EPL_LINE :
       /*
        * Set print rate...
	*/

	if ((choice = ppdFindMarkedChoice(ppd, "zePrintRate")) != NULL &&
	    strcmp(choice->choice, "Default"))
	  printf("\033S%.0f", atof(choice->choice) * 2.0 - 2.0);

       /*
        * Set darkness...
	*/

        if (header->cupsCompression > 0 && header->cupsCompression <= 100)
	  printf("\033D%d", 7 * header->cupsCompression / 100);

       /*
        * Set left margin to 0...
	*/

	fputs("\033M01", stdout);

       /*
        * Start buffered output...
	*/

        fputs("\033B", stdout);
        break;

    case ZEBRA_EPL_PAGE :
       /*
        * Start a new label...
	*/

        puts("");
	puts("N");

       /*
        * Set hardware options...
	*/

	if (!strcmp(header->MediaType, "Direct"))
	  puts("OD");

       /*
        * Set print rate...
	*/

	if ((choice = ppdFindMarkedChoice(ppd, "zePrintRate")) != NULL &&
	    strcmp(choice->choice, "Default"))
	{
	  float val = atof(choice->choice);

	  if (val >= 3.0)
	    printf("S%.0f\n", val);
	  else
	    printf("S%.0f\n", val * 2.0 - 2.0);
        }

       /*
        * Set darkness...
	*/

        if (header->cupsCompression > 0 && header->cupsCompression <= 100)
	  printf("D%d\n", 15 * header->cupsCompression / 100);

       /*
        * Set label size...
	*/

        printf("q%d\n", header->cupsWidth);
        break;

    case ZEBRA_ZPL :
       /*
        * Set darkness...
	*/

        if (header->cupsCompression > 0 && header->cupsCompression <= 100)
	  printf("~SD%02d\n", 30 * header->cupsCompression / 100);

       /*
        * Start bitmap graphics...
	*/

        printf("~DGR:CUPS.GRF,%d,%d,\n",
	       header->cupsHeight * header->cupsBytesPerLine,
	       header->cupsBytesPerLine);

       /*
        * Allocate compression buffers...
	*/

	CompBuffer = malloc(2 * header->cupsBytesPerLine + 1);
	LastBuffer = malloc(header->cupsBytesPerLine);
	LastSet    = 0;
        break;

    case ZEBRA_CPCL :
       /*
        * Start label...
	*/

        printf("! 0 %u %u %u %u\r\n", header->HWResolution[0],
	       header->HWResolution[1], header->cupsHeight,
	       header->NumCopies);
        break;
  }

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
EndPage(ppd_file_t *ppd,		/* I - PPD file */
        cups_page_header_t *header)	/* I - Page header */
{
  int		val;			/* Option value */
  ppd_choice_t	*choice;		/* Marked choice */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  switch (ModelNumber)
  {
    case DYMO_3x0 :
       /*
	* Eject the current page...
	*/

	fputs("\033E", stdout);
	break;

    case ZEBRA_EPL_LINE :
       /*
        * End buffered output, eject the label...
	*/

        fputs("\033E\014", stdout);
	break;

    case ZEBRA_EPL_PAGE :
       /*
        * Print the label...
	*/

        puts("P1");
	break;

    case ZEBRA_ZPL :
        if (Canceled)
	{
	 /*
	  * Cancel bitmap download...
	  */

	  puts("~DN");
	  break;
	}

       /*
        * Start label...
	*/

        puts("^XA");

       /*
        * Set print rate...
	*/

	if ((choice = ppdFindMarkedChoice(ppd, "zePrintRate")) != NULL &&
	    strcmp(choice->choice, "Default"))
	{
	  val = atoi(choice->choice);
	  printf("^PR%d,%d,%d\n", val, val, val);
	}

       /*
        * Put label home in default position (0,0)...
        */

	printf("^LH0,0\n");

       /*
        * Set media tracking...
	*/

	if (ppdIsMarked(ppd, "zeMediaTracking", "Continuous"))
	{
         /*
	  * Add label length command for continuous...
	  */

	  printf("^LL%d\n", header->cupsHeight);
	  printf("^MNN\n");
	}
	else if (ppdIsMarked(ppd, "zeMediaTracking", "Web"))
          printf("^MNY\n");
	else if (ppdIsMarked(ppd, "zeMediaTracking", "Mark"))
	  printf("^MNM\n");

       /*
        * Set label top
	*/

	if (header->cupsRowStep != 200)
	  printf("^LT%u\n", header->cupsRowStep);

       /*
        * Set media type...
	*/

	if (!strcmp(header->MediaType, "Thermal"))
	  printf("^MTT\n");
	else if (!strcmp(header->MediaType, "Direct"))
	  printf("^MTD\n");

       /*
        * Set print mode...
	*/

	if ((choice = ppdFindMarkedChoice(ppd, "zePrintMode")) != NULL &&
	    strcmp(choice->choice, "Saved"))
	{
	  printf("^MM");

	  if (!strcmp(choice->choice, "Tear"))
	    printf("T,Y\n");
	  else if (!strcmp(choice->choice, "Peel"))
	    printf("P,Y\n");
	  else if (!strcmp(choice->choice, "Rewind"))
	    printf("R,Y\n");
	  else if (!strcmp(choice->choice, "Applicator"))
	    printf("A,Y\n");
	  else
	    printf("C,Y\n");
	}

       /*
        * Set tear-off adjust position...
	*/

	if (header->AdvanceDistance != 1000)
	{
	  if ((int)header->AdvanceDistance < 0)
	    printf("~TA%04d\n", (int)header->AdvanceDistance);
	  else
	    printf("~TA%03d\n", (int)header->AdvanceDistance);
	}

       /*
        * Allow for reprinting after an error...
	*/

	if (ppdIsMarked(ppd, "zeErrorReprint", "Always"))
	  printf("^JZY\n");
	else if (ppdIsMarked(ppd, "zeErrorReprint", "Never"))
	  printf("^JZN\n");

       /*
        * Print multiple copies
	*/

	if (header->NumCopies > 1)
	  printf("^PQ%d, 0, 0, N\n", header->NumCopies);

       /*
        * Display the label image...
	*/

	puts("^FO0,0^XGR:CUPS.GRF,1,1^FS");

       /*
        * End the label and eject...
	*/

        puts("^XZ");

       /*
        * Free compression buffers...
	*/

	free(CompBuffer);
	free(LastBuffer);
        break;

    case ZEBRA_CPCL :
       /*
        * Set tear-off adjust position...
	*/

	if (header->AdvanceDistance != 1000)
          printf("PRESENT-AT %d 1\r\n", (int)header->AdvanceDistance);

       /*
        * Allow for reprinting after an error...
	*/

	if (ppdIsMarked(ppd, "zeErrorReprint", "Always"))
	  puts("ON-OUT-OF-PAPER WAIT\r");
	else if (ppdIsMarked(ppd, "zeErrorReprint", "Never"))
	  puts("ON-OUT-OF-PAPER PURGE\r");

       /*
        * Cut label?
	*/

	if (header->CutMedia)
	  puts("CUT\r");

       /*
        * Set darkness...
	*/

	if (header->cupsCompression > 0)
	  printf("TONE %u\r\n", 2 * header->cupsCompression);

       /*
        * Set print rate...
	*/

	if ((choice = ppdFindMarkedChoice(ppd, "zePrintRate")) != NULL &&
	    strcmp(choice->choice, "Default"))
	{
	  val = atoi(choice->choice);
	  printf("SPEED %d\r\n", val);
	}

       /*
        * Print the label...
	*/

        puts("FORM\r");
	puts("PRINT\r");
	break;
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

  free(Buffer);
}


/*
 * 'CancelJob()' - Cancel the current job...
 */

void
CancelJob(int sig)			/* I - Signal */
{
 /*
  * Tell the main loop to stop...
  */

  (void)sig;

  Canceled = 1;
}


/*
 * 'OutputLine()' - Output a line of graphics...
 */

void
OutputLine(ppd_file_t         *ppd,	/* I - PPD file */
           cups_page_header_t *header,	/* I - Page header */
           int                y)	/* I - Line number */
{
  int		i;			/* Looping var */
  unsigned char	*ptr;			/* Pointer into buffer */
  char		*compptr;		/* Pointer into compression buffer */
  char		repeat_char;		/* Repeated character */
  int		repeat_count;		/* Number of repeated characters */
  static const char *hex = "0123456789ABCDEF";
					/* Hex digits */


  switch (ModelNumber)
  {
    case DYMO_3x0 :
       /*
	* See if the line is blank; if not, write it to the printer...
	*/

	if (Buffer[0] ||
            memcmp(Buffer, Buffer + 1, header->cupsBytesPerLine - 1))
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
	  fwrite(Buffer, header->cupsBytesPerLine, 1, stdout);
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
	break;

    case ZEBRA_EPL_LINE :
        printf("\033g%03d", header->cupsBytesPerLine);
	fwrite(Buffer, 1, header->cupsBytesPerLine, stdout);
	fflush(stdout);
        break;

    case ZEBRA_EPL_PAGE :
        if (Buffer[0] || memcmp(Buffer, Buffer + 1, header->cupsBytesPerLine))
	{
          printf("GW0,%d,%d,1\n", y, header->cupsBytesPerLine);
	  for (i = header->cupsBytesPerLine, ptr = Buffer; i > 0; i --, ptr ++)
	    putchar(~*ptr);
	  putchar('\n');
	  fflush(stdout);
	}
        break;

    case ZEBRA_ZPL :
       /*
	* Determine if this row is the same as the previous line.
        * If so, output a ':' and return...
        */

        if (LastSet)
	{
	  if (!memcmp(Buffer, LastBuffer, header->cupsBytesPerLine))
	  {
	    putchar(':');
	    return;
	  }
	}

       /*
        * Convert the line to hex digits...
	*/

	for (ptr = Buffer, compptr = CompBuffer, i = header->cupsBytesPerLine;
	     i > 0;
	     i --, ptr ++)
        {
	  *compptr++ = hex[*ptr >> 4];
	  *compptr++ = hex[*ptr & 15];
	}

        *compptr = '\0';

       /*
        * Run-length compress the graphics...
	*/

	for (compptr = CompBuffer, repeat_char = CompBuffer[0], repeat_count = 1;
	     *compptr;
	     compptr ++)
	  if (*compptr == repeat_char)
	    repeat_count ++;
	  else
	  {
	    ZPLCompress(repeat_char, repeat_count);
	    repeat_char  = *compptr;
	    repeat_count = 1;
	  }

        if (repeat_char == '0')
	{
	 /*
	  * Handle 0's on the end of the line...
	  */

	  if (repeat_count & 1)
	    putchar('0');

	  putchar(',');
	}
	else
	  ZPLCompress(repeat_char, repeat_count);

       /*
        * Save this line for the next round...
	*/

	memcpy(LastBuffer, Buffer, header->cupsBytesPerLine);
	LastSet = 1;
        break;

    case ZEBRA_CPCL :
        if (Buffer[0] || memcmp(Buffer, Buffer + 1, header->cupsBytesPerLine))
	{
	  printf("CG %u 1 0 %d ", header->cupsBytesPerLine, y);
          fwrite(Buffer, 1, header->cupsBytesPerLine, stdout);
	  puts("\r");
	  fflush(stdout);
	}
	break;
  }
}


/*
 * 'ZPLCompress()' - Output a run-length compression sequence.
 */

void
ZPLCompress(char repeat_char,		/* I - Character to repeat */
	    int  repeat_count)		/* I - Number of repeated characters */
{
  if (repeat_count > 1)
  {
   /*
    * Print as many z's as possible - they are the largest denomination
    * representing 400 characters (zC stands for 400 adjacent C's)	
    */	

    while (repeat_count >= 400)
    {
      putchar('z');
      repeat_count -= 400;
    }

   /*
    * Then print 'g' through 'y' as multiples of 20 characters...
    */

    if (repeat_count >= 20)
    {
      putchar('f' + repeat_count / 20);
      repeat_count %= 20;
    }

   /*
    * Finally, print 'G' through 'Y' as 1 through 19 characters...
    */

    if (repeat_count > 0)
      putchar('F' + repeat_count);
  }

 /*
  * Then the character to be repeated...
  */

  putchar(repeat_char);
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
  cups_page_header_t	header;		/* Page header from file */
  int			y;		/* Current line */
  ppd_file_t		*ppd;		/* PPD file */
  int			num_options;	/* Number of options */
  cups_option_t		*options;	/* Options */


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
  * Open the PPD file and apply options...
  */

  num_options = cupsParseOptions(argv[5], 0, &options);

  if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL)
  {
    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, options);
  }

 /*
  * Initialize the print device...
  */

  Setup(ppd);

 /*
  * Process pages as needed...
  */

  Page      = 0;
  Canceled = 0;

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

    StartPage(ppd, &header);

   /*
    * Loop for each line on the page...
    */

    for (y = 0; y < header.cupsHeight && !Canceled; y ++)
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
      * Write it to the printer...
      */

      OutputLine(ppd, &header, y);
    }

   /*
    * Eject the page...
    */

    EndPage(ppd, &header);

    if (Canceled)
      break;
  }

 /*
  * Close the raster stream...
  */

  cupsRasterClose(ras);
  if (fd != 0)
    close(fd);

 /*
  * Close the PPD file and free the options...
  */

  ppdClose(ppd);
  cupsFreeOptions(num_options, options);

 /*
  * If no pages were printed, send an error message...
  */

  if (Page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: Ready to print.\n", stderr);

  return (Page == 0);
}


/*
 * End of "$Id$".
 */
