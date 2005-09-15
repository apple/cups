/*
 * "$Id$"
 *
 *   IPP backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()                 - Send a file to the printer or server.
 *   check_printer_state()  - Check the printer state...
 *   password_cb()          - Disable the password prompt for
 *                            cupsDoFileRequest().
 *   report_printer_state() - Report the printer state.
 *   run_pictwps_filter()   - Convert PICT files to PostScript when printing
 *                            remotely.
 *   sigterm_handler()      - Handle 'terminate' signals that stop the backend.
 */

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cups/http-private.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>
#include <signal.h>
#include <sys/wait.h>


/*
 * Globals...
 */

static char	*password = NULL;	/* Password for device URI */
#ifdef __APPLE__
static char	pstmpname[1024] = "";	/* Temporary PostScript file name */
#endif /* __APPLE__ */
static char	tmpfilename[1024] = "";	/* Temporary spool file name */


/*
 * Local functions...
 */

void		check_printer_state(http_t *http, cups_lang_t *language,
				    const char *charset, const char *uri,	/* I - Printer URI */
		                    const char *resource, const char *user,
				    int version);
const char	*password_cb(const char *);
int		report_printer_state(ipp_t *ipp);

#ifdef __APPLE__
int		run_pictwps_filter(char **argv, const char *filename);
#endif /* __APPLE__ */
static void	sigterm_handler(int sig);


/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments (6 or 7) */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		num_options;		/* Number of printer options */
  cups_option_t	*options;		/* Printer options */
  char		method[255],		/* Method in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info */
		resource[1024],		/* Resource info (printer name) */
		*optptr,		/* Pointer to URI options */
		name[255],		/* Name of option */
		value[255],		/* Value of option */
		*ptr;			/* Pointer into name or value */
  char		*filename;		/* File to print */
  int		port;			/* Port number (not used) */
  char		uri[HTTP_MAX_URI];	/* Updated URI without user/pass */
  ipp_status_t	ipp_status;		/* Status of IPP request */
  http_t	*http;			/* HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response,		/* IPP response */
		*supported;		/* get-printer-attributes response */
  int		waitjob,		/* Wait for job complete? */
		waitprinter;		/* Wait for printer ready? */
  ipp_attribute_t *job_id_attr;		/* job-id attribute */
  int		job_id;			/* job-id value */
  ipp_attribute_t *job_sheets;		/* job-media-sheets-completed attribute */
  ipp_attribute_t *job_state;		/* job-state attribute */
  ipp_attribute_t *copies_sup;		/* copies-supported attribute */
  ipp_attribute_t *charset_sup;		/* charset-supported attribute */
  ipp_attribute_t *format_sup;		/* document-format-supported attribute */
  ipp_attribute_t *printer_state;	/* printer-state attribute */
  ipp_attribute_t *printer_accepting;	/* printer-is-accepting-jobs attribute */
  const char	*charset;		/* Character set to use */
  cups_lang_t	*language;		/* Default language */
  int		copies;			/* Number of copies remaining */
  const char	*content_type;		/* CONTENT_TYPE environment variable */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		version;		/* IPP version */
  int		reasons;		/* Number of printer-state-reasons shown */
  static const char * const pattrs[] =
		{			/* Printer attributes we want */
		  "copies-supported",
		  "charset-supported",
		  "document-format-supported",
		  "printer-is-accepting-jobs",
		  "printer-state",
		  "printer-state-reasons",
		};
  static const char * const jattrs[] =
		{			/* Job attributes we want */
		  "job-media-sheets-completed",
		  "job-state"
		};


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Ignore SIGPIPE and catch SIGTERM signals...
  */

#ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);
  sigset(SIGTERM, sigterm_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM);
  action.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, sigterm_handler);
