/*
 * "$Id: texttops.c,v 1.5 1998/08/14 19:07:45 mike Exp $"
 *
 *   PostScript text output filter for espPrint, a collection of printer
 *   drivers.
 *
 *   Copyright 1993-1998 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs contain
 *   unpublished proprietary information of Easy Software Products, and
 *   are protected by Federal copyright law. They may not be disclosed
 *   to third parties or copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Contents:
 *
 * Revision History:
 *
 *   $Log: texttops.c,v $
 *   Revision 1.5  1998/08/14 19:07:45  mike
 *   Fixed bug in multi-column output - second (& third) column were being
 *   positioned off the page.
 *
 *   Revision 1.4  1998/08/10  17:14:06  mike
 *   Added wrap/nowrap option.
 *
 *   Revision 1.3  1998/07/28  17:42:01  mike
 *   Updated the page count at the end of the file - off by one...
 *
 *   Revision 1.2  1996/10/14  16:28:08  mike
 *   Updated for 3.2 release.
 *   Added width, length, left, right, top, and bottom margin options.
 *   Revamped Setup() code for new options.
 *
 *   Revision 1.1  1996/10/14  16:07:57  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <pod.h>
#include <errorcodes.h>
#include <license.h>


/*
 * Constants...
 */

#ifndef FALSE
#  define FALSE	0
#  define TRUE	(!FALSE)
#endif /* !FALSE */

#define MAX_COLUMNS	256
#define MAX_LINES	256

#define ATTR_BOLD	0x01
#define ATTR_UNDERLINE	0x02


/*
 * Character/attribute structure...
 */

typedef struct
{
  char ch,		/* ASCII character */
       attr;		/* Any attributes */
} lchar_t;


/*
 * Globals...
 */

int	Verbosity = 0,
	WrapLines = 1;
int	SizeLines = 60,
	SizeColumns = 80,
	PageColumns = 1;

float	CharsPerInch = 10.0;
float	LinesPerInch = 6.0;


/*
 * 'Setup()' - Output a PostScript prolog for this page.
 */

