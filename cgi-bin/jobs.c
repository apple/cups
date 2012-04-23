/*
 * "$Id: jobs.c 7237 2008-01-22 01:38:39Z mike $"
 *
 *   Job status CGI for CUPS.
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

static void	do_job_op(http_t *http, int job_id, ipp_op_t op);


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* Connection to the server */
  const char	*op;			/* Operation name */
  const char	*job_id_var;		/* Job ID form variable */
  int		job_id;			/* Job ID */


 /*
  * Get any form variables...
  */

  cgiInitialize();

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "jobs");
  cgiSetVariable("REFRESH_PAGE", "");

 /*
  * Connect to the HTTP server...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

 /*
  * Get the job ID, if any...
  */

  if ((job_id_var = cgiGetVariable("JOB_ID")) != NULL)
    job_id = atoi(job_id_var);
  else
    job_id = 0;

 /*
  * Do the operation...
  */

  if ((op = cgiGetVariable("OP")) != NULL && job_id > 0 && cgiIsPOST())
  {
   /*
    * Do the operation...
    */

    if (!strcmp(op, "cancel-job"))
      do_job_op(http, job_id, IPP_CANCEL_JOB);
    else if (!strcmp(op, "hold-job"))
      do_job_op(http, job_id, IPP_HOLD_JOB);
    else if (!strcmp(op, "move-job"))
      cgiMoveJobs(http, NULL, job_id);
    else if (!strcmp(op, "release-job"))
      do_job_op(http, job_id, IPP_RELEASE_JOB);
    else if (!strcmp(op, "restart-job"))
      do_job_op(http, job_id, IPP_RESTART_JOB);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiStartHTML(cgiText(_("Jobs")));
      cgiCopyTemplateLang("error-op.tmpl");
      cgiEndHTML();
    }
  }
  else
  {
   /*
    * Show a list of jobs...
    */

    cgiStartHTML(cgiText(_("Jobs")));
    cgiShowJobs(http, NULL);
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
 * 'do_job_op()' - Do a job operation.
 */

static void
do_job_op(http_t      *http,		/* I - HTTP connection */
          int         job_id,		/* I - Job ID */
	  ipp_op_t    op)		/* I - Operation to perform */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*user;			/* Username */


 /*
  * Build a job request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri (purge-jobs)
  *    requesting-user-name
  */

  request = ippNewRequest(op);

  snprintf(uri, sizeof(uri), "ipp://localhost/jobs/%d", job_id);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
               NULL, uri);

  if ((user = getenv("REMOTE_USER")) == NULL)
    user = "guest";

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, user);

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/jobs"));

  if (cupsLastError() <= IPP_OK_CONFLICT && getenv("HTTP_REFERER"))
  {
   /*
    * Redirect successful updates back to the parent page...
    */

    char	url[1024];		/* Encoded URL */


    strcpy(url, "5;URL=");
    cgiFormEncode(url + 6, getenv("HTTP_REFERER"), sizeof(url) - 6);
    cgiSetVariable("refresh_page", url);
  }
  else if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }

  cgiStartHTML(cgiText(_("Jobs")));

  if (cupsLastError() > IPP_OK_CONFLICT)
    cgiShowIPPError(_("Job operation failed"));
  else if (op == IPP_CANCEL_JOB)
    cgiCopyTemplateLang("job-cancel.tmpl");
  else if (op == IPP_HOLD_JOB)
    cgiCopyTemplateLang("job-hold.tmpl");
  else if (op == IPP_RELEASE_JOB)
    cgiCopyTemplateLang("job-release.tmpl");
  else if (op == IPP_RESTART_JOB)
    cgiCopyTemplateLang("job-restart.tmpl");

  cgiEndHTML();
}


/*
 * End of "$Id: jobs.c 7237 2008-01-22 01:38:39Z mike $".
 */
