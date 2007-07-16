/*
 * "$Id: hpgl-prolog.c 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 prolog routines for for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
             int   shading)	/* I - Type of shading */
{
  FILE		*prolog;	/* Prolog file */
  char		line[255];	/* Line from prolog file */
  const char	*datadir;	/* CUPS_DATADIR environment variable */
  char		filename[1024];	/* Name of prolog file */
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
  puts("%%DocumentSuppliedResources: procset hpgltops 1.1 0");
  puts("%%DocumentNeededResources: font Courier Helvetica");
  puts("%%Creator: hpgltops/" CUPS_SVERSION);
  strftime(line, sizeof(line), "%c", curtm);
  printf("%%%%CreationDate: %s\n", line);
  WriteTextComment("Title", title);
  WriteTextComment("For", user);
  printf("%%cupsRotation: %d\n", (Orientation & 3) * 90);
  puts("%%EndComments");
  puts("%%BeginProlog");
  printf("/DefaultPenWidth %.2f def\n", PenWidth * 72.0 / 25.4);
  if (!shading)			/* Black only */
    puts("/setrgbcolor { pop pop pop } bind def");
  else if (!ColorDevice)	/* Greyscale */
    puts("/setrgbcolor { 0.08 mul exch 0.61 mul add exch 0.31 mul add setgray } bind def\n");

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  snprintf(filename, sizeof(filename), "%s/data/HPGLprolog", datadir);

  if ((prolog = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr,
            "DEBUG: Unable to open HPGL prolog \"%s\" for reading - %s\n",
            filename, strerror(errno));
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

  puts("%%Trailer");
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
  float		iw1[2], iw2[2];	/* Clipping window */
  int		i;		/* Looping var */
  ppd_size_t	*size;		/* Page size */
  ppd_option_t	*option;	/* Page size option */
  ppd_choice_t	*choice;	/* Page size choice */
  float		width, length;	/* Page dimensions */
  int		landscape;	/* Rotate for landscape orientation? */


 /*
  * Write the page header as needed...
  */

  if (!PageDirty)
  {
    PageDirty = 1;
    PageCount ++;

    printf("%%%%Page: %d %d\n", PageCount, PageCount);

    landscape = 0;

    if (!FitPlot && PlotSizeSet)
    {
     /*
      * Set the page size for this page...
      */

      if (PageRotation == 0 || PageRotation == 180)
      {
	width  = PlotSize[0];
	length = PlotSize[1];
      }
      else
      {
	width  = PlotSize[1];
	length = PlotSize[0];
      }

      fprintf(stderr, "DEBUG: hpgltops setting page size (%.0f x %.0f)\n",
              width, length);

      if (PPD != NULL)
      {
        fputs("DEBUG: hpgltops has a PPD file!\n", stderr);

       /*
	* Lookup the closest PageSize and set it...
	*/

	for (i = PPD->num_sizes, size = PPD->sizes; i > 0; i --, size ++)
	  if ((fabs(length - size->length) < 36.0 && size->width >= width) ||
              (fabs(length - size->width) < 36.0 && size->length >= width))
	    break;

	if (i == 0 && PPD->variable_sizes)
	{
          for (i = PPD->num_sizes, size = PPD->sizes; i > 0; i --, size ++)
	    if (strcasecmp(size->name, "custom") == 0)
	      break;
	} 

	if (i > 0)
	{
	 /*
	  * Found a matching size...
	  */

	  option = ppdFindOption(PPD, "PageSize");
	  choice = ppdFindChoice(option, size->name);

          puts("%%BeginPageSetup");
          printf("%%%%BeginFeature: PageSize %s\n", size->name);

          if (strcasecmp(size->name, "custom") == 0)
	  {
	    PageLeft   = PPD->custom_margins[0];
	    PageRight  = width - PPD->custom_margins[2];
	    PageWidth  = width;
	    PageBottom = PPD->custom_margins[1];
	    PageTop    = length - PPD->custom_margins[3];
	    PageLength = length;

            printf("%.0f %.0f 0 0 0\n", width, length);

	    if (choice->code == NULL)
	    {
	     /*
	      * This can happen with certain buggy PPD files that don't include
	      * a CustomPageSize command sequence...  We just use a generic
	      * Level 2 command sequence...
	      */

	      puts("pop pop pop");
	      puts("<</PageSize[5 -2 roll]/ImagingBBox null>>setpagedevice\n");
	    }
            else
	    {
	     /*
	      * Use the vendor-supplied command...
	      */

	      printf("%s\n", choice->code);
	    }
	  }
	  else
	  {
	    if (choice->code)
              printf("%s\n", choice->code);

	    if (fabs(length - size->width) < 36.0)
	    {
	     /*
              * Do landscape orientation...
	      */

	      PageLeft   = size->bottom;
	      PageRight  = size->top;
	      PageWidth  = size->length;
	      PageBottom = size->left;
	      PageTop    = size->right;
	      PageLength = size->width;

              landscape = 1;
	    }
	    else
	    {
	     /*
              * Do portrait orientation...
	      */

	      PageLeft   = size->left;
	      PageRight  = size->right;
	      PageWidth  = size->width;
	      PageBottom = size->bottom;
	      PageTop    = size->top;
	      PageLength = size->length;
	    }
	  }

	  puts("%%EndFeature");
	  puts("%%EndPageSetup");
	}
      }
      else
      {
        fputs("DEBUG: hpgltops does not have a PPD file!\n", stderr);

        puts("%%BeginPageSetup");
        printf("%%%%BeginFeature: PageSize w%.0fh%.0f\n", width, length);
	printf("<</PageSize[%.0f %.0f]/ImageBBox null>>setpagedevice\n",
	       width, length);
	puts("%%EndFeature");
	puts("%%EndPageSetup");

	PageLeft   = 0.0;
	PageRight  = width;
	PageWidth  = width;
	PageBottom = 0.0;
	PageTop    = length;
	PageLength = length;
      }
    }

    define_font(0);
    define_font(1);

    printf("%.1f setmiterlimit\n", MiterLimit);
    printf("%d setlinecap\n", LineCap);
    printf("%d setlinejoin\n", LineJoin);

    printf("%.3f %.3f %.3f %.2f SP\n", Pens[1].rgb[0], Pens[1].rgb[1],
           Pens[1].rgb[2], Pens[1].width * PenScaling);

    puts("gsave");

    if (Duplex && (PageCount & 1) == 0)
      switch ((PageRotation / 90 + landscape) & 3)
      {
	case 0 :
            printf("%.1f %.1f translate\n", PageWidth - PageRight, PageBottom);
	    break;
	case 1 :
            printf("%.0f 0 translate 90 rotate\n", PageLength);
            printf("%.1f %.1f translate\n", PageLength - PageTop,
	           PageWidth - PageRight);
	    break;
	case 2 :
            printf("%.0f %.0f translate 180 rotate\n", PageWidth, PageLength);
            printf("%.1f %.1f translate\n", PageLeft, PageLength - PageTop);
	    break;
	case 3 :
            printf("0 %.0f translate -90 rotate\n", PageWidth);
            printf("%.1f %.1f translate\n", PageBottom, PageLeft);
	    break;
      }
    else
      switch ((PageRotation / 90 + landscape) & 3)
      {
	case 0 :
            printf("%.1f %.1f translate\n", PageLeft, PageBottom);
	    break;
	case 1 :
            printf("%.0f 0 translate 90 rotate\n", PageLength);
            printf("%.1f %.1f translate\n", PageBottom, PageWidth - PageRight);
	    break;
	case 2 :
            printf("%.0f %.0f translate 180 rotate\n", PageWidth, PageLength);
            printf("%.1f %.1f translate\n", PageWidth - PageRight,
	            PageLength - PageTop);
	    break;
	case 3 :
            printf("0 %.0f translate -90 rotate\n", PageWidth);
            printf("%.1f %.1f translate\n", PageLength - PageTop, PageLeft);
	    break;
      }

    if (IW1[0] != IW2[0] && IW1[1] != IW2[1])
    {
      iw1[0] = IW1[0] * 72.0f / 1016.0f;
      iw1[1] = IW1[1] * 72.0f / 1016.0f;
      iw2[0] = IW2[0] * 72.0f / 1016.0f;
      iw2[1] = IW2[1] * 72.0f / 1016.0f;

      printf("initclip MP %.3f %.3f MO %.3f %.3f LI %.3f %.3f LI %.3f %.3f LI CP clip\n",
	     iw1[0], iw1[1], iw1[0], iw2[1], iw2[0], iw2[1], iw2[0], iw1[1]);
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
 * End of "$Id: hpgl-prolog.c 6649 2007-07-11 21:46:42Z mike $".
 */
