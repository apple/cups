/*
 * "$Id$"
 *
 *   Class status CGI for the Common UNIX Printing System (CUPS).
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
 *   main() - Main entry for CGI.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_lang_t	*language;		/* Language information */
  char		*pclass;		/* Printer class name */
  http_t	*http;			/* Connection to the server */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  ipp_status_t	status;			/* Operation status... */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*which_jobs;		/* Which jobs to show */
  const char	*op;			/* Operation to perform, if any */
  static const char	*def_attrs[] =	/* Attributes for default printer */
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
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Connect to the HTTP server...
  */

  http = httpConnectEncrypt("localhost", ippPort(), cupsEncryption());

 /*
  * Tell the client to expect UTF-8 encoded HTML...
  */

  puts("Content-Type: text/html;charset=utf-8\n");

 /*
  * See if we need to show a list of printers or the status of a
  * single printer...
  */

  ippSetServerVersion();

  pclass = argv[0];
  if (strcmp(pclass, "/") == 0 || strcmp(pclass, "classes.cgi") == 0)
  {
    pclass = NULL;
    cgiSetVariable("TITLE", cupsLangString(language, CUPS_MSG_CLASS));
  }
  else
    cgiSetVariable("TITLE", pclass);

  cgiCopyTemplateLang(stdout, TEMPLATES, "header.tmpl", getenv("LANG"));

  if (op == NULL || strcasecmp(op, "print-test-page") != 0)
  {
   /*
    * Get the default destination...
    */

    request = ippNew();
    request->request.op.operation_id = CUPS_GET_DEFAULT;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

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
	               ippRewriteURL(attr->values[0].string.text,
		                     url, sizeof(url), NULL));
      }

      ippDelete(response);
    }

   /*
    * Get the class info...
    */

    request = ippNew();

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    if (pclass == NULL)
    {
     /*
      * Build a CUPS_GET_CLASSES request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      */

      request->request.op.operation_id = CUPS_GET_CLASSES;
      request->request.op.request_id   = 1;

      if (getenv("REMOTE_USER") != NULL)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                     NULL, getenv("REMOTE_USER"));
    }
    else
    {
     /*
      * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri
      */

      request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
      request->request.op.request_id   = 1;

      snprintf(uri, sizeof(uri), "ipp://%s/classes/%s", getenv("SERVER_NAME"),
               pclass);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                   uri);
    }

    ippGetAttributes(request, TEMPLATES, "classes.tmpl", getenv("LANG"));

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      ippSetCGIVars(response, NULL, NULL, NULL, 0);
      ippDelete(response);
    }

   /*
    * Write the report...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "classes.tmpl", getenv("LANG"));

   /*
    * Get jobs for the specified class if a class has been chosen...
    */

    if (pclass != NULL)
    {
     /*
      * Build an IPP_GET_JOBS request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri
      */

      request = ippNew();

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL, language->language);

      request->request.op.operation_id = IPP_GET_JOBS;
      request->request.op.request_id   = 1;

      snprintf(uri, sizeof(uri), "ipp://%s/classes/%s", getenv("SERVER_NAME"),
               pclass);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                   uri);

      if ((which_jobs = cgiGetVariable("which_jobs")) != NULL)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "which-jobs",
                     NULL, which_jobs);

      if (getenv("REMOTE_USER") != NULL)
      {
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                     NULL, getenv("REMOTE_USER"));

	if (strcmp(getenv("REMOTE_USER"), "root"))
	  ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
      }
      else
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                     NULL, "unknown");

      ippGetAttributes(request, TEMPLATES, "jobs.tmpl", getenv("LANG"));

     /*
      * Do the request and get back a response...
      */

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
      {
	ippSetCGIVars(response, NULL, NULL, NULL, 0);
	ippDelete(response);

	cgiCopyTemplateLang(stdout, TEMPLATES, "jobs.tmpl", getenv("LANG"));
      }
    }
  }
  else
  {
   /*
    * Print a test page...
    */

    snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s", pclass);

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

    request = ippNew();

    request->request.op.operation_id = IPP_PRINT_JOB;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    if (getenv("REMOTE_USER") != NULL)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                   NULL, getenv("REMOTE_USER"));
    else
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                   NULL, "root");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name",
        	 NULL, "Test Page");

    ippAddString(request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
        	 NULL, "application/postscript");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoFileRequest(http, request, uri + 15,
                                      CUPS_DATADIR "/data/testprint.ps")) != NULL)
    {
      status = response->request.status.status_code;
      ippSetCGIVars(response, NULL, NULL, NULL, 0);

      ippDelete(response);
    }
    else
      status = cupsLastError();

    cgiSetVariable("PRINTER_NAME", pclass);

    if (status > IPP_OK_CONFLICT)
    {
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
      cgiCopyTemplateLang(stdout, TEMPLATES, "test-page.tmpl", getenv("LANG"));
  }

  cgiCopyTemplateLang(stdout, TEMPLATES, "trailer.tmpl", getenv("LANG"));

 /*
  * Close the HTTP server connection...
  */

  httpClose(http);
  cupsLangFree(language);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * End of "$Id$".
 */
