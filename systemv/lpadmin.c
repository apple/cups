/*
 * "$Id: lpadmin.c,v 1.22.2.9 2002/06/27 19:04:33 mike Exp $"
 *
 *   "lpadmin" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */


/*
 * Local functions...
 */

static void	add_printer_to_class(http_t *, char *, char *);
static void	default_printer(http_t *, char *);
static void	delete_printer(http_t *, char *);
static void	delete_printer_from_class(http_t *, char *, char *);
static void	enable_printer(http_t *, char *);
static char	*get_line(char *, int, FILE *fp);
static void	set_printer_device(http_t *, char *, char *);
static void	set_printer_file(http_t *, char *, char *);
static void	set_printer_info(http_t *, char *, char *);
static void	set_printer_location(http_t *, char *, char *);
static void	set_printer_model(http_t *, char *, char *);
static void	set_printer_options(http_t *, char *, int, cups_option_t *);
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
		*host,		/* Pointer to hostname */
		*val;		/* Pointer to allow/deny value */
  int		num_options;	/* Number of options */
  cups_option_t	*options;	/* Options */
  http_encryption_t encryption;	/* Encryption? */


  http        = NULL;
  printer     = NULL;
  num_options = 0;
  options     = NULL;
  encryption  = cupsEncryption();

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'c' : /* Add printer to class */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to add a printer to the class:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      pclass = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected class name after \'-c\' option!\n", stderr);
		return (1);
	      }

	      pclass = argv[i];
	    }

            if (!validate_name(pclass))
	    {
	      fputs("lpadmin: Class name can only contain printable characters!\n", stderr);
	      return (1);
	    }

	    add_printer_to_class(http, printer, pclass);
	    break;

        case 'd' : /* Set as default destination */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
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
		fputs("lpadmin: Expected printer name after \'-d\' option!\n", stderr);
		return (1);
	      }

	      printer = argv[i];
	    }

            if (!validate_name(printer))
	    {
	      fputs("lpadmin: Printer name can only contain printable characters!\n", stderr);
	      return (1);
	    }

            default_printer(http, printer);
	    i = argc;
	    break;

        case 'h' : /* Connect to host */
	    if (http)
	      httpClose(http);

	    if (argv[i][2] != '\0')
	      http = httpConnectEncrypt(argv[i] + 2, ippPort(), encryption);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("lpadmin: Expected hostname after \'-h\' option!\n", stderr);
		return (1);
              }
	      else
		http = httpConnectEncrypt(argv[i], ippPort(), encryption);
	    }

	    if (http == NULL)
	    {
	      perror("lpadmin: Unable to connect to server");
	      return (1);
	    }
	    else
	      cupsSetServer(http->hostname);
	    break;

        case 'i' : /* Use the specified interface script */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the interface script:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_file(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected interface after \'-i\' option!\n", stderr);
		return (1);
	      }

	      set_printer_file(http, printer, argv[i]);
	    }
	    break;

        case 'E' : /* Enable the printer */
	    if (printer == NULL)
	    {
#ifdef HAVE_LIBSSL
	      cupsSetEncryption(encryption = HTTP_ENCRYPT_REQUIRED);

	      if (http)
		httpEncryption(http, encryption);
#else
              fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	              argv[0]);
