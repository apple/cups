/*
 * "$Id: lpstat.c,v 1.37.2.17 2003/06/14 14:08:46 mike Exp $"
 *
 *   "lpstat" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()           - Parse options and show status information.
 *   check_dest()     - Verify that the named destination(s) exists.
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
#include <ctype.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>
#include <cups/string.h>


/*
 * Local functions...
 */

static void	check_dest(const char *, int *, cups_dest_t **);
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
            fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	            argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'P' : /* Show paper types */
	    break;
	    
        case 'R' : /* Show ranking */
	    ranking = 1;
	    break;
	    
        case 'S' : /* Show charsets */
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
	        fputs("lpstat: Need \"completed\" or \"not-completed\" after -W!\n",
		      stderr);
		return (1);
              }

	      which = argv[i];
	    }

            if (strcmp(which, "completed") && strcmp(which, "not-completed"))
	    {
	      fputs("lpstat: Need \"completed\" or \"not-completed\" after -W!\n",
		    stderr);
	      return (1);
	    }
	    break;

        case 'a' : /* Show acceptance status */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	    {
              check_dest(argv[i] + 2, &num_dests, &dests);

	      status |= show_accepting(http, argv[i] + 2, num_dests, dests);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(argv[i], &num_dests, &dests);

	      status |= show_accepting(http, argv[i], num_dests, dests);
	    }
	    else
	    {
              if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      status |= show_accepting(http, NULL, num_dests, dests);
	    }
	    break;

#ifdef __sgi
        case 'b' : /* Show both the local and remote status */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

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

              check_dest(argv[i] + 2, &num_dests, &dests);

	      puts("");
	      status |= show_jobs(http, argv[i] + 2, NULL, 3, ranking, which);
	    }
	    else
	    {
	      fputs("lpstat: The -b option requires a destination argument.\n",
	            stderr);

	      return (1);
	    }
	    break;
#endif /* __sgi */

        case 'c' : /* Show classes and members */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	    {
              check_dest(argv[i] + 2, &num_dests, &dests);

	      status |= show_classes(http, argv[i] + 2);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(argv[i], &num_dests, &dests);

	      status |= show_classes(http, argv[i]);
	    }
	    else
	      status |= show_classes(http, NULL);
	    break;

        case 'd' : /* Show default destination */
            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            show_default(num_dests, dests);
	    break;

        case 'f' : /* Show forms */
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
	        fputs("Error: need hostname after \'-h\' option!\n", stderr);
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

        case 'l' : /* Long status or long job status */
#ifdef __sgi
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	    {
              check_dest(argv[i] + 2, &num_dests, &dests);

	      status |= show_jobs(http, argv[i] + 2, NULL, 3, ranking, which);
	    }
	    else
