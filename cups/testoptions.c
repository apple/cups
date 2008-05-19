/*
 * "$Id$"
 *
 *   Option test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2008 by Apple Inc.
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
 *   main() - Test option processing functions.
 */

/*
 * Include necessary headers...
 */

#include "string.h"
#include "cups.h"


/*
 * 'main()' - Test option processing functions.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		status = 0,		/* Exit status */
		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  const char	*value;			/* Value of an option */


  if (argc == 1)
  {
   /*
    * cupsParseOptions()
    */

    fputs("cupsParseOptions: ", stdout);

    num_options = cupsParseOptions("foo=1234 "
				   "bar=\"One Fish\",\"Two Fish\",\"Red Fish\","
				   "\"Blue Fish\" "
				   "baz={param1=1 param2=2} "
				   "foobar=FOO\\ BAR "
				   "barfoo=barfoo "
				   "barfoo=\"\'BAR FOO\'\"", 0, &options);

    if (num_options != 5)
    {
      printf("FAIL (num_options=%d, expected 5)\n", num_options);
      status ++;
    }
    else if ((value = cupsGetOption("foo", num_options, options)) == NULL ||
	     strcmp(value, "1234"))
    {
      printf("FAIL (foo=\"%s\", expected \"1234\")\n", value);
      status ++;
    }
    else if ((value = cupsGetOption("bar", num_options, options)) == NULL ||
	     strcmp(value, "One Fish,Two Fish,Red Fish,Blue Fish"))
    {
      printf("FAIL (bar=\"%s\", expected \"One Fish,Two Fish,Red Fish,Blue "
	     "Fish\")\n", value);
      status ++;
    }
    else if ((value = cupsGetOption("baz", num_options, options)) == NULL ||
	     strcmp(value, "{param1=1 param2=2}"))
    {
      printf("FAIL (baz=\"%s\", expected \"{param1=1 param2=2}\")\n", value);
      status ++;
    }
    else if ((value = cupsGetOption("foobar", num_options, options)) == NULL ||
	     strcmp(value, "FOO BAR"))
    {
      printf("FAIL (foobar=\"%s\", expected \"FOO BAR\")\n", value);
      status ++;
    }
    else if ((value = cupsGetOption("barfoo", num_options, options)) == NULL ||
	     strcmp(value, "\'BAR FOO\'"))
    {
      printf("FAIL (barfoo=\"%s\", expected \"\'BAR FOO\'\")\n", value);
      status ++;
    }
    else
      puts("PASS");
  }
  else
  {
    int			i;		/* Looping var */
    cups_option_t	*option;	/* Current option */


    num_options = cupsParseOptions(argv[1], 0, &options);

    for (i = 0, option = options; i < num_options; i ++, option ++)
      printf("options[%d].name=\"%s\", value=\"%s\"\n", i, option->name,
             option->value);
  }

  exit (status);
}


/*
 * End of "$Id$".
 */
