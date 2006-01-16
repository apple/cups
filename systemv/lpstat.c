/*
 * "$Id$"
 *
 *   "lpstat" command for the Common UNIX Printing System (CUPS).
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
 *   main()           - Parse options and show status information.
 *   check_dest()     - Verify that the named destination(s) exists.
 *   connect_server() - Connect to the server as necessary...
 *   show_accepting() - Show acceptance status.
 *   show_classes()   - Show printer classes.
 *   show_default()   - Show default destination.
 *   show_devices()   - Show printer devices.
 *   show_jobs()      - Show active print jobs.
 *   show_printers()  - Show printers.
 *   show_scheduler() - Show scheduler status.
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
#include <cups/debug.h>


/*
 * Local functions...
 */

static void	check_dest(http_t *, const char *, int *, cups_dest_t **);
static http_t	*connect_server(http_t *);
static int	show_accepting(http_t *, const char *, int, cups_dest_t *);
static int	show_classes(http_t *, const char *);
static void	show_default(int, cups_dest_t *);
static int	show_devices(http_t *, const char *, int, cups_dest_t *);
static int	show_jobs(http_t *, const char *, const char *, int, int,
		          const char *);
static int	show_printers(http_t *, const char *, int, cups_dest_t *, int);
static void	show_scheduler(http_t *);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i,		/* Looping var */
		status;		/* Exit status */
  http_t	*http;		/* Connection to server */
  int		num_dests;	/* Number of user destinations */
  cups_dest_t	*dests;		/* User destinations */
  int		long_status;	/* Long status report? */
  int		ranking;	/* Show job ranking? */
  const char	*which;		/* Which jobs to show? */
  char		op;		/* Last operation on command-line */


#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif /* LC_TIME */

  http        = NULL;
  num_dests   = 0;
  dests       = NULL;
  long_status = 0;
  ranking     = 0;
  status      = 0;
  which       = "not-completed";
  op          = 0;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'D' : /* Show description */
	    long_status = 1;
	    break;

        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);

	    if (http)
	      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'P' : /* Show paper types */
	    op = 'P';
	    break;

        case 'R' : /* Show ranking */
	    ranking = 1;
	    break;

        case 'S' : /* Show charsets */
	    op = 'S';
	    if (!argv[i][2])
	      i ++;
	    break;

        case 'W' : /* Show which jobs? */
	    if (argv[i][2])
	      which = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("lpstat: Need \"completed\", "
			        "\"not-completed\", or \"all\" after -W!\n"));
		return (1);
              }

	      which = argv[i];
	    }

            if (strcmp(which, "completed") && strcmp(which, "not-completed") &&
	        strcmp(which, "all"))
	    {
	      _cupsLangPuts(stderr,
		            _("lpstat: Need \"completed\", "
			      "\"not-completed\", or \"all\" after -W!\n"));
	      return (1);
	    }
	    break;

        case 'a' : /* Show acceptance status */
	    op   = 'a';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      status |= show_accepting(http, argv[i] + 2, num_dests, dests);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(http, argv[i], &num_dests, &dests);

	      status |= show_accepting(http, argv[i], num_dests, dests);
	    }
	    else
	    {
              if (num_dests == 0)
		num_dests = cupsGetDests2(http, &dests);

	      status |= show_accepting(http, NULL, num_dests, dests);
	    }
	    break;

#ifdef __sgi
        case 'b' : /* Show both the local and remote status */
	    op   = 'b';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
	     /*
	      * The local and remote status are separated by a blank line;
	      * since all CUPS jobs are networked, we only output the
	      * second list for now...  In the future, we might further
	      * emulate this by listing the remote server's queue, but
	      * for now this is enough to make the SGI printstatus program
	      * happy...
	      */

              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      puts("");
	      status |= show_jobs(http, argv[i] + 2, NULL, 3, ranking, which);
	    }
	    else
	    {
	      _cupsLangPuts(stderr,
	                    _("lpstat: The -b option requires a destination "
			      "argument.\n"));

	      return (1);
	    }
	    break;
#endif /* __sgi */

        case 'c' : /* Show classes and members */
	    op   = 'c';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      status |= show_classes(http, argv[i] + 2);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(http, argv[i], &num_dests, &dests);

	      status |= show_classes(http, argv[i]);
	    }
	    else
	      status |= show_classes(http, NULL);
	    break;

        case 'd' : /* Show default destination */
	    op   = 'd';
	    http = connect_server(http);

            if (num_dests == 0)
	      num_dests = cupsGetDests2(http, &dests);

            show_default(num_dests, dests);
	    break;

        case 'f' : /* Show forms */
	    op   = 'f';
	    if (!argv[i][2])
	      i ++;
	    break;

        case 'h' : /* Connect to host */
	    if (http)
	    {
	      httpClose(http);
	      http = NULL;
	    }

	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
	                      _("Error: need hostname after \'-h\' option!\n"));
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

        case 'l' : /* Long status or long job status */