#endif /* HAVE_SIGSET */

 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    char *s;

    if ((s = strrchr(argv[0], '/')) != NULL)
      s ++;
    else
      s = argv[0];

    printf("network %s \"Unknown\" \"Internet Printing Protocol (%s)\"\n", s, s);
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * Get the content type...
  */

  if (argc > 6)
    content_type = getenv("CONTENT_TYPE");
  else
    content_type = "application/vnd.cups-raw";

  if (content_type == NULL)
    content_type = "application/octet-stream";

 /*
  * Extract the hostname and printer name from the URI...
  */

  if (getenv("DEVICE_URI") != NULL)
    /* authentication information is only available in the env var */
    httpSeparate(getenv("DEVICE_URI"), method, username, hostname, &port,
                 resource);
  else if (strchr(argv[0], ':') != NULL)
    httpSeparate(argv[0], method, username, hostname, &port, resource);
  else
  {
    fputs("ERROR: Missing device URI on command-line and no DEVICE_URI environment variable!\n",
          stderr);
    return (1);
  }

  if (!strcmp(method, "https"))
    cupsSetEncryption(HTTP_ENCRYPT_ALWAYS);

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, copy stdin to a temporary file and print the temporary
  * file.
  */

  if (argc == 6)
  {
   /*
    * Copy stdin to a temporary file...
    */

    int  fd;		/* Temporary file */
    char buffer[8192];	/* Buffer for copying */
    int  bytes;		/* Number of bytes read */


    if ((fd = cupsTempFd(tmpfilename, sizeof(tmpfilename))) < 0)
    {
      perror("ERROR: unable to create temporary file");
      return (1);
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (write(fd, buffer, bytes) < bytes)
      {
        perror("ERROR: unable to write to temporary file");
	close(fd);
	unlink(tmpfilename);
	return (1);
      }

    close(fd);
    filename = tmpfilename;
  }
  else
    filename = argv[6];

 /*
  * See if there are any options...
  */

  version     = 1;
  waitjob     = 1;
  waitprinter = 1;

  if ((optptr = strchr(resource, '?')) != NULL)
  {
   /*
    * Yup, terminate the device name string and move to the first
    * character of the optptr...
    */

    *optptr++ = '\0';

   /*
    * Then parse the optptr...
    */

    while (*optptr)
    {
     /*
      * Get the name...
      */

      for (ptr = name; *optptr && *optptr != '=';)
        if (ptr < (name + sizeof(name) - 1))
          *ptr++ = *optptr++;
      *ptr = '\0';

      if (*optptr == '=')
      {
       /*
        * Get the value...
	*/

        optptr ++;

	for (ptr = value; *optptr && *optptr != '+' && *optptr != '&';)
          if (ptr < (value + sizeof(value) - 1))
            *ptr++ = *optptr++;
	*ptr = '\0';

	if (*optptr == '+')
	  optptr ++;
      }
      else
        value[0] = '\0';

     /*
      * Process the option...
      */

      if (!strcasecmp(name, "waitjob"))
      {
       /*
        * Wait for job completion?
	*/

        waitjob = !strcasecmp(value, "on") ||
	          !strcasecmp(value, "yes") ||
	          !strcasecmp(value, "true");
      }
      else if (!strcasecmp(name, "waitprinter"))
      {
       /*
        * Wait for printer idle?
	*/

        waitprinter = !strcasecmp(value, "on") ||
	              !strcasecmp(value, "yes") ||
	              !strcasecmp(value, "true");
      }
      else if (!strcasecmp(name, "encryption"))
      {
       /*
        * Enable/disable encryption?
	*/

        if (!strcasecmp(value, "always"))
	  cupsSetEncryption(HTTP_ENCRYPT_ALWAYS);
        else if (!strcasecmp(value, "required"))
	  cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
        else if (!strcasecmp(value, "never"))
	  cupsSetEncryption(HTTP_ENCRYPT_NEVER);
        else if (!strcasecmp(value, "ifrequested"))
	  cupsSetEncryption(HTTP_ENCRYPT_IF_REQUESTED);
	else
	{
	  fprintf(stderr, "ERROR: Unknown encryption option value \"%s\"!\n",
	          value);
        }
      }
      else if (!strcasecmp(name, "version"))
      {
        if (!strcmp(value, "1.0"))
	  version = 0;
	else if (!strcmp(value, "1.1"))
	  version = 1;
	else
	{
	  fprintf(stderr, "ERROR: Unknown version option value \"%s\"!\n",
	          value);
	}
      }
      else
      {
       /*
        * Unknown option...
	*/

	fprintf(stderr, "ERROR: Unknown option \"%s\" with value \"%s\"!\n",
	        name, value);
      }
    }
  }

 /*
  * Set the authentication info, if any...
  */

  cupsSetPasswordCB(password_cb);

  if (username[0])
  {
    if ((password = strchr(username, ':')) != NULL)
      *password++ = '\0';

    cupsSetUser(username);
  }

 /*
  * Try connecting to the remote server...
  */

  do
  {
    fprintf(stderr, "INFO: Connecting to %s on port %d...\n", hostname, port);

    if ((http = httpConnectEncrypt(hostname, port, cupsEncryption())) == NULL)
    {
      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        fprintf(stderr, "INFO: Unable to connect to %s, queuing on next printer in class...\n",
	        hostname);

        if (argc == 6 || strcmp(filename, argv[6]))
	  unlink(filename);

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (1);
      }

      if (errno == ECONNREFUSED || errno == EHOSTDOWN ||
          errno == EHOSTUNREACH)
      {
	fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in 30 seconds...\n",
                hostname);
	sleep(30);
      }
      else if (h_errno)
      {
        fprintf(stderr, "INFO: Unable to lookup host \'%s\' - %s\n",
	        hostname, hstrerror(h_errno));
	sleep(30);
      }
      else
      {
	perror("ERROR: Unable to connect to IPP host");
	sleep(30);
      }
    }
  }
  while (http == NULL);

  fprintf(stderr, "INFO: Connected to %s...\n", hostname);

 /*
  * Build a URI for the printer and fill the standard IPP attributes for
  * an IPP_PRINT_FILE request.  We can't use the URI in argv[0] because it
  * might contain username:password information...
  */

  snprintf(uri, sizeof(uri), "%s://%s:%d%s", method, hostname, port, resource);

 /*
  * First validate the destination and see if the device supports multiple
  * copies.  We have to do this because some IPP servers (e.g. HP JetDirect)
  * don't support the copies attribute...
  */

  language    = cupsLangDefault();
  charset_sup = NULL;
  copies_sup  = NULL;
  format_sup  = NULL;
  supported   = NULL;

  do
  {
   /*
    * Build the IPP request...
    */

    request = ippNew();
    request->request.op.version[1]   = version;
    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, "utf-8");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL,
        	 language != NULL ? language->language : "en");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		  NULL, pattrs);

   /*
    * Do the request...
    */

    fputs("DEBUG: Getting supported attributes...\n", stderr);

    if ((supported = cupsDoRequest(http, request, resource)) == NULL)
      ipp_status = cupsLastError();
    else
      ipp_status = supported->request.status.status_code;

    if (ipp_status > IPP_OK_CONFLICT)
    {
      if (ipp_status == IPP_PRINTER_BUSY ||
	  ipp_status == IPP_SERVICE_UNAVAILABLE)
      {
	fputs("INFO: Printer busy; will retry in 10 seconds...\n", stderr);
        report_printer_state(supported);
	sleep(10);
      }
      else if ((ipp_status == IPP_BAD_REQUEST ||
	        ipp_status == IPP_VERSION_NOT_SUPPORTED) && version == 1)
      {
       /*
	* Switch to IPP/1.0...
	*/

	fputs("INFO: Printer does not support IPP/1.1, trying IPP/1.0...\n", stderr);
	version = 0;
	httpReconnect(http);
      }
      else if (ipp_status == IPP_NOT_FOUND)
      {
        fputs("ERROR: Destination printer does not exist!\n", stderr);

	if (supported)
          ippDelete(supported);

	return (1);
      }
      else
      {
	fprintf(stderr, "ERROR: Unable to get printer status (%s)!\n",
	        ippErrorString(ipp_status));
        sleep(10);
      }

      if (supported)
        ippDelete(supported);

      continue;
    }
    else if ((copies_sup = ippFindAttribute(supported, "copies-supported",
	                                    IPP_TAG_RANGE)) != NULL)
    {
     /*
      * Has the "copies-supported" attribute - does it have an upper
      * bound > 1?
      */

      if (copies_sup->values[0].range.upper <= 1)
	copies_sup = NULL; /* No */
    }

    charset_sup = ippFindAttribute(supported, "charset-supported",
	                           IPP_TAG_CHARSET);
    format_sup  = ippFindAttribute(supported, "document-format-supported",
	                           IPP_TAG_MIMETYPE);

    if (format_sup)
    {
      fprintf(stderr, "DEBUG: document-format-supported (%d values)\n",
	      format_sup->num_values);
      for (i = 0; i < format_sup->num_values; i ++)
	fprintf(stderr, "DEBUG: [%d] = \"%s\"\n", i,
	        format_sup->values[i].string.text);
    }

    report_printer_state(supported);
  }
  while (ipp_status > IPP_OK_CONFLICT);

 /*
  * See if the printer is accepting jobs and is not stopped; if either
  * condition is true and we are printing to a class, requeue the job...
  */

  if (getenv("CLASS") != NULL)
  {
    printer_state     = ippFindAttribute(supported, "printer-state",
                                	 IPP_TAG_ENUM);
    printer_accepting = ippFindAttribute(supported, "printer-is-accepting-jobs",
                                	 IPP_TAG_BOOLEAN);

    if (printer_state == NULL ||
	(printer_state->values[0].integer > IPP_PRINTER_PROCESSING && waitprinter) ||
	printer_accepting == NULL ||
	!printer_accepting->values[0].boolean)
    {
     /*
      * If the CLASS environment variable is set, the job was submitted
      * to a class and not to a specific queue.  In this case, we want
      * to abort immediately so that the job can be requeued on the next
      * available printer in the class.
      */

      fprintf(stderr, "INFO: Unable to queue job on %s, queuing on next printer in class...\n",
	      hostname);

      ippDelete(supported);
      httpClose(http);

      if (argc == 6 || strcmp(filename, argv[6]))
	unlink(filename);

     /*
      * Sleep 5 seconds to keep the job from requeuing too rapidly...
      */

      sleep(5);

      return (1);
    }
  }

 /*
  * See if the printer supports multiple copies...
  */

  if (copies_sup || argc < 7)
    copies = 1;
  else
    copies = atoi(argv[4]);

 /*
  * Figure out the character set to use...
  */

  charset = language ? cupsLangEncoding(language) : "us-ascii";

  if (charset_sup)
  {
   /*
    * See if IPP server supports the requested character set...
    */

    for (i = 0; i < charset_sup->num_values; i ++)
      if (strcasecmp(charset, charset_sup->values[i].string.text) == 0)
        break;

   /*
    * If not, choose us-ascii or utf-8...
    */

    if (i >= charset_sup->num_values)
    {
     /*
      * See if us-ascii is supported...
      */

      for (i = 0; i < charset_sup->num_values; i ++)
        if (strcasecmp("us-ascii", charset_sup->values[i].string.text) == 0)
          break;

      if (i < charset_sup->num_values)
        charset = "us-ascii";
      else
        charset = "utf-8";
    }
  }

 /*
  * Then issue the print-job request...
  */

  reasons = 0;

  while (copies > 0)
  {
   /*
    * Build the IPP request...
    */

    request = ippNew();
    request->request.op.version[1]   = version;
    request->request.op.operation_id = IPP_PRINT_JOB;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, charset);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL,
        	 language != NULL ? language->language : "en");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    fprintf(stderr, "DEBUG: printer-uri = \"%s\"\n", uri);

    if (argv[2][0])
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, argv[2]);

    fprintf(stderr, "DEBUG: requesting-user-name = \"%s\"\n", argv[2]);

    if (argv[3][0])
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
        	   argv[3]);

    fprintf(stderr, "DEBUG: job-name = \"%s\"\n", argv[3]);

   /*
    * Handle options on the command-line...
    */

    options     = NULL;
    num_options = cupsParseOptions(argv[5], 0, &options);

