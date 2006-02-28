/*
 * "$Id$"
 *
 *   Printer status CGI for the Common UNIX Printing System (CUPS).
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
 *   main()              - Main entry for CGI.
 *   print_command()     - Send a print command to the printer.
 *   show_all_printers() - Show all printers...
 *   show_printer()      - Show a single printer.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"
#include <errno.h>


/*
 * Local functions...
 */

void	print_command(http_t *http, const char *printer, const char *command);
void	show_all_printers(http_t *http, const char *username);
void	show_printer(http_t *http, const char *printer);


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  const char	*printer;		/* Printer name */
  const char	*user;			/* Username */
  http_t	*http;			/* Connection to the server */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  const char	*op;			/* Operation to perform, if any */
  static const char *def_attrs[] =	/* Attributes for default printer */
		{
		  "printer-name",
		  "printer-uri-supported"
		};


 /*
  * Get any form variables...
  */

  cgiInitialize();

  op = cgiGetVariable("OP");

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "printers");

 /*
  * See if we are displaying a printer or all printers...
  */

  if ((printer = getenv("PATH_INFO")) != NULL)
  {
    printer ++;

    if (!*printer)
      printer = NULL;
  }

 /*
  * See who is logged in...
  */

  if ((user = getenv("REMOTE_USER")) == NULL)
    user = "guest";

 /*
  * Connect to the HTTP server...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

 /*
  * Get the default printer...
  */

  if (!op)
  {
   /*
    * Get the default destination...
    */

    request = ippNewRequest(CUPS_GET_DEFAULT);

    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes",
		  sizeof(def_attrs) / sizeof(def_attrs[0]), NULL, def_attrs);

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      if ((attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME)) != NULL)
        cgiSetVariable("DEFAULT_NAME", attr->values[0].string.text);

      if ((attr = ippFindAttribute(response, "printer-uri-supported", IPP_TAG_URI)) != NULL)
      {
	char	url[HTTP_MAX_URI];	/* New URL */


        cgiSetVariable("DEFAULT_URI",
	               cgiRewriteURL(attr->values[0].string.text,
		                     url, sizeof(url), NULL));
      }

      ippDelete(response);
    }

   /*
    * See if we need to show a list of printers or the status of a
    * single printer...
    */

    if (!printer)
      show_all_printers(http, user);
    else
      show_printer(http, printer);
  }
  else if (!strcasecmp(op, "print-self-test-page") && printer)
    print_command(http, printer, "PrintSelfTestPage");
  else if (!strcasecmp(op, "clean-print-heads") && printer)
    print_command(http, printer, "Clean all");
  else if (!strcasecmp(op, "print-test-page") && printer)
    cgiPrintTestPage(http, printer);
  else if (!strcasecmp(op, "move-jobs") && printer)
    cgiMoveJobs(http, printer, 0);
  else
  {
   /*
    * Unknown/bad operation...
    */

    if (printer)
      cgiStartHTML(printer);
    else
      cgiStartHTML(cgiText(_("Printers")));

    cgiCopyTemplateLang("error-op.tmpl");
    cgiEndHTML();
  }

 /*
  * Close the HTTP server connection...
  */

  httpClose(http);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'print_command()' - Send a print command to the printer.
 */

