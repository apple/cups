/*
 * "$Id: testi18n.c,v 1.1.2.1 2002/08/20 12:41:53 mike Exp $"
 *
 *   Internationalization test for Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are
 *   the property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the
 *   file "LICENSE.txt" which should have been included with this file.
 *   If this file is missing or damaged please contact Easy Software
 *   Products at:
 *
 *	 Attn: CUPS Licensing Information
 *	 Easy Software Products
 *	 44141 Airport View Drive, Suite 204
 *	 Hollywood, Maryland 20636-3111 USA
 *
 *	 Voice: (301) 373-9603
 *	 EMail: cups-info@cups.org
 *	   WWW: http://www.cups.org
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

static const char *program_synopsis[] =		/* Program help */
{
  "i18ntest [-v?]",
  "	    -v	verbose (print each called function and result)",
  "	    -?	help (print this synopsis)",
  "",
  "'i18ntest' is a utility to test CUPS internationalization",
  "Copyright (c) 2002 by Easy Software Products.",
  NULL
};
static int	error_count = 0;		/* Total error count */


/*
 * Local functions...
 */

static void	print_synopsis(void);
static void	print_utf8(const char *msg, const cups_utf8_t *src);
static void	print_utf16(const char *msg, const cups_utf16_t *src);
static void	print_utf32(const char *msg, const cups_utf32_t *src);
static int	test_transcode(const int verbose);
static int	test_normalize(const int verbose);


/*
 * 'main()' - Main entry for internationalization test module.
 */
int					/* O - Exit code */
main(int  argc,				/* I - Argument Count */
     char *argv[])			/* I - Arguments */
{
  int		ai;			/* Argument index */
  char		*ap;			/* Argument pointer */
  int		verbose = 0;		/* Verbose flag */
  int		errors;			/* Error count */


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
	case 'v':			/* verbose */
	  verbose = 1;
	  break;
	case '?':			/* help */
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
  printf("i18ntest: %d errors found in 'transcode.c'\n", errors);

  errors = test_normalize(verbose);
  error_count += errors;
  printf("i18ntest: %d errors found in 'normalize.c'\n", errors);

  return (error_count > 0);
}

/*
 * 'print_synopsis()' - Print program synopsis (help).
 */

