/*
 * "$Id: admin.c,v 1.6 2000/03/30 05:19:19 mike Exp $"
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
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"


/*
 * Local functions...
 */

static void	do_am_class(http_t *http, cups_lang_t *language, int modify);
static void	do_am_printer(http_t *http, cups_lang_t *language, int modify);
static void	do_delete_class(http_t *http, cups_lang_t *language);
static void	do_delete_printer(http_t *http, cups_lang_t *language);
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

  cgiCopyTemplateLang(stdout, TEMPLATES, "header.tmpl", getenv("LANG"));

 /*
  * See if we have form data...
  */

  if (!cgiInitialize())
  {
   /*
    * Nope, send the administration menu...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "admin.tmpl", getenv("LANG"));
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
    else if (strcmp(op, "restart-job") == 0)
      do_job_op(http, language, IPP_RESTART_JOB);
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
    else if (strcmp(op, "add-class") == 0)
      do_am_class(http, language, 0);
    else if (strcmp(op, "add-printer") == 0)
      do_am_printer(http, language, 0);
    else if (strcmp(op, "modify-class") == 0)
      do_am_class(http, language, 1);
    else if (strcmp(op, "modify-printer") == 0)
      do_am_printer(http, language, 1);
    else if (strcmp(op, "delete-class") == 0)
      do_delete_class(http, language);
    else if (strcmp(op, "delete-printer") == 0)
      do_delete_printer(http, language);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "admin-op.tmpl", getenv("LANG"));
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

    cgiCopyTemplateLang(stdout, TEMPLATES, "admin-op.tmpl", getenv("LANG"));
  }

 /*
  * Send the standard trailer...
  */

  cgiCopyTemplateLang(stdout, TEMPLATES, "trailer.tmpl", getenv("LANG"));

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
 * 'do_am_class()' - Add or modify a class.
 */

static void
do_am_class(http_t      *http,		/* I - HTTP connection */
            cups_lang_t *language,	/* I - Client's language */
	    int         modify)		/* I - Modify the printer? */
{
}


/*
 * 'do_am_printer()' - Add or modify a printer.
 */

static void
do_am_printer(http_t      *http,	/* I - HTTP connection */
              cups_lang_t *language,	/* I - Client's language */
	      int         modify)	/* I - Modify the printer? */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_status_t	status;			/* Request status */
  const char	*var;			/* CGI variable */
  char		uri[HTTP_MAX_URI],	/* Device or printer URI */
		*uriptr;		/* Pointer into URI */
  int		maxrate;		/* Maximum baud rate */
  char		baudrate[255];		/* Baud rate string */
  static int	baudrates[] =		/* Baud rates */
		{
		  1200,
		  2400,
		  4800,
		  9600,
		  19200,
		  38400,
		  57600,
		  115200,
		  230400,
		  460800
		};


  if (cgiGetVariable("PRINTER_LOCATION") == NULL)
  {
    if (modify)
    {
     /*
      * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
      * following attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri
      */

      request = ippNew();

      request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
      request->request.op.request_id   = 1;

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL, language->language);

      snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s",
               cgiGetVariable("PRINTER_NAME"));
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                   NULL, uri);

     /*
      * Do the request and get back a response...
      */

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
      {
	ippSetCGIVars(response);
	ippDelete(response);
      }

     /*
      * Update the location and description of an existing printer...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "modify-printer.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "add-printer.tmpl", getenv("LANG"));
    }
  }
  else if ((var = cgiGetVariable("DEVICE_URI")) == NULL)
  {
   /*
    * Build a CUPS_GET_DEVICES request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_GET_DEVICES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/printers/");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      ippSetCGIVars(response);
      ippDelete(response);
    }

   /*
    * Let the user choose...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-device.tmpl", getenv("LANG"));
  }
  else if (strchr(var, '/') == NULL)
  {
   /*
    * User needs to set the full URI...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-uri.tmpl", getenv("LANG"));
  }
  else if (strncmp(var, "serial:", 7) == 0 && cgiGetVariable("BAUDRATE") == NULL)
  {
   /*
    * Need baud rate, parity, etc.
    */

    if ((var = strchr(var, '?')) != NULL &&
        strncmp(var, "?baud=", 6) == 0)
      maxrate = atoi(var + 6);
    else
      maxrate = 19200;

    for (i = 0; i < 10; i ++)
      if (baudrates[i] > maxrate)
        break;
      else
      {
        sprintf(baudrate, "%d", baudrates[i]);
	cgiSetArray("BAUDRATES", i, baudrate);
      }

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-serial.tmpl", getenv("LANG"));
  }
  else if ((var = cgiGetVariable("PPD_NAME")) == NULL)
  {
   /*
    * Build a CUPS_GET_PPDS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_GET_PPDS;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/printers/");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      ippSetCGIVars(response);
      ippDelete(response);
    }

   /*
    * Let the user choose...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-model.tmpl", getenv("LANG"));
  }
  else
  {
   /*
    * Build a CUPS_ADD_PRINTER request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    printer-location
    *    printer-info
    *    ppd-name
    *    device-uri
    *    printer-is-accepting-jobs
    *    printer-state
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_PRINTER;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location",
                 NULL, cgiGetVariable("PRINTER_LOCATION"));

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
                 NULL, cgiGetVariable("PRINTER_INFO"));

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "ppd-name",
                 NULL, cgiGetVariable("PPD_NAME"));

    strcpy(uri, cgiGetVariable("DEVICE_URI"));
    if (strncmp(uri, "serial:", 7) == 0)
    {
     /*
      * Update serial port URI to include baud rate, etc.
      */

      if ((uriptr = strchr(uri, '?')) == NULL)
        uriptr = uri + strlen(uri);

      sprintf(uriptr, "?baud=%s+bits=%s+parity=%s+flow=%s",
              cgiGetVariable("BAUDRATE"), cgiGetVariable("BITS"),
	      cgiGetVariable("PARITY"), cgiGetVariable("FLOW"));
    }

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri",
                 NULL, uri);

    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                  IPP_PRINTER_IDLE);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
    {
      status = response->request.status.status_code;
      ippDelete(response);
    }
    else
      status = IPP_NOT_AUTHORIZED;

    if (status > IPP_OK_CONFLICT)
    {
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else if (modify)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-modified.tmpl", getenv("LANG"));
    else
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-added.tmpl", getenv("LANG"));
  }
}


