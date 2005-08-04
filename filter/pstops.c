/*
 * "$Id$"
 *
 *   PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2005 by Easy Software Products.
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
 *   main()        - Main entry...
 *   check_range() - Check to see if the current page is selected for
 *   copy_bytes()  - Copy bytes from the input file to stdout...
 *   do_prolog()   - Send the necessary document prolog commands...
 *   do_setup()    - Send the necessary document setup commands...
 *   end_nup()     - End processing for N-up printing...
 *   psbcp()       - Enable the binary communications protocol on the printer.
 *   psgets()      - Get a line from a file.
 *   pswrite()     - Write data from a file.
 *   start_nup()   - Start processing for N-up printing...
 */

/*
 * Include necessary headers...
 */

#include "common.h"


/*
 * Constants...
 */

#define MAX_PAGES	10000

#define BORDER_NONE	0		/* No border or hairline border */
#define BORDER_THICK	1		/* Think border */
#define BORDER_SINGLE	2		/* Single-line hairline border */
#define BORDER_SINGLE2	3		/* Single-line thick border */
#define BORDER_DOUBLE	4		/* Double-line hairline border */
#define BORDER_DOUBLE2	5		/* Double-line thick border */

#define LAYOUT_LRBT	0		/* Left to right, bottom to top */
#define LAYOUT_LRTB	1		/* Left to right, top to bottom */
#define LAYOUT_RLBT	2		/* Right to left, bottom to top */
#define LAYOUT_RLTB	3		/* Right to left, top to bottom */
#define LAYOUT_BTLR	4		/* Bottom to top, left to right */
#define LAYOUT_TBLR	5		/* Top to bottom, left to right */
#define LAYOUT_BTRL	6		/* Bottom to top, right to left */
#define LAYOUT_TBRL	7		/* Top to bottom, right to left */

#define LAYOUT_NEGATEY	1		/* The bits for the layout */
#define LAYOUT_NEGATEX	2		/* definitions above... */
#define LAYOUT_VERTICAL	4

#define PROT_STANDARD	0		/* Adobe standard protocol */
#define PROT_BCP	1		/* Adobe BCP protocol */
#define PROT_TBCP	2		/* Adobe TBCP protocol */


/*
 * Globals...
 */

int		NumPages = 0;		/* Number of pages in file */
long		Pages[MAX_PAGES];	/* Offsets to each page */
const char	*PageRanges = NULL;	/* Range of pages selected */
const char	*PageSet = NULL;	/* All, Even, Odd pages */
int		Order = 0,		/* 0 = normal, 1 = reverse pages */
		Flip = 0,		/* Flip/mirror pages */
		NUp = 1,		/* Number of pages on each sheet (1, 2, 4) */
		Collate = 0,		/* Collate copies? */
		Copies = 1,		/* Number of copies */
		UseESPsp = 0,		/* Use ESPshowpage? */
		Border = BORDER_NONE,	/* Border around pages */
		Layout = LAYOUT_LRTB,	/* Layout of N-up pages */
		NormalLandscape = 0,	/* Normal rotation for landscape? */
		Protocol = PROT_STANDARD;
					/* Transmission protocol to use */


/*
 * Local functions...
 */

static int	check_range(int page);
static void	copy_bytes(FILE *fp, size_t length);
static void	do_prolog(ppd_file_t *ppd);
static void 	do_setup(ppd_file_t *ppd, int copies,  int collate,
		         int slowcollate, float g, float b);
static void	end_nup(int number);
#define		is_first_page(p)	(NUp == 1 || (((p)+1) % NUp) == 1)
#define		is_last_page(p)		(NUp > 1 && (((p)+1) % NUp) == 0)
#define 	is_not_last_page(p)	(NUp > 1 && ((p) % NUp) != 0)
static void	psbcp(ppd_file_t *ppd);
static char	*psgets(char *buf, size_t *bytes, FILE *fp);
static size_t	pswrite(const char *buf, size_t bytes, FILE *fp);
static void	start_nup(int number, int show_border);


