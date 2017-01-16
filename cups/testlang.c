/*
 * Localization test program for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
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
  printf("No       = \"%s\"\n", _cupsLangString(language, "No"));
  printf("Yes      = \"%s\"\n", _cupsLangString(language, "Yes"));

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

      ppdClose(ppd);
    }
  }

  return (errors > 0);
}
