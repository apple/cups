/*
 * "$Id: admin.c,v 1.3 2000/02/08 20:38:43 mike Exp $"
 *
 *   Administration CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *   main()            - Main entry for CGI.
 *   show_class_list() - Show a list of classes...
 *   show_class_info() - Show class information.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * Local functions...
 */

static void	do_job_op(http_t *http, cups_lang_t *language, ipp_op_t op);
static void	do_printer_op(http_t *http, cups_lang_t *language, ipp_op_t op);
static void	do_test_page(http_t *http, cups_lang_t *language);


/*
 * 'main()' - Main entry for CGI.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  cups_lang_t	*language;	/* Language information */
  http_t	*http;		/* Connection to the server */
  const char	*op;		/* Operation name */


 /*
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Send a standard header...
  */

  printf("Content-Type: text/html;charset=%s\n\n", cupsLangEncoding(language));

  cgiSetVariable("TITLE", "Admin");
  cgiSetVariable("SERVER_NAME", getenv("SERVER_NAME"));
  cgiSetVariable("REMOTE_USER", getenv("REMOTE_USER"));
  cgiSetVariable("CUPS_VERSION", CUPS_SVERSION);

  cgiCopyTemplateFile(stdout, TEMPLATES "/header.tmpl");

 /*
  * See if we have form data...
  */

  if (!cgiInitialize())
  {
   /*
    * Nope, send the administration menu...
    */

    cgiCopyTemplateFile(stdout, TEMPLATES "/admin.tmpl");
  }
  else if ((op = cgiGetVariable("OP")) != NULL)
  {
   /*
    * Connect to the HTTP server...
    */

    http = httpConnect("localhost", ippPort());

   /*
    * Do the operation...
    */

    if (strcmp(op, "cancel-job") == 0)
      do_job_op(http, language, IPP_CANCEL_JOB);
    else if (strcmp(op, "hold-job") == 0)
      do_job_op(http, language, IPP_HOLD_JOB);
    else if (strcmp(op, "release-job") == 0)
      do_job_op(http, language, IPP_RELEASE_JOB);
    else if (strcmp(op, "start-printer") == 0)
      do_printer_op(http, language, IPP_RESUME_PRINTER);
    else if (strcmp(op, "stop-printer") == 0)
      do_printer_op(http, language, IPP_PAUSE_PRINTER);
    else if (strcmp(op, "accept-jobs") == 0)
      do_printer_op(http, language, CUPS_ACCEPT_JOBS);
    else if (strcmp(op, "reject-jobs") == 0)
      do_printer_op(http, language, CUPS_REJECT_JOBS);
    else if (strcmp(op, "print-test-page") == 0)
      do_test_page(http, language);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiCopyTemplateFile(stdout, TEMPLATES "/admin-op.tmpl");
    }

   /*
    * Close the HTTP server connection...
    */

    httpClose(http);
  }
  else
  {
   /*
    * Form data but no operation code...  Display an error...
    */

    cgiCopyTemplateFile(stdout, TEMPLATES "/admin-op.tmpl");
  }

 /*
  * Send the standard trailer...
  */

  cgiCopyTemplateFile(stdout, TEMPLATES "/trailer.tmpl");

 /*
  * Free the request language...
  */

  cupsLangFree(language);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'do_job_op()' - Do a job operation.
 */

static void
do_job_op(http_t      *http,		/* I - HTTP connection */
          cups_lang_t *language,	/* I - Client's language */
	  ipp_op_t    op)		/* I - Operation to perform */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*job;			/* Job ID */
  const char	*printer;		/* Printer name (purge-jobs) */
  ipp_status_t	status;			/* Operation status... */


  if ((job = cgiGetVariable("JOB_ID")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/jobs/%s", job);
  else if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateFile(stdout, TEMPLATES "/error.tmpl");
    return;
  }

 /*
  * Build a job request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri (purge-jobs)
  *    requesting-user-name
  */

  request = ippNew();

  request->request.op.operation_id = op;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  if (job)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
                 NULL, uri);
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

  if (getenv("REMOTE_USER") != NULL)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                 NULL, getenv("REMOTE_USER"));
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
                 NULL, "root");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
  {
    status = response->request.status.status_code;

    ippDelete(response);
  }
  else
    status = IPP_GONE;

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateFile(stdout, TEMPLATES "/error.tmpl");
  }
  else if (op == IPP_CANCEL_JOB)
    cgiCopyTemplateFile(stdout, TEMPLATES "/job-cancel.tmpl");
  else if (op == IPP_HOLD_JOB)
    cgiCopyTemplateFile(stdout, TEMPLATES "/job-hold.tmpl");
  else if (op == IPP_RELEASE_JOB)
    cgiCopyTemplateFile(stdout, TEMPLATES "/job-release.tmpl");
}


/*
 * 'do_printer_op()' - Do a printer operation.
 */

static void
do_printer_op(http_t      *http,	/* I - HTTP connection */
              cups_lang_t *language,	/* I - Client's language */
	      ipp_op_t    op)		/* I - Operation to perform */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer;		/* Printer name (purge-jobs) */
  ipp_status_t	status;			/* Operation status... */


  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateFile(stdout, TEMPLATES "/error.tmpl");
    return;
  }

 /*
  * Build a printer request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = op;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
  {
    status = response->request.status.status_code;

    ippDelete(response);
  }
  else
    status = IPP_GONE;

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateFile(stdout, TEMPLATES "/error.tmpl");
  }
  else if (op == IPP_PAUSE_PRINTER)
    cgiCopyTemplateFile(stdout, TEMPLATES "/printer-stop.tmpl");
  else if (op == IPP_RESUME_PRINTER)
    cgiCopyTemplateFile(stdout, TEMPLATES "/printer-start.tmpl");
  else if (op == CUPS_ACCEPT_JOBS)
    cgiCopyTemplateFile(stdout, TEMPLATES "/printer-accept.tmpl");
  else if (op == CUPS_REJECT_JOBS)
    cgiCopyTemplateFile(stdout, TEMPLATES "/printer-reject.tmpl");
}


/*
 * 'do_test_page()' - Send a test page.
 */

static void
do_test_page(http_t      *http,		/* I - HTTP connection */
             cups_lang_t *language)	/* I - Client's language */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*printer;		/* Printer name (purge-jobs) */
  ipp_status_t	status;			/* Operation status... */


  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateFile(stdout, TEMPLATES "/error.tmpl");
    return;
  }

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
    ippSetCGIVars(response);

    ippDelete(response);
  }
  else
    status = IPP_GONE;

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateFile(stdout, TEMPLATES "/error.tmpl");
  }
  else
    cgiCopyTemplateFile(stdout, TEMPLATES "/test-page.tmpl");
}


/*
 * End of "$Id: admin.c,v 1.3 2000/02/08 20:38:43 mike Exp $".
 */
