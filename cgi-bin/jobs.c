/*
 * "$Id: jobs.c,v 1.1 1999/06/21 18:45:23 mike Exp $"
 *
 *   Job status CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *   main()          - Main entry for CGI.
 *   show_job_list() - Show a list of jobs...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>


/*
 * Local functions...
 */

static void	show_job_list(http_t *http, cups_lang_t *language);
static void	show_job_info(http_t *http, cups_lang_t *language,
		              char *name);


/*
 * 'main()' - Main entry for CGI.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  cups_lang_t	*language;	/* Language information */
  char		*job;		/* Job name */
  http_t	*http;		/* Connection to the server */


 /*
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Connect to the HTTP server...
  */

  http = httpConnect("localhost", ippPort());

 /*
  * Tell the client to expect HTML...
  */

  printf("Content-Type: text/html;charset=%s\n\n", cupsLangEncoding(language));

 /*
  * See if we need to show a list of jobs or the status of a
  * single job...
  */

  job = argv[0];
  if (strcmp(job, "/") == 0 || strcmp(job, "jobs.cgi") == 0)
    job = NULL;

 /*
  * Print the standard header...
  */

  puts("<HTML>");
  puts("<HEAD>");
  if (job)
    puts("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"10\">");
  else
    puts("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"30\">");
  printf("<TITLE>%s on %s - Common UNIX Printing System</TITLE>\n",
         job == NULL ? "Jobs" : job, getenv("SERVER_NAME"));
  puts("<LINK REL=STYLESHEET TYPE=\"text/css\" HREF=\"/cups.css\">");
  puts("<MAP NAME=\"navbar\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"10,10,100,35\" HREF=\"/jobs\" ALT=\"Current Printer Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"115,10,205,35\" HREF=\"/classes\" ALT=\"Current Printer Classes Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"220,10,280,35\" HREF=\"/jobs\" ALT=\"Current Jobs Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"295,10,470,35\" HREF=\"/documentation.html\" ALT=\"Read CUPS Documentation On-Line\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"485,10,590,35\" HREF=\"http://www.cups.org\" ALT=\"Download the Current CUPS Software\">");
  puts("</MAP>");
  puts("</HEAD>");
  puts("<BODY>");
  puts("<P ALIGN=CENTER>");
  puts("<A HREF=\"http://www.easysw.com\" ALT=\"Easy Software Products Home Page\">");
  puts("<IMG SRC=\"/images/logo.gif\" WIDTH=\"71\" HEIGHT=\"40\" BORDER=0 ALT=\"Easy Software Products Home Page\"></A>");
  puts("<IMG SRC=\"/images/navbar.gif\" WIDTH=\"540\" HEIGHT=\"40\" USEMAP=\"#navbar\" BORDER=0>");

  printf("<H1>%s on %s</H1>\n", job == NULL ? "Jobs" : job,
         getenv("SERVER_NAME"));
  fflush(stdout);

 /*
  * Show the information...
  */

  if (job == NULL)
    show_job_list(http, language);
  else
    show_job_info(http, language, job);

 /*
  * Write a standard trailer...
  */

  puts("<HR>");

  puts("<P>The Common UNIX Printing System, CUPS, and the CUPS logo are the");
  puts("trademark property of <A HREF=\"http://www.easysw.com\">Easy Software");
  puts("Products</A>. CUPS is copyright 1997-1999 by Easy Software Products,");
  puts("All Rights Reserved.");

  puts("</BODY>");
  puts("</HTML>");

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
 * 'show_job_list()' - Show a list of jobs...
 */

