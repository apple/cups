/*
 * "$Id: jobs.c,v 1.7 1999/09/27 17:36:29 mike Exp $"
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
 *   show_job_info() - Show job information.
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
#include <config.h>


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
  printf("<TITLE>%s on %s - " CUPS_SVERSION "</TITLE>\n",
         job == NULL ? "Jobs" : job, getenv("SERVER_NAME"));
  puts("<LINK REL=STYLESHEET TYPE=\"text/css\" HREF=\"/cups.css\">");
  puts("<MAP NAME=\"navbar\">");
#ifdef ESPPRINTPRO
  puts("<AREA SHAPE=\"RECT\" COORDS=\"10,10,76,30\" HREF=\"printers\" ALT=\"Current Printer Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"88,10,158,30\" HREF=\"classes\" ALT=\"Current Printer Classes Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"170,10,210,30\" HREF=\"jobs\" ALT=\"Current Jobs Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"222,10,354,30\" HREF=\"documentation.html\" ALT=\"Read CUPS Documentation On-Line\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"366,10,442,30\" HREF=\"http://www.easysw.com/printpro/software.html\" ALT=\"Download the Current ESP Print Pro Software\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"454,10,530,30\" HREF=\"http://www.easysw.com/printpro/support.html\" ALT=\"Get Tech Support for Current ESP Print Pro\">");
#else
  puts("<AREA SHAPE=\"RECT\" COORDS=\"10,10,85,30\" HREF=\"/printers\" ALT=\"Current Printer Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"95,10,175,30\" HREF=\"/classes\" ALT=\"Current Printer Classes Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"185,10,235,30\" HREF=\"/jobs\" ALT=\"Current Jobs Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"245,10,395,30\" HREF=\"/documentation.html\" ALT=\"Read CUPS Documentation On-Line\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"405,10,490,30\" HREF=\"http://www.cups.org\" ALT=\"Download the Current CUPS Software\">");
#endif /* ESPPRINTPRO */
  puts("</MAP>");
  puts("</HEAD>");
  puts("<BODY>");
  puts("<P ALIGN=CENTER>");
  puts("<A HREF=\"http://www.easysw.com\" ALT=\"Easy Software Products Home Page\">");
  puts("<IMG SRC=\"/images/logo.gif\" WIDTH=\"71\" HEIGHT=\"40\" BORDER=0 ALT=\"Easy Software Products Home Page\"></A>");
  puts("<IMG SRC=\"/images/navbar.gif\" WIDTH=\"540\" HEIGHT=\"40\" USEMAP=\"#navbar\" BORDER=0>");

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


  printf("<H1>Jobs on %s</H1>\n", getenv("SERVER_NAME"));

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

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
               NULL, "ipp://localhost/jobs/");

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
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_PRINT_JOBS));
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_JOB_STATE));
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_JOB_NAME));
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_USER_NAME));
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_PRIORITY));
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_COPIES));
    printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_FILE_SIZE));
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
	printf("<TD><A HREF=\"http://%s:%s/jobs/%d\">%s-%d</A></TD>\n",
	       getenv("SERVER_NAME"), getenv("SERVER_PORT"), job_id,
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
show_job_info(http_t      *http,	/* I - Server connection */
              cups_lang_t *language,	/* I - Language */
              char        *name)	/* I - Job "name" */
{
  int		i;		/* Looping var */
  ipp_t		*request,	/* IPP request */
		*response;	/* IPP response */
  ipp_attribute_t *attr;	/* IPP attribute */
  char		uri[HTTP_MAX_URI];/* Real URI */
  char		*job_uri,	/* job-uri */
		*printer_uri,	/* job-printer-uri */
		*job_name,	/* job-name */
		*job_user;	/* job-originating-user-name */
  int		job_id,		/* job-id */
		job_priority,	/* job-priority */
		job_k_octets;	/* job-k-octets */
  ipp_jstate_t	job_state;	/* job-state */


 /*
  * Build an IPP_GET_JOB_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOB_ATTRIBUTES;
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

  if ((response = cupsDoRequest(http, request, uri + 15)) == NULL)
  {
    puts("<P>Unable to communicate with CUPS server!");
    return;
  }

  if (response->request.status.status_code == IPP_NOT_FOUND)
  {
    puts("<P>Job does not exist or has completed.");
    ippDelete(response);
    return;
  }

 /*
  * Get the job status for this job...
  */

  if ((attr = ippFindAttribute(response, "job-uri", IPP_TAG_URI)) != NULL)
    job_uri = attr->values[0].string.text;
  else
  {
    puts("<P>Missing job-uri attribute!");
    ippDelete(request);
    return;
  }

  if ((attr = ippFindAttribute(response, "job-printer-uri", IPP_TAG_URI)) != NULL)
    printer_uri = attr->values[0].string.text;
  else
  {
    puts("<P>Missing job-printer-uri attribute!");
    ippDelete(request);
    return;
  }

  if ((attr = ippFindAttribute(response, "job-name", IPP_TAG_NAME)) != NULL)
    job_name = attr->values[0].string.text;
  else
    job_name = "unknown";

  if ((attr = ippFindAttribute(response, "job-originating-user-name",
                               IPP_TAG_NAME)) != NULL)
    job_user = attr->values[0].string.text;
  else
    job_user = "unknown";

  if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
    job_id = attr->values[0].integer;
  else
  {
    puts("<P>Missing job-id attribute!");
    ippDelete(request);
    return;
  }

  if ((attr = ippFindAttribute(response, "job-priority", IPP_TAG_INTEGER)) != NULL)
    job_priority = attr->values[0].integer;
  else
    job_priority = 50;

  if ((attr = ippFindAttribute(response, "job-k-octets", IPP_TAG_INTEGER)) != NULL)
    job_k_octets = attr->values[0].integer;
  else
    job_k_octets = 0;

  if ((attr = ippFindAttribute(response, "job-state", IPP_TAG_ENUM)) != NULL)
    job_state = (ipp_jstate_t)attr->values[0].integer;
  else
    job_state = IPP_JOB_PENDING;

 /*
  * Do a table for the job...
  */

  printf("<H1><A HREF=\"http://%s:%s/printers/%s\">%s-%d</A></H1>\n",
	 getenv("SERVER_NAME"), getenv("SERVER_PORT"),
	 strrchr(printer_uri, '/') + 1, strrchr(printer_uri, '/') + 1, job_id);

  puts("<CENTER>");
  puts("<TABLE WIDTH=\"90%\" BORDER=\"1\">");

  puts("<TR>");
  printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_JOB_STATE));
  printf("<TD>%s</TD>\n", job_state == IPP_JOB_PROCESSING ?
	 cupsLangString(language, CUPS_MSG_PROCESSING) :
	 cupsLangString(language, CUPS_MSG_PENDING));
  puts("</TR>");

  puts("<TR>");
  printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_JOB_NAME));
  printf("<TD>%s</TD>\n", job_name);
  puts("</TR>");

  puts("<TR>");
  printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_USER_NAME));
  printf("<TD>%s</TD>\n", job_user);
  puts("</TR>");

  puts("<TR>");
  printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_PRIORITY));
  printf("<TD>%d</TD>\n", job_priority);
  puts("</TR>");

  puts("<TR>");
  printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_FILE_SIZE));
  printf("<TD>%dk</TD>\n", job_k_octets);
  puts("</TR>");

  puts("<TR VALIGN=\"TOP\">");
  printf("<TH>%s</TH>\n", cupsLangString(language, CUPS_MSG_OPTIONS));
  puts("<TD>");

  for (attr = response->attrs; attr != NULL; attr = attr->next)
  {
    if (attr->group_tag != IPP_TAG_JOB &&
        attr->group_tag != IPP_TAG_EXTENSION)
      continue;

    if (strcmp(attr->name, "job-uri") == 0 ||
        strcmp(attr->name, "job-printer-uri") == 0 ||
        strcmp(attr->name, "job-name") == 0 ||
        strcmp(attr->name, "job-originating-user-name") == 0 ||
        strcmp(attr->name, "job-id") == 0 ||
        strcmp(attr->name, "job-priority") == 0 ||
        strcmp(attr->name, "job-k-octets") == 0 ||
        strcmp(attr->name, "job-state") == 0)
      continue;

    if (attr->value_tag != IPP_TAG_BOOLEAN)
      printf("%s=", attr->name);

    for (i = 0; i < attr->num_values; i ++)
    {
      if (i)
	putchar(',');

      switch (attr->value_tag)
      {
	case IPP_TAG_INTEGER :
	case IPP_TAG_ENUM :
	    printf("%d", attr->values[i].integer);
	    break;

	case IPP_TAG_BOOLEAN :
	    if (!attr->values[i].boolean)
	      printf("no");

	case IPP_TAG_NOVALUE :
	    fputs(attr->name, stdout);
	    break;

	case IPP_TAG_RANGE :
	    printf("%d-%d", attr->values[i].range.lower,
		   attr->values[i].range.upper);
	    break;

	case IPP_TAG_RESOLUTION :
	    printf("%dx%d%s", attr->values[i].resolution.xres,
		   attr->values[i].resolution.yres,
		   attr->values[i].resolution.units == IPP_RES_PER_INCH ?
		       "dpi" : "dpc");
	    break;

        case IPP_TAG_STRING :
	case IPP_TAG_TEXT :
	case IPP_TAG_NAME :
	case IPP_TAG_KEYWORD :
	case IPP_TAG_CHARSET :
	case IPP_TAG_LANGUAGE :
	case IPP_TAG_MIMETYPE :
	case IPP_TAG_URI :
	    printf("\"%s\"", attr->values[i].string.text);
	    break;
      }
    }

    puts("<BR>");
  }

  puts("</TD>");
  puts("</TR>");
  puts("</TABLE></CENTER>");

  ippDelete(response);
}


/*
 * End of "$Id: jobs.c,v 1.7 1999/09/27 17:36:29 mike Exp $".
 */
