/*
 * "$Id$"
 *
 *   Class status CGI for the Common UNIX Printing System (CUPS).
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
 *   main()             - Main entry for CGI.
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

void	show_all_classes(http_t *http, const char *username);
void	show_class(http_t *http, const char *printer);


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

 /*
  * See if we are displaying a printer or all classes...
  */

  if ((pclass = getenv("PATH_INFO")) != NULL)
  {
    pclass ++;

    if (!*pclass)
      pclass = NULL;
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
    * See if we need to show a list of classes or the status of a
    * single printer...
    */

    if (!pclass)
      show_all_classes(http, user);
    else
      show_class(http, pclass);
  }
  else if (!strcasecmp(op, "print-test-page") && pclass)
    cgiPrintTestPage(http, pclass);
  else if (!strcasecmp(op, "move-jobs") && pclass)
    cgiMoveJobs(http, pclass, 0);
  else
  {
   /*
    * Unknown/bad operation...
    */

    if (pclass)
      cgiStartHTML(pclass);
    else
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
 * 'show_all_classes()' - Show all classes...
 */

void
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
  char			url[1024],	/* URL for prev/next/this */
			*urlptr,	/* Position in URL */
			*urlend;	/* End of URL */


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

    if ((var = cgiGetVariable("QUERY")) != NULL)
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

    sprintf(url, "%d", count);
    cgiSetVariable("TOTAL", url);

    if ((var = cgiGetVariable("ORDER")) != NULL)
      ascending = !strcasecmp(var, "asc");
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

    urlend = url + sizeof(url);

    if ((var = cgiGetVariable("QUERY")) != NULL)
    {
      strlcpy(url, "/classes/?QUERY=", sizeof(url));
      urlptr = url + strlen(url);

      cgiFormEncode(urlptr, var, urlend - urlptr);
      urlptr += strlen(urlptr);

      strlcpy(urlptr, "&", urlend - urlptr);
      urlptr += strlen(urlptr);
    }
    else
    {
      strlcpy(url, "/classes/?", sizeof(url));
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

    cgiCopyTemplateLang("classes-header.tmpl");

    if (count > 0)
      cgiCopyTemplateLang("pager.tmpl");

    cgiCopyTemplateLang("classes.tmpl");

    if (count > 0)
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

    cgiShowIPPError(_("Unable to get class list:"));
  }

   cgiEndHTML();
}


/*
 * 'show_class()' - Show a single class.
 */

void
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

  cgiGetAttributes(request, "classes.tmpl");

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
      snprintf(refresh, sizeof(refresh), "10;/classes/%s", uri);
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

    cgiCopyTemplateLang("classes.tmpl");

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
    cgiShowIPPError(_("Unable to get class status:"));
  }

   cgiEndHTML();
}


/*
 * End of "$Id$".
 */
