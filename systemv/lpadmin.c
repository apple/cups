/*
 * "$Id$"
 *
 *   "lpadmin" command for the Common UNIX Printing System (CUPS).
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
 *   main()                      - Parse options and configure the scheduler.
 *   add_printer_to_class()      - Add a printer to a class.
 *   default_printer()           - Set the default printing destination.
 *   delete_printer()            - Delete a printer from the system...
 *   delete_printer_from_class() - Delete a printer from a class.
 *   enable_printer()            - Enable a printer...
 *   set_printer_device()        - Set the device-uri attribute.
 *   set_printer_file()          - Set the interface script or PPD file.
 *   set_printer_info()          - Set the printer description string.
 *   set_printer_location()      - Set the printer location string.
 *   set_printer_model()         - Set the driver model file.
 *   set_printer_options()       - Set the printer options.
 *   validate_name()             - Make sure the printer name only contains
 *                                 valid chars...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/debug.h>
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */


/*
 * Local functions...
 */

static int	add_printer_to_class(http_t *, char *, char *);
static int	default_printer(http_t *, char *);
static int	delete_printer(http_t *, char *);
static int	delete_printer_from_class(http_t *, char *, char *);
static int	enable_printer(http_t *, char *);
static char	*get_line(char *, int, FILE *fp);
static int	set_printer_device(http_t *, char *, char *);
static int	set_printer_file(http_t *, char *, char *);
static int	set_printer_info(http_t *, char *, char *);
static int	set_printer_location(http_t *, char *, char *);
static int	set_printer_model(http_t *, char *, char *);
static int	set_printer_options(http_t *, char *, int, cups_option_t *);
static int	validate_name(const char *);


/*
 * 'main()' - Parse options and configure the scheduler.
 */

