/*
 * "$Id: lpr.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * "lpr" command for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/cups-private.h>


/*
 * 'main()' - Parse options and send files for printing.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j;			/* Looping var */
  int		job_id;			/* Job ID */
  char		ch;			/* Option character */
  char		*printer,		/* Destination printer or class */
		*instance;		/* Instance */
  const char	*title,			/* Job title */
		*val;			/* Environment variable name */
  int		num_copies;		/* Number of copies per file */
  int		num_files;		/* Number of files to print */
  const char	*files[1000];		/* Files to print */
  cups_dest_t	*dest;			/* Selected destination */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		deletefile;		/* Delete file after print? */
  char		buffer[8192];		/* Copy buffer */


  _cupsSetLocale(argv);

  deletefile  = 0;
  printer     = NULL;
  dest        = NULL;
  num_options = 0;
  options     = NULL;
  num_files   = 0;
  title       = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      switch (ch = argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr, _("%s: Sorry, no encryption support."),
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
				  "\"-U\" option."), argv[0]);
	        return (1);
	      }

              cupsSetUser(argv[i]);
	    }
	    break;

        case 'H' : /* Connect to host */
	    if (argv[i][2] != '\0')
              cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected hostname after "
			          "\"-H\" option."), argv[0]);
		return (1);
              }
	      else
                cupsSetServer(argv[i]);
	    }
	    break;

	case '1' : /* TROFF font set 1 */
	case '2' : /* TROFF font set 2 */
	case '3' : /* TROFF font set 3 */
	case '4' : /* TROFF font set 4 */
	case 'i' : /* indent */
	case 'w' : /* width */
	    if (argv[i][2] == '\0')
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPrintf(stderr,
		                _("%s: Error - expected value after \"-%c\" "
				  "option."), argv[0], ch);
		return (1);
	      }
	    }

        case 'c' : /* CIFPLOT */
	case 'd' : /* DVI */
	case 'f' : /* FORTRAN */
	case 'g' : /* plot */
	case 'n' : /* Ditroff */
	case 't' : /* Troff */
	case 'v' : /* Raster image */
	    _cupsLangPrintf(stderr,
	                    _("%s: Warning - \"%c\" format modifier not "
			      "supported - output may not be correct."),
			    argv[0], ch);
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
		                _("%s: Error - expected option=value after "
			          "\"-o\" option."), argv[0]);
		return (1);
	      }

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

	case 'l' : /* Literal/raw */
            num_options = cupsAddOption("raw", "true", num_options, &options);
	    break;

	case 'p' : /* Prettyprint */
            num_options = cupsAddOption("prettyprint", "true", num_options,
	                                &options);
	    break;

	case 'h' : /* Suppress burst page */
            num_options = cupsAddOption("job-sheets", "none", num_options,
	                                &options);
	    break;

	case 's' : /* Don't use symlinks */
	    break;

	case 'm' : /* Mail on completion */
	    {
	      char	email[1024];	/* EMail address */


	      snprintf(email, sizeof(email), "mailto:%s@%s", cupsUser(),
	               httpGetHostname(NULL, buffer, sizeof(buffer)));
	      num_options = cupsAddOption("notify-recipient-uri", email,
	                                  num_options, &options);
	    }
	    break;

	case 'q' : /* Queue file but don't print */
            num_options = cupsAddOption("job-hold-until", "indefinite",
	                                num_options, &options);
	    break;

	case 'r' : /* Remove file after printing */
	    deletefile = 1;
	    break;

        case 'P' : /* Destination printer or class */
	    if (argv[i][2] != '\0')
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected destination after "
			          "\"-P\" option."), argv[0]);
		return (1);
	      }

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

            if ((dest = cupsGetNamedDest(NULL, printer, instance)) != NULL)
	    {
	      for (j = 0; j < dest->num_options; j ++)
	        if (cupsGetOption(dest->options[j].name, num_options,
		                  options) == NULL)
	          num_options = cupsAddOption(dest->options[j].name,
		                              dest->options[j].value,
					      num_options, &options);
	    }
	    else if (cupsLastError() == IPP_STATUS_ERROR_BAD_REQUEST ||
		     cupsLastError() == IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED)
	    {
	      _cupsLangPrintf(stderr,
			      _("%s: Error - add '/version=1.1' to server "
				"name."), argv[0]);
	      return (1);
	    }
	    break;

	case '#' : /* Number of copies */
	    if (argv[i][2] != '\0')
	      num_copies = atoi(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected copies after "
			          "\"-#\" option."), argv[0]);
		return (1);
	      }

	      num_copies = atoi(argv[i]);
	    }

            sprintf(buffer, "%d", num_copies);
            num_options = cupsAddOption("copies", buffer, num_options, &options);
	    break;

	case 'C' : /* Class */
	case 'J' : /* Job name */
	case 'T' : /* Title */
	    if (argv[i][2] != '\0')
	      title = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
		_cupsLangPrintf(stderr,
		                _("%s: Error - expected name after \"-%c\" "
				  "option."), argv[0], ch);
		return (1);
	      }

	      title = argv[i];
	    }
	    break;

	default :
	    _cupsLangPrintf(stderr,
	                    _("%s: Error - unknown option \"%c\"."), argv[0],
			    argv[i][1]);
	    return (1);
      }
    }
    else if (num_files < 1000)
    {
     /*
      * Print a file...
      */

      if (access(argv[i], R_OK) != 0)
      {
        _cupsLangPrintf(stderr,
	                _("%s: Error - unable to access \"%s\" - %s"),
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
      _cupsLangPrintf(stderr,
                      _("%s: Error - too many files - \"%s\"."), argv[0],
		      argv[i]);
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
    else if (cupsLastError() == IPP_STATUS_ERROR_BAD_REQUEST ||
	     cupsLastError() == IPP_STATUS_ERROR_VERSION_NOT_SUPPORTED)
    {
      _cupsLangPrintf(stderr,
		      _("%s: Error - add '/version=1.1' to server "
			"name."), argv[0]);
      return (1);
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
		        "non-existent destination \"%s\"."), argv[0], val,
		      printer);
    else if (cupsLastError() == IPP_NOT_FOUND)
      _cupsLangPrintf(stderr,
                      _("%s: Error - no default destination available."),
		      argv[0]);
    else
      _cupsLangPrintf(stderr, _("%s: Error - scheduler not responding."),
		      argv[0]);

    return (1);
  }

  if (num_files > 0)
  {
    job_id = cupsPrintFiles(printer, num_files, files, title, num_options, options);

    if (deletefile && job_id > 0)
    {
     /*
      * Delete print files after printing...
      */

      for (i = 0; i < num_files; i ++)
        unlink(files[i]);
    }
  }
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
      status = cupsWriteRequestData(CUPS_HTTP_DEFAULT, buffer, (size_t)bytes);

    if (status != HTTP_CONTINUE)
    {
      _cupsLangPrintf(stderr, _("%s: Error - unable to queue from stdin - %s."),
		      argv[0], httpStatus(status));
      cupsFinishDocument(CUPS_HTTP_DEFAULT, printer);
      cupsCancelJob2(CUPS_HTTP_DEFAULT, printer, job_id, 0);
      return (1);
    }

    if (cupsFinishDocument(CUPS_HTTP_DEFAULT, printer) != IPP_OK)
    {
      _cupsLangPrintf(stderr, "%s: %s", argv[0], cupsLastErrorString());
      cupsCancelJob2(CUPS_HTTP_DEFAULT, printer, job_id, 0);
      return (1);
    }
  }

  if (job_id < 1)
  {
    _cupsLangPrintf(stderr, "%s: %s", argv[0], cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * End of "$Id: lpr.c 11558 2014-02-06 18:33:34Z msweet $".
 */