/*
 * 'main()' - Main entry...
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  FILE		*fp;		/* Print file */
  ppd_file_t	*ppd;		/* PPD file */
  ppd_attr_t	*attr;		/* Attribute in PPD file */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  const char	*val;		/* Option value */
  char		tempfile[255];	/* Temporary file name */
  FILE		*temp;		/* Temporary file */
  int		tempfd;		/* Temporary file descriptor */
  int		number;		/* Page number */
  int		slowcollate;	/* 1 if we need to collate manually */
  int		sloworder;	/* 1 if we need to order manually */
  int		slowduplex;	/* 1 if we need an even number of pages */
  char		line[8192];	/* Line buffer */
  size_t	len;		/* Length of line buffer */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */
  int		level;		/* Nesting level for embedded files */
  int		nbytes,		/* Number of bytes read */
		tbytes;		/* Total bytes to read for binary data */
  int		page;		/* Current page sequence number */
  int		real_page;	/* "Real" page number in document */
  int		page_count;	/* Page count for NUp */
  int		basepage;	/* Base page number */
  int		subpage;	/* Sub-page number */
  int		copy;		/* Current copy */
  int		saweof;		/* Did we see a %%EOF tag? */
  int		sent_espsp,	/* Did we send the ESPshowpage commands? */
		sent_prolog,	/* Did we send the prolog commands? */
		sent_setup;	/* Did we send the setup commands? */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Check command-line...
  */

  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: pstops job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
    fp = stdin;
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = fopen(argv[6], "rb")) == NULL)
    {
      fprintf(stderr, "ERROR: unable to open print file \"%s\" - %s\n",
              argv[6], strerror(errno));
      return (1);
    }
  }

 /*
  * Process command-line options and write the prolog...
  */

  g = 1.0;
  b = 1.0;

  Copies = atoi(argv[4]);

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppd = SetCommonOptions(num_options, options, 1);

  if (ppd && ppd->landscape > 0)
    NormalLandscape = 1;

  if ((val = cupsGetOption("page-ranges", num_options, options)) != NULL)
    PageRanges = val;

  if ((val = cupsGetOption("page-set", num_options, options)) != NULL)
    PageSet = val;

  if ((val = cupsGetOption("multiple-document-handling", num_options, options)) != NULL)
  {
   /*
    * This IPP attribute is unnecessarily complicated...
    *
    *   single-document, separate-documents-collated-copies, and
    *   single-document-new-sheet all require collated copies.
    *
    *   separate-documents-uncollated-copies allows for uncollated copies.
    */

    Collate = strcasecmp(val, "separate-documents-uncollated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      (!strcasecmp(val, "true") ||!strcasecmp(val, "on") ||
       !strcasecmp(val, "yes")))
    Collate = 1;

  if ((val = cupsGetOption("OutputOrder", num_options, options)) != NULL &&
      !strcasecmp(val, "Reverse"))
    Order = 1;

  if ((val = cupsGetOption("number-up", num_options, options)) != NULL)
    NUp = atoi(val);

  if ((val = cupsGetOption("page-border", num_options, options)) != NULL)
  {
    if (!strcasecmp(val, "none"))
      Border = BORDER_NONE;
    else if (!strcasecmp(val, "single"))
      Border = BORDER_SINGLE;
    else if (!strcasecmp(val, "single-thick"))
      Border = BORDER_SINGLE2;
    else if (!strcasecmp(val, "double"))
      Border = BORDER_DOUBLE;
    else if (!strcasecmp(val, "double-thick"))
      Border = BORDER_DOUBLE2;
  }

  if ((val = cupsGetOption("number-up-layout", num_options, options)) != NULL)
  {
    if (!strcasecmp(val, "lrtb"))
      Layout = LAYOUT_LRTB;
    else if (!strcasecmp(val, "lrbt"))
      Layout = LAYOUT_LRBT;
    else if (!strcasecmp(val, "rltb"))
      Layout = LAYOUT_RLTB;
    else if (!strcasecmp(val, "rlbt"))
      Layout = LAYOUT_RLBT;
    else if (!strcasecmp(val, "tblr"))
      Layout = LAYOUT_TBLR;
    else if (!strcasecmp(val, "tbrl"))
      Layout = LAYOUT_TBRL;
    else if (!strcasecmp(val, "btlr"))
      Layout = LAYOUT_BTLR;
    else if (!strcasecmp(val, "btrl"))
      Layout = LAYOUT_BTRL;
  }

  if ((val = cupsGetOption("gamma", num_options, options)) != NULL)
    g = atoi(val) * 0.001f;

  if ((val = cupsGetOption("brightness", num_options, options)) != NULL)
    b = atoi(val) * 0.01f;

  if ((val = cupsGetOption("mirror", num_options, options)) != NULL &&
      (!strcasecmp(val, "true") ||!strcasecmp(val, "on") ||
       !strcasecmp(val, "yes")))
    Flip = 1;

 /*
  * See if we have to filter the fast or slow way...
  */

  if (ppd && ppd->manual_copies && Duplex && Copies > 1)
  {
   /*
    * Force collated copies when printing a duplexed document to
    * a non-PS printer that doesn't do hardware copy generation.
    * Otherwise the copies will end up on the front/back side of
    * each page.  Also, set the "slowduplex" option to make sure
    * that we output an even number of pages...
    */

    Collate    = 1;
    slowduplex = 1;
  }
  else
    slowduplex = 0;

  if (ppdFindOption(ppd, "Collate") == NULL && Collate && Copies > 1)
    slowcollate = 1;
  else
    slowcollate = 0;

  if (ppdFindOption(ppd, "OutputOrder") == NULL && Order)
    sloworder = 1;
  else
    sloworder = 0;

 /*
  * If we need to filter slowly, then create a temporary file for page data...
  *
  * If the temp file can't be created, then we'll ignore the collating/output
  * order options...
  */

  if (sloworder || slowcollate)
  {
    tempfd = cupsTempFd(tempfile, sizeof(tempfile));
    if (tempfd < 0)
    {
      perror("ERROR: Unable to open temp file");
      temp = NULL;
    }
    else
      temp = fdopen(tempfd, "wb+");

    if (temp == NULL)
      slowcollate = sloworder = 0;
  }
  else
    temp = NULL;

 /*
  * See if we should use a binary transmission protocol...
  */

  if ((attr = ppdFindAttr(ppd, "cupsProtocol", NULL)) != NULL &&
      attr->value != NULL)
  {
    if (!strcasecmp(attr->value, "TBCP"))
      Protocol = PROT_TBCP;
    else if (!strcasecmp(attr->value, "BCP"))
    {
      Protocol = PROT_BCP;

      psbcp(ppd);
    }
  }

 /*
  * Write any "exit server" options that have been selected...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_EXIT);

 /*
  * Write any JCL commands that are needed to print PostScript code...
  */

  ppdEmitJCL(ppd, stdout, atoi(argv[1]), argv[2], argv[3]);

 /*
  * Read the first line to see if we have DSC comments...
  */

  len = sizeof(line);
  if (psgets(line, &len, fp) == NULL)
  {
    fputs("ERROR: Empty print file!\n", stderr);
    ppdClose(ppd);
    return (1);
  }

 /*
  * Handle leading PJL fun...
  */

  while (!strncmp(line, "\033%-12345X", 9) || !strncmp(line, "@PJL ", 5))
  {
   /*
    * Yup, we have leading PJL fun, so skip it until we hit the line
    * with "ENTER LANGUAGE"...
    */

    fputs("DEBUG: Skipping PJL header...\n", stderr);

    while (strstr(line, "ENTER LANGUAGE") == NULL)
    {
      len = sizeof(line);
      if (psgets(line, &len, fp) == NULL)
        break;
    }

    len = sizeof(line);
    if (psgets(line, &len, fp) == NULL)
      break;
  }

 /*
  * Switch to TBCP mode as needed...
  */

  if (Protocol == PROT_TBCP)
    fputs("\001M", stdout);

 /*
  * Start sending the document with any commands needed...
  */

  fwrite(line, 1, len, stdout);

  saweof      = 0;
  sent_espsp  = 0;
  sent_prolog = 0;
  sent_setup  = 0;

  if (Copies != 1 && (!Collate || !slowcollate))
  {
   /*
    * Tell the document processor the copy and duplex options
    * that are required...
    */

    printf("%%%%Requirements: numcopies(%d)%s%s\n", Copies,
           Collate ? " collate" : "",
	   Duplex ? " duplex" : "");

   /*
    * Apple uses RBI comments for various non-PPD options...
    */

    printf("%%RBINumCopies: %d\n", Copies);
  }
  else
  {
   /*
    * Tell the document processor the duplex option that is required...
    */

    if (Duplex)
      puts("%%Requirements: duplex");

   /*
    * Apple uses RBI comments for various non-PPD options...
    */

    puts("%RBINumCopies: 1");
  }

 /*
  * Figure out if we should use ESPshowpage or not...
  */

  val = cupsGetOption("page-label", num_options, options);

  if (val != NULL || getenv("CLASSIFICATION") != NULL || NUp > 1 ||
      Border || strstr(line, "EPS") != NULL)
  {
   /*
    * Yes, use ESPshowpage...
    */

    UseESPsp = 1;
  }

  fprintf(stderr, "DEBUG: slowcollate=%d, slowduplex=%d, sloworder=%d\n",
          slowcollate, slowduplex, sloworder);

  if (!strncmp(line, "%!PS-Adobe-", 11) && !strstr(line, "EPSF"))
  {
   /*
    * OK, we have DSC comments and this isn't an EPS file; read until we
    * find a %%Page comment...
    */

    puts("%%Pages: (atend)");

    level = 0;

    while (!feof(fp))
    {
      len = sizeof(line);
      if (psgets(line, &len, fp) == NULL)
        break;

      if (!strncmp(line, "%%", 2))
        fprintf(stderr, "DEBUG: %d %s", level, line);
      else if (line[0] != '%' && line[0] && !sent_espsp && UseESPsp)
      {
       /*
        * Send ESPshowpage stuff...
	*/

        sent_espsp = 1;

	puts("userdict/ESPshowpage/showpage load put\n"
	     "userdict/showpage{}put");
      }

      if (!strncmp(line, "%%BeginDocument:", 16) ||
          !strncmp(line, "%%BeginDocument ", 16))	/* Adobe Acrobat BUG */
      {
	fputs(line, stdout);
        level ++;
      }
      else if (!strncmp(line, "%%EndDocument", 13) && level > 0)
      {
	fputs(line, stdout);
        level --;
      }
      else if (!strncmp(line, "%cupsRotation:", 14) && level == 0)
      {
       /*
        * Reset orientation of document?
	*/

        int orient = (atoi(line + 14) / 90) & 3;

        if (orient != Orientation)
	{
	  Orientation = (4 - Orientation + orient) & 3;
	  UpdatePageVars();
	  Orientation = orient;
	}
      }
      else if (!strncmp(line, "%%BeginProlog", 13) && level == 0)
      {
       /*
        * Write the existing comment line, and then follow with patches
	* and prolog commands...
	*/

        fputs(line, stdout);

	if (!sent_prolog)
	{
	  sent_prolog = 1;
          do_prolog(ppd);
	}
      }
      else if (!strncmp(line, "%%BeginSetup", 12) && level == 0)
      {
       /*
        * Write the existing comment line, and then follow with document
	* setup commands...
	*/

        fputs(line, stdout);

	if (!sent_prolog)
	{
	  sent_prolog = 1;
          do_prolog(ppd);
	}

	if (!sent_setup)
	{
	  sent_setup = 1;
          do_setup(ppd, Copies, Collate, slowcollate, g, b);
	}
      }
      else if (!strncmp(line, "%%Page:", 7) && level == 0)
        break;
      else if (!strncmp(line, "%%BeginBinary:", 14) ||
               (!strncmp(line, "%%BeginData:", 12) &&
	        !strstr(line, "ASCII") && !strstr(line, "Hex")))
      {
       /*
        * Copy binary data...
	*/

        tbytes = atoi(strchr(line, ':') + 1);
	fputs(line, stdout);

	while (tbytes > 0)
	{
	  if (tbytes > sizeof(line))
	    nbytes = fread(line, 1, sizeof(line), fp);
	  else
	    nbytes = fread(line, 1, tbytes, fp);

          if (nbytes < 1)
	  {
	    perror("ERROR: Early end-of-file while reading binary data");
	    return (1);
	  }

	  pswrite(line, nbytes, stdout);
	  tbytes -= nbytes;
	}
      }
      else if (strncmp(line, "%%Pages:", 8) != 0)
        pswrite(line, len, stdout);
    }

   /*
    * Make sure we have the prolog and setup commands written...
    */

    if (!sent_prolog)
    {
      puts("%%BeginProlog");

      sent_prolog = 1;
      do_prolog(ppd);

      puts("%%EndProlog");
    }

    if (!sent_setup)
    {
      puts("%%BeginSetup");

      sent_setup = 1;
      do_setup(ppd, Copies, Collate, slowcollate, g, b);

      puts("%%EndSetup");
    }

    if (!sent_espsp && UseESPsp)
    {
     /*
      * Send ESPshowpage stuff...
      */

      sent_espsp = 1;

      puts("userdict/ESPshowpage/showpage load put\n"
	   "userdict/showpage{}put");
    }

   /*
    * Write the page and label prologs...
    */

    if (NUp == 2 || NUp == 6)
    {
     /*
      * For 2- and 6-up output, rotate the labels to match the orientation
      * of the pages...
      */

      if (Orientation & 1)
	WriteLabelProlog(val, PageBottom, PageWidth - PageLength + PageTop,
                	 PageLength);
      else
	WriteLabelProlog(val, PageLeft, PageRight, PageLength);
    }
    else
      WriteLabelProlog(val, PageBottom, PageTop, PageWidth);

   /*
    * Then read all of the pages, filtering as needed...
    */

    for (page = 1, real_page = 1;;)
    {
      if (!strncmp(line, "%%", 2))
        fprintf(stderr, "DEBUG: %d %s", level, line);

      if (!strncmp(line, "%%BeginDocument:", 16) ||
          !strncmp(line, "%%BeginDocument ", 16))	/* Adobe Acrobat BUG */
        level ++;
      else if (!strncmp(line, "%%EndDocument", 13) && level > 0)
        level --;
      else if (!strcmp(line, "\004") && len == 1)
        break;
      else if (!strncmp(line, "%%EOF", 5) && level == 0)
      {
        fputs("DEBUG: Saw EOF!\n", stderr);
        saweof = 1;
	break;
      }
      else if (!strncmp(line, "%%Page:", 7) && level == 0)
      {
	if (!check_range(real_page))
	{
	  while (!feof(fp))
	  {
	    len = sizeof(line);
	    if (psgets(line, &len, fp) == NULL)
	      break;

	    if (!strncmp(line, "%%", 2))
              fprintf(stderr, "DEBUG: %d %s", level, line);

	    if (!strncmp(line, "%%BeginDocument:", 16) ||
        	!strncmp(line, "%%BeginDocument ", 16))	/* Adobe Acrobat BUG */
              level ++;
	    else if (!strncmp(line, "%%EndDocument", 13) && level > 0)
              level --;
	    else if (!strncmp(line, "%%Page:", 7) && level == 0)
	    {
	      real_page ++;
	      break;
	    }
	    else if (!strncmp(line, "%%BeginBinary:", 14) ||
        	     (!strncmp(line, "%%BeginData:", 12) &&
	              !strstr(line, "ASCII") && !strstr(line, "Hex")))
	    {
	     /*
              * Skip binary data...
	      */

              tbytes = atoi(strchr(line, ':') + 1);

	      while (tbytes > 0)
	      {
		if (tbytes > sizeof(line))
		  nbytes = fread(line, 1, sizeof(line), fp);
		else
		  nbytes = fread(line, 1, tbytes, fp);

        	if (nbytes < 1)
		{
		  perror("ERROR: Early end-of-file while reading binary data");
		  return (1);
		}

		tbytes -= nbytes;
	      }
	    }
          }

          continue;
        }

        if (!sloworder && NumPages > 0)
	  end_nup(NumPages - 1);

	if (slowcollate || sloworder)
	  Pages[NumPages] = ftell(temp);

        if (!sloworder)
	{
	  if (is_first_page(NumPages))
	  {
	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d %d\n", page, slowcollate ? 1 : Copies);

            printf("%%%%Page: %d %d\n", page, page);
	    page ++;
	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	  }

	  start_nup(NumPages, 1);
	}

	NumPages ++;
	real_page ++;
      }
      else if (!strncmp(line, "%%BeginBinary:", 14) ||
               (!strncmp(line, "%%BeginData:", 12) &&
	        !strstr(line, "ASCII") && !strstr(line, "Hex")))
      {
       /*
        * Copy binary data...
	*/

        tbytes = atoi(strchr(line, ':') + 1);

	if (!sloworder)
	  fputs(line, stdout);
	if (slowcollate || sloworder)
	  fputs(line, temp);

	while (tbytes > 0)
	{
	  if (tbytes > sizeof(line))
	    nbytes = fread(line, 1, sizeof(line), fp);
	  else
	    nbytes = fread(line, 1, tbytes, fp);

          if (nbytes < 1)
	  {
	    perror("ERROR: Early end-of-file while reading binary data");
	    return (1);
	  }

          if (!sloworder)
	    pswrite(line, nbytes, stdout);

          if (slowcollate || sloworder)
	    fwrite(line, 1, nbytes, temp);

	  tbytes -= nbytes;
	}
      }
      else if (!strncmp(line, "%%Trailer", 9) && level == 0)
      {
        fputs("DEBUG: Saw Trailer!\n", stderr);
        break;
      }
      else
      {
        if (!sloworder)
          pswrite(line, len, stdout);

	if (slowcollate || sloworder)
	  fwrite(line, 1, len, temp);
      }

      len = sizeof(line);
      if (psgets(line, &len, fp) == NULL)
        break;
    }

    if (!sloworder)
    {
      end_nup(NumPages - 1);

      if (is_not_last_page(NumPages))
      {
	start_nup(NUp - 1, 0);
        end_nup(NUp - 1);
      }

      if (Duplex && !(page & 1))
      {
       /*
        * Make sure we have an even number of pages...
	*/

	if (ppd == NULL || ppd->num_filters == 0)
	  fprintf(stderr, "PAGE: %d %d\n", page, slowcollate ? 1 : Copies);

        printf("%%%%Page: %d %d\n", page, page);
	page ++;
	ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

	start_nup(NUp - 1, 0);
	puts("showpage");
        end_nup(NUp - 1);
      }
    }

    if (slowcollate || sloworder)
    {
      Pages[NumPages] = ftell(temp);

      if (!sloworder)
      {
        while (Copies > 1)
	{
	  rewind(temp);

	  for (number = 0; number < NumPages; number ++)
	  {
	    if (is_first_page(number))
	    {
	      if (ppd == NULL || ppd->num_filters == 0)
		fprintf(stderr, "PAGE: %d 1\n", page);

              printf("%%%%Page: %d %d\n", page, page);
	      page ++;
	      ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	    }

	    start_nup(number, 1);
	    copy_bytes(temp, Pages[number + 1] - Pages[number]);
	    end_nup(number);
	  }

          if (is_not_last_page(NumPages))
	  {
	    start_nup(NUp - 1, 0);
            end_nup(NUp - 1);
	  }

	  if (Duplex && !(page & 1))
	  {
	   /*
            * Make sure we have an even number of pages...
	    */

	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d 1\n", page);

            printf("%%%%Page: %d %d\n", page, page);
	    page ++;
	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

	    start_nup(NUp - 1, 0);
	    puts("showpage");
            end_nup(NUp - 1);
	  }

	  Copies --;
	}
      }
      else
      {
        page_count = (NumPages + NUp - 1) / NUp;
	copy       = 0;

        fprintf(stderr, "DEBUG: page_count=%d\n", page_count);

        do
	{
	  if (Duplex && (page_count & 1))
            basepage = page_count;
	  else
	    basepage = page_count - 1;

	  for (; basepage >= 0; basepage --)
	  {
	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d %d\n", page,
	              slowcollate ? 1 : Copies);

            printf("%%%%Page: %d %d\n", page, page);
	    page ++;

	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

            if (basepage >= page_count)
	    {
	      start_nup(NUp - 1, 0);
	      puts("showpage");
              end_nup(NUp - 1);
	    }
	    else
	    {
	      for (subpage = 0, number = basepage * NUp;
	           subpage < NUp && number < NumPages;
		   subpage ++, number ++)
	      {
		start_nup(number, 1);
		fseek(temp, Pages[number], SEEK_SET);
		copy_bytes(temp, Pages[number + 1] - Pages[number]);
		end_nup(number);
	      }

              if (is_not_last_page(number))
	      {
		start_nup(NUp - 1, 0);
        	end_nup(NUp - 1);
	      }
	    }
	  }

	  copy ++;
	}
	while (copy < Copies && slowcollate);
      }
    }

   /*
    * Copy the trailer, if any...
    */

    puts("%%Trailer");
    printf("%%%%Pages: %d\n", page - 1);

    if (UseESPsp)
      puts("userdict/showpage/ESPshowpage load put\n");

    while (!feof(fp))
    {
      len = sizeof(line);
      if (psgets(line, &len, fp) == NULL)
        break;

      if (!(!strcmp(line, "\004") && len == 1) &&
          strncmp(line, "%%Pages:", 8) != 0)
        pswrite(line, len, stdout);

      if (!strncmp(line, "%%EOF", 5))
      {
        fputs("DEBUG: Saw EOF!\n", stderr);
        saweof = 1;
	break;
      }
    }
  }
  else
  {
   /*
    * No DSC comments - write any page commands and then the rest of the file...
    */

    if (slowcollate && Copies > 1)
      printf("%%%%Pages: %d\n", Copies);
    else
      puts("%%Pages: 1");

    if (UseESPsp)
      puts("userdict/ESPshowpage/showpage load put\n"
	   "userdict/showpage{}put");

    puts("%%BeginProlog");
    WriteLabelProlog(val, PageBottom, PageTop, PageWidth);
    do_prolog(ppd);
    puts("%%EndProlog");

    puts("%%BeginSetup");
    do_setup(ppd, Copies, Collate, slowcollate, g, b);
    puts("%%EndSetup");

    if (ppd == NULL || ppd->num_filters == 0)
      fprintf(stderr, "PAGE: 1 %d\n", slowcollate ? 1 : Copies);

    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

    saweof = 1;

    while ((nbytes = fread(line, 1, sizeof(line), fp)) > 0)
    {
      pswrite(line, nbytes, stdout);

      if (slowcollate)
	fwrite(line, 1, nbytes, temp);
    }

    if (UseESPsp)
    {
      WriteLabels(Orientation);
      puts("ESPshowpage");
    }

    if (slowcollate)
    {
      while (Copies > 1)
      {
	if (ppd == NULL || ppd->num_filters == 0)
	  fputs("PAGE: 1 1\n", stderr);

        ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	rewind(temp);
	copy_bytes(temp, 0);
	Copies --;

        if (UseESPsp)
	{
	  WriteLabels(Orientation);
          puts("ESPshowpage");
	}
      }
    }
  }

 /*
  * Send %%EOF if needed...
  */

  if (!saweof)
    puts("%%EOF");

 /*
  * End the job with the appropriate JCL command or CTRL-D otherwise.
  */

  ppdEmitJCLEnd(ppd, stdout);

 /*
  * Close files and remove the temporary file if needed...
  */

  if (slowcollate || sloworder)
  {
    fclose(temp);
    unlink(tempfile);
  }

  ppdClose(ppd);

  if (fp != stdin)
    fclose(fp);

  return (0);
}