#endif /* __sgi */
	      long_status = 2;
	    break;

        case 'o' : /* Show jobs by destination */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	    {
              check_dest(argv[i] + 2, &num_dests, &dests);

	      status |= show_jobs(http, argv[i] + 2, NULL, long_status,
	                          ranking, which);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(argv[i], &num_dests, &dests);

	      status |= show_jobs(http, argv[i], NULL, long_status,
	                          ranking, which);
	    }
	    else
	      status |= show_jobs(http, NULL, NULL, long_status,
	                          ranking, which);
	    break;

        case 'p' : /* Show printers */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	    {
              check_dest(argv[i] + 2, &num_dests, &dests);

	      status |= show_printers(http, argv[i] + 2, num_dests, dests, long_status);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(argv[i], &num_dests, &dests);

	      status |= show_printers(http, argv[i], num_dests, dests, long_status);
	    }
	    else
	    {
              if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      status |= show_printers(http, NULL, num_dests, dests, long_status);
	    }
	    break;

        case 'r' : /* Show scheduler status */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    show_scheduler(http);
	    break;

        case 's' : /* Show summary */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    show_default(num_dests, dests);
	    status |= show_classes(http, NULL);
	    status |= show_devices(http, NULL, num_dests, dests);
	    break;

        case 't' : /* Show all info */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

            if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

	    show_scheduler(http);
	    show_default(num_dests, dests);
	    status |= show_classes(http, NULL);
	    status |= show_devices(http, NULL, num_dests, dests);
	    status |= show_accepting(http, NULL, num_dests, dests);
	    status |= show_printers(http, NULL, num_dests, dests, long_status);
	    status |= show_jobs(http, NULL, NULL, long_status, ranking, which);
	    break;

        case 'u' : /* Show jobs by user */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

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
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		perror("lpstat: Unable to connect to server");
		return (1);
	      }
            }

	    if (argv[i][2] != '\0')
	    {
              check_dest(argv[i] + 2, &num_dests, &dests);

	      status |= show_devices(http, argv[i] + 2, num_dests, dests);
	    }
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;

              check_dest(argv[i], &num_dests, &dests);

	      status |= show_devices(http, argv[i], num_dests, dests);
	    }
	    else
	    {
              if (num_dests == 0)
		num_dests = cupsGetDests(&dests);

	      status |= show_devices(http, NULL, num_dests, dests);
	    }
	    break;


	default :
	    fprintf(stderr, "lpstat: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
      if (!http)
      {
	http = httpConnectEncrypt(cupsServer(), ippPort(),
	                          cupsEncryption());

	if (http == NULL)
	{
	  perror("lpstat: Unable to connect to server");
	  return (1);
	}
      }

      status |= show_jobs(http, argv[i], NULL, long_status, ranking, which);
    }

  if (argc == 1)
  {
    if (!http)
    {
      http = httpConnectEncrypt(cupsServer(), ippPort(),
                                cupsEncryption());

      if (http == NULL)
      {
	perror("lpstat: Unable to connect to server");
	return (1);
      }
    }

    status |= show_jobs(http, NULL, cupsUser(), long_status, ranking, which);
  }

  return (status);
}


/*
 * 'check_dest()' - Verify that the named destination(s) exists.
 */