void
print_command(http_t     *http,		/* I - Connection to server */
              const char *printer,	/* I - Printer */
	      const char *command)	/* I - Command to send */
{
  cups_file_t	*fp;			/* File pointer */
  char		filename[1024];		/* Temporary file */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		resource[1024],		/* POST resource path */
		refresh[1024];		/* Refresh URL */
  const char	*user;			/* Username */


 /*
  * See who is logged in...
  */

  if ((user = getenv("REMOTE_USER")) == NULL)
    user = "guest";

 /*
  * Create the CUPS command file to print...
  */

  if ((fp = cupsTempFile2(filename, sizeof(filename))) == NULL)
  {
    cgiStartHTML(cgiText(_("Printer Maintenance")));
    cgiSetVariable("MESSAGE", _("Unable to create temporary file:"));
    cgiSetVariable("ERROR", strerror(errno));
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

  cupsFilePuts(fp, "#CUPS-COMMAND\n");
  cupsFilePrintf(fp, "%s\n", command);
  cupsFileClose(fp);

 /*
  * Point to the printer...
  */

  snprintf(resource, sizeof(resource), "/printers/%s", printer);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", ippPort(), "/printers/%s", printer);

 /*
  * Build an IPP_PRINT_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    document-format
  */

  request = ippNewRequest(IPP_PRINT_JOB);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, user);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name",
               NULL, "Printer Maintenance");

  ippAddString(request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
               NULL, "application/postscript");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoFileRequest(http, request, resource,
                                    filename)) != NULL)
  {
    cgiSetIPPVars(response, NULL, NULL, NULL, 0);

    ippDelete(response);
  }

  unlink(filename);

  if (cupsLastError() <= IPP_OK_CONFLICT)
  {
   /*
    * Automatically reload the printer status page...
    */

    cgiFormEncode(uri, resource, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "2;%s", uri);
    cgiSetVariable("refresh_page", refresh);
  }

  cgiStartHTML(cgiText(_("Printer Maintenance")));

  if (cupsLastError() > IPP_OK_CONFLICT)
    cgiShowIPPError(_("Unable to send maintenance job:"));
  else
  {
    cgiSetVariable("PRINTER_NAME", printer);

    cgiCopyTemplateLang("maintenance.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'show_all_printers()' - Show all printers...
 */

void
show_all_printers(http_t     *http,	/* I - Connection to server */
                  const char *user)	/* I - Username */
{
  int			i;		/* Looping var */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  cups_array_t		*printers;	/* Array of printer objects */
  ipp_attribute_t	*printer,	/* Printer object */
			*attr;		/* Current attribute */
  int			ascending,	/* Order of printers (0 = descending) */
			first,		/* First printer to show */
			count;		/* Number of printers */
  const char		*var;		/* Form variable */
  void			*search;	/* Search data */
  char			url[1024],	/* URL for prev/next/this */
			*urlptr,	/* Position in URL */
			*urlend;	/* End of URL */


  fprintf(stderr, "DEBUG: show_all_printers(http=%p, user=\"%s\")\n",
          http, user);

 /*
  * Show the standard header...
  */

  cgiStartHTML(cgiText(_("Printers")));

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-type
  *    printer-type-mask
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type", 0);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type-mask", CUPS_PRINTER_CLASS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, user);

  cgiGetAttributes(request, "printers.tmpl");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Get a list of matching job objects.
    */

    if ((var = cgiGetVariable("QUERY")) != NULL)
      search = cgiCompileSearch(var);
    else
      search = NULL;

    printers  = cgiGetIPPObjects(response, search);
    count     = cupsArrayCount(printers);

    if (search)
      cgiFreeSearch(search);

   /*
    * Figure out which printers to display...
    */

    if ((var = cgiGetVariable("FIRST")) != NULL)
      first = atoi(var);
    else
      first = 0;

    if (first >= count)
      first = count - CUPS_PAGE_MAX;

    first = (first / CUPS_PAGE_MAX) * CUPS_PAGE_MAX;

    if (first < 0)
      first = 0;

    sprintf(url, "%d", count);
    cgiSetVariable("TOTAL", url);

    if ((var = cgiGetVariable("ORDER")) != NULL)
      ascending = !strcasecmp(var, "asc");
    else
      ascending = 1;

    if (ascending)
    {
      for (i = 0, printer = (ipp_attribute_t *)cupsArrayIndex(printers, first);
	   i < CUPS_PAGE_MAX && printer;
	   i ++, printer = (ipp_attribute_t *)cupsArrayNext(printers))
      {
        cgiSetIPPObjectVars(printer, NULL, i);

        cgiSetArray("cupscommand", i, "0");

	for (attr = printer; attr; attr = attr->next)
	  if (attr->group_tag != IPP_TAG_PRINTER || !attr->name)
	    break;
	  else if (!strcmp(attr->name, "printer-type"))
	  {
            if (attr->values[0].integer & CUPS_PRINTER_COMMANDS)
	      cgiSetArray("cupscommand", i, "1");
	    break;
	  }
      }
    }
    else
    {
      for (i = 0, printer = (ipp_attribute_t *)cupsArrayIndex(printers, count - first - 1);
	   i < CUPS_PAGE_MAX && printer;
	   i ++, printer = (ipp_attribute_t *)cupsArrayPrev(printers))
      {
        cgiSetIPPObjectVars(printer, NULL, i);

        cgiSetArray("cupscommand", i, "0");

	for (attr = printer; attr; attr = attr->next)
	  if (attr->group_tag == IPP_TAG_ZERO || !attr->name)
	    break;
	  else if (!strcmp(attr->name, "printer-type"))
	  {
            if (attr->values[0].integer & CUPS_PRINTER_COMMANDS)
	      cgiSetArray("cupscommand", i, "1");
	    break;
	  }
      }
    }

   /*
    * Save navigation URLs...
    */

    urlend = url + sizeof(url);

    if ((var = cgiGetVariable("QUERY")) != NULL)
    {
      strlcpy(url, "/printers/?QUERY=", sizeof(url));
      urlptr = url + strlen(url);

      cgiFormEncode(urlptr, var, urlend - urlptr);
      urlptr += strlen(urlptr);

      strlcpy(urlptr, "&", urlend - urlptr);
      urlptr += strlen(urlptr);
    }
    else
    {
      strlcpy(url, "/printers/?", sizeof(url));
      urlptr = url + strlen(url);
    }

    snprintf(urlptr, urlend - urlptr, "FIRST=%d", first);
    cgiSetVariable("THISURL", url);

    if (first > 0)
    {
      snprintf(urlptr, urlend - urlptr, "FIRST=%d&ORDER=%s",
	       first - CUPS_PAGE_MAX, ascending ? "asc" : "dec");
      cgiSetVariable("PREVURL", url);
    }

    if ((first + CUPS_PAGE_MAX) < count)
    {
      snprintf(urlptr, urlend - urlptr, "FIRST=%d&ORDER=%s",
	       first + CUPS_PAGE_MAX, ascending ? "asc" : "dec");
      cgiSetVariable("NEXTURL", url);
    }

   /*
    * Then show everything...
    */

    cgiCopyTemplateLang("search.tmpl");

    cgiCopyTemplateLang("printers-header.tmpl");

    if (count > 0)
      cgiCopyTemplateLang("pager.tmpl");

    cgiCopyTemplateLang("printers.tmpl");

    if (count > 0)
      cgiCopyTemplateLang("pager.tmpl");

   /*
    * Delete the response...
    */

    cupsArrayDelete(printers);
    ippDelete(response);
  }
  else
  {
   /*
    * Show the error...
    */

    cgiShowIPPError(_("Unable to get printer list:"));
  }

   cgiEndHTML();
}