#ifdef __APPLE__
    if (content_type != NULL && strcasecmp(content_type, "application/pictwps") == 0)
    {
      if (format_sup != NULL)
      {
	for (i = 0; i < format_sup->num_values; i ++)
	  if (strcasecmp(content_type, format_sup->values[i].string.text) == 0)
	    break;
      }

      if (format_sup == NULL || i >= format_sup->num_values)
      {
       /*
	* Remote doesn't support "application/pictwps" (i.e. it's not MacOS X)
	* so convert the document to PostScript...
	*/

	if (run_pictwps_filter(argv, filename))
	  return (1);

        filename = pstmpname;

       /*
	* Change the MIME type to application/postscript...
	*/

	content_type = "application/postscript";
      }
    }
#endif /* __APPLE__ */

    if (content_type != NULL && format_sup != NULL)
    {
      for (i = 0; i < format_sup->num_values; i ++)
        if (strcasecmp(content_type, format_sup->values[i].string.text) == 0)
          break;

      if (i < format_sup->num_values)
        num_options = cupsAddOption("document-format", content_type,
	                            num_options, &options);
    }

    if (copies_sup)
    {
     /*
      * Only send options if the destination printer supports the copies
      * attribute.  This is a hack for the HP JetDirect implementation of
      * IPP, which does not accept extension attributes and incorrectly
      * reports a client-error-bad-request error instead of the
      * successful-ok-unsupported-attributes status.  In short, at least
      * some HP implementations of IPP are non-compliant.
      */

      cupsEncodeOptions(request, num_options, options);
      ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies",
                    atoi(argv[4]));
    }

    cupsFreeOptions(num_options, options);

   /*
    * If copies aren't supported, then we are likely dealing with an HP
    * JetDirect.  The HP IPP implementation seems to close the connection
    * after every request (that is, it does *not* implement HTTP Keep-
    * Alive, which is REQUIRED by HTTP/1.1...
    */

    if (!copies_sup)
      httpReconnect(http);

   /*
    * Do the request...
    */

    if ((response = cupsDoFileRequest(http, request, resource, filename)) == NULL)
      ipp_status = cupsLastError();
    else
      ipp_status = response->request.status.status_code;

    if (ipp_status > IPP_OK_CONFLICT)
    {
      job_id = 0;

      if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	  ipp_status == IPP_PRINTER_BUSY)
      {
	fputs("INFO: Printer is busy; retrying print job...\n", stderr);
	sleep(10);
      }
      else
        fprintf(stderr, "ERROR: Print file was not accepted (%s)!\n",
	        ippErrorString(ipp_status));
    }
    else if ((job_id_attr = ippFindAttribute(response, "job-id",
                                             IPP_TAG_INTEGER)) == NULL)
    {
      fputs("NOTICE: Print file accepted - job ID unknown.\n", stderr);
      job_id = 0;
    }
    else
    {
      job_id = job_id_attr->values[0].integer;
      fprintf(stderr, "NOTICE: Print file accepted - job ID %d.\n", job_id);
    }

    if (response)
      ippDelete(response);

    if (ipp_status <= IPP_OK_CONFLICT && argc > 6)
    {
      fprintf(stderr, "PAGE: 1 %d\n", copies_sup ? atoi(argv[4]) : 1);
      copies --;
    }
    else if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	     ipp_status == IPP_PRINTER_BUSY)
      break;
    else
      copies --;

   /*
    * Wait for the job to complete...
    */

    if (!job_id || !waitjob)
      continue;

    fputs("INFO: Waiting for job to complete...\n", stderr);

    for (;;)
    {
     /*
      * Build an IPP_GET_JOB_ATTRIBUTES request...
      */

      request = ippNew();
      request->request.op.version[1]   = version;
      request->request.op.operation_id = IPP_GET_JOB_ATTRIBUTES;
      request->request.op.request_id   = 1;

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, charset);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL,
        	   language != NULL ? language->language : "en");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	   NULL, uri);

      ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
        	    job_id);

      if (argv[2][0])
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	             "requesting-user-name", NULL, argv[2]);

      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                    "requested-attributes", sizeof(jattrs) / sizeof(jattrs[0]),
		    NULL, jattrs);

     /*
      * Do the request...
      */

      if (!copies_sup)
	httpReconnect(http);

      if ((response = cupsDoRequest(http, request, resource)) == NULL)
	ipp_status = cupsLastError();
      else
	ipp_status = response->request.status.status_code;

      if (ipp_status == IPP_NOT_FOUND)
      {
       /*
        * Job has gone away and/or the server has no job history...
	*/

        ippDelete(response);

	ipp_status = IPP_OK;
        break;
      }

      if (ipp_status > IPP_OK_CONFLICT)
      {
	if (ipp_status != IPP_SERVICE_UNAVAILABLE &&
	    ipp_status != IPP_PRINTER_BUSY)
	{
	  if (response)
	    ippDelete(response);

          fprintf(stderr, "ERROR: Unable to get job %d attributes (%s)!\n",
	          job_id, ippErrorString(ipp_status));
          break;
	}
      }

      if (response != NULL)
      {
	if ((job_state = ippFindAttribute(response, "job-state",
	                                  IPP_TAG_ENUM)) != NULL)
	{
	 /*
          * Stop polling if the job is finished or pending-held...
	  */

          if (job_state->values[0].integer > IPP_JOB_PROCESSING ||
	      job_state->values[0].integer == IPP_JOB_HELD)
	  {
	    if ((job_sheets = ippFindAttribute(response, "job-media-sheets-completed",
	                                   IPP_TAG_INTEGER)) != NULL)
	      fprintf(stderr, "PAGE: total %d\n", job_sheets->values[0].integer);

	    ippDelete(response);
	    break;
	  }
	}
      }

      if (response)
	ippDelete(response);

     /*
      * Check the printer state and report it if necessary...
      */

