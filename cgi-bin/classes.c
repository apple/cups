/*
 * "$Id: classes.c 7940 2008-09-16 00:45:16Z mike $"
 *
 *   Class status CGI for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
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
 *   main()             - Main entry for CGI.
 *   do_class_op()      - Do a class operation.
 *   show_all_classes() - Show all classes...
 *   show_class()       - Show a single class.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


/*
 * Local functions...
 */

static void	do_class_op(http_t *http, const char *printer, ipp_op_t op,
		            const char *title);
static void	show_all_classes(http_t *http, const char *username);
static void	show_class(http_t *http, const char *printer);


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  const char	*pclass;		/* Class name */
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

  cgiSetVariable("SECTION", "classes");
  cgiSetVariable("REFRESH_PAGE", "");

 /*
  * See if we are displaying a printer or all classes...
  */

  if ((pclass = getenv("PATH_INFO")) != NULL)
  {
    pclass ++;

    if (!*pclass)
      pclass = NULL;

    if (pclass)
      cgiSetVariable("PRINTER_NAME", pclass);
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
    * See if we need to show a list of classes or the status of a
    * single printer...
    */

    if (!pclass)
      show_all_classes(http, user);
    else
      show_class(http, pclass);
  }
  else if (pclass)
  {
    if (!*op)
    {
      const char *server_port = getenv("SERVER_PORT");
					/* Port number string */
      int	port = atoi(server_port ? server_port : "0");
      					/* Port number */
      char	uri[1024];		/* URL */

      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri),
		       getenv("HTTPS") ? "https" : "http", NULL,
		       getenv("SERVER_NAME"), port, "/classes/%s", pclass);

      printf("Location: %s\n\n", uri);
    }
    else if (!strcmp(op, "start-class"))
      do_class_op(http, pclass, IPP_RESUME_PRINTER, cgiText(_("Resume Class")));
    else if (!strcmp(op, "stop-class"))
      do_class_op(http, pclass, IPP_PAUSE_PRINTER, cgiText(_("Pause Class")));
    else if (!strcmp(op, "accept-jobs"))
      do_class_op(http, pclass, CUPS_ACCEPT_JOBS, cgiText(_("Accept Jobs")));
    else if (!strcmp(op, "reject-jobs"))
      do_class_op(http, pclass, CUPS_REJECT_JOBS, cgiText(_("Reject Jobs")));
    else if (!strcmp(op, "purge-jobs"))
      do_class_op(http, pclass, IPP_PURGE_JOBS, cgiText(_("Purge Jobs")));
    else if (!_cups_strcasecmp(op, "print-test-page"))
      cgiPrintTestPage(http, pclass);
    else if (!_cups_strcasecmp(op, "move-jobs"))
      cgiMoveJobs(http, pclass, 0);
    else
    {
     /*
      * Unknown/bad operation...
      */

      cgiStartHTML(pclass);
      cgiCopyTemplateLang("error-op.tmpl");
      cgiEndHTML();
    }
  }
  else
  {
   /*
    * Unknown/bad operation...
    */

    cgiStartHTML(cgiText(_("Classes")));
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
 * 'do_class_op()' - Do a class operation.
 */

static void
do_class_op(http_t      *http,		/* I - HTTP connection */
            const char	*printer,	/* I - Printer name */
	    ipp_op_t    op,		/* I - Operation to perform */
	    const char  *title)		/* I - Title of page */
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
                   "localhost", 0, "/classes/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  snprintf(resource, sizeof(resource), "/classes/%s", printer);
  ippDelete(cupsDoRequest(http, request, resource));

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(title);
    cgiShowIPPError(_("Unable to do maintenance command"));
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
    snprintf(refresh, sizeof(refresh), "5;URL=%s", uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(title);

    cgiSetVariable("IS_CLASS", "YES");

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
 * 'show_all_classes()' - Show all classes...
 */

static void
show_all_classes(http_t     *http,	/* I - Connection to server */
                 const char *user)	/* I - Username */
{
  int			i;		/* Looping var */
  ipp_t			*request,	/* IPP request */
			*response;	/* IPP response */
  cups_array_t		*classes;	/* Array of class objects */
  ipp_attribute_t	*pclass;	/* Class object */
  int			ascending,	/* Order of classes (0 = descending) */
			first,		/* First class to show */
			count;		/* Number of classes */
  const char		*var;		/* Form variable */
  void			*search;	/* Search data */
  char			val[1024];	/* Form variable */


 /*
  * Show the standard header...
  */

  cgiStartHTML(cgiText(_("Classes")));

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_GET_CLASSES);

  if (user)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
        	 "requesting-user-name", NULL, user);

  cgiGetAttributes(request, "classes.tmpl");

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

    classes = cgiGetIPPObjects(response, search);
    count   = cupsArrayCount(classes);

    if (search)
      cgiFreeSearch(search);

   /*
    * Figure out which classes to display...
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

    if ((var = cgiGetVariable("ORDER")) != NULL && *var)
      ascending = !_cups_strcasecmp(var, "asc");
    else
      ascending = 1;

    if (ascending)
    {
      for (i = 0, pclass = (ipp_attribute_t *)cupsArrayIndex(classes, first);
	   i < CUPS_PAGE_MAX && pclass;
	   i ++, pclass = (ipp_attribute_t *)cupsArrayNext(classes))
        cgiSetIPPObjectVars(pclass, NULL, i);
    }
    else
    {
      for (i = 0, pclass = (ipp_attribute_t *)cupsArrayIndex(classes, count - first - 1);
	   i < CUPS_PAGE_MAX && pclass;
	   i ++, pclass = (ipp_attribute_t *)cupsArrayPrev(classes))
        cgiSetIPPObjectVars(pclass, NULL, i);
    }

   /*
    * Save navigation URLs...
    */

    cgiSetVariable("THISURL", "/classes/");

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

    cgiCopyTemplateLang("classes-header.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("pager.tmpl");

    cgiCopyTemplateLang("classes.tmpl");

    if (count > CUPS_PAGE_MAX)
      cgiCopyTemplateLang("pager.tmpl");

   /*
    * Delete the response...
    */

    cupsArrayDelete(classes);
    ippDelete(response);
  }
  else
  {
   /*
    * Show the error...
    */

    cgiShowIPPError(_("Unable to get class list"));
  }

   cgiEndHTML();
}


