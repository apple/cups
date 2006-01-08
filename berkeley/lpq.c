/*
 * "$Id$"
 *
 *   "lpq" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   main()         - Parse options and commands.
 *   show_jobs()    - Show jobs.
 *   show_printer() - Show printer status.
 *   usage()        - Show program usage.
 */

/*
 * Include necessary headers...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/debug.h>


/*
 * Local functions...
 */

static int	show_jobs(http_t *, const char *, const char *, const int,
		          const int);
static void	show_printer(http_t *, const char *);
static void	usage(void);


/*
 * 'main()' - Parse options and commands.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  const char	*dest,		/* Desired printer */
		*user,		/* Desired user */
		*val;		/* Environment variable name */
  char		*instance;	/* Printer instance */
  int		id,		/* Desired job ID */
		all,		/* All printers */
		interval,	/* Reporting interval */
		longstatus;	/* Show file details */
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests;		/* Destinations */
  cups_lang_t	*language;	/* Language */
#ifdef HAVE_SSL
  http_encryption_t encryption;	/* Encryption? */
#endif /* HAVE_SSL */


  language = cupsLangDefault();

 /*
  * Connect to the scheduler...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
                                 cupsEncryption())) == NULL)
  {
    _cupsLangPuts(stderr, language,
                  _("lpq: Unable to contact server!\n"));
    return (1);
  }

 /*
  * Check for command-line options...
  */

  dest       = NULL;
  user       = NULL;
  id         = 0;
  interval   = 0;
  longstatus = 0;
  all        = 0;
  num_dests  = cupsGetDests(&dests);

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '+')
      interval = atoi(argv[i] + 1);
    else if (argv[i][0] == '-')
    {
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    encryption = HTTP_ENCRYPT_REQUIRED;

	    if (http)
	      httpEncryption(http, encryption);
#else
            _cupsLangPrintf(stderr, language,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'P' : /* Printer */
	    if (argv[i][2])
	      dest = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		httpClose(http);
		cupsFreeDests(num_dests, dests);
	        
	        usage();
	      }

	      dest = argv[i];
	    }

	    if ((instance = strchr(dest, '/')) != NULL)
	      *instance++ = '\0';

            if (cupsGetDest(dest, instance, num_dests, dests) == NULL)
	    {
	      if (instance)
		_cupsLangPrintf(stderr, language,
		                _("lpq: Unknown destination \"%s/%s\"!\n"),
		        	dest, instance);
              else
		_cupsLangPrintf(stderr, language,
		                _("lpq: Unknown destination \"%s\"!\n"), dest);

	      return (1);
	    }
	    break;

	case 'a' : /* All printers */
	    all = 1;
	    break;

	case 'l' : /* Long status */
	    longstatus = 1;
	    break;

	default :
	    httpClose(http);
	    cupsFreeDests(num_dests, dests);

	    usage();
	    break;
      }
    }
    else if (isdigit(argv[i][0] & 255))
      id = atoi(argv[i]);
    else
      user = argv[i];

  if (dest == NULL && !all)
  {
    for (i = 0; i < num_dests; i ++)
      if (dests[i].is_default)
	dest = dests[i].name;

    if (dest == NULL)
    {
      val = NULL;

      if ((dest = getenv("LPDEST")) == NULL)
      {
	if ((dest = getenv("PRINTER")) != NULL)
	{
          if (!strcmp(dest, "lp"))
            dest = NULL;
	  else
	    val = "PRINTER";
	}
      }
      else
	val = "LPDEST";

      if (dest && !cupsGetDest(dest, NULL, num_dests, dests))
	_cupsLangPrintf(stderr, language,
	                _("lp: error - %s environment variable names "
			  "non-existent destination \"%s\"!\n"),
        	        val, dest);
      else
	_cupsLangPuts(stderr, language,
	              _("lpq: error - no default destination available.\n"));
      httpClose(http);
      cupsFreeDests(num_dests, dests);
      return (1);
    }
  }

 /*
  * Show the status in a loop...
  */

  for (;;)
  {
    if (dest)
      show_printer(http, dest);

    i = show_jobs(http, dest, user, id, longstatus);

    if (i && interval)
    {
      fflush(stdout);
      sleep(interval);
    }
    else
      break;
  }

 /*
  * Close the connection to the server and return...
  */

  cupsFreeDests(num_dests, dests);
  httpClose(http);

  return (0);
}


/*
 * 'show_jobs()' - Show jobs.
 */

static int			/* O - Number of jobs in queue */
show_jobs(http_t     *http,	/* I - HTTP connection to server */
          const char *dest,	/* I - Destination */
	  const char *user,	/* I - User */
	  const int  id,	/* I - Job ID */
	  const int  longstatus)/* I - 1 if long report desired */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*jobdest,	/* Pointer into job-printer-uri */
		*jobuser,	/* Pointer to job-originating-user-name */
		*jobname;	/* Pointer to job-name */
  ipp_jstate_t	jobstate;	/* job-state */
  int		jobid,		/* job-id */
		jobsize,	/* job-k-octets */
#ifdef __osf__
		jobpriority,	/* job-priority */
