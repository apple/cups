/*
 * "$Id: hpgl-prolog.c,v 1.1 1996/08/24 19:41:24 mike Exp $"
 *
 *   PostScript prolog routines for the HPGL2PS program for espPrint, a
 *   collection of printer/image software.
 *
 *   Copyright (c) 1993-1995 by Easy Software Products
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
 *   $Log: hpgl-prolog.c,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include "hpgl2ps.h"

#define PROLOG_FILE	"/usr/lib/print/data/HPGLprolog"
/*#define PROLOG_FILE	"HPGLprolog" */


int
OutputProlog(PDInfoStruct *info)
{
  FILE			*prolog;
  char			line[255];
  time_t		curtime;
  PDSizeTableStruct	*pagesize;


  curtime  = time(NULL);
  pagesize = PDFindPageSize(info, PD_SIZE_CURRENT);

  fputs("%!PS-Adobe-3.0\n", OutputFile);
  fputs("%%Creator: hpgl2ps (ESP Print 2.2)\n", OutputFile);
  cftime(line, "%%%%CreationDate: %c\n", &curtime);
  fputs(line, OutputFile);
  fputs("%%LanguageLevel: 2\n", OutputFile);
  fputs("%%Pages: (atend)\n", OutputFile);
  fprintf(OutputFile, "%%%%BoundingBox: %.0f %.0f %.0f %.0f\n",
          pagesize->left_margin * 72.0,
          pagesize->top_margin * 72.0,
          (pagesize->width - pagesize->left_margin) * 72.0,
          (pagesize->length - pagesize->top_margin) * 72.0);
  fputs("%%DocumentData: Clean7Bit\n", OutputFile);
  fputs("%%EndComments\n", OutputFile);

  PageWidth  = (pagesize->width - pagesize->left_margin * 2.0) * 72.0;
  PageHeight = (pagesize->length - pagesize->top_margin * 2.0) * 72.0;
  PageLeft   = pagesize->left_margin * 72.0;
  PageBottom = pagesize->top_margin * 72.0;

  if ((prolog = fopen(PROLOG_FILE, "r")) == NULL)
  {
    fputs("hpgl2ps: Unable to open HPGL prolog \'" PROLOG_FILE "\' for reading!\n", stderr);
    return (-1);
  };

  while (fgets(line, sizeof(line), prolog) != NULL)
    fputs(line, OutputFile);

  fclose(prolog);

  fputs("%%Page: 1\n", OutputFile);
  fputs("gsave\n", OutputFile);
  fprintf(OutputFile, "%.1f %.1f translate\n", PageLeft, PageBottom);

  IN_initialize(0, NULL);
}


int
OutputTrailer(PDInfoStruct *info)
{
  fputs("grestore\n", OutputFile);
  fputs("showpage\n", OutputFile);
  fputs("%%EndPage\n", OutputFile);

  fputs("%%BeginTrailer\n", OutputFile);
  fprintf(OutputFile, "%%%%Pages: %d\n", PageCount);
  fputs("%%EndTrailer\n", OutputFile);

  fputs("%%EOF\n", OutputFile);
}


/*
 * End of "$Id: hpgl-prolog.c,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */
