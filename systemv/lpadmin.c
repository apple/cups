/*
 * "lpadmin" command for CUPS.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#define _CUPS_NO_DEPRECATED
#define _PPD_DEPRECATED
#include <cups/cups-private.h>
#include <cups/ppd-private.h>


/*
 * Local functions...
 */

static int		add_printer_to_class(http_t *http, char *printer, char *pclass);
static int		default_printer(http_t *http, char *printer);
static int		delete_printer(http_t *http, char *printer);
static int		delete_printer_from_class(http_t *http, char *printer,
			                          char *pclass);
static int		delete_printer_option(http_t *http, char *printer,
			                      char *option);
static int		enable_printer(http_t *http, char *printer);
static char		*get_printer_ppd(const char *uri, char *buffer, size_t bufsize, int *num_options, cups_option_t **options);
static cups_ptype_t	get_printer_type(http_t *http, char *printer, char *uri,
			                 size_t urisize);
static int		set_printer_options(http_t *http, char *printer,
			                    int num_options, cups_option_t *options,
					    char *file, int enable);
static int		validate_name(const char *name);


/*
 * 'main()' - Parse options and configure the scheduler.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  http_t	*http;			/* Connection to server */
  char		*printer,		/* Destination printer */
		*pclass,		/* Printer class name */
		*opt,			/* Option pointer */
		*val;			/* Pointer to allow/deny value */
  int		enable = 0;		/* Enable/resume printer? */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  char		*file,			/* New PPD file */
		evefile[1024] = "";	/* IPP Everywhere PPD */
  const char	*ppd_name,		/* ppd-name value */
		*device_uri;		/* device-uri value */


  _cupsSetLocale(argv);

  http        = NULL;
  printer     = NULL;
  num_options = 0;
  options     = NULL;
  file        = NULL;

  for (i = 1; i < argc; i ++)
  {
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
      {
	switch (*opt)
	{
	  case 'c' : /* Add printer to class */
	      if (!http)
	      {
		http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC, cupsEncryption(), 1, 30000, NULL);

		if (http == NULL)
		{
		  _cupsLangPrintf(stderr, _("lpadmin: Unable to connect to server: %s"), strerror(errno));
		  return (1);
		}
	      }

	      if (printer == NULL)
	      {
		_cupsLangPuts(stderr,
			      _("lpadmin: Unable to add a printer to the class:\n"
				"         You must specify a printer name first."));
		return (1);
	      }

	      if (opt[1] != '\0')
	      {
		pclass = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected class name after \"-c\" option."));
		  return (1);
		}

		pclass = argv[i];
	      }

	      if (!validate_name(pclass))
	      {
		_cupsLangPuts(stderr,
			      _("lpadmin: Class name can only contain printable "
				"characters."));
		return (1);
	      }

	      if (add_printer_to_class(http, printer, pclass))
		return (1);
	      break;

	  case 'd' : /* Set as default destination */
	      if (!http)
	      {
		http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC, cupsEncryption(), 1, 30000, NULL);

		if (http == NULL)
		{
		  _cupsLangPrintf(stderr, _("lpadmin: Unable to connect to server: %s"), strerror(errno));
		  return (1);
		}
	      }

	      if (opt[1] != '\0')
	      {
		printer = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected printer name after \"-d\" option."));
		  return (1);
		}

		printer = argv[i];
	      }

	      if (!validate_name(printer))
	      {
		_cupsLangPuts(stderr, _("lpadmin: Printer name can only contain printable characters."));
		return (1);
	      }

	      if (default_printer(http, printer))
		return (1);

	      i = argc;
	      break;

	  case 'h' : /* Connect to host */
	      if (http)
	      {
		httpClose(http);
		http = NULL;
	      }

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
		  _cupsLangPuts(stderr, _("lpadmin: Expected hostname after \"-h\" option."));
		  return (1);
		}

		cupsSetServer(argv[i]);
	      }
	      break;

	  case 'P' : /* Use the specified PPD file */
	  case 'i' : /* Use the specified PPD file */
	      if (opt[1] != '\0')
	      {
		file = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPrintf(stderr, _("lpadmin: Expected PPD after \"-%c\" option."), argv[i - 1][1]);
		  return (1);
		}

		file = argv[i];
	      }

	      if (*opt == 'i')
	      {
	       /*
	        * Check to see that the specified file is, in fact, a PPD...
	        */

                cups_file_t *fp = cupsFileOpen(file, "r");
                char line[256];

                if (!cupsFileGets(fp, line, sizeof(line)) || strncmp(line, "*PPD-Adobe", 10))
                {
                  _cupsLangPuts(stderr, _("lpadmin: System V interface scripts are no longer supported for security reasons."));
                  cupsFileClose(fp);
                  return (1);
                }

                cupsFileClose(fp);
	      }
	      break;

	  case 'E' : /* Enable the printer/enable encryption */
	      if (printer == NULL)
	      {
#ifdef HAVE_SSL
		cupsSetEncryption(HTTP_ENCRYPTION_REQUIRED);

		if (http)
		  httpEncryption(http, HTTP_ENCRYPTION_REQUIRED);
#else
		_cupsLangPrintf(stderr, _("%s: Sorry, no encryption support."), argv[0]);
#endif /* HAVE_SSL */
		break;
	      }

	      if (!http)
	      {
		http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC, cupsEncryption(), 1, 30000, NULL);

		if (http == NULL)
		{
		  _cupsLangPrintf(stderr,
				  _("lpadmin: Unable to connect to server: %s"),
				  strerror(errno));
		  return (1);
		}
	      }

              enable = 1;
	      break;

	  case 'm' : /* Use the specified standard script/PPD file */
	      if (opt[1] != '\0')
	      {
		num_options = cupsAddOption("ppd-name", opt + 1, num_options, &options);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected model after \"-m\" option."));
		  return (1);
		}

		num_options = cupsAddOption("ppd-name", argv[i], num_options, &options);
	      }
	      break;

	  case 'o' : /* Set option */
	      if (opt[1] != '\0')
	      {
		num_options = cupsParseOptions(opt + 1, num_options, &options);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected name=value after \"-o\" option."));
		  return (1);
		}

		num_options = cupsParseOptions(argv[i], num_options, &options);
	      }
	      break;

	  case 'p' : /* Add/modify a printer */
	      if (opt[1] != '\0')
	      {
		printer = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected printer after \"-p\" option."));
		  return (1);
		}

		printer = argv[i];
	      }

	      if (!validate_name(printer))
	      {
		_cupsLangPuts(stderr, _("lpadmin: Printer name can only contain printable characters."));
		return (1);
	      }
	      break;

	  case 'r' : /* Remove printer from class */
	      if (!http)
	      {
		http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC, cupsEncryption(), 1, 30000, NULL);

		if (http == NULL)
		{
		  _cupsLangPrintf(stderr,
				  _("lpadmin: Unable to connect to server: %s"),
				  strerror(errno));
		  return (1);
		}
	      }

	      if (printer == NULL)
	      {
		_cupsLangPuts(stderr,
			      _("lpadmin: Unable to remove a printer from the class:\n"
				"         You must specify a printer name first."));
		return (1);
	      }

	      if (opt[1] != '\0')
	      {
		pclass = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected class after \"-r\" option."));
		  return (1);
		}

		pclass = argv[i];
	      }

	      if (!validate_name(pclass))
	      {
		_cupsLangPuts(stderr, _("lpadmin: Class name can only contain printable characters."));
		return (1);
	      }

	      if (delete_printer_from_class(http, printer, pclass))
		return (1);
	      break;

	  case 'R' : /* Remove option */
	      if (!http)
	      {
		http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC, cupsEncryption(), 1, 30000, NULL);

		if (http == NULL)
		{
		  _cupsLangPrintf(stderr, _("lpadmin: Unable to connect to server: %s"), strerror(errno));
		  return (1);
		}
	      }

	      if (printer == NULL)
	      {
		_cupsLangPuts(stderr,
			      _("lpadmin: Unable to delete option:\n"
				"         You must specify a printer name first."));
		return (1);
	      }

	      if (opt[1] != '\0')
	      {
		val = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected name after \"-R\" option."));
		  return (1);
		}

		val = argv[i];
	      }

	      if (delete_printer_option(http, printer, val))
		return (1);
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
		  return (1);
		}

		cupsSetUser(argv[i]);
	      }
	      break;

	  case 'u' : /* Allow/deny users */
	      if (opt[1] != '\0')
	      {
		val = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected allow/deny:userlist after \"-u\" option."));
		  return (1);
		}

		val = argv[i];
	      }

	      if (!_cups_strncasecmp(val, "allow:", 6))
		num_options = cupsAddOption("requesting-user-name-allowed", val + 6, num_options, &options);
	      else if (!_cups_strncasecmp(val, "deny:", 5))
		num_options = cupsAddOption("requesting-user-name-denied", val + 5, num_options, &options);
	      else
	      {
		_cupsLangPrintf(stderr, _("lpadmin: Unknown allow/deny option \"%s\"."), val);
		return (1);
	      }
	      break;

	  case 'v' : /* Set the device-uri attribute */
	      if (opt[1] != '\0')
	      {
		num_options = cupsAddOption("device-uri", opt + 1, num_options, &options);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected device URI after \"-v\" option."));
		  return (1);
		}

		num_options = cupsAddOption("device-uri", argv[i], num_options, &options);
	      }
	      break;

	  case 'x' : /* Delete a printer */
	      if (!http)
	      {
		http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC, cupsEncryption(), 1, 30000, NULL);

		if (http == NULL)
		{
		  _cupsLangPrintf(stderr,
				  _("lpadmin: Unable to connect to server: %s"),
				  strerror(errno));
		  return (1);
		}
	      }

	      if (opt[1] != '\0')
	      {
		printer = opt + 1;
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected printer or class after \"-x\" option."));
		  return (1);
		}

		printer = argv[i];
	      }

	      if (!validate_name(printer))
	      {
		_cupsLangPuts(stderr, _("lpadmin: Printer name can only contain printable characters."));
		return (1);
	      }

	      if (delete_printer(http, printer))
		return (1);

	      i = argc;
	      break;

	  case 'D' : /* Set the printer-info attribute */
	      if (opt[1] != '\0')
	      {
		num_options = cupsAddOption("printer-info", opt + 1, num_options, &options);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected description after \"-D\" option."));
		  return (1);
		}

		num_options = cupsAddOption("printer-info", argv[i], num_options, &options);
	      }
	      break;

	  case 'I' : /* Set the supported file types (ignored) */
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr, _("lpadmin: Expected file type(s) after \"-I\" option."));
		return (1);
	      }

	      _cupsLangPuts(stderr, _("lpadmin: Warning - content type list ignored."));
	      break;

	  case 'L' : /* Set the printer-location attribute */
	      if (opt[1] != '\0')
	      {
		num_options = cupsAddOption("printer-location", opt + 1, num_options, &options);
		opt += strlen(opt) - 1;
	      }
	      else
	      {
		i ++;

		if (i >= argc)
		{
		  _cupsLangPuts(stderr, _("lpadmin: Expected location after \"-L\" option."));
		  return (1);
		}

		num_options = cupsAddOption("printer-location", argv[i], num_options, &options);
	      }
	      break;

	  default :
	      _cupsLangPrintf(stderr, _("lpadmin: Unknown option \"%c\"."), *opt);
	      return (1);
	}
      }
    }
    else
    {
      _cupsLangPrintf(stderr, _("lpadmin: Unknown argument \"%s\"."), argv[i]);
      return (1);
    }
  }

 /*
  * Set options as needed...
  */

  ppd_name   = cupsGetOption("ppd-name", num_options, options);
  device_uri = cupsGetOption("device-uri", num_options, options);

  if (ppd_name && !strcmp(ppd_name, "raw"))
  {
    _cupsLangPuts(stderr, _("lpadmin: Raw queues are deprecated and will stop working in a future version of CUPS."));

    if (device_uri && (!strncmp(device_uri, "ipp://", 6) || !strncmp(device_uri, "ipps://", 7)) && strstr(device_uri, "/printers/"))
      _cupsLangPuts(stderr, _("lpadmin: Use the 'everywhere' model for shared printers."));
  }
  else if (ppd_name && !strcmp(ppd_name, "everywhere") && device_uri)
  {
    if ((file = get_printer_ppd(device_uri, evefile, sizeof(evefile), &num_options, &options)) == NULL)
      return (1);

    num_options = cupsRemoveOption("ppd-name", num_options, &options);
  }

  if (num_options || file)
  {
    if (printer == NULL)
    {
      _cupsLangPuts(stderr,
                    _("lpadmin: Unable to set the printer options:\n"
                      "         You must specify a printer name first."));
      return (1);
    }

    if (!http)
    {
      http = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC,
                          cupsEncryption(), 1, 30000, NULL);

      if (http == NULL) {
        _cupsLangPrintf(stderr, _("lpadmin: Unable to connect to server: %s"),
                        strerror(errno));
        return (1);
      }
    }

    if (set_printer_options(http, printer, num_options, options, file, enable))
      return (1);
  }
  else if (enable && enable_printer(http, printer))
    return (1);

  if (evefile[0])
    unlink(evefile);

  if (printer == NULL)
  {
    _cupsLangPuts(stdout,
	          _("Usage:\n"
		    "\n"
		    "    lpadmin [-h server] -d destination\n"
		    "    lpadmin [-h server] -x destination\n"
		    "    lpadmin [-h server] -p printer [-c add-class] "
		    "[-i interface] [-m model]\n"
		    "                       [-r remove-class] [-v device] "
		    "[-D description]\n"
		    "                       [-P ppd-file] [-o name=value]\n"
		    "                       [-u allow:user,user] "
		    "[-u deny:user,user]"));
  }

  if (http)
    httpClose(http);

  return (0);
}


/*
 * 'add_printer_to_class()' - Add a printer to a class.
 */

static int				/* O - 0 on success, 1 on fail */
add_printer_to_class(http_t *http,	/* I - Server connection */
                     char   *printer,	/* I - Printer to add */
		     char   *pclass)	/* I - Class to add to */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr,		/* Current attribute */
		*members;		/* Members in class */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("add_printer_to_class(%p, \"%s\", \"%s\")\n", http,
                printer, pclass));

 /*
  * Build an IPP_OP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/classes/%s", pclass);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  response = cupsDoRequest(http, request, "/");

 /*
  * Build a CUPS-Add-Modify-Class request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    member-uris
  */

  request = ippNewRequest(IPP_OP_CUPS_ADD_MODIFY_CLASS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * See if the printer is already in the class...
  */

  if (response != NULL &&
      (members = ippFindAttribute(response, "member-names",
                                  IPP_TAG_NAME)) != NULL)
    for (i = 0; i < members->num_values; i ++)
      if (_cups_strcasecmp(printer, members->values[i].string.text) == 0)
      {
        _cupsLangPrintf(stderr,
	                _("lpadmin: Printer %s is already a member of class "
			  "%s."), printer, pclass);
        ippDelete(request);
	ippDelete(response);
	return (0);
      }

 /*
  * OK, the printer isn't part of the class, so add it...
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);

  if (response != NULL &&
      (members = ippFindAttribute(response, "member-uris",
                                  IPP_TAG_URI)) != NULL)
  {
   /*
    * Add the printer to the existing list...
    */

    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI,
                         "member-uris", members->num_values + 1, NULL, NULL);
    for (i = 0; i < members->num_values; i ++)
      attr->values[i].string.text =
          _cupsStrAlloc(members->values[i].string.text);

    attr->values[i].string.text = _cupsStrAlloc(uri);
  }
  else
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "member-uris", NULL,
                 uri);

 /*
  * Then send the request...
  */

  ippDelete(response);

  ippDelete(cupsDoRequest(http, request, "/admin/"));
  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'default_printer()' - Set the default printing destination.
 */

static int				/* O - 0 on success, 1 on fail */
default_printer(http_t *http,		/* I - Server connection */
                char   *printer)	/* I - Printer name */
{
  ipp_t		*request;		/* IPP Request */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("default_printer(%p, \"%s\")\n", http, printer));

 /*
  * Build a CUPS-Set-Default request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);

  request = ippNewRequest(IPP_OP_CUPS_SET_DEFAULT);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'delete_printer()' - Delete a printer from the system...
 */

static int				/* O - 0 on success, 1 on fail */
delete_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to delete */
{
  ipp_t		*request;		/* IPP Request */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("delete_printer(%p, \"%s\")\n", http, printer));

 /*
  * Build a CUPS-Delete-Printer request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  */

  request = ippNewRequest(IPP_OP_CUPS_DELETE_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'delete_printer_from_class()' - Delete a printer from a class.
 */

static int				/* O - 0 on success, 1 on fail */
delete_printer_from_class(
    http_t *http,			/* I - Server connection */
    char   *printer,			/* I - Printer to remove */
    char   *pclass)	  		/* I - Class to remove from */
{
  int		i, j, k;		/* Looping vars */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr,		/* Current attribute */
		*members;		/* Members in class */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("delete_printer_from_class(%p, \"%s\", \"%s\")\n", http,
                printer, pclass));

 /*
  * Build an IPP_OP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/classes/%s", pclass);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/classes/")) == NULL ||
      response->request.status.status_code == IPP_STATUS_ERROR_NOT_FOUND)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }

 /*
  * See if the printer is already in the class...
  */

  if ((members = ippFindAttribute(response, "member-names", IPP_TAG_NAME)) == NULL)
  {
    _cupsLangPuts(stderr, _("lpadmin: No member names were seen."));

    ippDelete(response);

    return (1);
  }

  for (i = 0; i < members->num_values; i ++)
    if (!_cups_strcasecmp(printer, members->values[i].string.text))
      break;

  if (i >= members->num_values)
  {
    _cupsLangPrintf(stderr,
                    _("lpadmin: Printer %s is not a member of class %s."),
	            printer, pclass);

    ippDelete(response);

    return (1);
  }

  if (members->num_values == 1)
  {
   /*
    * Build a CUPS-Delete-Class request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    requesting-user-name
    */

    request = ippNewRequest(IPP_OP_CUPS_DELETE_CLASS);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	 "printer-uri", NULL, uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser());
  }
  else
  {
   /*
    * Build a IPP_OP_CUPS_ADD_MODIFY_CLASS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    requesting-user-name
    *    member-uris
    */

    request = ippNewRequest(IPP_OP_CUPS_ADD_MODIFY_CLASS);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	 "printer-uri", NULL, uri);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser());

   /*
    * Delete the printer from the class...
    */

    members = ippFindAttribute(response, "member-uris", IPP_TAG_URI);
    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI,
                         "member-uris", members->num_values - 1, NULL, NULL);

    for (j = 0, k = 0; j < members->num_values; j ++)
      if (j != i)
        attr->values[k ++].string.text =
	    _cupsStrAlloc(members->values[j].string.text);
  }

 /*
  * Then send the request...
  */

  ippDelete(response);

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'delete_printer_option()' - Delete a printer option.
 */

