/*
 * "$Id: texttops.c,v 1.12 1999/03/23 18:39:08 mike Exp $"
 *
 *   Text to PostScript filter for the Common UNIX Printing System (CUPS).
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
 *   main()             - Main entry and processing of driver.
 *   compare_keywords() - Compare two C/C++ keywords.
 *   getutf8()          - Get a UTF-8 encoded wide character...
 *   write_epilogue()   - Write the PostScript file epilogue.
 *   write_line()       - Write a row of text.
 *   write_page()       - Write a page of text.
 *   write_prolog()     - Write the PostScript file prolog with options.
 *   write_string()     - Write a string of text.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>


/*
 * Constants...
 */

#define ATTR_BOLD	0x01
#define ATTR_UNDERLINE	0x02
#define ATTR_RAISED	0x04
#define ATTR_LOWERED	0x08
#define ATTR_ITALIC	0x10
#define ATTR_RED	0x20
#define ATTR_GREEN	0x40
#define ATTR_BLUE	0x80


/*
 * Structures...
 */

typedef struct			/**** Character/attribute structure... ****/
{
  unsigned short ch,		/* Character */
		attr;		/* Any attributes */
} lchar_t;


/*
 * Globals...
 */

int	WrapLines = 0,		/* Wrap text in lines */
	SizeLines = 60,		/* Number of lines on a page */
	SizeColumns = 80,	/* Number of columns on a line */
	PageColumns = 1,	/* Number of columns on a page */
	ColumnGutter = 0,	/* Number of characters between text columns */
	ColumnWidth = 80,	/* Width of each column */
	Orientation = 0,	/* 0 = portrait, 1 = landscape, etc. */
	Duplex = 0,		/* Duplexed? */
	PrettyPrint = 0,	/* Do pretty code formatting */
	ColorDevice = 0;	/* Do color text? */
lchar_t	**Page = NULL;		/* Page characters */
int	NumPages = 0;		/* Number of pages in document */
int	CharsPerInch = 10;	/* Number of character columns per inch */
int	LinesPerInch = 6;	/* Number of lines per inch */
float	PageLeft = 18.0f,	/* Left margin */
	PageRight = 594.0f,	/* Right margin */
	PageBottom = 36.0f,	/* Bottom margin */
	PageTop = 756.0f,	/* Top margin */
	PageWidth = 612.0f,	/* Total page width */
	PageLength = 792.0f;	/* Total page length */
int	UTF8 = 0;		/* Use UTF-8 encoding? */
char	*Glyphs[65536];		/* PostScript glyphs for Unicode */
char	*Keywords[] =		/* List of known keywords... */
	{
	  "auto",
	  "break",
	  "case",
	  "char",
	  "class",
	  "const",
	  "continue",
	  "default",
	  "delete",
	  "double",
	  "do",
	  "else",
	  "enum",
	  "extern",
	  "float",
	  "for",
	  "friend",
	  "goto",
	  "if",
	  "int",
	  "long",
	  "new",
	  "private",
	  "protected",
	  "public",
	  "register",
	  "return",
	  "short",
	  "signed",
	  "sizeof",
	  "static",
	  "struct",
	  "switch",
	  "typedef",
	  "union",
	  "unsigned",
	  "void",
	  "volatile",
	  "while"
	};

/*
 * Local functions...
 */

static int	compare_keywords(const void *, const void *);
static int	getutf8(FILE *fp);
static void	write_epilogue(void);
static void	write_line(int row, lchar_t *line);
static void	write_page(void);
static void	write_prolog(char *title, char *user);
static void	write_string(int col, int row, int len, lchar_t *s);


