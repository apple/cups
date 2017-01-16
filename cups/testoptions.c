/*
 * Option unit test program for CUPS.
 *
 * Copyright 2008-2016 by Apple Inc.
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
  ipp_t		*request;		/* IPP request */
  ipp_attribute_t *attr;		/* IPP attribute */
  int		count;			/* Number of attributes */


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
				   "barfoo=\"\'BAR FOO\'\" "
				   "auth-info=user,pass\\\\,word\\\\\\\\", 0, &options);

    if (num_options != 6)
    {
      printf("FAIL (num_options=%d, expected 6)\n", num_options);
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
    else if ((value = cupsGetOption("auth-info", num_options, options)) == NULL ||
             strcmp(value, "user,pass\\,word\\\\"))
    {
      printf("FAIL (auth-info=\"%s\", expected \"user,pass\\,word\\\\\")\n", value);
      status ++;
    }
    else
      puts("PASS");

    fputs("cupsEncodeOptions2: ", stdout);
    request = ippNew();
    ippSetOperation(request, IPP_OP_PRINT_JOB);

    cupsEncodeOptions2(request, num_options, options, IPP_TAG_JOB);
    for (count = 0, attr = ippFirstAttribute(request); attr; attr = ippNextAttribute(request), count ++);
    if (count != 6)
    {
      printf("FAIL (%d attributes, expected 6)\n", count);
      status ++;
    }
    else if ((attr = ippFindAttribute(request, "foo", IPP_TAG_ZERO)) == NULL)
    {
      puts("FAIL (Unable to find attribute \"foo\")");
      status ++;
    }
    else if (ippGetValueTag(attr) != IPP_TAG_NAME)
    {
      printf("FAIL (\"foo\" of type %s, expected name)\n", ippTagString(ippGetValueTag(attr)));
      status ++;
    }
    else if (ippGetCount(attr) != 1)
    {
      printf("FAIL (\"foo\" has %d values, expected 1)\n", (int)ippGetCount(attr));
      status ++;
    }
    else if (strcmp(ippGetString(attr, 0, NULL), "1234"))
    {
      printf("FAIL (\"foo\" has value %s, expected 1234)\n", ippGetString(attr, 0, NULL));
      status ++;
    }
    else if ((attr = ippFindAttribute(request, "auth-info", IPP_TAG_ZERO)) == NULL)
    {
      puts("FAIL (Unable to find attribute \"auth-info\")");
      status ++;
    }
    else if (ippGetValueTag(attr) != IPP_TAG_TEXT)
    {
      printf("FAIL (\"auth-info\" of type %s, expected text)\n", ippTagString(ippGetValueTag(attr)));
      status ++;
    }
    else if (ippGetCount(attr) != 2)
    {
      printf("FAIL (\"auth-info\" has %d values, expected 2)\n", (int)ippGetCount(attr));
      status ++;
    }
    else if (strcmp(ippGetString(attr, 0, NULL), "user"))
    {
      printf("FAIL (\"auth-info\"[0] has value \"%s\", expected \"user\")\n", ippGetString(attr, 0, NULL));
      status ++;
    }
    else if (strcmp(ippGetString(attr, 1, NULL), "pass,word\\"))
    {
      printf("FAIL (\"auth-info\"[1] has value \"%s\", expected \"pass,word\\\")\n", ippGetString(attr, 1, NULL));
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