static void
check_dest(const char  *name,		/* I  - Name of printer/class(es) */
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
    *num_dests = cupsGetDests(dests);

 /*
  * Scan the name string for printer/class name(s)...
  */

  for (dptr = name; *dptr != '\0';) 
  {
   /*
    * Skip leading whitespace and commas...
    */

    while (isspace(*dptr) || *dptr == ',')
      dptr ++;

    if (*dptr == '\0')
      break;

   /*
    * Extract a single destination name from the name string...
    */

    for (pptr = printer; !isspace(*dptr) && *dptr != ',' && *dptr != '\0';)
    {
      if ((pptr - printer) < (sizeof(printer) - 1))
        *pptr++ = *dptr++;
      else
      {
        fprintf(stderr, "lpstat: Invalid destination name in list \"%s\"!\n", name);
        exit(1);
      }
    }

    *pptr = '\0';

   /*
    * Check the destination...
    */

    if (cupsGetDest(printer, NULL, *num_dests, *dests) == NULL)
    {
      fprintf(stderr, "lpstat: Unknown destination \"%s\"!\n", printer);
      exit(1);
    }
  }
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
  cups_lang_t	*language;		/* Default language */
  const char	*printer,		/* Printer name */
		*message;		/* Printer device URI */
  int		accepting;		/* Accepting requests? */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  static const char *pattrs[] =		/* Attributes we need for printers... */
		{
		  "printer-name",
		  "printer-state-message",
		  "printer-is-accepting-jobs"
		};


  DEBUG_printf(("show_accepting(%p, %p)\n", http, printers));

  if (http == NULL)
    return (1);

  if (printers != NULL && strcmp(printers, "all") == 0)
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_accepting: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-printers failed: %s\n",
              ippErrorString(response->request.status.status_code));
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

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state-message") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-is-accepting-jobs") == 0 &&
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

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr) == tolower(*dptr);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
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
        if (accepting)
	  printf("%s accepting requests since Jan 01 00:00\n", printer);
	else
	  printf("%s not accepting requests since Jan 01 00:00 -\n\t%s\n", printer,
	         message == NULL ? "reason unknown" : message);

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(dests[i].name, printer) == 0 && dests[i].instance)
	  {
            if (accepting)
	      printf("%s/%s accepting requests since Jan 01 00:00\n", printer, dests[i].instance);
	    else
	      printf("%s/%s not accepting requests since Jan 01 00:00 -\n\t%s\n", printer,
	             dests[i].instance,
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
    fprintf(stderr, "lpstat: get-printers failed: %s\n",
            ippErrorString(cupsLastError()));
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
  cups_lang_t	*language;		/* Default language */
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

  if (dests != NULL && strcmp(dests, "all") == 0)
    dests = NULL;

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_CLASSES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(cattrs) / sizeof(cattrs[0]),
		NULL, cattrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_classes: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-classes failed: %s\n",
              ippErrorString(response->request.status.status_code));
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
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-uri-supported") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  printer_uri = attr->values[0].string.text;

        if (strcmp(attr->name, "member-names") == 0 &&
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
        httpSeparate(printer_uri, method, username, server, &port, resource);

        if ((http2 = httpConnectEncrypt(server, port, cupsEncryption())) != NULL)
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

	  request = ippNew();

	  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
	  request->request.op.request_id   = 1;

	  language = cupsLangDefault();

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	       "attributes-charset", NULL, cupsLangEncoding(language));

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	       "attributes-natural-language", NULL, language->language);

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	       "printer-uri", NULL, printer_uri);

	  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                	"requested-attributes", sizeof(cattrs) / sizeof(cattrs[0]),
			NULL, cattrs);

          if ((response2 = cupsDoRequest(http2, request, "/")) != NULL)
	    members = ippFindAttribute(response2, "member-names", IPP_TAG_NAME);

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

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr) == tolower(*dptr);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
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
        printf("members of class %s:\n", printer);

	if (members)
	{
	  for (i = 0; i < members->num_values; i ++)
	    printf("\t%s\n", members->values[i].string.text);
        }
	else
	  puts("\tunknown");
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
    fprintf(stderr, "lpstat: get-classes failed: %s\n",
            ippErrorString(cupsLastError()));
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

  if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
  {
    if (dest->instance)
      printf("system default destination: %s/%s\n", dest->name, dest->instance);
    else
      printf("system default destination: %s\n", dest->name);
  }
  else
    puts("no system default destination");
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
  cups_lang_t	*language;		/* Default language */
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

  if (printers != NULL && strcmp(printers, "all") == 0)
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_devices: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-printers failed: %s\n",
              ippErrorString(response->request.status.status_code));
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
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-uri-supported") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;

        if (strcmp(attr->name, "device-uri") == 0 &&
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

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr) == tolower(*dptr);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
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
          printf("Output for printer %s is sent to remote printer %s on %s\n",
	         printer, strrchr(resource, '/') + 1, hostname);
        }
        else if (strncmp(device, "file:", 5) == 0)
          printf("Output for printer %s is sent to %s\n", printer, device + 5);
        else
          printf("Output for printer %s is sent to %s\n", printer, device);

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(printer, dests[i].name) == 0 && dests[i].instance)
	  {
            if (device == NULL)
              printf("Output for printer %s/%s is sent to remote printer %s on %s\n",
	             printer, dests[i].instance, strrchr(resource, '/') + 1,
		     hostname);
            else if (strncmp(device, "file:", 5) == 0)
              printf("Output for printer %s/%s is sent to %s\n", printer, dests[i].instance, device + 5);
            else
              printf("Output for printer %s/%s is sent to %s\n", printer, dests[i].instance, device);
	  }