/*
 * 'check_range()' - Check to see if the current page is selected for
 *                   printing.
 */

static int		/* O - 1 if selected, 0 otherwise */
check_range(int page)	/* I - Page number */
{
  const char	*range;		/* Pointer into range string */
  int		lower, upper;	/* Lower and upper page numbers */


  if (PageSet != NULL)
  {
   /*
    * See if we only print even or odd pages...
    */

    if (!strcasecmp(PageSet, "even") && ((page - 1) % (NUp << 1)) <  NUp)
      return (0);
    if (!strcasecmp(PageSet, "odd") && ((page - 1) % (NUp << 1)) >= NUp)
      return (0);
  }

  if (PageRanges == NULL)
    return (1);		/* No range, print all pages... */

  for (range = PageRanges; *range != '\0';)
  {
    if (*range == '-')
    {
      lower = 1;
      range ++;
      upper = strtol(range, (char **)&range, 10);
    }
    else
    {
      lower = strtol(range, (char **)&range, 10);

      if (*range == '-')
      {
        range ++;
	if (!isdigit(*range & 255))
	  upper = 65535;
	else
	  upper = strtol(range, (char **)&range, 10);
      }
      else
        upper = lower;
    }

    if (page >= lower && page <= upper)
      return (1);

    if (*range == ',')
      range ++;
    else
      break;
  }

  return (0);
}