void
Setup(FILE  *out,
      float width,
      float length,
      float left,
      float right,
      float bottom,
      float top,
      char  *fontname,
      float fontsize,
      int   landscape)
{
  float	temp;


  CharsPerInch = 120.0 / fontsize;

  if (CharsPerInch > 12)
    LinesPerInch = 8;
  else if (CharsPerInch < 10)
    LinesPerInch = 4;
  else
    LinesPerInch = 6;
  
  if (landscape)
  {
    SizeColumns = ((length - bottom - top) / 72.0 - (PageColumns - 1) * 0.25) *
                  CharsPerInch;
    SizeLines   = (width - left - right) / 72.0 * LinesPerInch;

    temp   = width;
    width  = length;
    length = temp;

    temp   = left;
    left   = bottom;
    bottom = temp;

    temp   = right;
    right  = top;
    top    = temp;
  }
  else
  {
    SizeColumns = ((width - left - right) / 72.0 - (PageColumns - 1) * 0.25) *
                  CharsPerInch;
    SizeLines   = (length - bottom - top) / 72.0 * LinesPerInch;
  };

  SizeColumns /= PageColumns;

  fputs("%!PS-Adobe-3.0\n", out);
  fprintf(out, "%%%%BoundingBox: %f %f %f %f\n",
          left, bottom, width - right, length - top);
  fputs("%%LanguageLevel: 1\n", out);
  fputs("%%Creator: text2ps 3.2 Copyright 1993-1996 Easy Software Products\n", out);
  fprintf(out, "%%%%Pages: (atend)\n");
  fputs("%%EndComments\n\n", out);

  fputs("%%BeginProlog\n", out);

 /*
  * Oh, joy, another font encoding.  For now assume that all documents use
  * ISO-8859-1 encoding (this covers about 1/2 of the human population)...
  */

  fputs("/iso8859encoding [\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/.notdef /.notdef /.notdef /.notdef\n", out);
  fputs("/space /exclam /quotedbl /numbersign\n", out);
  fputs("/dollar /percent /ampersand /quoteright\n", out);
  fputs("/parenleft /parenright /asterisk /plus\n", out);
  fputs("/comma /minus /period /slash\n", out);
  fputs("/zero /one /two /three /four /five /six /seven\n", out);
  fputs("/eight /nine /colon /semicolon\n", out);
  fputs("/less /equal /greater /question\n", out);
  fputs("/at /A /B /C /D /E /F /G\n", out);
  fputs("/H /I /J /K /L /M /N /O\n", out);
  fputs("/P /Q /R /S /T /U /V /W\n", out);
  fputs("/X /Y /Z /bracketleft\n", out);
  fputs("/backslash /bracketright /asciicircum /underscore\n", out);
  fputs("/quoteleft /a /b /c /d /e /f /g\n", out);
  fputs("/h /i /j /k /l /m /n /o\n", out);
  fputs("/p /q /r /s /t /u /v /w\n", out);
  fputs("/x /y /z /braceleft\n", out);
  fputs("/bar /braceright /asciitilde /guilsinglright\n", out);
  fputs("/fraction /florin /quotesingle /quotedblleft\n", out);
  fputs("/guilsinglleft /fi /fl /endash\n", out);
  fputs("/dagger /daggerdbl /bullet /quotesinglbase\n", out);
  fputs("/quotedblbase /quotedblright /ellipsis /trademark\n", out);
  fputs("/dotlessi /grave /acute /circumflex\n", out);
  fputs("/tilde /macron /breve /dotaccent\n", out);
  fputs("/dieresis /perthousand /ring /cedilla\n", out);
  fputs("/Ydieresis /hungarumlaut /ogonek /caron\n", out);
  fputs("/emdash /exclamdown /cent /sterling\n", out);
  fputs("/currency /yen /brokenbar /section\n", out);
  fputs("/dieresis /copyright /ordfeminine /guillemotleft\n", out);
  fputs("/logicalnot /hyphen /registered /macron\n", out);
  fputs("/degree /plusminus /twosuperior /threesuperior\n", out);
  fputs("/acute /mu /paragraph /periodcentered\n", out);
  fputs("/cedilla /onesuperior /ordmasculine /guillemotright\n", out);
  fputs("/onequarter /onehalf /threequarters /questiondown\n", out);
  fputs("/Agrave /Aacute /Acircumflex /Atilde\n", out);
  fputs("/Adieresis /Aring /AE /Ccedilla\n", out);
  fputs("/Egrave /Eacute /Ecircumflex /Edieresis\n", out);
  fputs("/Igrave /Iacute /Icircumflex /Idieresis\n", out);
  fputs("/Eth /Ntilde /Ograve /Oacute\n", out);
  fputs("/Ocircumflex /Otilde /Odieresis /multiply\n", out);
  fputs("/Oslash /Ugrave /Uacute /Ucircumflex\n", out);
  fputs("/Udieresis /Yacute /Thorn /germandbls\n", out);
  fputs("/agrave /aacute /acircumflex /atilde\n", out);
  fputs("/adieresis /aring /ae /ccedilla\n", out);
  fputs("/egrave /eacute /ecircumflex /edieresis\n", out);
  fputs("/igrave /iacute /icircumflex /idieresis\n", out);
  fputs("/eth /ntilde /ograve /oacute\n", out);
  fputs("/ocircumflex /otilde /odieresis /divide\n", out);
  fputs("/oslash /ugrave /uacute /ucircumflex\n", out);
  fputs("/udieresis /yacute /thorn /ydieresis ] def\n", out);

  fprintf(out, "/%s findfont\n", fontname);
  fputs("dup length dict begin\n"
        "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
        "	/Encoding iso8859encoding def\n"
        "	currentdict\n"
        "end\n", out);
  fprintf(out, "/%s exch definefont pop\n", fontname);

  fprintf(out, "/R /%s findfont %f scalefont def\n", fontname, fontsize);

  if (strcasecmp(fontname, "Times-Roman") == 0)
    fputs("/Times-Bold findfont\n", out);
  else
    fprintf(out, "/%s findfont\n", fontname);
  fputs("dup length dict begin\n"
        "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
        "	/Encoding iso8859encoding def\n"
        "	currentdict\n"
        "end\n", out);
  if (strcasecmp(fontname, "Times-Roman") == 0)
    fputs("/Times-Bold exch definefont pop\n", out);
  else
    fprintf(out, "/%s exch definefont pop\n", fontname);

  if (strcasecmp(fontname, "Times-Roman") == 0)
    fprintf(out, "/B /Times-Bold findfont %f scalefont def\n", fontsize);
  else
    fprintf(out, "/B /%s-Bold findfont %f scalefont def\n", fontname, fontsize);

  fprintf(out, "/S { setfont /y exch %f mul %f sub neg def %f mul %f add exch %f mul add /x exch def "
               "x y moveto show } bind def\n",
          fontsize, length - top - fontsize, 72.0 / CharsPerInch, left,
	  left + 72.0 * SizeColumns / CharsPerInch);

  fprintf(out, "/U { setfont /y exch %f mul %f sub neg def %f mul %f add exch %f mul add /x exch def "
               "x y moveto dup show x y moveto stringwidth rlineto } bind def\n",
          fontsize, length - top - fontsize, 72.0 / CharsPerInch, left,
	  left + 72.0 * SizeColumns / CharsPerInch);

  fputs("%%EndProlog\n", out);

  if (Verbosity)
    fprintf(stderr, "text2ps: cpi = %.2f, lpi = %.2f, chars/col = %d\n"
                    "text2ps: columns = %d, lines = %d\n",
            CharsPerInch, LinesPerInch, SizeColumns, PageColumns, SizeLines);
}