/*      if (!copies_sup)
	httpReconnect(http);*/

      check_printer_state(http, language, charset, uri, resource, argv[2],
                          version);

     /*
      * Wait 10 seconds before polling again...
      */

      sleep(10);
    }
  }

 /*
  * Check the printer state and report it if necessary...
  */

/*      if (!copies_sup)
	httpReconnect(http);*/

  check_printer_state(http, language, charset, uri, resource, argv[2], version);

 /*
  * Free memory...
  */

  httpClose(http);

  if (supported)
    ippDelete(supported);

 /*
  * Remove the temporary file(s) if necessary...
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

#ifdef __APPLE__
  if (pstmpname[0])
    unlink(pstmpname);
#endif /* __APPLE__ */

 /*
  * Return the queue status...
  */

  return (ipp_status > IPP_OK_CONFLICT);
}


/*
 * 'check_printer_state()' - Check the printer state...
 */

void
check_printer_state(http_t      *http,	/* I - HTTP connection */
                    cups_lang_t *language,
					/* I - Language */
		    const char  *charset,
					/* I - Charset */
		    const char  *uri,	/* I - Printer URI */
		    const char  *resource,
					/* I - Resource path */
		    const char  *user,	/* I - Username, if any */
		    int         version)/* I - IPP version */
{
  ipp_t	*request,			/* IPP request */
	*response;			/* IPP response */


 /*
  * Check on the printer state...
  */

  request = ippNew();
  request->request.op.version[1]   = version;
  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, charset);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "en");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  if (user && user[0])
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, user);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-state-reasons");

 /*
  * Do the request...
  */

  if ((response = cupsDoRequest(http, request, resource)) != NULL)
  {
    report_printer_state(response);
    ippDelete(response);
  }
}


