/*
 * "$Id: texttops.c,v 1.15 1999/05/11 19:46:19 mike Exp $"
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
 *   main()          - Main entry for text to PostScript filter.
 *   WriteEpilogue() - Write the PostScript file epilogue.
 *   WritePage()     - Write a page of text.
 *   WriteProlog()   - Write the PostScript file prolog with options.
 *   write_line()    - Write a row of text.
 *   write_string()  - Write a string of text.
 */

/*
 * Include necessary headers...
 */

#include "textcommon.h"


/*
 * Globals...
 */

char	*Glyphs[65536];		/* PostScript glyphs for Unicode */

/*
 * Local functions...
 */

static void	write_line(int row, lchar_t *line);
static void	write_string(int col, int row, int len, lchar_t *s);


/*
 * 'main()' - Main entry for text to PostScript filter.
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  return (TextMain("texttops", argc, argv));
}


/*
 * 'WriteEpilogue()' - Write the PostScript file epilogue.
 */

void
WriteEpilogue(void)
{
  puts("%%BeginTrailer");
  printf("%%%%Pages: %d\n", NumPages);
  puts("%%EOF");

  free(Page[0]);
  free(Page);
}


/*
 * 'WritePage()' - Write a page of text.
 */

void
WritePage(void)
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
 * 'WriteProlog()' - Write the PostScript file prolog with options.
 */

void
WriteProlog(char *title,	/* I - Title of job */
	    char *user)		/* I - Username */
{
  int		line;		/* Current output line */
  char		*charset;	/* Character set string */
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
  puts("%%Creator: texttops/" CUPS_SVERSION);
  printf("%%%%CreationDate: %s\n", curdate);
  printf("%%%%Title: %s\n", title);
  printf("%%%%For: %s\n", user);
  if (PrettyPrint)
    puts("%%DocumentNeededResources: font Courier Courier-Bold Courier-Oblique");
  else
    puts("%%DocumentNeededResources: font Courier Courier-Bold");
  puts("%%DocumentSuppliedResources: procset texttops 1.0 0");
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
  puts("%%BeginResource: procset texttops 1.0 0");

  charset = getenv("CHARSET");
  if (charset != NULL && strcmp(charset, "us-ascii") != 0)
  {
   /*
    * Load the PostScript glyph names and the corresponding character
    * set definition...
    */

    memset(Glyphs, 0, sizeof(Glyphs));

    if ((fp = fopen(CUPS_DATADIR "/data/psglyphs", "r")) != NULL)
    {
      while (fscanf(fp, "%x%s", &unicode, glyph) == 2)
        Glyphs[unicode] = strdup(glyph);

      fclose(fp);
    }

    if (strncmp(charset, "iso-", 4) == 0)
    {
      memset(chars, 0, sizeof(chars));

      sprintf(filename, CUPS_DATADIR "/%s", charset + 4);

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

    if (PrettyPrint)
    {
      puts("/Courier-Oblique findfont");
      puts("dup length dict begin\n"
           "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
           "	/Encoding textEncoding def\n"
           "	currentdict\n"
           "end");
      puts("/Courier-Oblique exch definefont pop");
    }
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
    else
      printf("\t%.1f %.1f translate\n",
             PageLeft, PageTop + 72.0f / LinesPerInch);

    printf("\t0 0 %.1f %.1f rectfill\n", PageRight - PageLeft,
	   144.0f / LinesPerInch);

    puts("\tFN setfont");
    puts("\t0 setgray");

    if (Duplex)
    {
      puts("\tdup 2 mod 0 eq {");
      printf("\t\tT stringwidth pop neg %.1f add %.1f } {\n",
             PageRight - PageLeft - 36.0f / LinesPerInch, 54.0f / LinesPerInch);
      printf("\t\t%.1f %.1f } ifelse\n", 36.0f / LinesPerInch,
             54.0f / LinesPerInch);
    }
    else
      printf("\t%.1f %.1f\n", 36.0f / LinesPerInch,
             54.0f / LinesPerInch);

    puts("\tmoveto T show");

    printf("\t(%s)\n", curdate);
    printf("\tdup stringwidth pop neg 2 div %.1f add %.1f\n",
           (PageRight - PageLeft) * 0.5, 54.0f / LinesPerInch);
    puts("\tmoveto show");

    if (Duplex)
    {
      puts("\tdup P cvs exch 2 mod 0 eq {");
      printf("\t\t%.1f %.1f } {\n", 36.0f / LinesPerInch,
             54.0f / LinesPerInch);
      printf("\t\tdup stringwidth pop neg %.1f add %.1f } ifelse\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
             54.0f / LinesPerInch);
    }
    else
      printf("\tP cvs dup stringwidth pop neg %.1f add %.1f\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
             54.0f / LinesPerInch);

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
 * End of "$Id: texttops.c,v 1.15 1999/05/11 19:46:19 mike Exp $".
 */
