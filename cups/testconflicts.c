/*
 * "$Id: testconflicts.c 3755 2012-03-30 05:59:14Z msweet $"
 *
 *   PPD constraint test program for CUPS.
 *
 *   Copyright 2008-2012 by Apple Inc.
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
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ppd.h"
#include "string-private.h"


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  ppd_file_t	*ppd;			/* PPD file loaded from disk */
  char		line[256],		/* Input buffer */
		*ptr,			/* Pointer into buffer */
		*optr,			/* Pointer to first option name */
		*cptr;			/* Pointer to first choice */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  char		*option,		/* Current option */
		*choice;		/* Current choice */


  if (argc != 2)
  {
    puts("Usage: testconflicts filename.ppd");
    return (1);
  }

  if ((ppd = ppdOpenFile(argv[1])) == NULL)
  {
    ppd_status_t	err;		/* Last error in file */
    int			linenum;	/* Line number in file */

    err = ppdLastError(&linenum);

    printf("Unable to open PPD file \"%s\": %s on line %d\n", argv[1],
           ppdErrorString(err), linenum);
    return (1);
  }

  ppdMarkDefaults(ppd);

  option = NULL;
  choice = NULL;

  for (;;)
  {
    num_options = 0;
    options     = NULL;

    if (!cupsResolveConflicts(ppd, option, choice, &num_options, &options))
      puts("Unable to resolve conflicts!");
    else if ((!option && num_options > 0) || (option && num_options > 1))
    {
      fputs("Resolved conflicts with the following options:\n   ", stdout);
      for (i = 0; i < num_options; i ++)
        if (!option || _cups_strcasecmp(option, options[i].name))
	  printf(" %s=%s", options[i].name, options[i].value);
      putchar('\n');

      cupsFreeOptions(num_options, options);
    }

    if (option)
    {
      free(option);
      option = NULL;
    }

    if (choice)
    {
      free(choice);
      choice = NULL;
    }

    printf("\nNew Option(s): ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin) || line[0] == '\n')
      break;

    for (ptr = line; isspace(*ptr & 255); ptr ++);
    for (optr = ptr; *ptr && *ptr != '='; ptr ++);
    if (!*ptr)
      break;
    for (*ptr++ = '\0', cptr = ptr; *ptr && !isspace(*ptr & 255); ptr ++);
    if (!*ptr)
      break;
    *ptr++ = '\0';

    option      = strdup(optr);
    choice      = strdup(cptr);
    num_options = cupsParseOptions(ptr, 0, &options);

    ppdMarkOption(ppd, option, choice);
    if (cupsMarkOptions(ppd, num_options, options))
      puts("Options Conflict!");
    cupsFreeOptions(num_options, options);
  }

  if (option)
    free(option);
  if (choice)
    free(choice);

  return (0);
}


/*
 * End of "$Id: testconflicts.c 3755 2012-03-30 05:59:14Z msweet $".
 */