/*
 * 'copy_bytes()' - Copy bytes from the input file to stdout...
 */

static void
copy_bytes(FILE   *fp,		/* I - File to read from */
           size_t length)	/* I - Length of page data */
{
  char		buffer[8192];	/* Data buffer */
  size_t	nbytes,		/* Number of bytes read */
		nleft;		/* Number of bytes left/remaining */


  nleft = length;

  while (nleft > 0 || length == 0)
  {
    if (nleft > sizeof(buffer) || length == 0)
      nbytes = sizeof(buffer);
    else
      nbytes = nleft;

    if ((nbytes = fread(buffer, 1, nbytes, fp)) < 1)
      return;

    nleft -= nbytes;

    pswrite(buffer, nbytes, stdout);
  }
}


/*
 * 'do_prolog()' - Send the necessary document prolog commands...
 */

static void
do_prolog(ppd_file_t *ppd)		/* I - PPD file */
{
 /*
  * Send the document prolog commands...
  */

  if (ppd != NULL && ppd->patches != NULL)
  {
    puts("%%BeginFeature: *JobPatchFile 1");
    puts(ppd->patches);
    puts("%%EndFeature");
  }

  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);
}


/*
 * 'do_setup()' - Send the necessary document setup commands...
 */

static void
do_setup(ppd_file_t *ppd,		/* I - PPD file */
         int        copies,		/* I - Number of copies */
	 int        collate,		/* I - Collate output? */
	 int        slowcollate,	/* I - Slow collate */
	 float      g,			/* I - Gamma value */
	 float      b)			/* I - Brightness value */
{
 /*
  * Send all the printer-specific setup commands...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);

 /*
  * Set the number of copies for the job...
  */

  if (copies != 1 && (!collate || !slowcollate))
  {
    printf("%%RBIBeginNonPPDFeature: *NumCopies %d\n", copies);
    printf("%d/languagelevel where{pop languagelevel 2 ge}{false}ifelse{1 dict begin"
	    "/NumCopies exch def currentdict end " 
	    "setpagedevice}{userdict/#copies 3 -1 roll put}ifelse\n", copies);
    printf("%%RBIEndNonPPDFeature\n");
  }

 /*
  * Changes to the transfer function must be made AFTER any
  * setpagedevice code...
  */

  if (g != 1.0 || b != 1.0)
    printf("{ neg 1 add dup 0 lt { pop 1 } { %.3f exp neg 1 add } "
	   "ifelse %.3f mul } bind settransfer\n", g, b);

 /*
  * Make sure we have rectclip and rectstroke procedures of some sort...
  */

  WriteCommon();
}