int
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  char		*printer,	/* Destination printer */
		*pclass,	/* Printer class name */
		*val;		/* Pointer to allow/deny value */
  int		num_options;	/* Number of options */
  cups_option_t	*options;	/* Options */


  _cupsSetLocale(argv);

  http        = NULL;
  printer     = NULL;
  num_options = 0;
  options     = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'c' : /* Add printer to class */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to add a printer to the class:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	      pclass = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
		              _("lpadmin: Expected class name after \'-c\' "
			        "option!\n"));
		return (1);
	      }

	      pclass = argv[i];
	    }

            if (!validate_name(pclass))
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Class name can only contain printable "
			      "characters!\n"));
	      return (1);
	    }

	    if (add_printer_to_class(http, printer, pclass))
	      return (1);
	    break;

        case 'd' : /* Set as default destination */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected printer name after \'-d\' "
			        "option!\n"));
		return (1);
	      }

	      printer = argv[i];
	    }

            if (!validate_name(printer))
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Printer name can only contain "
			      "printable characters!\n"));
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

	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
	                      _("lpadmin: Expected hostname after \'-h\' "
			        "option!\n"));
		return (1);
              }

              cupsSetServer(argv[i]);
	    }
	    break;

        case 'i' : /* Use the specified interface script */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to set the interface script:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	    {
	      if (set_printer_file(http, printer, argv[i] + 2))
	        return (1);
            }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected interface after \'-i\' "
			        "option!\n"));
		return (1);
	      }

	      if (set_printer_file(http, printer, argv[i]))
	        return (1);
	    }
	    break;

        case 'E' : /* Enable the printer */
	    if (printer == NULL)
	    {
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
	    }

	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

            if (enable_printer(http, printer))
	      return (1);
            break;

        case 'm' : /* Use the specified standard script/PPD file */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to set the interface script or "
			      "PPD file:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	    {
	      if (set_printer_model(http, printer, argv[i] + 2))
	        return (1);
	    }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected model after \'-m\' "
			        "option!\n"));
		return (1);
	      }

	      if (set_printer_model(http, printer, argv[i]))
	        return (1);
	    }
	    break;

        case 'o' : /* Set option */
	    if (argv[i][2])
	      num_options = cupsParseOptions(argv[i] + 2, num_options, &options);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected name=value after \'-o\' "
			        "option!\n"));
		return (1);
	      }

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

        case 'p' : /* Add/modify a printer */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected printer after \'-p\' "
			        "option!\n"));
		return (1);
	      }

	      printer = argv[i];
	    }

            if (!validate_name(printer))
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Printer name can only contain "
			      "printable characters!\n"));
	      return (1);
	    }
	    break;

        case 'r' : /* Remove printer from class */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to remove a printer from the "
			      "class:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	      pclass = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected class after \'-r\' "
			        "option!\n"));
		return (1);
	      }

	      pclass = argv[i];
	    }

            if (!validate_name(pclass))
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Class name can only contain printable "
			      "characters!\n"));
	      return (1);
	    }

            if (delete_printer_from_class(http, printer, pclass))
	      return (1);
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
	    
        case 'u' : /* Allow/deny users */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected allow/deny:userlist after "
			        "\'-u\' option!\n"));
		return (1);
	      }

              val = argv[i];
	    }

            if (!strncasecmp(val, "allow:", 6))
	      num_options = cupsAddOption("requesting-user-name-allowed",
	                                  val + 6, num_options, &options);
            else if (!strncasecmp(val, "deny:", 5))
	      num_options = cupsAddOption("requesting-user-name-denied",
	                                  val + 5, num_options, &options);
            else
	    {
	      _cupsLangPrintf(stderr,
	                      _("lpadmin: Unknown allow/deny option \"%s\"!\n"),
	                      val);
	      return (1);
	    }
	    break;

        case 'v' : /* Set the device-uri attribute */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to set the device URI:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	    {
	      if (set_printer_device(http, printer, argv[i] + 2))
	        return (1);
            }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected device URI after \'-v\' "
			        "option!\n"));
		return (1);
	      }

	      if (set_printer_device(http, printer, argv[i]))
	        return (1);
	    }
	    break;

        case 'x' : /* Delete a printer */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (argv[i][2])
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected printer or class after "
			        "\'-x\' option!\n"));
		return (1);
	      }

	      printer = argv[i];
	    }

            if (!validate_name(printer))
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Printer name can only contain "
			      "printable characters!\n"));
	      return (1);
	    }

            if (delete_printer(http, printer))
	      return (1);

	    i = argc;
	    break;

        case 'D' : /* Set the printer-info attribute */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to set the printer "
			      "description:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	    {
	      if (set_printer_info(http, printer, argv[i] + 2))
	        return (1);
	    }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected description after "
			        "\'-D\' option!\n"));
		return (1);
	      }

	      if (set_printer_info(http, printer, argv[i]))
	        return (1);
	    }
	    break;

        case 'I' : /* Set the supported file types (ignored) */
	    i ++;

	    if (i >= argc)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Expected file type(s) after \'-I\' "
			      "option!\n"));
	      return (1);
	    }

	    _cupsLangPuts(stderr,
	                  _("lpadmin: Warning - content type list ignored!\n"));
	    break;
	    
        case 'L' : /* Set the printer-location attribute */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to set the printer location:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	    {
	      if (set_printer_location(http, printer, argv[i] + 2))
	        return (1);
            }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected location after \'-L\' "
			        "option!\n"));
		return (1);
	      }

	      if (set_printer_location(http, printer, argv[i]))
	        return (1);
	    }
	    break;

        case 'P' : /* Use the specified PPD file */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(),
	                                cupsEncryption());

	      if (http == NULL)
	      {
		_cupsLangPrintf(stderr,
		                _("lpadmin: Unable to connect to server: %s\n"),
				strerror(errno));
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      _cupsLangPuts(stderr,
	                    _("lpadmin: Unable to set the PPD file:\n"
			      "         You must specify a printer name "
			      "first!\n"));
	      return (1);
	    }

	    if (argv[i][2])
	    {
	      if (set_printer_file(http, printer, argv[i] + 2))
	        return (1);
	    }
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		_cupsLangPuts(stderr,
	                      _("lpadmin: Expected PPD after \'-P\' option!\n"));
		return (1);
	      }

	      if (set_printer_file(http, printer, argv[i]))
	        return (1);
	    }
	    break;

	default :
	    _cupsLangPrintf(stderr,
	                    _("lpadmin: Unknown option \'%c\'!\n"), argv[i][1]);
	    return (1);
      }
    else
    {
      _cupsLangPrintf(stderr, _("lpadmin: Unknown argument \'%s\'!\n"),
                      argv[i]);
      return (1);
    }

 /*
  * Set options as needed...
  */

  if (num_options)
  {
    if (!http)
    {
      http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

      if (http == NULL)
      {
	_cupsLangPrintf(stderr,
			_("lpadmin: Unable to connect to server: %s\n"),
			strerror(errno));
	return (1);
      }
    }

    if (printer == NULL)
    {
      _cupsLangPuts(stderr,
                    _("lpadmin: Unable to set the printer options:\n"
		      "         You must specify a printer name first!\n"));
      return (1);
    }

    if (set_printer_options(http, printer, num_options, options))
      return (1);
  }

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
		    "[-u deny:user,user]\n"
		    "\n"));
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
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/classes/%s", pclass);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  response = cupsDoRequest(http, request, "/");

 /*
  * Build a CUPS_ADD_CLASS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    member-uris
  */

  request = ippNewRequest(CUPS_ADD_CLASS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * See if the printer is already in the class...
  */

  if (response != NULL &&
      (members = ippFindAttribute(response, "member-names", IPP_TAG_NAME)) != NULL)
    for (i = 0; i < members->num_values; i ++)
      if (strcasecmp(printer, members->values[i].string.text) == 0)
      {
        _cupsLangPrintf(stderr,
	                _("lpadmin: Printer %s is already a member of class %s.\n"),
	                printer, pclass);
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
      (members = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
  {
   /*
    * Add the printer to the existing list...
    */

    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI,
                         "member-uris", members->num_values + 1, NULL, NULL);
    for (i = 0; i < members->num_values; i ++)
      attr->values[i].string.text = strdup(members->values[i].string.text);

    attr->values[i].string.text = strdup(uri);
  }
  else
    attr = ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "member-uris", NULL, uri);

 /*
  * Then send the request...
  */

  ippDelete(response);

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'default_printer()' - Set the default printing destination.
 */

static int				/* O - 0 on success, 1 on fail */
default_printer(http_t *http,		/* I - Server connection */
                char   *printer)	/* I - Printer name */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("default_printer(%p, \"%s\")\n", http, printer));

 /*
  * Build a CUPS_SET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);

  request = ippNewRequest(CUPS_SET_DEFAULT);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'delete_printer()' - Delete a printer from the system...
 */

