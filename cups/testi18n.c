/*
 * "$Id: testi18n.c,v 1.1.2.3 2003/01/07 18:26:30 mike Exp $"
 *
 *   Internationalization test for Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are
 *   the property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the
 *   file "LICENSE.txt" which should have been included with this file.
 *   If this file is missing or damaged please contact Easy Software
 *   Products at:
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
 *   main()           - Main entry for internationalization test module.
 *   print_synopsis() - Print program synopsis (help).
 *   print_utf8()     - Print UTF-8 string with (optional) message.
 *   print_utf16()    - Print UTF-16 string with (optional) message.
 *   print_utf32()    - Print UTF-32 string with (optional) message.
 *   test_transcode() - Test 'transcode.c' module.
 *   test_normalize() - Test 'normalize.c' module.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "language.h"
#include "string.h"
#include "transcode.h"
#include "normalize.h"


/*
 * Local Globals...
 */

static const char *program_synopsis[] =         /* Program help */
{
  "testi18n [-vh]",
  "         -v  verbose (print each called function and result)",
  "         -h  help (print this synopsis)",
  "",
  "'testi18n' is a utility to test CUPS internationalization",
  "Copyright 2002 by Easy Software Products.",
  NULL
};
static int      error_count = 0;                /* Total error count */


/*
 * Local functions...
 */

static void     print_synopsis(void);
static void     print_utf8(const char *msg, const cups_utf8_t *src);
static void     print_utf16(const char *msg, const cups_utf16_t *src);
static void     print_utf32(const char *msg, const cups_utf32_t *src);
static int      test_transcode(const int verbose);
static int      test_normalize(const int verbose);


/*
 * 'main()' - Main entry for internationalization test module.
 */
int                                     /* O - Exit code */
main(int  argc,                         /* I - Argument Count */
     char *argv[])                      /* I - Arguments */
{
  int           ai;                     /* Argument index */
  char          *ap;                    /* Argument pointer */
  int           verbose = 0;            /* Verbose flag */
  int           errors;                 /* Error count */


 /*
  * Check for switches...
  */

  for (ai = 1; ai < argc; ai ++)
  {
    ap = argv[ai];
    if (*ap != '-')
      break;
    for (ap ++; *ap != '\0'; ap ++)
    {
      switch (*ap)
      {
        case 'v':                       /* verbose */
          verbose = 1;
          break;
        case 'h':                       /* help */
          print_synopsis();
          return (0);
        default:
          break;
      }
    }
  }

 /*
  * Test all internationalization modules and functions...
  */

  errors = test_transcode(verbose);
  error_count += errors;
  printf("\ntesti18n: %d errors found in 'transcode.c'\n", errors);

  errors = test_normalize(verbose);
  error_count += errors;
  printf("\ntesti18n: %d errors found in 'normalize.c'\n", errors);

  return (error_count > 0);
}

/*
 * 'print_synopsis()' - Print program synopsis (help).
 */

static void
print_synopsis(void)
{
  int           i;                      /* Looping variable */
  const char    *line;                  /* Pointer to synopsis line */


  for (i = 0;; i ++)
  {
    line = program_synopsis[i];
    if (line == NULL)
      break;
    printf ("%s\n", line);
  }
  return;
}

/*
 * 'print_utf8()' - Print UTF-8 string with (optional) message.
 */

void
print_utf8(const char        *msg,      /* I - Message String */
           const cups_utf8_t *src)      /* I - UTF-8 Source String */
{
  if (msg != NULL)
    printf ("%s:", msg);
  for (; *src; src ++)
    printf (" %02x", (int) *src);
  printf ("\n");
  return;
}

/*
 * 'print_utf16()' - Print UTF-16 string with (optional) message.
 */

void
print_utf16(const char         *msg,    /* I - Message String */
            const cups_utf16_t *src)    /* I - UTF-16 Source String */
{
  if (msg != NULL)
    printf ("%s:", msg);
  for (; *src; src ++)
    printf (" %04x", (int) *src);
  printf ("\n");
  return;
}

/*
 * 'print_utf32()' - Print UTF-32 string with (optional) message.
 */

