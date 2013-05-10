/*
 * "$Id$"
 *
 *   Scheduler control program for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 2006-2007 by Easy Software Products.
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
 *   main()  - Get/set server settings.
 *   usage() - Show program usage.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>
#include <cups/adminutil.h>


/*
 * Local functions...
 */

static void	usage(const char *opt);


/*
 * 'main()' - Get/set server settings.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i,			/* Looping var */
		num_settings;		/* Number of settings */
  cups_option_t	*settings;		/* Settings */
  const char	*opt;			/* Current option character */
  http_t	*http;			/* Connection to server */


 /*
  * Process the command-line...
  */

  _cupsSetLocale(argv);

  num_settings = 0;
  settings     = NULL;

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      if (argv[i][1] == '-')
      {
        if (!strcmp(argv[i], "--debug-logging"))
	  num_settings = cupsAddOption(CUPS_SERVER_DEBUG_LOGGING, "1",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--no-debug-logging"))
	  num_settings = cupsAddOption(CUPS_SERVER_DEBUG_LOGGING, "0",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--remote-admin"))
	  num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ADMIN, "1",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--no-remote-admin"))
	  num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ADMIN, "0",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--remote-any"))
	  num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ANY, "1",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--no-remote-any"))
	  num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ANY, "0",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--remote-printers"))
	  num_settings = cupsAddOption(CUPS_SERVER_REMOTE_PRINTERS, "1",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--no-remote-printers"))
	  num_settings = cupsAddOption(CUPS_SERVER_REMOTE_PRINTERS, "0",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--share-printers"))
	  num_settings = cupsAddOption(CUPS_SERVER_SHARE_PRINTERS, "1",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--no-share-printers"))
	  num_settings = cupsAddOption(CUPS_SERVER_SHARE_PRINTERS, "0",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--user-cancel-any"))
	  num_settings = cupsAddOption(CUPS_SERVER_USER_CANCEL_ANY, "1",
	                               num_settings, &settings);
        else if (!strcmp(argv[i], "--no-user-cancel-any"))
	  num_settings = cupsAddOption(CUPS_SERVER_USER_CANCEL_ANY, "0",
	                               num_settings, &settings);
        else
	  usage(argv[i]);
      }
      else
      {
        for (opt = argv[i] + 1; *opt; opt ++)
	  switch (*opt)
	  {
	    case 'E' :
	        cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
	        break;

	    case 'U' :
	        i ++;
		if (i >= argc)
		  usage(NULL);

                cupsSetUser(argv[i]);
	        break;

	    case 'h' :
	        i ++;
		if (i >= argc)
		  usage(NULL);

                cupsSetServer(argv[i]);
	        break;

	    default :
	        usage(opt);
		break;
	  }
      }
    }
    else if (strchr(argv[i], '='))
      num_settings = cupsParseOptions(argv[i], num_settings, &settings);
    else
      usage(argv[i]);
  }

  if (cupsGetOption("Listen", num_settings, settings) ||
      cupsGetOption("Port", num_settings, settings))
  {
    _cupsLangPuts(stderr, _("cupsctl: Cannot set Listen or Port directly."));
    return (1);
  }

 /*
  * Connect to the server using the defaults...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    _cupsLangPrintf(stderr, _("cupsctl: Unable to connect to server: %s"),
                    strerror(errno));
    return (1);
  }

 /*
  * Set the current configuration if we have anything on the command-line...
  */

  if (num_settings > 0)
  {
    if (!cupsAdminSetServerSettings(http, num_settings, settings))
    {
      _cupsLangPrintf(stderr, "cupsctl: %s", cupsLastErrorString());
      return (1);
    }
  }
  else if (!cupsAdminGetServerSettings(http, &num_settings, &settings))
  {
    _cupsLangPrintf(stderr, "cupsctl: %s", cupsLastErrorString());
    return (1);
  }
  else
  {
    for (i = 0; i < num_settings; i ++)
      _cupsLangPrintf(stdout, "%s=%s", settings[i].name, settings[i].value);
  }

  cupsFreeOptions(num_settings, settings);
  return (0);
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(const char *opt)			/* I - Option character/string */
{
  if (opt)
  {
    if (*opt == '-')
      _cupsLangPrintf(stderr, _("cupsctl: Unknown option \"%s\""), opt);
    else
      _cupsLangPrintf(stderr, _("cupsctl: Unknown option \"-%c\""), *opt);
  }

  _cupsLangPuts(stdout, _("Usage: cupsctl [options] [param=value ... "
                          "paramN=valueN]"));
  _cupsLangPuts(stdout, "");
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stdout, "");
  _cupsLangPuts(stdout, _("  -E                      Enable encryption."));
  _cupsLangPuts(stdout, _("  -U username             Specify username."));
  _cupsLangPuts(stdout, _("  -h server[:port]        Specify server "
                          "address."));
  _cupsLangPuts(stdout, "");
  _cupsLangPuts(stdout, _("  --[no-]debug-logging    Turn debug logging "
                          "on/off."));
  _cupsLangPuts(stdout, _("  --[no-]remote-admin     Turn remote "
                          "administration on/off."));
  _cupsLangPuts(stdout, _("  --[no-]remote-any       Allow/prevent access "
                          "from the Internet."));
  _cupsLangPuts(stdout, _("  --[no-]remote-printers  Show/hide remote "
                          "printers."));
  _cupsLangPuts(stdout, _("  --[no-]share-printers   Turn printer sharing "
                          "on/off."));
  _cupsLangPuts(stdout, _("  --[no-]user-cancel-any  Allow/prevent users to "
                          "cancel any job."));

  exit(1);
}


/*
 * End of "$Id$".
 */
