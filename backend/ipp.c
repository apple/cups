/*
 * "$Id$"
 *
 *   IPP backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()                 - Send a file to the printer or server.
 *   cancel_job()           - Cancel a print job.
 *   check_printer_state()  - Check the printer state...
 *   compress_files()       - Compress print files...
 *   password_cb()          - Disable the password prompt for
 *                            cupsDoFileRequest().
 *   report_attr()          - Report an IPP attribute value.
 *   report_printer_state() - Report the printer state.
 *   run_pictwps_filter()   - Convert PICT files to PostScript when printing
 *                            remotely.
 *   sigterm_handler()      - Handle 'terminate' signals that stop the backend.
 */

/*
 * Include necessary headers.
 */

#include <cups/http-private.h>
#include "backend-private.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/*
 * Globals...
 */

static char	*password = NULL;	/* Password for device URI */
static int	password_tries = 0;	/* Password tries */
#ifdef __APPLE__
static char	pstmpname[1024] = "";	/* Temporary PostScript file name */
#endif /* __APPLE__ */
static char	tmpfilename[1024] = "";	/* Temporary spool file name */
static int	job_cancelled = 0;	/* Job cancelled? */


/*
 * Local functions...
 */

static void	cancel_job(http_t *http, const char *uri, int id,
		           const char *resource, const char *user, int version);
static void	check_printer_state(http_t *http, const char *uri,
		                    const char *resource, const char *user,
				    int version, int job_id);
#ifdef HAVE_LIBZ
static void	compress_files(int num_files, char **files);
#endif /* HAVE_LIBZ */
static const char *password_cb(const char *);
static void	report_attr(ipp_attribute_t *attr);
static int	report_printer_state(ipp_t *ipp, int job_id);

#ifdef __APPLE__
static int	run_pictwps_filter(char **argv, const char *filename);
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
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  int		send_options;		/* Send job options? */
  int		num_options;		/* Number of printer options */
  cups_option_t	*options;		/* Printer options */
  const char	*device_uri;		/* Device URI */
  char		method[255],		/* Method in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info */
		resource[1024],		/* Resource info (printer name) */
		addrname[256],		/* Address name */
		*optptr,		/* Pointer to URI options */
		*name,			/* Name of option */
		*value,			/* Value of option */
		sep;			/* Separator character */
  int		snmp_fd,		/* SNMP socket */
		start_count,		/* Page count via SNMP at start */
		page_count,		/* Page count via SNMP */
		have_supplies;		/* Printer supports supply levels? */
  int		num_files;		/* Number of files to print */
  char		**files,		/* Files to print */
		*filename;		/* Pointer to single filename */
  int		port;			/* Port number (not used) */
  char		uri[HTTP_MAX_URI];	/* Updated URI without user/pass */
  ipp_status_t	ipp_status;		/* Status of IPP request */
  http_t	*http;			/* HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response,		/* IPP response */
		*supported;		/* get-printer-attributes response */
  time_t	start_time;		/* Time of first connect */
  int		recoverable;		/* Recoverable error shown? */
  int		contimeout;		/* Connection timeout */
  int		delay;			/* Delay for retries... */
  int		compression,		/* Do compression of the job data? */
		waitjob,		/* Wait for job complete? */
		waitprinter;		/* Wait for printer ready? */
  ipp_attribute_t *job_id_attr;		/* job-id attribute */
  int		job_id;			/* job-id value */
  ipp_attribute_t *job_sheets;		/* job-media-sheets-completed */
  ipp_attribute_t *job_state;		/* job-state */
  ipp_attribute_t *copies_sup;		/* copies-supported */
  ipp_attribute_t *format_sup;		/* document-format-supported */
  ipp_attribute_t *printer_state;	/* printer-state attribute */
  ipp_attribute_t *printer_accepting;	/* printer-is-accepting-jobs */
  int		copies,			/* Number of copies for job */
		copies_remaining;	/* Number of copies remaining */
  const char	*content_type,		/* CONTENT_TYPE environment variable */
		*final_content_type;	/* FINAL_CONTENT_TYPE environment var */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		version;		/* IPP version */
  static const char * const pattrs[] =
		{			/* Printer attributes we want */
                  "com.apple.print.recoverable-message",
		  "copies-supported",
		  "document-format-supported",
		  "marker-colors",
		  "marker-levels",
		  "marker-message",
		  "marker-names",
		  "marker-types",
		  "printer-is-accepting-jobs",
		  "printer-state",
		  "printer-state-message",
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

    printf("network %s \"Unknown\" \"%s (%s)\"\n",
           s, _cupsLangString(cupsLangDefault(),
	                      _("Internet Printing Protocol")), s);
    return (CUPS_BACKEND_OK);
  }
  else if (argc < 6)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]\n"),
		    argv[0]);
    return (CUPS_BACKEND_STOP);
  }

 /*
  * Get the (final) content type...
  */

  if ((content_type = getenv("CONTENT_TYPE")) == NULL)
    content_type = "application/octet-stream";

  if ((final_content_type = getenv("FINAL_CONTENT_TYPE")) == NULL)
  {
    final_content_type = content_type;

    if (!strncmp(final_content_type, "printer/", 8))
      final_content_type = "application/vnd.cups-raw";
  }

 /*
  * Extract the hostname and printer name from the URI...
  */

  if ((device_uri = cupsBackendDeviceURI(argv)) == NULL)
    return (CUPS_BACKEND_FAILED);

  if (httpSeparateURI(HTTP_URI_CODING_ALL, device_uri,
                      method, sizeof(method), username, sizeof(username),
		      hostname, sizeof(hostname), &port,
		      resource, sizeof(resource)) < HTTP_URI_OK)
  {
    _cupsLangPuts(stderr,
                  _("ERROR: Missing device URI on command-line and no "
		    "DEVICE_URI environment variable!\n"));
    return (CUPS_BACKEND_STOP);
  }

  if (!port)
    port = IPP_PORT;			/* Default to port 631 */

  if (!strcmp(method, "https"))
    cupsSetEncryption(HTTP_ENCRYPT_ALWAYS);
  else
    cupsSetEncryption(HTTP_ENCRYPT_IF_REQUESTED);

 /*
  * See if there are any options...
  */

  compression = 0;
  version     = 1;
  waitjob     = 1;
  waitprinter = 1;
  contimeout  = 7 * 24 * 60 * 60;

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

      name = optptr;

      while (*optptr && *optptr != '=' && *optptr != '+' && *optptr != '&')
        optptr ++;

      if ((sep = *optptr) != '\0')
        *optptr++ = '\0';

      if (sep == '=')
      {
       /*
        * Get the value...
	*/

        value = optptr;

	while (*optptr && *optptr != '+' && *optptr != '&')
	  optptr ++;

        if (*optptr)
	  *optptr++ = '\0';
      }
      else
        value = (char *)"";

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
	  _cupsLangPrintf(stderr,
	                  _("ERROR: Unknown encryption option value \"%s\"!\n"),
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
	  _cupsLangPrintf(stderr,
	                  _("ERROR: Unknown version option value \"%s\"!\n"),
	        	  value);
	}
      }
