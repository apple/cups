/*
 * "$Id$"
 *
 *   "lp" command for the Common UNIX Printing System (CUPS).
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
 * Contents:
 *
 *   main()          - Parse options and send files for printing.
 *   restart_job()   - Restart a job.
 *   set_job_attrs() - Set job attributes.
 *   sighandler()    - Signal catcher for when we print from stdin...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/i18n.h>


#ifndef WIN32
#  include <unistd.h>
#  include <signal.h>


/*
 * Local functions.
 */

void	sighandler(int);
#endif /* !WIN32 */
int	restart_job(int job_id);
int	set_job_attrs(int job_id, int num_options, cups_option_t *options);


/*
 * Globals...
 */

char	tempfile[1024];		/* Temporary file for printing from stdin */


/*
 * 'main()' - Parse options and send files for printing.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j;			/* Looping vars */
  int		job_id;			/* Job ID */
  char		*printer,		/* Printer name */
		*instance,		/* Instance name */ 
		*val,			/* Option value */
		*title;			/* Job title */
  int		priority;		/* Job priority (1-100) */
  int		num_copies;		/* Number of copies per file */
  int		num_files;		/* Number of files to print */
  const char	*files[1000];		/* Files to print */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dests,			/* Destinations */
		*dest;			/* Selected destination */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		silent;			/* Silent or verbose output? */
  char		buffer[8192];		/* Copy buffer */
  int		temp;			/* Temporary file descriptor */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Signal action */
  struct sigaction oldaction;		/* Old signal action */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET*/


#ifdef __sun
 /*
  * Solaris does some rather strange things to re-queue remote print
  * jobs.  On bootup, the "lp" command is run as "printd" to re-spool
  * any remote jobs in /var/spool/print.  Since CUPS doesn't need this
  * nonsense, we just need to add the necessary check here to prevent
  * lp from causing boot problems...
  */

  if ((val = strrchr(argv[0], '/')) != NULL)
    val ++;
  else
    val = argv[0];

  if (!strcmp(val, "printd"))
    return (0);
#endif /* __sun */

  silent      = 0;
  printer     = NULL;
  num_dests   = 0;
  dests       = NULL;
  num_options = 0;
  options     = NULL;
  num_files   = 0;
  title       = NULL;
  job_id      = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1])
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'c' : /* Copy to spool dir (always enabled) */
	    break;

        case 'd' : /* Destination printer or class */
	    if (argv[i][2] != '\0')
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected destination after -d option!\n"));
		return (1);
              }

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

	    if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            if ((dest = cupsGetDest(printer, instance, num_dests, dests)) != NULL)
	    {
	      for (j = 0; j < dest->num_options; j ++)
	        if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	          num_options = cupsAddOption(dest->options[j].name,
		                              dest->options[j].value,
					      num_options, &options);
	    }
	    break;

        case 'f' : /* Form */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected form after -f option!\n"));
		return (1);
              }
	    }

	    fputs("lp: Warning - form option ignored!\n", stderr);
	    break;

        case 'h' : /* Destination host */
	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected hostname after -h option!\n"));
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

        case 'i' : /* Change job */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected job ID after -i option!\n"));
		return (1);
              }

	      val = argv[i];
	    }

            if (num_files > 0)
	    {
	      _cupsLangPuts(stderr,
		              _("lp: Error - cannot print files and alter "
			        "jobs simultaneously!\n"));
	      return (1);
	    }

            if (strrchr(val, '-') != NULL)
	      job_id = atoi(strrchr(val, '-') + 1);
	    else
	      job_id = atoi(val);

            if (job_id < 0)
	    {
	      _cupsLangPuts(stderr, _("lp: Error - bad job ID!\n"));
	      break;
	    }
	    break;

	case 'm' : /* Send email when job is done */
#ifdef __sun
	case 'p' : /* Notify on completion */
#endif /* __sun */
	case 'w' : /* Write to console or email */
	    break;

	case 'n' : /* Number of copies */
	    if (argv[i][2] != '\0')
	      num_copies = atoi(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected copies after -n option!\n"));
		return (1);
              }

	      num_copies = atoi(argv[i]);
	    }

            sprintf(buffer, "%d", num_copies);
            num_options = cupsAddOption("copies", buffer, num_options, &options);
	    break;

	case 'o' : /* Option */
	    if (argv[i][2] != '\0')
	      num_options = cupsParseOptions(argv[i] + 2, num_options, &options);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected option string after -o option!\n"));
		return (1);
              }

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

#ifndef __sun
	case 'p' : /* Queue priority */
