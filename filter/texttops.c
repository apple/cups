/*
 * "$Id: texttops.c,v 1.7 1999/03/11 20:55:05 mike Exp $"
 *
 *   Text to PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/language.h>
#include <cups/string.h>


/*
 * Constants...
 */

#define ATTR_BOLD	0x01
#define ATTR_UNDERLINE	0x02
#define ATTR_RAISED	0x04
#define ATTR_LOWERED	0x08

/*
 * Structures...
 */

typedef struct			/**** Character/attribute structure... ****/

{
  short	ch,			/* Character */
	attr;			/* Any attributes */
} lchar_t;


/*
 * Globals...
 */

int	Verbosity = 0,		/* Be verbose? */
	WrapLines = 1;		/* Wrap text in lines */
int	SizeLines = 60,		/* Number of lines on a page */
	SizeColumns = 80,	/* Number of columns on a line */
	PageColumns = 1,	/* Number of columns on a page */
	ColumnGutter = 0,	/* Number of characters between text columns */
	ColumnWidth = 80;	/* Width of each column */
lchar_t	**Page = NULL;		/* Page characters */
int	NumPages = 0;		/* Number of pages in document */
int	CharsPerInch = 10;	/* Number of character columns per inch */
int	LinesPerInch = 6;	/* Number of lines per inch */
int	Top = 36;		/* Top position in points */
	Left = 18;		/* Left position in points */


/*
 * Local functions...
 */

static int	getutf8(FILE *fp);
static int	write_epilogue(ppd_t *ppd);
static int	write_line(int row, lchar_t *line);
static int	write_page(void);
static int	write_prolog(ppd_t *ppd);
static int	write_string(int col, int row, int len, lchar_t *s);


/*
 * 'getutf8()' - Get a UTF-8 encoded wide character...
 */

static int		/* O - Character or -1 on error */
getutf8(FILE *fp)	/* I - File to read from */
{
  int	ch;		/* Current character value */
  int	next;		/* Next character from file */


 /*
  * Read the first character and process things accordingly...
  *
  * UTF-8 maps 16-bit characters to:
  *
  *        0 to 127 = 0xxxxxxx
  *     128 to 2047 = 110xxxxx 10yyyyyy (xxxxxyyyyyy)
  *   2048 to 65535 = 1110xxxx 10yyyyyy 10zzzzzz (xxxxyyyyyyzzzzzz)
  *
  * We also accept:
  *
  *      128 to 191 = 10xxxxxx
  *
  * since this range of values is otherwise undefined unless you are
  * in the middle of a multi-byte character...
  *
  * This code currently does not support anything beyond 16-bit
  * characters, in part because PostScript doesn't support more than
  * 16-bit characters...
  */

  if ((ch = getc(fp)) == EOF)
    return (EOF);

  if (ch < 0xc0)	/* One byte character? */
    return (ch);
  else if ((ch & 0xe0) == 0xc0)
  {
   /*
    * Two byte character...
    */

    if ((next = getc(fp)) == EOF)
      return (EOF)
    else
      return (((ch & 0x1f) << 6) | (next & 0x3f));
  }
  else if ((ch & 0xf0) == 0xe0)
  {
   /*
    * Three byte character...
    */

    if ((next = getc(fp)) == EOF)
      return (EOF)

    ch = ((ch & 0x0f) << 6) | (next & 0x3f);

    if ((next = getc(fp)) == EOF)
      return (EOF)
    else
      return ((ch << 6) | (next & 0x3f));
  }
  else
  {
   /*
    * More than three bytes...  We don't support that...
    */

    return (EOF);
  }
}


/*
 * 'write_epilogue()' - Write the PostScript file epilogue.
 */

static int
write_epilogue(ppd_t *ppd)
{
}


/*
 * 'write_line()' - Write a row of text.
 */

static int			/* O - 0 on success, -1 on error */
write_line(int     row,		/* I - Row number (0 to N) */
           lchar_t *line)	/* I - Line to print */
{
  int		col;		/* Current column */
  int		attr;		/* Current attribute */
  lchar_t	*start;		/* First character in sequence */

  
  for (col = 0, attr = 0, start = line;
       col < SizeColumns;
       col ++, line ++)
    if (attr != line->attr || line->ch == 0)
    {
      if (start < line)
        if (write_string(col - (line - start), row, line - start, start))
	  return (-1);

      if (line->ch == 0)
      {
	start = line + 1;
	attr  = 0;
	continue;
      }
      else
      {
        start = line;
	attr  = line->attr;
      }
    }

  if (start < line)
    if (write_string(col - (line - start), row, line - start, start))
      return (-1);

  return (0);
}


/*
 * 'write_page()' - Write a page of text.
 */

static int
write_page(void)
{
}


/*
 * 'write_prolog()' - Write the PostScript file prolog with options.
 */

static int
write_prolog(ppd_t *ppd)
{
}


/*
 * 'write_string()' - Write a string of text.
 */

static int			/* O - 0 on success, -1 on error */
write_string(int     col,	/* I - Start column */
             int     row,	/* I - Row */
             int     len,	/* I - Number of characters */
             lchar_t *s)	/* I - String to print */
{
  int	i;			/* Looping var */
  float	x, y;			/* Position of text */


 /*
  * Position the text and set the font...
  */

  x = (float)Left + (float)col * 72.0 / (float)CharsPerInch;
  y = (float)Top - (float)row * 72.0 / (float)LinesPerInch;

  if (s->attr & ATTR_RAISED)
    y += 36.0 / (float)LinesPerInch;
  else if (s->attr & ATTR_LOWERED)
    y -= 36.0 / (float)LinesPerInch;

  printf("%.1f %.1f %s", x, y, (s->attr & ATTR_BOLD) ? "B" : "N");

  if (s->attr & ATTR_UNDERLINE)
    printf(" %.1f U", (float)len * 72.0 / (float)CharsPerInch);

 /*
  * See if the string contains 16-bit characters...
  */

  for (i = 0; i < len; i ++)
    if (s[i].ch > 255)
      break;

  if (i < len)
  {
   /*
    * Write a hex Unicode string...
    */

    fputs("<feff", stdout);

    while (len > 0)
    {
      printf("%04x", s->ch);
      len --;
      s ++;
    }

    puts(">S");
  }
  else
  {
   /*
    * Write a quoted string...
    */

    putchar('(');

    while (len > 0)
    {
      if (s->ch > 126)
      {
       /*
        * Quote 8-bit characters...
	*/

        printf("\%03o", s->ch);
      }
      else
      {
       /*
        * Quote the parenthesis and backslash as needed...
	*/

        if (s->ch == '(' || s->ch == ')' || s->ch == '\\')
	  putchar('\\');

	putchar(s->ch);
      }

      len --;
      s ++;
    }

    puts(")S");
  }
}


/*
 * Control codes:
 *
 *   BS		Backspace (0x08)
 *   HT		Horizontal tab; next 8th column (0x09)
 *   LF		Line feed; forward full line (0x0a)
 *   VT		Vertical tab; reverse full line (0x0b)
 *   FF		Form feed (0x0c)
 *   CR		Carriage return (0x0d)
 *   ESC 7	Reverse full line (0x1b 0x37)
 *   ESC 8	Reverse half line (0x1b 0x38)
 *   ESC 9	Forward half line (0x1b 0x39)
 */

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
 * End of "$Id: texttops.c,v 1.7 1999/03/11 20:55:05 mike Exp $".
 */