#ifdef HAVE_LIBZ
      else if (!strcasecmp(name, "compression"))
      {
        compression = !strcasecmp(value, "true") ||
	              !strcasecmp(value, "yes") ||
	              !strcasecmp(value, "on") ||
	              !strcasecmp(value, "gzip");
      }
#endif /* HAVE_LIBZ */
      else if (!strcasecmp(name, "contimeout"))
      {
       /*
        * Set the connection timeout...
	*/

	if (atoi(value) > 0)
	  contimeout = atoi(value);
      }
      else
      {
       /*
        * Unknown option...
	*/

	_cupsLangPrintf(stderr,
	                _("ERROR: Unknown option \"%s\" with value \"%s\"!\n"),
			name, value);
      }
    }
  }

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

    int		fd;			/* File descriptor */
    cups_file_t	*fp;			/* Temporary file */
    char	buffer[8192];		/* Buffer for copying */
    int		bytes;			/* Number of bytes read */
    off_t	tbytes;			/* Total bytes copied */


    if ((fd = cupsTempFd(tmpfilename, sizeof(tmpfilename))) < 0)
    {
      _cupsLangPrintError(_("ERROR: Unable to create temporary file"));
      return (CUPS_BACKEND_FAILED);
    }

    if ((fp = cupsFileOpenFd(fd, compression ? "w9" : "w")) == NULL)
    {
      _cupsLangPrintError(_("ERROR: Unable to open temporary file"));
      close(fd);
      unlink(tmpfilename);
      return (CUPS_BACKEND_FAILED);
    }

    tbytes = 0;

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (cupsFileWrite(fp, buffer, bytes) < bytes)
      {
        _cupsLangPrintError(_("ERROR: Unable to write to temporary file"));
	cupsFileClose(fp);
	unlink(tmpfilename);
	return (CUPS_BACKEND_FAILED);
      }
      else
        tbytes += bytes;

    cupsFileClose(fp);

   /*
    * Don't try printing files less than 2 bytes...
    */

    if (tbytes <= 1)
    {
      _cupsLangPuts(stderr, _("ERROR: Empty print file!\n"));
      unlink(tmpfilename);
      return (CUPS_BACKEND_FAILED);
    }

   /*
    * Point to the single file from stdin...
    */

    filename     = tmpfilename;
    num_files    = 1;
    files        = &filename;
    send_options = 0;
  }
  else
  {
   /*
    * Point to the files on the command-line...
    */

    num_files    = argc - 6;
    files        = argv + 6;
    send_options = 1;

#ifdef HAVE_LIBZ
    if (compression)
      compress_files(num_files, files);
#endif /* HAVE_LIBZ */
  }

  fprintf(stderr, "DEBUG: %d files to send in job...\n", num_files);

 /*
  * Set the authentication info, if any...
  */

  cupsSetPasswordCB(password_cb);

  if (username[0])
  {
   /*
    * Use authenticaion information in the device URI...
    */

    if ((password = strchr(username, ':')) != NULL)
      *password++ = '\0';

    cupsSetUser(username);
  }
  else if (!getuid())
  {
   /*
    * Try loading authentication information from the environment.
    */

    const char *ptr = getenv("AUTH_USERNAME");

    if (ptr)
      cupsSetUser(ptr);

    password = getenv("AUTH_PASSWORD");
  }

 /*
  * Try connecting to the remote server...
  */

  delay       = 5;
  recoverable = 0;
  start_time  = time(NULL);

  fputs("STATE: +connecting-to-device\n", stderr);

  do
  {
    fprintf(stderr, "DEBUG: Connecting to %s:%d\n",
	    hostname, port);
    _cupsLangPuts(stderr, _("INFO: Connecting to printer...\n"));

    if ((http = httpConnectEncrypt(hostname, port, cupsEncryption())) == NULL)
    {
      if (job_cancelled)
	break;

      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        _cupsLangPuts(stderr,
	              _("INFO: Unable to contact printer, queuing on next "
			"printer in class...\n"));

        if (tmpfilename[0])
	  unlink(tmpfilename);

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (CUPS_BACKEND_FAILED);
      }

      if (errno == ECONNREFUSED || errno == EHOSTDOWN ||
          errno == EHOSTUNREACH)
      {
        if (contimeout && (time(NULL) - start_time) > contimeout)
	{
	  _cupsLangPuts(stderr, _("ERROR: Printer not responding!\n"));
	  return (CUPS_BACKEND_FAILED);
	}

        recoverable = 1;

	_cupsLangPrintf(stderr,
			_("WARNING: recoverable: Network host \'%s\' is busy; "
			  "will retry in %d seconds...\n"),
			hostname, delay);

	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else if (h_errno)
      {
	_cupsLangPrintf(stderr, _("ERROR: Unable to locate printer \'%s\'!\n"),
			hostname);
	return (CUPS_BACKEND_STOP);
      }
      else
      {
        recoverable = 1;

        fprintf(stderr, "DEBUG: Connection error: %s\n", strerror(errno));
	_cupsLangPuts(stderr,
	              _("ERROR: recoverable: Unable to connect to printer; will "
			"retry in 30 seconds...\n"));
	sleep(30);
      }

      if (job_cancelled)
	break;
    }
  }
  while (http == NULL);

  if (job_cancelled)
  {
    if (tmpfilename[0])
      unlink(tmpfilename);

    return (CUPS_BACKEND_FAILED);
  }

  fputs("STATE: -connecting-to-device\n", stderr);
  _cupsLangPuts(stderr, _("INFO: Connected to printer...\n"));

