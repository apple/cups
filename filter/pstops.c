/*
 * "$Id: pstops.c,v 1.44 2000/10/04 14:47:19 mike Exp $"
 *
 *   PostScript filter for the Common UNIX Printing System (CUPS).
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
 *   main()        - Main entry...
 *   check_range() - Check to see if the current page is selected for
 *   copy_bytes()  - Copy bytes from the input file to stdout...
 *   end_nup()     - End processing for N-up printing...
 *   psgets()      - Get a line from a file.
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


/*
 * Globals...
 */

int		NumPages = 0;		/* Number of pages in file */
long		Pages[MAX_PAGES];	/* Offsets to each page */
char		PageLabels[MAX_PAGES][64];
					/* Page labels */
const char	*PageRanges = NULL;	/* Range of pages selected */
const char	*PageSet = NULL;	/* All, Even, Odd pages */
int		Order = 0,		/* 0 = normal, 1 = reverse pages */
		Flip = 0,		/* Flip/mirror pages */
		NUp = 1,		/* Number of pages on each sheet (1, 2, 4) */
		Collate = 0,		/* Collate copies? */
		Copies = 1;		/* Number of copies */


/*
 * Local functions...
 */

static int	check_range(int page);
static void	copy_bytes(FILE *fp, size_t length);
static void	end_nup(int number);
static char	*psgets(char *buf, size_t len, FILE *fp);
static void	start_nup(int number);


