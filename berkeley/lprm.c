/*
 * "lprm" command for CUPS.
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

static void	usage(void) _CUPS_NORETURN;


/*
 * 'main()' - Parse options and cancel jobs.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		job_id;		/* Job ID */
  const char	*name;		/* Destination printer */
  char		*instance,	/* Pointer to instance name */
		*opt;		/* Option pointer */
  cups_dest_t	*dest,		/* Destination */
		*defdest;	/* Default destination */
  int		did_cancel;	/* Did we cancel something? */


  _cupsSetLocale(argv);

 /*
  * Setup to cancel individual print jobs...
  */

  did_cancel = 0;
  defdest    = cupsGetNamedDest(CUPS_HTTP_DEFAULT, NULL, NULL);
  name       = defdest ? defdest->name : NULL;

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
      usage();
    else if (argv[i][0] == '-' && argv[i][1] != '\0')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
	switch (*opt)
	{
	  case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	      cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
	      _cupsLangPrintf(stderr, _("%s: Sorry, no encryption support."), argv[0]);
#endif /* HAVE_SSL */
	      break;

	  case 'P' : /* Cancel jobs on a printer */
	      if (opt[1] != '\0')
	      {
		name = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;
		name = argv[i];
	      }

	      if ((instance = strchr(name, '/')) != NULL)
		*instance = '\0';

	      if ((dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, name, NULL)) == NULL)
	      {
		_cupsLangPrintf(stderr, _("%s: Error - unknown destination \"%s\"."), argv[0], name);
		goto error;
	      }

	      cupsFreeDests(1, dest);
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
		  _cupsLangPrintf(stderr, _("%s: Error - expected username after \"-U\" option."), argv[0]);
		  usage();
		}

		cupsSetUser(argv[i]);
	      }
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
		  _cupsLangPrintf(stderr, _("%s: Error - expected hostname after \"-h\" option."), argv[0]);
		  usage();
		}
		else
		  cupsSetServer(argv[i]);
	      }

	      if (defdest)
		cupsFreeDests(1, defdest);

	      defdest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, NULL, NULL);
	      name    = defdest ? defdest->name : NULL;
	      break;

	  default :
	      _cupsLangPrintf(stderr, _("%s: Error - unknown option \"%c\"."), argv[0], *opt);
	      usage();
	}
      }
    }
    else
    {
     /*
      * Cancel a job or printer...
      */

      if ((dest = cupsGetNamedDest(CUPS_HTTP_DEFAULT, argv[i], NULL)) != NULL)
        cupsFreeDests(1, dest);

      if (dest)
      {
        name   = argv[i];
        job_id = 0;
      }
      else if (isdigit(argv[i][0] & 255))
      {
        name   = NULL;
        job_id = atoi(argv[i]);
      }
      else if (!strcmp(argv[i], "-"))
      {
       /*
        * Cancel all jobs
        */

        job_id = -1;
      }
      else
      {
	_cupsLangPrintf(stderr, _("%s: Error - unknown destination \"%s\"."),
			argv[0], argv[i]);
	goto error;
      }

      if (cupsCancelJob2(CUPS_HTTP_DEFAULT, name, job_id, 0) != IPP_OK)
      {
        _cupsLangPrintf(stderr, "%s: %s", argv[0], cupsLastErrorString());
	goto error;
      }

      did_cancel = 1;
    }
  }

 /*
  * If nothing has been canceled yet, cancel the current job on the specified
  * (or default) printer...
  */

  if (!did_cancel && cupsCancelJob2(CUPS_HTTP_DEFAULT, name, 0, 0) != IPP_OK)
    {
      _cupsLangPrintf(stderr, "%s: %s", argv[0], cupsLastErrorString());
      goto error;
    }

  if (defdest)
    cupsFreeDests(1, defdest);

  return (0);

 /*
  * If we get here there was an error, so clean up...
  */

  error:

  if (defdest)
    cupsFreeDests(1, defdest);

  return (1);
}


/*
 * 'usage()' - Show program usage and exit.
 */

static void
usage(void)
{
  _cupsLangPuts(stdout, _("Usage: lprm [options] [id]\n"
                          "       lprm [options] -"));
  _cupsLangPuts(stdout, _("Options:"));
  _cupsLangPuts(stdout, _("-                       Cancel all jobs"));
  _cupsLangPuts(stdout, _("-E                      Encrypt the connection to the server"));
  _cupsLangPuts(stdout, _("-h server[:port]        Connect to the named server and port"));
  _cupsLangPuts(stdout, _("-P destination          Specify the destination"));
  _cupsLangPuts(stdout, _("-U username             Specify the username to use for authentication"));

  exit(1);
}