#ifdef AF_INET6
  if (http->hostaddr->addr.sa_family == AF_INET6)
    fprintf(stderr, "DEBUG: Connected to [%s]:%d (IPv6)...\n",
	    httpAddrString(http->hostaddr, addrname, sizeof(addrname)),
	    ntohs(http->hostaddr->ipv6.sin6_port));
  else
#endif /* AF_INET6 */
    if (http->hostaddr->addr.sa_family == AF_INET)
      fprintf(stderr, "DEBUG: Connected to %s:%d (IPv4)...\n",
	      httpAddrString(http->hostaddr, addrname, sizeof(addrname)),
	      ntohs(http->hostaddr->ipv4.sin_port));

 /*
  * See if the printer supports SNMP...
  */

  if ((snmp_fd = _cupsSNMPOpen(http->hostaddr->addr.sa_family)) >= 0)
    have_supplies = !backendSNMPSupplies(snmp_fd, http->hostaddr, &start_count,
                                         NULL);
  else
    have_supplies = start_count = 0;

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

  copies_sup = NULL;
  format_sup = NULL;
  supported  = NULL;

  do
  {
   /*
    * Check for side-channel requests...
    */

    backendCheckSideChannel(snmp_fd, http->hostaddr);

   /*
    * Build the IPP request...
    */

    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
    request->request.op.version[1] = version;

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
        if (contimeout && (time(NULL) - start_time) > contimeout)
	{
	  _cupsLangPuts(stderr, _("ERROR: Printer not responding!\n"));
	  return (CUPS_BACKEND_FAILED);
	}

        recoverable = 1;

	_cupsLangPrintf(stderr,
			_("WARNING: recoverable: Network host \'%s\' is busy; "
			  "will retry in %d seconds...\n"),
			hostname, delay);

        report_printer_state(supported, 0);

	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else if ((ipp_status == IPP_BAD_REQUEST ||
	        ipp_status == IPP_VERSION_NOT_SUPPORTED) && version == 1)
      {
       /*
	* Switch to IPP/1.0...
	*/

	_cupsLangPuts(stderr,
	              _("INFO: Printer does not support IPP/1.1, trying "
		        "IPP/1.0...\n"));
	version = 0;
	httpReconnect(http);
      }
      else if (ipp_status == IPP_NOT_FOUND)
      {
        _cupsLangPuts(stderr, _("ERROR: Destination printer does not exist!\n"));

	if (supported)
          ippDelete(supported);

	return (CUPS_BACKEND_STOP);
      }
      else
      {
	_cupsLangPrintf(stderr,
	                _("ERROR: Unable to get printer status (%s)!\n"),
			cupsLastErrorString());
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

    format_sup = ippFindAttribute(supported, "document-format-supported",
	                          IPP_TAG_MIMETYPE);

    if (format_sup)
    {
      fprintf(stderr, "DEBUG: document-format-supported (%d values)\n",
	      format_sup->num_values);
      for (i = 0; i < format_sup->num_values; i ++)
	fprintf(stderr, "DEBUG: [%d] = \"%s\"\n", i,
	        format_sup->values[i].string.text);
    }

    report_printer_state(supported, 0);
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
	(printer_state->values[0].integer > IPP_PRINTER_PROCESSING &&
	 waitprinter) ||
	printer_accepting == NULL ||
	!printer_accepting->values[0].boolean)
    {
     /*
      * If the CLASS environment variable is set, the job was submitted
      * to a class and not to a specific queue.  In this case, we want
      * to abort immediately so that the job can be requeued on the next
      * available printer in the class.
      */

      _cupsLangPuts(stderr,
                    _("INFO: Unable to contact printer, queuing on next "
		      "printer in class...\n"));

      ippDelete(supported);
      httpClose(http);

      if (tmpfilename[0])
	unlink(tmpfilename);

     /*
      * Sleep 5 seconds to keep the job from requeuing too rapidly...
      */

      sleep(5);

      return (CUPS_BACKEND_FAILED);
    }
  }

  if (recoverable)
  {
   /*
    * If we've shown a recoverable error make sure the printer proxies
    * have a chance to see the recovered message. Not pretty but
    * necessary for now...
    */

    fputs("INFO: recovered: \n", stderr);
    sleep(5);
  }

 /*
  * See if the printer supports multiple copies...
  */

  copies = atoi(argv[4]);

  if (copies_sup || argc < 7)
  {
    copies_remaining = 1;

    if (argc < 7)
      copies = 1;
  }
  else
    copies_remaining = copies;

 /*
  * Then issue the print-job request...
  */

  job_id  = 0;

  while (copies_remaining > 0)
  {
   /*
    * Check for side-channel requests...
    */

    backendCheckSideChannel(snmp_fd, http->hostaddr);

   /*
    * Build the IPP request...
    */

    if (job_cancelled)
      break;

    if (num_files > 1)
      request = ippNewRequest(IPP_CREATE_JOB);
    else
      request = ippNewRequest(IPP_PRINT_JOB);

    request->request.op.version[1] = version;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    fprintf(stderr, "DEBUG: printer-uri = \"%s\"\n", uri);

    if (argv[2][0])
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, argv[2]);

    fprintf(stderr, "DEBUG: requesting-user-name = \"%s\"\n", argv[2]);

   /*
    * Only add a "job-name" attribute if the remote server supports
    * copy generation - some IPP implementations like HP's don't seem
    * to like UTF-8 job names (STR #1837)...
    */

    if (argv[3][0] && copies_sup)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
        	   argv[3]);

    fprintf(stderr, "DEBUG: job-name = \"%s\"\n", argv[3]);

