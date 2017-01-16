/*
 * Internationalization test for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "string-private.h"
#include "language-private.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


/*
 * Local globals...
 */

static const char * const lang_encodings[] =
			{		/* Encoding strings */
			  "us-ascii",		"iso-8859-1",
			  "iso-8859-2",		"iso-8859-3",
			  "iso-8859-4",		"iso-8859-5",
			  "iso-8859-6",		"iso-8859-7",
			  "iso-8859-8",		"iso-8859-9",
			  "iso-8859-10",	"utf-8",
			  "iso-8859-13",	"iso-8859-14",
			  "iso-8859-15",	"windows-874",
			  "windows-1250",	"windows-1251",
			  "windows-1252",	"windows-1253",
			  "windows-1254",	"windows-1255",
			  "windows-1256",	"windows-1257",
			  "windows-1258",	"koi8-r",
			  "koi8-u",		"iso-8859-11",
			  "iso-8859-16",	"mac-roman",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "windows-932",	"windows-936",
			  "windows-949",	"windows-950",
			  "windows-1361",	"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "unknown",		"unknown",
			  "euc-cn",		"euc-jp",
			  "euc-kr",		"euc-tw",
			  "jis-x0213"
			};


/*
 * Local functions...
 */

static void	print_utf8(const char *msg, const cups_utf8_t *src);


/*
 * 'main()' - Main entry for internationalization test module.
 */