/*
 * 'end_nup()' - End processing for N-up printing...
 */

static void
end_nup(int number)	/* I - Page number */
{
  puts("");

  if (Flip || Orientation || NUp > 1)
    puts("userdict /ESPsave get restore");

  switch (NUp)
  {
    case 1 :
	if (UseESPsp)
	{
	  WriteLabels(Orientation);
          puts("ESPshowpage");
	}
	break;

    case 2 :
    case 6 :
	if (is_last_page(number) && UseESPsp)
	{
	  if (Orientation & 1)
	  {
	   /*
	    * Rotate the labels back to portrait...
	    */

	    WriteLabels(Orientation - 1);
	  }
	  else if (Orientation == 0)
	  {
	   /*
	    * Rotate the labels to landscape...
	    */

	    WriteLabels(NormalLandscape ? 1 : 3);
	  }
	  else
	  {
	   /*
	    * Rotate the labels to landscape...
	    */

	    WriteLabels(NormalLandscape ? 3 : 1);
	  }

          puts("ESPshowpage");
	}
        break;

    default :
	if (is_last_page(number) && UseESPsp)
	{
	  WriteLabels(Orientation);
          puts("ESPshowpage");
	}
        break;
  }

  fflush(stdout);
}


/*
 * 'psbcp()' - Enable the binary communications protocol on the printer.
 */