#endif /* __osf__ */
		jobcount,	/* Number of jobs */
		jobcopies,	/* Number of copies */
		rank;		/* Rank of job */
  char		resource[1024];	/* Resource string */
  char		rankstr[255];	/* Rank string */
  char		namestr[1024];	/* Job name string */
  static const char *ranks[10] =/* Ranking strings */
		{
		  "th",
		  "st",
		  "nd",
		  "rd",
		  "th",
		  "th",
		  "th",
		  "th",
		  "th",
		  "th"
		};


  DEBUG_printf(("show_jobs(%08x, %08x, %08x, %d, %d)\n", http, dest, user, id,
                longstatus));

  if (http == NULL)
    return (0);

 /*
  * Build an IPP_GET_JOBS or IPP_GET_JOB_ATTRIBUTES request, which requires
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = id ? IPP_GET_JOB_ATTRIBUTES : IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

  if (dest == NULL)
  {
    if (id)
      sprintf(resource, "ipp://localhost/jobs/%d", id);
    else
      strcpy(resource, "ipp://localhost/jobs");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
                 NULL, resource);
  }
  else
  {
    httpAssembleURIf(resource, sizeof(resource), "ipp", NULL, "localhost", 0,
	             "/printers/%s", dest);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, resource);
  }

  if (user)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, user);
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
  }

 /*
  * Do the request and get back a response...
  */

  jobcount = 0;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, language, _("lpq: get-jobs failed: %s\n"),
        	      ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (0);
    }

    rank = 1;

   /*
    * Loop through the job list and display them...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      jobid       = 0;
      jobsize     = 0;
#ifdef __osf__
      jobpriority = 50;
#endif /* __osf__ */
      jobstate    = IPP_JOB_PENDING;
      jobname     = "untitled";
      jobuser     = NULL;
      jobdest     = NULL;
      jobcopies   = 1;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobsize = attr->values[0].integer;

#ifdef __osf__
        if (strcmp(attr->name, "job-priority") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobpriority = attr->values[0].integer;
#endif /* __osf__ */

        if (strcmp(attr->name, "job-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  jobstate = (ipp_jstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((jobdest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    jobdest ++;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  jobuser = attr->values[0].string.text;

        if (strcmp(attr->name, "job-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  jobname = attr->values[0].string.text;

        if (strcmp(attr->name, "copies") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobcopies = attr->values[0].integer;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (jobdest == NULL || jobid == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

      if (!longstatus && jobcount == 0)
#ifdef __osf__
	_cupsLangPuts(stdout, language,
	              _("Rank   Owner      Pri  Job        Files"
		        "                       Total Size\n"));
#else
	_cupsLangPuts(stdout, language,
	              _("Rank    Owner   Job     File(s)"
		        "                         Total Size\n"));
#endif /* __osf__ */

      jobcount ++;

     /*
      * Display the job...
      */

      if (jobstate == IPP_JOB_PROCESSING)
	strcpy(rankstr, "active");
      else
      {
       /*
        * Make the rank show the "correct" suffix for each number
	* (11-13 are the only special cases, for English anyways...)
	*/

	if ((rank % 100) >= 11 && (rank % 100) <= 13)
	  snprintf(rankstr, sizeof(rankstr), "%dth", rank);
	else
	  snprintf(rankstr, sizeof(rankstr), "%d%s", rank, ranks[rank % 10]);

	rank ++;
      }

      if (longstatus)
      {
        _cupsLangPuts(stdout, language, "");

        if (jobcopies > 1)
	  snprintf(namestr, sizeof(namestr), "%d copies of %s", jobcopies,
	           jobname);
	else
	  strlcpy(namestr, jobname, sizeof(namestr));

        _cupsLangPrintf(stdout, language, _("%s: %-33.33s [job %d localhost]\n"),
	                jobuser, rankstr, jobid);
        _cupsLangPrintf(stdout, language, _("        %-39.39s %.0f bytes\n"),
	                namestr, 1024.0 * jobsize);
      }
      else
#ifdef __osf__
        _cupsLangPrintf(stdout, language,
	                _("%-6s %-10.10s %-4d %-10d %-27.27s %.0f bytes\n"),
			rankstr, jobuser, jobpriority, jobid, jobname,
			1024.0 * jobsize);
#else
        _cupsLangPrintf(stdout, language,
	                _("%-7s %-7.7s %-7d %-31.31s %.0f bytes\n"),
			rankstr, jobuser, jobid, jobname, 1024.0 * jobsize);
#endif /* __osf */

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, language, _("lpq: get-jobs failed: %s\n"),
                    ippErrorString(cupsLastError()));
    return (0);
  }

  if (jobcount == 0)
    _cupsLangPuts(stdout, language, _("no entries\n"));

  return (jobcount);
}


/*
 * 'show_printer()' - Show printer status.
 */

static void
show_printer(http_t     *http,	/* I - HTTP connection to server */
             const char *dest)	/* I - Destination */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  ipp_pstate_t	state;		/* Printer state */
  char		uri[HTTP_MAX_URI];
				/* Printer URI */


  if (http == NULL)
    return;

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
	           "/printers/%s", dest);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, language,
                      _("lpq: get-printer-attributes failed: %s\n"),
        	      ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

    if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
      state = (ipp_pstate_t)attr->values[0].integer;
    else
      state = IPP_PRINTER_STOPPED;

    switch (state)
    {
      case IPP_PRINTER_IDLE :
          _cupsLangPrintf(stdout, language, _("%s is ready\n"), dest);
	  break;
      case IPP_PRINTER_PROCESSING :
          _cupsLangPrintf(stdout, language, _("%s is ready and printing\n"),
	                  dest);
	  break;
      case IPP_PRINTER_STOPPED :
          _cupsLangPrintf(stdout, language, _("%s is not ready\n"), dest);
	  break;
    }

    ippDelete(response);
  }
  else
    _cupsLangPrintf(stderr, language,
                    _("lpq: get-printer-attributes failed: %s\n"),
        	    ippErrorString(cupsLastError()));
}


/*
 * 'usage()' - Show program usage.
 */

static void
usage(void)
{
  _cupsLangPuts(stderr, cupsLangDefault(),
                _("Usage: lpq [-P dest] [-l] [+interval]\n"));
  exit(1);
}


/*
 * End of "$Id$".
 */