void
print_utf32(const char         *msg,    /* I - Message String */
            const cups_utf32_t *src)    /* I - UTF-32 Source String */
{
  if (msg != NULL)
    printf ("%s:", msg);
  for (; *src; src ++)
    printf (" %04x", (int) *src);
  printf ("\n");
  return;
}

/*
 * 'test_transcode()' - Test 'transcode.c' module.
 */

static int                              /* O - Zero or error count */
test_transcode(const int verbose)       /* I - Verbose flag */
{
  FILE          *fp;                    /* File pointer */
  int           count;                  /* File line counter */
  char          line[1024];             /* File line source string */
  int           len;                    /* Length (count) of string */
  char          legsrc[1024];           /* Legacy source string */
  char          legdest[1024];          /* Legacy destination string */
  cups_utf8_t   utf8latin[] =           /* UTF-8 Latin-1 source */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A != <A WITH DIAERESIS>." - use ISO 8859-1 */
  cups_utf8_t   utf8repla[] =           /* UTF-8 Latin-1 replacement */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <A WITH DIAERESIS>." */
  cups_utf8_t   utf8greek[] =           /* UTF-8 Greek source string */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xCE, 0x91, 0x2E, 0x00 };
    /* "A != <ALHPA>." - use ISO 8859-7 */
  cups_utf8_t   utf8japan[] =           /* UTF-8 Japanese source */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xEE, 0x9C, 0x80, 0x2E, 0x00 };
    /* "A != <PRIVATE U+E700>." - use Windows 932 or EUC-JP */
  cups_utf8_t   utf8taiwan[] =          /* UTF-8 Chinese source */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xE4, 0xB9, 0x82, 0x2E, 0x00 };
    /* "A != <CJK U+4E42>." - use Windows 950 (Big5) or EUC-TW */
  cups_utf8_t   utf8good[] =            /* UTF-8 good 16-bit source */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <A WITH DIAERESIS>." */
  cups_utf8_t   utf8bad[] =             /* UTF-8 bad 16-bit source */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xF8, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <...bad stuff...>." */
  cups_utf8_t   utf8dest[1024];         /* UTF-8 destination string */
  cups_utf16_t  utf16sur[] =            /* UTF-16 with surrogates */
    { 0xD800, 0xDC00, 0x20, 0x21, 0x3D, 0x20, 0xC4, 0x2E, 0x00 };
    /* "<Surrogate pair> != <A WITH DIAERESIS>." */
  cups_utf16_t  utf16src[1024];         /* UTF-16 source string */
  cups_utf16_t  utf16dest[1024];        /* UTF-16 destination string */
  cups_utf32_t  utf32src[1024];         /* UTF-32 source string */
  cups_utf32_t  utf32dest[1024];        /* UTF-32 destination string */
  cups_vmap_t   *vmap;                  /* VBCS charmap pointer */

 /*
  * Test with (inserted) and (deleted) leading BOM...
  */
  if (verbose)
  {
    printf("\ntesti18n: Testing 'transcode.c'...\n");
    printf(" testing with insert/delete leading BOM...\n");
  }

 /*
  * Test UTF-8 to UTF-32/EUC-JP on demo file...
  */
  if (verbose)
  {
    printf("\ntesti18n: Testing UTF-8 source 'utf8demo.txt'...\n");
    printf(" testing UTF-8 to UTF-32...\n");
    printf(" testing UTF-8 to EUC-JP...\n");
  }
  if ((fp = fopen("utf8demo.txt", "r")) == NULL)
    return (1);
  for (count = 0;;)
  {
    if (fgets(line, 1024, fp) == NULL)
      break;
    count ++;
    len = cupsUTF8ToUTF32(utf32dest, line, 1024);
    if (len < 0)
      printf(" error line: %d (UTF-8 to UTF-32)\n", count);
    len = cupsUTF8ToCharset(legdest, line, 1024, CUPS_EUC_JP);
    if (len < 0)
      printf(" error line: %d (UTF-8 to EUC-JP)\n", count);
  }
  fclose(fp);
  if (verbose)
    printf(" total lines: %d\n", count);

 /*
  * Test VBCS charmap load for EUC-JP...
  */
  if (verbose)
    printf("\ntesti18n: Loading VBCS charmap EUC-JP (Japanese)...\n");
  vmap = (cups_vmap_t *) cupsCharmapGet(CUPS_EUC_JP);
  if (vmap == NULL)
    return (1);
  if (verbose)
  {
    printf(" charcount: %d\n", vmap->charcount);
    printf(" widecount: %d\n", vmap->widecount);
  }

 /*
  * Test VBCS charmap load for EUC-TW...
  */
  if (verbose)
    printf("\ntesti18n: Loading VBCS charmap EUC-TW (Taiwan)...\n");
  vmap = (cups_vmap_t *) cupsCharmapGet(CUPS_EUC_TW);
  if (vmap == NULL)
    return (1);
  if (verbose)
  {
    printf(" charcount: %d\n", vmap->charcount);
    printf(" widecount: %d\n", vmap->widecount);
  }

 /*
  * Test UTF-8 to legacy charset (ISO 8859-1)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to ISO 8859-1 (Latin1)...\n");
  legdest[0] = 0;
  len = cupsUTF8ToCharset(legdest, utf8latin, 1024, CUPS_ISO8859_1);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8latin", utf8latin);
    print_utf8(" legdest  ", (cups_utf8_t *) legdest);
  }
  strcpy (legsrc, legdest);
  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_ISO8859_1);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8latin))
    return (1);
  if (memcmp (utf8latin, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 to Latin-1 (ISO 8859-1) with replacement...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to ISO 8859-1 w/ replace...\n");
  len = cupsUTF8ToCharset(legdest, utf8repla, 1024, CUPS_ISO8859_1);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8repla", utf8repla);
    print_utf8(" legdest  ", (cups_utf8_t *) legdest);
  }

 /*
  * Test UTF-8 to legacy charset (ISO 8859-7)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to ISO 8859-7 (Greek)...\n");
  legdest[0] = 0;
  len = cupsUTF8ToCharset(legdest, utf8greek, 1024, CUPS_ISO8859_7);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8greek", utf8greek);
    print_utf8(" legdest  ", (cups_utf8_t *) legdest);
  }
  strcpy (legsrc, legdest);
  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_ISO8859_7);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8greek))
    return (1);
  if (memcmp (utf8greek, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 to legacy charset (Windows 932)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to Windows 932 (Japanese)...\n");
  legdest[0] = 0;
  len = cupsUTF8ToCharset(legdest, utf8japan, 1024, CUPS_WINDOWS_932);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8japan", utf8japan);
    print_utf8(" legdest  ", (cups_utf8_t *) legdest);
  }
  strcpy (legsrc, legdest);
  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_WINDOWS_932);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8japan))
    return (1);
  if (memcmp (utf8japan, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 to legacy charset (EUC-JP)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to EUC-JP (Japanese)...\n");
  legdest[0] = 0;
  len = cupsUTF8ToCharset(legdest, utf8japan, 1024, CUPS_EUC_JP);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8japan", utf8japan);
    print_utf8(" legdest  ", (cups_utf8_t *) legdest);
  }
  strcpy (legsrc, legdest);
  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_EUC_JP);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8japan))
    return (1);
  if (memcmp (utf8japan, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 to legacy charset (Windows 950)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to Windows 950 (Chinese)...\n");
  legdest[0] = 0;
  len = cupsUTF8ToCharset(legdest, utf8taiwan, 1024, CUPS_WINDOWS_950);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8taiwan", utf8taiwan);
    print_utf8(" legdest   ", (cups_utf8_t *) legdest);
  }
  strcpy (legsrc, legdest);
  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_WINDOWS_950);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8taiwan))
    return (1);
  if (memcmp (utf8taiwan, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 to legacy charset (EUC-TW)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to EUC-TW (Chinese)...\n");
  legdest[0] = 0;
  len = cupsUTF8ToCharset(legdest, utf8taiwan, 1024, CUPS_EUC_TW);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8taiwan", utf8taiwan);
    print_utf8(" legdest   ", (cups_utf8_t *) legdest);
  }
  strcpy (legsrc, legdest);
  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_EUC_TW);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8taiwan))
    return (1);
  if (memcmp (utf8taiwan, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 (16-bit) to UTF-32 (w/ BOM)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to UTF-32 (w/ BOM)...\n");
  len = cupsUTF8ToUTF32(utf32dest, utf8good, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8good ", utf8good);
    print_utf32(" utf32dest", utf32dest);
  }
  memcpy (utf32src, utf32dest, (len + 1) * sizeof(cups_utf32_t));
  len = cupsUTF32ToUTF8(utf8dest, utf32src, 1024);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8good))
    return (1);
  if (memcmp (utf8good, utf8dest, len) != 0)
    return (1);

 /*
  * Test invalid UTF-8 (16-bit) to UTF-32 (w/ BOM)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 bad 16-bit source string...\n");
  len = cupsUTF8ToUTF32(utf32dest, utf8bad, 1024);
  if (len >= 0)
    return (1);
  if (verbose)
    print_utf8(" utf8bad  ", utf8bad);

 /*
  * Test UTF-8 (16-bit) to UTF-16 (w/ BOM)...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 to UTF-16 (w/ BOM)...\n");
  len = cupsUTF8ToUTF16(utf16dest, utf8good, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8good ", utf8good);
    print_utf16(" utf16dest", utf16dest);
  }
  memcpy (utf16src, utf16dest, (len + 1) * sizeof(cups_utf16_t));
  len = cupsUTF16ToUTF8(utf8dest, utf16src, 1024);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8good))
    return (1);
  if (memcmp (utf8good, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-16 to UTF-32 with surrogates...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-16 to UTF-32 w/ surrogates...\n");
  len = cupsUTF16ToUTF32(utf32dest, utf16sur, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf16(" utf16sur ", utf16sur);
    print_utf32(" utf32dest", utf32dest);
  }

 /*
  * Test cupsCharmapFlush()...
  */
  if (verbose)
    printf("\ntesti18n: Testing cupsCharmapFlush()...\n");
  cupsCharmapFlush();
  return (0);
}

