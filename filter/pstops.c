/*
 * "$Id: pstops.c,v 1.1 1996/09/30 18:43:52 mike Exp $"
 *
 *   PostScript filter for espPrint, a collection of printer/image software.
 *
 *   Copyright 1993-1995 by Easy Software Products
 *
 *   These coded instructions, statements, and computer  programs  contain
 *   unpublished  proprietary  information  of Easy Software Products, and
 *   are protected by Federal copyright law.  They may  not  be  disclosed
 *   to  third  parties  or  copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: pstops.c,v $
 *   Revision 1.1  1996/09/30 18:43:52  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#define NO_ERROR		0	/* Standard error codes */
#define ERR_UNKNOWN		1	/* Other 'unknown' error */
#define ERR_NO_LICENSE		2	/* No license or bad license */
#define ERR_BAD_ARG		3	/* Bad command-line argument */
#define ERR_FILE_CONVERT	4	/* Could not convert file */
#define ERR_DATA_BUFFER		5	/* Could not allocate data buffer */

#define MAX_PAGES	10000
#define FALSE		0
#define TRUE		(!FALSE)


int	PrintNumPages = 0;
long	PrintPages[MAX_PAGES];
int	PrintEvenPages = 1,
	PrintOddPages = 1,
	PrintReversed = 0;
char	*PrintRange = NULL;
int	Verbosity = 0;


/*
 * 'test_page()' - Test the given page number.  Returns TRUE if the page
 *                 should be printed, false otherwise...
 */

int
test_page(int number)
{
  char	*range;
  int	lower, upper;


  if (((number & 1) && !PrintOddPages) ||
      (!(number & 1) && !PrintEvenPages))
    return (FALSE);

  if (PrintRange == NULL)
    return (TRUE);

  for (range = PrintRange; *range != '\0';)
  {
    if (*range == '-')
      lower = 0;
    else
    {
      lower = atoi(range);
      while (isdigit(*range) || *range == ' ')
        range ++;
    };

    if (*range == '-')
    {
      range ++;
      if (*range == '\0')
        upper = MAX_PAGES;
      else
        upper = atoi(range);

      while (isdigit(*range) || *range == ' ')
        range ++;

      if (number >= lower && number <= upper)
        return (TRUE);
    };

    if (number == lower)
      return (TRUE);

    if (*range != '\0')
      range ++;
  };

  return (FALSE);
}


/*
 * 'copy_bytes()' - Copy bytes from the input file to stdout...
 */

void
copy_bytes(FILE *fp,
           int  nleft)
{
  char	buffer[8192];
  int	nbytes;


  while (nleft > 0)
  {
    if (nleft > sizeof(buffer))
      nbytes = sizeof(buffer);
    else
      nbytes = nleft;

    nbytes = fread(buffer, 1, nbytes, fp);
    if (nbytes < 0)
      return;

    fwrite(buffer, 1, nbytes, stdout);
    nleft -= nbytes;
  };
}


/*
 * 'print_page()' - Print the specified page...
 */

void
print_page(FILE *fp,
           int  number)
{
  if (number < 1 || number > PrintNumPages || !test_page(number))
    return;

  if (Verbosity)
    fprintf(stderr, "psfilter: Printing page %d\n", number);

  number --;
  if (PrintPages[number] != ftell(fp))
    fseek(fp, PrintPages[number], SEEK_SET);

  copy_bytes(fp, PrintPages[number + 1] - PrintPages[number]);
}


/*
 * 'scan_file()' - Scan a file for %%Page markers...
 */

#define PS_DOCUMENT	0
#define PS_FILE		1
#define PS_FONT		2
#define PS_RESOURCE	3

#define PS_MAX		1000

#define pushdoc(n)	{ if (doclevel < PS_MAX) { doclevel ++; docstack[doclevel] = (n); if (Verbosity) fprintf(stderr, "psfilter: pushdoc(%d), doclevel = %d\n", (n), doclevel); }; }
#define popdoc(n)	{ if (doclevel >= 0 && docstack[doclevel] == (n)) doclevel --; if (Verbosity) fprintf(stderr, "psfilter: popdoc(%d), doclevel = %d\n", (n), doclevel); }

