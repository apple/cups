/*
 * "$Id: classes.c,v 1.6 1999/08/16 17:52:06 mike Exp $"
 *
 *   Class status CGI for the Common UNIX Printing System (CUPS).
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
 *   main()            - Main entry for CGI.
 *   show_class_list() - Show a list of classes...
 *   show_class_info() - Show class information.
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

static void	show_class_list(http_t *http, cups_lang_t *language);
static void	show_class_info(http_t *http, cups_lang_t *language,
		                  char *name);


/*
 * 'main()' - Main entry for CGI.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  cups_lang_t	*language;	/* Language information */
  char		*name;		/* Class name */
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
  * See if we need to show a list of classes or the status of a
  * single class...
  */

  name = argv[0];
  if (strcmp(name, "/") == 0 || strcmp(name, "classes.cgi") == 0)
    name = NULL;

 /*
  * Print the standard header...
  */

  puts("<HTML>");
  puts("<HEAD>");
  if (name)
    puts("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"10\">");
  else
    puts("<META HTTP-EQUIV=\"Refresh\" CONTENT=\"30\">");
  printf("<TITLE>%s on %s - Common UNIX Printing System</TITLE>\n",
         name == NULL ? "Classes" : name, getenv("SERVER_NAME"));
  puts("<LINK REL=STYLESHEET TYPE=\"text/css\" HREF=\"/cups.css\">");
  puts("<MAP NAME=\"navbar\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"10,10,85,30\" HREF=\"/printers\" ALT=\"Current Printer Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"95,10,175,30\" HREF=\"/classes\" ALT=\"Current Printer Classes Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"185,10,235,30\" HREF=\"/jobs\" ALT=\"Current Jobs Status\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"245,10,395,30\" HREF=\"/documentation.html\" ALT=\"Read CUPS Documentation On-Line\">");
#ifdef ESPPRINTPRO
  puts("<AREA SHAPE=\"RECT\" COORDS=\"405,10,490,30\" HREF=\"http://www.easysw.com/printpro/software.html\" ALT=\"Download the Current ESP Print Pro Software\">");
  puts("<AREA SHAPE=\"RECT\" COORDS=\"505,10,585,30\" HREF=\"http://www.easysw.com/printpro/support.html\" ALT=\"Get Tech Support for Current ESP Print Pro\">");
#else
  puts("<AREA SHAPE=\"RECT\" COORDS=\"405,10,490,30\" HREF=\"http://www.cups.org\" ALT=\"Download the Current CUPS Software\">");
#endif /* ESPPRINTPRO */
  puts("</MAP>");
  puts("</HEAD>");
  puts("<BODY>");
  puts("<P ALIGN=CENTER>");
  puts("<A HREF=\"http://www.easysw.com\" ALT=\"Easy Software Products Home Page\">");
  puts("<IMG SRC=\"/images/logo.gif\" WIDTH=\"71\" HEIGHT=\"40\" BORDER=0 ALT=\"Easy Software Products Home Page\"></A>");
#ifdef ESPPRINTPRO
  puts("<IMG SRC=\"/images/navbar.gif\" WIDTH=\"600\" HEIGHT=\"40\" USEMAP=\"#navbar\" BORDER=0>");
#else
  puts("<IMG SRC=\"/images/navbar.gif\" WIDTH=\"540\" HEIGHT=\"40\" USEMAP=\"#navbar\" BORDER=0>");
#endif /* ESPPRINTPRO */

  printf("<H1>%s on %s</H1>\n", name == NULL ? "Classes" : name,
         getenv("SERVER_NAME"));
  fflush(stdout);

  puts("<CENTER>");
  puts("<TABLE WIDTH=\"90%\" BORDER=\"1\">");
  puts("<TR>");
  puts("<TH>Name</TH>");
  puts("<TH WIDTH=\"50%\">Status</TH>");
  puts("<TH WIDTH=\"25%\">Jobs</TH>");
  puts("</TR>");

 /*
  * Show the information...
  */

  if (name == NULL)
    show_class_list(http, language);
  else
    show_class_info(http, language, name);

 /*
  * Write a standard trailer...
  */

  puts("</TABLE>");
  puts("</CENTER>");

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
 * 'show_class_list()' - Show a list of classes...
 */

static void
show_class_list(http_t      *http,	/* I - HTTP connection */
                  cups_lang_t *language)/* I - Client's language */
{
  ipp_t		*request,	/* IPP request */
		*response;	/* IPP response */
  ipp_attribute_t *attr;	/* IPP attribute */


 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_CLASSES;
  request->request.op.request_id   = 1;


  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/classes/")) != NULL)
  {
   /*
    * Loop through the classes returned in the list and display
    * their devices...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Show the class status for each class...
      */

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  show_class_info(http, language, attr->values[0].string.text);

        attr = attr->next;
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * 'show_class_info()' - Show class information.
 */

static void
show_class_info(http_t      *http,
                  cups_lang_t *language,
                  char        *name)
{
  ipp_t		*request,	/* IPP request */
		*response,	/* IPP response */
		*jobs;		/* IPP Get Jobs response */
  int		jobcount;	/* Number of jobs */
  ipp_attribute_t *attr;	/* IPP attribute */
  char		*message;	/* Printer state message */
  int		accepting;	/* Accepting requests? */
  ipp_pstate_t	pstate;		/* Printer state */
  char		uri[HTTP_MAX_URI];/* Printer URI */


 /*
  * Build a IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
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

  sprintf(uri, "ipp://localhost/classes/%s", name);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);

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
    puts("<P>Class does not exist.");
    ippDelete(response);
    return;
  }

 /*
  * Grab the needed class attributes...
  */

  if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
    pstate = (ipp_pstate_t)attr->values[0].integer;
  else
    pstate = IPP_PRINTER_IDLE;

  if ((attr = ippFindAttribute(response, "printer-state-message", IPP_TAG_TEXT)) != NULL)
    message = attr->values[0].string.text;
  else
    message = NULL;

  if ((attr = ippFindAttribute(response, "printer-is-accepting-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
    accepting = attr->values[0].boolean;
  else
    accepting = 1;

  if ((attr = ippFindAttribute(response, "printer-uri-supported", IPP_TAG_URI)) != NULL)
  {
    strcpy(uri, "http:");
    strcpy(uri + 5, strchr(attr->values[0].string.text, '/'));
  }

 /*
  * Display the class entry...
  */

  puts("<TR>");

  printf("<TD VALIGN=TOP><A HREF=\"%s\">%s</A></TD>\n", uri, name);

  puts("<TD VALIGN=TOP><IMG SRC=\"/images/classes.gif\" ALIGN=\"LEFT\">");

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
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = IPP_GET_JOBS;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
              	 "attributes-charset", NULL,
		 cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                 "attributes-natural-language", NULL,
		 language->language);

    sprintf(uri, "ipp://localhost/printers/%s", name);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	         "printer-uri", NULL, uri);

    jobs = cupsDoRequest(http, request, uri + 15);
  }
  else
    jobs = NULL;

  puts("<TD VALIGN=\"TOP\">");
  jobcount = 0;

  if (jobs != NULL)
  {
    char	*username;	/* Pointer to job-originating-user-name */
    int		jobid,		/* job-id */
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
      * Display the job if it matches the current class...
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


/*
 * End of "$Id: classes.c,v 1.6 1999/08/16 17:52:06 mike Exp $".
 */
