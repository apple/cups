/*
 * "$Id: pstops.c,v 1.17 1999/05/18 21:21:46 mike Exp $"
 *
 *   PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-1999 by Easy Software Products.
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

int	NumPages = 0;		/* Number of pages in file */
long	Pages[MAX_PAGES];	/* Offsets to each page */
char	PageLabels[MAX_PAGES][64];
				/* Page labels */
char	*PageRanges = NULL;	/* Range of pages selected */
char	*PageSet = NULL;	/* All, Even, Odd pages */
int	Order = 0,		/* 0 = normal, 1 = reverse pages */
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
  char		*val;		/* Option value */
  char		tempfile[255];	/* Temporary file name */
  FILE		*temp;		/* Temporary file */
  int		number;		/* Page number */
  int		slowcollate;	/* 1 if we need to collate manually */
  int		sloworder;	/* 1 if we need to order manually */
  char		line[8192];	/* Line buffer */
  float		g;		/* Gamma correction value */
  float		b;		/* Brightness factor */


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

  ppd = ppdOpenFile(getenv("PPD"));

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppd = SetCommonOptions(num_options, options, 1);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  if ((val = cupsGetOption("page-ranges", num_options, options)) != NULL)
    PageRanges = val;

  if ((val = cupsGetOption("page-set", num_options, options)) != NULL)
    PageSet = val;

  if ((val = cupsGetOption("copies", num_options, options)) != NULL)
    Copies = atoi(val);

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

    Collate = strcmp(val, "separate-documents-collated-copies") != 0;
  }

  if ((val = cupsGetOption("Collate", num_options, options)) != NULL &&
      strcmp(val, "True") == 0)
    Collate = 1;

  if ((val = cupsGetOption("OutputOrder", num_options, options)) != NULL &&
      strcmp(val, "Reverse") == 0)
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
    temp = fopen(tmpnam(tempfile), "wb+");

    if (temp == NULL)
      slowcollate = sloworder = 0;
  }

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

  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);
  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);

  if (NUp > 1)
    puts("userdict begin\n"
         "/ESPshowpage /showpage load def\n"
         "/showpage { } def\n"
         "end");

  if (g != 1.0 || b != 1.0)
    printf("{ neg 1 add %.3f exp neg 1 add %.3f mul } bind settransfer\n", g, b);

  if (Copies > 1 && (!Collate || !slowcollate))
    printf("/#copies %d def\n", Copies);

  if (strncmp(line, "%!PS-Adobe-", 11) == 0)
  {
   /*
    * OK, we have DSC comments; read until we find a %%Page comment...
    */

    while (psgets(line, sizeof(line), fp) != NULL)
      if (strncmp(line, "%%Page:", 7) == 0)
        break;
      else
        puts(line);

   /*
    * Then read all of the pages, filtering as needed...
    */

    for (;;)
    {
      if (strncmp(line, "%%Page:", 7) == 0)
      {
        if (sscanf(line, "%*s%*s%d", &number) == 1)
	{
	  if (!check_range(number))
	  {
	    while (psgets(line, sizeof(line), fp) != NULL)
	      if (strncmp(line, "%%Page:", 7) == 0)
	        break;
            continue;
          }

          if (!sloworder && NumPages > 0)
	    end_nup(NumPages - 1);

	  if (slowcollate || sloworder)
	    Pages[NumPages] = ftell(temp);

	  NumPages ++;

          if (!sloworder)
	  {
	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d %d\n", NumPages, Copies);

	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	    start_nup(NumPages - 1);
	  }
	}
      }
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
      end_nup((NumPages + NUp - 1) & (NUp - 1));

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
	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d 1\n", number + 1);

	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	    start_nup(number);
	    copy_bytes(temp, Pages[number + 1] - Pages[number]);
	    end_nup(number);
	  }

	  Copies --;
	}
      }
      else
      {
        do
	{
	  for (number = NumPages - 1; number >= 0; number --)
	  {
	    if (ppd == NULL || ppd->num_filters == 0)
	      fprintf(stderr, "PAGE: %d %d\n", NumPages - number,
	              slowcollate ? 1 : Copies);

	    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
	    start_nup(NumPages - 1 - number);
	    fseek(temp, Pages[number], SEEK_SET);
	    copy_bytes(temp, Pages[number + 1] - Pages[number]);
	    end_nup(NumPages - 1 - number);
	  }

	  Copies --;
	}
	while (Copies > 0 || !slowcollate);
      }
    }
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
      }
    }
  }

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
  char	*range;		/* Pointer into range string */
  int	lower, upper;	/* Lower and upper page numbers */


  if (PageSet != NULL)
  {
   /*
    * See if we only print even or odd pages...
    */

    if (strcmp(PageSet, "even") == 0 && (page & 1))
      return (0);
    if (strcmp(PageSet, "odd") == 0 && !(page & 1))
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
      upper = strtol(range, &range, 10);
    }
    else
    {
      lower = strtol(range, &range, 10);

      if (*range == '-')
      {
        range ++;
	if (!isdigit(*range))
	  upper = 65535;
	else
	  upper = strtol(range, &range, 10);
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
    if ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) < 1)
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
  puts("grestore");

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


  puts("gsave");

  if (Flip)
    printf("%.0f 0 translate -1 1 scale\n", PageWidth);

  switch (Orientation)
  {
    case 1 : /* Landscape */
        printf("%.0f 0 translate 90 rotate\n", PageLength);
        break;
    case 2 : /* Reverse Portrait */
        printf("%.0f %.0f translate 180 rotate\n", PageWidth, PageLength);
        break;
    case 3 : /* Reverse Landscape */
        printf("0 %.0f translate -90 rotate\n", PageWidth);
        break;
  }

  switch (NUp)
  {
    case 2 :
        x = number & 1;

        if (Orientation & 1)
	{
	  x = 1 - x;
          w = PageLength;
          l = w * PageLength / PageWidth;

          if (l > (PageWidth * 0.5))
          {
            l = PageWidth * 0.5;
            w = l * PageWidth / PageLength;
          }

          tx = PageWidth * 0.5 - l;
          ty = (PageLength - w) * 0.5;
        }
	else
	{
          l = PageWidth;
          w = l * PageWidth / PageLength;

          if (w > (PageLength * 0.5))
          {
            w = PageLength * 0.5;
            l = w * PageLength / PageWidth;
          }

          tx = PageLength * 0.5 - w;
          ty = (PageWidth - l) * 0.5;
        }

        if (Orientation & 1)
	{
          printf("0 %.0f translate -90 rotate\n", PageLength);
          printf("%.0f %.0f translate %.3f %.3f scale\n",
                 ty, tx + l * x, w / PageWidth, l / PageLength);
        }
        else
	{
          printf("%.0f 0 translate 90 rotate\n", PageWidth);
          printf("%.0f %.0f translate %.3f %.3f scale\n",
                 tx + w * x, ty, w / PageWidth, l / PageLength);
        }

	printf("newpath\n"
               "0 0 moveto\n"
               "%.0f 0 lineto\n"
               "%.0f %.0f lineto\n"
               "0 %.0f lineto\n"
               "closepath clip newpath\n",
               PageWidth, PageWidth, PageLength, PageLength);
        break;

    case 4 :
        x = number & 1;
	y = 1 - ((number & 2) != 0);
        w = PageWidth * 0.5;
        l = PageLength * 0.5;

	printf("%.0f %.0f translate 0.5 0.5 scale\n", x * w, y * l);
        printf("newpath\n"
               "0 0 moveto\n"
               "%.0f 0 lineto\n"
               "%.0f %.0f lineto\n"
               "0 %.0f lineto\n"
               "closepath clip newpath\n",
               PageWidth, PageWidth, PageLength, PageLength);
        break;
  }
}


/*
 * End of "$Id: pstops.c,v 1.17 1999/05/18 21:21:46 mike Exp $".
 */
