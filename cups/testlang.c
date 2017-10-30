/*
 * Localization test program for CUPS.
 *
 * Copyright 2007-2017 by Apple Inc.
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

#include "cups-private.h"
#include "ppd-private.h"
#ifdef __APPLE__
#  include <CoreFoundation/CoreFoundation.h>
#endif /* __APPLE__ */


/*
 * Local functions...
 */

static int  test_string(cups_lang_t *language, const char *msgid);


/*
 * 'main()' - Load the specified language and show the strings for yes and no.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  int			errors = 0;	/* Number of errors */
  cups_lang_t		*language;	/* Message catalog */
  cups_lang_t		*language2;	/* Message catalog */
  struct lconv		*loc;		/* Locale data */
  char			buffer[1024];	/* String buffer */
  double		number;		/* Number */
  static const char * const tests[] =	/* Test strings */
  {
    "1",
    "-1",
    "3",
    "5.125"
  };


  if (argc == 1)
  {
    language  = cupsLangDefault();
    language2 = cupsLangDefault();
  }
  else
  {
    language  = cupsLangGet(argv[1]);
    language2 = cupsLangGet(argv[1]);

    setenv("LANG", argv[1], 1);
    setenv("SOFTWARE", "CUPS/" CUPS_SVERSION, 1);
  }

  _cupsSetLocale(argv);

  if (language != language2)
  {
    errors ++;

    puts("**** ERROR: Language cache did not work! ****");
    puts("First result from cupsLangGet:");
  }

  printf("Language = \"%s\"\n", language->language);
  printf("Encoding = \"%s\"\n", _cupsEncodingName(language->encoding));

  errors += test_string(language, "No");
  errors += test_string(language, "Yes");

  if (language != language2)
  {
    puts("Second result from cupsLangGet:");

    printf("Language = \"%s\"\n", language2->language);
    printf("Encoding = \"%s\"\n", _cupsEncodingName(language2->encoding));
    printf("No       = \"%s\"\n", _cupsLangString(language2, "No"));
    printf("Yes      = \"%s\"\n", _cupsLangString(language2, "Yes"));
  }

  loc = localeconv();

  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i ++)
  {
    number = _cupsStrScand(tests[i], NULL, loc);

    printf("_cupsStrScand(\"%s\") number=%f\n", tests[i], number);

    _cupsStrFormatd(buffer, buffer + sizeof(buffer), number, loc);

    printf("_cupsStrFormatd(%f) buffer=\"%s\"\n", number, buffer);

    if (strcmp(buffer, tests[i]))
    {
      errors ++;
      puts("**** ERROR: Bad formatted number! ****");
    }
  }

  if (argc == 3)
  {
    ppd_file_t		*ppd;		/* PPD file */
    ppd_option_t	*option;	/* PageSize option */
    ppd_choice_t	*choice;	/* PageSize/Letter choice */

    if ((ppd = ppdOpenFile(argv[2])) == NULL)
    {
      printf("Unable to open PPD file \"%s\".\n", argv[2]);
      errors ++;
    }
    else
    {
      ppdLocalize(ppd);

      if ((option = ppdFindOption(ppd, "PageSize")) == NULL)
      {
        puts("No PageSize option.");
        errors ++;
      }
      else
      {
        printf("PageSize: %s\n", option->text);

        if ((choice = ppdFindChoice(option, "Letter")) == NULL)
        {
	  puts("No Letter PageSize choice.");
	  errors ++;
        }
        else
        {
	  printf("Letter: %s\n", choice->text);
        }
      }

      printf("media-empty: %s\n", ppdLocalizeIPPReason(ppd, "media-empty", NULL, buffer, sizeof(buffer)));

      ppdClose(ppd);
    }
  }