#endif /* HAVE_LIBSSL */
	      break;
	    }

	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

            enable_printer(http, printer);
            break;

        case 'm' : /* Use the specified standard script/PPD file */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the interface script or PPD file:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_model(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected model after \'-m\' option!\n", stderr);
		return (1);
	      }

	      set_printer_model(http, printer, argv[i]);
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
		fputs("lpadmin: Expected name=value after \'-o\' option!\n", stderr);
		return (1);
	      }

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

        case 'p' : /* Add/modify a printer */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
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
		fputs("lpadmin: Expected printer after \'-p\' option!\n", stderr);
		return (1);
	      }

	      printer = argv[i];
	    }

            if (!validate_name(printer))
	    {
	      fputs("lpadmin: Printer name can only contain printable characters!\n", stderr);
	      return (1);
	    }

	    if ((host = strchr(printer, '@')) != NULL)
	    {
	     /*
	      * printer@host - reconnect to the named host...
	      */

	      httpClose(http);

              *host++ = '\0';
              if ((http = httpConnectEncrypt(host, ippPort(), encryption)) == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
	    }
	    break;

        case 'r' : /* Remove printer from class */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to remove a printer from the class:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      pclass = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected class after \'-r\' option!\n", stderr);
		return (1);
	      }

	      pclass = argv[i];
	    }

            if (!validate_name(pclass))
	    {
	      fputs("lpadmin: Class name can only contain printable characters!\n", stderr);
	      return (1);
	    }

            delete_printer_from_class(http, printer, pclass);
	    break;

        case 'u' : /* Allow/deny users */
	    if (argv[i][2])
	      val = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected name=value after \'-o\' option!\n", stderr);
		return (1);
	      }

              val = argv[i];
	    }

            if (strncasecmp(val, "allow:", 6) == 0)
	      num_options = cupsAddOption("requesting-user-name-allowed",
	                                  val + 6, num_options, &options);
            else if (strncasecmp(val, "deny:", 5) == 0)
	      num_options = cupsAddOption("requesting-user-name-denied",
	                                  val + 5, num_options, &options);
            else
	    {
	      fprintf(stderr, "lpadmin: Unknown allow/deny option \"%s\"!\n",
	              val);
	      return (1);
	    }
	    break;

        case 'v' : /* Set the device-uri attribute */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the device URI:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_device(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected device URI after \'-v\' option!\n", stderr);
		return (1);
	      }

	      set_printer_device(http, printer, argv[i]);
	    }
	    break;

        case 'x' : /* Delete a printer */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
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
		fputs("lpadmin: Expected printer or class after \'-x\' option!\n", stderr);
		return (1);
	      }

	      printer = argv[i];
	    }

            if (!validate_name(printer))
	    {
	      fputs("lpadmin: Printer name can only contain printable characters!\n", stderr);
	      return (1);
	    }

	    if ((host = strchr(printer, '@')) != NULL)
	    {
	     /*
	      * printer@host - reconnect to the named host...
	      */

	      httpClose(http);

              *host++ = '\0';
              if ((http = httpConnectEncrypt(host, ippPort(), encryption)) == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
	    }

            delete_printer(http, printer);
	    i = argc;
	    break;

        case 'D' : /* Set the printer-info attribute */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the printer description:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_info(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected description after \'-D\' option!\n", stderr);
		return (1);
	      }

	      set_printer_info(http, printer, argv[i]);
	    }
	    break;

        case 'I' : /* Set the supported file types (ignored) */
	    i ++;

	    if (i >= argc)
	    {
	      fputs("lpadmin: Expected file type(s) after \'-I\' option!\n", stderr);
	      return (1);
	    }

	    fputs("lpadmin: Warning - content type list ignored!\n", stderr);
	    break;
	    
        case 'L' : /* Set the printer-location attribute */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the printer location:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_location(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected location after \'-L\' option!\n", stderr);
		return (1);
	      }

	      set_printer_location(http, printer, argv[i]);
	    }
	    break;

        case 'P' : /* Use the specified PPD file */
	    if (!http)
	    {
              http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

	      if (http == NULL)
	      {
		perror("lpadmin: Unable to connect to server");
		return (1);
	      }
            }

	    if (printer == NULL)
	    {
	      fputs("lpadmin: Unable to set the PPD file:\n", stderr);
	      fputs("         You must specify a printer name first!\n", stderr);
	      return (1);
	    }

	    if (argv[i][2])
	      set_printer_file(http, printer, argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
		fputs("lpadmin: Expected PPD after \'-P\' option!\n", stderr);
		return (1);
	      }

	      set_printer_file(http, printer, argv[i]);
	    }
	    break;

	default :
	    fprintf(stderr, "lpadmin: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
      fprintf(stderr, "lpadmin: Unknown argument \'%s\'!\n", argv[i]);
      return (1);
    }

 /*
  * Set options as needed...
  */

  if (num_options)
  {
    if (!http)
    {
      http = httpConnectEncrypt(cupsServer(), ippPort(), encryption);

      if (http == NULL)
      {
	perror("lpadmin: Unable to connect to server");
	return (1);
      }
    }

    if (printer == NULL)
    {
      fputs("lpadmin: Unable to set the printer options:\n", stderr);
      fputs("         You must specify a printer name first!\n", stderr);
      return (1);
    }

    set_printer_options(http, printer, num_options, options);
  }

  if (printer == NULL)
  {
    puts("Usage:");
    puts("");
    puts("    lpadmin [-h server] -d destination");
    puts("    lpadmin [-h server] -x destination");
    puts("    lpadmin [-h server] -p printer [-c add-class] [-i interface] [-m model]");
    puts("                       [-r remove-class] [-v device] [-D description]");
    puts("                       [-P ppd-file] [-o name=value]");
    puts("                       [-u allow:user,user] [-u deny:user,user]");
    puts("");
  }

  if (http)
    httpClose(http);

  return (0);
}