static int				/* O - 0 on success, 1 on fail */
delete_printer_option(http_t *http,	/* I - Server connection */
                      char   *printer,	/* I - Printer */
		      char   *option)	/* I - Option to delete */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


 /*
  * Build a IPP_OP_CUPS_ADD_MODIFY_PRINTER or IPP_OP_CUPS_ADD_MODIFY_CLASS request, which
  * requires the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    option with deleteAttr tag
  */

  if (get_printer_type(http, printer, uri, sizeof(uri)) & CUPS_PRINTER_CLASS)
    request = ippNewRequest(IPP_OP_CUPS_ADD_MODIFY_CLASS);
  else
    request = ippNewRequest(IPP_OP_CUPS_ADD_MODIFY_PRINTER);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());
  ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_DELETEATTR, option, 0);

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'enable_printer()' - Enable a printer...
 */

static int				/* O - 0 on success, 1 on fail */
enable_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to enable */
{
  ipp_t		*request;		/* IPP Request */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("enable_printer(%p, \"%s\")\n", http, printer));

 /*
  * Send IPP_OP_ENABLE_PRINTER and IPP_OP_RESUME_PRINTER requests, which
  * require the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  */

  request = ippNewRequest(IPP_OP_ENABLE_PRINTER);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }

  request = ippNewRequest(IPP_OP_RESUME_PRINTER);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }

  return (0);
}


