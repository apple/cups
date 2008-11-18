/*
 * "$Id$"
 *
 *   "lp" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()          - Parse options and send files for printing.
 *   restart_job()   - Restart a job.
 *   set_job_attrs() - Set job attributes.
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


/*
 * Local functions.
 */

int	restart_job(const char *command, int job_id);
int	set_job_attrs(const char *command, int job_id, int num_options,
	              cups_option_t *options);


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
  cups_dest_t	*dest;			/* Selected destination */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		end_options;		/* No more options? */
  int		silent;			/* Silent or verbose output? */
  char		buffer[8192];		/* Copy buffer */


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

  _cupsSetLocale(argv);

  silent      = 0;
  printer     = NULL;
  dest        = NULL;
  num_options = 0;
  options     = NULL;
  num_files   = 0;
  title       = NULL;
  job_id      = 0;
  end_options = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1] && !end_options)
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

        case 'U' : /* Username */
	    if (argv[i][2] != '\0')
	      cupsSetUser(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected username after "
				  "\'-U\' option!\n"),
		        	argv[0]);
	        return (1);
	      }

              cupsSetUser(argv[i]);
	    }
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
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected destination after "
			          "\'-d\' option!\n"),
				argv[0]);
		return (1);
              }

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

            if ((dest = cupsGetNamedDest(NULL, printer, instance)) != NULL)
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
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected form after \'-f\' "
				  "option!\n"),
				argv[0]);
		return (1);
              }
	    }

	    _cupsLangPrintf(stderr, _("%s: Warning - form option ignored!\n"),
	                    argv[0]);
	    break;

        case 'h' : /* Destination host */
	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected hostname after "
				  "\'-h\' option!\n"),
				argv[0]);
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
	        _cupsLangPrintf(stderr,
		                _("%s: Expected job ID after \'-i\' option!\n"),
				argv[0]);
		return (1);
              }

	      val = argv[i];
	    }

            if (num_files > 0)
	    {
	      _cupsLangPrintf(stderr,
		              _("%s: Error - cannot print files and alter "
			        "jobs simultaneously!\n"),
			      argv[0]);
	      return (1);
	    }

            if (strrchr(val, '-') != NULL)
	      job_id = atoi(strrchr(val, '-') + 1);
	    else
	      job_id = atoi(val);

            if (job_id < 0)
	    {
	      _cupsLangPrintf(stderr, _("%s: Error - bad job ID!\n"), argv[0]);
	      break;
	    }
	    break;

	case 'm' : /* Send email when job is done */
#ifdef __sun
	case 'p' : /* Notify on completion */