#ifdef HAVE_LIBZ
    if (compression)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                   "compression", NULL, "gzip");
#endif /* HAVE_LIBZ */

   /*
    * Handle options on the command-line...
    */

    options     = NULL;
    num_options = cupsParseOptions(argv[5], 0, &options);

#ifdef __APPLE__
    if (!strcasecmp(final_content_type, "application/pictwps") &&
        num_files == 1)
    {
      if (format_sup != NULL)
      {
	for (i = 0; i < format_sup->num_values; i ++)
	  if (!strcasecmp(final_content_type, format_sup->values[i].string.text))
	    break;
      }

      if (format_sup == NULL || i >= format_sup->num_values)
      {
       /*
	* Remote doesn't support "application/pictwps" (i.e. it's not MacOS X)
	* so convert the document to PostScript...
	*/

	if (run_pictwps_filter(argv, files[0]))
	{
	  if (pstmpname[0])
	    unlink(pstmpname);

	  if (tmpfilename[0])
	    unlink(tmpfilename);

	  return (CUPS_BACKEND_FAILED);
        }

        files[0] = pstmpname;

       /*
	* Change the MIME type to application/postscript and change the
	* number of copies to 1...
	*/

	final_content_type = "application/postscript";
	copies             = 1;
	copies_remaining   = 1;
        send_options       = 0;
      }
    }
