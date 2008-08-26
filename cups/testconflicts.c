/*
 * "$Id$"
 *
 *   PPD constraint test program for the Common UNIX Printing System (CUPS).
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
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  ppd_file_t	*ppd;			/* PPD file loaded from disk */
  char		line[256];		/* Input buffer */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */


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

  for (;;)
  {
    num_options = 0;
    options     = NULL;

    if (!cupsResolveConflicts(ppd, NULL, NULL, &num_options, &options))
      puts("Unable to resolve conflicts!");
    else if (num_options > 0)
    {
      fputs("Resolved conflicts with the following options:\n   ", stdout);
      for (i = 0; i < num_options; i ++)
	printf(" %s=%s", options[i].name, options[i].value);
      putchar('\n');

      cupsFreeOptions(num_options, options);
    }

    printf("\nNew Option(s): ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin) || line[0] == '\n')
      break;

    num_options = cupsParseOptions(line, 0, &options);
    if (cupsMarkOptions(ppd, num_options, options))
      puts("Options Conflict!");
    cupsFreeOptions(num_options, options);
  }

  return (0);
}


/*
 * End of "$Id$".
 */