static int				/* O - 0 on success, 1 on fail */
delete_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to delete */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("delete_printer(%p, \"%s\")\n", http, printer));

 /*
  * Build a CUPS_DELETE_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_DELETE_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
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
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/classes/%s", pclass);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/classes/")) == NULL ||
      response->request.status.status_code == IPP_NOT_FOUND)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }

 /*
  * See if the printer is already in the class...
  */

  if ((members = ippFindAttribute(response, "member-names", IPP_TAG_NAME)) == NULL)
  {
    _cupsLangPuts(stderr, _("lpadmin: No member names were seen!\n"));

    ippDelete(response);

    return (1);
  }

  for (i = 0; i < members->num_values; i ++)
    if (!strcasecmp(printer, members->values[i].string.text))
      break;

  if (i >= members->num_values)
  {
    _cupsLangPrintf(stderr,
                    _("lpadmin: Printer %s is not a member of class %s.\n"),
	            printer, pclass);

    ippDelete(response);

    return (1);
  }

  if (members->num_values == 1)
  {
   /*
    * Build a CUPS_DELETE_CLASS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNewRequest(CUPS_DELETE_CLASS);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	 "printer-uri", NULL, uri);
  }
  else
  {
   /*
    * Build a CUPS_ADD_CLASS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    member-uris
    */

    request = ippNewRequest(CUPS_ADD_CLASS);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	 "printer-uri", NULL, uri);

   /*
    * Delete the printer from the class...
    */

    members = ippFindAttribute(response, "member-uris", IPP_TAG_URI);
    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI,
                         "member-uris", members->num_values - 1, NULL, NULL);

    for (j = 0, k = 0; j < members->num_values; j ++)
      if (j != i)
        attr->values[k ++].string.text = strdup(members->values[j].string.text);
  }

 /*
  * Then send the request...
  */

  ippDelete(response);

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'enable_printer()' - Enable a printer...
 */

static int				/* O - 0 on success, 1 on fail */
enable_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to enable */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("enable_printer(%p, \"%s\")\n", http, printer));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    printer-state
  *    printer-is-accepting-jobs
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                IPP_PRINTER_IDLE);

  ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'get_line()' - Get a line that is terminated by a LF, CR, or CR LF.
 */