/*
 * 'show_class()' - Show a single class.
 */

static void
show_class(http_t     *http,		/* I - Connection to server */
           const char *pclass)		/* I - Name of class */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  char		refresh[1024];		/* Refresh URL */


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
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
               uri);

  cgiGetAttributes(request, "class.tmpl");

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

    if (pclass && (attr = ippFindAttribute(response, "printer-state",
                                            IPP_TAG_ENUM)) != NULL &&
        attr->values[0].integer == IPP_PRINTER_PROCESSING)
    {
     /*
      * Class is processing - automatically refresh the page until we
      * are done printing...
      */

      cgiFormEncode(uri, pclass, sizeof(uri));
      snprintf(refresh, sizeof(refresh), "10;URL=/classes/%s", uri);
      cgiSetVariable("refresh_page", refresh);
    }

   /*
    * Delete the response...
    */

    ippDelete(response);

   /*
    * Show the standard header...
    */

    cgiStartHTML(pclass);

   /*
    * Show the class status...
    */

    cgiCopyTemplateLang("class.tmpl");

   /*
    * Show jobs for the specified class...
    */

    cgiCopyTemplateLang("class-jobs-header.tmpl");
    cgiShowJobs(http, pclass);
  }
  else
  {
   /*
    * Show the IPP error...
    */

    cgiStartHTML(pclass);
    cgiShowIPPError(_("Unable to get class status"));
  }

   cgiEndHTML();
}


/*
 * End of "$Id: classes.c 7940 2008-09-16 00:45:16Z mike $".
 */
