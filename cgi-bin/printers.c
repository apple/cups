/*
 * "$Id$"
 *
 *   Printer status CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"
#include <errno.h>


/*
 * Local functions...
 */

static void	do_printer_op(http_t *http, const char *printer, ipp_op_t op,
		              const char *title);
static void	show_all_printers(http_t *http, const char *username);
static void	show_printer(http_t *http, const char *printer);


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

    if (printer)
      cgiSetVariable("PRINTER_NAME", printer);
  }

 /*
  * See who is logged in...
  */

  user = getenv("REMOTE_USER");

 /*
  * Connect to the HTTP server...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

 /*
  * Get the default printer...
  */

  if (!op || !cgiIsPOST())
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
  else if (printer)
  {
    if (!strcmp(op, "start-printer"))
      do_printer_op(http, printer, IPP_RESUME_PRINTER,
                    cgiText(_("Resume Printer")));
    else if (!strcmp(op, "stop-printer"))
      do_printer_op(http, printer, IPP_PAUSE_PRINTER,
                    cgiText(_("Pause Printer")));
    else if (!strcmp(op, "accept-jobs"))
      do_printer_op(http, printer, CUPS_ACCEPT_JOBS, cgiText(_("Accept Jobs")));
    else if (!strcmp(op, "reject-jobs"))
      do_printer_op(http, printer, CUPS_REJECT_JOBS, cgiText(_("Reject Jobs")));
    else if (!strcmp(op, "purge-jobs"))
      do_printer_op(http, printer, IPP_PURGE_JOBS, cgiText(_("Purge Jobs")));
    else if (!strcasecmp(op, "print-self-test-page"))
      cgiPrintCommand(http, printer, "PrintSelfTestPage",
                      cgiText(_("Print Self-Test Page")));
    else if (!strcasecmp(op, "clean-print-heads"))
      cgiPrintCommand(http, printer, "Clean all",
                      cgiText(_("Clean Print Heads")));
    else if (!strcasecmp(op, "print-test-page"))
      cgiPrintTestPage(http, printer);
    else if (!strcasecmp(op, "move-jobs"))
      cgiMoveJobs(http, printer, 0);
    else
    {
     /*
      * Unknown/bad operation...
      */

      cgiStartHTML(printer);
      cgiCopyTemplateLang("error-op.tmpl");
      cgiEndHTML();
    }
  }
  else
  {
   /*
    * Unknown/bad operation...
    */

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
 * 'do_printer_op()' - Do a printer operation.
 */

static void
do_printer_op(http_t      *http,	/* I - HTTP connection */
              const char  *printer,	/* I - Printer name */
	      ipp_op_t    op,		/* I - Operation to perform */
	      const char  *title)	/* I - Title of page */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		resource[HTTP_MAX_URI];	/* Path for request */


 /*
  * Build a printer request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNewRequest(op);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  snprintf(resource, sizeof(resource), "/printers/%s", printer);
  ippDelete(cupsDoRequest(http, request, resource));

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(title);
    cgiShowIPPError(_("Unable to do maintenance command:"));
  }
  else
  {
   /*
    * Redirect successful updates back to the printer page...
    */

    char	url[1024],		/* Printer/class URL */
		refresh[1024];		/* Refresh URL */


    cgiRewriteURL(uri, url, sizeof(url), NULL);
    cgiFormEncode(uri, url, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "5;URL=q%s", uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(title);

    if (op == IPP_PAUSE_PRINTER)
      cgiCopyTemplateLang("printer-stop.tmpl");
    else if (op == IPP_RESUME_PRINTER)
      cgiCopyTemplateLang("printer-start.tmpl");
    else if (op == CUPS_ACCEPT_JOBS)
      cgiCopyTemplateLang("printer-accept.tmpl");
    else if (op == CUPS_REJECT_JOBS)
      cgiCopyTemplateLang("printer-reject.tmpl");
    else if (op == IPP_PURGE_JOBS)
      cgiCopyTemplateLang("printer-purge.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'show_all_printers()' - Show all printers...
 */

static void
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
  char			val[1024];	/* Form variable */


  fprintf(stderr, "DEBUG: show_all_printers(http=%p, user=\"%s\")\n",
          http, user ? user : "(null)");

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

  if (user)
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

    if ((var = cgiGetVariable("QUERY")) != NULL &&
        !cgiGetVariable("CLEAR"))
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

    sprintf(val, "%d", count);
    cgiSetVariable("TOTAL", val);

    if ((var = cgiGetVariable("ORDER")) != NULL)
      ascending = !strcasecmp(var, "asc");
    else
      ascending = 1;

    if (ascending)
    {
      for (i = 0, printer = (ipp_attribute_t *)cupsArrayIndex(printers, first);
	   i < CUPS_PAGE_MAX && printer;
	   i ++, printer = (ipp_attribute_t *)cupsArrayNext(printers))
        cgiSetIPPObjectVars(printer, NULL, i);
    }
    else
    {
      for (i = 0, printer = (ipp_attribute_t *)cupsArrayIndex(printers, count - first - 1);
	   i < CUPS_PAGE_MAX && printer;
	   i ++, printer = (ipp_attribute_t *)cupsArrayPrev(printers))
        cgiSetIPPObjectVars(printer, NULL, i);
    }

   /*
    * Save navigation URLs...
    */

    cgiSetVariable("THISURL", "/printers/");

    if (first > 0)
    {
      sprintf(val, "%d", first - CUPS_PAGE_MAX);
      cgiSetVariable("PREV", val);
    }

    if ((first + CUPS_PAGE_MAX) < count)
    {
      sprintf(val, "%d", first + CUPS_PAGE_MAX);
      cgiSetVariable("NEXT", val);
    }

   /*
    * Then show everything...
    */

    cgiCopyTemplateLang("search.tmpl");

    cgiCopyTemplateLang("printers-header.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("pager.tmpl");

    cgiCopyTemplateLang("printers.tmpl");

    if (count > CUPS_PAGE_MAX)
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

static void
show_printer(http_t     *http,		/* I - Connection to server */
             const char *printer)	/* I - Name of printer */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  char		refresh[1024];		/* Refresh URL */


  fprintf(stderr, "DEBUG: show_printer(http=%p, printer=\"%s\")\n",
          http, printer ? printer : "(null)");

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

  cgiGetAttributes(request, "printer.tmpl");

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
      snprintf(refresh, sizeof(refresh), "10;URL=/printers/%s", uri);
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

    cgiCopyTemplateLang("printer.tmpl");

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