#endif /* !__sun */
	case 'q' : /* Queue priority */
	    if (argv[i][2] != '\0')
	      priority = atoi(argv[i] + 2);
	    else
	    {
	      if ((i + 1) >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("lp: Expected priority after -%c option!\n"),
		        	argv[i][1]);
		return (1);
              }

	      i ++;

	      priority = atoi(argv[i]);
	    }

           /*
	    * For 100% Solaris compatibility, need to add:
	    *
	    *   priority = 99 * (39 - priority) / 39 + 1;
	    *
	    * However, to keep CUPS lp the same across all platforms
	    * we will break compatibility this far...
	    */

	    if (priority < 1 || priority > 100)
	    {
	      _cupsLangPuts(stderr,
		            _("lp: Priority must be between 1 and 100.\n"));
	      return (1);
	    }

            sprintf(buffer, "%d", priority);
            num_options = cupsAddOption("job-priority", buffer, num_options, &options);
	    break;

	case 's' : /* Silent */
	    silent = 1;
	    break;

	case 't' : /* Title */
	    if (argv[i][2] != '\0')
	      title = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected title after -t option!\n"));
		return (1);
              }

	      title = argv[i];
	    }
	    break;

        case 'y' : /* mode-list */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected mode list after -y option!\n"));
		return (1);
              }
	    }

	    _cupsLangPuts(stderr,
		          _("lp: Warning - mode option ignored!\n"));
	    break;

        case 'H' : /* Hold job */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected hold name after -H option!\n"));
		return (1);
              }

	      val = argv[i];
	    }

	    if (!strcmp(val, "hold"))
              num_options = cupsAddOption("job-hold-until", "indefinite",
	                                  num_options, &options);
	    else if (!strcmp(val, "resume") ||
	             !strcmp(val, "release"))
              num_options = cupsAddOption("job-hold-until", "no-hold",
	                                  num_options, &options);
	    else if (!strcmp(val, "immediate"))
              num_options = cupsAddOption("job-priority", "100",
	                                  num_options, &options);
	    else if (!strcmp(val, "restart"))
	    {
	      if (job_id < 1)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Need job ID (-i) before \"-H restart\"!\n"));
		return (1);
	      }

	      if (restart_job(job_id))
	        return (1);
	    }
	    else
              num_options = cupsAddOption("job-hold-until", val,
	                                  num_options, &options);
	    break;

        case 'P' : /* Page list */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected page list after -P option!\n"));
		return (1);
              }

	      val = argv[i];
	    }

            num_options = cupsAddOption("page-ranges", val, num_options,
	                                &options);
            break;

        case 'S' : /* character set */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected character set after -S option!\n"));
		return (1);
              }
	    }

	    _cupsLangPuts(stderr,
		          _("lp: Warning - character set option ignored!\n"));
	    break;

        case 'T' : /* Content-Type */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lp: Expected content type after -T option!\n"));
		return (1);
              }
	    }

	    _cupsLangPuts(stderr,
		          _("lp: Warning - content type option ignored!\n"));
	    break;

	default :
	    _cupsLangPrintf(stderr, _("lp: Unknown option \'%c\'!\n"),
	                    argv[i][1]);
	    return (1);
      }
    else if (!strcmp(argv[i], "-"))
    {
      if (num_files || job_id)
      {
        _cupsLangPuts(stderr,
		      _("lp: Error - cannot print from stdin if files or a "
		        "job ID are provided!\n"));
	return (1);
      }

      break;
    }
    else if (num_files < 1000 && job_id == 0)
    {
     /*
      * Print a file...
      */

      if (access(argv[i], R_OK) != 0)
      {
        _cupsLangPrintf(stderr, _("lp: Unable to access \"%s\" - %s\n"),
	                argv[i], strerror(errno));
        return (1);
      }

      files[num_files] = argv[i];
      num_files ++;

      if (title == NULL)
      {
        if ((title = strrchr(argv[i], '/')) != NULL)
	  title ++;
	else
          title = argv[i];
      }
    }
    else
      _cupsLangPrintf(stderr, _("lp: Too many files - \"%s\"\n"),
                      argv[i]);

 /*
  * See if we are altering an existing job...
  */

  if (job_id)
    return (set_job_attrs(job_id, num_options, options));

 /*
  * See if we have any files to print; if not, print from stdin...
  */

  if (printer == NULL)
  {
    if (num_dests == 0)
      num_dests = cupsGetDests(&dests);

    if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
    {
      printer = dest->name;

      for (j = 0; j < dest->num_options; j ++)
	if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	  num_options = cupsAddOption(dest->options[j].name,
		                      dest->options[j].value,
				      num_options, &options);
    }
  }

  if (printer == NULL)
  {
    val = NULL;

    if ((printer = getenv("LPDEST")) == NULL)
    {
      if ((printer = getenv("PRINTER")) != NULL)
      {
        if (!strcmp(printer, "lp"))
          printer = NULL;
	else
	  val = "PRINTER";
      }
    }
    else
      val = "LPDEST";

    if (printer && !cupsGetDest(printer, NULL, num_dests, dests))
      _cupsLangPrintf(stderr,
		      _("lp: error - %s environment variable names "
		        "non-existent destination \"%s\"!\n"),
        	      val, printer);
    else if (cupsLastError() == IPP_NOT_FOUND)
      _cupsLangPuts(stderr,
		    _("lp: error - no default destination available.\n"));
    else
      _cupsLangPuts(stderr,
		    _("lp: error - scheduler not responding!\n"));

    return (1);
  }

  if (num_files > 0)
    job_id = cupsPrintFiles(printer, num_files, files, title, num_options, options);
  else
  {
    num_files = 1;

#ifndef WIN32
#  if defined(HAVE_SIGSET)
    sigset(SIGHUP, sighandler);
    if (sigset(SIGINT, sighandler) == SIG_IGN)
      sigset(SIGINT, SIG_IGN);
    sigset(SIGTERM, sighandler);
#  elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));
    action.sa_handler = sighandler;

    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, NULL, &oldaction);
    if (oldaction.sa_handler != SIG_IGN)
      sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