/*
 * 'get_printer_ppd()' - Get an IPP Everywhere PPD file for the given URI.
 */

static char *				/* O  - Filename or NULL */
get_printer_ppd(
    const char    *uri,			/* I  - Printer URI */
    char          *buffer,		/* I  - Filename buffer */
    size_t        bufsize,		/* I  - Size of filename buffer */
    int           *num_options,		/* IO - Number of options */
    cups_option_t **options)		/* IO - Options */
{
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* Get-Printer-Attributes request */
		*response;		/* Get-Printer-Attributes response */
  ipp_attribute_t *attr;		/* Attribute from response */
  char		resolved[1024],		/* Resolved URI */
		scheme[32],		/* URI scheme */
		userpass[256],		/* Username:password */
		host[256],		/* Hostname */
		resource[256];		/* Resource path */
  int		port;			/* Port number */
  static const char * const pattrs[] =	/* Attributes to use */
  {
    "all",
    "media-col-database"
  };


 /*
  * Connect to the printer...
  */

  if (strstr(uri, "._tcp"))
  {
   /*
    * Resolve URI...
    */

    if (!_httpResolveURI(uri, resolved, sizeof(resolved), _HTTP_RESOLVE_DEFAULT, NULL, NULL))
    {
      _cupsLangPrintf(stderr, _("%s: Unable to resolve \"%s\"."), "lpadmin", uri);
      return (NULL);
    }

    uri = resolved;
  }

  if (httpSeparateURI(HTTP_URI_CODING_ALL, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
  {
    _cupsLangPrintf(stderr, _("%s: Bad printer URI \"%s\"."), "lpadmin", uri);
    return (NULL);
  }

  http = httpConnect2(host, port, NULL, AF_UNSPEC, !strcmp(scheme, "ipps") ? HTTP_ENCRYPTION_ALWAYS : HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
  if (!http)
  {
    _cupsLangPrintf(stderr, _("%s: Unable to connect to \"%s:%d\": %s"), "lpadmin", host, port, cupsLastErrorString());
    return (NULL);
  }

 /*
  * Send a Get-Printer-Attributes request...
  */

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]), NULL, pattrs);
  response = cupsDoRequest(http, request, resource);

  if (cupsLastError() >= IPP_STATUS_REDIRECTION_OTHER_SITE)
  {
    _cupsLangPrintf(stderr, _("%s: Unable to query printer: %s"), "lpadmin", cupsLastErrorString());
    buffer[0] = '\0';
  }
  else if (_ppdCreateFromIPP(buffer, bufsize, response))
  {
    if (!cupsGetOption("printer-geo-location", *num_options, *options) && (attr = ippFindAttribute(response, "printer-geo-location", IPP_TAG_URI)) != NULL)
      *num_options = cupsAddOption("printer-geo-location", ippGetString(attr, 0, NULL), *num_options, options);

    if (!cupsGetOption("printer-info", *num_options, *options) && (attr = ippFindAttribute(response, "printer-info", IPP_TAG_TEXT)) != NULL)
      *num_options = cupsAddOption("printer-info", ippGetString(attr, 0, NULL), *num_options, options);

    if (!cupsGetOption("printer-location", *num_options, *options) && (attr = ippFindAttribute(response, "printer-location", IPP_TAG_TEXT)) != NULL)
      *num_options = cupsAddOption("printer-location", ippGetString(attr, 0, NULL), *num_options, options);
  }
  else
    _cupsLangPrintf(stderr, _("%s: Unable to create PPD file: %s"), "lpadmin", strerror(errno));

  ippDelete(response);
  httpClose(http);

  if (buffer[0])
    return (buffer);
  else
    return (NULL);
}