/*
 * 'main()' - Main entry...
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  FILE		*fp;		/* Print file */
  ppd_file_t	*ppd;		/* PPD file */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  const char	*val;		/* Option value */
  char		tempfile[255];	/* Temporary file name */
  FILE		*temp;		/* Temporary file */
  int		number;		/* Page number */
  int		slowcollate;	/* 1 if we need to collate manually */
  int		sloworder;	/* 1 if we need to order manually */
  char		line[8192];	/* Line buffer */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */
  int		level;		/* Nesting level for embedded files */
  int		nbytes,		/* Number of bytes read */
		tbytes;		/* Total bytes to read for binary data */
  int		page;		/* Current page sequence number */
  int		page_count;	/* Page count for NUp */
  int		subpage;	/* Sub-page number */
  int		copy;		/* Current copy */
  int		saweof;		/* Did we see a %%EOF tag? */


 /* 
  * Check arguments...
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
      perror("ERROR: unable to open print file - ");
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
    *   separate-documents-collated-copies allows for uncollated copies.
    */

    Collate = strcasecmp(val, "separate-documents-collated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      strcasecmp(val, "True") == 0)
    Collate = 1;

  if ((val = cupsGetOption("OutputOrder", num_options, options)) != NULL &&
      strcasecmp(val, "Reverse") == 0)
    Order = 1;

  if ((val = cupsGetOption("number-up", num_options, options)) != NULL)
    NUp = atoi(val);

  if ((val = cupsGetOption("gamma", num_options, options)) != NULL)
    g = atoi(val) * 0.001f;

  if ((val = cupsGetOption("brightness", num_options, options)) != NULL)
    b = atoi(val) * 0.01f;

 /*
  * See if we have to filter the fast or slow way...
  */

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
    temp = fopen(cupsTempFile(tempfile, sizeof(tempfile)), "wb+");

    if (temp == NULL)
      slowcollate = sloworder = 0;
  }
  else
    temp = NULL;

 /*
  * Write any "exit server" options that have been selected...
  */

  ppdEmit(ppd, stdout, PPD_ORDER_EXIT);

 /*
  * Write any JCL commands that are needed to print PostScript code...
  */

  if (ppd != NULL && ppd->jcl_begin && ppd->jcl_ps)
  {
    fputs(ppd->jcl_begin, stdout);
    ppdEmit(ppd, stdout, PPD_ORDER_JCL);

    if (strncmp(ppd->jcl_ps, "@PJL", 4) == 0)
    {
     /*
      * Send other PJL commands before we enter PostScript mode...
      */

      printf("@PJL JOB NAME = \"%s\" DISPLAY = \"%s %s %s\"\n", argv[3],
             argv[1], argv[2], argv[3]);
    }

    fputs(ppd->jcl_ps, stdout);
  }

 /*
  * Read the first line to see if we have DSC comments...
  */

  if (psgets(line, sizeof(line), fp) == NULL)
  {
    fputs("ERROR: Empty print file!\n", stderr);
    ppdClose(ppd);
    return (1);
  }

 /*
  * Start sending the document with any commands needed...
  */

  puts(line);

  saweof = 0;

  if (ppd != NULL && ppd->patches != NULL)
    puts(ppd->patches);

  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);
  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);

  if (NUp > 1)
    puts("userdict begin\n"
         "/ESPshowpage /showpage load def\n"
         "/showpage { } def\n"
         "end");

  if (g != 1.0 || b != 1.0)
    printf("{ neg 1 add dup 0 lt { pop 1 } { %.3f exp neg 1 add } "
           "ifelse %.3f mul } bind settransfer\n", g, b);

  if (Copies > 1 && (!Collate || !slowcollate))
    printf("/#copies %d def\n", Copies);

  if (strncmp(line, "%!PS-Adobe-", 11) == 0)
  {
   /*
    * OK, we have DSC comments; read until we find a %%Page comment...
    */

    level = 0;

    while (psgets(line, sizeof(line), fp) != NULL)
      if (strncmp(line, "%%BeginDocument:", 16) == 0 ||
          strncmp(line, "%%BeginDocument ", 16) == 0)	/* Adobe Acrobat BUG */
        level ++;
      else if (strcmp(line, "%%EndDocument") == 0 && level > 0)
        level --;
      else if (strncmp(line, "%%Page:", 7) == 0 && level == 0)
        break;
      else if (strncmp(line, "%%BeginBinary:", 14) == 0 ||
               (strncmp(line, "%%BeginData:", 12) == 0 &&
	        strstr(line, "Binary") != NULL))
      {
       /*
        * Copy binary data...
	*/

        tbytes = atoi(strchr(line, ':') + 1);
	while (tbytes > 0)
	{
	  if (tbytes > sizeof(line))
	    nbytes = fread(line, 1, sizeof(line), fp);
	  else
	    nbytes = fread(line, 1, tbytes, fp);

	  fwrite(line, 1, nbytes, stdout);
	  tbytes -= nbytes;
	}
      }
      else
        puts(line);

   /*
    * Then read all of the pages, filtering as needed...
    */

    for (page = 1;;)
    {
      if (strncmp(line, "%%BeginDocument:", 16) == 0 ||
          strncmp(line, "%%BeginDocument ", 16) == 0)	/* Adobe Acrobat BUG */
        level ++;
      else if (strcmp(line, "%%EndDocument") == 0 && level > 0)
        level --;
      else if (strcmp(line, "%%EOF") == 0 && level == 0)
        saweof = 1;
      else if (strncmp(line, "%%Page:", 7) == 0 && level == 0)
      {
        if (sscanf(line, "%*s%*s%d", &number) == 1)
	{
	  if (!check_range(number))
	  {
	    while (psgets(line, sizeof(line), fp) != NULL)
	      if (strncmp(line, "%%BeginDocument:", 16) == 0 ||
        	  strncmp(line, "%%BeginDocument ", 16) == 0)	/* Adobe Acrobat BUG */
        	level ++;
	      else if (strcmp(line, "%%EndDocument") == 0 && level > 0)
        	level --;
	      else if (strncmp(line, "%%Page:", 7) == 0 && level == 0)
	        break;

            continue;
          }

          if (!sloworder && NumPages > 0)
	    end_nup(NumPages - 1);

	  if (slowcollate || sloworder)
	    Pages[NumPages] = ftell(temp);

          if (!sloworder)
	  {
	    if ((NumPages & (NUp - 1)) == 0)
	    {
	      if (ppd == NULL || ppd->num_filters == 0)
		fprintf(stderr, "PAGE: %d %d\n", page, Copies);

              printf("%%%%Page: %d %d\n", page, page);
	      page ++;
	      ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	    }

	    start_nup(NumPages);
	  }

	  NumPages ++;
	}
      }
      else if (strncmp(line, "%%BeginBinary:", 14) == 0 ||
               (strncmp(line, "%%BeginData:", 12) == 0 &&
	        strstr(line, "Binary") != NULL))
      {
       /*
        * Copy binary data...
	*/

        tbytes = atoi(strchr(line, ':') + 1);
	while (tbytes > 0)
	{
	  if (tbytes > sizeof(line))
	    nbytes = fread(line, 1, sizeof(line), fp);
	  else
	    nbytes = fread(line, 1, tbytes, fp);

          if (!sloworder)
	    fwrite(line, 1, nbytes, stdout);

          if (slowcollate || sloworder)
	    fwrite(line, 1, nbytes, stdout);

	  tbytes -= nbytes;
	}
      }
      else if (strcmp(line, "%%Trailer") == 0 && level == 0)
        break;
      else
      {
        if (!sloworder)
	  puts(line);

	if (slowcollate || sloworder)
	{
	  fputs(line, temp);
	  putc('\n', temp);
	}
      }

      if (psgets(line, sizeof(line), fp) == NULL)
        break;
    }

    if (!sloworder)
    {
      end_nup(NumPages - 1);

      if (NumPages & (NUp - 1))
      {
	start_nup(NUp - 1);
        end_nup(NUp - 1);
      }
    }

    if (slowcollate || sloworder)
    {
      Pages[NumPages] = ftell(temp);
      page = 1;

      if (!sloworder)
      {
        while (Copies > 0)
	{
	  rewind(temp);

	  for (number = 0; number < NumPages; number ++)
	  {
	    if ((number & (NUp - 1)) == 0)
	    {
	      if (ppd == NULL || ppd->num_filters == 0)
		fprintf(stderr, "PAGE: %d 1\n", page);

              printf("%%%%Page: %d %d\n", page, page);
	      page ++;
	      ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	    }

	    start_nup(number);
	    copy_bytes(temp, Pages[number + 1] - Pages[number]);
	    end_nup(number);
	  }

          if (NumPages & (NUp - 1))
	  {
	    start_nup(NUp - 1);
            end_nup(NUp - 1);
	  }

	  Copies --;
	}
      }
      else
      {
        page_count = (NumPages + NUp - 1) / NUp;
	copy       = 0;

        do
	{
	  for (page = page_count - 1; page >= 0; page --)
	  {
	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d %d\n", page + 1,
	              slowcollate ? 1 : Copies);

            if (slowcollate)
              printf("%%%%Page: %d %d\n", page + 1,
	             page_count - page + copy * page_count);
            else
	      printf("%%%%Page: %d %d\n", page + 1, page_count - page);

	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

	    for (subpage = 0, number = page * NUp;
	         subpage < NUp && number < NumPages;
		 subpage ++, number ++)
	    {
	      start_nup(number);
	      fseek(temp, Pages[number], SEEK_SET);
	      copy_bytes(temp, Pages[number + 1] - Pages[number]);
	      end_nup(number);
	    }

            if (number & (NUp - 1))
	    {
	      start_nup(NUp - 1);
              end_nup(NUp - 1);
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

    while ((nbytes = fread(line, 1, sizeof(line), fp)) > 0)
      fwrite(line, 1, nbytes, stdout);
  }
  else
  {
   /*
    * No DSC comments - write any page commands and then the rest of the file...
    */

    if (ppd == NULL || ppd->num_filters == 0)
      fprintf(stderr, "PAGE: 1 %d\n", slowcollate ? 1 : Copies);

    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);

    while (psgets(line, sizeof(line), fp) != NULL)
    {
      puts(line);

      if (slowcollate)
      {
	fputs(line, temp);
	putc('\n', temp);
      }
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

  if (ppd != NULL && ppd->jcl_end)
    fputs(ppd->jcl_end, stdout);
  else
    putchar(0x04);

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

    if (strcasecmp(PageSet, "even") == 0 && (page & 1))
      return (0);
    if (strcasecmp(PageSet, "odd") == 0 && !(page & 1))
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
	if (!isdigit(*range))
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
    if (nleft > sizeof(buffer))
      nbytes = sizeof(buffer);
    else
      nbytes = nleft;

    if ((nbytes = fread(buffer, 1, nbytes, fp)) < 1)
      return;

    nleft -= nbytes;

    fwrite(buffer, 1, nbytes, stdout);
  }
}


/*
 * 'end_nup()' - End processing for N-up printing...
 */

static void
end_nup(int number)	/* I - Page number */
{
  if (Flip || Orientation || NUp > 1)
    puts("ESPsave restore");

  switch (NUp)
  {
    case 2 :
	if ((number & 1) == 1)
          puts("ESPshowpage");
        break;

    case 4 :
	if ((number & 3) == 3)
          puts("ESPshowpage");
        break;
  }
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

static char *		/* O - String or NULL if EOF */
psgets(char   *buf,	/* I - Buffer to read into */
       size_t len,	/* I - Length of buffer */
       FILE   *fp)	/* I - File to read from */
{
  char	*bufptr;	/* Pointer into buffer */
  int	ch;		/* Character from file */


  len --;
  bufptr = buf;
  ch     = EOF;

  while ((bufptr - buf) < len)
  {
    if ((ch = getc(fp)) == EOF)
      break;

    if (ch == 0x0d)
    {
     /*
      * Got a CR; see if there is a LF as well...
      */

      ch = getc(fp);
      if (ch != EOF && ch != 0x0a)
        ungetc(ch, fp);	/* Nope, save it for later... */

      break;
    }
    else if (ch == 0x0a)
      break;
    else
      *bufptr++ = ch;
  }

 /*
  * Nul-terminate the string and return it (or NULL for EOF).
  */

  *bufptr = '\0';

  if (ch == EOF && bufptr == buf)
    return (NULL);
  else
    return (buf);
}


/*
 * 'start_nup()' - Start processing for N-up printing...
 */

static void
start_nup(int number)	/* I - Page number */
{
  int	x, y;		/* Relative position of subpage */
  float	w, l,		/* Width and length of subpage */
	tx, ty;		/* Translation values for subpage */
  float	pw, pl;		/* Printable width and length of full page */


  if (Flip || Orientation || NUp > 1)
    puts("/ESPsave save def");

  if (Flip)
    printf("%.1f 0.0 translate -1 1 scale\n", PageWidth);

  pw = PageRight - PageLeft;
  pl = PageTop - PageBottom;

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

  switch (NUp)
  {
    case 2 :
        x = number & 1;

        if (Orientation & 1)
	{
	  x = 1 - x;
          w = pl;
          l = w * PageLength / PageWidth;

          if (l > (pw * 0.5))
          {
            l = pw * 0.5;
            w = l * PageWidth / PageLength;
          }

          tx = pw * 0.5 - l;
          ty = (pl - w) * 0.5;
        }
	else
	{
          l = pw;
          w = l * PageWidth / PageLength;

          if (w > (pl * 0.5))
          {
            w = pl * 0.5;
            l = w * PageLength / PageWidth;
          }

          tx = pl * 0.5 - w;
          ty = (pw - l) * 0.5;
        }

        if (Duplex && (number & 2))
	  printf("%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
	else
	  printf("%.1f %.1f translate\n", PageLeft, PageBottom);

        if (Orientation & 1)
	{
          printf("0.0 %.1f translate -90 rotate\n", pl);
          printf("%.1f %.1f translate %.3f %.3f scale\n",
                 ty, tx + l * x, w / PageWidth, l / PageLength);
        }
        else
	{
          printf("%.1f 0.0 translate 90 rotate\n", pw);
          printf("%.1f %.1f translate %.3f %.3f scale\n",
                 tx + w * x, ty, w / PageWidth, l / PageLength);
        }

	printf("newpath\n"
               "0.0 0.0 moveto\n"
               "%.1f 0.0 lineto\n"
               "%.1f %.1f lineto\n"
               "0.0 %.1f lineto\n"
               "closepath clip newpath\n",
               PageWidth, PageWidth, PageLength, PageLength);
        break;

    case 4 :
        x = number & 1;
	y = 1 - ((number & 2) != 0);

        w = pw * 0.5;
	l = w * PageLength / PageWidth;

	if (l > (pl * 0.5))
	{
	  l = pl * 0.5;
	  w = l * PageWidth / PageLength;
	}

        if (Duplex && (number & 4))
	  printf("%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
	else
	  printf("%.1f %.1f translate\n", PageLeft, PageBottom);

	printf("%.1f %.1f translate %.3f %.3f scale\n", x * w, y * l,
	       w / PageWidth, l / PageLength);
        printf("newpath\n"
               "0.0 0.0 moveto\n"
               "%.1f 0.0 lineto\n"
               "%.1f %.1f lineto\n"
               "0.0 %.1f lineto\n"
               "closepath clip newpath\n",
               PageWidth, PageWidth, PageLength, PageLength);
        break;
  }
}


/*
 * End of "$Id: pstops.c,v 1.44 2000/10/04 14:47:19 mike Exp $".
 */