#endif /* __APPLE__ */

    if (format_sup != NULL)
    {
      for (i = 0; i < format_sup->num_values; i ++)
        if (!strcasecmp(final_content_type, format_sup->values[i].string.text))
          break;

      if (i < format_sup->num_values)
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
	             "document-format", NULL, final_content_type);
    }

    if (copies_sup && version > 0 && send_options)
    {
     /*
      * Only send options if the destination printer supports the copies
      * attribute and IPP/1.1.  This is a hack for the HP and Lexmark
      * implementations of IPP, which do not accept extension attributes
      * and incorrectly report a client-error-bad-request error instead of
      * the successful-ok-unsupported-attributes status.  In short, at least
      * some HP and Lexmark implementations of IPP are non-compliant.
      */

      cupsEncodeOptions(request, num_options, options);

      ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies",
                    copies);
    }

    cupsFreeOptions(num_options, options);

   /*
    * If copies aren't supported, then we are likely dealing with an HP
    * JetDirect.  The HP IPP implementation seems to close the connection
    * after every request - that is, it does *not* implement HTTP Keep-
    * Alive, which is REQUIRED by HTTP/1.1...
    */

    if (!copies_sup)
      httpReconnect(http);

   /*
    * Do the request...
    */

    if (num_files > 1)
      response = cupsDoRequest(http, request, resource);
    else
      response = cupsDoFileRequest(http, request, resource, files[0]);

    ipp_status = cupsLastError();

    if (ipp_status > IPP_OK_CONFLICT)
    {
      job_id = 0;

      if (job_cancelled)
        break;

      if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	  ipp_status == IPP_PRINTER_BUSY)
      {
        _cupsLangPuts(stderr,
	              _("INFO: Printer busy; will retry in 10 seconds...\n"));
	sleep(10);
      }
      else if ((ipp_status == IPP_BAD_REQUEST ||
	        ipp_status == IPP_VERSION_NOT_SUPPORTED) && version == 1)
      {
       /*
	* Switch to IPP/1.0...
	*/

	_cupsLangPuts(stderr,
	              _("INFO: Printer does not support IPP/1.1, trying "
		        "IPP/1.0...\n"));
	version = 0;
	httpReconnect(http);
      }
      else
        _cupsLangPrintf(stderr, _("ERROR: Print file was not accepted (%s)!\n"),
			cupsLastErrorString());
    }
    else if ((job_id_attr = ippFindAttribute(response, "job-id",
                                             IPP_TAG_INTEGER)) == NULL)
    {
      _cupsLangPuts(stderr,
                    _("NOTICE: Print file accepted - job ID unknown.\n"));
      job_id = 0;
    }
    else
    {
      job_id = job_id_attr->values[0].integer;
      _cupsLangPrintf(stderr, _("NOTICE: Print file accepted - job ID %d.\n"),
                      job_id);
    }

    ippDelete(response);

    if (job_cancelled)
      break;

    if (job_id && num_files > 1)
    {
      for (i = 0; i < num_files; i ++)
      {
       /*
	* Check for side-channel requests...
	*/

	backendCheckSideChannel(snmp_fd, http->hostaddr);

       /*
        * Send the next file in the job...
	*/

	request = ippNewRequest(IPP_SEND_DOCUMENT);

	request->request.op.version[1] = version;

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	     NULL, uri);

        ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
	              job_id);

	if (argv[2][0])
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                       "requesting-user-name", NULL, argv[2]);

        if ((i + 1) == num_files)
	  ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);

        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
	             "document-format", NULL, content_type);

        ippDelete(cupsDoFileRequest(http, request, resource, files[i]));

	if (cupsLastError() > IPP_OK_CONFLICT)
	{
	  ipp_status = cupsLastError();

	  _cupsLangPrintf(stderr,
			  _("ERROR: Unable to add file %d to job: %s\n"),
			  job_id, cupsLastErrorString());
	  break;
	}
      }
    }

    if (ipp_status <= IPP_OK_CONFLICT && argc > 6)
    {
      fprintf(stderr, "PAGE: 1 %d\n", copies_sup ? atoi(argv[4]) : 1);
      copies_remaining --;
    }
    else if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	     ipp_status == IPP_PRINTER_BUSY)
      continue;
    else
      copies_remaining --;

   /*
    * Wait for the job to complete...
    */

    if (!job_id || !waitjob)
      continue;

    _cupsLangPuts(stderr, _("INFO: Waiting for job to complete...\n"));

    for (delay = 1; !job_cancelled;)
    {
     /*
      * Check for side-channel requests...
      */

      backendCheckSideChannel(snmp_fd, http->hostaddr);

     /*
      * Build an IPP_GET_JOB_ATTRIBUTES request...
      */

      request = ippNewRequest(IPP_GET_JOB_ATTRIBUTES);
      request->request.op.version[1] = version;

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

      response   = cupsDoRequest(http, request, resource);
      ipp_status = cupsLastError();

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
	  ippDelete(response);

          _cupsLangPrintf(stderr,
			  _("ERROR: Unable to get job %d attributes (%s)!\n"),
			  job_id, cupsLastErrorString());
          break;
	}
      }

      if (response)
      {
	if ((job_state = ippFindAttribute(response, "job-state",
	                                  IPP_TAG_ENUM)) != NULL)
	{
	 /*
          * Stop polling if the job is finished or pending-held...
	  */

          if (job_state->values[0].integer > IPP_JOB_STOPPED)
	  {
	    if ((job_sheets = ippFindAttribute(response, 
	                                       "job-media-sheets-completed",
	                                       IPP_TAG_INTEGER)) != NULL)
	      fprintf(stderr, "PAGE: total %d\n",
	              job_sheets->values[0].integer);

	    ippDelete(response);
	    break;
	  }
	}
      }

      ippDelete(response);

     /*
      * Check the printer state and report it if necessary...
      */

      check_printer_state(http, uri, resource, argv[2], version, job_id);

     /*
      * Wait 1-10 seconds before polling again...
      */

      sleep(delay);

      delay ++;
      if (delay > 10)
        delay = 1;
    }
  }

 /*
  * Cancel the job as needed...
  */

  if (job_cancelled && job_id)
    cancel_job(http, uri, job_id, resource, argv[2], version);

 /*
  * Check the printer state and report it if necessary...
  */

  check_printer_state(http, uri, resource, argv[2], version, job_id);

 /*
  * Collect the final page count as needed...
  */

  if (have_supplies && 
      !backendSNMPSupplies(snmp_fd, http->hostaddr, &page_count, NULL) &&
      page_count > start_count)
    fprintf(stderr, "PAGE: total %d\n", page_count - start_count);

 /*
  * Free memory...
  */

  httpClose(http);

  ippDelete(supported);

 /*
  * Remove the temporary file(s) if necessary...
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

#ifdef HAVE_LIBZ
  if (compression)
  {
    for (i = 0; i < num_files; i ++)
      unlink(files[i]);
  }
#endif /* HAVE_LIBZ */

#ifdef __APPLE__
  if (pstmpname[0])
    unlink(pstmpname);
