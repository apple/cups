/*
 * "$Id: hpgl-prolog.c,v 1.14 1999/05/11 19:46:18 mike Exp $"
 *
 *   HP-GL/2 prolog routines for for the Common UNIX Printing System (CUPS).
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
 *  OutputProlog()  - Output the PostScript prolog...
 *  OutputTrailer() - Output the PostScript trailer...
 *  Outputf()       - Write a formatted string to the output file, creating the
 *                    page header as needed...
 */

/*
 * Include necessary headers...
 */

#include "hpgltops.h"
#include <stdarg.h>


/*
 * 'OutputProlog()' - Output the PostScript prolog...
 */

void
OutputProlog(char  *title,	/* I - Job title */
             char  *user,	/* I - Username */
             int   shading,	/* I - Type of shading */
             float penwidth)	/* I - Default pen width */
{
  FILE		*prolog;	/* Prolog file */
  char		line[255];	/* Line from prolog file */
  time_t	curtime;	/* Current time */
  struct tm	*curtm;		/* Current date */


  curtime = time(NULL);
  curtm   = localtime(&curtime);

  puts("%!PS-Adobe-3.0");
  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
         PageLeft, PageBottom, PageRight, PageTop);
  puts("%%Pages: (atend)");
  printf("%%%%LanguageLevel: %d\n", LanguageLevel);
  puts("%%DocumentData: Clean7Bit");
  puts("%%DocumentSuppliedResources: procset hpgltops 1.0 0");
  puts("%%DocumentNeededResources: font Courier Helvetica");
  puts("%%Creator: hpgltops/" CUPS_SVERSION);
  strftime(line, sizeof(line), "%%%%CreationDate: %c", curtm);
  puts(line);
  printf("%%%%Title: %s\n", title);
  printf("%%%%For: %s\n", user);
  if (Orientation & 1)
    puts("%%Orientation: Landscape");
  puts("%%EndComments");
  puts("%%BeginProlog");
  printf("/DefaultPenWidth %.2f def\n", penwidth * 72.0 / 25.4);
  puts("3.0 setmiterlimit");
  if (!shading)			/* Black only */
    puts("/setrgbcolor { pop pop pop } bind def");
  else if (!ColorDevice)	/* Greyscale */
    puts("/setrgbcolor { 0.08 mul exch 0.61 mul add exch 0.31 mul add setgray } bind def\n");

  if ((prolog = fopen(CUPS_DATADIR "/HPGLprolog", "r")) == NULL)
  {
    perror("ERROR: Unable to open HPGL prolog \"" CUPS_DATADIR "/HPGLprolog\" for reading");
    exit(1);
  }

  while (fgets(line, sizeof(line), prolog) != NULL)
    fputs(line, stdout);

  fclose(prolog);

  puts("%%EndProlog");

  IN_initialize(0, NULL);
}


/*
 * 'OutputTrailer()' - Output the PostScript trailer...
 */

void
OutputTrailer(void)
{
  if (PageDirty)
    PG_advance_page(0, NULL);

  puts("%%BeginTrailer");
  printf("%%%%Pages: %d\n", PageCount);
  puts("%%EOF");
}


/*
 * 'Outputf()' - Write a formatted string to the output file, creating the
 *               page header as needed...
 */

int				/* O - Number of bytes written */
Outputf(const char *format,	/* I - Printf-style string */
        ...)			/* I - Additional args as needed */
{
  va_list	ap;		/* Argument pointer */
  int		bytes;		/* Number of bytes written */


 /*
  * Write the page header as needed...
  */

  if (!PageDirty)
  {
    PageDirty = 1;
    PageCount ++;

    printf("%%%%Page: %d %d\n", PageCount, PageCount);
    printf("/PenScaling %.3f def\n", PenScaling);
    puts("gsave");

    if (Duplex && (PageCount & 1) == 0)
      switch ((PageRotation / 90) & 3)
      {
	case 0 :
            printf("%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
	    break;
	case 1 :
            printf("%.1f %.1f translate\n", PageLength - PageTop,
	           PageWidth - PageRight);
	    break;
	case 2 :
            printf("%.1f %.1f translate\n", PageLeft, PageLength - PageTop);
	    break;
	case 3 :
            printf("%.1f %.1f translate\n", PageBottom, PageLeft);
	    break;
      }
    else
      switch ((PageRotation / 90) & 3)
      {
	case 0 :
            printf("%.1f %.1f translate\n", PageLeft, PageBottom);
	    break;
	case 1 :
            printf("%.1f %.1f translate\n", PageBottom, PageWidth - PageRight);
	    break;
	case 2 :
            printf("%.1f %.1f translate\n", PageWidth - PageRight,
	            PageLength - PageTop);
	    break;
	case 3 :
            printf("%.1f %.1f translate\n", PageLength - PageTop, PageLeft);
	    break;
      }
  }

 /*
  * Write the string to the output file...
  */

  va_start(ap, format);
  bytes = vprintf(format, ap);
  va_end(ap);

  return (bytes);
}


/*
 * End of "$Id: hpgl-prolog.c,v 1.14 1999/05/11 19:46:18 mike Exp $".
 */