/*
 * 'do_delete_class()' - Delete a class...
 */

static void
do_delete_class(http_t      *http,	/* I - HTTP connection */
                cups_lang_t *language)	/* I - Client's language */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*pclass;		/* Printer class name */
  ipp_status_t	status;			/* Operation status... */


  if (cgiGetVariable("CONFIRM") == NULL)
  {
    cgiCopyTemplateLang(stdout, TEMPLATES, "class-confirm.tmpl", getenv("LANG"));
    return;
  }

  if ((pclass = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s", pclass);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "class-deleted.tmpl", getenv("LANG"));
}


/*
 * 'do_delete_printer()' - Delete a printer...
 */

static void
do_delete_printer(http_t      *http,	/* I - HTTP connection */
                  cups_lang_t *language)/* I - Client's language */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*printer;		/* Printer printer name */
  ipp_status_t	status;			/* Operation status... */


  if (cgiGetVariable("CONFIRM") == NULL)
  {
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-confirm.tmpl", getenv("LANG"));
    return;
  }

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

 /*
  * Build a CUPS_DELETE_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_DELETE_PRINTER;
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-deleted.tmpl", getenv("LANG"));
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else if (op == IPP_CANCEL_JOB)
    cgiCopyTemplateLang(stdout, TEMPLATES, "job-cancel.tmpl", getenv("LANG"));
  else if (op == IPP_HOLD_JOB)
    cgiCopyTemplateLang(stdout, TEMPLATES, "job-hold.tmpl", getenv("LANG"));
  else if (op == IPP_RELEASE_JOB)
    cgiCopyTemplateLang(stdout, TEMPLATES, "job-release.tmpl", getenv("LANG"));
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else if (op == IPP_PAUSE_PRINTER)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-stop.tmpl", getenv("LANG"));
  else if (op == IPP_RESUME_PRINTER)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-start.tmpl", getenv("LANG"));
  else if (op == CUPS_ACCEPT_JOBS)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-accept.tmpl", getenv("LANG"));
  else if (op == CUPS_REJECT_JOBS)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-reject.tmpl", getenv("LANG"));
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
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
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "test-page.tmpl", getenv("LANG"));
}


/*
 * End of "$Id: admin.c,v 1.6 2000/03/30 05:19:19 mike Exp $".
 */