#ifdef __APPLE__
  else
  {
   /*
    * Test all possible language IDs for compatibility with _cupsAppleLocale...
    */

    CFIndex     j,                      /* Looping var */
                num_locales;            /* Number of locales */
    CFArrayRef  locales;                /* Locales */
    CFStringRef locale_id,              /* Current locale ID */
                language_id;            /* Current language ID */
    char        locale_str[256],        /* Locale ID C string */
                language_str[256],      /* Language ID C string */
                *bufptr;                /* Pointer to ".UTF-8" in POSIX locale */
    size_t      buflen;                 /* Length of POSIX locale */
#  if TEST_COUNTRY_CODES
    CFIndex     k,                      /* Looping var */
                num_country_codes;      /* Number of country codes */
    CFArrayRef  country_codes;          /* Country codes */
    CFStringRef country_code,           /* Current country code */
                temp_id;                /* Temporary language ID */
    char        country_str[256];       /* Country code C string */
#  endif /* TEST_COUNTRY_CODES */

    locales     = CFLocaleCopyAvailableLocaleIdentifiers();
    num_locales = CFArrayGetCount(locales);

#  if TEST_COUNTRY_CODES
    country_codes     = CFLocaleCopyISOCountryCodes();
    num_country_codes = CFArrayGetCount(country_codes);
#  endif /* TEST_COUNTRY_CODES */

    printf("%d locales are available:\n", (int)num_locales);

    for (j = 0; j < num_locales; j ++)
    {
      locale_id   = CFArrayGetValueAtIndex(locales, j);
      language_id = CFLocaleCreateCanonicalLanguageIdentifierFromString(kCFAllocatorDefault, locale_id);

      if (!locale_id || !CFStringGetCString(locale_id, locale_str, (CFIndex)sizeof(locale_str), kCFStringEncodingASCII))
      {
        printf("%d: FAIL (unable to get locale ID string)\n", (int)j + 1);
        errors ++;
        continue;
      }

      if (!language_id || !CFStringGetCString(language_id, language_str, (CFIndex)sizeof(language_str), kCFStringEncodingASCII))
      {
        printf("%d %s: FAIL (unable to get language ID string)\n", (int)j + 1, locale_str);
        errors ++;
        continue;
      }

      if (!_cupsAppleLocale(language_id, buffer, sizeof(buffer)))
      {
        printf("%d %s(%s): FAIL (unable to convert language ID string to POSIX locale)\n", (int)j + 1, locale_str, language_str);
        errors ++;
        continue;
      }

      if ((bufptr = strstr(buffer, ".UTF-8")) != NULL)
        buflen = (size_t)(bufptr - buffer);
      else
        buflen = strlen(buffer);

      if ((language = cupsLangGet(buffer)) == NULL)
      {
        printf("%d %s(%s): FAIL (unable to load POSIX locale \"%s\")\n", (int)j + 1, locale_str, language_str, buffer);
        errors ++;
        continue;
      }

      if (strncasecmp(language->language, buffer, buflen))
      {
        printf("%d %s(%s): FAIL (unable to load POSIX locale \"%s\", got \"%s\")\n", (int)j + 1, locale_str, language_str, buffer, language->language);
        errors ++;
        continue;
      }

      printf("%d %s(%s): PASS (POSIX locale is \"%s\")\n", (int)j + 1, locale_str, language_str, buffer);
    }

    CFRelease(locales);

#  if TEST_COUNTRY_CODES
    CFRelease(country_codes);
#  endif /* TEST_COUNTRY_CODES */
  }
#endif /* __APPLE__ */

  if (errors == 0)
    puts("ALL TESTS PASSED");

  return (errors > 0);
}


/*
 * 'test_string()' - Test the localization of a string.
 */

static int                            /* O - 1 on failure, 0 on success */
test_string(cups_lang_t *language,    /* I - Language */
            const char  *msgid)       /* I - Message */
{
  const char  *msgstr;                /* Localized string */


 /*
  * Get the localized string and then see if we got what we expected.
  *
  * For the POSIX locale, the string pointers should be the same.
  * For any other locale, the string pointers should be different.
  */

  msgstr = _cupsLangString(language, msgid);
  if (strcmp(language->language, "C") && msgid == msgstr)
  {
    printf("%-8s = \"%s\" (FAIL - no message catalog loaded)\n", msgid, msgstr);
    return (1);
  }
  else if (!strcmp(language->language, "C") && msgid != msgstr)
  {
    printf("%-8s = \"%s\" (FAIL - POSIX locale is localized)\n", msgid, msgstr);
    return (1);
  }

  printf("%-8s = \"%s\" (PASS)\n", msgid, msgstr);

  return (0);
}