/*
 * 'test_normalize()' - Test 'normalize.c' module.
 */

static int                              /* O - Zero or error count */
test_normalize(const int verbose)       /* I - Verbose flag */
{
  FILE          *fp;                    /* File pointer */
  int           count;                  /* File line counter */
  char          line[1024];             /* File line source string */
  int           len;                    /* Length (count) of string */
  int           diff;                   /* Difference of two strings */
  int           prop;                   /* Property of a character */
  int           i;                      /* Looping variable */
  cups_utf32_t  utf32char;              /* UTF-32 character */
  cups_utf8_t   utf8src[1024];          /* UTF-8 source string */
  cups_utf8_t   utf8dest[1024];         /* UTF-8 destination string */
  cups_utf16_t  utf16src[] =            /* UTF-16 non-normal source */
    { 0x0149, 0x20, 0x21, 0x3D, 0x20, 0xC4, 0x2E, 0x00 };
    /* "<SMALL N PRECEDED BY APOSTROPHE> != <A WITH DIAERESIS>." */
  cups_utf16_t  utf16dest[1024];        /* UTF-16 destination string */
  cups_utf32_t  utf32dest[1024];        /* UTF-32 destination string */

  if (verbose)
    printf("\ntesti18n: Testing 'normalize.c'...\n");

 /*
  * Test UTF-8 to NFKD/NFC/Properties on demo file...
  */
  if (verbose)
  {
    printf("\ntesti18n: Testing UTF-8 source 'utf8demo.txt'...\n");
    printf(" testing UTF-8 to NFKD...\n");
    printf(" testing UTF-8 to NFC...\n");
    printf(" testing UTF-8 to Character Properties...\n");
  }
  if ((fp = fopen("utf8demo.txt", "r")) == NULL)
    return (1);
  for (count = 0;;)
  {
    if (fgets(line, 1024, fp) == NULL)
      break;
    count ++;
    len = cupsUTF8Normalize(utf8dest, line, 1024, CUPS_NORM_NFKD);
    if (len < 0)
      printf(" error line: %d (UTF-8 to NFKD)\n", count);
    len = cupsUTF8Normalize(utf8dest, line, 1024, CUPS_NORM_NFC);
    if (len < 0)
      printf(" error line: %d (UTF-8 to NFC)\n", count);
    len = cupsUTF8ToUTF32(utf32dest, line, 1024);
    if (len < 0)
    {
      printf(" error line: %d (UTF-8 to UTF-32)\n", count);
      continue;
    }
    for (i = 0; i < len; i ++)
    {
      prop = cupsUTF32CharacterProperty(utf32dest[i],
                                        CUPS_PROP_GENERAL_CATEGORY);
      if (prop < 0)
        printf(" error line: %d (Prop - General Category)\n", count);
      prop = cupsUTF32CharacterProperty(utf32dest[i],
                                        CUPS_PROP_BIDI_CATEGORY);
      if (prop < 0)
        printf(" error line: %d (Prop - Bidi Category)\n", count);
      prop = cupsUTF32CharacterProperty(utf32dest[i],
                                        CUPS_PROP_COMBINING_CLASS);
      if (prop < 0)
        printf(" error line: %d (Prop - Combining Class)\n", count);
      prop = cupsUTF32CharacterProperty(utf32dest[i],
                                        CUPS_PROP_BREAK_CLASS);
      if (prop < 0)
        printf(" error line: %d (Prop - Break Class)\n", count);
    }
  }
  fclose(fp);
  if (verbose)
    printf(" total lines: %d\n", count);

 /*
  * Test UTF-8 normalization NFKD...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 normalization NFKD...\n");
  len = cupsUTF16ToUTF8(utf8dest, utf16src, 1024);
  if (len < 0)
    return (1);
  strcpy ((char *) utf8src, (char *) utf8dest);
  len = cupsUTF8Normalize(utf8dest, utf8src, 1024, CUPS_NORM_NFKD);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf16(" utf16src ", utf16src);
    print_utf16(" utf16dest", utf16dest);
  }

 /*
  * Test UTF-8 normalization NFD...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 normalization NFD...\n");
  len = cupsUTF8Normalize(utf8dest, utf8src, 1024, CUPS_NORM_NFD);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf16(" utf16src ", utf16src);
    print_utf16(" utf16dest", utf16dest);
  }

 /*
  * Test UTF-8 normalization NFC...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 normalization NFC...\n");
  len = cupsUTF8Normalize(utf8dest, utf8src, 1024, CUPS_NORM_NFC);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf16(" utf16src ", utf16src);
    print_utf16(" utf16dest", utf16dest);
  }

 /*
  * Test UTF-8 simple case folding...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 simple case folding...\n");
  len = cupsUTF8CaseFold(utf8dest, utf8src, 1024, CUPS_FOLD_SIMPLE);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf16(" utf16src ", utf16src);
    print_utf16(" utf16dest", utf16dest);
  }

 /*
  * Test UTF-8 full case folding...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 full case folding...\n");
  len = cupsUTF8CaseFold(utf8dest, utf8src, 1024, CUPS_FOLD_FULL);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 1024);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf16(" utf16src ", utf16src);
    print_utf16(" utf16dest", utf16dest);
  }

 /*
  * Test UTF-8 caseless comparison...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 caseless comparison..\n");
  diff = cupsUTF8CompareCaseless(utf8src, utf8dest);
  if (verbose)
    printf (" diff: %d\n", diff);
  if (verbose)
    printf("\ntesti18n: Testing UTF-8 identifier comparison..\n");
  diff = cupsUTF8CompareIdentifier(utf8src, utf8dest);
  if (verbose)
    printf (" diff: %d\n", diff);

 /*
  * Test UTF-32 character properties...
  */
  if (verbose)
    printf("\ntesti18n: Testing UTF-32 character properties..\n");
  utf32char = 0x02B0;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_GENERAL_CATEGORY);
  if (verbose)
    printf(" utf32char: %04lx  general category %d\n", utf32char, prop);
  utf32char = 0x0621;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_BIDI_CATEGORY);
  if (verbose)
    printf(" utf32char: %04lx  bidi category    %d\n", utf32char, prop);
  utf32char = 0x0308;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_COMBINING_CLASS);
  if (verbose)
    printf(" utf32char: %04lx  combining class  %d\n", utf32char, prop);
  utf32char = 0x0009;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_BREAK_CLASS);
  if (verbose)
    printf(" utf32char: %04lx  break class      %d\n", utf32char, prop);

 /*
  * Test cupsNormalizeMapsFlush()...
  */
  if (verbose)
    printf("\ntesti18n: Testing cupsNormalizeMapsFlush()...\n");
  cupsNormalizeMapsFlush();
  return (0);
}


/*
 * End of "$Id: testi18n.c,v 1.1.2.3 2003/01/07 18:26:30 mike Exp $"
 */
