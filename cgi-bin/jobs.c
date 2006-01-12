/*
 * "$Id$"
 *
 *   Job status CGI for the Common UNIX Printing System (CUPS).
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
 *   main()      - Main entry for CGI.
 *   do_job_op() - Do a job operation.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"


/*
 * Local functions...
 */

static void	do_job_op(http_t *http, cups_lang_t *language, ipp_op_t op);


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_lang_t	*language;		/* Language information */
  http_t	*http;			/* Connection to the server */
  const char	*op;			/* Operation name */
  

 /*
  * Get any form variables...
  */

  cgiInitialize();

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "jobs");

 /*
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Connect to the HTTP server...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

 /*
  * Tell the client to expect UTF-8 encoded HTML...
  */

  puts("Content-Type: text/html;charset=utf-8\n");

 /*
  * Send a standard header...
  */

  cgiSetVariable("TITLE", _cupsLangString(language, _("Jobs")));

  cgiSetServerVersion();

  cgiCopyTemplateLang("header.tmpl");

  if ((op = cgiGetVariable("OP")) != NULL)
  {
   /*
    * Do the operation...
    */

    if (!strcmp(op, "cancel-job"))
      do_job_op(http, language, IPP_CANCEL_JOB);
    else if (!strcmp(op, "hold-job"))
      do_job_op(http, language, IPP_HOLD_JOB);
    else if (!strcmp(op, "release-job"))
      do_job_op(http, language, IPP_RELEASE_JOB);
    else if (!strcmp(op, "restart-job"))
      do_job_op(http, language, IPP_RESTART_JOB);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiCopyTemplateLang("job-op.tmpl");
    }
  }
  else
  {
   /*
    * Show a list of jobs...
    */

    cgiShowJobs(http, NULL);
  }

  cgiCopyTemplateLang("trailer.tmpl");

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
  ipp_status_t	status;			/* Operation status... */


  if ((job = cgiGetVariable("JOB_ID")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/jobs/%s", job);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang("error.tmpl");
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

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
               NULL, uri);

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

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
  {
    status = response->request.status.status_code;

    ippDelete(response);
  }
  else
    status = cupsLastError();

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang("error.tmpl");
  }
  else if (op == IPP_CANCEL_JOB)
    cgiCopyTemplateLang("job-cancel.tmpl");
  else if (op == IPP_HOLD_JOB)
    cgiCopyTemplateLang("job-hold.tmpl");
  else if (op == IPP_RELEASE_JOB)
    cgiCopyTemplateLang("job-release.tmpl");
  else if (op == IPP_RESTART_JOB)
    cgiCopyTemplateLang("job-restart.tmpl");
}


/*
 * End of "$Id$".
 */
