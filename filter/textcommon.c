/*
 * "$Id: textcommon.c,v 1.1 1999/03/23 20:10:19 mike Exp $"
 *
 *   Common text filter routines for the Common UNIX Printing System (CUPS).
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   TextMain()         - Standard main entry for text filters.
 *   compare_keywords() - Compare two C/C++ keywords.
 *   getutf8()          - Get a UTF-8 encoded wide character...
 */

/*
 * Include necessary headers...
 */

#include "textcommon.h"


/*
 * Globals...
 */

int	WrapLines = 0,		/* Wrap text in lines */
	SizeLines = 60,		/* Number of lines on a page */
	SizeColumns = 80,	/* Number of columns on a line */
	PageColumns = 1,	/* Number of columns on a page */
	ColumnGutter = 0,	/* Number of characters between text columns */
	ColumnWidth = 80,	/* Width of each column */
	PrettyPrint = 0;	/* Do pretty code formatting */
lchar_t	**Page = NULL;		/* Page characters */
int	NumPages = 0;		/* Number of pages in document */
int	CharsPerInch = 10;	/* Number of character columns per inch */
int	LinesPerInch = 6;	/* Number of lines per inch */
int	UTF8 = 0;		/* Use UTF-8 encoding? */
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


/*
 * 'TextMain()' - Standard main entry for text filters.
 */

int				/* O - Exit status */
TextMain(char *name,		/* I - Name of filter */
         int  argc,		/* I - Number of command-line arguments */
         char *argv[])		/* I - Command-line arguments */
{
  FILE		*fp;		/* Print file */
  int		i,		/* Looping var */
		ch,		/* Current char from file */
		lastch,		/* Previous char from file */
		attr,		/* Current attribute */
		line,		/* Current line */
  		column,		/* Current column */
  		page_column;	/* Current page column */
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
    fprintf(stderr, "ERROR: %s job-id user title copies options [file]\n",
            name);
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

  options     = NULL;
  num_options = cupsParseOptions(argv[5], 0, &options);

  SetCommonOptions(num_options, options);

  WrapLines = cupsGetOption("wrap", num_options, options) != NULL;

  if ((val = cupsGetOption("columns", num_options, options)) != NULL)
    PageColumns = atoi(val);

  if ((val = cupsGetOption("cpi", num_options, options)) != NULL)
    CharsPerInch = atoi(val);

  if ((val = cupsGetOption("lpi", num_options, options)) != NULL)
    LinesPerInch = atoi(val);

  if ((val = cupsGetOption("prettyprint", num_options, options)) != NULL)
  {
    PrettyPrint = 1;
    PageTop     -= 216.0f / LinesPerInch;
  }

  WriteProlog(argv[3], argv[2]);

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
                WritePage();
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
              WritePage();
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
            WritePage();
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
        	WritePage();
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
    WritePage();

 /*
  * Write the epilog and return...
  */

  WriteEpilogue();

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
 * End of "$Id: textcommon.c,v 1.1 1999/03/23 20:10:19 mike Exp $".
 */