#endif /* __APPLE__ */

 /*
  * Return the queue status...
  */

  if (ipp_status == IPP_NOT_AUTHORIZED)
  {
   /*
    * Authorization failures here mean that we need Kerberos.  Username +
    * password authentication is handled in the password_cb function.
    */

    fputs("ATTR: auth-info-required=negotiate\n", stderr);
    return (CUPS_BACKEND_AUTH_REQUIRED);
  }
  else if (ipp_status > IPP_OK_CONFLICT)
    return (CUPS_BACKEND_FAILED);
  else
    return (CUPS_BACKEND_OK);
}


/*
 * 'cancel_job()' - Cancel a print job.
 */

static void
cancel_job(http_t     *http,		/* I - HTTP connection */
           const char *uri,		/* I - printer-uri */
	   int        id,		/* I - job-id */
	   const char *resource,	/* I - Resource path */
	   const char *user,		/* I - requesting-user-name */
	   int        version)		/* I - IPP version */
{
  ipp_t	*request;			/* Cancel-Job request */


  _cupsLangPuts(stderr, _("INFO: Canceling print job...\n"));

  request = ippNewRequest(IPP_CANCEL_JOB);
  request->request.op.version[1] = version;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", id);

  if (user && user[0])
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, user);

 /*
  * Do the request...
  */

  ippDelete(cupsDoRequest(http, request, resource));

  if (cupsLastError() > IPP_OK_CONFLICT)
    _cupsLangPrintf(stderr, _("ERROR: Unable to cancel job %d: %s\n"), id,
		    cupsLastErrorString());
}


/*
 * 'check_printer_state()' - Check the printer state...
 */

static void
check_printer_state(
    http_t      *http,			/* I - HTTP connection */
    const char  *uri,			/* I - Printer URI */
    const char  *resource,		/* I - Resource path */
    const char  *user,			/* I - Username, if any */
    int         version,		/* I - IPP version */
    int         job_id)			/* I - Current job ID */
{
  ipp_t	*request,			/* IPP request */
	*response;			/* IPP response */
  static const char * const attrs[] =	/* Attributes we want */
  {
    "com.apple.print.recoverable-message",
    "marker-colors",
    "marker-levels",
    "marker-message",
    "marker-names",
    "marker-types",
    "printer-state-message",
    "printer-state-reasons"
  };


 /*
  * Check on the printer state...
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  request->request.op.version[1] = version;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  if (user && user[0])
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, user);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes",
		(int)(sizeof(attrs) / sizeof(attrs[0])), NULL, attrs);

 /*
  * Do the request...
  */

  if ((response = cupsDoRequest(http, request, resource)) != NULL)
  {
    report_printer_state(response, job_id);
    ippDelete(response);
  }
}


#ifdef HAVE_LIBZ
/*
 * 'compress_files()' - Compress print files...
 */

static void
compress_files(int  num_files,		/* I - Number of files */
               char **files)		/* I - Files */
{
  int		i,			/* Looping var */
		fd;			/* Temporary file descriptor */
  ssize_t	bytes;			/* Bytes read/written */
  size_t	total;			/* Total bytes read */
  cups_file_t	*in,			/* Input file */
		*out;			/* Output file */
  struct stat	outinfo;		/* Output file information */
  char		filename[1024],		/* Temporary filename */
		buffer[32768];		/* Copy buffer */


  fprintf(stderr, "DEBUG: Compressing %d job files...\n", num_files);
  for (i = 0; i < num_files; i ++)
  {
    if ((fd = cupsTempFd(filename, sizeof(filename))) < 0)
    {
      _cupsLangPrintf(stderr,
		      _("ERROR: Unable to create temporary compressed print "
		        "file: %s\n"), strerror(errno));
      exit(CUPS_BACKEND_FAILED);
    }

    if ((out = cupsFileOpenFd(fd, "w9")) == NULL)
    {
      _cupsLangPrintf(stderr,
		      _("ERROR: Unable to open temporary compressed print "
		        "file: %s\n"), strerror(errno));
      exit(CUPS_BACKEND_FAILED);
    }

    if ((in = cupsFileOpen(files[i], "r")) == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("ERROR: Unable to open print file \"%s\": %s\n"),
		      files[i], strerror(errno));
      cupsFileClose(out);
      exit(CUPS_BACKEND_FAILED);
    }

    total = 0;
    while ((bytes = cupsFileRead(in, buffer, sizeof(buffer))) > 0)
      if (cupsFileWrite(out, buffer, bytes) < bytes)
      {
        _cupsLangPrintf(stderr,
		        _("ERROR: Unable to write %d bytes to \"%s\": %s\n"),
			(int)bytes, filename, strerror(errno));
        cupsFileClose(in);
        cupsFileClose(out);
	exit(CUPS_BACKEND_FAILED);
      }
      else
        total += bytes;

    cupsFileClose(out);
    cupsFileClose(in);

    files[i] = strdup(filename);

    if (!stat(filename, &outinfo))
      fprintf(stderr,
              "DEBUG: File %d compressed to %.1f%% of original size, "
	      CUPS_LLFMT " bytes...\n",
              i + 1, 100.0 * outinfo.st_size / total,
	      CUPS_LLCAST outinfo.st_size);
  }
}
#endif /* HAVE_LIBZ */


/*
 * 'password_cb()' - Disable the password prompt for cupsDoFileRequest().
 */

