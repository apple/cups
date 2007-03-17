/*
 * "$Id$"
 *
 *   Localization test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main() - Load the specified language and show the strings for yes and no.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include "i18n.h"
#include "string.h"


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


  _cupsSetLocale(argv);

  if (argc == 1)
  {
    language  = cupsLangDefault();
    language2 = cupsLangDefault();
  }
  else
  {
    language  = cupsLangGet(argv[1]);
    language2 = cupsLangGet(argv[1]);
  }

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

  return (errors > 0);
}


/*
 * End of "$Id$".
 */
