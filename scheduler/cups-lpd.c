/*
 * "$Id: cups-lpd.c,v 1.2 2000/05/11 14:11:54 mike Exp $"
 *
 *   Line Printer Daemon interface for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *   main()             - Process an incoming LPD request...
 *   recv_print_job()   - Receive a print job from the client.
 *   send_short_state() - Send the short queue state.
 *   remove_jobs()      - Cancel one or more jobs.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/string.h>
#include <cups/language.h>
#include <stdlib.h>
#include <ctype.h>


/*
 * LPD "mini-daemon" for CUPS.  This program must be used in conjunction
 * with inetd or another similar program that monitors ports and starts
 * daemons for each client connection.  A typical configuration is:
 *
 *    printer stream tcp nowait lp /usr/lib/cups/daemon/cups-lpd cups-lpd
 *
 * This daemon implements most of RFC 1179 (the unofficial LPD specification)
 * except for:
 *
 *     - This daemon does not check to make sure that the source port is
 *       between 721 and 731, since it isn't necessary for proper
 *       functioning and port-based security is no security at all!
 *
 *     - The "Print any waiting jobs" command is a no-op.
 *
 * The LPD-to-IPP mapping and report formats are as defined in RFC 2569.
 */

/*
 * Prototypes...
 */

int	recv_print_job(const char *dest);
int	send_state(const char *dest, const char *list, int longstatus);
int	remove_jobs(const char *dest, const char *agent, const char *list);


/*
 * 'main()' - Process an incoming LPD request...
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  char	line[1024],	/* Command string */
	command,	/* Command code */
	*dest,		/* Pointer to destination */
	*list,		/* Pointer to list */
	*agent,		/* Pointer to user */
	status;		/* Status for client */


 /*
  * Don't buffer the output...
  */

  setbuf(stdout, NULL);

 /*
  * RFC1179 specifies that only 1 daemon command can be received for
  * every connection.
  */

  if (fgets(line, sizeof(line), stdin) == NULL)
  {
   /*
    * Unable to get command from client!  Send an error status and return.
    */

    putchar(1);
    return (1);
  }

 /*
  * The first byte is the command byte.  After that will be the queue name,
  * resource list, and/or user name.
  */

  command = line[0];
  dest    = line + 1;

  for (list = dest + 1; *list && !isspace(*list); list ++);

  while (isspace(*list))
    *list++ = '\0';

 /*
  * Do the command...
  */

  switch (command)
  {
    default : /* Unknown command */
	putchar(1);

        status = 1;
	break;

    case 0x01 : /* Print any waiting jobs */
	putchar(0);

        status = 0;
	break;

    case 0x02 : /* Receive a printer job */
	putchar(0);

        status = recv_print_job(dest);
	break;

    case 0x03 : /* Send queue state (short) */
	putchar(0);

        status = send_state(dest, list, 0);
	break;

    case 0x04 : /* Send queue state (long) */
	putchar(0);

        status = send_state(dest, list, 1);
	break;

    case 0x05 : /* Remove jobs */
	putchar(0);

       /*
        * Grab the agent and skip to the list of users and/or jobs.
	*/

        agent = list;

	for (; *list && !isspace(*list); list ++);
	while (isspace(*list))
	  *list++ = '\0';

        status = remove_jobs(dest, agent, list);
	break;
  }

  return (status);
}


/*
 * 'recv_print_job()' - Receive a print job from the client.
 */

