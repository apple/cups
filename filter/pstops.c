/*
 * "$Id: pstops.c,v 1.3 1997/06/19 20:05:05 mike Exp $"
 *
 *   PostScript filter for espPrint, a collection of printer drivers.
 *
 *   Copyright 1993-1997 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law.  They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 *
 * Revision History:
 *
 *   $Log: pstops.c,v $
 *   Revision 1.3  1997/06/19 20:05:05  mike
 *   Optimized code so that non-filtered output is just copied to stdout.
 *
 *   Revision 1.2  1996/10/23  20:06:28  mike
 *   Added gamma and brightness correction.
 *   Added 1/2/4up printing.
 *   Added 'userdict' code around gamma/brightness stuff.
 *
 *   Revision 1.1  1996/09/30  18:43:52  mike
 *   Initial revision
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
#include <pod.h>
#include <errorcodes.h>
#include <printutil.h>


#define MAX_PAGES	10000
#define FALSE		0
#define TRUE		(!FALSE)

int	PrintNumPages = 0;
long	PrintPages[MAX_PAGES];
int	PrintEvenPages = 1,
	PrintOddPages = 1,
	PrintReversed = 0,
	PrintColor = 0,
	PrintWidth = 612,
	PrintLength = 792,
	PrintFlip = 0;
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

int
print_page(FILE *fp,
           int  number)
{
  if (number < 1 || number > PrintNumPages || !test_page(number))
    return (0);

  if (Verbosity)
    fprintf(stderr, "psfilter: Printing page %d\n", number);

  number --;
  if (PrintPages[number] != ftell(fp))
    fseek(fp, PrintPages[number], SEEK_SET);

  copy_bytes(fp, PrintPages[number + 1] - PrintPages[number]);

  return (1);
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
      else if (strncmp(line, "%%BeginResource", 15) == 0)
	pushdoc(PS_RESOURCE)
      else if (strncmp(line, "%%EndDocument", 13) == 0)
	popdoc(PS_DOCUMENT)
      else if (strncmp(line, "%%EndFont", 9) == 0)
	popdoc(PS_FONT)
      else if (strncmp(line, "%%EndFile", 9) == 0)
	popdoc(PS_FILE)
      else if (strncmp(line, "%%EndResource", 13) == 0)
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
 * 'print_header()' - Print the output header...
 */

void
print_header(float gammaval[2],
             int   brightness[2])
{
  puts("%!PS-Adobe-3.0");

 /*
  * Gamma correct the output...
  */

  if (gammaval[0] == 0.0)
    gammaval[0] = LutDefaultGamma();

  if (PrintColor)
  {
    if (gammaval[1] == 0.0)
      gammaval[1] = LutDefaultGamma();

    if (gammaval[0] != 1.0 ||
	gammaval[1] != 1.0 ||
	brightness[0] != 100 ||
	brightness[1] != 100)
    {
      printf("userdict begin\n"
             "{ neg 1 add %f exp %f mul neg 1 add } bind dup dup "
             "{ neg 1 add %f exp %f mul neg 1 add } bind\n"
             "setcolortransfer\n"
             "/setcolortransfer { pop pop pop pop } def\n"
             "end\n",
             gammaval[1], 100.0 / brightness[1],
             gammaval[0], 100.0 / brightness[0]);
    };
  }
  else if (gammaval[0] != 1.0 ||
	   brightness[0] != 100)
    printf("userdict begin\n"
           "{ neg 1 add %f exp %f mul neg 1 add } bind "
           "settransfer\n"
           "/settransfer { pop } def\n"
           "end\n",
           gammaval[0], 100.0 / brightness[0]);
}


/*
 * 'print_file()' - Print a file...
 */