#ifdef __sgi
	    op   = 'l';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      status |= show_jobs(http, argv[i] + 2, NULL, 3, ranking, which);
	    }
	    else
#endif /* __sgi */
	      long_status = 2;
	    break;

        case 'o' : /* Show jobs by destination */
	    op   = 'o';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      status |= show_jobs(http, argv[i] + 2, NULL, long_status,
	                          ranking, which);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(http, argv[i], &num_dests, &dests);

	      status |= show_jobs(http, argv[i], NULL, long_status,
	                          ranking, which);
	    }
	    else
	      status |= show_jobs(http, NULL, NULL, long_status,
	                          ranking, which);
	    break;

        case 'p' : /* Show printers */
	    op   = 'p';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      status |= show_printers(http, argv[i] + 2, num_dests, dests, long_status);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(http, argv[i], &num_dests, &dests);

	      status |= show_printers(http, argv[i], num_dests, dests, long_status);
	    }
	    else
	    {
              if (num_dests == 0)
		num_dests = cupsGetDests2(http, &dests);

	      status |= show_printers(http, NULL, num_dests, dests, long_status);
	    }
	    break;

        case 'r' : /* Show scheduler status */
	    op   = 'r';
	    http = connect_server(http);

	    show_scheduler(http);
	    break;

        case 's' : /* Show summary */
	    op   = 's';
	    http = connect_server(http);

            if (num_dests == 0)
	      num_dests = cupsGetDests2(http, &dests);

	    show_default(num_dests, dests);
	    status |= show_classes(http, NULL);
	    status |= show_devices(http, NULL, num_dests, dests);
	    break;

        case 't' : /* Show all info */
	    op   = 't';
	    http = connect_server(http);

            if (num_dests == 0)
	      num_dests = cupsGetDests2(http, &dests);

	    show_scheduler(http);
	    show_default(num_dests, dests);
	    status |= show_classes(http, NULL);
	    status |= show_devices(http, NULL, num_dests, dests);
	    status |= show_accepting(http, NULL, num_dests, dests);
	    status |= show_printers(http, NULL, num_dests, dests, long_status);
	    status |= show_jobs(http, NULL, NULL, long_status, ranking, which);
	    break;

        case 'u' : /* Show jobs by user */
	    op   = 'u';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	      status |= show_jobs(http, NULL, argv[i] + 2, long_status,
	                          ranking, which);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      status |= show_jobs(http, NULL, argv[i], long_status,
	                          ranking, which);
	    }
	    else
	      status |= show_jobs(http, NULL, NULL, long_status,
	                          ranking, which);
	    break;

        case 'v' : /* Show printer devices */
	    op   = 'v';
	    http = connect_server(http);

	    if (argv[i][2] != '\0')
	    {
              check_dest(http, argv[i] + 2, &num_dests, &dests);

	      status |= show_devices(http, argv[i] + 2, num_dests, dests);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(http, argv[i], &num_dests, &dests);

	      status |= show_devices(http, argv[i], num_dests, dests);
	    }
	    else
	    {
              if (num_dests == 0)
		num_dests = cupsGetDests2(http, &dests);

	      status |= show_devices(http, NULL, num_dests, dests);
	    }
	    break;


	default :
	    _cupsLangPrintf(stderr,
	                    _("lpstat: Unknown option \'%c\'!\n"), argv[i][1]);
	    return (1);
      }
    else
    {
      http = connect_server(http);

      status |= show_jobs(http, argv[i], NULL, long_status, ranking, which);
      op = 'o';
    }

  if (!op)
  {
    http = connect_server(http);

    status |= show_jobs(http, NULL, cupsUser(), long_status, ranking, which);
  }

  return (status);
}


/*
 * 'check_dest()' - Verify that the named destination(s) exists.
 */

static void
check_dest(http_t      *http,		/* I  - HTTP connection */
           const char  *name,		/* I  - Name of printer/class(es) */
           int         *num_dests,	/* IO - Number of destinations */
	   cups_dest_t **dests)		/* IO - Destinations */
{
  const char	*dptr;
  char		*pptr,
		printer[128];


 /*
  * Load the destination list as necessary...
  */

  if (*num_dests == 0)
    *num_dests = cupsGetDests2(http, dests);

 /*
  * Scan the name string for printer/class name(s)...
  */

  for (dptr = name; *dptr != '\0';) 
  {
   /*
    * Skip leading whitespace and commas...
    */

    while (isspace(*dptr & 255) || *dptr == ',')
      dptr ++;

    if (*dptr == '\0')
      break;

   /*
    * Extract a single destination name from the name string...
    */

    for (pptr = printer; !isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0';)
    {
      if ((pptr - printer) < (sizeof(printer) - 1))
        *pptr++ = *dptr++;
      else
      {
        _cupsLangPrintf(stderr,
	                _("lpstat: Invalid destination name in list \"%s\"!\n"),
			name);
        exit(1);
      }
    }

    *pptr = '\0';

   /*
    * Check the destination...
    */

    if (cupsGetDest(printer, NULL, *num_dests, *dests) == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("lpstat: Unknown destination \"%s\"!\n"), printer);
      exit(1);
    }
  }
}