void
scan_file(FILE *fp)
{
  char	line[8192];
  int	doclevel,		/* Sub-document stack level */
	docstack[PS_MAX + 1];	/* Stack contents... */


  PrintNumPages = 0;
  PrintPages[0] = 0;
  doclevel = -1;

  rewind(fp);

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    if (line[0] == '\r')
      strcpy(line, line + 1);		/* Strip leading CR */
    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';	/* Strip trailing LF */
    if (line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = '\0';	/* Strip trailing CR */

    if (line[0] == '%' && line[1] == '%')
    {
      if (Verbosity)
        fprintf(stderr, "psfilter: Control line - %s\n", line);

     /*
      * Note that we do not (correctly) check for colons after the BeginXXXX control
      * lines because Adobe's Acrobat product produces incorrect output!
      */

      if (strncmp(line, "%%BeginDocument", 15) == 0)
	pushdoc(PS_DOCUMENT)
      else if (strncmp(line, "%%BeginFont", 11) == 0)
	pushdoc(PS_FONT)
      else if (strncmp(line, "%%BeginFile", 11) == 0)
	pushdoc(PS_FILE)
      else if (strncmp(line, "%%BeginResource", 13) == 0)
	pushdoc(PS_RESOURCE)
      else if (strcmp(line, "%%EndDocument") == 0)
	popdoc(PS_DOCUMENT)
      else if (strcmp(line, "%%EndFont") == 0)
	popdoc(PS_FONT)
      else if (strcmp(line, "%%EndFile") == 0)
	popdoc(PS_FILE)
      else if (strcmp(line, "%%EndResource") == 0)
	popdoc(PS_RESOURCE)
      else if (strncmp(line, "%%Page:", 7) == 0 && doclevel < 0)
      {
	if (Verbosity)
	  fprintf(stderr, "psfilter: Page %d begins at offset %d\n",
	          PrintNumPages + 2, PrintPages[PrintNumPages]);

	PrintNumPages ++;
      }
      else if (strcmp(line, "%%Trailer") == 0 && doclevel < 0)
        break;
      else if (strcmp(line, "%%EOF") == 0)
      {
        doclevel --;
        if (doclevel < 0)
          doclevel = -1;
      };
    };

    PrintPages[PrintNumPages] = ftell(fp);
  };

  rewind(fp);

  if (PrintNumPages == 0)
  {
    fputs("psfilter: Warning - this PostScript file does not conform to the DSC!\n", stderr);

    PrintPages[1] = PrintPages[0];
    PrintPages[0] = 0;
    PrintNumPages = 1;
  }
  else if (Verbosity)
    fprintf(stderr, "psfilter: Saw %d pages total.\n", PrintNumPages);
}


/*
 * 'print_file()' - Print a file...
 */

void
print_file(char *filename)
{
  FILE	*fp;
  int	number;
  long	end;


  if ((fp = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "psfilter: Unable to open file \'%s\' for reading - %s\n",
            filename, strerror(errno));
    exit(1);
  };

  scan_file(fp);

  copy_bytes(fp, PrintPages[0]);

  if (PrintReversed)
    for (number = PrintNumPages; number > 0; number --)
      print_page(fp, number);
  else
    for (number = 1; number <= PrintNumPages; number ++)
      print_page(fp, number);

  fseek(fp, 0, SEEK_END);
  end = ftell(fp);
  fseek(fp, PrintPages[PrintNumPages], SEEK_SET);

  copy_bytes(fp, end - PrintPages[PrintNumPages]);

  fclose(fp);
}


void
usage(void)
{
  fputs("Usage: psfilter [-e] [-o] [-r] [-p<pages>] [-h] [-D] infile\n", stderr);
  exit(ERR_BAD_ARG);
}


/*
 * 'main()' - Main entry...
 */

void
main(int  argc,
     char *argv[])
{
  int	i, nfiles;
  char	*opt;
  char	tempfile[255];
  FILE	*temp;
  char	buffer[8192];


  for (i = 1, nfiles = 0; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
        {
          default :
          case 'h' : /* Help */
              usage();
              break;

          case 'e' : /* Print even pages */
              PrintEvenPages = 1;
              PrintOddPages  = 0;
              break;
          case 'o' : /* Print odd pages */
              PrintEvenPages = 0;
              PrintOddPages  = 1;
              break;
          case 'r' : /* Print pages reversed */
              PrintReversed = 1;
              break;
          case 'p' : /* Print page range */
              PrintRange = opt + 1;
              opt += strlen(opt) - 1;
              break;
          case 'D' : /* Debug ... */
              Verbosity ++;
              break;
        }
    else
    {
      print_file(argv[i]);
      nfiles ++;
    };

  if (nfiles == 0)
  {
   /*
    * Copy stdin to a temporary file and filter the temporary file.
    */

    if ((temp = fopen(tmpnam(tempfile), "w")) == NULL)
      exit(ERR_DATA_BUFFER);

    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
      fputs(buffer, temp);
    fclose(temp);

    print_file(tempfile);

    unlink(tempfile);
  };
}


/*
 * End of "$Id: pstops.c,v 1.1 1996/09/30 18:43:52 mike Exp $".
 */