static void
show_job_list(http_t      *http,	/* I - HTTP connection */
              cups_lang_t *language)	/* I - Client's language */
{
  ipp_t		*request,	/* IPP request */
		*response;	/* IPP response */
  ipp_attribute_t *attr;	/* IPP attribute */
  char		*job_uri,	/* job-uri */
		*printer_uri,	/* job-printer-uri */
		*job_name,	/* job-name */
		*job_user;	/* job-originating-user-name */
  int		job_id,		/* job-id */
		job_priority,	/* job-priority */
		job_k_octets,	/* job-k-octets */
		copies;		/* copies */
  ipp_jstate_t	job_state;	/* job-state */


 /*
  * Build an IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/jobs/")) != NULL)
  {
   /*
    * Do a table for the jobs...
    */

    puts("<CENTER>");
    puts("<TABLE WIDTH=\"90%\" BORDER=\"1\">");
    puts("<TR>");
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_PRINT_JOBS));
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_JOB_STATE));
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_JOB_NAME));
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_USER_NAME));
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_PRIORITY));
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_COPIES));
    printf("<TD>%s</TD>\n", cupsLangString(language, CUPS_MSG_FILE_SIZE));
    puts("</TR>");

   /*
    * Loop through the jobs returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Show the job status for each job...
      */

      job_uri      = NULL;
      printer_uri  = NULL;
      job_name     = "unknown";
      job_user     = "unknown";
      job_id       = 0;
      job_priority = 50;
      job_k_octets = 0;
      copies       = 1;
      job_state    = IPP_JOB_PENDING;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  job_uri = attr->values[0].string.text;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  printer_uri = attr->values[0].string.text;

        if (strcmp(attr->name, "job-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  job_name = attr->values[0].string.text;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  job_user = attr->values[0].string.text;

        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  job_id = attr->values[0].integer;

        if (strcmp(attr->name, "job-priority") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  job_priority = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  job_k_octets = attr->values[0].integer;

        if (strcmp(attr->name, "copies") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  copies = attr->values[0].integer;

        if (strcmp(attr->name, "job-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  job_state = (ipp_jstate_t)attr->values[0].integer;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (job_id && job_uri != NULL && printer_uri != NULL)
      {
	puts("<TR>");
	printf("<TD><A HREF=\"http://%s:%d/jobs/%d\">%s-%d</A></TD>\n",
	       getenv("SERVER_HOST"), ippPort(), job_id,
	       strrchr(printer_uri, '/') + 1, job_id);
	printf("<TD>%s</TD>\n", job_state == IPP_JOB_PROCESSING ?
	       cupsLangString(language, CUPS_MSG_PROCESSING) :
	       cupsLangString(language, CUPS_MSG_PENDING));
	printf("<TD>%s</TD>\n", job_name);
	printf("<TD>%s</TD>\n", job_user);
	printf("<TD>%d</TD>\n", job_priority);
	printf("<TD>%d</TD>\n", copies);
	printf("<TD>%dk</TD>\n", job_k_octets);
	puts("</TR>");
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);

    puts("</TABLE>");
    puts("</CENTER>");
  }
  else
    puts("<P>No jobs found.");
}


/*
 * 'show_job_info()' - Show job information.
 */

static void
show_job_info(http_t      *http,
              cups_lang_t *language,
              char        *name)
{
#if 0
  ipp_t		*request,	/* IPP request */
		*response;	/* IPP response */
  ipp_attribute_t *attr;	/* IPP attribute */
  char		*job_uri,	/* job-uri */
		*printer_uri,	/* job-printer-uri */
		*job_name,	/* job-name */
		*job_user;	/* job-originating-user-name */
  int		job_id,		/* job-id */
		job_priority,	/* job-priority */
		job_k_octets,	/* job-k-octets */
		copies;		/* copies */
  ipp_jstate_t	job_state;	/* job-state */


 /*
  * Build a IPP_GET_JOB_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  sprintf(uri, "ipp://localhost/jobs/%s", name);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, uri + 15)) != NULL)
  {
   /*
    * Grab the needed job attributes...
    */

    if ((attr = ippFindAttribute(response, "job-state", IPP_TAG_ENUM)) != NULL)
      pstate = (ipp_pstate_t)attr->values[0].integer;
    else
      pstate = IPP_PRINTER_IDLE;

    if ((attr = ippFindAttribute(response, "job-state-message", IPP_TAG_TEXT)) != NULL)
      message = attr->values[0].string.text;
    else
      message = NULL;

    if ((attr = ippFindAttribute(response, "job-is-accepting-jobs",
                                 IPP_TAG_BOOLEAN)) != NULL)
      accepting = attr->values[0].boolean;
    else
      accepting = 1;

   /*
    * Display the job entry...
    */

    puts("<TR>");

    printf("<TD VALIGN=TOP><A HREF=\"/jobs/%s\">%s</A></TD>\n", name, name);

    printf("<TD VALIGN=TOP><IMG SRC=\"/images/job-%s.gif\" ALIGN=\"LEFT\">\n",
           pstate == IPP_PRINTER_IDLE ? "idle" :
	       pstate == IPP_PRINTER_PROCESSING ? "processing" : "stopped");

    printf("%s: %s, %s<BR>\n",
           cupsLangString(language, CUPS_MSG_PRINTER_STATE),
           cupsLangString(language, pstate == IPP_PRINTER_IDLE ? CUPS_MSG_IDLE :
	                            pstate == IPP_PRINTER_PROCESSING ?
				    CUPS_MSG_PROCESSING : CUPS_MSG_STOPPED),
           cupsLangString(language, accepting ? CUPS_MSG_ACCEPTING_JOBS :
	                            CUPS_MSG_NOT_ACCEPTING_JOBS));

    if (message)
      printf("<BR CLEAR=ALL><I>\"%s\"</I>\n", message);
    else if (!accepting || pstate == IPP_PRINTER_STOPPED)
      puts("<BR CLEAR=ALL><I>\"Reason Unknown\"</I>");

    puts("</TD>");

   /*
    * Show a list of jobs as needed...
    */

    if (pstate != IPP_PRINTER_IDLE)
    {
     /*
      * Build an IPP_GET_JOBS request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    job-uri
      */

      request = ippNew();

      request->request.op.operation_id = IPP_GET_JOBS;
      request->request.op.request_id   = 1;

      attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                	  "attributes-charset", NULL,
			  cupsLangEncoding(language));

      attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                	  "attributes-natural-language", NULL,
			  language->language);

      attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	                  "job-uri", NULL, uri);

      jobs = cupsDoRequest(http, request, uri + 15);
    }
    else
      jobs = NULL;

    puts("<TD VALIGN=\"TOP\">");
    jobcount = 0;

    if (jobs != NULL)
    {
      char	*username;	/* Pointer to job-originating-user-name */
      int	jobid,		/* job-id */
		size;		/* job-k-octets */


      for (attr = jobs->attrs; attr != NULL; attr = attr->next)
      {
       /*
	* Skip leading attributes until we hit a job...
	*/

	while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
          attr = attr->next;

	if (attr == NULL)
          break;

       /*
	* Pull the needed attributes from this job...
	*/

	jobid    = 0;
	size     = 0;
	username = NULL;

	while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
	{
          if (strcmp(attr->name, "job-id") == 0 &&
	      attr->value_tag == IPP_TAG_INTEGER)
	    jobid = attr->values[0].integer;

          if (strcmp(attr->name, "job-k-octets") == 0 &&
	      attr->value_tag == IPP_TAG_INTEGER)
	    size = attr->values[0].integer;

          if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	      attr->value_tag == IPP_TAG_NAME)
	    username = attr->values[0].string.text;

          attr = attr->next;
	}

       /*
        * Display the job if it matches the current job...
	*/

        if (username != NULL)
	{
	  jobcount ++;
	  printf("<A HREF=\"/jobs/%d\">%s-%d %s %dk</A><BR>\n", jobid, name,
	         jobid, username, size);
	}

	if (attr == NULL)
          break;
      }

      ippDelete(jobs);
    }
      
    if (jobcount == 0)
      puts("None");
    puts("</TD>");
    puts("</TR>");

    ippDelete(response);
  }
#endif /* 0 */
}


/*
 * End of "$Id: jobs.c,v 1.1 1999/06/21 18:45:23 mike Exp $".
 */