int					/* O - Command status */
recv_print_job(const char *dest)	/* I - Destination */
{
  int		i;			/* Looping var */
  int		status;			/* Command status */
  FILE		*fp;			/* Temporary file */
  char		filename[1024];		/* Filename */
  int		bytes;			/* Bytes received */
  char		line[1024],		/* Line from file/stdin */
		command,		/* Command from line */
		*count,			/* Number of bytes */
		*name;			/* Name of file */
  int		num_data;		/* Number of data files */
  char		data[32][256];		/* Data files */
  const char	*tmpdir;		/* Temporary directory */
  char		user[1024],		/* User name */
		title[1024],		/* Job title */
		docname[1024];		/* Document name */
  http_t	*http;			/* HTTP server connection */
  cups_lang_t	*language;		/* Language information */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */


  status   = 0;
  num_data = 0;
  if ((tmpdir = getenv("TMP")) == NULL)
    tmpdir = "/var/tmp";

  while (fgets(line, sizeof(line), stdin) != NULL)
  {
    if (strlen(line) < 2)
    {
      status = 1;
      break;
    }

    command = line[0];
    count   = line + 1;

    for (name = count + 1; *name && !isspace(*name); name ++);
    while (isspace(*name))
      *name++ = '\0';

    switch (command)
    {
      default :
      case 0x01 : /* Abort */
          status = 1;
	  break;
      case 0x02 : /* Receive control file */
          if (strlen(name) < 2)
	  {
	    putchar(1);
	    status = 1;
	    break;
	  }

          snprintf(filename, sizeof(filename), "%s/%06d-0", tmpdir, getpid());
	  break;
      case 0x03 : /* Receive data file */
          if (strlen(name) < 2)
	  {
	    putchar(1);
	    status = 1;
	    break;
	  }

          name[strlen(name) - 1] = '\0'; /* Strip LF */
	  strncpy(data[num_data], name, sizeof(data[0]) - 1);
	  data[num_data][sizeof(data[0]) - 1] = '\0';

          num_data ++;
          snprintf(filename, sizeof(filename), "%s/%06d-%d", tmpdir, getpid(),
	           num_data);
	  break;
    }

    putchar(status);

    if (status)
      break;

   /*
    * Try opening the temp file...
    */

    if ((fp = fopen(filename, "wb")) == NULL)
    {
      putchar(1);
      status = 1;
      break;
    }

   /*
    * Copy the data or control file from the client...
    */

    for (i = atoi(count); i > 0; i -= bytes)
    {
      if (i > sizeof(line))
        bytes = sizeof(line);
      else
        bytes = i;

      if ((bytes = fread(line, 1, bytes, stdin)) > 0)
        bytes = fwrite(line, 1, bytes, fp);

      if (bytes < 1)
      {
        status = 1;
	break;
      }
    }

   /*
    * Read trailing nul...
    */

    if (!status)
    {
      fread(line, 1, 1, stdin);
      status = line[0];
    }

   /*
    * Close the file and send an acknowledgement...
    */

    fclose(fp);

    putchar(status);

    if (status)
      break;
  }

  if (!status)
  {
   /*
    * Process the control file and print stuff...
    */

    snprintf(filename, sizeof(filename), "%s/%06d-0", tmpdir, getpid());
    if ((fp = fopen(filename, "rb")) == NULL)
      status = 1;
    else if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
      status = 1;
    else
    {
      language    = cupsLangDefault();
      title[0]    = '\0';
      user[0]     = '\0';
      docname[0]  = '\0';

      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Strip the trailing newline...
	*/

        if (line[0])
	  line[strlen(line) - 1] = '\0';

       /*
        * Process control lines...
	*/

	switch (line[0])
	{
	  case 'J' : /* Job name */
	      strcpy(title, line + 1);
	      break;
	  case 'N' : /* Document name */
	      strcpy(docname, line + 1);
	      break;
	  case 'P' : /* User identification */
	      strcpy(user, line + 1);
	      break;
	  case 'c' : /* Plot CIF file */
	  case 'd' : /* Print DVI file */
	  case 'f' : /* Print formatted file */
	  case 'g' : /* Plot file */
	  case 'l' : /* Print file leaving control characters (raw) */
	  case 'n' : /* Print ditroff output file */
	  case 'o' : /* Print PostScript output file */
	  case 'p' : /* Print file with 'pr' format (prettyprint) */
	  case 'r' : /* File to print with FORTRAN carriage control */
	  case 't' : /* Print troff output file */
	  case 'v' : /* Print raster file */
	     /*
	      * Verify that we have a username...
	      */

	      if (!user[0])
	      {
	        status = 1;
		break;
	      }

             /*
	      * Figure out which file we are printing...
	      */

	      for (i = 0; i < num_data; i ++)
	        if (strcmp(data[i], line + 1) == 0)
		  break;

              if (i >= num_data)
	      {
	        status = 1;
		break;
	      }

             /*
	      * Build an IPP_PRINT_JOB request...
	      */

              if ((request = ippNew()) == NULL)
	      {
	        status = 1;
		break;
	      }

	      request->request.op.operation_id = IPP_PRINT_JOB;
              request->request.op.request_id   = 1;

              snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s",
	               cupsServer(), ippPort(), dest);

	      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        		   "attributes-charset", NULL,
			   cupsLangEncoding(language));

	      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        		   "attributes-natural-language", NULL,
        		   language != NULL ? language->language : "C");

	      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	                   "printer-uri", NULL, uri);

	      if (line[0] == 'l')
		ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
		             "document-format", NULL, "application/vnd.cups-raw");
	      else
		ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
		             "document-format", NULL, "application/octet-stream");

	      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	                   "requesting-user-name", NULL, user);

	      if (title[0])
		ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		             "job-name", NULL, title);

	      if (docname[0])
		ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		             "document-name", NULL, docname);

              if (line[0] == 'p')
	        ippAddBoolean(request, IPP_TAG_JOB, "prettyprint", 1);

              snprintf(filename, sizeof(filename), "%s/%06d-%d", tmpdir,
	               getpid(), i + 1);

             /*
	      * Do the request...
	      */

	      snprintf(uri, sizeof(uri), "/printers/%s", dest);

	      if ((response = cupsDoFileRequest(http, request, uri, filename)) == NULL)
	        status = 1;
	      else if (response->request.status.status_code > IPP_OK_CONFLICT)
	        status = 1;

              if (response != NULL)
	        ippDelete(response);
	      break;
	}

	if (status)
	  break;
      }

      fclose(fp);
      httpClose(http);
      cupsLangFree(language);
    }
  }

 /*
  * Clean up all temporary files and return...
  */

  for (i = -1; i < num_data; i ++)
  {
    snprintf(filename, sizeof(filename), "%s/%06d-%d", tmpdir, getpid(), i + 1);
    unlink(filename);
  }

  return (status);
}