void
Shutdown(FILE *out,
         int  pages)
{
  fprintf(out, "%%%%Pages: %d\n", pages - 1);
  fputs("%%EOF\n", out);
}


void
StartPage(FILE *out,
          int  page)
{
  fprintf(out, "%%%%Page: %d\n", page);
}


void
EndPage(FILE *out)
{
  fputs("showpage\n", out);
  fputs("%%EndPage\n", out);
}


void
output_line(FILE *out, int page_column, int column, int row, int attr, char *line)
{
  fprintf(out, "(%s) %d %d %d %s %s\n", line, page_column, column,
          row, (attr & ATTR_BOLD) ? "B" : "R", (attr & ATTR_UNDERLINE) ? "U" : "S");
}


void
OutputLine(FILE    *out,
           int     page_column,
           int     row,
           lchar_t *buffer)
{
  int	column,
        linecol,
	attr;
  char	line[MAX_COLUMNS * 2 + 1],
	*lineptr;


  for (column = 0, attr = 0, lineptr = line, linecol = 0;
       column < SizeColumns;
       column ++, buffer ++)
  {
    if (buffer->ch == '\0')
      break;

    if (attr ^ buffer->attr)
    {
      if (lineptr > line)
      {
        *lineptr = '\0';
        output_line(out, page_column, linecol, row, attr, line);
        lineptr = line;
      };

      attr    = buffer->attr;
      linecol = column;
    };

    if (strchr("()\\", buffer->ch) != NULL)
    {
      *lineptr++ = '\\';
      *lineptr++ = buffer->ch;
    }
    else if (buffer->ch < ' ' || buffer->ch > 126)
    {
      sprintf(lineptr, "\\%03o", buffer->ch);
      lineptr += 4;
    }
    else
      *lineptr++ = buffer->ch;
  };

  if (lineptr > line)
  {
    *lineptr = '\0';
    output_line(out, page_column, linecol, row, attr, line);
  };
}


/*
 * 'Usage()' - Print usage message and exit.
 */