/*
 * 'add_printer_to_class()' - Add a printer to a class.
 */

static void
add_printer_to_class(http_t *http,	/* I - Server connection */
                     char   *printer,	/* I - Printer to add */
		     char   *pclass)	/* I - Class to add to */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr,		/* Current attribute */
		*members;		/* Members in class */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s", pclass);

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

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

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_CLASS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

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
        fprintf(stderr, "lpadmin: Printer %s is already a member of class %s.\n",
	        printer, pclass);
        ippDelete(request);
	ippDelete(response);
	return;
      }

 /*
  * OK, the printer isn't part of the class, so add it...
  */

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

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
    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "member-uris", NULL, uri);

 /*
  * Then send the request...
  */

  ippDelete(response);

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fprintf(stderr, "lpadmin: add-class failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-class failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'default_printer()' - Set the default printing destination.
 */

static void
default_printer(http_t *http,		/* I - Server connection */
                char   *printer)	/* I - Printer name */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_SET_DEFAULT;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fprintf(stderr, "lpadmin: set-default failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: set-default failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'delete_printer()' - Delete a printer from the system...
 */

static void
delete_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to delete */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_DELETE_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fprintf(stderr, "lpadmin: delete-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: delete-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'delete_printer_from_class()' - Delete a printer from a class.
 */

static void
delete_printer_from_class(http_t *http,		/* I - Server connection */
                          char   *printer,	/* I - Printer to remove */
			  char   *pclass)	/* I - Class to remove from */
{
  int		i, j, k;		/* Looping vars */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr,		/* Current attribute */
		*members;		/* Members in class */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s", pclass);

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/classes/")) == NULL ||
      response->request.status.status_code == IPP_NOT_FOUND)
  {
    ippDelete(response);
    fprintf(stderr, "lpadmin: Class %s does not exist!\n", pclass);
    return;
  }

 /*
  * See if the printer is already in the class...
  */

  if ((members = ippFindAttribute(response, "member-names", IPP_TAG_NAME)) == NULL)
  {
    ippDelete(response);
    fputs("lpadmin: No member names were seen!\n", stderr);
    return;
  }

  for (i = 0; i < members->num_values; i ++)
    if (strcasecmp(printer, members->values[i].string.text) == 0)
      break;

  if (i >= members->num_values)
  {
    fprintf(stderr, "lpadmin: Printer %s is not a member of class %s.\n",
	    printer, pclass);
    ippDelete(response);
    return;
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

    request = ippNew();

    request->request.op.operation_id = CUPS_DELETE_CLASS;
    request->request.op.request_id   = 1;

    language = cupsLangDefault();

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

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

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_CLASS;
    request->request.op.request_id   = 1;

    language = cupsLangDefault();

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

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
    fprintf(stderr, "lpadmin: add/delete-class failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add/delete-class failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'enable_printer()' - Enable a printer...
 */

static void
enable_printer(http_t *http,		/* I - Server connection */
               char   *printer)		/* I - Printer to enable */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                IPP_PRINTER_IDLE);

  ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'get_line()' - Get a line that is terminated by a LF, CR, or CR LF.
 */

static char *		/* O - Pointer to buf or NULL on EOF */
get_line(char *buf,	/* I - Line buffer */
         int  length,	/* I - Length of buffer */
	 FILE *fp)	/* I - File to read from */
{
  char	*bufptr;	/* Pointer into buffer */
  int	ch;		/* Character from file */


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

static void
set_printer_device(http_t *http,	/* I - Server connection */
                   char   *printer,	/* I - Printer */
		   char   *device)	/* I - New device URI */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

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

    snprintf(uri, sizeof(uri), "file:%s", device);
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
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'set_printer_file()' - Set the interface script or PPD file.
 */

static void
set_printer_file(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer */
		 char   *file)		/* I - PPD file or interface script */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  if (strcmp(file + strlen(file) - 3, ".gz") == 0)
  {
   /*
    * Yes, the file is compressed; uncompress to a temp file...
    */

    if ((fd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      perror("lpadmin: Unable to create temporary file");
      return;
    }

    if ((gz = gzopen(file, "rb")) == NULL)
    {
      perror("lpadmin: Unable to open file");
      close(fd);
      unlink(tempfile);
      return;
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoFileRequest(http, request, "/admin/", file)) == NULL)
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }

#ifdef HAVE_LIBZ
 /*
  * Remove the temporary file as needed...
  */

  if (file == tempfile)
    unlink(tempfile);
#endif /* HAVE_LIBZ */
}


/*
 * 'set_printer_info()' - Set the printer description string.
 */

static void
set_printer_info(http_t *http,		/* I - Server connection */
                 char   *printer,	/* I - Printer */
		 char   *info)		/* I - New description string */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

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
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'set_printer_location()' - Set the printer location string.
 */

static void
set_printer_location(http_t *http,	/* I - Server connection */
                     char   *printer,	/* I - Printer */
		     char   *location)	/* I - New location string */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

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
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'set_printer_model()' - Set the driver model file.
 */

static void
set_printer_model(http_t *http,		/* I - Server connection */
                  char   *printer,	/* I - Printer */
		  char   *model)	/* I - Driver model file */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
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

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "ppd-name", NULL, model);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) == NULL)
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'set_printer_options()' - Set the printer options.
 */

static void
set_printer_options(http_t        *http,	/* I - Server connection */
                    char          *printer,	/* I - Printer */
		    int           num_options,	/* I - Number of options */
		    cups_option_t *options)	/* I - Options */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  const char	*val,			/* Option value */
		*ppdfile;		/* PPD filename */
  char		uri[HTTP_MAX_URI],	/* URI for printer/class */
		line[1024],		/* Line from PPD file */
		keyword[1024],		/* Keyword from Default line */
		*keyptr,		/* Pointer into keyword... */
		tempfile[1024];		/* Temporary filename */
  FILE		*in,			/* PPD file */
		*out;			/* Temporary file */
  int		outfd;			/* Temporary file descriptor */


  DEBUG_printf(("set_printer_options(%p, \"%s\", %d, %p)\n", http, printer,
                num_options, options));

 /*
  * Build a CUPS_ADD_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    other options
  */

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Add the options...
  */

  cupsEncodeOptions(request, num_options, options);

  if ((ppdfile = cupsGetPPD(printer)) != NULL)
  {
   /*
    * Set default options in the PPD file...
    */

    outfd = cupsTempFd(tempfile, sizeof(tempfile));

    in  = fopen(ppdfile, "rb");
    out = fdopen(outfd, "wb");

    while (get_line(line, sizeof(line), in) != NULL)
    {
      if (strncmp(line, "*Default", 8) != 0)
        fprintf(out, "%s\n", line);
      else
      {
       /*
        * Get default option name...
	*/

        strlcpy(keyword, line + 8, sizeof(keyword));

	for (keyptr = keyword; *keyptr; keyptr ++)
	  if (*keyptr == ':' || isspace(*keyptr))
	    break;

        *keyptr = '\0';

        if (strcmp(keyword, "PageRegion") == 0)
	  val = cupsGetOption("PageSize", num_options, options);
	else
	  val = cupsGetOption(keyword, num_options, options);

        if (val != NULL)
	  fprintf(out, "*Default%s: %s\n", keyword, val);
	else
	  fprintf(out, "%s\n", line);
      }
    }

    fclose(in);
    fclose(out);
    close(outfd);

   /*
    * Do the request...
    */

    response = cupsDoFileRequest(http, request, "/admin/", tempfile);

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

    response = cupsDoRequest(http, request, "/admin/");
  }

 /*
  * Do the request and get back a response...
  */

  if (response == NULL)
    fprintf(stderr, "lpadmin: add-printer failed: %s\n",
            ippErrorString(cupsLastError()));
  else
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
      fprintf(stderr, "lpadmin: add-printer failed: %s\n",
              ippErrorString(response->request.status.status_code));

    ippDelete(response);
  }
}


/*
 * 'validate_name()' - Make sure the printer name only contains valid chars.
 */

static int			/* O - 0 if name is no good, 1 if name is good */
validate_name(const char *name)	/* I - Name to check */
{
 /*
  * Scan the whole name...
  */

  while (*name)
    if (*name == '@')
      return (1);
    else if (*name <= ' ' || *name == 127)
      return (0);
    else
      name ++;

  return (1);
}


/*
 * End of "$Id: lpadmin.c,v 1.22.2.9 2002/06/27 19:04:33 mike Exp $".
 */
