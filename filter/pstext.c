/*
 * "$Id$"
 *
 *   Common PostScript text code for CUPS.
 *
 *   Copyright 2008-2010 by Apple Inc.
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
 *   psTextEmbedFonts() - Embed PostScript fonts.
 *   psTextListFonts()  - List PostScript fonts.
 *   psTextInitialize() - Load and embed font data for UTF-8 text.
 *   psTextUTF8()       - Output UTF-8 text at the current position.
 *   psTextUTF32()      - Output UTF-32 text at the current position.
 */

/*
 * Include necessary headers...
 */

#include "pstext.h"
#include <cups/language-private.h>


/*
 * Composite font names...
 */

static const char * const ps_font_names[] =
{
  "cupsNormal",
  "cupsBold",
  "cupsItalic",
  "cupsBoldItalic"
};


/*
 * 'psTextEmbedFonts()'- Embed PostScript fonts.
 */

void
psTextEmbedFonts(ps_text_t *fonts)	/* I - Font data */
{
  int		i, j;			/* Looping vars */
  const char	*cups_datadir;		/* CUPS_DATADIR environment variable */
  char		*font;			/* Current font */
  char		filename[1024];		/* Current filename */
  FILE		*fp;			/* Current file */
  char		line[1024];		/* Line from file */
  int		ch;			/* Character value */


 /*
  * Get the data directory...
  */

  if ((cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cups_datadir = CUPS_DATADIR;

 /*
  * Embed each font...
  */

  for (font = (char *)cupsArrayFirst(fonts->unique);
       font;
       font = (char *)cupsArrayNext(fonts->unique))
  {
    printf("%%%%BeginResource: font %s\n", font);

    snprintf(filename, sizeof(filename), "%s/fonts/%s", cups_datadir, font);
    if ((fp = fopen(filename, "rb")) != NULL)
    {
      while ((j = fread(line, 1, sizeof(line), fp)) > 0)
	fwrite(line, 1, j, stdout);

      fclose(fp);
    }
    else
      fprintf(stderr, "DEBUG: Unable to open font file \"%s\" - %s\n",
              filename, strerror(errno));

    puts("\n%%EndResource");
  }

 /*
  * Write the encoding arrays...
  */

  puts("% Character encodings");

  for (i = 0; i < fonts->num_fonts; i ++)
  {
    printf("/cupsEncoding%02x [\n", i);

    for (ch = 0; ch < 256; ch ++)
    {
      if (fonts->glyphs[fonts->codes[i * 256 + ch]])
	printf("/%s", fonts->glyphs[fonts->codes[i * 256 + ch]]);
      else if (fonts->codes[i * 256 + ch] > 255)
        printf("/uni%04X", fonts->codes[i * 256 + ch]);
      else
	printf("/.notdef");

      if ((ch & 7) == 7)
	putchar('\n');
    }

    puts("] def");
  }

 /*
  * Construct composite fonts...  Start by reencoding the base fonts...
  */

  puts("% Reencode base fonts");

  for (i = 0; i < 4; i ++)
    for (j = 0; j < fonts->num_fonts; j ++)
    {
      printf("/%s findfont\n", fonts->fonts[j][i]);
      printf("dup length 1 add dict begin\n"
	     "	{ 1 index /FID ne { def } { pop pop } ifelse } forall\n"
	     "	/Encoding cupsEncoding%02x def\n"
	     "	currentdict\n"
	     "end\n", j);
      printf("/%s%02x exch definefont /%s%02x exch def\n", ps_font_names[i], j,
	     ps_font_names[i], j);
    }

 /*
  * Then merge them into composite fonts...
  */

  puts("% Create composite fonts");

  for (i = 0; i < 4; i ++)
  {
    puts("8 dict begin");
    puts("/FontType 0 def/FontMatrix[1.0 0 0 1.0 0 0]def/FMapType 2 def"
         "/Encoding[");
    for (j = 0; j < fonts->num_fonts; j ++)
      if (j == (fonts->num_fonts - 1))
	printf("%d", j);
      else if ((j & 15) == 15)
	printf("%d\n", j);
      else
	printf("%d ", j);
    puts("]def/FDepVector[");
    for (j = 0; j < fonts->num_fonts; j ++)
      if (j == (fonts->num_fonts - 1))
	printf("%s%02x", ps_font_names[i], j);
      else if ((j & 3) == 3)
	printf("%s%02x\n", ps_font_names[i], j);
      else
	printf("%s%02x ", ps_font_names[i], j);
    puts("]def currentdict end");
    printf("/%s exch definefont pop\n", ps_font_names[i]);
  }

 /*
  * Procedures...
  */

  puts("% Procedures to justify text...\n"
       "/showcenter{dup stringwidth pop -0.5 mul 0 rmoveto show}bind def\n"
       "/showleft{show}bind def\n"
       "/showright{dup stringwidth pop neg 0 rmoveto show}bind def");
}


/*
 * 'psTextListFonts()' - List PostScript fonts.
 */

void
psTextListFonts(ps_text_t *fonts)	/* I - Font data */
{
  char	*font;				/* Current font */


  font = (char *)cupsArrayFirst(fonts->unique);
  printf("%%%%DocumentSuppliedResources: font %s\n", font);
  while ((font = (char *)cupsArrayNext(fonts->unique)) != NULL)
    printf("%%%%+ font %s\n", font);
}


/*
 * 'psTextInitialize()' - Load and embed font data for UTF-8 text.
 */

ps_text_t *				/* O - Font data */
psTextInitialize(void)
{
  ps_text_t	*fonts;			/* Font data */
  int		i, j;			/* Looping vars */
  char		filename[1024];		/* Current filename */
  FILE		*fp;			/* Current file */
  const char	*cups_datadir;		/* CUPS_DATADIR environment variable */
  char		line[1024],		/* Line from file */
		*lineptr,		/* Pointer into line */
		*valptr;		/* Pointer to value in line */
  int		unicode;		/* Character value */
  int		start, end;		/* Start and end values for range */
  char		glyph[64];		/* Glyph name */


 /*
  * Get the data directory...
  */

  if ((cups_datadir = getenv("CUPS_DATADIR")) == NULL)
    cups_datadir = CUPS_DATADIR;

 /*
  * Initialize the PostScript text data...
  */

  fonts        = (ps_text_t *)calloc(1, sizeof(ps_text_t));
  fonts->size  = -1.0;
  fonts->style = -1;

 /*
  * Load the PostScript glyph names...
  */

  snprintf(filename, sizeof(filename), "%s/data/psglyphs", cups_datadir);

  if ((fp = fopen(filename, "r")) != NULL)
  {
    while (fscanf(fp, "%x%63s", &unicode, glyph) == 2)
      fonts->glyphs[unicode] = _cupsStrAlloc(glyph);

    fclose(fp);
  }
  else
  {
    _cupsLangPrintf(stderr, _("ERROR: Unable to open \"%s\" - %s\n"), filename,
                    strerror(errno));
    exit(1);
  }

 /*
  * Open the UTF-8 character set definition...
  */

  snprintf(filename, sizeof(filename), "%s/charsets/utf-8", cups_datadir);

  if ((fp = fopen(filename, "r")) == NULL)
  {
   /*
    * Can't open charset file!
    */

    _cupsLangPrintf(stderr, _("ERROR: Unable to open %s: %s\n"), filename,
		    strerror(errno));
    exit(1);
  }

  if (!fgets(line, sizeof(line), fp) || strncmp(line, "charset utf8", 12))
  {
   /*
    * Bad/empty charset file!
    */

    fclose(fp);
    _cupsLangPrintf(stderr, _("ERROR: Bad charset file %s\n"), filename);
    exit(1);
  }

 /*
  * Read the font descriptions...
  */

  fonts->unique = cupsArrayNew((cups_array_func_t)strcmp, NULL);

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

    while (isspace(*lineptr & 255))
      lineptr ++;

    valptr = lineptr;

    while (!isspace(*lineptr & 255) && *lineptr)
      lineptr ++;

    if (!*lineptr)
    {
     /*
      * Can't have a font without all required values...
      */

      _cupsLangPrintf(stderr, _("ERROR: Bad font description line: %s\n"),
                      valptr);
      fclose(fp);
      exit(1);
    }

    *lineptr++ = '\0';

    if (!strcmp(valptr, "ltor"))
      fonts->directions[fonts->num_fonts] = 1;
    else if (!strcmp(valptr, "rtol"))
      fonts->directions[fonts->num_fonts] = -1;
    else
    {
      _cupsLangPrintf(stderr, _("ERROR: Bad text direction %s\n"), valptr);
      fclose(fp);
      exit(1);
    }

   /*
    * Got the direction, now get the width...
    */

    while (isspace(*lineptr & 255))
      lineptr ++;

    valptr = lineptr;

    while (!isspace(*lineptr & 255) && *lineptr)
      lineptr ++;

    if (!*lineptr)
    {
     /*
      * Can't have a font without all required values...
      */

      _cupsLangPrintf(stderr, _("ERROR: Bad font description line: %s\n"),
                      valptr);
      fclose(fp);
      exit(1);
    }

    *lineptr++ = '\0';

    if (!strcmp(valptr, "single"))
      fonts->widths[fonts->num_fonts] = 1;
    else if (!strcmp(valptr, "double"))
      fonts->widths[fonts->num_fonts] = 2;
    else 
    {
      _cupsLangPrintf(stderr, _("ERROR: Bad text width %s\n"), valptr);
      fclose(fp);
      exit(1);
    }

   /*
    * Get the fonts...
    */

    for (i = 0; *lineptr && i < 4; i ++)
    {
      while (isspace(*lineptr & 255))
	lineptr ++;

      valptr = lineptr;

      while (!isspace(*lineptr & 255) && *lineptr)
	lineptr ++;

      if (*lineptr)
	*lineptr++ = '\0';

      if (lineptr > valptr)
      {
        if (!cupsArrayFind(fonts->unique, valptr))
	  cupsArrayAdd(fonts->unique, _cupsStrAlloc(valptr));

	fonts->fonts[fonts->num_fonts][i] = _cupsStrAlloc(valptr);
      }
    }

   /*
    * Fill in remaining fonts as needed...
    */

    for (j = i; j < 4; j ++)
      fonts->fonts[fonts->num_fonts][j] =
          _cupsStrAlloc(fonts->fonts[fonts->num_fonts][0]);

   /*
    * Define the character mappings...
    */

    for (i = start, j = fonts->num_fonts * 256; i <= end; i ++, j ++)
    {
      fonts->chars[i] = j;
      fonts->codes[j] = i;
    }

   /*
    * Move to the next font, stopping if needed...
    */

    fonts->num_fonts ++;
    if (fonts->num_fonts >= 256)
      break;
  }

  fclose(fp);

  if (cupsArrayCount(fonts->unique) == 0)
  {
    _cupsLangPrintf(stderr, _("ERROR: No fonts in charset file %s\n"), filename);
    exit(1);
  }

  return (fonts);
}


/*
 * 'psTextUTF8()' - Output UTF-8 text at the current position.
 */

void
psTextUTF8(ps_text_t  *fonts,		/* I - Font data */
           float      size,		/* I - Size in points */
	   int        style,		/* I - Style */
	   int        align,		/* I - Alignment */
	   const char *text)		/* I - UTF-8 text */
{
  cups_utf32_t	utf32[2048];		/* Temporary buffer */
  int		utf32len;		/* Number of characters */


  if (!text)
  {
    puts("");
    return;
  }

  if ((utf32len = cupsUTF8ToUTF32(utf32, (cups_utf8_t *)text,
                                  (int)(sizeof(utf32) / sizeof(utf32[0])))) > 0)
    psTextUTF32(fonts, size, style, align, utf32, utf32len);
}


/*
 * 'psTextUTF32()' - Output UTF-32 text at the current position.
 */

void
psTextUTF32(ps_text_t          *fonts,	/* I - Font data */
            float              size,	/* I - Size in points */
	    int                style,	/* I - Font style */
	    int                align,	/* I - Alignment */
	    const cups_utf32_t *text,	/* I - UTF-32 text */
	    int                textlen)	/* I - Length of text */
{
  if (size != fonts->size || style != fonts->style)
  {
    printf("/%s findfont %g scalefont setfont\n", ps_font_names[style], size);
    fonts->size  = size;
    fonts->style = style;
  }

  putchar('<');
  while (textlen > 0)
  {
    printf("%04x", fonts->chars[*text]);
    text ++;
    textlen --;
  }

  if (align == PS_CENTER)
    puts(">showcenter");
  else if (align == PS_RIGHT)
    puts(">showright");
  else
    puts(">showleft");
}


/*
 * End of "$Id$".
 */