#endif /* __sun */
	case 'w' : /* Write to console or email */
	    {
	      char	email[1024];	/* EMail address */


	      snprintf(email, sizeof(email), "mailto:%s@%s", cupsUser(),
	               httpGetHostname(NULL, buffer, sizeof(buffer)));
	      num_options = cupsAddOption("notify-recipient-uri", email,
	                                  num_options, &options);
	    }

	    silent = 1;
	    break;

	case 'n' : /* Number of copies */
	    if (argv[i][2] != '\0')
	      num_copies = atoi(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected copies after "
				  "\'-n\' option!\n"),
				argv[0]);
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
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected option string after "
				  "\'-o\' option!\n"),
				argv[0]);
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
		                _("%s: Error - expected priority after "
				  "\'-%c\' option!\n"),
		        	argv[0], argv[i][1]);
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
	      _cupsLangPrintf(stderr,
		              _("%s: Error - priority must be between 1 and "
			        "100.\n"),
		              argv[0]);
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
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected title after "
				  "\'-t\' option!\n"),
				argv[0]);
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
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected mode list after "
				  "\'-y\' option!\n"),
				argv[0]);
		return (1);
              }
	    }

	    _cupsLangPrintf(stderr,
		            _("%s: Warning - mode option ignored!\n"),
			    argv[0]);
	    break;

        case 'H' : /* Hold job */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected hold name after "
				  "\'-H\' option!\n"),
				argv[0]);
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
	    {
              num_options = cupsAddOption("job-hold-until", "no-hold",
	                                  num_options, &options);
              num_options = cupsAddOption("job-priority", "100",
	                                  num_options, &options);
	    }
	    else if (!strcmp(val, "restart"))
	    {
	      if (job_id < 1)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Need job ID (\'-i jobid\') before "
			          "\'-H restart\'!\n"),
				argv[0]);
		return (1);
	      }

	      if (restart_job(argv[0], job_id))
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
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected page list after "
				  "\'-P\' option!\n"),
				argv[0]);
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
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected character set after "
				  "\'-S\' option!\n"),
				argv[0]);
		return (1);
              }
	    }

	    _cupsLangPrintf(stderr,
		            _("%s: Warning - character set option ignored!\n"),
			    argv[0]);
	    break;

        case 'T' : /* Content-Type */
	    if (!argv[i][2])
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected content type after "
			          "\'-T\' option!\n"),
				argv[0]);
		return (1);
              }
	    }

	    _cupsLangPrintf(stderr,
		            _("%s: Warning - content type option ignored!\n"),
			    argv[0]);
	    break;

        case '-' : /* Stop processing options */
	    end_options = 1;
	    break;

	default :
	    _cupsLangPrintf(stderr, _("%s: Error - unknown option \'%c\'!\n"),
	                    argv[0], argv[i][1]);
	    return (1);
      }
    else if (!strcmp(argv[i], "-"))
    {
      if (num_files || job_id)
      {
        _cupsLangPrintf(stderr,
			_("%s: Error - cannot print from stdin if files or a "
		          "job ID are provided!\n"),
			 argv[0]);
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
        _cupsLangPrintf(stderr, _("%s: Error - unable to access \"%s\" - %s\n"),
	                argv[0], argv[i], strerror(errno));
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
      _cupsLangPrintf(stderr, _("%s: Error - too many files - \"%s\"\n"),
                      argv[0], argv[i]);

 /*
  * See if we are altering an existing job...
  */

  if (job_id)
    return (set_job_attrs(argv[0], job_id, num_options, options));

 /*
  * See if we have any files to print; if not, print from stdin...
  */

  if (printer == NULL)
  {
    if ((dest = cupsGetNamedDest(NULL, NULL, NULL)) != NULL)
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

    if (printer && !cupsGetNamedDest(NULL, printer, NULL))
      _cupsLangPrintf(stderr,
		      _("%s: Error - %s environment variable names "
		        "non-existent destination \"%s\"!\n"),
        	      argv[0], val, printer);
    else if (cupsLastError() == IPP_NOT_FOUND)
      _cupsLangPrintf(stderr,
		      _("%s: Error - no default destination available.\n"),
		      argv[0]);
    else
      _cupsLangPrintf(stderr,
		      _("%s: Error - scheduler not responding!\n"),
		      argv[0]);

    return (1);
  }

  if (num_files > 0)
    job_id = cupsPrintFiles(printer, num_files, files, title, num_options, options);
  else if ((job_id = cupsCreateJob(CUPS_HTTP_DEFAULT, printer,
                                   title ? title : "(stdin)",
                                   num_options, options)) > 0)
  {
    http_status_t	status;		/* Write status */
    const char		*format;	/* Document format */
    ssize_t		bytes;		/* Bytes read */


    if (cupsGetOption("raw", num_options, options))
      format = CUPS_FORMAT_RAW;
    else if ((format = cupsGetOption("document-format", num_options,
                                     options)) == NULL)
      format = CUPS_FORMAT_AUTO;

    status = cupsStartDocument(CUPS_HTTP_DEFAULT, printer, job_id, NULL,
                               format, 1);

    while (status == HTTP_CONTINUE &&
           (bytes = read(0, buffer, sizeof(buffer))) > 0)
      status = cupsWriteRequestData(CUPS_HTTP_DEFAULT, buffer, bytes);

    if (status != HTTP_CONTINUE)
    {
      _cupsLangPrintf(stderr,
		      _("%s: Error - unable to queue from stdin - %s\n"),
		      argv[0], httpStatus(status));
      return (1);
    }

    if (cupsFinishDocument(CUPS_HTTP_DEFAULT, printer) != IPP_OK)
      job_id = 0;
  }

  if (job_id < 1)
  {
    _cupsLangPrintf(stderr, "%s: %s\n", argv[0], cupsLastErrorString());
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
restart_job(const char *command,	/* I - Command name */
            int        job_id)		/* I - Job ID */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* URI for job */


  request = ippNewRequest(IPP_RESTART_JOB);

  sprintf(uri, "ipp://localhost/jobs/%d", job_id);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "job-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(CUPS_HTTP_DEFAULT, request, "/jobs"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "%s: %s\n", command, cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'set_job_attrs()' - Set job attributes.
 */

int					/* O - Exit status */
set_job_attrs(const char    *command,	/* I - Command name */
              int           job_id,	/* I - Job ID */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* URI for job */


  if (num_options == 0)
    return (0);

  request = ippNewRequest(IPP_SET_JOB_ATTRIBUTES);

  sprintf(uri, "ipp://localhost/jobs/%d", job_id);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "job-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  cupsEncodeOptions(request, num_options, options);

  ippDelete(cupsDoRequest(CUPS_HTTP_DEFAULT, request, "/jobs"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "%s: %s\n", command, cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * End of "$Id$".
 */