static char *				/* O - Pointer to buf or NULL on EOF */
get_line(char *buf,			/* I - Line buffer */
         int  length,			/* I - Length of buffer */
	 FILE *fp)			/* I - File to read from */
{
  char	*bufptr;			/* Pointer into buffer */
  int	ch;				/* Character from file */


  length --;
  bufptr = buf;

  while ((ch = getc(fp)) != EOF)
  {
    if (ch == '\n')
      break;
    else if (ch == '\r')
    {
     /*
      * Look for LF...
      */

      ch = getc(fp);
      if (ch != '\n' && ch != EOF)
        ungetc(ch, fp);

      break;
    }

    *bufptr++ = ch;
    length --;
    if (length == 0)
      break;
  }

  *bufptr = '\0';

  if (ch == EOF)
    return (NULL);
  else
    return (buf);
}


/*
 * 'set_printer_device()' - Set the device-uri attribute.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_device(http_t *http,	/* I - Server connection */
                   char   *printer,	/* I - Printer */
		   char   *device)	/* I - New device URI */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("set_printer_device(%p, \"%s\", \"%s\")\n", http, printer,
                device));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the device URI...
  */

  if (device[0] == '/')
  {
   /*
    * Convert filename to URI...
    */

    snprintf(uri, sizeof(uri), "file://%s", device);
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
                 uri);
  }
  else
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri", NULL,
                 device);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'set_printer_file()' - Set the interface script or PPD file.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_file(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer */
		 char   *file)		/* I - PPD file or interface script */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */
#ifdef HAVE_LIBZ
  char		tempfile[1024];		/* Temporary filename */
  int		fd;			/* Temporary file */
  gzFile	*gz;			/* GZIP'd file */
  char		buffer[8192];		/* Copy buffer */
  int		bytes;			/* Bytes in buffer */


  DEBUG_printf(("set_printer_file(%p, \"%s\", \"%s\")\n", http, printer,
                file));

 /*
  * See if the file is gzip'd; if so, unzip it to a temporary file and
  * send the uncompressed file.
  */

  if (!strcmp(file + strlen(file) - 3, ".gz"))
  {
   /*
    * Yes, the file is compressed; uncompress to a temp file...
    */

    if ((fd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      _cupsLangPrintf(stderr,
                      _("lpadmin: Unable to create temporary file: %s\n"),
		      strerror(errno));
      return (1);
    }

    if ((gz = gzopen(file, "rb")) == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("lpadmin: Unable to open file \"%s\": %s\n"),
		      file, strerror(errno));
      close(fd);
      unlink(tempfile);
      return (1);
    }

    while ((bytes = gzread(gz, buffer, sizeof(buffer))) > 0)
      write(fd, buffer, bytes);

    close(fd);
    gzclose(gz);

    file = tempfile;
  }
#endif /* HAVE_LIBZ */

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  response = cupsDoFileRequest(http, request, "/admin/", file);
  ippDelete(response);

#ifdef HAVE_LIBZ
 /*
  * Remove the temporary file as needed...
  */

  if (file == tempfile)
    unlink(tempfile);
#endif /* HAVE_LIBZ */

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else
    return (0);
}


/*
 * 'set_printer_info()' - Set the printer description string.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_info(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer */
		 char   *info)		/* I - New description string */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("set_printer_info(%p, \"%s\", \"%s\")\n", http, printer,
                info));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the info string...
  */

  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info", NULL,
               info);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());
    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'set_printer_location()' - Set the printer location string.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_location(http_t *http,	/* I - Server connection */
                     char   *printer,	/* I - Printer */
		     char   *location)	/* I - New location string */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


  DEBUG_printf(("set_printer_location(%p, \"%s\", \"%s\")\n", http, printer,
                location));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the location string...
  */

  ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location", NULL,
               location);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'set_printer_model()' - Set the driver model file.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_model(http_t *http,		/* I - Server connection */
                  char   *printer,	/* I - Printer */
		  char   *model)	/* I - Driver model file */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  char		uri[HTTP_MAX_URI];	/* URI for printer/class */


 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    ppd-name
  */

  request = ippNewRequest(CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "ppd-name", NULL, model);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    return (1);
  }
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

    ippDelete(response);

    return (1);
  }
  else
  {
    ippDelete(response);

    return (0);
  }
}