void
Usage(void)
{    
  fputs("Usage: text2ps -P <printer-name> [-D]\n", stderr);
  fputs("               [-e] [-s] [-w] [-Z]\n", stderr);
  fputs("               [-L <log-file>] [-O <output-file>]\n", stderr);
  fputs("               [-M <printer-model]\n", stderr);

  exit(ERR_BAD_ARG);
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int
main(int  argc,    /* I - Number of command-line arguments */
     char *argv[]) /* I - Command-line arguments */
{
  int			i,		/* Looping var */
			ch;		/* Current char from file */
  char			*opt;		/* Current option character */
  int			empty_infile;	/* TRUE if the input file is empty */
  char			*filename;	/* Input filename, if specified (NULL otherwise). */
  FILE			*fp;		/* Input file */
  int			line,
  			column,
  			page_column,
  			page,
  			landscape;
  char			*fontname;
  float			fontsize,
			width,
			length,
			temp,
			left,
			right,
			bottom,
			top;
  PDInfoStruct		*info;		/* POD info */
  PDStatusStruct	*status;	/* POD status */
  time_t		mod_time;	/* Modification time */
  PDSizeTableStruct	*size;		/* Page size */
  char			*outfile;
  FILE			*out;
  lchar_t		buffer[MAX_COLUMNS];


 /*
  * Process any command-line args...
  */

  filename  = NULL;
  outfile   = NULL;
  fontname  = "Courier";
  fontsize  = 12.0;
  landscape = 0;
  width     = 612;
  length    = 792;
  left      = 18;
  right     = 18;
  bottom    = 36;
  top       = 36;
  
  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      for (opt = argv[i] + 1; *opt != '\0'; opt ++)
        switch (*opt)
        {
          case 'P' : /* Specify the printer name */
              i ++;
              if (i >= argc)
                Usage();

	     /*
	      * Open the POD database files and get the printer definition record.
	      */

	      if (PDLocalReadInfo(argv[i], &info, &mod_time) < 0)
	      {
		fprintf(stderr, "text2ps: Could not open required POD database files for printer \'%s\'.\n", 
        		argv[i]);
		fprintf(stderr, "text2ps: Are you sure all required POD files are properly installed?\n");

		PDPerror("text2ps");
		exit(ERR_POD_ACCESS);
	      };

	      status = info->active_status;
	      size   = PDFindPageSize(info, PD_SIZE_CURRENT);

              width  = size->width * 72.0;
              length = size->length * 72.0;

              temp = size->left_margin * 72.0;
	      if (temp > left)
	        left = temp;

              temp = 72.0 * (size->width - size->left_margin -
                             size->horizontal_addr / (float)info->horizontal_resolution);
              if (temp > right)
	        right = temp;

              temp = 72.0 * (size->length - size->top_margin -
                             size->vertical_addr / (float)info->vertical_resolution);
              if (temp > bottom)
	        bottom = temp;

              temp = size->top_margin * 72.0;
	      if (temp > top)
	        top = temp;
              break;

          case 'O' : /* Output file */
              i ++;
              if (i >= argc || outfile != NULL)
                Usage();

              outfile = argv[i];
              break;

          case 'D' : /* Produce debugging messages */
              Verbosity ++;
              break;

          case 'l' : /* Landscape output */
              landscape = TRUE;
              break;

          case 'w' : /* Line wrapping */
              i ++;
              if (i >= argc)
                Usage();

              WrapLines = atoi(argv[i]);
              break;

          case 'F' : /* Font name */
              i ++;
              if (i >= argc)
                Usage();

              fontname = argv[i];
              break;

          case 'p' : /* Font pointsize */
              i ++;
              if (i >= argc)
                Usage();

              fontsize = atof(argv[i]);
              break;

          case 'M' : /* Multiple column mode */
              i ++;
              if (i >= argc)
                Usage();

              PageColumns = atof(argv[i]);
              break;

          case 'W' : /* Width */
              i ++;
              if (i >= argc)
                Usage();

              width = atof(argv[i]) * 72.0;
              break;

          case 'H' : /* Length */
              i ++;
              if (i >= argc)
                Usage();

              length = atof(argv[i]) * 72.0;
              break;

          case 'L' : /* Left margin */
              i ++;
              if (i >= argc)
                Usage();

              left = atof(argv[i]) * 72.0;
              break;

          case 'R' : /* Right margin */
              i ++;
              if (i >= argc)
                Usage();

              right = atof(argv[i]) * 72.0;
              break;

          case 'T' : /* Top margin */
              i ++;
              if (i >= argc)
                Usage();

              top = atof(argv[i]) * 72.0;
              break;

          case 'B' : /* Bottom margin */
              i ++;
              if (i >= argc)
                Usage();

              bottom = atof(argv[i]) * 72.0;
              break;

          default :
              Usage();
              break;
        }
    else if (filename != NULL)
      Usage();
    else
      filename = argv[i];

  if (Verbosity)
  {
    fputs("text2ps: Command-line args are:", stderr);
    for (i = 1; i < argc; i ++)
      fprintf(stderr, " %s", argv[i]);
    fputs("\n", stderr);
  };

 /*
  * Setup the output file...
  */

  if (outfile == NULL)
    out = stdout;
  else
    out = fopen(outfile, "w");

  if (out == NULL)
  {
    fprintf(stderr, "text2ps: Unable to create PostScript output to %s - %s\n",
            outfile == NULL ? "(stdout)" : outfile, strerror(errno));
    exit(ERR_TRANSMISSION);
  };

  Setup(out, width, length, left, right, bottom, top, fontname, fontsize,
        landscape);

 /*
  * Read text from the specified source and print them...
  */

  if (filename != NULL)
  {
    if ((fp = fopen(filename, "r")) == NULL)
    {
      Shutdown(out, 0);
      exit(ERR_DATA_FILE);
    };
  }
  else
    fp = stdin;

  if (fp == NULL)
  {
    Shutdown(out, 0);
    exit(ERR_DATA_SHORT_FILE);
  };

  empty_infile = TRUE;
  column       = 0;
  line         = -1;
  page         = 0;
  page_column  = 0;

  memset(buffer, 0, sizeof(buffer));

  while ((ch = getc(fp)) >= 0)
  {
    empty_infile = FALSE;

    if (line < 0)
    {
      page ++;
      StartPage(out, page);
      line         = 0;
      page_column  = 0;
    };

    switch (ch)
    {
      case 0x08 :		/* BS - backspace for boldface & underline */
          if (column > 0)
            column --;
          break;
      case 0x09 :		/* HT - tab to next 8th column */
          do
          {
            if (column >= SizeColumns && WrapLines)
            {			/* Wrap text to margins */
              OutputLine(out, page_column, line, buffer);
              line ++;
              if (line >= SizeLines)
              {
                page_column ++;
                line = 0;
                if (page_column >= PageColumns)
                {
          	  EndPage(out);
        	  line = -1;
        	};
              };
              memset(buffer, 0, sizeof(buffer));
              column = 0;
            };

            if (column < SizeColumns)
              buffer[column].ch = ' ';

            column ++;
          }
          while (column & 7);
          break;
      case 0x0a :		/* LF - output current line */
          OutputLine(out, page_column, line, buffer);
          line ++;
          if (line >= SizeLines)
          {
            page_column ++;
            line = 0;
            if (page_column >= PageColumns)
            {
              EndPage(out);
              line = -1;
            };
          };
          memset(buffer, 0, sizeof(buffer));
          column = 0;
          break;
      case 0x0c :		/* FF - eject current page... */
          OutputLine(out, page_column, line, buffer);
          page_column ++;
          line = 0;
          if (page_column >= PageColumns)
          {
            EndPage(out);
            line = -1;
          };
          memset(buffer, 0, sizeof(buffer));
          column = 0;
          break;
      case 0x0d :		/* CR */
          column = 0;
          break;
      default :			/* All others... */
          if (ch < ' ')
            break;		/* Ignore other control chars */

          if (column >= SizeColumns && WrapLines)
          {			/* Wrap text to margins */
            OutputLine(out, page_column, line, buffer);
            line ++;
            if (line >= SizeLines)
            {
              page_column ++;
              line = 0;
              if (page_column >= PageColumns)
              {
        	EndPage(out);
        	line = -1;
              };
            };
            memset(buffer, 0, sizeof(buffer));
            column = 0;
          };

          if (column < SizeColumns)
	  {
            if (ch == buffer[column].ch)
              buffer[column].attr |= ATTR_BOLD;
            else if (buffer[column].ch == '_')
              buffer[column].attr |= ATTR_UNDERLINE;

            buffer[column].ch = ch;
	  };

          column ++;
          break;          
    };
  };

  if (line >= 0)
  {
    OutputLine(out, page_column, line, buffer);
    EndPage(out);
    page ++;
  };

  Shutdown(out, page);

  if (empty_infile)
    exit(ERR_DATA_SHORT_FILE);

 /*
  * Exit with no errors...
  */

  return (NO_ERROR);
}


/*
 * End of "$Id: texttops.c,v 1.5 1998/08/14 19:07:45 mike Exp $".
 */