/*
 * 'connect_server()' - Connect to the server as necessary...
 */

static http_t *				/* O - New HTTP connection */
connect_server(http_t *http)		/* I - Current HTTP connection */
{
  if (!http)
  {
    http = httpConnectEncrypt(cupsServer(), ippPort(),
	                      cupsEncryption());

    if (http == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("lpstat: Unable to connect to server %s on port %d: %s\n"),
		      cupsServer(), ippPort(), strerror(errno));
      exit(1);
    }
  }

  return (http);
}


/*
 * 'show_accepting()' - Show acceptance status.
 */

static int				/* O - 0 on success, 1 on fail */
show_accepting(http_t      *http,	/* I - HTTP connection to server */
               const char  *printers,	/* I - Destinations */
               int         num_dests,	/* I - Number of user-defined dests */
	       cups_dest_t *dests)	/* I - User-defined destinations */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*printer,		/* Printer name */
		*message;		/* Printer device URI */
  int		accepting;		/* Accepting requests? */
  time_t	ptime;			/* Printer state time */
  struct tm	*pdate;			/* Printer state date & time */
  char		printer_state_time[255];/* Printer state time */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  static const char *pattrs[] =		/* Attributes we need for printers... */
		{
		  "printer-name",
		  "printer-state-change-time",
		  "printer-state-message",
		  "printer-is-accepting-jobs"
		};


  DEBUG_printf(("show_accepting(%p, %p)\n", http, printers));

  if (http == NULL)
    return (1);

  if (printers != NULL && !strcmp(printers, "all"))
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_accepting: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

   /*
    * Loop through the printers returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a printer...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this printer...
      */

      printer   = NULL;
      message   = NULL;
      accepting = 1;
      ptime     = 0;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "printer-name") &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-state-change-time") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  ptime = (time_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "printer-state-message") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-is-accepting-jobs") &&
	         attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = printers == NULL;

      if (printers != NULL)
      {
        for (dptr = printers; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr & 255) == tolower(*dptr & 255);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
        pdate = localtime(&ptime);
        strftime(printer_state_time, sizeof(printer_state_time), "%c", pdate);

        if (accepting)
	  _cupsLangPrintf(stdout, _("%s accepting requests since %s\n"),
			  printer, printer_state_time);
	else
	  _cupsLangPrintf(stdout, _("%s not accepting requests since %s -\n"
			            "\t%s\n"),
			  printer, printer_state_time,
			  message == NULL ? "reason unknown" : message);

        for (i = 0; i < num_dests; i ++)
	  if (!strcasecmp(dests[i].name, printer) && dests[i].instance)
	  {
            if (accepting)
	      _cupsLangPrintf(stdout, _("%s/%s accepting requests since %s\n"),
			      printer, dests[i].instance, printer_state_time);
	    else
	      _cupsLangPrintf(stdout, _("%s/%s not accepting requests since "
			                "%s -\n\t%s\n"),
			      printer, dests[i].instance, printer_state_time,
	        	      message == NULL ? "reason unknown" : message);
	  }
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'show_classes()' - Show printer classes.
 */

static int				/* O - 0 on success, 1 on fail */
show_classes(http_t     *http,		/* I - HTTP connection to server */
             const char *dests)		/* I - Destinations */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response,		/* IPP Response */
		*response2;		/* IPP response from remote server */
  http_t	*http2;			/* Remote server */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*printer,		/* Printer class name */
		*printer_uri;		/* Printer class URI */
  ipp_attribute_t *members;		/* Printer members */
  char		method[HTTP_MAX_URI],	/* Request method */
		username[HTTP_MAX_URI],	/* Username:password */
		server[HTTP_MAX_URI],	/* Server name */
		resource[HTTP_MAX_URI];	/* Resource name */
  int		port;			/* Port number */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  static const char *cattrs[] =		/* Attributes we need for classes... */
		{
		  "printer-name",
		  "printer-uri-supported",
		  "member-names"
		};


  DEBUG_printf(("show_classes(%p, %p)\n", http, dests));

  if (http == NULL)
    return (1);

  if (dests != NULL && !strcmp(dests, "all"))
    dests = NULL;

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_GET_CLASSES);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(cattrs) / sizeof(cattrs[0]),
		NULL, cattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_classes: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

   /*
    * Loop through the printers returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer     = NULL;
      printer_uri = NULL;
      members     = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "printer-name") &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (!strcmp(attr->name, "printer-uri-supported") &&
	    attr->value_tag == IPP_TAG_URI)
	  printer_uri = attr->values[0].string.text;

        if (!strcmp(attr->name, "member-names") &&
	    attr->value_tag == IPP_TAG_NAME)
	  members = attr;

        attr = attr->next;
      }

     /*
      * If this is a remote class, grab the class info from the
      * remote server...
      */

      response2 = NULL;
      if (members == NULL && printer_uri != NULL)
      {
        httpSeparateURI(printer_uri, method, sizeof(method),
	                username, sizeof(username), server, sizeof(server),
			&port, resource, sizeof(resource));

        if (!strcasecmp(server, http->hostname))
	  http2 = http;
	else
	  http2 = httpConnectEncrypt(server, port, cupsEncryption());

	if (http2 != NULL)
	{
	 /*
	  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
	  * following attributes:
	  *
	  *    attributes-charset
	  *    attributes-natural-language
	  *    printer-uri
	  *    requested-attributes
	  */

	  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	       "printer-uri", NULL, printer_uri);

	  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                	"requested-attributes",
		        sizeof(cattrs) / sizeof(cattrs[0]),
			NULL, cattrs);

          if ((response2 = cupsDoRequest(http2, request, "/")) != NULL)
	    members = ippFindAttribute(response2, "member-names", IPP_TAG_NAME);

          if (http2 != http)
            httpClose(http2);
        }
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (response2)
	  ippDelete(response2);

        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = dests == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr & 255) == tolower(*dptr & 255);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
        _cupsLangPrintf(stdout, _("members of class %s:\n"), printer);

	if (members)
	{
	  for (i = 0; i < members->num_values; i ++)
	    _cupsLangPrintf(stdout, "\t%s\n",
	                    members->values[i].string.text);
        }
	else
	  _cupsLangPuts(stdout, "\tunknown\n");
      }

      if (response2)
	ippDelete(response2);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'show_default()' - Show default destination.
 */