#  else
    signal(SIGHUP, sighandler);
    if (signal(SIGINT, sighandler) == SIG_IGN)
      signal(SIGINT, SIG_IGN);
    signal(SIGTERM, sighandler);
#  endif
#endif /* !WIN32 */

    temp = cupsTempFd(tempfile, sizeof(tempfile));

    if (temp < 0)
    {
      _cupsLangPrintf(stderr,
		      _("lp: unable to create temporary file \"%s\" - %s\n"),
        	      tempfile, strerror(errno));
      return (1);
    }

    while ((i = read(0, buffer, sizeof(buffer))) > 0)
      if (write(temp, buffer, i) < 0)
      {
	_cupsLangPrintf(stderr,
		        _("lp: error - unable to write to temporary file "
			  "\"%s\" - %s\n"),
        	        tempfile, strerror(errno));
        close(temp);
        unlink(tempfile);
	return (1);
      }

    i = lseek(temp, 0, SEEK_CUR);
    close(temp);

    if (i == 0)
    {
      _cupsLangPuts(stderr,
		    _("lp: stdin is empty, so no job has been sent.\n"));
      unlink(tempfile);
      return (1);
    }

    if (title)
      job_id = cupsPrintFile(printer, tempfile, title, num_options, options);
    else
      job_id = cupsPrintFile(printer, tempfile, "(stdin)", num_options, options);

    unlink(tempfile);
  }

  if (job_id < 1)
  {
    _cupsLangPrintf(stderr, "lp: %s\n", cupsLastErrorString());
    return (1);
  }
  else if (!silent)
    _cupsLangPrintf(stdout, _("request id is %s-%d (%d file(s))\n"),
		    printer, job_id, num_files);

  return (0);
}


/*
 * 'restart_job()' - Restart a job.
 */

int					/* O - Exit status */
restart_job(int job_id)			/* I - Job ID */
{
  http_t	*http;			/* HTTP connection to server */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* URI for job */


  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

  request = ippNewRequest(IPP_RESTART_JOB);

  sprintf(uri, "ipp://localhost/jobs/%d", job_id);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "job-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lp: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lp: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'set_job_attrs()' - Set job attributes.
 */

int					/* O - Exit status */
set_job_attrs(int           job_id,	/* I - Job ID */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  http_t	*http;			/* HTTP connection to server */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* URI for job */


  if (num_options == 0)
    return (0);

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

  request = ippNewRequest(IPP_SET_JOB_ATTRIBUTES);

  sprintf(uri, "ipp://localhost/jobs/%d", job_id);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "job-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  cupsEncodeOptions(request, num_options, options);

  if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lp: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lp: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


#ifndef WIN32
/*
 * 'sighandler()' - Signal catcher for when we print from stdin...
 */

void
sighandler(int s)	/* I - Signal number */
{
 /*
  * Remove the temporary file we're using to print from stdin...
  */

  unlink(tempfile);

 /*
  * Exit...
  */

  exit(s);
}
#endif /* !WIN32 */


/*
 * End of "$Id$".
 */
