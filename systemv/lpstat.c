/*
 * "$Id: lpstat.c,v 1.6 1999/04/21 14:16:29 mike Exp $"
 *
 *   "lpstat" command for the Common UNIX Printing System (CUPS).
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
 *   main()           - Parse options and show status information.
 *   show_accepting() - Show acceptance status.
 *   show_classes()   - Show printer classes.
 *   show_default()   - Show default destination.
 *   show_devices()   - Show printer devices.
 *   show_jobs()      - Show active print jobs.
 *   show_printers()  - Show printers.
 *   show_scheduler() - Show scheduler status.
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

static void	show_accepting(http_t *, char *);
static void	show_classes(http_t *, char *);
static void	show_default(http_t *);
static void	show_devices(http_t *, char *);
static void	show_jobs(http_t *, char *, char *);
static void	show_printers(http_t *, char *);
static void	show_scheduler(http_t *);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;	/* Looping var */
  http_t	*http;	/* Connection to server */


  http = httpConnect("localhost", ippPort());

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'a' : /* Show acceptance status */
	    if (argv[i][2] != '\0')
	      show_accepting(http, argv[i] + 2);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_accepting(http, argv[i]);
	    }
	    else
	      show_accepting(http, NULL);
	    break;

        case 'c' : /* Show classes and members */
	    if (argv[i][2] != '\0')
	      show_classes(http, argv[i] + 2);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_classes(http, argv[i]);
	    }
	    else
	      show_classes(http, NULL);
	    break;

        case 'd' : /* Show default destination */
	    show_default(http);
	    break;

        case 'o' : /* Show jobs by destination */
	    if (argv[i][2] != '\0')
	      show_jobs(http, argv[i] + 2, NULL);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_jobs(http, argv[i], NULL);
	    }
	    else
	      show_jobs(http, NULL, NULL);
	    break;

        case 'p' : /* Show printers */
	    if (argv[i][2] != '\0')
	      show_printers(http, argv[i] + 2);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_printers(http, argv[i]);
	    }
	    else
	      show_printers(http, NULL);
	    break;

        case 'r' : /* Show scheduler status */
	    show_scheduler(http);
	    break;

        case 's' : /* Show summary */
	    show_default(http);
	    show_classes(http, NULL);
	    show_devices(http, NULL);
	    break;

        case 't' : /* Show all info */
	    show_scheduler(http);
	    show_default(http);
	    show_classes(http, NULL);
	    show_devices(http, NULL);
	    show_accepting(http, NULL);
	    show_printers(http, NULL);
	    show_jobs(http, NULL, NULL);
	    break;

        case 'u' : /* Show jobs by user */
	    if (argv[i][2] != '\0')
	      show_jobs(http, NULL, argv[i] + 2);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_jobs(http, NULL, argv[i]);
	    }
	    else
	      show_jobs(http, NULL, NULL);
	    break;

        case 'v' : /* Show printer devices */
	    if (argv[i][2] != '\0')
	      show_devices(http, argv[i] + 2);
	    else if ((i + 1) < argc && argv[i + 1][0] != '-')
	    {
	      i ++;
	      show_devices(http, argv[i]);
	    }
	    else
	      show_devices(http, NULL);
	    break;


	default :
	    fprintf(stderr, "lpstat: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
      fprintf(stderr, "lpstat: Unknown argument \'%s\'!\n", argv[i]);
      return (1);
    }

  if (argc == 1)
    show_jobs(http, NULL, cuserid(NULL));

  return (0);
}


/*
 * 'show_accepting()' - Show acceptance status.
 */

static void
show_accepting(http_t *http,	/* I - HTTP connection to server */
               char   *dests)	/* I - Destinations */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		*printer,	/* Printer name */
		*message;	/* Printer device URI */
  int		accepting;	/* Accepting requests? */
  char		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */


  DEBUG_printf(("show_accepting(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/printers/")) != NULL)
  {
    DEBUG_puts("show_accepting: request succeeded...");

   /*
    * Loop through the printers returned in the list and display
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
      * Pull the needed attributes from this job...
      */

      printer   = NULL;
      message   = NULL;
      accepting = 1;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state-message") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-is-accepting-jobs") == 0 &&
	    attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = dests == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
        if (accepting)
	  printf("%s accepting requests\n", printer);
	else
	  printf("%s not accepting requests -\n\t%s\n", printer,
	         message == NULL ? "reason unknown" : message);
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * 'show_classes()' - Show printer classes.
 */

static void
show_classes(http_t *http,	/* I - HTTP connection to server */
             char   *dests)	/* I - Destinations */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */


  DEBUG_printf(("show_classes(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

}


/*
 * 'show_default()' - Show default destination.
 */

static void
show_default(http_t *http)	/* I - HTTP connection to server */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */


  DEBUG_printf(("show_default(%08x)\n", http));

  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_DEFAULT;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/printers/")) != NULL)
  {
    if ((attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME)) != NULL)
      printf("system default destination: %s\n", attr->values[0].string.text);

    ippDelete(response);
  }
  else
    puts("no system default destination");
}


/*
 * 'show_devices()' - Show printer devices.
 */