static void
show_default(int         num_dests,	/* I - Number of user-defined dests */
	     cups_dest_t *dests)	/* I - User-defined destinations */
{
  cups_dest_t	*dest;			/* Destination */
  const char	*printer,		/* Printer name */
		*val;			/* Environment variable name */

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
  {
    if (dest->instance)
      _cupsLangPrintf(stdout, _("system default destination: %s/%s\n"),
                      dest->name, dest->instance);
    else
      _cupsLangPrintf(stdout, _("system default destination: %s\n"),
                      dest->name);
  }
  else
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
      _cupsLangPrintf(stdout,
                      _("lpstat: error - %s environment variable names "
		        "non-existent destination \"%s\"!\n"),
        	      val, printer);
    else
      _cupsLangPuts(stdout, _("no system default destination\n"));
  }
}


/*
 * 'show_devices()' - Show printer devices.
 */

static int				/* O - 0 on success, 1 on fail */
show_devices(http_t      *http,		/* I - HTTP connection to server */
             const char  *printers,	/* I - Destinations */
             int         num_dests,	/* I - Number of user-defined dests */
	     cups_dest_t *dests)	/* I - User-defined destinations */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*printer,		/* Printer name */
		*uri,			/* Printer URI */
		*device,		/* Printer device URI */
		*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  static const char *pattrs[] =		/* Attributes we need for printers... */
		{
		  "printer-name",
		  "printer-uri-supported",
		  "device-uri"
		};


  DEBUG_printf(("show_devices(%p, %p)\n", http, dests));

  if (http == NULL)
    return (1);

  if (printers != NULL && !strcmp(printers, "all"))
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_devices: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

   /*
    * Loop through the printers returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer = NULL;
      device  = NULL;
      uri     = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "printer-name") &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (!strcmp(attr->name, "printer-uri-supported") &&
	    attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;

        if (!strcmp(attr->name, "device-uri") &&
	    attr->value_tag == IPP_TAG_URI)
	  device = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = printers == NULL;

      if (printers != NULL)
      {
        for (dptr = printers; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr & 255) == tolower(*dptr & 255);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
#ifdef __osf__ /* Compaq/Digital like to do it their own way... */
        char	method[HTTP_MAX_URI],	/* Components of printer URI */
		username[HTTP_MAX_URI],
		hostname[HTTP_MAX_URI],
		resource[HTTP_MAX_URI];
	int	port;


        if (device == NULL)
	{
	  httpSeparate(uri, method, username, hostname, &port, resource);
          _cupsLangPrintf(stdout,
	                  _("Output for printer %s is sent to remote "
			    "printer %s on %s\n"),
	        	  printer, strrchr(resource, '/') + 1, hostname);
        }
        else if (!strncmp(device, "file:", 5))
          _cupsLangPrintf(stdout,
	                  _("Output for printer %s is sent to %s\n"),
			  printer, device + 5);
        else
          _cupsLangPrintf(stdout,
	                  _("Output for printer %s is sent to %s\n"),
			  printer, device);

        for (i = 0; i < num_dests; i ++)
	  if (!strcasecmp(printer, dests[i].name) && dests[i].instance)
	  {
            if (device == NULL)
              _cupsLangPrintf(stdout,
	                      _("Output for printer %s/%s is sent to "
			        "remote printer %s on %s\n"),
	        	      printer, dests[i].instance,
			      strrchr(resource, '/') + 1, hostname);
            else if (!strncmp(device, "file:", 5))
              _cupsLangPrintf(stdout,
	                      _("Output for printer %s/%s is sent to %s\n"),
			      printer, dests[i].instance, device + 5);
            else
              _cupsLangPrintf(stdout,
	                      _("Output for printer %s/%s is sent to %s\n"),
			      printer, dests[i].instance, device);
	  }
