/*
 * "$Id: texttops.c,v 1.23 2000/02/27 16:01:31 mike Exp $"
 *
 *   Text to PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1993-2000 by Easy Software Products.
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

char		*Glyphs[65536];	/* PostScript glyphs for Unicode */
int		NumFonts;	/* Number of fonts to use */
char		*Fonts[256][4];	/* Fonts to use */
unsigned short	Chars[65536];	/* 0xffcc (ff = font, cc = char) */
unsigned short	Codes[65536];	/* Unicode glyph mapping to fonts */
int		Widths[256];	/* Widths of each font */
int		Directions[256];/* Text directions for each font */


/*
 * Local functions...
 */

static void	write_line(int row, lchar_t *line);
static void	write_string(int col, int row, int len, lchar_t *s);
static void	write_text(char *s);


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
WriteProlog(char       *title,	/* I - Title of job */
	    char       *user,	/* I - Username */
            ppd_file_t *ppd)	/* I - PPD file info */
{
  int		i, j, k;	/* Looping vars */
  char		*charset;	/* Character set string */
  char		filename[1024];	/* Glyph filenames */
  FILE		*fp;		/* Glyph files */
  char		line[1024],	/* Line from file */
		*lineptr,	/* Pointer into line */
		*valptr;	/* Pointer to value in line */
  int		ch, unicode;	/* Character values */
  int		start, end;	/* Start and end values for range */
  char		glyph[64];	/* Glyph name */
  time_t	curtime;	/* Current time */
  struct tm	*curtm;		/* Current date */
  char		curdate[255];	/* Current date (text format) */
  int		num_fonts;	/* Number of unique fonts */
  char		*fonts[1024];	/* Unique fonts */
  static char	*names[] =	/* Font names */
		{
		  "cupsNormal",
		  "cupsBold",
		  "cupsItalic"
		};


 /*
  * Allocate memory for the page...
  */

  SizeColumns = (PageRight - PageLeft) / 72.0 * CharsPerInch;
  SizeLines   = (PageTop - PageBottom) / 72.0 * LinesPerInch;

  Page    = calloc(sizeof(lchar_t *), SizeLines);
  Page[0] = calloc(sizeof(lchar_t), SizeColumns * SizeLines);
  for (i = 1; i < SizeLines; i ++)
    Page[i] = Page[0] + i * SizeColumns;

  if (PageColumns > 1)
  {
    ColumnGutter = CharsPerInch / 2;
    ColumnWidth  = (SizeColumns - ColumnGutter * (PageColumns - 1)) /
                   PageColumns;
  }
  else
    ColumnWidth = SizeColumns;

 /*
  * Output the DSC header...
  */

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
  puts("%%DocumentSuppliedResources: procset texttops 1.1 0");
  puts("%%Pages: (atend)");

 /*
  * Initialize globals...
  */

  NumFonts = 0;
  memset(Fonts, 0, sizeof(Fonts));
  memset(Glyphs, 0, sizeof(Glyphs));
  memset(Chars, 0, sizeof(Chars));
  memset(Codes, 0, sizeof(Codes));

 /*
  * Load the PostScript glyph names and the corresponding character
  * set definition...
  */

  if ((fp = fopen(CUPS_DATADIR "/data/psglyphs", "r")) != NULL)
  {
    while (fscanf(fp, "%x%63s", &unicode, glyph) == 2)
      Glyphs[unicode] = strdup(glyph);

    fclose(fp);
  }
  else
  {
    perror("ERROR: Unable to open " CUPS_DATADIR "/data/psglyphs");
    exit(1);
  }

 /*
  * Get the output character set...
  */

  charset = getenv("CHARSET");
  if (charset != NULL && strcmp(charset, "us-ascii") != 0)
  {
    snprintf(filename, sizeof(filename), CUPS_DATADIR "/charsets/%s", charset);

    if ((fp = fopen(filename, "r")) == NULL)
    {
     /*
      * Can't open charset file!
      */

      fprintf(stderr, "ERROR: Unable to open %s: %s\n", filename,
              strerror(errno));
      exit(1);
    }

   /*
    * Opened charset file; now see if this is really a charset file...
    */

    if (fgets(line, sizeof(line), fp) == NULL)
    {
     /*
      * Bad/empty charset file!
      */

      fclose(fp);
      fprintf(stderr, "ERROR: Bad/empty charset file %s\n", filename);
      exit(1);
    }

    if (strncmp(line, "charset", 7) != 0)
    {
     /*
      * Bad format/not a charset file!
      */

      fclose(fp);
      fprintf(stderr, "ERROR: Bad charset file %s\n", filename);
      exit(1);
    }

   /*
    * See if this is an 8-bit or UTF-8 character set file...
    */

    line[strlen(line) - 1] = '\0'; /* Drop \n */
    for (lineptr = line + 7; isspace(*lineptr); lineptr ++); /* Skip whitespace */

    if (strcmp(lineptr, "8bit") == 0)
    {
     /*
      * 8-bit text...
      */

      UTF8     = 0;
      NumFonts = 1;

     /*
      * Read the font description...
      */

      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Skip comment and blank lines...
	*/

        if (line[0] != '#' && line[0] != '\n')
	  break;
      }

     /*
      * Read the font descriptions that should look like:
      *
      *   direction width normal [bold italic bold-italic]
      */

      lineptr = line;
      while (isspace(*lineptr))
	lineptr ++;

      valptr = lineptr;

      while (!isspace(*lineptr) && *lineptr)
	lineptr ++;

      if (!*lineptr)
      {
       /*
	* Can't have a font without all required values...
	*/

	fprintf(stderr, "ERROR: bad font description line: %s\n", valptr);
	fclose(fp);
	exit(1);
      }

      *lineptr++ = '\0';

      if (strcmp(valptr, "ltor") == 0)
	Directions[0] = 1;
      else if (strcmp(valptr, "rtol") == 0)
	Directions[0] = -1;
      else
      {
	fprintf(stderr, "ERROR: Bad text direction %s\n", valptr);
	fclose(fp);
	exit(1);
      }

     /*
      * Got the direction, now get the width...
      */

      while (isspace(*lineptr))
	lineptr ++;

      valptr = lineptr;

      while (!isspace(*lineptr) && *lineptr)
	lineptr ++;

      if (!*lineptr)
      {
       /*
	* Can't have a font without all required values...
	*/

	fprintf(stderr, "ERROR: bad font description line: %s\n", valptr);
	fclose(fp);
	exit(1);
      }

      *lineptr++ = '\0';

      if (strcmp(valptr, "single") == 0)
        Widths[0] = 1;
      else if (strcmp(valptr, "double") == 0)
        Widths[0] = 2;
      else 
      {
	fprintf(stderr, "ERROR: Bad text width %s\n", valptr);
	fclose(fp);
	exit(1);
      }

     /*
      * Get the fonts...
      */

      for (i = 0; *lineptr && i < 4; i ++)
      {
	while (isspace(*lineptr))
	  lineptr ++;

	valptr = lineptr;

	while (!isspace(*lineptr) && *lineptr)
	  lineptr ++;

        if (*lineptr)
	  *lineptr++ = '\0';

        if (lineptr > valptr)
	  Fonts[0][i] = strdup(valptr);
      }

     /*
      * Fill in remaining fonts as needed...
      */

      for (j = i; j < 4; j ++)
	Fonts[0][j] = strdup(Fonts[0][0]);

     /*
      * Read encoding lines...
      */

      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Skip comment and blank lines...
	*/

        if (line[0] == '#' || line[0] == '\n')
	  continue;

       /*
        * Grab the character and unicode glyph number.
	*/

	if (sscanf(line, "%x%x", &ch, &unicode) == 2 && ch < 256)
	{
          Codes[ch] = unicode;
	  Chars[ch] = ch;
	}
      }

      fclose(fp);
    }
    else if (strcmp(lineptr, "utf8") == 0)
    {
     /*
      * UTF-8 (Unicode) text...
      */

      UTF8 = 1;

     /*
      * Read the font descriptions...
      */

      NumFonts = 0;

      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Skip comment and blank lines...
	*/

        if (line[0] == '#' || line[0] == '\n')
	  continue;

       /*
	* Read the font descriptions that should look like:
	*
	*   start end direction width normal [bold italic bold-italic]
	*/

	lineptr = line;

        start = strtol(lineptr, &lineptr, 16);
	end   = strtol(lineptr, &lineptr, 16);

	while (isspace(*lineptr))
	  lineptr ++;

	valptr = lineptr;

	while (!isspace(*lineptr) && *lineptr)
	  lineptr ++;

	if (!*lineptr)
	{
	 /*
	  * Can't have a font without all required values...
	  */

	  fprintf(stderr, "ERROR: bad font description line: %s\n", valptr);
	  fclose(fp);
	  exit(1);
	}

	*lineptr++ = '\0';

	if (strcmp(valptr, "ltor") == 0)
	  Directions[NumFonts] = 1;
	else if (strcmp(valptr, "rtol") == 0)
	  Directions[NumFonts] = -1;
	else
	{
	  fprintf(stderr, "ERROR: Bad text direction %s\n", valptr);
	  fclose(fp);
	  exit(1);
	}

       /*
	* Got the direction, now get the width...
	*/

	while (isspace(*lineptr))
	  lineptr ++;

	valptr = lineptr;

	while (!isspace(*lineptr) && *lineptr)
	  lineptr ++;

	if (!*lineptr)
	{
	 /*
	  * Can't have a font without all required values...
	  */

	  fprintf(stderr, "ERROR: bad font description line: %s\n", valptr);
	  fclose(fp);
	  exit(1);
	}

	*lineptr++ = '\0';

	if (strcmp(valptr, "single") == 0)
          Widths[NumFonts] = 1;
	else if (strcmp(valptr, "double") == 0)
          Widths[NumFonts] = 2;
	else 
	{
	  fprintf(stderr, "ERROR: Bad text width %s\n", valptr);
	  fclose(fp);
	  exit(1);
	}

       /*
	* Get the fonts...
	*/

	for (i = 0; *lineptr && i < 4; i ++)
	{
	  while (isspace(*lineptr))
	    lineptr ++;

	  valptr = lineptr;

	  while (!isspace(*lineptr) && *lineptr)
	    lineptr ++;

          if (*lineptr)
	    *lineptr++ = '\0';

          if (lineptr > valptr)
	    Fonts[NumFonts][i] = strdup(valptr);
	}

       /*
	* Fill in remaining fonts as needed...
	*/

	for (j = i; j < 4; j ++)
	  Fonts[NumFonts][j] = strdup(Fonts[NumFonts][0]);

       /*
        * Define the character mappings...
	*/

	for (i = start, j = NumFonts * 256; i <= end; i ++, j ++)
	{
	  Chars[i] = j;
          Codes[j] = i;
	}

       /*
        * Move to the next font, stopping if needed...
	*/

        NumFonts ++;
	if (NumFonts >= 256)
	  break;
      }

      fclose(fp);
    }
    else
    {
      fprintf(stderr, "ERROR: Bad charset type %s\n", lineptr);
      fclose(fp);
      exit(1);
    }
  }
  else
  {
   /*
    * Standard ASCII output just uses Courier, Courier-Bold, and
    * possibly Courier-Oblique.
    */

    NumFonts = 1;

    Fonts[0][ATTR_NORMAL]     = strdup("Courier");
    Fonts[0][ATTR_BOLD]       = strdup("Courier-Bold");
    Fonts[0][ATTR_ITALIC]     = strdup("Courier-Oblique");
    Fonts[0][ATTR_BOLDITALIC] = strdup("Courier-BoldOblique");

    Widths[0]     = 1;
    Directions[0] = 1;

   /*
    * Define US-ASCII characters...
    */

    for (i = 32; i < 127; i ++)
    {
      Chars[i] = i;
      Codes[i] = i;
    }
  }

 /*
  * Generate a list of unique fonts to use...
  */

  for (i = 0, num_fonts = 0; i < NumFonts; i ++)
    for (j = 1 + PrettyPrint; j >= 0; j --)
    {
      for (k = 0; k < num_fonts; k ++)
        if (strcmp(Fonts[i][j], fonts[k]) == 0)
	  break;

      if (k >= num_fonts)
      {
       /*
        * Add new font...
	*/

        fonts[num_fonts] = Fonts[i][j];
	num_fonts ++;
      }
    }

 /*
  * List the fonts that will be used...
  */

  for (i = 0; i < num_fonts; i ++)
    if (i == 0)
      printf("%%DocumentNeededResources: font %s\n", fonts[i]);
    else
      printf("%%+ font %s\n", fonts[i]);

  puts("%%EndComments");

  puts("%%BeginProlog");

 /*
  * Write the encoding array(s)...
  */

  puts("% character encoding(s)");

  for (i = 0; i < NumFonts; i ++)
  {
    printf("/cupsEncoding%02x [\n", i);

    for (ch = 0; ch < 256; ch ++)
    {
      if (Glyphs[Codes[i * 256 + ch]])
	printf("/%s", Glyphs[Codes[i * 256 + ch]]);
      else
	printf("/.notdef");

      if ((ch & 7) == 7)
	putchar('\n');
    }

    puts("] def");
  }

 /*
  * Create the fonts...
  */

  if (NumFonts == 1)
  {
   /*
    * Just reencode the named fonts...
    */

    puts("% Reencode fonts");

    for (i = 1 + PrettyPrint; i >= 0; i --)
    {
      printf("/%s findfont\n", Fonts[0][i]);
      puts("dup length 1 add dict begin\n"
	   "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
	   "	/Encoding cupsEncoding00 def\n"
	   "	currentdict\n"
	   "end");
      printf("/%s exch definefont pop\n", names[i]);
    }
  }
  else
  {
   /*
    * Construct composite fonts...  Start by reencoding the base fonts...
    */

    puts("% Reencode base fonts");

    for (i = 1 + PrettyPrint; i >= 0; i --)
      for (j = 0; j < NumFonts; j ++)
      {
	printf("/%s findfont\n", Fonts[j][i]);
	printf("dup length 1 add dict begin\n"
	       "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
	       "	/Encoding cupsEncoding%02x def\n"
	       "	currentdict\n"
	       "end\n", j);
	printf("/%s%02x exch definefont /%s%02x exch def\n", names[i], j,
	       names[i], j);
      }

   /*
    * Then merge them into composite fonts...
    */

    puts("% Create composite fonts...");

    for (i = 1 + PrettyPrint; i >= 0; i --)
    {
      puts("8 dict begin");
      puts("/FontType 0 def/FontMatrix[1.0 0 0 1.0 0 0]def/FMapType 2 def/Encoding[");
      for (j = 0; j < NumFonts; j ++)
        if (j == (NumFonts - 1))
	  printf("%d", j);
	else if ((j & 15) == 15)
          printf("%d\n", j);
	else
	  printf("%d ", j);
      puts("]def/FDepVector[");
      for (j = 0; j < NumFonts; j ++)
        if (j == (NumFonts - 1))
          printf("%s%02x", names[i], j);
	else if ((j & 3) == 3)
          printf("%s%02x\n", names[i], j);
	else
	  printf("%s%02x ", names[i], j);
      puts("]def currentdict end");
      printf("/%s exch definefont pop\n", names[i]);
    }
  }

 /*
  * Output the texttops procset...
  */

  puts("%%BeginResource: procset texttops 1.1 0");

  puts("% Define fonts");

  printf("/FN /cupsNormal findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
         120.0 / CharsPerInch, 68.0 / LinesPerInch);
  printf("/FB /cupsBold findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
         120.0 / CharsPerInch, 68.0 / LinesPerInch);
  if (PrettyPrint)
    printf("/FI /cupsItalic findfont [%.1f 0 0 %.1f 0 0] makefont def\n",
           120.0 / CharsPerInch, 68.0 / LinesPerInch);

  puts("% Common procedures");

  puts("/N { FN setfont moveto } bind def");
  puts("/B { FB setfont moveto } bind def");
  printf("/U { gsave 0.5 setlinewidth 0 %.2f rmoveto "
         "0 rlineto stroke grestore } bind def\n", -6.8 / LinesPerInch);

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

    puts("/n {");
    puts("\t20 string cvs % convert page number to string");
    puts("\tdup length % get length");
    puts("\tdup 2 mul string /P exch def % P = string twice as long");
    puts("\t0 1 2 index 1 sub { % loop through each character in the page number");
    puts("\t\tdup 3 index exch get % get character N from the page number");
    puts("\t\texch 2 mul dup % compute offset in P");
    puts("\t\tP exch 0 put % font 0");
    puts("\t\t1 add P exch 2 index put % character");
    puts("\t\tpop % discard character");
    puts("\t} for % do for loop");
    puts("\tpop pop % discard string and length");
    puts("\tP % put string on stack");
    puts("} bind def");

    printf("/T");
    write_text(title);
    puts("def");

    printf("/D");
    write_text(curdate);
    puts("def");

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

    puts("\tFB setfont");
    puts("\t0 setgray");

    if (Duplex)
    {
      puts("\tdup 2 mod 0 eq {");
      printf("\t\tT stringwidth pop neg %.1f add %.1f } {\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
	     (0.5f + 0.157f) * 72.0f / LinesPerInch);
      printf("\t\t%.1f %.1f } ifelse\n", 36.0f / LinesPerInch,
	     (0.5f + 0.157f) * 72.0f / LinesPerInch);
    }
    else
      printf("\t%.1f %.1f\n", 36.0f / LinesPerInch,
	     (0.5f + 0.157f) * 72.0f / LinesPerInch);

    puts("\tmoveto T show");

    printf("\tD dup stringwidth pop neg 2 div %.1f add %.1f\n",
           (PageRight - PageLeft) * 0.5,
           (0.5f + 0.157f) * 72.0f / LinesPerInch);
    puts("\tmoveto show");

    if (Duplex)
    {
      puts("\tdup n exch 2 mod 0 eq {");
      printf("\t\t%.1f %.1f } {\n", 36.0f / LinesPerInch,
	     (0.5f + 0.157f) * 72.0f / LinesPerInch);
      printf("\t\tdup stringwidth pop neg %.1f add %.1f } ifelse\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
	     (0.5f + 0.157f) * 72.0f / LinesPerInch);
    }
    else
      printf("\tn dup stringwidth pop neg %.1f add %.1f\n",
             PageRight - PageLeft - 36.0f / LinesPerInch,
	     (0.5f + 0.157f) * 72.0f / LinesPerInch);

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


  /**** TODO - handle bidi text and arabic composition ****/
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
  int		ch;		/* Current character */
  int		font;		/* Font for character */
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

  x += (float)col * 72.0f / (float)CharsPerInch;
  y -= (float)(row + 0.843) * 72.0f / (float)LinesPerInch;

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

  if (UTF8)
  {
   /*
    * Write a hex string...
    */

    putchar('<');

    while (len > 0)
    {
      printf("%04x", Chars[s->ch]);

      len --;
      s ++;
    }

    putchar('>');
  }
  else
  {
   /*
    * Write a quoted string...
    */

    putchar('(');

    while (len > 0)
    {
      ch = Chars[s->ch];

      if (ch < 32 || ch > 126)
      {
       /*
	* Quote 8-bit and control characters...
	*/

	printf("\\%03o", ch);
      }
      else
      {
       /*
	* Quote the parenthesis and backslash as needed...
	*/

	if (ch == '(' || ch == ')' || ch == '\\')
	  putchar('\\');

	putchar(ch);
      }

      len --;
      s ++;
    }

    putchar(')');
  }

  if (PrettyPrint)
  {
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
    puts("S");
}