/*
 * 'get_printer_type()' - Determine the printer type and URI.
 */

static cups_ptype_t			/* O - printer-type value */
get_printer_type(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer name */
		 char   *uri,		/* I - URI buffer */
                 size_t urisize)	/* I - Size of URI buffer */
{
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  ipp_attribute_t	*attr;		/* printer-type attribute */
  cups_ptype_t		type;		/* printer-type value */


 /*
  * Build a GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requested-attributes
  *    requesting-user-name
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, (int)urisize, "ipp", NULL, "localhost", ippPort(), "/printers/%s", printer);

  request = ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-type");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

 /*
  * Do the request...
  */

  response = cupsDoRequest(http, request, "/");
  if ((attr = ippFindAttribute(response, "printer-type",
                               IPP_TAG_ENUM)) != NULL)
  {
    type = (cups_ptype_t)attr->values[0].integer;

    if (type & CUPS_PRINTER_CLASS)
      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, (int)urisize, "ipp", NULL, "localhost", ippPort(), "/classes/%s", printer);
  }
  else
    type = CUPS_PRINTER_LOCAL;

  ippDelete(response);

  return (type);
}


/*
 * 'set_printer_options()' - Set the printer options.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_options(
    http_t        *http,		/* I - Server connection */
    char          *printer,		/* I - Printer */
    int           num_options,		/* I - Number of options */
    cups_option_t *options,		/* I - Options */
    char          *file,		/* I - PPD file */
    int           enable)		/* I - Enable printer? */
{
  ipp_t		*request;		/* IPP Request */
  const char	*ppdfile;		/* PPD filename */
  int		ppdchanged = 0;		/* PPD changed? */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_choice_t	*choice;		/* Marked choice */
  char		uri[HTTP_MAX_URI],	/* URI for printer/class */
		line[1024],		/* Line from PPD file */
		keyword[1024],		/* Keyword from Default line */
		*keyptr,		/* Pointer into keyword... */
		tempfile[1024];		/* Temporary filename */
  cups_file_t	*in,			/* PPD file */
		*out;			/* Temporary file */
  const char	*ppdname,		/* ppd-name value */
		*protocol,		/* Old protocol option */
		*customval,		/* Custom option value */
		*boolval;		/* Boolean value */
  int		wrote_ipp_supplies = 0,	/* Wrote cupsIPPSupplies keyword? */
		wrote_snmp_supplies = 0,/* Wrote cupsSNMPSupplies keyword? */
		copied_options = 0;	/* Copied options? */


  DEBUG_printf(("set_printer_options(http=%p, printer=\"%s\", num_options=%d, "
                "options=%p, file=\"%s\")\n", http, printer, num_options,
		options, file));

 /*
  * Build a CUPS-Add-Modify-Printer or CUPS-Add-Modify-Class request,
  * which requires the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    other options
  */

  if (get_printer_type(http, printer, uri, sizeof(uri)) & CUPS_PRINTER_CLASS)
    request = ippNewRequest(IPP_OP_CUPS_ADD_MODIFY_CLASS);
  else
    request = ippNewRequest(IPP_OP_CUPS_ADD_MODIFY_PRINTER);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

 /*
  * Add the options...
  */

  if (file)
    ppdfile = file;
  else if ((ppdname = cupsGetOption("ppd-name", num_options, options)) != NULL && strcmp(ppdname, "raw") && num_options > 1)
  {
    if ((ppdfile = cupsGetServerPPD(http, ppdname)) != NULL)
    {
     /*
      * Copy options array and remove ppd-name from it...
      */

      cups_option_t *temp = NULL, *optr;
      int i, num_temp = 0;
      for (i = num_options, optr = options; i > 0; i --, optr ++)
        if (strcmp(optr->name, "ppd-name"))
	  num_temp = cupsAddOption(optr->name, optr->value, num_temp, &temp);

      copied_options = 1;
      ppdchanged     = 1;
      num_options    = num_temp;
      options        = temp;
    }
  }
  else if (request->request.op.operation_id == IPP_OP_CUPS_ADD_MODIFY_PRINTER)
    ppdfile = cupsGetPPD(printer);
  else
    ppdfile = NULL;

  cupsEncodeOptions2(request, num_options, options, IPP_TAG_OPERATION);

  if (enable)
  {
    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state", IPP_PSTATE_IDLE);
    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);
  }

  cupsEncodeOptions2(request, num_options, options, IPP_TAG_PRINTER);

  if ((protocol = cupsGetOption("protocol", num_options, options)) != NULL)
  {
    if (!_cups_strcasecmp(protocol, "bcp"))
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "port-monitor",
                   NULL, "bcp");
    else if (!_cups_strcasecmp(protocol, "tbcp"))
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "port-monitor",
                   NULL, "tbcp");
  }

  if (ppdfile)
  {
   /*
    * Set default options in the PPD file...
    */

    if ((ppd = ppdOpenFile(ppdfile)) == NULL)
    {
      int		linenum;	/* Line number of error */
      ppd_status_t	status = ppdLastError(&linenum);
					/* Status code */

      _cupsLangPrintf(stderr, _("lpadmin: Unable to open PPD \"%s\": %s on line %d."), ppdfile, ppdErrorString(status), linenum);
    }

    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, options);

    if ((out = cupsTempFile2(tempfile, sizeof(tempfile))) == NULL)
    {
      _cupsLangPrintError(NULL, _("lpadmin: Unable to create temporary file"));
      ippDelete(request);
      if (ppdfile != file)
        unlink(ppdfile);
      if (copied_options)
        cupsFreeOptions(num_options, options);
      return (1);
    }

    if ((in = cupsFileOpen(ppdfile, "r")) == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("lpadmin: Unable to open PPD file \"%s\" - %s"),
        	      ppdfile, strerror(errno));
      ippDelete(request);
      if (ppdfile != file)
	unlink(ppdfile);
      if (copied_options)
        cupsFreeOptions(num_options, options);
      cupsFileClose(out);
      unlink(tempfile);
      return (1);
    }

    while (cupsFileGets(in, line, sizeof(line)))
    {
      if (!strncmp(line, "*cupsIPPSupplies:", 17) &&
	  (boolval = cupsGetOption("cupsIPPSupplies", num_options,
	                           options)) != NULL)
      {
        wrote_ipp_supplies = 1;
        cupsFilePrintf(out, "*cupsIPPSupplies: %s\n",
	               (!_cups_strcasecmp(boolval, "true") ||
		        !_cups_strcasecmp(boolval, "yes") ||
		        !_cups_strcasecmp(boolval, "on")) ? "True" : "False");
      }
      else if (!strncmp(line, "*cupsSNMPSupplies:", 18) &&
	       (boolval = cupsGetOption("cupsSNMPSupplies", num_options,
	                                options)) != NULL)
      {
        wrote_snmp_supplies = 1;
        cupsFilePrintf(out, "*cupsSNMPSupplies: %s\n",
	               (!_cups_strcasecmp(boolval, "true") ||
		        !_cups_strcasecmp(boolval, "yes") ||
		        !_cups_strcasecmp(boolval, "on")) ? "True" : "False");
      }
      else if (strncmp(line, "*Default", 8))
        cupsFilePrintf(out, "%s\n", line);
      else
      {
       /*
        * Get default option name...
	*/

        strlcpy(keyword, line + 8, sizeof(keyword));

	for (keyptr = keyword; *keyptr; keyptr ++)
	  if (*keyptr == ':' || isspace(*keyptr & 255))
	    break;

        *keyptr++ = '\0';
        while (isspace(*keyptr & 255))
	  keyptr ++;

        if (!strcmp(keyword, "PageRegion") ||
	    !strcmp(keyword, "PageSize") ||
	    !strcmp(keyword, "PaperDimension") ||
	    !strcmp(keyword, "ImageableArea"))
	{
	  if ((choice = ppdFindMarkedChoice(ppd, "PageSize")) == NULL)
	    choice = ppdFindMarkedChoice(ppd, "PageRegion");
        }
	else
	  choice = ppdFindMarkedChoice(ppd, keyword);

        if (choice && strcmp(choice->choice, keyptr))
	{
	  if (strcmp(choice->choice, "Custom"))
	  {
	    cupsFilePrintf(out, "*Default%s: %s\n", keyword, choice->choice);
	    ppdchanged = 1;
	  }
	  else if ((customval = cupsGetOption(keyword, num_options,
	                                      options)) != NULL)
	  {
	    cupsFilePrintf(out, "*Default%s: %s\n", keyword, customval);
	    ppdchanged = 1;
	  }
	  else
	    cupsFilePrintf(out, "%s\n", line);
	}
	else
	  cupsFilePrintf(out, "%s\n", line);
      }
    }

    if (!wrote_ipp_supplies &&
	(boolval = cupsGetOption("cupsIPPSupplies", num_options,
				 options)) != NULL)
    {
      cupsFilePrintf(out, "*cupsIPPSupplies: %s\n",
		     (!_cups_strcasecmp(boolval, "true") ||
		      !_cups_strcasecmp(boolval, "yes") ||
		      !_cups_strcasecmp(boolval, "on")) ? "True" : "False");
    }

    if (!wrote_snmp_supplies &&
        (boolval = cupsGetOption("cupsSNMPSupplies", num_options,
			         options)) != NULL)
    {
      cupsFilePrintf(out, "*cupsSNMPSupplies: %s\n",
		     (!_cups_strcasecmp(boolval, "true") ||
		      !_cups_strcasecmp(boolval, "yes") ||
		      !_cups_strcasecmp(boolval, "on")) ? "True" : "False");
    }

    cupsFileClose(in);
    cupsFileClose(out);
    ppdClose(ppd);

   /*
    * Do the request...
    */

    ippDelete(cupsDoFileRequest(http, request, "/admin/",
                                ppdchanged ? tempfile : file));

   /*
    * Clean up temp files... (TODO: catch signals in case we CTRL-C during
    * lpadmin)
    */

    if (ppdfile != file)
      unlink(ppdfile);
    unlink(tempfile);
  }
  else
  {
   /*
    * No PPD file - just set the options...
    */

    ippDelete(cupsDoRequest(http, request, "/admin/"));
  }

  if (copied_options)
    cupsFreeOptions(num_options, options);

 /*
  * Check the response...
  */

  if (cupsLastError() > IPP_STATUS_OK_CONFLICTING)
  {
    _cupsLangPrintf(stderr, _("%s: %s"), "lpadmin", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'validate_name()' - Make sure the printer name only contains valid chars.
 */

static int				/* O - 0 if name is no good, 1 if name is good */
validate_name(const char *name)		/* I - Name to check */
{
  const char	*ptr;			/* Pointer into name */


 /*
  * Scan the whole name...
  */

  for (ptr = name; *ptr; ptr ++)
    if (*ptr == '@')
      break;
    else if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' || *ptr == '\\' || *ptr == '?' || *ptr == '\'' || *ptr == '\"' || *ptr == '#')
      return (0);

 /*
  * All the characters are good; validate the length, too...
  */

  return ((ptr - name) < 128);
}
