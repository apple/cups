/*
 * "cupsaccept", "cupsdisable", "cupsenable", and "cupsreject" commands for
 * CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>


/*
 * Local functions...
 */

static void	usage(const char *command) _CUPS_NORETURN;


/*
 * 'main()' - Parse options and accept/reject jobs or disable/enable printers.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  char		*command,		/* Command to do */
		*opt,			/* Option pointer */
		uri[1024],		/* Printer URI */
		*reason;		/* Reason for reject/disable */
  ipp_t		*request;		/* IPP request */
  ipp_op_t	op;			/* Operation */
  int		cancel;			/* Cancel jobs? */


  _cupsSetLocale(argv);

 /*
  * See what operation we're supposed to do...
  */

  if ((command = strrchr(argv[0], '/')) != NULL)
    command ++;
  else
    command = argv[0];

  cancel = 0;

  if (!strcmp(command, "cupsaccept"))
    op = CUPS_ACCEPT_JOBS;
  else if (!strcmp(command, "cupsreject"))
    op = CUPS_REJECT_JOBS;
  else if (!strcmp(command, "cupsdisable"))
    op = IPP_PAUSE_PRINTER;
  else if (!strcmp(command, "cupsenable"))
    op = IPP_RESUME_PRINTER;
  else
  {
    _cupsLangPrintf(stderr, _("%s: Don't know what to do."), command);
    return (1);
  }

  reason = NULL;

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
      usage(command);
    else if (!strcmp(argv[i], "--hold"))
      op = IPP_HOLD_NEW_JOBS;
    else if (!strcmp(argv[i], "--release"))
      op = IPP_RELEASE_HELD_NEW_JOBS;
    else if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
	switch (*opt)
	{
	  case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	      cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
	      _cupsLangPrintf(stderr, _("%s: Sorry, no encryption support."), command);
#endif /* HAVE_SSL */
	      break;

	  case 'U' : /* Username */
	      if (opt[1] != '\0')
	      {
		cupsSetUser(opt + 1);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr, _("%s: Error - expected username after \"-U\" option."), command);
		  usage(command);
		}

		cupsSetUser(argv[i]);
	      }
	      break;

	  case 'c' : /* Cancel jobs */
	      cancel = 1;
	      break;

	  case 'h' : /* Connect to host */
	      if (opt[1] != '\0')
	      {
		cupsSetServer(opt + 1);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr, _("%s: Error - expected hostname after \"-h\" option."), command);
		  usage(command);
		}

		cupsSetServer(argv[i]);
	      }
	      break;

	  case 'r' : /* Reason for cancellation */
	      if (opt[1] != '\0')
	      {
		reason = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;
		if (i >= argc)
		{
		  _cupsLangPrintf(stderr, _("%s: Error - expected reason text after \"-r\" option."), command);
		  usage(command);
		}

		reason = argv[i];
	      }
	      break;

	  default :
	      _cupsLangPrintf(stderr, _("%s: Error - unknown option \"%c\"."), command, *opt);
	      usage(command);
	}
      }
    }
    else
    {
     /*
      * Accept/disable/enable/reject a destination...
      */

      request = ippNewRequest(op);

      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                       "localhost", 0, "/printers/%s", argv[i]);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                   "printer-uri", NULL, uri);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser());

      if (reason != NULL)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                     "printer-state-message", NULL, reason);

     /*
      * Do the request and get back a response...
      */

      ippDelete(cupsDoRequest(CUPS_HTTP_DEFAULT, request, "/admin/"));

      if (cupsLastError() > IPP_OK_CONFLICT)
      {
	_cupsLangPrintf(stderr,
			_("%s: Operation failed: %s"),
			command, ippErrorString(cupsLastError()));
	return (1);
      }

     /*
      * Cancel all jobs if requested...
      */

      if (cancel)
      {
       /*
	* Build an IPP_PURGE_JOBS request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    printer-uri
	*/

	request = ippNewRequest(IPP_PURGE_JOBS);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, uri);

	ippDelete(cupsDoRequest(CUPS_HTTP_DEFAULT, request, "/admin/"));

        if (cupsLastError() > IPP_OK_CONFLICT)
	{
	  _cupsLangPrintf(stderr, "%s: %s", command, cupsLastErrorString());
	  return (1);
	}
      }
    }
  }

  return (0);
}


/*
 * 'usage()' - Show program usage and exit.
 */

static void
usage(const char *command)		/* I - Command name */
{
  _cupsLangPrintf(stdout, _("Usage: %s [options] destination(s)"), command);
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stdout, _("-E                      Encrypt the connection to the server"));
  _cupsLangPuts(stdout, _("-h server[:port]        Connect to the named server and port"));
  _cupsLangPuts(stdout, _("-r reason               Specify a reason message that others can see"));
  _cupsLangPuts(stdout, _("-U username             Specify the username to use for authentication"));
  if (!strcmp(command, "cupsdisable"))
    _cupsLangPuts(stdout, _("--hold                  Hold new jobs"));
  if (!strcmp(command, "cupsenable"))
    _cupsLangPuts(stdout, _("--release               Release previously held jobs"));

  exit(1);
}