/*
 * 'show_printer()' - Show a single printer.
 */

void
show_printer(http_t     *http,		/* I - Connection to server */
             const char *printer)	/* I - Name of printer */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  char		refresh[1024];		/* Refresh URL */


  fprintf(stderr, "DEBUG: show_printer(http=%p, printer=\"%s\")\n",
          http, printer);

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
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
               uri);

  cgiGetAttributes(request, "printers.tmpl");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Got the result; set the CGI variables and check the status of a
    * single-queue request...
    */

    cgiSetIPPVars(response, NULL, NULL, NULL, 0);

    if ((attr = ippFindAttribute(response, "printer-type",
                                 IPP_TAG_ENUM)) != NULL)
    {
      cgiSetVariable("cupscommand",
                     (attr->values[0].integer & CUPS_PRINTER_COMMANDS) ?
		         "1" : "0");
    }

    if (printer && (attr = ippFindAttribute(response, "printer-state",
                                            IPP_TAG_ENUM)) != NULL &&
        attr->values[0].integer == IPP_PRINTER_PROCESSING)
    {
     /*
      * Printer is processing - automatically refresh the page until we
      * are done printing...
      */

      cgiFormEncode(uri, printer, sizeof(uri));
      snprintf(refresh, sizeof(refresh), "10;/printers/%s", uri);
      cgiSetVariable("refresh_page", refresh);
    }

   /*
    * Delete the response...
    */

    ippDelete(response);

   /*
    * Show the standard header...
    */

    cgiStartHTML(printer);

   /*
    * Show the printer status...
    */

    cgiCopyTemplateLang("printers.tmpl");

   /*
    * Show jobs for the specified printer...
    */

    cgiCopyTemplateLang("printer-jobs-header.tmpl");
    cgiShowJobs(http, printer);
  }
  else
  {
   /*
    * Show the IPP error...
    */

    cgiStartHTML(printer);
    cgiShowIPPError(_("Unable to get printer status:"));
  }

   cgiEndHTML();
}


/*
 * End of "$Id$".
 */