static void
psbcp(ppd_file_t *ppd)		/* I - PPD file */
{
  if (ppd->jcl_begin)
    fputs(ppd->jcl_begin, stdout);
  if (ppd->jcl_ps)
    fputs(ppd->jcl_ps, stdout);

  if (ppd->language_level == 1)
  {
   /*
    * Use setsoftwareiomode for BCP mode...
    */

    fputs("%!PS-Adobe-3.0 ExitServer\n", stdout);
    fputs("%%Title: (BCP - Level 1)\n", stdout);
    fputs("%%EndComments\n", stdout);
    fputs("%%BeginExitServer: 0\n", stdout);
    fputs("serverdict begin 0 exitserver\n", stdout);
    fputs("%%EndExitServer\n", stdout);
    fputs("statusdict begin\n", stdout);
    fputs("/setsoftwareiomode known {100 setsoftwareiomode}\n", stdout);
    fputs("end\n", stdout);
    fputs("%EOF\n", stdout);
  }
  else
  {
   /*
    * Use setdevparams for BCP mode...
    */

    fputs("%!PS-Adobe-3.0\n", stdout);
    fputs("%%Title: (BCP - Level 2)\n", stdout);
    fputs("%%EndComments\n", stdout);
    fputs("currentsysparams\n", stdout);
    fputs("/CurInputDevice 2 copy known {\n", stdout);
    fputs("get\n", stdout);
    fputs("<</Protocol /Binary>> setdevparams\n", stdout);
    fputs("}{\n", stdout);
    fputs("pop pop\n", stdout);
    fputs("} ifelse\n", stdout);
    fputs("%EOF\n", stdout);
  }

  if (ppd->jcl_end)
    fputs(ppd->jcl_end, stdout);
  else if (ppd->num_filters == 0)
    putchar(0x04);
}


/*
 * 'psgets()' - Get a line from a file.
 *
 * Note:
 *
 *   This function differs from the gets() function in that it
 *   handles any combination of CR, LF, or CR LF to end input
 *   lines.
 */

static char *				/* O  - String or NULL if EOF */
psgets(char   *buf,			/* I  - Buffer to read into */
       size_t *bytes,			/* IO - Length of buffer */
       FILE   *fp)			/* I  - File to read from */
{
  char		*bufptr;		/* Pointer into buffer */
  int		ch;			/* Character from file */
  size_t	len;			/* Max length of string */


  len    = *bytes - 1;
  bufptr = buf;
  ch     = EOF;

  while ((bufptr - buf) < len)
  {
    if ((ch = getc(fp)) == EOF)
      break;

    if (ch == '\r')
    {
     /*
      * Got a CR; see if there is a LF as well...
      */

      ch = getc(fp);

      if (ch != EOF && ch != '\n')
      {
        ungetc(ch, fp);	/* Nope, save it for later... */
        ch = '\r';
      }
      else
        *bufptr++ = '\r';
      break;
    }
    else if (ch == '\n')
      break;
    else
      *bufptr++ = ch;
  }

 /*
  * Add a trailing newline if it is there...
  */

  if (ch == '\n' || ch == '\r')
  {
    if ((bufptr - buf) < len)
      *bufptr++ = ch;
    else
      ungetc(ch, fp);
  }

 /*
  * Nul-terminate the string and return it (or NULL for EOF).
  */

  *bufptr = '\0';
  *bytes  = bufptr - buf;

  if (ch == EOF && bufptr == buf)
    return (NULL);
  else
    return (buf);
}