static const char *			/* O - Password  */
password_cb(const char *prompt)		/* I - Prompt (not used) */
{
  (void)prompt;

  if (password && *password && password_tries < 3)
  {
    password_tries ++;

    return (password);
  }
  else
  {
   /*
    * If there is no password set in the device URI, return the
    * "authentication required" exit code...
    */

    if (tmpfilename[0])
      unlink(tmpfilename);

#ifdef __APPLE__
    if (pstmpname[0])
      unlink(pstmpname);
#endif /* __APPLE__ */

    fputs("ATTR: auth-info-required=username,password\n", stderr);

    exit(CUPS_BACKEND_AUTH_REQUIRED);

    return (NULL);			/* Eliminate compiler warning */
  }
}


/*
 * 'report_attr()' - Report an IPP attribute value.
 */

static void
report_attr(ipp_attribute_t *attr)	/* I - Attribute */
{
  int	i;				/* Looping var */
  char	value[1024],			/* Value string */
	*valptr,			/* Pointer into value string */
	*attrptr;			/* Pointer into attribute value */


 /*
  * Convert the attribute values into quoted strings...
  */

  for (i = 0, valptr = value;
       i < attr->num_values && valptr < (value + sizeof(value) - 10);
       i ++)
  {
    if (i > 0)
      *valptr++ = ',';

    switch (attr->value_tag)
    {
      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          snprintf(valptr, sizeof(value) - (valptr - value), "%d",
	           attr->values[i].integer);
	  valptr += strlen(valptr);
	  break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
          *valptr++ = '\"';
	  for (attrptr = attr->values[i].string.text;
	       *attrptr && valptr < (value + sizeof(value) - 10);
	       attrptr ++)
	  {
	    if (*attrptr == '\\' || *attrptr == '\"')
	      *valptr++ = '\\';

	    *valptr++ = *attrptr;
	  }
          *valptr++ = '\"';
          break;

      default :
         /*
	  * Unsupported value type...
	  */

          return;
    }
  }

  *valptr = '\0';

 /*
  * Tell the scheduler about the new values...
  */

  fprintf(stderr, "ATTR: %s=%s\n", attr->name, value);
}


/*
 * 'report_printer_state()' - Report the printer state.
 */