/*
 * 'send_short_state()' - Send the short queue state.
 */

int					/* O - Command status */
send_state(const char *dest,		/* I - Destination */
           const char *list,		/* I - Job or user */
	   int        longstatus)	/* I - List of jobs or users */
{
  int		id;			/* Job ID from list */
  http_t	*http;			/* HTTP server connection */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  ipp_pstate_t	state;			/* Printer state */
  const char	*jobdest,		/* Pointer into job-printer-uri */
		*jobuser,		/* Pointer to job-originating-user-name */
		*jobname;		/* Pointer to job-name */
  ipp_jstate_t	jobstate;		/* job-state */
  int		jobid,			/* job-id */
		jobsize,		/* job-k-octets */
		jobcount,		/* Number of jobs */
		jobcopies,		/* Number of copies */
		rank;			/* Rank of job */
  char		rankstr[255];		/* Rank string */
  char		namestr[1024];		/* Job name string */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  static const char *ranks[10] =	/* Ranking strings */
		{
		  "th",
		  "st",
		  "nd",
		  "rd",
		  "th",
		  "th",
		  "th",
		  "th",
		  "th",
		  "th"
		};


 /*
  * Try connecting to the local server...
  */

  if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
    return (1);

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  sprintf(uri, "ipp://localhost/printers/%s", dest);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      printf("get-printer-attributes failed: %s\n",
             ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (1);
    }

    if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL)
      state = (ipp_pstate_t)attr->values[0].integer;
    else
      state = IPP_PRINTER_STOPPED;

    switch (state)
    {
      case IPP_PRINTER_IDLE :
          printf("%s is ready\n", dest);
	  break;
      case IPP_PRINTER_PROCESSING :
          printf("%s is ready and printing\n", dest);
	  break;
      case IPP_PRINTER_STOPPED :
          printf("%s is not ready\n", dest);
	  break;
    }

    ippDelete(response);
  }
  else
  {
    printf("get-printer-attributes failed: %s\n", ippErrorString(cupsLastError()));
    return (1);
  }

 /*
  * Build an IPP_GET_JOBS or IPP_GET_JOB_ATTRIBUTES request, which requires
  * the following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri
  */

  id = atoi(list);

  request = ippNew();

  request->request.op.operation_id = id ? IPP_GET_JOB_ATTRIBUTES : IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", dest);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  if (id)
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", id);
  else
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, list);
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
  }

 /*
  * Do the request and get back a response...
  */

  jobcount = 0;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      printf("get-jobs failed: %s\n",
             ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (1);
    }

    rank = 1;

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

      jobid       = 0;
      jobsize     = 0;
      jobstate    = IPP_JOB_PENDING;
      jobname     = "untitled";
      jobuser     = NULL;
      jobdest     = NULL;
      jobcopies   = 1;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobid = attr->values[0].integer;

        if (strcmp(attr->name, "job-k-octets") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobsize = attr->values[0].integer * 1024;

        if (strcmp(attr->name, "job-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  jobstate = (ipp_jstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "job-printer-uri") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  if ((jobdest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    jobdest ++;

        if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  jobuser = attr->values[0].string.text;

        if (strcmp(attr->name, "job-name") == 0 &&
	    attr->value_tag == IPP_TAG_NAME)
	  jobname = attr->values[0].string.text;

        if (strcmp(attr->name, "copies") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  jobcopies = attr->values[0].integer;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (jobdest == NULL || jobid == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

      if (!longstatus && jobcount == 0)
	puts("Rank   Owner      Job             Files                       Total Size");

      jobcount ++;

     /*
      * Display the job...
      */

      if (jobstate == IPP_JOB_PROCESSING)
	strcpy(rankstr, "active");
      else
      {
	sprintf(rankstr, "%d%s", rank, ranks[rank % 10]);
	rank ++;
      }

      if (longstatus)
      {
        puts("");

        if (jobcopies > 1)
	  snprintf(namestr, sizeof(namestr), "%d copies of %s", jobcopies,
	           jobname);
	else
	{
	  strncpy(namestr, jobname, sizeof(namestr) - 1);
	  namestr[sizeof(namestr) - 1] = '\0';
	}

        printf("%s: %-31s [job %d localhost]\n", jobuser, rankstr, jobid);
        printf("        %-31.31s %d bytes\n", namestr, jobsize);
      }
      else
        printf("%-6s %-10.10s %-15d %-27.27s %d bytes\n", rankstr, jobuser,
	       jobid, jobname, jobsize);

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    printf("get-jobs failed: %s\n", ippErrorString(cupsLastError()));
    return (1);
  }

  if (jobcount == 0)
    puts("no entries");

  cupsLangFree(language);
  httpClose(http);

  return (0);
}


/*
 * 'remove_jobs()' - Cancel one or more jobs.
 */

int					/* O - Command status */
remove_jobs(const char *dest,		/* I - Destination */
            const char *agent,		/* I - User agent */
	    const char *list)		/* I - List of jobs or users */
{
  int		id;			/* Job ID */
  http_t	*http;			/* HTTP server connection */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		uri[HTTP_MAX_URI];	/* Job URI */


 /*
  * Try connecting to the local server...
  */

  if ((http = httpConnect(cupsServer(), ippPort())) == NULL)
    return (1);

  language = cupsLangDefault();

 /*
  * Loop for each job...
  */

  while ((id = atoi(list)) > 0)
  {
   /*
    * Skip job ID in list...
    */

    while (isdigit(*list))
      list ++;
    while (isspace(*list))
      list ++;

   /*
    * Build an IPP_CANCEL_JOB request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    job-uri
    *    requesting-user-name
    */

    request = ippNew();

    request->request.op.operation_id = IPP_CANCEL_JOB;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    sprintf(uri, "ipp://localhost/jobs/%d", id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, agent);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
    {
      if (response->request.status.status_code > IPP_OK_CONFLICT)
      {
	printf("cancel-job failed: %s\n",
               ippErrorString(response->request.status.status_code));
	ippDelete(response);
	cupsLangFree(language);
	httpClose(http);
	return (1);
      }

      ippDelete(response);
    }
    else
    {
      printf("cancel-job failed: %s\n", ippErrorString(cupsLastError()));
      cupsLangFree(language);
      httpClose(http);
      return (1);
    }
  }

  cupsLangFree(language);
  httpClose(http);

  return (0);
}


/*
 * End of "$Id: cups-lpd.c,v 1.2 2000/05/11 14:11:54 mike Exp $".
 */