/*
 * 'pswrite()' - Write data from a file.
 */

static size_t				/* O - Number of bytes written */
pswrite(const char *buf,		/* I - Buffer to write */
        size_t     bytes,		/* I - Bytes to write */
	FILE       *fp)			/* I - File to write to */
{
  size_t	count;			/* Remaining bytes */


  switch (Protocol)
  {
    case PROT_STANDARD :
        return (fwrite(buf, 1, bytes, fp));

    case PROT_BCP :
        for (count = bytes; count > 0; count --, buf ++)
	  switch (*buf)
	  {
	    case 0x01 : /* CTRL-A */
	    case 0x03 : /* CTRL-C */
	    case 0x04 : /* CTRL-D */
	    case 0x05 : /* CTRL-E */
	    case 0x11 : /* CTRL-Q */
	    case 0x13 : /* CTRL-S */
	    case 0x14 : /* CTRL-T */
	    case 0x1c : /* CTRL-\ */
	        putchar(0x01);
		putchar(*buf ^ 0x40);
	        break;

	    default :
	        putchar(*buf);
		break;
	  }
        return (bytes);

    case PROT_TBCP :
        for (count = bytes; count > 0; count --, buf ++)
	  switch (*buf)
	  {
	    case 0x01 : /* CTRL-A */
	    case 0x03 : /* CTRL-C */
	    case 0x04 : /* CTRL-D */
	    case 0x05 : /* CTRL-E */
	    case 0x11 : /* CTRL-Q */
	    case 0x13 : /* CTRL-S */
	    case 0x14 : /* CTRL-T */
	    case 0x1b : /* CTRL-[ (aka ESC) */
	    case 0x1c : /* CTRL-\ */
	        putchar(0x01);
		putchar(*buf ^ 0x40);
	        break;

	    default :
	        putchar(*buf);
		break;
	  }
        return (bytes);
  }

  return (fwrite(buf, 1, bytes, fp));
}


/*
 * 'start_nup()' - Start processing for N-up printing...
 */

