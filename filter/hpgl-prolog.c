/*
 * "$Id: hpgl-prolog.c,v 1.7 1999/03/06 18:02:26 mike Exp $"
 *
 *   PostScript prolog routines for the HPGL2PS program for espPrint, a
 *   collection of printer drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law.  They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: hpgl-prolog.c,v $
 *   Revision 1.7  1999/03/06 18:02:26  mike
 *   Updated for CVS check-in.
 *
 *   Revision 1.6  1998/09/16  14:37:29  mike
 *   Fixed landscape printing bug.
 *   Fixed margins when page is rotated.
 *
 *   Revision 1.6  1998/09/16  14:37:29  mike
 *   Fixed landscape printing bug.
 *   Fixed margins when page is rotated.
 *
 *   Revision 1.5  1998/08/31  20:35:49  mike
 *   Updated pen width code to automatically adjust scaling as needed.
 *   Updated PS code to adjust width/height by a factor of 0.75 for better
 *   scaling of plots.
 *
 *   Revision 1.4  1998/03/17  21:43:10  mike
 *   Fixed grayscale mode - had red & blue reversed...
 *
 *   Revision 1.3  1998/03/10  16:49:58  mike
 *   Changed cftime call to strftime for portability.
 *   Added return values to OutputProlog and OutputTrailer.
 *
 *   Revision 1.2  1996/10/14  16:50:14  mike
 *   Updated for 3.2 release.
 *   Added 'blackplot', grayscale, and default pen width options.
 *   Added encoded polyline support.
 *   Added fit-to-page code.
 *   Added pen color palette support.
 *
 *   Revision 1.1  1996/08/24  19:41:24  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"

#define PROLOG_FILE	BASEDIR "/print/data/HPGLprolog"


int
OutputProlog(int   shading,
             float penwidth)
{
  FILE		*prolog;
  char		line[255];
  time_t	curtime;
  struct tm	*curtm;


  curtime = time(NULL);
  curtm   = localtime(&curtime);

  fputs("%!PS-Adobe-3.0\n", OutputFile);
  fputs("%%Creator: hpgl2ps (ESP Print 3.2)\n", OutputFile);
  strftime(line, sizeof(line), "%%%%CreationDate: %c\n", curtm);
  fputs(line, OutputFile);
  fputs("%%LanguageLevel: 1\n", OutputFile);
  fputs("%%Pages: (atend)\n", OutputFile);
  fprintf(OutputFile, "%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
          PageLeft, PageBottom, PageWidth + PageLeft, PageHeight + PageBottom);
  fputs("%%DocumentData: Clean7Bit\n", OutputFile);
  fputs("%%EndComments\n", OutputFile);
  fputs("%%BeginProlog\n", OutputFile);
  fprintf(OutputFile, "/DefaultPenWidth %.2f def\n", penwidth * 72.0 / 25.4);
  fputs("/PenScaling 1.0 def\n", OutputFile);
  switch (shading)
  {
    case -1 : /* Black only */
        fputs("/setrgbcolor { pop pop pop } bind def\n", OutputFile);
        break;
    case 0 : /* Greyscale */
        fputs("/setrgbcolor { 0.08 mul exch 0.61 mul add exch 0.31 mul add setgray } bind def\n",
              OutputFile);
        break;
  };

  if ((prolog = fopen(PROLOG_FILE, "r")) == NULL)
  {
    fputs("hpgl2ps: Unable to open HPGL prolog \'" PROLOG_FILE "\' for reading!\n", stderr);
    return (-1);
  };

  while (fgets(line, sizeof(line), prolog) != NULL)
    fputs(line, OutputFile);

  fclose(prolog);

  fputs("%%EndProlog\n", OutputFile);
  fputs("%%Page: 1\n", OutputFile);
  fputs("gsave\n", OutputFile);

  switch (PageRotation)
  {
    case 0 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageLeft, PageBottom);
	break;
    case 90 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageBottom, PageRight);
	break;
    case 180 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageRight, PageTop);
	break;
    case 270 :
        fprintf(OutputFile, "%.1f %.1f translate\n", PageTop, PageLeft);
	break;
  };

  IN_initialize(0, NULL);

  return (0);
}


int
OutputTrailer(void)
{
  fputs("grestore\n", OutputFile);
  if (PageDirty)
    fputs("showpage\n", OutputFile);
  else
    PageCount --;
  fputs("%%EndPage\n", OutputFile);

  fputs("%%BeginTrailer\n", OutputFile);
  fprintf(OutputFile, "%%%%Pages: %d\n", PageCount);
  fputs("%%EndTrailer\n", OutputFile);

  fputs("%%EOF\n", OutputFile);

  return (0);
}


/*
 * End of "$Id: hpgl-prolog.c,v 1.7 1999/03/06 18:02:26 mike Exp $".
 */