/*
 * 'set_printer_options()' - Set the printer options.
 */

static int				/* O - 0 on success, 1 on fail */
set_printer_options(
    http_t        *http,		/* I - Server connection */
    char          *printer,		/* I - Printer */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* IPP attribute */
  ipp_op_t	op;			/* Operation to perform */
  const char	*ppdfile;		/* PPD filename */
  int		ppdchanged;		/* PPD changed? */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_choice_t	*choice;		/* Marked choice */
  char		uri[HTTP_MAX_URI],	/* URI for printer/class */
		line[1024],		/* Line from PPD file */
		keyword[1024],		/* Keyword from Default line */
		*keyptr,		/* Pointer into keyword... */
		tempfile[1024];		/* Temporary filename */
  FILE		*in,			/* PPD file */
		*out;			/* Temporary file */
  int		outfd;			/* Temporary file descriptor */
  const char	*protocol;		/* Old protocol option */


  DEBUG_printf(("set_printer_options(%p, \"%s\", %d, %p)\n", http, printer,
                num_options, options));

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);

 /*
  * Build a GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requested-attributes
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-type");

 /*
  * Do the request...
  */

  op = CUPS_ADD_PRINTER;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * See what kind of printer or class it is...
    */

    if ((attr = ippFindAttribute(response, "printer-type",
                                 IPP_TAG_ENUM)) != NULL)
    {
      if (attr->values[0].integer & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT))
      {
        op = CUPS_ADD_CLASS;
	httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
	                 "localhost", 0, "/classes/%s", printer);
      }
    }

    ippDelete(response);
  }

 /*
  * Build a CUPS_ADD_PRINTER or CUPS_ADD_CLASS request, which requires
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    other options
  */

  request = ippNewRequest(op);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the options...
  */

  cupsEncodeOptions2(request, num_options, options, IPP_TAG_PRINTER);

  if ((protocol = cupsGetOption("protocol", num_options, options)) != NULL)
  {
    if (!strcasecmp(protocol, "bcp"))
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "port-monitor",
                   NULL, "bcp");
    else if (!strcasecmp(protocol, "tbcp"))
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "port-monitor",
                   NULL, "tbcp");
  }

  if (op == CUPS_ADD_PRINTER)
    ppdfile = cupsGetPPD(printer);
  else
    ppdfile = NULL;

  if (ppdfile != NULL)
  {
   /*
    * Set default options in the PPD file...
    */

    ppd = ppdOpenFile(ppdfile);
    ppdMarkDefaults(ppd);
    cupsMarkOptions(ppd, num_options, options);

    if ((outfd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      _cupsLangPrintf(stderr,
                      _("lpadmin: Unable to create temporary file - %s\n"),
        	      strerror(errno));
      ippDelete(request);
      unlink(ppdfile);
      return (1);
    }

    if ((in = fopen(ppdfile, "rb")) == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("lpadmin: Unable to open PPD file \"%s\" - %s\n"),
        	      ppdfile, strerror(errno));
      ippDelete(request);
      unlink(ppdfile);
      close(outfd);
      unlink(tempfile);
      return (1);
    }

    out        = fdopen(outfd, "wb");
    ppdchanged = 0;

    while (get_line(line, sizeof(line), in) != NULL)
    {
      if (strncmp(line, "*Default", 8))
        fprintf(out, "%s\n", line);
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
	  fprintf(out, "*Default%s: %s\n", keyword, choice->choice);
	  ppdchanged = 1;
	}
	else
	  fprintf(out, "%s\n", line);
      }
    }

    fclose(in);
    fclose(out);
    close(outfd);
    ppdClose(ppd);

   /*
    * Do the request...
    */

    ippDelete(cupsDoFileRequest(http, request, "/admin/",
                                ppdchanged ? tempfile : NULL));

   /*
    * Clean up temp files... (TODO: catch signals in case we CTRL-C during
    * lpadmin)
    */

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

 /*
  * Check the response...
  */

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpadmin: %s\n", cupsLastErrorString());

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
    else if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' ||
             *ptr == '#')
      return (0);

 /*
  * All the characters are good; validate the length, too...
  */

  return ((ptr - name) < 128);
}


/*
 * End of "$Id$".
 */