/*
 * 'main()' - Main entry and processing of driver.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  FILE		*fp;		/* Print file */
  float		temp;		/* Swapping variable */
  int		i,		/* Looping var */
		ch,		/* Current char from file */
		lastch,		/* Previous char from file */
		attr,		/* Current attribute */
		line,		/* Current line */
  		column,		/* Current column */
  		page_column;	/* Current page column */
  ppd_file_t	*ppd;		/* PPD file */
  ppd_size_t	*pagesize;	/* Current page size */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  char		*val;		/* Option value */
  char		keyword[64],	/* Keyword string */
		*keyptr;	/* Pointer into string */
  int		keycol;		/* Column where keyword starts */
  int		ccomment;	/* Inside a C-style comment? */
  int		cstring;	/* Inside a C string */


  if (argc < 6 || argc > 7)
  {
    fputs("ERROR: texttops job-id user title copies options [file]\n", stderr);
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

  ppd = ppdOpenFile(getenv("PPD"));

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  ppdMarkDefaults(ppd);
  cupsMarkOptions(ppd, num_options, options);

  if ((pagesize = ppdPageSize(ppd, NULL)) != NULL)
  {
    PageWidth  = pagesize->width;
    PageLength = pagesize->length;
    PageTop    = pagesize->top;
    PageBottom = pagesize->bottom;
    PageLeft   = pagesize->left;
    PageRight  = pagesize->right;
  }

  if (ppd != NULL)
    ColorDevice = ppd->color_device;

  ppdClose(ppd);

  if ((val = cupsGetOption("landscape", num_options, options)) != NULL)
    Orientation = 1;

  if ((val = cupsGetOption("orientation-requested", num_options, options)) != NULL)
  {
   /*
    * Map IPP orientation values to 0 to 3:
    *
    *   3 = 0 degrees   = 0
    *   4 = 90 degrees  = 1
    *   5 = -90 degrees = 3
    *   6 = 180 degrees = 2
    */

    Orientation = atoi(val) - 3;
    if (Orientation >= 2)
      Orientation ^= 1;
  }

  if ((val = cupsGetOption("page-left", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageLeft = (float)atof(val);
	  break;
      case 1 :
          PageBottom = (float)atof(val);
	  break;
      case 2 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 3 :
          PageTop = PageLength - (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-right", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 1 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 2 :
          PageLeft = (float)atof(val);
	  break;
      case 3 :
          PageBottom = (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-bottom", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageBottom = (float)atof(val);
	  break;
      case 1 :
          PageRight = PageWidth - (float)atof(val);
	  break;
      case 2 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 3 :
          PageLeft = (float)atof(val);
	  break;
    }
  }

  if ((val = cupsGetOption("page-top", num_options, options)) != NULL)
  {
    switch (Orientation)
    {
      case 0 :
          PageTop = PageLength - (float)atof(val);
	  break;
      case 1 :
          PageLeft = (float)atof(val);
	  break;
      case 2 :
          PageBottom = (float)atof(val);
	  break;
      case 3 :
          PageRight = PageWidth - (float)atof(val);
	  break;
    }
  }

  switch (Orientation)
  {
    case 0 : /* Portait */
        break;

    case 1 : /* Landscape */
	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;

    case 2 : /* Reverse Portrait */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;
        break;

    case 3 : /* Reverse Landscape */
	temp       = PageWidth - PageLeft;
	PageLeft   = PageWidth - PageRight;
	PageRight  = temp;

	temp       = PageLength - PageBottom;
	PageBottom = PageLength - PageTop;
	PageTop    = temp;

	temp       = PageLeft;
	PageLeft   = PageBottom;
	PageBottom = temp;

	temp       = PageRight;
	PageRight  = PageTop;
	PageTop    = temp;

	temp       = PageWidth;
	PageWidth  = PageLength;
	PageLength = temp;
	break;
  }

  WrapLines = cupsGetOption("wrap", num_options, options) != NULL;

  if ((val = cupsGetOption("columns", num_options, options)) != NULL)
    PageColumns = atoi(val);

  if ((val = cupsGetOption("cpi", num_options, options)) != NULL)
    CharsPerInch = atoi(val);

  if ((val = cupsGetOption("lpi", num_options, options)) != NULL)
    LinesPerInch = atoi(val);

  if ((val = cupsGetOption("sides", num_options, options)) != NULL &&
      strncmp(val, "two-", 4) == 0)
    Duplex = 1;

  if ((val = cupsGetOption("Duplex", num_options, options)) != NULL &&
      strcmp(val, "NoTumble") == 0)
    Duplex = 1;

  if ((val = cupsGetOption("prettyprint", num_options, options)) != NULL)
  {
    PrettyPrint = 1;
    PageTop     -= 216.0f / LinesPerInch;
  }

  write_prolog(argv[3], argv[2]);

 /*
  * Read text from the specified source and print it...
  */

  column       = 0;
  line         = 0;
  page_column  = 0;
  attr         = 0;
  keyptr       = keyword;
  keycol       = 0;
  ccomment     = 0;
  cstring      = 0;

  while ((ch = getutf8(fp)) >= 0)
  {
   /*
    * Control codes:
    *
    *   BS	Backspace (0x08)
    *   HT	Horizontal tab; next 8th column (0x09)
    *   LF	Line feed; forward full line (0x0a)
    *   VT	Vertical tab; reverse full line (0x0b)
    *   FF	Form feed (0x0c)
    *   CR	Carriage return (0x0d)
    *   ESC 7	Reverse full line (0x1b 0x37)
    *   ESC 8	Reverse half line (0x1b 0x38)
    *   ESC 9	Forward half line (0x1b 0x39)
    */

    switch (ch)
    {
      case 0x08 :		/* BS - backspace for boldface & underline */
          if (column > 0)
            column --;

          keyptr = keyword;
	  keycol = column;
          break;

      case 0x09 :		/* HT - tab to next 8th column */
          if (PrettyPrint && keyptr > keyword)
	  {
	    *keyptr = '\0';
	    keyptr  = keyword;

	    if (bsearch(&keyptr, Keywords, sizeof(Keywords) / sizeof(Keywords[0]),
	                sizeof(Keywords[0]), compare_keywords))
            {
	     /*
	      * Put keywords in boldface...
	      */

	      i = page_column * (ColumnWidth + ColumnGutter);

	      while (keycol < column)
	      {
	        Page[line][keycol + i].attr |= ATTR_BOLD;
		keycol ++;
	      }
	    }
	  }

          column = (column + 8) & ~7;

          if (column >= ColumnWidth && WrapLines)
          {			/* Wrap text to margins */
            line ++;
            column = 0;

            if (line >= SizeLines)
            {
              page_column ++;
              line = 0;

              if (page_column >= PageColumns)
              {
                write_page();
		page_column = 0;
              }
            }
          }

	  keycol = column;
          break;

      case 0x0a :		/* LF - output current line */
          if (PrettyPrint && keyptr > keyword)
	  {
	    *keyptr = '\0';
	    keyptr  = keyword;

	    if (bsearch(&keyptr, Keywords, sizeof(Keywords) / sizeof(Keywords[0]),
	                sizeof(Keywords[0]), compare_keywords))
            {
	     /*
	      * Put keywords in boldface...
	      */

	      i = page_column * (ColumnWidth + ColumnGutter);

	      while (keycol < column)
	      {
	        Page[line][keycol + i].attr |= ATTR_BOLD;
		keycol ++;
	      }
	    }
	  }

          line ++;
          column = 0;
	  keycol = 0;

          if (!ccomment && !cstring)
	    attr &= ~(ATTR_ITALIC | ATTR_BOLD | ATTR_RED | ATTR_GREEN | ATTR_BLUE);

          if (line >= SizeLines)
          {
            page_column ++;
            line = 0;

            if (page_column >= PageColumns)
            {
              write_page();
	      page_column = 0;
            }
          }
          break;

      case 0x0b :		/* VT - move up 1 line */
          if (line > 0)
	    line --;

          keyptr = keyword;
	  keycol = column;

          if (!ccomment && !cstring)
	    attr &= ~(ATTR_ITALIC | ATTR_BOLD | ATTR_RED | ATTR_GREEN | ATTR_BLUE);
          break;

      case 0x0c :		/* FF - eject current page... */
          if (PrettyPrint && keyptr > keyword)
	  {
	    *keyptr = '\0';
	    keyptr  = keyword;

	    if (bsearch(&keyptr, Keywords, sizeof(Keywords) / sizeof(Keywords[0]),
	                sizeof(Keywords[0]), compare_keywords))
            {
	     /*
	      * Put keywords in boldface...
	      */

	      i = page_column * (ColumnWidth + ColumnGutter);

	      while (keycol < column)
	      {
	        Page[line][keycol + i].attr |= ATTR_BOLD;
		keycol ++;
	      }
	    }
	  }

          page_column ++;
	  column = 0;
	  keycol = 0;
          line   = 0;

          if (!ccomment && !cstring)
	    attr &= ~(ATTR_ITALIC | ATTR_BOLD | ATTR_RED | ATTR_GREEN | ATTR_BLUE);

          if (page_column >= PageColumns)
          {
            write_page();
            page_column = 0;
          }
          break;

      case 0x0d :		/* CR */
          column = 0;
          break;

      case 0x1b :		/* Escape sequence */
          ch = getutf8(fp);
	  if (ch == '7')
	  {
	   /*
	    * ESC 7	Reverse full line (0x1b 0x37)
	    */

            if (line > 0)
	      line --;
	  }
	  else if (ch == '8')
	  {
           /*
	    *   ESC 8	Reverse half line (0x1b 0x38)
	    */

            if ((attr & ATTR_RAISED) && line > 0)
	    {
	      attr &= ~ATTR_RAISED;
              line --;
	    }
	    else if (attr & ATTR_LOWERED)
	      attr &= ~ATTR_LOWERED;
	    else
	      attr |= ATTR_RAISED;
	  }
	  else if (ch == '9')
	  {
           /*
	    *   ESC 9	Forward half line (0x1b 0x39)
	    */

            if ((attr & ATTR_LOWERED) && line < (SizeLines - 1))
	    {
	      attr &= ~ATTR_LOWERED;
              line ++;
	    }
	    else if (attr & ATTR_RAISED)
	      attr &= ~ATTR_RAISED;
	    else
	      attr |= ATTR_LOWERED;
	  }
	  break;

      default :			/* All others... */
          if (ch < ' ')
            break;		/* Ignore other control chars */

          if (PrettyPrint)
	  {
	   /*
	    * Do highlighting of C/C++ keywords, preprocessor commands,
	    * and comments...
	    */

	    if ((ch == ' ' || ch == '\t') && (attr & ATTR_BOLD))
	    {
	     /*
	      * Stop bolding preprocessor command...
	      */

	      attr &= ~ATTR_BOLD;
	    }
	    else if (!isalpha(ch) && keyptr > keyword)
	    {
	     /*
	      * Look for a keyword...
	      */

	      *keyptr = '\0';
	      keyptr  = keyword;

	      if (bsearch(&keyptr, Keywords, sizeof(Keywords) / sizeof(Keywords[0]),
	                  sizeof(Keywords[0]), compare_keywords))
              {
	       /*
	        * Put keywords in boldface...
		*/

	        i = page_column * (ColumnWidth + ColumnGutter);

		while (keycol < column)
		{
	          Page[line][keycol + i].attr |= ATTR_BOLD;
		  keycol ++;
		}
	      }
	    }
	    else if (isalpha(ch) && !ccomment && !cstring)
	    {
	     /*
	      * Add characters to the current keyword (if they'll fit).
	      */

              if (keyptr == keyword)
	        keycol = column;

	      if (keyptr < (keyword + sizeof(keyword) - 1))
	        *keyptr++ = ch;
            }
	    else if (ch == '\"' && lastch != '\\' && !ccomment && !cstring)
	    {
	     /*
	      * Start a C string constant...
	      */

	      cstring = -1;
              attr    |= ATTR_BLUE;
	    }
            else if (ch == '*' && lastch == '/' && !cstring)
	    {
	     /*
	      * Start a C-style comment...
	      */

	      ccomment = 1;
	      attr     |= ATTR_ITALIC | ATTR_GREEN;
	    }
	    else if (ch == '/' && lastch == '/' && !cstring)
	    {
	     /*
	      * Start a C++-style comment...
	      */

	      attr |= ATTR_ITALIC | ATTR_GREEN;
	    }
	    else if (ch == '#' && column == 0 && !ccomment && !cstring)
	    {
	     /*
	      * Start a preprocessor command...
	      */

	      attr |= ATTR_BOLD | ATTR_RED;
	    }
          }

          if (column >= ColumnWidth && WrapLines)
          {			/* Wrap text to margins */
            column = 0;
	    line ++;

            if (line >= SizeLines)
            {
              page_column ++;
              line = 0;

              if (page_column >= PageColumns)
              {
        	write_page();
        	page_column = 0;
              }
            }
          }

         /*
	  * Add text to the current column & line...
	  */

          if (column < ColumnWidth)
	  {
	    i = column + page_column * (ColumnWidth + ColumnGutter);

            if (PrettyPrint)
              Page[line][i].attr = attr;
            else if (ch == Page[line][i].ch)
              Page[line][i].attr |= ATTR_BOLD;
            else if (Page[line][i].ch == '_')
              Page[line][i].attr |= ATTR_UNDERLINE;
	    else
              Page[line][i].attr = attr;

            Page[line][i].ch = ch;
	  }

          if (PrettyPrint)
	  {
	    if ((ch == '{' || ch == '}') && !ccomment && !cstring &&
	        column < ColumnWidth)
	    {
	     /*
	      * Highlight curley braces...
	      */

	      Page[line][i].attr |= ATTR_BOLD;
	    }
	    else if ((ch == '/' || ch == '*') && lastch == '/' &&
	             column < ColumnWidth)
	    {
	     /*
	      * Highlight first comment character...
	      */

	      Page[line][i - 1].attr = attr;
	    }
	    else if (ch == '\"' && lastch != '\\' && !ccomment && cstring > 0)
	    {
	     /*
	      * End a C string constant...
	      */

	      cstring = 0;
	      attr    &= ~ATTR_BLUE;
            }
	    else if (ch == '/' && lastch == '*' && ccomment)
	    {
	     /*
	      * End a C-style comment...
	      */

	      ccomment = 0;
	      attr     &= ~(ATTR_ITALIC | ATTR_GREEN);
	    }

            if (cstring < 0)
	      cstring = 1;
	  }

          column ++;
          break;          
    }

   /*
    * Save this character for the next cycle.
    */

    lastch = ch;
  }

 /*
  * Write any remaining page data...
  */

  if (line > 0 || page_column > 0 || column > 0)
    write_page();

 /*
  * Write the epilog and return...
  */

  write_epilogue();

  return (0);
}


/*
 * 'compare_keywords()' - Compare two C/C++ keywords.
 */

static int				/* O - Result of strcmp */
compare_keywords(const void *k1,	/* I - First keyword */
                 const void *k2)	/* I - Second keyword */
{
  return (strcmp(*((const char **)k1), *((const char **)k2)));
}


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

  if (ch < 0xc0 || !UTF8)	/* One byte character? */
    return (ch);
  else if ((ch & 0xe0) == 0xc0)
  {
   /*
    * Two byte character...
    */

    if ((next = getc(fp)) == EOF)
      return (EOF);
    else
      return (((ch & 0x1f) << 6) | (next & 0x3f));
  }
  else if ((ch & 0xf0) == 0xe0)
  {
   /*
    * Three byte character...
    */

    if ((next = getc(fp)) == EOF)
      return (EOF);

    ch = ((ch & 0x0f) << 6) | (next & 0x3f);

    if ((next = getc(fp)) == EOF)
      return (EOF);
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

static void
write_epilogue(void)
{
  puts("%%BeginTrailer");
  printf("%%%%Pages: %d\n", NumPages);
  puts("%%EOF");

  free(Page[0]);
  free(Page);
}


/*
 * 'write_line()' - Write a row of text.
 */

static void
write_line(int     row,		/* I - Row number (0 to N) */
           lchar_t *line)	/* I - Line to print */
{
  int		col;		/* Current column */
  int		attr;		/* Current attribute */
  lchar_t	*start;		/* First character in sequence */

  
  for (col = 0, start = line; col < SizeColumns;)
  {
    while (col < SizeColumns && (line->ch == ' ' || line->ch == 0))
    {
      col ++;
      line ++;
    }

    if (col >= SizeColumns)
      break;

    attr  = line->attr;
    start = line;

    while (col < SizeColumns && line->ch != 0 && attr == line->attr)
    {
      col ++;
      line ++;
    }

    write_string(col - (line - start), row, line - start, start);
  }
}


/*
 * 'write_page()' - Write a page of text.
 */

static void
write_page(void)
{
  int	line;			/* Current line */


  NumPages ++;
  printf("%%%%Page: %d %d\n", NumPages, NumPages);

  puts("gsave");

  if (PrettyPrint)
    printf("%d H\n", NumPages);

  for (line = 0; line < SizeLines; line ++)
    write_line(line, Page[line]);

  puts("grestore");
  puts("showpage");

  memset(Page[0], 0, sizeof(lchar_t) * SizeColumns * SizeLines);
}


/*
 * 'write_prolog()' - Write the PostScript file prolog with options.
 */

static void
write_prolog(char *title,	/* I - Title of job */
	     char *user)	/* I - Username */
{
  int		line;		/* Current output line */
  char		*charset;	/* Character set string */
  char		*server_root;	/* SERVER_ROOT variable */
  char		filename[1024];	/* Glyph filenames */
  FILE		*fp;		/* Glyph files */
  int		ch, unicode;	/* Character values */
  char		glyph[64];	/* Glyph name */
  int		chars[256];	/* Character encoding array */
  time_t	curtime;	/* Current time */
  struct tm	*curtm;		/* Current date */
  char		curdate[255];	/* Current date (text format) */


  curtime = time(NULL);
  curtm   = localtime(&curtime);
  strftime(curdate, sizeof(curdate), "%c", curtm);

  puts("%!PS-Adobe-3.0");
  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", PageLeft, PageBottom,
         PageRight, PageTop);
  if (Orientation & 1)
    puts("%%Orientation: Landscape");
  puts("%%Creator: texttops/CUPS-" CUPS_SVERSION);
  printf("%%%%CreationDate: %s\n", curdate);
  printf("%%%%Title: %s\n", title);
  printf("%%%%For: %s\n", user);
  if (PrettyPrint)
    puts("%%DocumentNeededResources: font Courier Courier-Bold Courier-Oblique");
  else
    puts("%%DocumentNeededResources: font Courier Courier-Bold");
  puts("%%DocumentSuppliedResources: procset texttops 4.0 0");
  puts("%%Pages: (atend)");

  puts("%%EndComments");

  SizeColumns = (PageRight - PageLeft) / 72.0 * CharsPerInch;
  SizeLines   = (PageTop - PageBottom) / 72.0 * LinesPerInch;

  Page    = calloc(sizeof(lchar_t *), SizeLines);
  Page[0] = calloc(sizeof(lchar_t), SizeColumns * SizeLines);
  for (line = 1; line < SizeLines; line ++)
    Page[line] = Page[0] + line * SizeColumns;

  if (PageColumns > 1)
  {
    ColumnGutter = CharsPerInch / 2;
    ColumnWidth  = (SizeColumns - ColumnGutter * (PageColumns - 1)) /
                   PageColumns;
  }
  else
    ColumnWidth = SizeColumns;

 /*
  * Get the output character set; if it is undefined or "us-ascii", do
  * nothing because we can use the default encoding...
  */

  puts("%%BeginProlog");
  puts("%%BeginResource procset texttops 4.0 0");

  charset = getenv("CHARSET");
  if (charset != NULL && strcmp(charset, "us-ascii") != 0)
  {
   /*
    * Load the PostScript glyph names and the corresponding character
    * set definition...
    */

    if ((server_root = getenv("SERVER_ROOT")) == NULL)
      strcpy(filename, "../data/psglyphs");
    else
      sprintf(filename, "%s/data/psglyphs", server_root);

    memset(Glyphs, 0, sizeof(Glyphs));

    if ((fp = fopen(filename, "r")) != NULL)
    {
      while (fscanf(fp, "%x%s", &unicode, glyph) == 2)
        Glyphs[unicode] = strdup(glyph);

      fclose(fp);
    }

    if (strncmp(charset, "iso-", 4) == 0)
    {
      memset(chars, 0, sizeof(chars));

      if (server_root == NULL)
        sprintf(filename, "../data/%s", charset + 4);
      else
        sprintf(filename, "%s/data/%s", server_root, charset + 4);

      if ((fp = fopen(filename, "r")) != NULL)
      {
        while (fscanf(fp, "%x%x", &ch, &unicode) == 2)
          chars[ch] = unicode;

        fclose(fp);
      }
    }
    else
    {
     /*
      * UTF-8 encoding - just pass the first 256 characters for now...
      */

      UTF8 = 1;

      for (unicode = 0; unicode < 256; unicode ++)
        chars[unicode] = unicode;
    }

   /*
    * Write the encoding array...
    */

    printf("%% %s encoding\n", charset);
    puts("/textEncoding [");

    for (ch = 0; ch < 256; ch ++)
    {
      if (Glyphs[chars[ch]])
	printf("/%s", Glyphs[chars[ch]]);
      else
	printf("/.notdef");

      if ((ch & 7) == 7)
        putchar('\n');
    }

    puts("] def");

    puts("% Reencode fonts");
    puts("/Courier findfont");
    puts("dup length dict begin\n"
         "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
         "	/Encoding textEncoding def\n"
         "	currentdict\n"
         "end");
    puts("/Courier exch definefont pop");

    puts("/Courier-Bold findfont");
    puts("dup length dict begin\n"
         "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
         "	/Encoding textEncoding def\n"
         "	currentdict\n"
         "end");
    puts("/Courier-Bold exch definefont pop");

    puts("/Courier-Oblique findfont");
    puts("dup length dict begin\n"
         "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
         "	/Encoding textEncoding def\n"
         "	currentdict\n"
         "end");
    puts("/Courier-Oblique exch definefont pop");
  }

  puts("% Define fonts");

  printf("/FN /Courier findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
         120.0 / CharsPerInch, 68.0 / LinesPerInch);
  printf("/FB /Courier-Bold findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
         120.0 / CharsPerInch, 68.0 / LinesPerInch);
  if (PrettyPrint)
    printf("/FI /Courier-Oblique findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
           120.0 / CharsPerInch, 68.0 / LinesPerInch);

  puts("% Common procedures");

  puts("/N { FN setfont moveto } bind def");
  puts("/B { FB setfont moveto } bind def");
  puts("/U { gsave 0 rlineto stroke grestore } bind def");

  if (PrettyPrint)
  {
    if (ColorDevice)
    {
      puts("/S { 0.0 setgray show } bind def");
      puts("/r { 0.5 0.0 0.0 setrgbcolor show } bind def");
      puts("/g { 0.0 0.5 0.0 setrgbcolor show } bind def");
      puts("/b { 0.0 0.0 0.5 setrgbcolor show } bind def");
    }
    else
    {
      puts("/S { 0.0 setgray show } bind def");
      puts("/r { 0.2 setgray show } bind def");
      puts("/g { 0.2 setgray show } bind def");
      puts("/b { 0.2 setgray show } bind def");
    }

    puts("/I { FI setfont moveto } bind def");

    puts("/P 20 string def");
    printf("/T(");

    while (*title != '\0')
    {
      if (*title == '(' || *title == ')' || *title == '\\')
	putchar('\\');

      putchar(*title++);
    }

    puts(")def");

    puts("/H {");
    puts("gsave");
    puts("\t0.9 setgray");

    if (Duplex)
    {
      puts("\tdup 2 mod 0 eq {");
      printf("\t\t%.1f %.1f translate } {\n",
             PageWidth - PageRight, PageTop + 72.0f / LinesPerInch);
      printf("\t\t%.1f %.1f translate } ifelse\n",
             PageLeft, PageTop + 72.0f / LinesPerInch);
    }

    printf("\t0 0 %.1f %.1f rectfill\n", PageRight - PageLeft,
	   144.0f / LinesPerInch);

    puts("\tFN setfont");
    puts("\t0 setgray");

    if (Duplex)
    {
      puts("\tdup 2 mod 0 eq {");
      printf("\t\tT stringwidth pop neg %.1f add %.1f } {\n",
             PageRight - PageLeft - 36.0f / LinesPerInch, 36.0f / LinesPerInch);
      printf("\t\t%.1f %.1f } ifelse\n", 36.0f / LinesPerInch,
             36.0f / LinesPerInch);
    }
    else
      printf("\t%.1f %.1f\n", 36.0f / LinesPerInch,
             36.0f / LinesPerInch);

    puts("\tmoveto T show");

    printf("\t(%s)\n", curdate);
    printf("\tdup stringwidth pop neg 2 div %.1f add %.1f\n",
           (PageRight - PageLeft) * 0.5, 36.0f / LinesPerInch);
    puts("\tmoveto show");

    if (Duplex)
    {
      puts("\tdup P cvs exch 2 mod 0 eq {");
      printf("\t\t%.1f %.1f } {\n", 36.0f / LinesPerInch,
             36.0f / LinesPerInch);
      printf("\t\tdup stringwidth pop neg %.1f add %.1f } ifelse\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
             36.0f / LinesPerInch);
    }
    else
      printf("\tP cvs dup stringwidth pop neg %.1f add %.1f\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
             36.0f / LinesPerInch);

    puts("\tmoveto show");
    puts("\tgrestore");
    puts("} bind def");
  }
  else
    puts("/S { show } bind def");

  puts("%%EndResource");

  puts("%%EndProlog");
}


/*
 * 'write_string()' - Write a string of text.
 */

static void
write_string(int     col,	/* I - Start column */
             int     row,	/* I - Row */
             int     len,	/* I - Number of characters */
             lchar_t *s)	/* I - String to print */
{
  int		i;		/* Looping var */
  float		x, y;		/* Position of text */
  unsigned	attr;		/* Character attributes */


 /*
  * Position the text and set the font...
  */

  if (Duplex && (NumPages & 1) == 0)
  {
    x = PageWidth - PageRight;
    y = PageTop;
  }
  else
  {
    x = PageLeft;
    y = PageTop;
  }

  x += (float)col * 72.0 / (float)CharsPerInch;
  y -= (float)(row + 1) * 72.0 / (float)LinesPerInch;

  attr = s->attr;

  if (attr & ATTR_RAISED)
    y += 36.0 / (float)LinesPerInch;
  else if (attr & ATTR_LOWERED)
    y -= 36.0 / (float)LinesPerInch;

  if (x == (int)x)
    printf("%.0f ", x);
  else
    printf("%.1f ", x);

  if (y == (int)y)
    printf("%.0f ", y);
  else
    printf("%.1f ", y);

  if (attr & ATTR_BOLD)
    putchar('B');
  else if (attr & ATTR_ITALIC)
    putchar('I');
  else
    putchar('N');

  if (attr & ATTR_UNDERLINE)
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

    putchar('>');

    if (attr & ATTR_RED)
      puts("r");
    else if (attr & ATTR_GREEN)
      puts("g");
    else if (attr & ATTR_BLUE)
      puts("b");
    else
      puts("S");
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

        printf("\\%03o", s->ch);
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

    putchar(')');

    if (attr & ATTR_RED)
      puts("r");
    else if (attr & ATTR_GREEN)
      puts("g");
    else if (attr & ATTR_BLUE)
      puts("b");
    else
      puts("S");
  }
}


/*
 * End of "$Id: texttops.c,v 1.12 1999/03/23 18:39:08 mike Exp $".
 */
