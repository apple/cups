/*
 * "$Id: texttops.c,v 1.9 1999/03/21 21:12:19 mike Exp $"
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
 *   check_range()    - Check to see if the current page is selected for
 *                      printing.
 *   getutf8()        - Get a UTF-8 encoded wide character...
 *   write_epilogue() - Write the PostScript file epilogue.
 *   write_line()     - Write a row of text.
 *   write_page()     - Write a page of text.
 *   write_prolog()   - Write the PostScript file prolog with options.
 *   write_string()   - Write a string of text.
 *   main()           - Main entry and processing of driver.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
	Landscape = 0,		/* Landscape orientation? */
	Duplex = 0;		/* Duplexed? */
lchar_t	**Page = NULL;		/* Page characters */
int	NumPages = 0,		/* Number of pages in document */
	PageNumber = 0;		/* Current page number */
char	*PageRanges = NULL;	/* Range of pages selected */
char	*PageSet = NULL;	/* All, Even, Odd pages */
int	Reversed = 0,		/* Reverse pages */
	Flip = 0;		/* Flip/mirror pages */
int	CharsPerInch = 10;	/* Number of character columns per inch */
int	LinesPerInch = 6;	/* Number of lines per inch */
float	Left = 18.0f,		/* Left margin */
	Right = 594.0f,		/* Right margin */
	Bottom = 36.0f,		/* Bottom margin */
	Top = 756.0f,		/* Top margin */
	Width = 612.0f,		/* Total page width */
	Length = 792.0f;	/* Total page length */
int	UTF8 = 0;		/* Use UTF-8 encoding? */
char	*Glyphs[65536];		/* PostScript glyphs for Unicode */


/*
 * Local functions...
 */

static int	check_range(void);
static int	getutf8(FILE *fp);
static void	write_epilogue(ppd_file_t *ppd);
static void	write_line(int row, lchar_t *line);
static void	write_page(ppd_file_t *ppd);
static void	write_prolog(ppd_file_t *ppd, char *title, int num_copies);
static void	write_string(int col, int row, int len, lchar_t *s);


/*
 * 'check_range()' - Check to see if the current page is selected for
 *                   printing.
 */