/*
 * 'password_cb()' - Disable the password prompt for cupsDoFileRequest().
 */

const char *			/* O - Password  */
password_cb(const char *prompt)	/* I - Prompt (not used) */
{
  (void)prompt;

  return (password);
}


/*
 * 'report_printer_state()' - Report the printer state.
 */

int					/* O - Number of reasons shown */
report_printer_state(ipp_t *ipp)	/* I - IPP response */
{
  int			i;		/* Looping var */
  int			count;		/* Count of reasons shown... */
  ipp_attribute_t	*reasons;	/* printer-state-reasons */
  const char		*reason;	/* Current reason */
  const char		*message;	/* Message to show */
  char			unknown[1024];	/* Unknown message string */
  const char		*prefix;	/* Prefix for STATE: line */
  char			state[1024];	/* State string */


  if ((reasons = ippFindAttribute(ipp, "printer-state-reasons",
                                  IPP_TAG_KEYWORD)) == NULL)
    return (0);

  state[0] = '\0';
  prefix   = "STATE: ";

  for (i = 0, count = 0; i < reasons->num_values; i ++)
  {
    reason = reasons->values[i].string.text;

    strlcat(state, prefix, sizeof(state));
    strlcat(state, reason, sizeof(state));

    prefix  = ",";
    message = NULL;

    if (strncmp(reason, "media-needed", 12) == 0)
      message = "Media tray needs to be filled.";
    else if (strncmp(reason, "media-jam", 9) == 0)
      message = "Media jam!";
    else if (strncmp(reason, "moving-to-paused", 16) == 0 ||
             strncmp(reason, "paused", 6) == 0 ||
	     strncmp(reason, "shutdown", 8) == 0)
      message = "Printer off-line.";
    else if (strncmp(reason, "toner-low", 9) == 0)
      message = "Toner low.";
    else if (strncmp(reason, "toner-empty", 11) == 0)
      message = "Out of toner!";
    else if (strncmp(reason, "cover-open", 10) == 0)
      message = "Cover open.";
    else if (strncmp(reason, "interlock-open", 14) == 0)
      message = "Interlock open.";
    else if (strncmp(reason, "door-open", 9) == 0)
      message = "Door open.";
    else if (strncmp(reason, "input-tray-missing", 18) == 0)
      message = "Media tray missing!";
    else if (strncmp(reason, "media-low", 9) == 0)
      message = "Media tray almost empty.";
    else if (strncmp(reason, "media-empty", 11) == 0)
      message = "Media tray empty!";
    else if (strncmp(reason, "output-tray-missing", 19) == 0)
      message = "Output tray missing!";
    else if (strncmp(reason, "output-area-almost-full", 23) == 0)
      message = "Output bin almost full.";
    else if (strncmp(reason, "output-area-full", 16) == 0)
      message = "Output bin full!";
    else if (strncmp(reason, "marker-supply-low", 17) == 0)
      message = "Ink/toner almost empty.";
    else if (strncmp(reason, "marker-supply-empty", 19) == 0)
      message = "Ink/toner empty!";
    else if (strncmp(reason, "marker-waste-almost-full", 24) == 0)
      message = "Ink/toner waste bin almost full.";
    else if (strncmp(reason, "marker-waste-full", 17) == 0)
      message = "Ink/toner waste bin full!";
    else if (strncmp(reason, "fuser-over-temp", 15) == 0)
      message = "Fuser temperature high!";
    else if (strncmp(reason, "fuser-under-temp", 16) == 0)
      message = "Fuser temperature low!";
    else if (strncmp(reason, "opc-near-eol", 12) == 0)
      message = "OPC almost at end-of-life.";
    else if (strncmp(reason, "opc-life-over", 13) == 0)
      message = "OPC at end-of-life!";
    else if (strncmp(reason, "developer-low", 13) == 0)
      message = "Developer almost empty.";
    else if (strncmp(reason, "developer-empty", 15) == 0)
      message = "Developer empty!";
    else if (strstr(reason, "error") != NULL)
    {
      message = unknown;

      snprintf(unknown, sizeof(unknown), "Unknown printer error (%s)!",
               reason);
    }

    if (message)
    {
      count ++;
      if (strstr(reasons->values[i].string.text, "error"))
        fprintf(stderr, "ERROR: %s\n", message);
      else if (strstr(reasons->values[i].string.text, "warning"))
        fprintf(stderr, "WARNING: %s\n", message);
      else
        fprintf(stderr, "INFO: %s\n", message);
    }
  }

  fprintf(stderr, "%s\n", state);

  return (count);
}