void
print_file(char  *filename,
           float gammaval[2],
           int   brightness[2],
           int   nup)
{
  FILE	*fp;
  int	number,
	endpage,
	dir,
	x, y;
  float	w, l,
	tx, ty;
  long	end;


  if ((fp = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "psfilter: Unable to open file \'%s\' for reading - %s\n",
            filename, strerror(errno));
    exit(1);
  };

  scan_file(fp);

  print_header(gammaval, brightness);

  if (PrintReversed)
  {
    number  = PrintNumPages;
    dir     = -1;
    endpage = 0;
  }
  else
  {
    number  = 1;
    dir     = 1;
    endpage = PrintNumPages + 1;
  };

  switch (nup)
  {
    case 1 :
        copy_bytes(fp, PrintPages[0]);

        for (; number != endpage; number += dir)
        {
          if (PrintFlip)
            printf("gsave\n"
                   "%d 0 translate\n"
                   "-1 1 scale\n",
                   PrintWidth);

          print_page(fp, number);

          if (PrintFlip)
            puts("grestore\n");
        };
        break;

    case 2 :
        l = (float)PrintWidth;
        w = l * (float)PrintWidth / (float)PrintLength;
        if (w > ((float)PrintLength * 0.5))
        {
          w = (float)PrintLength * 0.5;
          l = w * (float)PrintLength / (float)PrintWidth;
        };

        tx = (float)PrintLength * 0.5 - w;
        ty = ((float)PrintWidth - l) * 0.5;

        puts("userdict begin\n"
             "/ESPshowpage /showpage load def\n"
             "/showpage { } def\n"
             "end");

        copy_bytes(fp, PrintPages[0]);

        for (x = 0; number != endpage;)
        {
          puts("gsave");
          printf("%d 0.0 translate\n"
                 "90 rotate\n",
                 PrintWidth);
          printf("%f %f translate\n"
                 "%f %f scale\n",
                 tx + w * x, ty,
                 w / (float)PrintWidth, l / (float)PrintLength);
          printf("newpath\n"
                 "0 0 moveto\n"
                 "%d 0 lineto\n"
                 "%d %d lineto\n"
                 "0 %d lineto\n"
                 "closepath clip newpath\n",
                 PrintWidth, PrintWidth, PrintLength, PrintLength);
          if (PrintFlip)
            printf("%d 0 translate\n"
                   "-1 1 scale\n",
                   PrintWidth);

          if (print_page(fp, number))
          {
            number += dir;
            x = 1 - x;
          };

          puts("grestore");

          if (x == 0)
            puts("ESPshowpage");
        };

        if (x)
          puts("ESPshowpage");
        break;

    case 4 :
        puts("userdict begin\n"
             "/ESPshowpage /showpage load def\n"
             "/showpage { } def\n"
             "end");


        w = (float)PrintWidth * 0.5;
        l = (float)PrintLength * 0.5;

        copy_bytes(fp, PrintPages[0]);

        for (x = 0, y = 1; number != endpage;)
        {
          printf("gsave\n"
                 "%f %f translate\n"
                 "0.5 0.5 scale\n",
                 (float)x * w, (float)y * l);
          printf("newpath\n"
                 "0 0 moveto\n"
                 "%d 0 lineto\n"
                 "%d %d lineto\n"
                 "0 %d lineto\n"
                 "closepath clip newpath\n",
                 PrintWidth, PrintWidth, PrintLength, PrintLength);
          if (PrintFlip)
            printf("%d 0 translate\n"
                   "-1 1 scale\n",
                   PrintWidth);

          if (print_page(fp, number))
          {
            number += dir;
            x = 1 - x;
          };

          puts("grestore");

          if (x == 0)
          {
            y = 1 - y;
            if (y == 1)
              puts("ESPshowpage");
          };
        };

        if (y != 1 || x != 1)
          puts("ESPshowpage");
        break;
  };

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
  int			i, n, nfiles;
  char			*opt;
  char			tempfile[255];
  FILE			*temp;
  char			buffer[8192];
  float			gammaval[2];
  int			brightness[2];
  int			nup;
  PDInfoStruct		*info;
  PDSizeTableStruct	*size;
  time_t		modtime;


  gammaval[0]   = 0.0;
  gammaval[1]   = 0.0;
  brightness[0] = 100;
  brightness[1] = 100;

  nup = 1;

  for (i = 1, nfiles = 0; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
        {
          default :
          case 'h' : /* Help */
              usage();
              break;

          case 'P' : /* Printer */
              i ++;
              if (i >= argc)
                usage();

              PDLocalReadInfo(argv[i], &info, &modtime);
              size = PDFindPageSize(info, PD_SIZE_CURRENT);

              PrintColor  = strncasecmp(info->printer_class, "Color", 5) == 0;
              PrintWidth  = 72.0 * size->width;
              PrintLength = 72.0 * size->length;
              break;

          case '1' : /* 1-up printing... */
              nup = 1;
              break;
          case '2' : /* 2-up printing... */
              nup = 2;
              break;
          case '4' : /* 4-up printing... */
              nup = 4;
              break;

          case 'f' : /* Flip pages */
              PrintFlip = 1;
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

          case 'g' :	/* Gamma correction */
	      i ++;
	      if (i < argc)
	        if (sscanf(argv[i], "%f,%f", gammaval + 0, gammaval + 1) == 1)
	          gammaval[1] = gammaval[0];
	      break;

          case 'b' :	/* Brightness */
	      i ++;
	      if (i < argc)
	        if (sscanf(argv[i], "%d,%d", brightness + 0, brightness + 1) == 1)
	          brightness[1] = brightness[0];
	      break;
        }
    else
    {
      if (nup == 1 && PrintEvenPages && PrintOddPages && PrintRange == NULL &&
	  !PrintReversed)
      {
       /*
	* Just cat the file to stdout - we don't need to to any processing.
	*/

        print_header(gammaval, brightness);

        if ((temp = fopen(argv[i], "r")) != NULL)
        {
          while ((n = fread(buffer, 1, sizeof(buffer), temp)) > 0)
            fwrite(buffer, 1, n, stdout);

          fclose(temp);
        };
      }
      else
      {
       /*
        * Filter the file as necessary...
        */

        print_file(argv[i], gammaval, brightness, nup);
      };

      nfiles ++;
    };

  if (nfiles == 0)
  {
    if (nup == 1 && PrintEvenPages && PrintOddPages && PrintRange == NULL &&
	!PrintReversed)
    {
     /*
      * Just cat stdin to stdout - we don't need to to any processing.
      */

      print_header(gammaval, brightness);

      while ((n = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
        fwrite(buffer, 1, n, stdout);
    }
    else
    {
     /*
      * Copy stdin to a temporary file and filter the temporary file.
      */

      if ((temp = fopen(tmpnam(tempfile), "w")) == NULL)
	exit(ERR_DATA_BUFFER);

      while (fgets(buffer, sizeof(buffer), stdin) != NULL)
	fputs(buffer, temp);
      fclose(temp);

      print_file(tempfile, gammaval, brightness, nup);

      unlink(tempfile);
    };
  };
}


/*
 * End of "$Id: pstops.c,v 1.3 1997/06/19 20:05:05 mike Exp $".
 */