int					/* O - Exit code */
main(int  argc,				/* I - Argument Count */
     char *argv[])			/* I - Arguments */
{
  FILE		*fp;			/* File pointer */
  int		count;			/* File line counter */
  int		status,			/* Status of current test */
		errors;			/* Error count */
  char		line[1024];		/* File line source string */
  int		len;			/* Length (count) of string */
  char		legsrc[1024],		/* Legacy source string */
		legdest[1024],		/* Legacy destination string */
		*legptr;		/* Pointer into legacy string */
  cups_utf8_t	utf8latin[] =		/* UTF-8 Latin-1 source */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A != <A WITH DIAERESIS>." - use ISO 8859-1 */
  cups_utf8_t	utf8repla[] =		/* UTF-8 Latin-1 replacement */
    { 0x41, 0x20, 0xE2, 0x89, 0xA2, 0x20, 0xC3, 0x84, 0x2E, 0x00 };
    /* "A <NOT IDENTICAL TO> <A WITH DIAERESIS>." */
  cups_utf8_t	utf8greek[] =		/* UTF-8 Greek source string */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xCE, 0x91, 0x2E, 0x00 };
    /* "A != <ALPHA>." - use ISO 8859-7 */
  cups_utf8_t	utf8japan[] =		/* UTF-8 Japanese source */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xEE, 0x9C, 0x80, 0x2E, 0x00 };
    /* "A != <PRIVATE U+E700>." - use Windows 932 or EUC-JP */
  cups_utf8_t	utf8taiwan[] =		/* UTF-8 Chinese source */
    { 0x41, 0x20, 0x21, 0x3D, 0x20, 0xE4, 0xB9, 0x82, 0x2E, 0x00 };
    /* "A != <CJK U+4E42>." - use Windows 950 (Big5) or EUC-TW */
  cups_utf8_t	utf8dest[1024];		/* UTF-8 destination string */
  cups_utf32_t	utf32dest[1024];	/* UTF-32 destination string */


  if (argc > 1)
  {
    int			i;		/* Looping var */
    cups_encoding_t	encoding;	/* Source encoding */


    if (argc != 3)
    {
      puts("Usage: ./testi18n [filename charset]");
      return (1);
    }

    if ((fp = fopen(argv[1], "rb")) == NULL)
    {
      perror(argv[1]);
      return (1);
    }

    for (i = 0, encoding = CUPS_AUTO_ENCODING;
         i < (int)(sizeof(lang_encodings) / sizeof(lang_encodings[0]));
	 i ++)
      if (!_cups_strcasecmp(lang_encodings[i], argv[2]))
      {
        encoding = (cups_encoding_t)i;
	break;
      }

    if (encoding == CUPS_AUTO_ENCODING)
    {
      fprintf(stderr, "%s: Unknown character set!\n", argv[2]);
      return (1);
    }

    while (fgets(line, sizeof(line), fp))
    {
      if (cupsCharsetToUTF8(utf8dest, line, sizeof(utf8dest), encoding) < 0)
      {
        fprintf(stderr, "%s: Unable to convert line: %s", argv[1], line);
	return (1);
      }

      fputs((char *)utf8dest, stdout);
    }

    fclose(fp);
    return (0);
  }

 /*
  * Start with some conversion tests from a UTF-8 test file.
  */

  errors = 0;

  if ((fp = fopen("utf8demo.txt", "rb")) == NULL)
  {
    perror("utf8demo.txt");
    return (1);
  }

 /*
  * cupsUTF8ToUTF32
  */

  fputs("cupsUTF8ToUTF32 of utfdemo.txt: ", stdout);

  for (count = 0, status = 0; fgets(line, sizeof(line), fp);)
  {
    count ++;

    if (cupsUTF8ToUTF32(utf32dest, (cups_utf8_t *)line, 1024) < 0)
    {
      printf("FAIL (UTF-8 to UTF-32 on line %d)\n", count);
      errors ++;
      status = 1;
      break;
    }
  }

  if (!status)
    puts("PASS");

 /*
  * cupsUTF8ToCharset(CUPS_EUC_JP)
  */

  fputs("cupsUTF8ToCharset(CUPS_EUC_JP) of utfdemo.txt: ", stdout);

  rewind(fp);

  for (count = 0, status = 0; fgets(line, sizeof(line), fp);)
  {
    count ++;

    len = cupsUTF8ToCharset(legdest, (cups_utf8_t *)line, 1024, CUPS_EUC_JP);
    if (len < 0)
    {
      printf("FAIL (UTF-8 to EUC-JP on line %d)\n", count);
      errors ++;
      status = 1;
      break;
    }
  }

  if (!status)
    puts("PASS");

  fclose(fp);

 /*
  * Test UTF-8 to legacy charset (ISO 8859-1)...
  */

  fputs("cupsUTF8ToCharset(CUPS_ISO8859_1): ", stdout);

  legdest[0] = 0;

  len = cupsUTF8ToCharset(legdest, utf8latin, 1024, CUPS_ISO8859_1);
  if (len < 0)
  {
    printf("FAIL (len=%d)\n", len);
    errors ++;
  }
  else
    puts("PASS");

 /*
  * cupsCharsetToUTF8
  */

  fputs("cupsCharsetToUTF8(CUPS_ISO8859_1): ", stdout);

  strlcpy(legsrc, legdest, sizeof(legsrc));

  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_ISO8859_1);
  if ((size_t)len != strlen((char *)utf8latin))
  {
    printf("FAIL (len=%d, expected %d)\n", len, (int)strlen((char *)utf8latin));
    print_utf8("    utf8latin", utf8latin);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (memcmp(utf8latin, utf8dest, (size_t)len))
  {
    puts("FAIL (results do not match)");
    print_utf8("    utf8latin", utf8latin);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (cupsUTF8ToCharset(legdest, utf8repla, 1024, CUPS_ISO8859_1) < 0)
  {
    puts("FAIL (replacement characters do not work!)");
    errors ++;
  }
  else
    puts("PASS");

 /*
  * Test UTF-8 to/from legacy charset (ISO 8859-7)...
  */

  fputs("cupsUTF8ToCharset(CUPS_ISO8859_7): ", stdout);

  if (cupsUTF8ToCharset(legdest, utf8greek, 1024, CUPS_ISO8859_7) < 0)
  {
    puts("FAIL");
    errors ++;
  }
  else
  {
    for (legptr = legdest; *legptr && *legptr != '?'; legptr ++);

    if (*legptr)
    {
      puts("FAIL (unknown character)");
      errors ++;
    }
    else
      puts("PASS");
  }

  fputs("cupsCharsetToUTF8(CUPS_ISO8859_7): ", stdout);

  strlcpy(legsrc, legdest, sizeof(legsrc));

  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_ISO8859_7);
  if ((size_t)len != strlen((char *)utf8greek))
  {
    printf("FAIL (len=%d, expected %d)\n", len, (int)strlen((char *)utf8greek));
    print_utf8("    utf8greek", utf8greek);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (memcmp(utf8greek, utf8dest, (size_t)len))
  {
    puts("FAIL (results do not match)");
    print_utf8("    utf8greek", utf8greek);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else
    puts("PASS");

 /*
  * Test UTF-8 to/from legacy charset (Windows 932)...
  */

  fputs("cupsUTF8ToCharset(CUPS_WINDOWS_932): ", stdout);

  if (cupsUTF8ToCharset(legdest, utf8japan, 1024, CUPS_WINDOWS_932) < 0)
  {
    puts("FAIL");
    errors ++;
  }
  else
  {
    for (legptr = legdest; *legptr && *legptr != '?'; legptr ++);

    if (*legptr)
    {
      puts("FAIL (unknown character)");
      errors ++;
    }
    else
      puts("PASS");
  }

  fputs("cupsCharsetToUTF8(CUPS_WINDOWS_932): ", stdout);

  strlcpy(legsrc, legdest, sizeof(legsrc));

  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_WINDOWS_932);
  if ((size_t)len != strlen((char *)utf8japan))
  {
    printf("FAIL (len=%d, expected %d)\n", len, (int)strlen((char *)utf8japan));
    print_utf8("    utf8japan", utf8japan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (memcmp(utf8japan, utf8dest, (size_t)len))
  {
    puts("FAIL (results do not match)");
    print_utf8("    utf8japan", utf8japan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else
    puts("PASS");

 /*
  * Test UTF-8 to/from legacy charset (EUC-JP)...
  */

  fputs("cupsUTF8ToCharset(CUPS_EUC_JP): ", stdout);

  if (cupsUTF8ToCharset(legdest, utf8japan, 1024, CUPS_EUC_JP) < 0)
  {
    puts("FAIL");
    errors ++;
  }
  else
  {
    for (legptr = legdest; *legptr && *legptr != '?'; legptr ++);

    if (*legptr)
    {
      puts("FAIL (unknown character)");
      errors ++;
    }
    else
      puts("PASS");
  }

#ifndef __linux
  fputs("cupsCharsetToUTF8(CUPS_EUC_JP): ", stdout);

  strlcpy(legsrc, legdest, sizeof(legsrc));

  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_EUC_JP);
  if ((size_t)len != strlen((char *)utf8japan))
  {
    printf("FAIL (len=%d, expected %d)\n", len, (int)strlen((char *)utf8japan));
    print_utf8("    utf8japan", utf8japan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (memcmp(utf8japan, utf8dest, (size_t)len))
  {
    puts("FAIL (results do not match)");
    print_utf8("    utf8japan", utf8japan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else
    puts("PASS");
#endif /* !__linux */

 /*
  * Test UTF-8 to/from legacy charset (Windows 950)...
  */

  fputs("cupsUTF8ToCharset(CUPS_WINDOWS_950): ", stdout);

  if (cupsUTF8ToCharset(legdest, utf8taiwan, 1024, CUPS_WINDOWS_950) < 0)
  {
    puts("FAIL");
    errors ++;
  }
  else
  {
    for (legptr = legdest; *legptr && *legptr != '?'; legptr ++);

    if (*legptr)
    {
      puts("FAIL (unknown character)");
      errors ++;
    }
    else
      puts("PASS");
  }

  fputs("cupsCharsetToUTF8(CUPS_WINDOWS_950): ", stdout);

  strlcpy(legsrc, legdest, sizeof(legsrc));

  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_WINDOWS_950);
  if ((size_t)len != strlen((char *)utf8taiwan))
  {
    printf("FAIL (len=%d, expected %d)\n", len, (int)strlen((char *)utf8taiwan));
    print_utf8("    utf8taiwan", utf8taiwan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (memcmp(utf8taiwan, utf8dest, (size_t)len))
  {
    puts("FAIL (results do not match)");
    print_utf8("    utf8taiwan", utf8taiwan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else
    puts("PASS");

 /*
  * Test UTF-8 to/from legacy charset (EUC-TW)...
  */

  fputs("cupsUTF8ToCharset(CUPS_EUC_TW): ", stdout);

  if (cupsUTF8ToCharset(legdest, utf8taiwan, 1024, CUPS_EUC_TW) < 0)
  {
    puts("FAIL");
    errors ++;
  }
  else
  {
    for (legptr = legdest; *legptr && *legptr != '?'; legptr ++);

    if (*legptr)
    {
      puts("FAIL (unknown character)");
      errors ++;
    }
    else
      puts("PASS");
  }

  fputs("cupsCharsetToUTF8(CUPS_EUC_TW): ", stdout);

  strlcpy(legsrc, legdest, sizeof(legsrc));

  len = cupsCharsetToUTF8(utf8dest, legsrc, 1024, CUPS_EUC_TW);
  if ((size_t)len != strlen((char *)utf8taiwan))
  {
    printf("FAIL (len=%d, expected %d)\n", len, (int)strlen((char *)utf8taiwan));
    print_utf8("    utf8taiwan", utf8taiwan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else if (memcmp(utf8taiwan, utf8dest, (size_t)len))
  {
    puts("FAIL (results do not match)");
    print_utf8("    utf8taiwan", utf8taiwan);
    print_utf8("    utf8dest", utf8dest);
    errors ++;
  }
  else
    puts("PASS");

#if 0
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
  memcpy(utf32src, utf32dest, (len + 1) * sizeof(cups_utf32_t));
  len = cupsUTF32ToUTF8(utf8dest, utf32src, 1024);
  if (len < 0)
    return (1);
  if (len != strlen ((char *) utf8good))
    return (1);
  if (memcmp(utf8good, utf8dest, len) != 0)
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
  * Test _cupsCharmapFlush()...
  */
  if (verbose)
    printf("\ntesti18n: Testing _cupsCharmapFlush()...\n");
  _cupsCharmapFlush();
  return (0);
#endif /* 0 */

  return (errors > 0);
}


/*
 * 'print_utf8()' - Print UTF-8 string with (optional) message.
 */

static void
print_utf8(const char	     *msg,	/* I - Message String */
	   const cups_utf8_t *src)	/* I - UTF-8 Source String */
{
  const char	*prefix;		/* Prefix string */


  if (msg)
    printf("%s:", msg);

  for (prefix = " "; *src; src ++)
  {
    printf("%s%02x", prefix, *src);

    if ((src[0] & 0x80) && (src[1] & 0x80))
      prefix = "";
    else
      prefix = " ";
  }

  putchar('\n');
}