static int				/* O - Number of reasons shown */
report_printer_state(ipp_t *ipp,	/* I - IPP response */
                     int   job_id)	/* I - Current job ID */
{
  int			i;		/* Looping var */
  int			count;		/* Count of reasons shown... */
  ipp_attribute_t	*caprm,		/* com.apple.print.recoverable-message */
			*psm,		/* printer-state-message */
			*reasons,	/* printer-state-reasons */
			*marker;	/* marker-* attributes */
  const char		*reason;	/* Current reason */
  const char		*message;	/* Message to show */
  char			unknown[1024];	/* Unknown message string */
  const char		*prefix;	/* Prefix for STATE: line */
  char			state[1024];	/* State string */
  cups_lang_t		*language;	/* Current localization */
  int			saw_caprw;	/* Saw com.apple.print.recoverable-warning state */


  if ((psm = ippFindAttribute(ipp, "printer-state-message",
                              IPP_TAG_TEXT)) != NULL)
    fprintf(stderr, "INFO: %s\n", psm->values[0].string.text);

  if ((reasons = ippFindAttribute(ipp, "printer-state-reasons",
                                  IPP_TAG_KEYWORD)) == NULL)
    return (0);

  saw_caprw = 0;
  state[0]  = '\0';
  prefix    = "STATE: ";
  language  = cupsLangDefault();

  for (i = 0, count = 0; i < reasons->num_values; i ++)
  {
    reason = reasons->values[i].string.text;

    if (strcmp(reason, "paused"))
    {
      strlcat(state, prefix, sizeof(state));
      strlcat(state, reason, sizeof(state));

      prefix  = ",";
    }

    message = "";

    if (!strncmp(reason, "media-needed", 12))
      message = _("Media tray needs to be filled.");
    else if (!strncmp(reason, "media-jam", 9))
      message = _("Media jam!");
    else if (!strncmp(reason, "moving-to-paused", 16) ||
             !strncmp(reason, "offline", 7) ||
             !strncmp(reason, "paused", 6) ||
	     !strncmp(reason, "shutdown", 8))
      message = _("Printer offline.");
    else if (!strncmp(reason, "toner-low", 9))
      message = _("Toner low.");
    else if (!strncmp(reason, "toner-empty", 11))
      message = _("Out of toner!");
    else if (!strncmp(reason, "cover-open", 10))
      message = _("Cover open.");
    else if (!strncmp(reason, "interlock-open", 14))
      message = _("Interlock open.");
    else if (!strncmp(reason, "door-open", 9))
      message = _("Door open.");
    else if (!strncmp(reason, "input-tray-missing", 18))
      message = _("Media tray missing!");
    else if (!strncmp(reason, "media-low", 9))
      message = _("Media tray almost empty.");
    else if (!strncmp(reason, "media-empty", 11))
      message = _("Media tray empty!");
    else if (!strncmp(reason, "output-tray-missing", 19))
      message = _("Output tray missing!");
    else if (!strncmp(reason, "output-area-almost-full", 23))
      message = _("Output bin almost full.");
    else if (!strncmp(reason, "output-area-full", 16))
      message = _("Output bin full!");
    else if (!strncmp(reason, "marker-supply-low", 17))
      message = _("Ink/toner almost empty.");
    else if (!strncmp(reason, "marker-supply-empty", 19))
      message = _("Ink/toner empty!");
    else if (!strncmp(reason, "marker-waste-almost-full", 24))
      message = _("Ink/toner waste bin almost full.");
    else if (!strncmp(reason, "marker-waste-full", 17))
      message = _("Ink/toner waste bin full!");
    else if (!strncmp(reason, "fuser-over-temp", 15))
      message = _("Fuser temperature high!");
    else if (!strncmp(reason, "fuser-under-temp", 16))
      message = _("Fuser temperature low!");
    else if (!strncmp(reason, "opc-near-eol", 12))
      message = _("OPC almost at end-of-life.");
    else if (!strncmp(reason, "opc-life-over", 13))
      message = _("OPC at end-of-life!");
    else if (!strncmp(reason, "developer-low", 13))
      message = _("Developer almost empty.");
    else if (!strncmp(reason, "developer-empty", 15))
      message = _("Developer empty!");
    else if (!strcmp(reason, "com.apple.print.recoverable-warning"))
      saw_caprw = 1;
    else if (strstr(reason, "error") != NULL)
    {
      message = unknown;

      snprintf(unknown, sizeof(unknown), _("Unknown printer error (%s)!"),
               reason);
    }

    if (message[0])
    {
      count ++;
      if (strstr(reasons->values[i].string.text, "error"))
        fprintf(stderr, "ERROR: %s\n", _cupsLangString(language, message));
      else if (strstr(reasons->values[i].string.text, "warning"))
        fprintf(stderr, "WARNING: %s\n", _cupsLangString(language, message));
      else
        fprintf(stderr, "INFO: %s\n", _cupsLangString(language, message));
    }
  }

  fprintf(stderr, "%s\n", state);

 /*
  * Relay com.apple.print.recoverable-message...
  */

  if ((caprm = ippFindAttribute(ipp, "com.apple.print.recoverable-message",
                                IPP_TAG_TEXT)) != NULL)
    fprintf(stderr, "WARNING: %s: %s\n",
            saw_caprw ? "recoverable" : "recovered",
	    caprm->values[0].string.text);

 /*
  * Relay the current marker-* attribute values...
  */

  if ((marker = ippFindAttribute(ipp, "marker-colors", IPP_TAG_NAME)) != NULL)
    report_attr(marker);
  if ((marker = ippFindAttribute(ipp, "marker-levels",
                                 IPP_TAG_INTEGER)) != NULL)
    report_attr(marker);
  if ((marker = ippFindAttribute(ipp, "marker-message", IPP_TAG_TEXT)) != NULL)
    report_attr(marker);
  if ((marker = ippFindAttribute(ipp, "marker-names", IPP_TAG_NAME)) != NULL)
    report_attr(marker);
  if ((marker = ippFindAttribute(ipp, "marker-types", IPP_TAG_KEYWORD)) != NULL)
    report_attr(marker);

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

static int				/* O - Exit status of filter */
run_pictwps_filter(char       **argv,	/* I - Command-line arguments */
                   const char *filename)/* I - Filename */
{
  struct stat	fileinfo;		/* Print file information */
  const char	*ppdfile;		/* PPD file for destination printer */
  int		pid;			/* Child process ID */
  int		fd;			/* Temporary file descriptor */
  int		status;			/* Exit status of filter */
  const char	*printer;		/* PRINTER env var */
  static char	ppdenv[1024];		/* PPD environment variable */


 /*
  * First get the PPD file for the printer...
  */

  printer = getenv("PRINTER");
  if (!printer)
  {
    _cupsLangPuts(stderr,
                  _("ERROR: PRINTER environment variable not defined!\n"));
    return (-1);
  }

  if ((ppdfile = cupsGetPPD(printer)) == NULL)
  {
    _cupsLangPrintf(stderr,
		    _("ERROR: Unable to get PPD file for printer \"%s\" - "
		      "%s.\n"), printer, cupsLastErrorString());
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
    _cupsLangPrintf(stderr, _("ERROR: Unable to create temporary file - %s.\n"),
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
    _cupsLangPrintf(stderr, _("ERROR: Unable to exec pictwpstops: %s\n"),
		    strerror(errno));
    return (errno);
  }

  close(fd);

  if (pid < 0)
  {
   /*
    * Error!
    */

    _cupsLangPrintf(stderr, _("ERROR: Unable to fork pictwpstops: %s\n"),
		    strerror(errno));
    if (ppdfile)
      unlink(ppdfile);
    return (-1);
  }

 /*
  * Now wait for the filter to complete...
  */

  if (wait(&status) < 0)
  {
    _cupsLangPrintf(stderr, _("ERROR: Unable to wait for pictwpstops: %s\n"),
		    strerror(errno));
    close(fd);
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
      _cupsLangPrintf(stderr, _("ERROR: pictwpstops exited with status %d!\n"),
		      status / 256);
    else
      _cupsLangPrintf(stderr, _("ERROR: pictwpstops exited on signal %d!\n"),
		      status);

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

  if (!job_cancelled)
  {
   /*
    * Flag that the job should be cancelled...
    */

    job_cancelled = 1;
    return;
  }

 /*
  * The scheduler already tried to cancel us once, now just terminate
  * after removing our temp files!
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