static void
print_synopsis(void)
{
  int		i;			/* Looping variable */
  const char	*line;			/* Pointer to line in synopsis */


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
print_utf8(const char        *msg,	/* I - Message String */
           const cups_utf8_t *src)	/* I - UTF-8 Source String */
{
  if (msg != NULL)
    printf ("%s:", msg);
  for (; *src; src ++)
    printf (" %02X", (int) *src);
  printf ("\n");
  return;
}

/*
 * 'print_utf16()' - Print UTF-16 string with (optional) message.
 */

void
print_utf16(const char         *msg,	/* I - Message String */
            const cups_utf16_t *src)	/* I - UTF-16 Source String */
{
  if (msg != NULL)
    printf ("%s:", msg);
  for (; *src; src ++)
    printf (" %04X", (int) *src);
  printf ("\n");
  return;
}

/*
 * 'print_utf32()' - Print UTF-32 string with (optional) message.
 */

void
print_utf32(const char         *msg,	/* I - Message String */
            const cups_utf32_t *src)	/* I - UTF-32 Source String */
{
  if (msg != NULL)
    printf ("%s:", msg);
  for (; *src; src ++)
    printf (" %04X", (int) *src);
  printf ("\n");
  return;
}

/*
 * 'test_transcode()' - Test 'transcode.c' module.
 */

static int				/* O - Zero or error count */
test_transcode(const int verbose)	/* I - Verbose flag */
{
  int		len;			/* Length (count) of string */
  char		src[100];		/* Legacy source string */
  char		dest[100];		/* Legacy destination string */
  cups_utf8_t	latinsrc[100] =		/* UTF-8 Latin-1 source string */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A != <A WITH DIAERESIS>." - use ISO 8859-1 */
  cups_utf8_t	latinrep[100] =		/* UTF-8 Latin-1 with replacement */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <A WITH DIAERESIS>." */
  cups_utf8_t	greeksrc[100] =		/* UTF-8 Greek source string */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xCE, 0x91, 0x2E, 0x00 };
    /* "A != <ALHPA>." - use ISO 8859-7 */
  cups_utf8_t	utf8src[100] =		/* UTF-8 16-bit source string */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <A WITH DIAERESIS>." */
  cups_utf8_t	utf8bad[100] =		/* UTF-8 bad 16-bit source string */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xF8, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <...bad stuff...>." */
  cups_utf8_t	utf8dest[100];		/* UTF-8 destination string */
  cups_utf16_t	utf16sur[100] =		/* UTF-16 with surrogates string */
    { 0xD800, 0xDC00, 0x20, 0x21, 0x3D, 0x20, 0xC4, 0x2E, 0x00 };
    /* "<Surrogate pair> != <A WITH DIAERESIS>." */
  cups_utf16_t	utf16src[100];		/* UTF-16 source string */
  cups_utf16_t	utf16dest[100];		/* UTF-16 destination string */
  cups_utf32_t	utf32src[100];		/* UTF-32 source string */
  cups_utf32_t	utf32dest[100];		/* UTF-32 destination string */


  if (verbose)
    printf("i18ntest: Testing 'transcode.c'...\n");
  if (verbose)
    printf("i18ntest: Testing with strict UTF-8/16/32 rules...\n");
  _cupsStrictUTF8 = 1;
  _cupsStrictUTF16 = 1;
  _cupsStrictUTF32 = 1;

 /*
  * Test required (inserted) and supported (deleted) leading BOM...
  */

  if (verbose)
    printf("i18ntest: Testing with insert/delete leading BOM...\n");
  _cupsRequireBOM=1;
  _cupsSupportBOM=1;

 /*
  * Test UTF-8 to/from legacy charset (ISO 8859-1)...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to/from ISO 8859-1 (Latin1)...\n");
  dest[0] = 0;
  len = cupsUTF8ToCharset(dest, latinsrc, 100, CUPS_ISO8859_1);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" latinsrc", latinsrc);
    print_utf8(" dest	 ", (cups_utf8_t *) dest);
  }
  strcpy (src, dest);
  len = cupsCharsetToUTF8(utf8dest, src, 100, CUPS_ISO8859_1);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) latinsrc))
    return (1);
  if (memcmp (latinsrc, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 to Latin-1 (ISO 8859-1) with replacement...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to ISO 8859-1 w/ replacement...\n");
  len = cupsUTF8ToCharset(dest, latinrep, 100, CUPS_ISO8859_1);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" latinrep", latinrep);
    print_utf8(" dest	 ", (cups_utf8_t *) dest);
  }

 /*
  * Test UTF-8 to/from legacy charset (ISO 8859-7)...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to/from ISO 8859-7 (Greek)...\n");
  dest[0] = 0;
  len = cupsUTF8ToCharset(dest, greeksrc, 100, CUPS_ISO8859_7);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" greeksrc", latinsrc);
    print_utf8(" dest	 ", (cups_utf8_t *) dest);
  }
  strcpy (src, dest);
  len = cupsCharsetToUTF8(utf8dest, src, 100, CUPS_ISO8859_7);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) greeksrc))
    return (1);
  if (memcmp (greeksrc, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 (16-bit) to/from UTF-32 (w/ BOM)...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to/from UTF-32 (w/ BOM)...\n");
  len = cupsUTF8ToUTF32(utf32dest, utf8src, 100);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8src  ", utf8src);
    print_utf32(" utf32dest", utf32dest);
  }
  memcpy (utf32src, utf32dest, (len + 1) * sizeof(cups_utf32_t));
  len = cupsUTF32ToUTF8(utf8dest, utf32src, 100);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8src))
    return (1);
  if (memcmp (utf8src, utf8dest, len) != 0)
    return (1);

 /*
  * Test invalid UTF-8 (16-bit) to UTF-32 (w/ BOM)...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 bad 16-bit source string...\n");
  len = cupsUTF8ToUTF32(utf32dest, utf8bad, 100);
  if (len >= 0)
    return (1);
  if (verbose)
    print_utf8(" utf8bad  ", utf8bad);

 /*
  * Test UTF-8 (16-bit) to/from UTF-16 (w/ BOM)...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to/from UTF-16 (w/ BOM)...\n");
  len = cupsUTF8ToUTF16(utf16dest, utf8src, 100);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8src  ", utf8src);
    print_utf16(" utf16dest", utf16dest);
  }
  memcpy (utf16src, utf16dest, (len + 1) * sizeof(cups_utf16_t));
  len = cupsUTF16ToUTF8(utf8dest, utf16src, 100);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8src))
    return (1);
  if (memcmp (utf8src, utf8dest, len) != 0)
    return (1);

 /*
  * Test without using leading BOM...
  */
  if (verbose)
    printf("i18ntest: Testing without insert/delete leading BOM...\n");
  _cupsRequireBOM=0;
  _cupsSupportBOM=0;

 /*
  * Test UTF-8 (16-bit) to/from UTF-32 without leading BOM...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to/from UTF-32 (w/out BOM)...\n");
  len = cupsUTF8ToUTF32(utf32dest, utf8src, 100);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8src  ", utf8src);
    print_utf32(" utf32dest", utf32dest);
  }
  memcpy (utf32src, utf32dest, (len + 1) * sizeof(cups_utf32_t));
  len = cupsUTF32ToUTF8(utf8dest, utf32src, 100);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8src))
    return (1);
  if (memcmp (utf8src, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-8 (16-bit) to/from UTF-16 (w/out BOM)...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 to/from UTF-16 (w/out BOM)...\n");
  len = cupsUTF8ToUTF16(utf16dest, utf8src, 100);
  if (len < 0)
    return (1);
  if (verbose)
  {
    print_utf8(" utf8src  ", utf8src);
    print_utf16(" utf16dest", utf16dest);
  }
  memcpy (utf16src, utf16dest, (len + 1) * sizeof(cups_utf16_t));
  len = cupsUTF16ToUTF8(utf8dest, utf16src, 100);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8src))
    return (1);
  if (memcmp (utf8src, utf8dest, len) != 0)
    return (1);

 /*
  * Test UTF-16 to UTF-32 with surrogates...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-16 to UTF-32 w/ surrogates...\n");
  len = cupsUTF16ToUTF32(utf32dest, utf16sur, 100);
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
    printf("i18ntest: Testing cupsCharmapFlush()...\n");
  cupsCharmapFlush();
  return (0);
}

/*
 * 'test_normalize()' - Test 'normalize.c' module.
 */

static int				/* O - Zero or error count */
test_normalize(const int verbose)	/* I - Verbose flag */
{
  int		len;			/* Length (count) of string */
  int		diff;			/* Difference of two strings */
  int		prop;			/* Property of a character */
  cups_utf32_t	utf32char;		/* UTF-32 character */
  cups_utf8_t	utf8src[100];		/* UTF-8 source string */
  cups_utf8_t	utf8dest[100];		/* UTF-8 destination string */
  cups_utf16_t	utf16src[100] =		/* UTF-16 non-normal source string */
    { 0x0149, 0x20, 0x21, 0x3D, 0x20, 0xC4, 0x2E, 0x00 };
    /* "<SMALL N PRECEDED BY APOSTROPHE> != <A WITH DIAERESIS>." */
  cups_utf16_t	utf16dest[100];		/* UTF-16 destination string */

  if (verbose)
    printf("i18ntest: Testing 'normalize.c'...\n");

 /*
  * Test UTF-8 normalization NFKD...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-8 normalization NFKD...\n");
  len = cupsUTF16ToUTF8(utf8dest, utf16src, 100);
  if (len < 0)
    return (1);
  strcpy ((char *) utf8src, (char *) utf8dest);
  len = cupsUTF8Normalize(utf8dest, utf8src, 100, CUPS_NORM_NFKD);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 100);
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
    printf("i18ntest: Testing UTF-8 normalization NFD...\n");
  len = cupsUTF8Normalize(utf8dest, utf8src, 100, CUPS_NORM_NFD);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 100);
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
    printf("i18ntest: Testing UTF-8 normalization NFC...\n");
  len = cupsUTF8Normalize(utf8dest, utf8src, 100, CUPS_NORM_NFC);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 100);
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
    printf("i18ntest: Testing UTF-8 simple case folding...\n");
  len = cupsUTF8CaseFold(utf8dest, utf8src, 100, CUPS_FOLD_SIMPLE);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 100);
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
    printf("i18ntest: Testing UTF-8 full case folding...\n");
  len = cupsUTF8CaseFold(utf8dest, utf8src, 100, CUPS_FOLD_FULL);
  if (len < 0)
    return (1);
  len = cupsUTF8ToUTF16(utf16dest, utf8dest, 100);
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
    printf("i18ntest: Testing UTF-8 caseless comparison..\n");
  diff = cupsUTF8CompareCaseless(utf8src, utf8dest);
  if (verbose)
    printf (" diff: %d\n", diff);
  if (verbose)
    printf("i18ntest: Testing UTF-8 identifier comparison..\n");
  diff = cupsUTF8CompareIdentifier(utf8src, utf8dest);
  if (verbose)
    printf (" diff: %d\n", diff);

 /*
  * Test UTF-32 character properties...
  */
  if (verbose)
    printf("i18ntest: Testing UTF-32 character properties..\n");
  utf32char = 0x02B0;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_GENERAL_CATEGORY);
  if (verbose)
    printf(" utf32char: %04lX  general category %d\n", utf32char, prop);
  utf32char = 0x0621;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_BIDI_CATEGORY);
  if (verbose)
    printf(" utf32char: %04lX  bidi category %d\n", utf32char, prop);
  utf32char = 0x0308;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_COMBINING_CLASS);
  if (verbose)
    printf(" utf32char: %04lX  combining class %d\n", utf32char, prop);
  utf32char = 0x0009;
  prop = cupsUTF32CharacterProperty (utf32char,
    CUPS_PROP_BREAK_CLASS);
  if (verbose)
    printf(" utf32char: %04lX  break class %d\n", utf32char, prop);

 /*
  * Test cupsNormalizeMapsFlush()...
  */
  if (verbose)
    printf("i18ntest: Testing cupsNormalizeMapsFlush()...\n");
  cupsNormalizeMapsFlush();
  return (0);
}


/*
 * End of "$Id: testi18n.c,v 1.1.2.1 2002/08/20 12:41:53 mike Exp $"
 */