static int		/* O - 1 if selected, 0 otherwise */
check_range(void)
{
  char	*range;		/* Pointer into range string */
  int	lower, upper;	/* Lower and upper page numbers */


  if (PageSet != NULL)
  {
   /*
    * See if we only print even or odd pages...
    */

    if (strcmp(PageSet, "even") == 0 && (PageNumber & 1))
      return (0);
    if (strcmp(PageSet, "odd") == 0 && !(PageNumber & 1))
      return (0);
  }

  if (PageRanges == NULL)
    return (1);		/* No range, print all pages... */

  for (range = PageRanges; *range != '\0';)
  {
    if (*range == '-')
    {
      lower = 1;
      range ++;
      upper = strtol(range, &range, 10);
    }
    else
    {
      lower = strtol(range, &range, 10);

      if (*range == '-')
      {
        range ++;
	if (!isdigit(*range))
	  upper = 65535;
	else
	  upper = strtol(range, &range, 10);
      }
      else
        upper = lower;
    }

    if (PageNumber >= lower && PageNumber <= upper)
      return (1);

    if (*range == ',')
      range ++;
    else
      break;
  }

  return (0);
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
write_epilogue(ppd_file_t *ppd)	/* I - PPD file */
{
  puts("%%BeginEpilogue");
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

  
  for (col = 0, attr = 0, start = line;
       col < SizeColumns;
       col ++, line ++)
    if (attr != line->attr || line->ch == 0)
    {
      if (start < line)
        write_string(col - (line - start), row, line - start, start);

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
    write_string(col - (line - start), row, line - start, start);
}


/*
 * 'write_page()' - Write a page of text.
 */

static void
write_page(ppd_file_t *ppd)	/* I - PPD file */
{
  int	line;			/* Current line */


  PageNumber ++;
  if (check_range())
  {
    NumPages ++;
    printf("%%%%Page: %d %d\n", PageNumber, NumPages);
    puts("%%BeginPageSetup");
    ppdEmit(ppd, stdout, PPD_ORDER_PAGE);
    puts("%%EndPageSetup");

    puts("gsave");

    if (Landscape)
    {
      if (Duplex && (NumPages & 1) == 0)
        printf("0 %.1f translate -90 rotate\n", Width);
      else
        printf("%.1f 0 translate 90 rotate\n", Length);
    }

    for (line = 0; line < SizeLines; line ++)
      write_line(line, Page[line]);

    puts("grestore");
    puts("showpage");
    puts("%%EndPage");
  }

  memset(Page[0], 0, sizeof(lchar_t) * SizeColumns * SizeLines);
}


/*
 * 'write_prolog()' - Write the PostScript file prolog with options.
 */

static void
write_prolog(ppd_file_t *ppd,		/* I - PPD file */
             char       *title,		/* I - Title of job */
             int        num_copies)	/* I - Number of copies */
{
  int	line;				/* Current output line */
  float	temp;				/* Swapping variable */
  char	*charset;			/* Character set string */
  char	*server_root;			/* SERVER_ROOT variable */
  char	filename[1024];			/* Glyph filenames */
  FILE	*fp;				/* Glyph files */
  int	ch, unicode;			/* Character values */
  char	glyph[64];			/* Glyph name */
  int	chars[256];			/* Character encoding array */


  ppdEmit(ppd, stdout, PPD_ORDER_EXIT);
  ppdEmit(ppd, stdout, PPD_ORDER_JCL);

  puts("%!PS-Adobe-3.0");
  printf("%%%%BoundingBox: %.0f %.0f %.0f %.0f\n", Left, Bottom, Right, Top);
  if (ppd)
    printf("%%%%LanguageLevel: %d\n", ppd->language_level);
  else
    puts("%%LanguageLevel: 1");
  puts("%%Creator: texttops/CUPS-" CUPS_SVERSION);
  printf("%%%%Title: %s\n", title);
  puts("%%DocumentFonts: Courier Courier-Bold");
  puts("%%Pages: (atend)");
  puts("%%EndComments");

  if (Landscape)
  {
    temp   = Left;
    Left   = Bottom;
    Bottom = temp;

    temp   = Right;
    Right  = Top;
    Top    = temp;

    temp   = Width;
    Width  = Length;
    Length = temp;
  }

  SizeColumns = (Right - Left) / 72.0 * CharsPerInch;
  SizeLines   = (Top - Bottom) / 72.0 * LinesPerInch;

  Page    = calloc(sizeof(lchar_t *), SizeLines);
  Page[0] = calloc(sizeof(lchar_t), SizeColumns * SizeLines);
  for (line = 1; line < SizeLines; line ++)
    Page[line] = Page[0] + line * SizeColumns;

  if (PageColumns > 1)
  {
    ColumnGutter = CharsPerInch / 2;
    ColumnWidth  = (SizeColumns - ColumnGutter * (PageColumns - 1)) / PageColumns;
  }
  else
    ColumnWidth = SizeColumns;

  puts("%%BeginDocumentSetup");
  ppdEmit(ppd, stdout, PPD_ORDER_DOCUMENT);
  ppdEmit(ppd, stdout, PPD_ORDER_ANY);
  puts("%%EndDocumentSetup");

  puts("%%BeginProlog");
  ppdEmit(ppd, stdout, PPD_ORDER_PROLOG);

 /*
  * Get the output character set; if it is undefined or "us-ascii", do
  * nothing because we can use the default encoding...
  */

  charset = getenv("CHARSET");
  if (charset != NULL && strcmp(charset, "us-ascii") != 0)
  {
   /*
    * Load the PostScript glyph names and the corresponding character
    * set definition...
    */

    if ((server_root = getenv("SERVER_ROOT")) == NULL)
      strcpy(filename, "psglyphs.dat");
    else
      sprintf(filename, "%s/filter/psglyphs.dat");

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
        sprintf(filename, "%s.dat", charset + 4);
      else
        sprintf(filename, "%s/filter/%s.dat", server_root, charset + 4);

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
  }

  puts("% Define fonts");

  printf("/FN /Courier findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
         120.0 / CharsPerInch, 68.0 / LinesPerInch);
  printf("/FB /Courier-Bold findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
         120.0 / CharsPerInch, 68.0 / LinesPerInch);

  puts("% Common procedures");

  puts("/N { FN setfont moveto } bind def");
  puts("/B { FB setfont moveto } bind def");
  puts("/U { dup 0 rlineto stroke neg 0 rmoveto } bind def");
  puts("/S { show } bind def");

  puts("% Number copies");
  printf("/#copies %d def\n", num_copies);

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
  int	i;			/* Looping var */
  float	x, y;			/* Position of text */


 /*
  * Position the text and set the font...
  */

  if (Duplex && (NumPages & 1) == 0)
  {
    x = Width - Right;
    y = Length - Bottom;
  }
  else
  {
    x = Left;
    y = Top;
  }

  x += (float)col * 72.0 / (float)CharsPerInch;
  y -= (float)(row + 1) * 72.0 / (float)LinesPerInch;

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

    puts(")S");
  }
}


/*
 * 'main()' - Main entry and processing of driver.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  FILE		*fp;		/* Print file */
  int		i,		/* Looping var */
		ch,		/* Current char from file */
		attr,		/* Current attribute */
		line,		/* Current line */
  		column,		/* Current column */
  		page_column;	/* Current page column */
  ppd_file_t	*ppd;		/* PPD file */
  ppd_size_t	*pagesize;	/* Current page size */
  int		num_options;	/* Number of print options */
  cups_option_t	*options;	/* Print options */
  char		*val;		/* Option value */


  if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
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
    Width  = pagesize->width;
    Length = pagesize->length;
    Top    = pagesize->top;
    Bottom = pagesize->bottom;
    Left   = pagesize->left;
    Right  = pagesize->right;
  }

  Landscape = cupsGetOption("landscape", num_options, options) != NULL;
  WrapLines = cupsGetOption("wrap", num_options, options) != NULL;
  if ((val = cupsGetOption("columns", num_options, options)) != NULL)
    PageColumns = atoi(val);
  if ((val = cupsGetOption("cpi", num_options, options)) != NULL)
    CharsPerInch = atoi(val);
  if ((val = cupsGetOption("lpi", num_options, options)) != NULL)
    LinesPerInch = atoi(val);
  if ((val = cupsGetOption("sides", num_options, options)) != NULL &&
      atoi(val) == 2)
    Duplex = 1;
  if ((val = cupsGetOption("Duplex", num_options, options)) != NULL &&
      strcmp(val, "NoTumble") == 0)
    Duplex = 1;
  if ((val = cupsGetOption("page-ranges", num_options, options)) != NULL)
    PageRanges = val;
  if ((val = cupsGetOption("page-set", num_options, options)) != NULL)
    PageSet = val;

  write_prolog(ppd, argv[3], atoi(argv[4]));

 /*
  * Read text from the specified source and print it...
  */

  column       = 0;
  line         = 0;
  page_column  = 0;
  attr         = 0;

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
          break;

      case 0x09 :		/* HT - tab to next 8th column */
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
                write_page(ppd);
		page_column = 0;
              }
            }
          }
          break;

      case 0x0a :		/* LF - output current line */
          line ++;
          column = 0;

          if (line >= SizeLines)
          {
            page_column ++;
            line = 0;

            if (page_column >= PageColumns)
            {
              write_page(ppd);
	      page_column = 0;
            }
          }
          break;

      case 0x0b :		/* VT - move up 1 line */
          if (line > 0)
	    line --;
          break;

      case 0x0c :		/* FF - eject current page... */
          page_column ++;
	  column = 0;
          line = 0;

          if (page_column >= PageColumns)
          {
            write_page(ppd);
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
        	write_page(ppd);
        	page_column = 0;
              }
            }
          }

          if (column < ColumnWidth)
	  {
	    i = column + page_column * (ColumnWidth + ColumnGutter);

            if (ch == Page[line][i].ch)
              Page[line][i].attr |= ATTR_BOLD;
            else if (Page[line][i].ch == '_')
              Page[line][i].attr |= ATTR_UNDERLINE;
	    else
              Page[line][i].attr = attr;

            Page[line][i].ch = ch;
	  }

          column ++;
          break;          
    }
  }

 /*
  * Write any remaining page data...
  */

  if (line > 0 || page_column > 0 || column > 0)
    write_page(ppd);

 /*
  * Write the epilog and return...
  */

  write_epilogue(ppd);

  return (0);
}


/*
 * End of "$Id: texttops.c,v 1.9 1999/03/21 21:12:19 mike Exp $".
 */