#ifdef __APPLE__
/*
 * 'run_pictwps_filter()' - Convert PICT files to PostScript when printing
 *                          remotely.
 *
 * This step is required because the PICT format is not documented and
 * subject to change, so developing a filter for other OS's is infeasible.
 * Also, fonts required by the PICT file need to be embedded on the
 * client side (which has the fonts), so we run the filter to get a
 * PostScript file for printing...
 */

int						/* O - Exit status of filter */
run_pictwps_filter(char       **argv,		/* I - Command-line arguments */
                   const char *filename)	/* I - Filename */
{
  struct stat	fileinfo;			/* Print file information */
  const char	*ppdfile;			/* PPD file for destination printer */
  int		pid;				/* Child process ID */
  int		fd;				/* Temporary file descriptor */
  int		status;				/* Exit status of filter */
  const char	*printer;			/* PRINTER env var */
  static char	ppdenv[1024];			/* PPD environment variable */


 /*
  * First get the PPD file for the printer...
  */

  printer = getenv("PRINTER");
  if (!printer)
  {
    fputs("ERROR: PRINTER environment variable not defined!\n", stderr);
    return (-1);
  }

  if ((ppdfile = cupsGetPPD(printer)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to get PPD file for printer \"%s\" - %s.\n",
            printer, ippErrorString(cupsLastError()));
    /*return (-1);*/
  }
  else
  {
    snprintf(ppdenv, sizeof(ppdenv), "PPD=%s", ppdfile);
    putenv(ppdenv);
  }

 /*
  * Then create a temporary file for printing...
  */

  if ((fd = cupsTempFd(pstmpname, sizeof(pstmpname))) < 0)
  {
    fprintf(stderr, "ERROR: Unable to create temporary file - %s.\n",
            strerror(errno));
    if (ppdfile)
      unlink(ppdfile);
    return (-1);
  }

 /*
  * Get the owner of the spool file - it is owned by the user we want to run
  * as...
  */

  if (argv[6])
    stat(argv[6], &fileinfo);
  else
  {
   /*
    * Use the OSX defaults, as an up-stream filter created the PICT
    * file...
    */

    fileinfo.st_uid = 1;
    fileinfo.st_gid = 80;
  }

  if (ppdfile)
    chown(ppdfile, fileinfo.st_uid, fileinfo.st_gid);

  fchown(fd, fileinfo.st_uid, fileinfo.st_gid);

 /*
  * Finally, run the filter to convert the file...
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child process for pictwpstops...  Redirect output of pictwpstops to a
    * file...
    */

    close(1);
    dup(fd);
    close(fd);

    if (!getuid())
    {
     /*
      * Change to an unpriviledged user...
      */

      setgid(fileinfo.st_gid);
      setuid(fileinfo.st_uid);
    }

    execlp("pictwpstops", printer, argv[1], argv[2], argv[3], argv[4], argv[5],
           filename, NULL);
    perror("ERROR: Unable to exec pictwpstops");
    return (errno);
  }

  close(fd);

  if (pid < 0)
  {
   /*
    * Error!
    */

    perror("ERROR: Unable to fork pictwpstops");
    unlink(filename);
    if (ppdfile)
      unlink(ppdfile);
    return (-1);
  }

 /*
  * Now wait for the filter to complete...
  */

  if (wait(&status) < 0)
  {
    perror("ERROR: Unable to wait for pictwpstops");
    close(fd);
    unlink(filename);
    if (ppdfile)
      unlink(ppdfile);
    return (-1);
  }

  if (ppdfile)
    unlink(ppdfile);

  close(fd);

  if (status)
  {
    if (status >= 256)
      fprintf(stderr, "ERROR: pictwpstops exited with status %d!\n",
              status / 256);
    else
      fprintf(stderr, "ERROR: pictwpstops exited on signal %d!\n",
              status);

    unlink(filename);
    return (status);
  }

 /*
  * Return with no errors..
  */

  return (0);
}
#endif /* __APPLE__ */


/*
 * 'sigterm_handler()' - Handle 'terminate' signals that stop the backend.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
  (void)sig;	/* remove compiler warnings... */

 /*
  * Remove the temporary file(s) if necessary...
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

#ifdef __APPLE__
  if (pstmpname[0])
    unlink(pstmpname);
#endif /* __APPLE__ */

  exit(1);
}


/*
 * End of "$Id$".
 */