static void
show_devices(http_t *http,	/* I - HTTP connection to server */
             char   *dests)	/* I - Destinations */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		*printer,	/* Printer name */
		*device;	/* Printer device URI */
  char		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */


  DEBUG_printf(("show_devices(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/printers/")) != NULL)
  {
    DEBUG_puts("show_devices: request succeeded...");

   /*
    * Loop through the printers returned in the list and display
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
      * Pull the needed attributes from this job...
      */

      printer = NULL;
      device  = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-device-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  device = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL || device == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = dests == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
        if (strncmp(device, "file:", 5) == 0)
          printf("device for %s: %s\n", printer, device + 5);
        else
          printf("device for %s: %s\n", printer, device);
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * 'show_jobs()' - Show active print jobs.
 */

static void
show_jobs(http_t *http,		/* I - HTTP connection to server */
          char   *dests,	/* I - Destinations */
          char   *users)	/* I - Users */
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		*dest,		/* Pointer into job-printer-uri */
		*username;	/* Pointer to job-originating-user-name */
  int		jobid,		/* job-id */
		size;		/* job-k-octets */
  char		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */


  DEBUG_printf(("show_jobs(%08x, %08x, %08x)\n", http, dests, users));

  if (http == NULL)
    return;

 /*
  * Build a IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
                      NULL, "http://localhost/jobs/");

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/jobs/")) != NULL)
  {
   /*
    * Loop through the job list and display them...
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
      * Pull the needed attributes from this job...
      */

      jobid    = 0;
      size     = 0;
      username = NULL;
      dest     = NULL;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer * 1024;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  username = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (dest == NULL || jobid == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a job we're interested in...
      */

      match = dests == NULL && users == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = dest;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

      if (users != NULL && username != NULL)
      {
        for (dptr = users; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = username;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the job...
      */

      if (match)
        printf("%s-%d %s %d\n", dest, jobid, username ? username : "unknown",
	       size);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * 'show_printers()' - Show printers.
 */

static void
show_printers(http_t *http,	/* I - HTTP connection to server */
              char   *dests)	/* I - Destinations */
{
  ipp_t		*request,	/* IPP Request */
		*response,	/* IPP Response */
		*jobs;		/* IPP Get Jobs response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		*printer,	/* Printer name */
		*message;	/* Printer state message */
  ipp_pstate_t	pstate;		/* Printer state */
  int		jobid;		/* Job ID of current job */
  char		*dptr,		/* Pointer into destination list */
		*ptr;		/* Pointer into printer name */
  int		match;		/* Non-zero if this job matches */
  char		printer_uri[HTTP_MAX_URI];
				/* Printer URI */


  DEBUG_printf(("show_printers(%08x, %08x)\n", http, dests));

  if (http == NULL)
    return;

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/printers/")) != NULL)
  {
    DEBUG_puts("show_printers: request succeeded...");

   /*
    * Loop through the printers returned in the list and display
    * their status...
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
      * Pull the needed attributes from this job...
      */

      printer = NULL;
      pstate  = IPP_PRINTER_IDLE;
      message = NULL;
      jobid   = 0;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  printer = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  pstate = (ipp_pstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-state-message") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  message = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (printer == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * See if this is a printer we're interested in...
      */

      match = dests == NULL;

      if (dests != NULL)
      {
        for (dptr = dests; *dptr != '\0';)
	{
	 /*
	  * Skip leading whitespace and commas...
	  */

	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;

         /*
	  * Compare names...
	  */

	  for (ptr = printer;
	       *ptr != '\0' && *dptr != '\0' && *ptr == *dptr;
	       ptr ++, dptr ++);

          if (*ptr == '\0' && (*dptr == '\0' || *dptr == ',' || isspace(*dptr)))
	  {
	    match = 1;
	    break;
	  }

         /*
	  * Skip trailing junk...
	  */

          while (!isspace(*dptr) && *dptr != '\0')
	    dptr ++;
	  while (isspace(*dptr) || *dptr == ',')
	    dptr ++;

	  if (*dptr == '\0')
	    break;
        }
      }

     /*
      * Display the printer entry if needed...
      */

      if (match)
      {
       /*
        * If the printer state is "IPP_PRINTER_PROCESSING", then grab the
	* current job for the printer.
	*/

        if (pstate == IPP_PRINTER_PROCESSING)
	{
	 /*
	  * Build an IPP_GET_JOBS request, which requires the following
	  * attributes:
	  *
	  *    attributes-charset
	  *    attributes-natural-language
	  *    printer-uri
	  *    limit
	  */

	  request = ippNew();

	  request->request.op.operation_id = IPP_GET_JOBS;
	  request->request.op.request_id   = 1;

	  language = cupsLangDefault();

	  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                	      "attributes-charset", NULL,
			      cupsLangEncoding(language));

	  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                	      "attributes-natural-language", NULL,
			      language->language);

          sprintf(printer_uri, "http://localhost/printers/%s", printer);
	  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	                      "printer-uri", NULL, printer_uri);

	  attr = ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
	                       "limit", 1);

          if ((jobs = cupsDoRequest(http, request, "/jobs/")) != NULL)
	  {
	    if ((attr = ippFindAttribute(jobs, "job-id", IPP_TAG_INTEGER)) != NULL)
              jobid = attr->values[0].integer;

            ippDelete(jobs);
	  }
        }

       /*
        * Display it...
	*/

        switch (pstate)
	{
	  case IPP_PRINTER_IDLE :
	      printf("printer %s is idle.\n", printer);
	      break;
	  case IPP_PRINTER_PROCESSING :
	      printf("printer %s now printing %s-%d.\n", printer, printer, jobid);
	      break;
	  case IPP_PRINTER_STOPPED :
	      printf("printer %s disabled -\n\t%s\n", printer,
	             message == NULL ? "reason unknown" : message);
	      break;
	}
      }

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
}


/*
 * 'show_scheduler()' - Show scheduler status.
 */

static void
show_scheduler(http_t *http)	/* I - HTTP connection to server */
{
  printf("scheduler is %srunning\n", http == NULL ? "not " : "");
}


/*
 * End of "$Id: lpstat.c,v 1.6 1999/04/21 14:16:29 mike Exp $".
 */