static void
start_nup(int number,			/* I - Page number */
          int show_border)		/* I - Show the page border? */
{
  int	pos;				/* Position on page */
  int	x, y;				/* Relative position of subpage */
  float	w, l,				/* Width and length of subpage */
	tx, ty;				/* Translation values for subpage */
  float	pw, pl;				/* Printable width and length of full page */


  if (Flip || Orientation || NUp > 1)
    puts("userdict/ESPsave save put");

  if (Flip)
    printf("%.1f 0.0 translate -1 1 scale\n", PageWidth);

  pos = number % NUp;
  pw  = PageRight - PageLeft;
  pl  = PageTop - PageBottom;

  fprintf(stderr, "DEBUG: pw = %.1f, pl = %.1f\n", pw, pl);
  fprintf(stderr, "DEBUG: PageLeft = %.1f, PageRight = %.1f\n", PageLeft, PageRight);
  fprintf(stderr, "DEBUG: PageTop = %.1f, PageBottom = %.1f\n", PageTop, PageBottom);
  fprintf(stderr, "DEBUG: PageWidth = %.1f, PageLength = %.1f\n", PageWidth, PageLength);

  switch (Orientation)
  {
    case 1 : /* Landscape */
        printf("%.1f 0.0 translate 90 rotate\n", PageLength);
        break;
    case 2 : /* Reverse Portrait */
        printf("%.1f %.1f translate 180 rotate\n", PageWidth, PageLength);
        break;
    case 3 : /* Reverse Landscape */
        printf("0.0 %.1f translate -90 rotate\n", PageWidth);
        break;
  }

  if (Duplex && NUp > 1 && ((number / NUp) & 1))
    printf("%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
  else if (NUp > 1)
    printf("%.1f %.1f translate\n", PageLeft, PageBottom);

  switch (NUp)
  {
    default :
        w = PageWidth;
	l = PageLength;
	break;

    case 2 :
        if (Orientation & 1)
	{
          x = pos & 1;

          if (Layout & LAYOUT_NEGATEY)
	    x = 1 - x;

          w = pl;
          l = w * PageLength / PageWidth;

          if (l > (pw * 0.5))
          {
            l = pw * 0.5;
            w = l * PageWidth / PageLength;
          }

          tx = 0.5 * (pw * 0.5 - l);
          ty = 0.5 * (pl - w);

          if (NormalLandscape)
            printf("0.0 %.1f translate -90 rotate\n", pl);
	  else
	    printf("%.1f 0.0 translate 90 rotate\n", pw);

          printf("%.1f %.1f translate %.3f %.3f scale\n",
                 ty, tx + l * x, w / PageWidth, l / PageLength);
        }
	else
	{
          x = pos & 1;

          if (Layout & LAYOUT_NEGATEX)
	    x = 1 - x;

          l = pw;
          w = l * PageWidth / PageLength;

          if (w > (pl * 0.5))
          {
            w = pl * 0.5;
            l = w * PageLength / PageWidth;
          }

          tx = 0.5 * (pl * 0.5 - w);
          ty = 0.5 * (pw - l);

          if (NormalLandscape)
	    printf("%.1f 0.0 translate 90 rotate\n", pw);
	  else
            printf("0.0 %.1f translate -90 rotate\n", pl);

          printf("%.1f %.1f translate %.3f %.3f scale\n",
                 tx + w * x, ty, w / PageWidth, l / PageLength);
        }
        break;

    case 4 :
        if (Layout & LAYOUT_VERTICAL)
	{
	  x = (pos / 2) & 1;
          y = pos & 1;
        }
	else
	{
          x = pos & 1;
	  y = (pos / 2) & 1;
        }

        if (Layout & LAYOUT_NEGATEX)
	  x = 1 - x;

	if (Layout & LAYOUT_NEGATEY)
	  y = 1 - y;

        w = pw * 0.5;
	l = w * PageLength / PageWidth;

	if (l > (pl * 0.5))
	{
	  l = pl * 0.5;
	  w = l * PageWidth / PageLength;
	}

        tx = 0.5 * (pw * 0.5 - w);
        ty = 0.5 * (pl * 0.5 - l);

	printf("%.1f %.1f translate %.3f %.3f scale\n", tx + x * w, ty + y * l,
	       w / PageWidth, l / PageLength);
        break;

    case 6 :
        if (Orientation & 1)
	{
	  if (Layout & LAYOUT_VERTICAL)
	  {
	    x = pos / 3;
	    y = pos % 3;

            if (Layout & LAYOUT_NEGATEX)
	      x = 1 - x;

            if (Layout & LAYOUT_NEGATEY)
	      y = 2 - y;
	  }
	  else
	  {
	    x = pos & 1;
	    y = pos / 2;

            if (Layout & LAYOUT_NEGATEX)
	      x = 1 - x;

            if (Layout & LAYOUT_NEGATEY)
	      y = 2 - y;
	  }

          w = pl * 0.5;
          l = w * PageLength / PageWidth;

          if (l > (pw * 0.333))
          {
            l = pw * 0.333;
            w = l * PageWidth / PageLength;
          }

          tx = 0.5 * (pl - 2 * w);
          ty = 0.5 * (pw - 3 * l);

          if (NormalLandscape)
            printf("0.0 %.1f translate -90 rotate\n", pl);
	  else
	    printf("%.1f 0.0 translate 90 rotate\n", pw);

          printf("%.1f %.1f translate %.3f %.3f scale\n",
                 tx + x * w, ty + y * l, w / PageWidth, l / PageLength);
        }
	else
	{
	  if (Layout & LAYOUT_VERTICAL)
	  {
	    x = pos / 2;
	    y = pos & 1;

            if (Layout & LAYOUT_NEGATEX)
	      x = 2 - x;

            if (Layout & LAYOUT_NEGATEY)
	      y = 1 - y;
	  }
	  else
	  {
	    x = pos % 3;
	    y = pos / 3;

            if (Layout & LAYOUT_NEGATEX)
	      x = 2 - x;

            if (Layout & LAYOUT_NEGATEY)
	      y = 1 - y;
	  }

          l = pw * 0.5;
          w = l * PageWidth / PageLength;

          if (w > (pl * 0.333))
          {
            w = pl * 0.333;
            l = w * PageLength / PageWidth;
          }

          tx = 0.5 * (pl - 3 * w);
          ty = 0.5 * (pw - 2 * l);

          if (NormalLandscape)
	    printf("%.1f 0.0 translate 90 rotate\n", pw);
	  else
            printf("0.0 %.1f translate -90 rotate\n", pl);

          printf("%.1f %.1f translate %.3f %.3f scale\n",
                 tx + w * x, ty + l * y, w / PageWidth, l / PageLength);
        }
        break;

    case 9 :
        if (Layout & LAYOUT_VERTICAL)
	{
	  x = (pos / 3) % 3;
          y = pos % 3;
        }
	else
	{
          x = pos % 3;
	  y = (pos / 3) % 3;
        }

        if (Layout & LAYOUT_NEGATEX)
	  x = 2 - x;

	if (Layout & LAYOUT_NEGATEY)
	  y = 2 - y;

        w = pw * 0.333;
	l = w * PageLength / PageWidth;

	if (l > (pl * 0.333))
	{
	  l = pl * 0.333;
	  w = l * PageWidth / PageLength;
	}

        tx = 0.5 * (pw * 0.333 - w);
        ty = 0.5 * (pl * 0.333 - l);

	printf("%.1f %.1f translate %.3f %.3f scale\n", tx + x * w, ty + y * l,
	       w / PageWidth, l / PageLength);
        break;

    case 16 :
        if (Layout & LAYOUT_VERTICAL)
	{
	  x = (pos / 4) & 3;
          y = pos & 3;
        }
	else
	{
          x = pos & 3;
	  y = (pos / 4) & 3;
        }

        if (Layout & LAYOUT_NEGATEX)
	  x = 3 - x;

	if (Layout & LAYOUT_NEGATEY)
	  y = 3 - y;

        w = pw * 0.25;
	l = w * PageLength / PageWidth;

	if (l > (pl * 0.25))
	{
	  l = pl * 0.25;
	  w = l * PageWidth / PageLength;
	}

        tx = 0.5 * (pw * 0.25 - w);
        ty = 0.5 * (pl * 0.25 - l);

	printf("%.1f %.1f translate %.3f %.3f scale\n", tx + x * w, ty + y * l,
	       w / PageWidth, l / PageLength);
        break;
  }

 /*
  * Draw borders as necessary...
  */

  if (Border && show_border)
  {
    int		rects;		/* Number of border rectangles */
    float	fscale,		/* Scaling value for points */
		margin;		/* Current margin for borders */


    rects  = (Border & BORDER_DOUBLE) ? 2 : 1;
    fscale = PageWidth / w;
    margin = 2.25 * fscale;

   /*
    * Set the line width and color...
    */

    puts("gsave");
    printf("%.3f setlinewidth 0 setgray newpath\n",
           (Border & BORDER_THICK) ? 0.5 * fscale : 0.24 * fscale);

   /*
    * Draw border boxes...
    */

    for (; rects > 0; rects --, margin += 2 * fscale)
      if (NUp > 1)
	printf("%.1f %.1f %.1f %.1f ESPrs\n",
	       margin,
	       margin,
	       PageWidth - 2 * margin,
	       PageLength - 2 * margin);
      else
	printf("%.1f %.1f %.1f %.1f ESPrs\n",
               PageLeft + margin,
	       PageBottom + margin,
	       PageRight - PageLeft - 2 * margin,
	       PageTop - PageBottom - 2 * margin);

   /*
    * Restore pen settings...
    */

    puts("grestore");
  }

  if (NUp > 1)
  {
   /*
    * Clip the page that follows to the bounding box of the page...
    */

    printf("0 0 %.1f %.1f ESPrc\n", PageWidth, PageLength);
  }
}


/*
 * End of "$Id$".
 */