#else
        if (device == NULL)
          _cupsLangPrintf(stdout, _("device for %s: %s\n"),
	                  printer, uri);
        else if (!strncmp(device, "file:", 5))
          _cupsLangPrintf(stdout, _("device for %s: %s\n"),
	                  printer, device + 5);
        else
          _cupsLangPrintf(stdout, _("device for %s: %s\n"),
	                  printer, device);

        for (i = 0; i < num_dests; i ++)
	  if (!strcasecmp(printer, dests[i].name) && dests[i].instance)
	  {
            if (device == NULL)
              _cupsLangPrintf(stdout, _("device for %s/%s: %s\n"),
	                      printer, dests[i].instance, uri);
            else if (!strncmp(device, "file:", 5))
              _cupsLangPrintf(stdout, _("device for %s/%s: %s\n"),
	                      printer, dests[i].instance, device + 5);
            else
              _cupsLangPrintf(stdout, _("device for %s/%s: %s\n"),
	                      printer, dests[i].instance, device);
	  }
#endif /* __osf__ */
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'show_jobs()' - Show active print jobs.
 */

static int				/* O - 0 on success, 1 on fail */
show_jobs(http_t     *http,		/* I - HTTP connection to server */
          const char *dests,		/* I - Destinations */
          const char *users,		/* I - Users */
          int        long_status,	/* I - Show long status? */
          int        ranking,		/* I - Show job ranking? */
	  const char *which)		/* I - Show which jobs? */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*dest,			/* Pointer into job-printer-uri */
		*username,		/* Pointer to job-originating-user-name */
		*title;			/* Pointer to job-name */
  int		rank,			/* Rank in queue */
		jobid,			/* job-id */
		size;			/* job-k-octets */
  time_t	jobtime;		/* time-at-creation */
  struct tm	*jobdate;		/* Date & time */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  char		temp[255],		/* Temporary buffer */
		date[255];		/* Date buffer */
  static const char *jattrs[] =		/* Attributes we need for jobs... */
		{
		  "job-id",
		  "job-k-octets",
		  "job-name",
		  "time-at-creation",
		  "job-printer-uri",
		  "job-originating-user-name"
		};


  DEBUG_printf(("show_jobs(%p, %p, %p)\n", http, dests, users));

  if (http == NULL)
    return (1);

  if (dests != NULL && !strcmp(dests, "all"))
    dests = NULL;

 /*
  * Build a IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  *    requested-attributes
  */

  request = ippNewRequest(IPP_GET_JOBS);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(jattrs) / sizeof(jattrs[0]),
		NULL, jattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
               NULL, "ipp://localhost/jobs/");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs",
               NULL, which);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Loop through the job list and display them...
    */

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

    rank = -1;

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

      jobid    = 0;
      size     = 0;
      username = NULL;
      dest     = NULL;
      jobtime  = 0;
      title    = "no title";

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (!strcmp(attr->name, "job-id") &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (!strcmp(attr->name, "job-k-octets") &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer;

        if (!strcmp(attr->name, "time-at-creation") &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobtime = attr->values[0].integer;

        if (!strcmp(attr->name, "job-printer-uri") &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;

        if (!strcmp(attr->name, "job-originating-user-name") &&
	    attr->value_tag == IPP_TAG_NAME)
	  username = attr->values[0].string.text;

        if (!strcmp(attr->name, "job-name") &&
	    attr->value_tag == IPP_TAG_NAME)
	  title = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (dest == NULL || jobid == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a job we're interested in...
      */

      match = (dests == NULL && users == NULL);
      rank ++;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = dest;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr & 255) == tolower(*dptr & 255);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

      if (users != NULL && username != NULL)
      {
        for (dptr = users; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = username;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the job...
      */

      if (match)
      {
        jobdate = localtime(&jobtime);
        snprintf(temp, sizeof(temp), "%s-%d", dest, jobid);

        if (long_status == 3)
	{
	 /*
	  * Show the consolidated output format for the SGI tools...
	  */

	  if (!strftime(date, sizeof(date), "%b %d %H:%M", jobdate))
	    strcpy(date, "Unknown");

	  _cupsLangPrintf(stdout, "%s;%s;%d;%s;%s\n",
	                  temp, username ? username : "unknown",
	        	  size, title ? title : "unknown", date);
	}
	else
	{
	  if (!strftime(date, sizeof(date), "%c", jobdate))
	    strcpy(date, "Unknown");

          if (ranking)
	    _cupsLangPrintf(stdout, "%3d %-21s %-13s %8.0f %s\n",
	                    rank, temp, username ? username : "unknown",
			    1024.0 * size, date);
          else
	    _cupsLangPrintf(stdout, "%-23s %-13s %8.0f   %s\n",
	                    temp, username ? username : "unknown",
			    1024.0 * size, date);
          if (long_status)
	    _cupsLangPrintf(stdout, _("\tqueued for %s\n"), dest);
	}
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'show_printers()' - Show printers.
 */

static int				/* O - 0 on success, 1 on fail */
show_printers(http_t      *http,	/* I - HTTP connection to server */
              const char  *printers,	/* I - Destinations */
              int         num_dests,	/* I - Number of user-defined dests */
	      cups_dest_t *dests,	/* I - User-defined destinations */
              int         long_status)	/* I - Show long status? */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response,		/* IPP Response */
		*jobs;			/* IPP Get Jobs response */
  ipp_attribute_t *attr,		/* Current attribute */
		*jobattr,		/* Job ID attribute */
		*reasons;		/* Job state reasons attribute */
  const char	*printer,		/* Printer name */
		*message,		/* Printer state message */
		*description,		/* Description of printer */
		*location,		/* Location of printer */
		*make_model,		/* Make and model of printer */
		*uri;			/* URI of printer */
  ipp_attribute_t *allowed,		/* requesting-user-name-allowed */
		*denied;		/* requestint-user-name-denied */
  ipp_pstate_t	pstate;			/* Printer state */
  cups_ptype_t	ptype;			/* Printer type */
  time_t	ptime;			/* Printer state time */
  struct tm	*pdate;			/* Printer state date & time */
  int		jobid;			/* Job ID of current job */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  char		printer_uri[HTTP_MAX_URI],
					/* Printer URI */
		printer_state_time[255];/* Printer state time */
  const char	*root;			/* Server root directory... */
  static const char *pattrs[] =		/* Attributes we need for printers... */
		{
		  "printer-name",
		  "printer-state",
		  "printer-state-message",
		  "printer-state-reasons",
		  "printer-state-change-time",
		  "printer-type",
		  "printer-info",
                  "printer-location",
		  "printer-make-and-model",
		  "printer-uri-supported",
		  "requesting-user-name-allowed",
		  "requesting-user-name-denied"
		};
  static const char *jattrs[] =		/* Attributes we need for jobs... */
		{
		  "job-id"
		};


  DEBUG_printf(("show_printers(%p, %p)\n", http, dests));

  if (http == NULL)
    return (1);

  if ((root = getenv("CUPS_SERVERROOT")) == NULL)
    root = CUPS_SERVERROOT;

  if (printers != NULL && !strcmp(printers, "all"))
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_printers: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
      ippDelete(response);
      return (1);
    }

   /*
    * Loop through the printers returned in the list and display
    * their status...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      printer     = NULL;
      ptime       = 0;
      ptype       = CUPS_PRINTER_LOCAL;
      pstate      = IPP_PRINTER_IDLE;
      message     = NULL;
      description = NULL;
      location    = NULL;
      make_model  = NULL;
      reasons     = NULL;
      uri         = NULL;
      jobid       = 0;
      allowed     = NULL;
      denied      = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "printer-name") &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-state") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "printer-type") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  ptype = (cups_ptype_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "printer-state-message") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-state-change-time") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  ptime = (time_t)attr->values[0].integer;
	else if (!strcmp(attr->name, "printer-info") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  description = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-location") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  location = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-make-and-model") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  make_model = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-uri-supported") &&
	         attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-state-reasons") &&
	         attr->value_tag == IPP_TAG_KEYWORD)
	  reasons = attr;
        else if (!strcmp(attr->name, "requesting-user-name-allowed") &&
	         attr->value_tag == IPP_TAG_NAME)
	  allowed = attr;
        else if (!strcmp(attr->name, "requesting-user-name-denied") &&
	         attr->value_tag == IPP_TAG_NAME)
	  denied = attr;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = printers == NULL;

      if (printers != NULL)
      {
        for (dptr = printers; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr & 255) == tolower(*dptr & 255);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr & 255)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr & 255) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr & 255) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
       /*
        * If the printer state is "IPP_PRINTER_PROCESSING", then grab the
	* current job for the printer.
	*/

        if (pstate == IPP_PRINTER_PROCESSING)
	{
	 /*
	  * Build an IPP_GET_JOBS request, which requires the following
	  * attributes:
	  *
	  *    attributes-charset
	  *    attributes-natural-language
	  *    printer-uri
	  *    limit
          *    requested-attributes
	  */

	  request = ippNewRequest(IPP_GET_JOBS);

	  request->request.op.operation_id = IPP_GET_JOBS;
	  request->request.op.request_id   = 1;

	  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                	"requested-attributes",
		        sizeof(jattrs) / sizeof(jattrs[0]), NULL, jattrs);

	  httpAssembleURIf(printer_uri, sizeof(printer_uri), "ipp", NULL,
	                   "localhost", 0, "/printers/%s", printer);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	               "printer-uri", NULL, printer_uri);

          if ((jobs = cupsDoRequest(http, request, "/")) != NULL)
	  {
	   /*
	    * Get the current active job on this queue...
	    */

	    jobid = 0;

	    for (jobattr = jobs->attrs; jobattr; jobattr = jobattr->next)
	    {
	      if (!jobattr->name)
	        continue;

	      if (!strcmp(jobattr->name, "job-id") &&
	          jobattr->value_tag == IPP_TAG_INTEGER)
		jobid = jobattr->values[0].integer;
              else if (!strcmp(jobattr->name, "job-state") &&
	               jobattr->value_tag == IPP_TAG_ENUM &&
		       jobattr->values[0].integer == IPP_JOB_PROCESSING)
		break;
	    }

            ippDelete(jobs);
	  }
        }

       /*
        * Display it...
	*/

        pdate = localtime(&ptime);
        strftime(printer_state_time, sizeof(printer_state_time), "%c", pdate);

        switch (pstate)
	{
	  case IPP_PRINTER_IDLE :
	      _cupsLangPrintf(stdout,
	                      _("printer %s is idle.  enabled since %s\n"),
			      printer, printer_state_time);
	      break;
	  case IPP_PRINTER_PROCESSING :
	      _cupsLangPrintf(stdout,
	                      _("printer %s now printing %s-%d.  "
			        "enabled since %s\n"),
	        	      printer, printer, jobid, printer_state_time);
	      break;
	  case IPP_PRINTER_STOPPED :
	      _cupsLangPrintf(stdout,
	                      _("printer %s disabled since %s -\n"),
			      printer, printer_state_time);
	      break;
	}

        if ((message && *message) || pstate == IPP_PRINTER_STOPPED)
	{
	  if (!message || !*message)
	    _cupsLangPuts(stdout, _("\treason unknown\n"));
	  else
	    _cupsLangPrintf(stdout, "\t%s\n", message);
	}

        if (long_status > 1)
	  _cupsLangPuts(stdout,
	                _("\tForm mounted:\n"
			  "\tContent types: any\n"
			  "\tPrinter types: unknown\n"));

        if (long_status)
	{
	  _cupsLangPrintf(stdout, _("\tDescription: %s\n"),
	                  description ? description : "");

	  if (reasons)
	  {
	    _cupsLangPuts(stdout, _("\tAlerts:"));
	    for (i = 0; i < reasons->num_values; i ++)
	      _cupsLangPrintf(stdout, " %s",
	                      reasons->values[i].string.text);
	    _cupsLangPuts(stdout, "\n");
	  }
	}
        if (long_status > 1)
	{
	  _cupsLangPrintf(stdout, _("\tLocation: %s\n"),
	                  location ? location : "");

	  if (ptype & CUPS_PRINTER_REMOTE)
	  {
	    _cupsLangPuts(stdout, _("\tConnection: remote\n"));

	    if (make_model && !strstr(make_model, "System V Printer") &&
	             !strstr(make_model, "Raw Printer") && uri)
	      _cupsLangPrintf(stdout, _("\tInterface: %s.ppd\n"),
	                      uri);
	  }
	  else
	  {
	    _cupsLangPuts(stdout, _("\tConnection: direct\n"));

	    if (make_model && strstr(make_model, "System V Printer"))
	      _cupsLangPrintf(stdout,
	                      _("\tInterface: %s/interfaces/%s\n"),
			      root, printer);
	    else if (make_model && !strstr(make_model, "Raw Printer"))
	      _cupsLangPrintf(stdout,
	                      _("\tInterface: %s/ppd/%s.ppd\n"), root, printer);
          }
	  _cupsLangPuts(stdout, _("\tOn fault: no alert\n"));
	  _cupsLangPuts(stdout, _("\tAfter fault: continue\n"));
	      // TODO update to use printer-error-policy
          if (allowed)
	  {
	    _cupsLangPuts(stdout, _("\tUsers allowed:\n"));
	    for (i = 0; i < allowed->num_values; i ++)
	      _cupsLangPrintf(stdout, "\t\t%s\n",
	                      allowed->values[i].string.text);
	  }
	  else if (denied)
	  {
	    _cupsLangPuts(stdout, _("\tUsers denied:\n"));
	    for (i = 0; i < denied->num_values; i ++)
	      _cupsLangPrintf(stdout, "\t\t%s\n",
	                      denied->values[i].string.text);
	  }
	  else
	  {
	    _cupsLangPuts(stdout, _("\tUsers allowed:\n"));
	    _cupsLangPuts(stdout, _("\t\t(all)\n"));
	  }
	  _cupsLangPuts(stdout, _("\tForms allowed:\n"));
	  _cupsLangPuts(stdout, _("\t\t(none)\n"));
	  _cupsLangPuts(stdout, _("\tBanner required\n"));
	  _cupsLangPuts(stdout, _("\tCharset sets:\n"));
	  _cupsLangPuts(stdout, _("\t\t(none)\n"));
	  _cupsLangPuts(stdout, _("\tDefault pitch:\n"));
	  _cupsLangPuts(stdout, _("\tDefault page size:\n"));
	  _cupsLangPuts(stdout, _("\tDefault port settings:\n"));
	}

        for (i = 0; i < num_dests; i ++)
	  if (!strcasecmp(printer, dests[i].name) && dests[i].instance)
	  {
            switch (pstate)
	    {
	      case IPP_PRINTER_IDLE :
		  _cupsLangPrintf(stdout,
		                  _("printer %s/%s is idle.  "
				    "enabled since %s\n"),
				  printer, dests[i].instance,
				  printer_state_time);
		  break;
	      case IPP_PRINTER_PROCESSING :
		  _cupsLangPrintf(stdout,
		                  _("printer %s/%s now printing %s-%d.  "
				    "enabled since %s\n"),
				  printer, dests[i].instance, printer, jobid,
				  printer_state_time);
		  break;
	      case IPP_PRINTER_STOPPED :
		  _cupsLangPrintf(stdout,
		                  _("printer %s/%s disabled since %s -\n"),
				  printer, dests[i].instance,
				  printer_state_time);
		  break;
	    }

            if ((message && *message) || pstate == IPP_PRINTER_STOPPED)
	    {
	      if (!message || !*message)
		_cupsLangPuts(stdout, _("\treason unknown\n"));
	      else
		_cupsLangPrintf(stdout, "\t%s\n", message);
            }

            if (long_status > 1)
	      _cupsLangPuts(stdout,
	                    _("\tForm mounted:\n"
			      "\tContent types: any\n"
			      "\tPrinter types: unknown\n"));

            if (long_status)
	    {
	      _cupsLangPrintf(stdout, _("\tDescription: %s\n"),
	                      description ? description : "");

	      if (reasons)
	      {
		_cupsLangPuts(stdout, _("\tAlerts:"));
		for (i = 0; i < reasons->num_values; i ++)
		  _cupsLangPrintf(stdout, " %s",
	                	  reasons->values[i].string.text);
		_cupsLangPuts(stdout, "\n");
	      }
	    }
            if (long_status > 1)
	    {
	      _cupsLangPrintf(stdout, _("\tLocation: %s\n"),
	                      location ? location : "");

	      if (ptype & CUPS_PRINTER_REMOTE)
	      {
		_cupsLangPuts(stdout, _("\tConnection: remote\n"));

		if (make_model && !strstr(make_model, "System V Printer") &&
	        	 !strstr(make_model, "Raw Printer") && uri)
		  _cupsLangPrintf(stdout, _("\tInterface: %s.ppd\n"),
	                	  uri);
	      }
	      else
	      {
		_cupsLangPuts(stdout, _("\tConnection: direct\n"));

		if (make_model && strstr(make_model, "System V Printer"))
		  _cupsLangPrintf(stdout,
	                	  _("\tInterface: %s/interfaces/%s\n"),
				  root, printer);
		else if (make_model && !strstr(make_model, "Raw Printer"))
		  _cupsLangPrintf(stdout,
	                	  _("\tInterface: %s/ppd/%s.ppd\n"), root, printer);
              }
	      _cupsLangPuts(stdout, _("\tOn fault: no alert\n"));
	      _cupsLangPuts(stdout, _("\tAfter fault: continue\n"));
		  // TODO update to use printer-error-policy
              if (allowed)
	      {
		_cupsLangPuts(stdout, _("\tUsers allowed:\n"));
		for (i = 0; i < allowed->num_values; i ++)
		  _cupsLangPrintf(stdout, "\t\t%s\n",
	                	  allowed->values[i].string.text);
	      }
	      else if (denied)
	      {
		_cupsLangPuts(stdout, _("\tUsers denied:\n"));
		for (i = 0; i < denied->num_values; i ++)
		  _cupsLangPrintf(stdout, "\t\t%s\n",
	                	  denied->values[i].string.text);
	      }
	      else
	      {
		_cupsLangPuts(stdout, _("\tUsers allowed:\n"));
		_cupsLangPuts(stdout, _("\t\t(all)\n"));
	      }
	      _cupsLangPuts(stdout, _("\tForms allowed:\n"));
	      _cupsLangPuts(stdout, _("\t\t(none)\n"));
	      _cupsLangPuts(stdout, _("\tBanner required\n"));
	      _cupsLangPuts(stdout, _("\tCharset sets:\n"));
	      _cupsLangPuts(stdout, _("\t\t(none)\n"));
	      _cupsLangPuts(stdout, _("\tDefault pitch:\n"));
	      _cupsLangPuts(stdout, _("\tDefault page size:\n"));
	      _cupsLangPuts(stdout, _("\tDefault port settings:\n"));
	    }
	  }
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    _cupsLangPrintf(stderr, "lpstat: %s\n", cupsLastErrorString());
    return (1);
  }

  return (0);
}


/*
 * 'show_scheduler()' - Show scheduler status.
 */

static void
show_scheduler(http_t *http)	/* I - HTTP connection to server */
{
  if (http)
    _cupsLangPuts(stdout, _("scheduler is running\n"));
  else
    _cupsLangPuts(stdout, _("scheduler is not running\n"));
}


/*
 * End of "$Id$".
 */