#else
        if (device == NULL)
          printf("device for %s: %s\n", printer, uri);
        else if (strncmp(device, "file:", 5) == 0)
          printf("device for %s: %s\n", printer, device + 5);
        else
          printf("device for %s: %s\n", printer, device);

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(printer, dests[i].name) == 0 && dests[i].instance)
	  {
            if (device == NULL)
              printf("device for %s/%s: %s\n", printer, dests[i].instance, uri);
            else if (strncmp(device, "file:", 5) == 0)
              printf("device for %s/%s: %s\n", printer, dests[i].instance, device + 5);
            else
              printf("device for %s/%s: %s\n", printer, dests[i].instance, device);
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
    fprintf(stderr, "lpstat: get-printers failed: %s\n",
            ippErrorString(cupsLastError()));
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
  cups_lang_t	*language;		/* Default language */
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
		date[32];		/* Date buffer */
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

  if (dests != NULL && strcmp(dests, "all") == 0)
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

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

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
      fprintf(stderr, "lpstat: get-jobs failed: %s\n",
              ippErrorString(response->request.status.status_code));
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
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer * 1024;

        if (strcmp(attr->name, "time-at-creation") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobtime = attr->values[0].integer;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  username = attr->values[0].string.text;

        if (strcmp(attr->name, "job-name") == 0 &&
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

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = dest;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr) == tolower(*dptr);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
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

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = username;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
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

	  strftime(date, sizeof(date), "%b %d %H:%M", jobdate);

	  printf("%s;%s;%d;%s;%s\n", temp, username ? username : "unknown",
	         size, title ? title : "unknown", date);
	}
	else
	{
	  strftime(date, sizeof(date), CUPS_STRFTIME_FORMAT, jobdate);

          if (ranking)
	    printf("%3d %-21s %-13s %8d %s\n", rank, temp,
	           username ? username : "unknown", size, date);
          else
	    printf("%-23s %-13s %8d   %s\n", temp,
	           username ? username : "unknown", size, date);
          if (long_status)
	    printf("\tqueued for %s\n", dest);
	}
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    fprintf(stderr, "lpstat: get-jobs failed: %s\n",
            ippErrorString(cupsLastError()));
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
  int		i, j;			/* Looping vars */
  ipp_t		*request,		/* IPP Request */
		*response,		/* IPP Response */
		*jobs;			/* IPP Get Jobs response */
  ipp_attribute_t *attr,		/* Current attribute */
		*jobattr,		/* Job ID attribute */
		*reasons;		/* Job state reasons attribute */
  cups_lang_t	*language;		/* Default language */
  const char	*printer,		/* Printer name */
		*message,		/* Printer state message */
		*description,		/* Description of printer */
		*location,		/* Location of printer */
		*make_model,		/* Make and model of printer */
		*uri;			/* URI of printer */
  ipp_pstate_t	pstate;			/* Printer state */
  cups_ptype_t	ptype;			/* Printer type */
  int		jobid;			/* Job ID of current job */
  const char	*dptr,			/* Pointer into destination list */
		*ptr;			/* Pointer into printer name */
  int		match;			/* Non-zero if this job matches */
  char		printer_uri[HTTP_MAX_URI];
					/* Printer URI */
  const char	*root;			/* Server root directory... */
  static const char *pattrs[] =		/* Attributes we need for printers... */
		{
		  "printer-name",
		  "printer-state",
		  "printer-state-message",
		  "printer-state-reasons",
		  "printer-type",
		  "printer-info",
                  "printer-location",
		  "printer-make-and-model",
		  "printer-uri-supported"
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

  if (printers != NULL && strcmp(printers, "all") == 0)
    printers = NULL;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		NULL, pattrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    DEBUG_puts("show_printers: request succeeded...");

    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "lpstat: get-printers failed: %s\n",
              ippErrorString(response->request.status.status_code));
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
      ptype       = CUPS_PRINTER_LOCAL;
      pstate      = IPP_PRINTER_IDLE;
      message     = NULL;
      description = NULL;
      location    = NULL;
      make_model  = NULL;
      reasons     = NULL;
      uri         = NULL;
      jobid       = 0;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;
        else if (strcmp(attr->name, "printer-state") == 0 &&
	         attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;
        else if (strcmp(attr->name, "printer-type") == 0 &&
	         attr->value_tag == IPP_TAG_ENUM)
	  ptype = (cups_ptype_t)attr->values[0].integer;
        else if (strcmp(attr->name, "printer-state-message") == 0 &&
	         attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;
        else if (strcmp(attr->name, "printer-info") == 0 &&
	         attr->value_tag == IPP_TAG_TEXT)
	  description = attr->values[0].string.text;
        else if (strcmp(attr->name, "printer-location") == 0 &&
	         attr->value_tag == IPP_TAG_TEXT)
	  location = attr->values[0].string.text;
        else if (strcmp(attr->name, "printer-make-and-model") == 0 &&
	         attr->value_tag == IPP_TAG_TEXT)
	  make_model = attr->values[0].string.text;
        else if (strcmp(attr->name, "printer-uri-supported") == 0 &&
	         attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;
        else if (strcmp(attr->name, "printer-state-reasons") == 0 &&
	         attr->value_tag == IPP_TAG_KEYWORD)
	  reasons = attr;

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

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && tolower(*ptr) == tolower(*dptr);
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != ',' && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
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

	  request = ippNew();

	  request->request.op.operation_id = IPP_GET_JOBS;
	  request->request.op.request_id   = 1;

	  language = cupsLangDefault();

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
            	       "attributes-charset", NULL,
		       cupsLangEncoding(language));

	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                       "attributes-natural-language", NULL,
		       language->language);

	  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                	"requested-attributes",
		        sizeof(jattrs) / sizeof(jattrs[0]), NULL, jattrs);

          snprintf(printer_uri, sizeof(printer_uri), "ipp://%s/printers/%s",
	           http->hostname, printer);
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	               "printer-uri", NULL, printer_uri);

	  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
	        	"limit", 1);

          if ((jobs = cupsDoRequest(http, request, "/")) != NULL)
	  {
	    if ((jobattr = ippFindAttribute(jobs, "job-id", IPP_TAG_INTEGER)) != NULL)
              jobid = jobattr->values[0].integer;

            ippDelete(jobs);
	  }
        }

       /*
        * Display it...
	*/

        switch (pstate)
	{
	  case IPP_PRINTER_IDLE :
	      printf("printer %s is idle.  enabled since Jan 01 00:00\n", printer);
	      break;
	  case IPP_PRINTER_PROCESSING :
	      printf("printer %s now printing %s-%d.  enabled since Jan 01 00:00\n", printer, printer, jobid);
	      break;
	  case IPP_PRINTER_STOPPED :
	      printf("printer %s disabled since Jan 01 00:00 -\n", printer);
	      break;
	}

        if ((message && *message) || pstate == IPP_PRINTER_STOPPED)
	  printf("\t%s\n", message == NULL || !*message ? "reason unknown" :
	                                                  message);

        if (long_status > 1)
	{
	  puts("\tForm mounted:");
	  puts("\tContent types: any");
	  puts("\tPrinter types: unknown");
	}
        if (long_status)
	{
	  printf("\tDescription: %s\n", description ? description : "");
	  if (reasons)
	  {
	    printf("\tAlerts:");
	    for (i = 0; i < reasons->num_values; i ++)
	      printf(" %s", reasons->values[i].string.text);
	    putchar('\n');
	  }
	}
        if (long_status > 1)
	{
	  printf("\tLocation: %s\n", location ? location : "");
	  printf("\tConnection: %s\n",
	         (ptype & CUPS_PRINTER_REMOTE) ? "remote" : "direct");
	  if (!(ptype & CUPS_PRINTER_REMOTE))
	  {
	    if (make_model && strstr(make_model, "System V Printer"))
	      printf("\tInterface: %s/ppd/%s.ppd\n", root, printer);
	    else if (make_model && !strstr(make_model, "Raw Printer"))
	      printf("\tInterface: %s/ppd/%s.ppd\n", root, printer);
          }
	  else if (make_model && !strstr(make_model, "System V Printer") &&
	           !strstr(make_model, "Raw Printer") && uri)
	    printf("\tInterface: %s.ppd\n", uri);

	  puts("\tOn fault: no alert");
	  puts("\tAfter fault: continue");
	  puts("\tUsers allowed:");
	  puts("\t\t(all)");
	  puts("\tForms allowed:");
	  puts("\t\t(none)");
	  puts("\tBanner required");
	  puts("\tCharset sets:");
	  puts("\t\t(none)");
	  puts("\tDefault pitch:");
	  puts("\tDefault page size:");
	  puts("\tDefault port settings:");
	}

        for (i = 0; i < num_dests; i ++)
	  if (strcasecmp(printer, dests[i].name) == 0 && dests[i].instance)
	  {
            switch (pstate)
	    {
	      case IPP_PRINTER_IDLE :
		  printf("printer %s/%s is idle.  enabled since Jan 01 00:00\n", printer, dests[i].instance);
		  break;
	      case IPP_PRINTER_PROCESSING :
		  printf("printer %s/%s now printing %s-%d.  enabled since Jan 01 00:00\n", printer,
		         dests[i].instance, printer, jobid);
		  break;
	      case IPP_PRINTER_STOPPED :
		  printf("printer %s/%s disabled since Jan 01 00:00 -\n", printer,
		         dests[i].instance);
		  break;
	    }

            if ((message && *message) || pstate == IPP_PRINTER_STOPPED)
	      printf("\t%s\n", message == NULL || !*message ? "reason unknown" :
	                                                      message);

            if (long_status > 1)
	    {
	      puts("\tForm mounted:");
	      puts("\tContent types: any");
	      puts("\tPrinter types: unknown");
	    }
            if (long_status)
	    {
	      printf("\tDescription: %s\n", description ? description : "");
	      if (reasons)
	      {
	        printf("\tAlerts:");
		for (j = 0; j < reasons->num_values; j ++)
		  printf(" %s", reasons->values[j].string.text);
		putchar('\n');
	      }
	    }
            if (long_status > 1)
	    {
	      printf("\tLocation: %s\n", location ? location : "");
	      printf("\tConnection: %s\n",
	             (ptype & CUPS_PRINTER_REMOTE) ? "remote" : "direct");
	      if (!(ptype & CUPS_PRINTER_REMOTE))
	      {
		if (make_model && strstr(make_model, "System V Printer"))
		  printf("\tInterface: %s/ppd/%s.ppd\n", root, printer);
		else if (make_model && !strstr(make_model, "Raw Printer"))
		  printf("\tInterface: %s/ppd/%s.ppd\n", root, printer);
              }
	      else if (make_model && !strstr(make_model, "System V Printer") &&
	               !strstr(make_model, "Raw Printer") && uri)
		printf("\tInterface: %s.ppd\n", uri);
	      puts("\tOn fault: no alert");
	      puts("\tAfter fault: continue");
	      puts("\tUsers allowed:");
	      puts("\t\t(all)");
	      puts("\tForms allowed:");
	      puts("\t\t(none)");
	      puts("\tBanner required");
	      puts("\tCharset sets:");
	      puts("\t\t(none)");
	      puts("\tDefault pitch:");
	      puts("\tDefault page size:");
	      puts("\tDefault port settings:");
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
    fprintf(stderr, "lpstat: get-printers failed: %s\n",
            ippErrorString(cupsLastError()));
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
  printf("scheduler is %srunning\n", http == NULL ? "not " : "");
}


/*
 * End of "$Id: lpstat.c,v 1.37.2.17 2003/06/14 14:08:46 mike Exp $".
 */