/*
 * 'write_text()' - Write a text string, quoting/encoding as needed.
 */

static void
write_text(char *s)	/* I - String to write */
{
  int		ch;	/* Actual character value (UTF8) */
  unsigned char	*utf8;	/* UTF8 text */


  if (UTF8)
  {
   /*
    * 8/8 encoding...
    */

    putchar('<');

    utf8 = (unsigned char *)s;

    while (*utf8)
    {
      if (*utf8 < 0xc0)
        ch = *utf8 ++;
      else if ((*utf8 & 0xe0) == 0xc0)
      {
       /*
        * Two byte character...
	*/

        ch = ((utf8[0] & 0x1f) << 6) | (utf8[1] & 0x3f);
	utf8 += 2;
      }
      else
      {
       /*
        * Three byte character...
	*/

        ch = ((((utf8[0] & 0x1f) << 6) | (utf8[1] & 0x3f)) << 6) |
	     (utf8[2] & 0x3f);
	utf8 += 3;
      }

      printf("%04x", Chars[ch]);
    }

    putchar('>');
  }
  else
  {
   /*
    * Standard 8-bit encoding...
    */

    putchar('(');

    while (*s)
    {
      if (*s < 32 || *s > 126)
        printf("\\%03o", *s);
      else
      {
	if (*s == '(' || *s == ')' || *s == '\\')
	  putchar('\\');

	putchar(*s);
      }

      s ++;
    }

    putchar(')');
  }
}


/*
 * End of "$Id: texttops.c,v 1.23 2000/02/27 16:01:31 mike Exp $".
 */
