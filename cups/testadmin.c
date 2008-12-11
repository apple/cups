/*
 * "$Id: testadmin.c 7720 2008-07-11 22:46:21Z mike $"
 *
 *   Admin function test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006 by Easy Software Products.
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
 *   main()          - Main entry.
 *   show_settings() - Show settings in the array...
 */

/*
 * Include necessary headers...
 */

#include "adminutil.h"
#include "string.h"


/*
 * Local functions...
 */

static void	show_settings(int num_settings, cups_option_t *settings);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i,			/* Looping var */
		num_settings;		/* Number of settings */
  cups_option_t	*settings;		/* Settings */
  http_t	*http;			/* Connection to server */


 /*
  * Connect to the server using the defaults...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

 /*
  * Set the current configuration if we have anything on the command-line...
  */

  if (argc > 1)
  {
    for (i = 1, num_settings = 0, settings = NULL; i < argc; i ++)
      num_settings = cupsParseOptions(argv[i], num_settings, &settings);

    if (cupsAdminSetServerSettings(http, num_settings, settings))
    {
      puts("New server settings:");
      cupsFreeOptions(num_settings, settings);
    }
    else
    {
      printf("Server settings not changed: %s\n", cupsLastErrorString());
      return (1);
    }
  }
  else
    puts("Current server settings:");

 /*
  * Get the current configuration...
  */

  if (cupsAdminGetServerSettings(http, &num_settings, &settings))
  {
    show_settings(num_settings, settings);
    cupsFreeOptions(num_settings, settings);
    return (0);
  }
  else
  {
    printf("    %s\n", cupsLastErrorString());
    return (1);
  }
}


/*
 * 'show_settings()' - Show settings in the array...
 */

static void
show_settings(
    int           num_settings,		/* I - Number of settings */
    cups_option_t *settings)		/* I - Settings */
{
  while (num_settings > 0)
  {
    printf("    %s=%s\n", settings->name, settings->value);

    settings ++;
    num_settings --;
  }
}


/*
 * End of "$Id: testadmin.c 7720 2008-07-11 22:46:21Z mike $".
 */
