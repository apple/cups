/*
 * "$Id: ipp.c 7948 2008-09-17 00:04:12Z mike $"
 *
 *   IPP backend for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   check_printer_state()  - Check the printer state.
 *   compress_files()       - Compress print files...
 *   monitor_printer()      - Monitor the printer state...
 *   new_request()          - Create a new print creation or validation request.
 *   password_cb()          - Disable the password prompt for
 *                            cupsDoFileRequest().
 *   report_attr()          - Report an IPP attribute value.
 *   report_printer_state() - Report the printer state.
 *   sigterm_handler()      - Handle 'terminate' signals that stop the backend.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


/*
 * Types...
 */

typedef struct _cups_monitor_s		/**** Monitoring data ****/
{
  const char		*uri,		/* Printer URI */
			*hostname,	/* Hostname */
			*user,		/* Username */
			*resource;	/* Resource path */
  int			port,		/* Port number */
			version,	/* IPP version */
			job_id;		/* Job ID for submitted job */
  http_encryption_t	encryption;	/* Use encryption? */
  ipp_jstate_t		job_state;	/* Current job state */
  ipp_pstate_t		printer_state;	/* Current printer state */
} _cups_monitor_t;


/*
 * Globals...
 */

static const char *auth_info_required = "none";
					/* New auth-info-required value */
static const char * const jattrs[] =	/* Job attributes we want */
{
  "job-media-sheets-completed",
  "job-state",
  "job-state-reasons"
};
static int	job_canceled = 0;	/* Job cancelled? */
static char	*password = NULL;	/* Password for device URI */
static int	password_tries = 0;	/* Password tries */
static const char * const pattrs[] =	/* Printer attributes we want */
{
  "copies-supported",
  "cups-version",
  "document-format-supported",
  "marker-colors",
  "marker-high-levels",
  "marker-levels",
  "marker-low-levels",
  "marker-message",
  "marker-names",
  "marker-types",
  "media-col-supported",
  "operations-supported",
  "printer-alert",
  "printer-alert-description",
  "printer-is-accepting-jobs",
  "printer-state",
  "printer-state-message",
  "printer-state-reasons",
};


/*
 * Local functions...
 */

static void		cancel_job(http_t *http, const char *uri, int id,
			           const char *resource, const char *user,
				   int version);
static ipp_pstate_t	check_printer_state(http_t *http, const char *uri,
		                            const char *resource,
					    const char *user, int version,
					    int job_id);
#ifdef HAVE_LIBZ
static void		compress_files(int num_files, char **files);
#endif /* HAVE_LIBZ */
static void		*monitor_printer(_cups_monitor_t *monitor);
static ipp_t		*new_request(ipp_op_t op, int version, const char *uri,
			             const char *user, const char *title,
				     int num_options, cups_option_t *options,
				     const char *compression, int copies,
				     const char *format, _pwg_t *pwg,
				     ipp_attribute_t *media_col_sup);
static const char	*password_cb(const char *);
static void		report_attr(ipp_attribute_t *attr);
static int		report_printer_state(ipp_t *ipp, int job_id);
static void		sigterm_handler(int sig);


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
  char		scheme[255],		/* Scheme in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info */
		resource[1024],		/* Resource info (printer name) */
		addrname[256],		/* Address name */
		*optptr,		/* Pointer to URI options */
		*name,			/* Name of option */
		*value,			/* Value of option */
		sep;			/* Separator character */
  http_addrlist_t *addrlist;		/* Address of printer */
  int		snmp_fd,		/* SNMP socket */
		start_count,		/* Page count via SNMP at start */
		page_count,		/* Page count via SNMP */
		have_supplies;		/* Printer supports supply levels? */
  int		num_files;		/* Number of files to print */
  char		**files;		/* Files to print */
  int		port;			/* Port number (not used) */
  char		portname[255];		/* Port name */
  char		uri[HTTP_MAX_URI];	/* Updated URI without user/pass */
  http_status_t	http_status;		/* Status of HTTP request */
  ipp_status_t	ipp_status;		/* Status of IPP request */
  http_t	*http;			/* HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response,		/* IPP response */
		*supported;		/* get-printer-attributes response */
  time_t	start_time;		/* Time of first connect */
  int		contimeout;		/* Connection timeout */
  int		delay;			/* Delay for retries... */
  const char	*compression;		/* Compression mode */
  int		waitjob,		/* Wait for job complete? */
		waitprinter;		/* Wait for printer ready? */
  _cups_monitor_t monitor;		/* Monitoring data */
  ipp_attribute_t *job_id_attr;		/* job-id attribute */
  int		job_id;			/* job-id value */
  ipp_attribute_t *job_sheets;		/* job-media-sheets-completed */
  ipp_attribute_t *job_state;		/* job-state */
  ipp_attribute_t *copies_sup;		/* copies-supported */
  ipp_attribute_t *cups_version;	/* cups-version */
  ipp_attribute_t *format_sup;		/* document-format-supported */
  ipp_attribute_t *media_col_sup;	/* media-col-supported */
  ipp_attribute_t *operations_sup;	/* operations-supported */
  ipp_attribute_t *printer_state;	/* printer-state attribute */
  ipp_attribute_t *printer_accepting;	/* printer-is-accepting-jobs */
  int		validate_job;		/* Does printer support Validate-Job? */
  int		copies,			/* Number of copies for job */
		copies_remaining;	/* Number of copies remaining */
  const char	*content_type,		/* CONTENT_TYPE environment variable */
		*final_content_type,	/* FINAL_CONTENT_TYPE environment var */
		*document_format;	/* document-format value */
  int		fd;			/* File descriptor */
  off_t		bytes;			/* Bytes copied */
  char		buffer[16384];		/* Copy buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		version;		/* IPP version */
  ppd_file_t	*ppd;			/* PPD file */
  _pwg_t	*pwg;			/* PWG<->PPD mapping data */


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
                    _("Usage: %s job-id user title copies options [file]"),
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

  while ((device_uri = cupsBackendDeviceURI(argv)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "INFO", _("Unable to locate printer."));
    sleep(10);

    if (getenv("CLASS") != NULL)
      return (CUPS_BACKEND_FAILED);
  }

  httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme),
                  username, sizeof(username), hostname, sizeof(hostname), &port,
		  resource, sizeof(resource));

  if (!port)
    port = IPP_PORT;			/* Default to port 631 */

  if (!strcmp(scheme, "https"))
    cupsSetEncryption(HTTP_ENCRYPT_ALWAYS);
  else
    cupsSetEncryption(HTTP_ENCRYPT_IF_REQUESTED);

 /*
  * See if there are any options...
  */

  compression = NULL;
  version     = 20;
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
	  _cupsLangPrintFilter(stderr, "ERROR",
			       _("Unknown encryption option value: \"%s\"."),
			       value);
        }
      }
      else if (!strcasecmp(name, "version"))
      {
        if (!strcmp(value, "1.0"))
	  version = 10;
	else if (!strcmp(value, "1.1"))
	  version = 11;
	else if (!strcmp(value, "2.0"))
	  version = 20;
	else if (!strcmp(value, "2.1"))
	  version = 21;
	else if (!strcmp(value, "2.2"))
	  version = 22;
	else
	{
	  _cupsLangPrintFilter(stderr, "ERROR",
			       _("Unknown version option value: \"%s\"."),
			       value);
	}
      }
#ifdef HAVE_LIBZ
      else if (!strcasecmp(name, "compression"))
      {
        if (!strcasecmp(value, "true") || !strcasecmp(value, "yes") ||
	    !strcasecmp(value, "on") || !strcasecmp(value, "gzip"))
	  compression = "gzip";
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

	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("Unknown option \"%s\" with value \"%s\"."),
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
    num_files    = 0;
    send_options = !strcasecmp(final_content_type, "application/pdf") ||
                   !strcasecmp(final_content_type, "application/vnd.cups-pdf") ||
                   !strncasecmp(final_content_type, "image/", 6);

    fputs("DEBUG: Sending stdin for job...\n", stderr);
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

    fprintf(stderr, "DEBUG: %d files to send in job...\n", num_files);
  }

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
  * Try finding the remote server...
  */

  start_time = time(NULL);

  sprintf(portname, "%d", port);

  fputs("STATE: +connecting-to-device\n", stderr);
  fprintf(stderr, "DEBUG: Looking up \"%s\"...\n", hostname);

  while ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "INFO",
                         _("Unable to locate printer \"%s\"."), hostname);
    sleep(10);

    if (getenv("CLASS") != NULL)
    {
      fputs("STATE: -connecting-to-device\n", stderr);
      return (CUPS_BACKEND_STOP);
    }
  }

  http = _httpCreate(hostname, port, addrlist, cupsEncryption(), AF_UNSPEC);

 /*
  * See if the printer supports SNMP...
  */

  if ((snmp_fd = _cupsSNMPOpen(addrlist->addr.addr.sa_family)) >= 0)
  {
    have_supplies = !backendSNMPSupplies(snmp_fd, &(addrlist->addr),
                                         &start_count, NULL);
  }
  else
    have_supplies = start_count = 0;

 /*
  * Wait for data from the filter...
  */

  if (num_files == 0)
    if (!backendWaitLoop(snmp_fd, &(addrlist->addr), backendNetworkSideCB))
      return (CUPS_BACKEND_OK);

 /*
  * Try connecting to the remote server...
  */

  delay = 5;

  do
  {
    fprintf(stderr, "DEBUG: Connecting to %s:%d\n", hostname, port);
    _cupsLangPrintFilter(stderr, "INFO", _("Connecting to printer."));

    if (httpReconnect(http))
    {
      int error = errno;		/* Connection error */

      if (http->status == HTTP_PKI_ERROR)
	fputs("STATE: +cups-certificate-error\n", stderr);

      if (job_canceled)
	break;

      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        _cupsLangPrintFilter(stderr, "INFO",
			     _("Unable to contact printer, queuing on next "
			       "printer in class."));

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

	fputs("STATE: -connecting-to-device\n", stderr);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "DEBUG: Connection error: %s\n", strerror(errno));

      if (errno == ECONNREFUSED || errno == EHOSTDOWN ||
          errno == EHOSTUNREACH)
      {
        if (contimeout && (time(NULL) - start_time) > contimeout)
	{
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("The printer is not responding."));
	  fputs("STATE: -connecting-to-device\n", stderr);
	  return (CUPS_BACKEND_FAILED);
	}

	switch (error)
	{
	  case EHOSTDOWN :
	      _cupsLangPrintFilter(stderr, "WARNING",
			           _("Network printer \"%s\" may not exist or "
				     "is unavailable at this time."),
				   hostname);
	      break;

	  case EHOSTUNREACH :
	      _cupsLangPrintFilter(stderr, "WARNING",
				   _("Network printer \"%s\" is unreachable at "
				     "this time."), hostname);
	      break;

	  case ECONNREFUSED :
	  default :
	      _cupsLangPrintFilter(stderr, "WARNING",
				   _("Network printer \"%s\" is busy."),
			           hostname);
	      break;
        }

	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else
      {
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("Network printer \"%s\" is not responding."),
			     hostname);
	sleep(30);
      }

      if (job_canceled)
	break;
    }
    else
      fputs("STATE: -cups-certificate-error\n", stderr);
  }
  while (http->fd < 0);

  if (job_canceled || !http)
    return (CUPS_BACKEND_FAILED);

  fputs("STATE: -connecting-to-device\n", stderr);
  _cupsLangPrintFilter(stderr, "INFO", _("Connected to printer."));

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
  * Build a URI for the printer and fill the standard IPP attributes for
  * an IPP_PRINT_FILE request.  We can't use the URI in argv[0] because it
  * might contain username:password information...
  */

  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), scheme, NULL, hostname,
		  port, resource);

 /*
  * First validate the destination and see if the device supports multiple
  * copies...
  */

  copies_sup     = NULL;
  cups_version   = NULL;
  format_sup     = NULL;
  media_col_sup  = NULL;
  supported      = NULL;
  operations_sup = NULL;
  validate_job   = 0;

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
    request->request.op.version[0] = version / 10;
    request->request.op.version[1] = version % 10;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes", sizeof(pattrs) / sizeof(pattrs[0]),
		  NULL, pattrs);

   /*
    * Do the request...
    */

    fputs("DEBUG: Getting supported attributes...\n", stderr);

    if (http->version < HTTP_1_1)
    {
      fprintf(stderr, "DEBUG: Printer responded with HTTP version %d.%d.\n",
              http->version / 100, http->version % 100);

      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Unable to print: the printer does not conform to "
		             "the IPP standard."));
      exit(CUPS_BACKEND_STOP);
    }

    supported  = cupsDoRequest(http, request, resource);
    ipp_status = cupsLastError();

    if (ipp_status > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "DEBUG: Get-Printer-Attributes returned %s.\n",
              ippErrorString(ipp_status));

      if (ipp_status == IPP_PRINTER_BUSY ||
	  ipp_status == IPP_SERVICE_UNAVAILABLE)
      {
        if (contimeout && (time(NULL) - start_time) > contimeout)
	{
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("The printer is not responding."));
	  return (CUPS_BACKEND_FAILED);
	}

	_cupsLangPrintFilter(stderr, "WARNING",
			     _("Network host \"%s\" is busy; will retry in %d "
			       "seconds."), hostname, delay);

        report_printer_state(supported, 0);

	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else if ((ipp_status == IPP_BAD_REQUEST ||
	        ipp_status == IPP_VERSION_NOT_SUPPORTED) && version > 10)
      {
       /*
	* Switch to IPP/1.1 or IPP/1.0...
	*/

        if (version >= 20)
	{
	  _cupsLangPrintFilter(stderr, "INFO",
			       _("Printer does not support IPP/%d.%d, trying "
			         "IPP/%s."), version / 10, version % 10, "1.1");
	  version = 11;
	}
	else
	{
	  _cupsLangPrintFilter(stderr, "INFO",
			       _("Printer does not support IPP/%d.%d, trying "
			         "IPP/%s."), version / 10, version % 10, "1.0");
	  version = 10;
        }

	httpReconnect(http);
      }
      else if (ipp_status == IPP_NOT_FOUND)
      {
        _cupsLangPrintFilter(stderr, "ERROR",
			     _("The printer URI is incorrect or no longer "
		               "exists."));

	if (supported)
          ippDelete(supported);

	return (CUPS_BACKEND_STOP);
      }
      else if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN)
      {
	if (!strncmp(httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE),
		     "Negotiate", 9))
	  auth_info_required = "negotiate";

	fprintf(stderr, "ATTR: auth-info-required=%s\n", auth_info_required);
	return (CUPS_BACKEND_AUTH_REQUIRED);
      }
      else
      {
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("Unable to get printer status: %s"),
			     cupsLastErrorString());
        sleep(10);
      }

      if (supported)
        ippDelete(supported);

      continue;
    }

   /*
    * Check for supported attributes...
    */

    if ((copies_sup = ippFindAttribute(supported, "copies-supported",
	                               IPP_TAG_RANGE)) != NULL)
    {
     /*
      * Has the "copies-supported" attribute - does it have an upper
      * bound > 1?
      */

      fprintf(stderr, "DEBUG: copies-supported=%d-%d\n",
	      copies_sup->values[0].range.lower,
	      copies_sup->values[0].range.upper);

      if (copies_sup->values[0].range.upper <= 1)
	copies_sup = NULL; /* No */
    }

    cups_version = ippFindAttribute(supported, "cups-version", IPP_TAG_TEXT);

    if ((format_sup = ippFindAttribute(supported, "document-format-supported",
	                               IPP_TAG_MIMETYPE)) != NULL)
    {
      fprintf(stderr, "DEBUG: document-format-supported (%d values)\n",
	      format_sup->num_values);
      for (i = 0; i < format_sup->num_values; i ++)
	fprintf(stderr, "DEBUG: [%d] = \"%s\"\n", i,
	        format_sup->values[i].string.text);
    }

    if ((media_col_sup = ippFindAttribute(supported, "media-col-supported",
	                                  IPP_TAG_KEYWORD)) != NULL)
    {
      fprintf(stderr, "DEBUG: media-col-supported (%d values)\n",
	      media_col_sup->num_values);
      for (i = 0; i < media_col_sup->num_values; i ++)
	fprintf(stderr, "DEBUG: [%d] = \"%s\"\n", i,
	        media_col_sup->values[i].string.text);
    }

    if ((operations_sup = ippFindAttribute(supported, "operations-supported",
					   IPP_TAG_ENUM)) != NULL)
    {
      for (i = 0; i < operations_sup->num_values; i ++)
        if (operations_sup->values[i].integer == IPP_VALIDATE_JOB)
	{
	  validate_job = 1;
	  break;
	}

      if (!validate_job)
      {
        _cupsLangPrintFilter(stderr, "WARNING",
	                     _("This printer does not conform to the IPP "
			       "standard and may not work."));
        fputs("DEBUG: operations-supported does not list Validate-Job.\n",
	      stderr);
      }
    }
    else
    {
      _cupsLangPrintFilter(stderr, "WARNING",
			   _("This printer does not conform to the IPP "
			     "standard and may not work."));
      fputs("DEBUG: operations-supported not returned in "
            "Get-Printer-Attributes request.\n", stderr);
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

      _cupsLangPrintFilter(stderr, "INFO",
                           _("Unable to contact printer, queuing on next "
		             "printer in class."));

      ippDelete(supported);
      httpClose(http);

     /*
      * Sleep 5 seconds to keep the job from requeuing too rapidly...
      */

      sleep(5);

      return (CUPS_BACKEND_FAILED);
    }
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
  * Prepare remaining printing options...
  */

  options = NULL;
  pwg     = NULL;

  if (send_options)
  {
    num_options = cupsParseOptions(argv[5], 0, &options);

    if (!cups_version && media_col_sup)
    {
     /*
      * Load the PPD file and generate PWG attribute mapping information...
      */

      ppd = ppdOpenFile(getenv("PPD"));
      pwg = _pwgCreateWithPPD(ppd);

      ppdClose(ppd);
    }
  }
  else
    num_options = 0;

  document_format = NULL;

  if (format_sup != NULL)
  {
    for (i = 0; i < format_sup->num_values; i ++)
      if (!strcasecmp(final_content_type, format_sup->values[i].string.text))
      {
        document_format = final_content_type;
	break;
      }

    if (!document_format)
    {
      for (i = 0; i < format_sup->num_values; i ++)
	if (!strcasecmp("application/octet-stream",
	                format_sup->values[i].string.text))
	{
	  document_format = "application/octet-stream";
	  break;
	}
    }
  }

 /*
  * Start monitoring the printer in the background...
  */

  monitor.uri           = uri;
  monitor.hostname      = hostname;
  monitor.user          = argv[2];
  monitor.resource      = resource;
  monitor.port          = port;
  monitor.version       = version;
  monitor.job_id        = 0;
  monitor.encryption    = cupsEncryption();
  monitor.job_state     = IPP_JOB_PENDING;
  monitor.printer_state = IPP_PRINTER_IDLE;

  _cupsThreadCreate((_cups_thread_func_t)monitor_printer, &monitor);

 /*
  * Validate access to the printer...
  */

  while (!job_canceled)
  {
    request = new_request(IPP_VALIDATE_JOB, version, uri, argv[2], argv[3],
                          num_options, options, compression,
			  copies_sup ? copies : 1, document_format, pwg,
			  media_col_sup);

    ippDelete(cupsDoRequest(http, request, resource));

    ipp_status = cupsLastError();

    if (ipp_status > IPP_OK_CONFLICT &&
        ipp_status != IPP_OPERATION_NOT_SUPPORTED)
    {
      if (job_canceled)
        break;

      if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	  ipp_status == IPP_PRINTER_BUSY)
      {
        _cupsLangPrintFilter(stderr, "INFO",
			     _("Printer busy; will retry in 10 seconds."));
	sleep(10);
      }
      else
      {
       /*
	* Update auth-info-required as needed...
	*/

        _cupsLangPrintFilter(stderr, "ERROR", "%s", cupsLastErrorString());

	if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN)
	{
	  fprintf(stderr, "DEBUG: WWW-Authenticate=\"%s\"\n",
		  httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE));

         /*
	  * Normal authentication goes through the password callback, which sets
	  * auth_info_required to "username,password".  Kerberos goes directly
	  * through GSSAPI, so look for Negotiate in the WWW-Authenticate header
	  * here and set auth_info_required as needed...
	  */

	  if (!strncmp(httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE),
		       "Negotiate", 9))
	    auth_info_required = "negotiate";
	}

	goto cleanup;
      }
    }
    else
      break;
  }

 /*
  * Then issue the print-job request...
  */

  job_id = 0;

  while (!job_canceled && copies_remaining > 0)
  {
   /*
    * Check for side-channel requests...
    */

    backendCheckSideChannel(snmp_fd, http->hostaddr);

   /*
    * Build the IPP job creation request...
    */

    if (job_canceled)
      break;

    request = new_request(num_files > 1 ? IPP_CREATE_JOB : IPP_PRINT_JOB,
			  version, uri, argv[2], argv[3], num_options, options,
			  compression, copies_sup ? copies : 1, document_format,
			  pwg, media_col_sup);

   /*
    * Do the request...
    */

    if (num_files > 1)
      response = cupsDoRequest(http, request, resource);
    else
    {
      fputs("DEBUG: Sending file using chunking...\n", stderr);
      http_status = cupsSendRequest(http, request, resource, 0);
      if (http_status == HTTP_CONTINUE && request->state == IPP_DATA)
      {
        if (num_files == 1)
	  fd = open(files[0], O_RDONLY);
	else
	  fd = 0;

        while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	{
	  fprintf(stderr, "DEBUG: Read %d bytes...\n", (int)bytes);

	  if (cupsWriteRequestData(http, buffer, bytes) != HTTP_CONTINUE)
            break;
          else
	  {
	   /*
	    * Check for side-channel requests...
	    */

	    backendCheckSideChannel(snmp_fd, http->hostaddr);
	  }
	}

        if (num_files == 1)
	  close(fd);
      }

      response = cupsGetResponse(http, resource);
      ippDelete(request);
    }

    ipp_status = cupsLastError();

    if (ipp_status > IPP_OK_CONFLICT)
    {
      job_id = 0;

      if (job_canceled)
        break;

      if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	  ipp_status == IPP_PRINTER_BUSY)
      {
        _cupsLangPrintFilter(stderr, "INFO",
			     _("Printer busy; will retry in 10 seconds."));
	sleep(10);
      }
      else
      {
       /*
	* Update auth-info-required as needed...
	*/

        _cupsLangPrintFilter(stderr, "ERROR",
	                     _("Print file was not accepted: %s"),
			     cupsLastErrorString());

	if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN)
	{
	  fprintf(stderr, "DEBUG: WWW-Authenticate=\"%s\"\n",
		  httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE));

         /*
	  * Normal authentication goes through the password callback, which sets
	  * auth_info_required to "username,password".  Kerberos goes directly
	  * through GSSAPI, so look for Negotiate in the WWW-Authenticate header
	  * here and set auth_info_required as needed...
	  */

	  if (!strncmp(httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE),
		       "Negotiate", 9))
	    auth_info_required = "negotiate";
	}
      }
    }
    else if ((job_id_attr = ippFindAttribute(response, "job-id",
                                             IPP_TAG_INTEGER)) == NULL)
    {
      _cupsLangPrintFilter(stderr, "INFO",
			   _("Print file accepted - job ID unknown."));
      job_id = 0;
    }
    else
    {
      monitor.job_id = job_id = job_id_attr->values[0].integer;
      _cupsLangPrintFilter(stderr, "INFO",
                           _("Print file accepted - job ID %d."), job_id);
    }

    ippDelete(response);

    if (job_canceled)
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
	request->request.op.version[0] = version / 10;
	request->request.op.version[1] = version % 10;

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

	fprintf(stderr, "DEBUG: Sending file %d using chunking...\n", i + 1);
	http_status = cupsSendRequest(http, request, resource, 0);
	if (http_status == HTTP_CONTINUE && request->state == IPP_DATA &&
	    (fd = open(files[i], O_RDONLY)) >= 0)
	{
	  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	  {
	    if (cupsWriteRequestData(http, buffer, bytes) != HTTP_CONTINUE)
	      break;
	    else
	    {
	     /*
	      * Check for side-channel requests...
	      */

	      backendCheckSideChannel(snmp_fd, http->hostaddr);
	    }
	  }

	  close(fd);
	}

	ippDelete(cupsGetResponse(http, resource));
	ippDelete(request);

	if (cupsLastError() > IPP_OK_CONFLICT)
	{
	  ipp_status = cupsLastError();

	  _cupsLangPrintFilter(stderr, "ERROR",
			       _("Unable to add file to job: %s"),
			       cupsLastErrorString());
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

    _cupsLangPrintFilter(stderr, "INFO", _("Waiting for job to complete."));

    for (delay = 1; !job_canceled;)
    {
     /*
      * Check for side-channel requests...
      */

      backendCheckSideChannel(snmp_fd, http->hostaddr);

     /*
      * Build an IPP_GET_JOB_ATTRIBUTES request...
      */

      request = ippNewRequest(IPP_GET_JOB_ATTRIBUTES);
      request->request.op.version[0] = version / 10;
      request->request.op.version[1] = version % 10;

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

          _cupsLangPrintFilter(stderr, "ERROR",
			       _("Unable to get job attributes: %s"),
			       cupsLastErrorString());
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
	else
	{
	 /*
	  * If the printer does not return a job-state attribute, it does not
	  * conform to the IPP specification - break out immediately and fail
	  * the job...
	  */

          fputs("DEBUG: No job-state available from printer - stopping queue.\n",
	        stderr);
	  ipp_status = IPP_INTERNAL_ERROR;
	  break;
	}
      }

      ippDelete(response);

#if 0
     /*
      * Check the printer state and report it if necessary...
      */

      check_printer_state(http, uri, resource, argv[2], version, job_id);
#endif /* 0 */

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

  if (job_canceled && job_id)
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

#ifdef HAVE_GSSAPI
 /*
  * See if we used Kerberos at all...
  */

  if (http->gssctx)
    auth_info_required = "negotiate";
#endif /* HAVE_GSSAPI */

 /*
  * Free memory...
  */

  cleanup:

  cupsFreeOptions(num_options, options);
  _pwgDestroy(pwg);

  httpClose(http);

  ippDelete(supported);

 /*
  * Remove the temporary file(s) if necessary...
  */

#ifdef HAVE_LIBZ
  if (compression)
  {
    for (i = 0; i < num_files; i ++)
      unlink(files[i]);
  }
#endif /* HAVE_LIBZ */

 /*
  * Return the queue status...
  */

  fprintf(stderr, "ATTR: auth-info-required=%s\n", auth_info_required);

  if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN)
    return (CUPS_BACKEND_AUTH_REQUIRED);
  else if (ipp_status == IPP_INTERNAL_ERROR)
    return (CUPS_BACKEND_STOP);
  else if (ipp_status > IPP_OK_CONFLICT)
    return (CUPS_BACKEND_FAILED);
  else
  {
    _cupsLangPrintFilter(stderr, "INFO", _("Ready to print."));
    return (CUPS_BACKEND_OK);
  }
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


  _cupsLangPrintFilter(stderr, "INFO", _("Canceling print job."));

  request = ippNewRequest(IPP_CANCEL_JOB);
  request->request.op.version[0] = version / 10;
  request->request.op.version[1] = version % 10;

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
    _cupsLangPrintFilter(stderr, "ERROR", _("Unable to cancel job: %s"),
		         cupsLastErrorString());
}


/*
 * 'check_printer_state()' - Check the printer state.
 */

static ipp_pstate_t			/* O - Current printer-state */
check_printer_state(
    http_t      *http,			/* I - HTTP connection */
    const char  *uri,			/* I - Printer URI */
    const char  *resource,		/* I - Resource path */
    const char  *user,			/* I - Username, if any */
    int         version,		/* I - IPP version */
    int         job_id)
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Attribute in response */
  ipp_pstate_t	printer_state = IPP_PRINTER_STOPPED;
					/* Current printer-state */


 /*
  * Send a Get-Printer-Attributes request and log the results...
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  request->request.op.version[0] = version / 10;
  request->request.op.version[1] = version % 10;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
	       NULL, uri);

  if (user && user[0])
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		 "requesting-user-name", NULL, user);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
		"requested-attributes",
		(int)(sizeof(pattrs) / sizeof(pattrs[0])), NULL, pattrs);

  if ((response = cupsDoRequest(http, request, resource)) != NULL)
  {
    report_printer_state(response, job_id);

    if ((attr = ippFindAttribute(response, "printer-state",
				 IPP_TAG_ENUM)) != NULL)
      printer_state = (ipp_pstate_t)attr->values[0].integer;

    ippDelete(response);
  }

 /*
  * Return the printer-state value...
  */

  return (printer_state);
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
      _cupsLangPrintError("ERROR", _("Unable to create compressed print file"));
      exit(CUPS_BACKEND_FAILED);
    }

    if ((out = cupsFileOpenFd(fd, "w9")) == NULL)
    {
      _cupsLangPrintError("ERROR", _("Unable to open compressed print file"));
      exit(CUPS_BACKEND_FAILED);
    }

    if ((in = cupsFileOpen(files[i], "r")) == NULL)
    {
      _cupsLangPrintError("ERROR", _("Unable to open print file"));
      cupsFileClose(out);
      exit(CUPS_BACKEND_FAILED);
    }

    total = 0;
    while ((bytes = cupsFileRead(in, buffer, sizeof(buffer))) > 0)
      if (cupsFileWrite(out, buffer, bytes) < bytes)
      {
	_cupsLangPrintError("ERROR",
	                    _("Unable to generate compressed print file"));
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
 * 'monitor_printer()' - Monitor the printer state...
 */

static void *				/* O - Thread exit code */
monitor_printer(
    _cups_monitor_t *monitor)		/* I - Monitoring data */
{
  http_t	*http;			/* Connection to printer */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Attribute in response */
  int		delay,			/* Current delay */
		prev_delay,		/* Previous delay */
		temp_delay;		/* Temporary delay value */


 /*
  * Make a copy of the printer connection...
  */

  http = _httpCreate(monitor->hostname, monitor->port, NULL, monitor->encryption,
                     AF_UNSPEC);
  cupsSetPasswordCB(password_cb);

 /*
  * Loop until the job is canceled, aborted, or completed.
  */

  delay      = 1;
  prev_delay = 0;

  while (monitor->job_state < IPP_JOB_CANCELED && !job_canceled)
  {
   /*
    * Reconnect to the printer...
    */

    if (!httpReconnect(http))
    {
     /*
      * Connected, so check on the printer state...
      */

      monitor->printer_state = check_printer_state(http, monitor->uri,
                                                   monitor->resource,
						   monitor->user,
						   monitor->version,
						   monitor->job_id);

      if (monitor->job_id > 0)
      {
       /*
        * Check the status of the job itself...
	*/

	request = ippNewRequest(IPP_GET_JOB_ATTRIBUTES);
	request->request.op.version[0] = monitor->version / 10;
	request->request.op.version[1] = monitor->version % 10;

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
		     NULL, monitor->uri);
	ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
	              monitor->job_id);

	if (monitor->user && monitor->user[0])
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		       "requesting-user-name", NULL, monitor->user);

	ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
		      "requested-attributes",
		      (int)(sizeof(jattrs) / sizeof(jattrs[0])), NULL, jattrs);

       /*
	* Do the request...
	*/

	response = cupsDoRequest(http, request, monitor->resource);

	if ((attr = ippFindAttribute(response, "job-state",
				     IPP_TAG_ENUM)) != NULL)
	  monitor->job_state = (ipp_jstate_t)attr->values[0].integer;
	else
	  monitor->job_state = IPP_JOB_COMPLETED;

	ippDelete(response);
      }

     /*
      * Disconnect from the printer - we'll reconnect on the next poll...
      */

      _httpDisconnect(http);
    }

   /*
    * Sleep for N seconds, and then update the next sleep time using a
    * Fibonacci series (1 1 2 3 5 8)...
    */

    sleep(delay);

    temp_delay = delay;
    delay      = (delay + prev_delay) % 12;
    prev_delay = delay < temp_delay ? 0 : temp_delay;
  }

 /*
  * Cleanup and return...
  */

  httpClose(http);

  return (NULL);
}


/*
 * 'new_request()' - Create a new print creation or validation request.
 */

static ipp_t *				/* O - Request data */
new_request(
    ipp_op_t        op,			/* I - IPP operation code */
    int             version,		/* I - IPP version number */
    const char      *uri,		/* I - printer-uri value */
    const char      *user,		/* I - requesting-user-name value */
    const char      *title,		/* I - job-name value */
    int             num_options,	/* I - Number of options to send */
    cups_option_t   *options,		/* I - Options to send */
    const char      *compression,	/* I - compression value or NULL */
    int             copies,		/* I - copies value or 0 */
    const char      *format,		/* I - documet-format value or NULL */
    _pwg_t          *pwg,		/* I - PWG<->PPD mapping data */
    ipp_attribute_t *media_col_sup)	/* I - media-col-supported values */
{
  int		i;			/* Looping var */
  ipp_t		*request;		/* Request data */
  const char	*keyword;		/* PWG keyword */
  _pwg_size_t	*size;			/* PWG media size */
  ipp_t		*media_col,		/* media-col value */
		*media_size;		/* media-size value */
  const char	*media_source,		/* media-source value */
		*media_type;		/* media-type value */


 /*
  * Create the IPP request...
  */

  request                        = ippNewRequest(op);
  request->request.op.version[0] = version / 10;
  request->request.op.version[1] = version % 10;

  fprintf(stderr, "DEBUG: %s IPP/%d.%d\n",
	  ippOpString(request->request.op.operation_id),
	  request->request.op.version[0],
	  request->request.op.version[1]);

 /*
  * Add standard attributes...
  */

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
	       NULL, uri);
  fprintf(stderr, "DEBUG: printer-uri=\"%s\"\n", uri);

  if (user && *user)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
		 "requesting-user-name", NULL, user);
    fprintf(stderr, "DEBUG: requesting-user-name=\"%s\"\n", user);
  }

  if (title && *title)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
		 title);
    fprintf(stderr, "DEBUG: job-name=\"%s\"\n", title);
  }

  if (format)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
		 "document-format", NULL, format);
    fprintf(stderr, "DEBUG: document-format=\"%s\"\n", format);
  }

#ifdef HAVE_LIBZ
  if (compression)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
		 "compression", NULL, compression);
    fprintf(stderr, "DEBUG: compression=\"%s\"\n", compression);
  }
#endif /* HAVE_LIBZ */

 /*
  * Handle options on the command-line...
  */

  if (num_options > 0)
  {
    if (pwg)
    {
     /*
      * Send standard IPP attributes...
      */

      if ((keyword = cupsGetOption("PageSize", num_options, options)) == NULL)
	keyword = cupsGetOption("media", num_options, options);

      if ((size = _pwgGetSize(pwg, keyword)) != NULL)
      {
       /*
        * Add a media-col value...
	*/

	media_size = ippNew();
	ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER,
		      "x-dimension", size->width);
	ippAddInteger(media_size, IPP_TAG_ZERO, IPP_TAG_INTEGER,
		      "y-dimension", size->length);

	media_col = ippNew();
	ippAddCollection(media_col, IPP_TAG_ZERO, "media-size", media_size);

	media_source = _pwgGetSource(pwg, cupsGetOption("InputSlot",
							num_options,
							options));
	media_type   = _pwgGetType(pwg, cupsGetOption("MediaType",
						      num_options, options));

	for (i = 0; i < media_col_sup->num_values; i ++)
	{
	  if (!strcmp(media_col_sup->values[i].string.text,
		      "media-left-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-left-margin", size->left);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-bottom-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-bottom-margin", size->left);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-right-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-right-margin", size->left);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-top-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-top-margin", size->left);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-source") && media_source)
	    ippAddString(media_col, IPP_TAG_ZERO, IPP_TAG_KEYWORD,
			 "media-source", NULL, media_source);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-type") && media_type)
	    ippAddString(media_col, IPP_TAG_ZERO, IPP_TAG_KEYWORD,
			 "media-type", NULL, media_type);
	}

	ippAddCollection(request, IPP_TAG_JOB, "media-col", media_col);
      }

      if ((keyword = cupsGetOption("output-bin", num_options,
				   options)) == NULL)
	keyword = _pwgGetBin(pwg, cupsGetOption("OutputBin", num_options,
						options));

      if (keyword)
	ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "output-bin",
		     NULL, keyword);

      if ((keyword = cupsGetOption("output-mode", num_options,
				   options)) != NULL)
	ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "output-mode",
		     NULL, keyword);
      else if ((keyword = cupsGetOption("ColorModel", num_options,
					options)) != NULL)
      {
	if (!strcasecmp(keyword, "Gray"))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "output-mode",
			       NULL, "monochrome");
	else
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "output-mode",
			   NULL, "color");
      }

      if ((keyword = cupsGetOption("print-quality", num_options,
				   options)) != NULL)
	ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
		      atoi(keyword));
      else if ((keyword = cupsGetOption("cupsPrintQuality", num_options,
					options)) != NULL)
      {
	if (!strcasecmp(keyword, "draft"))
	  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
			IPP_QUALITY_DRAFT);
	else if (!strcasecmp(keyword, "normal"))
	  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
			IPP_QUALITY_NORMAL);
	else if (!strcasecmp(keyword, "high"))
	  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
			IPP_QUALITY_HIGH);
      }

      if ((keyword = cupsGetOption("sides", num_options, options)) != NULL)
	ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		     NULL, keyword);
      else if (pwg->sides_option &&
               (keyword = cupsGetOption(pwg->sides_option, num_options,
					options)) != NULL)
      {
	if (!strcasecmp(keyword, pwg->sides_1sided))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		       NULL, "one-sided");
	else if (!strcasecmp(keyword, pwg->sides_2sided_long))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		       NULL, "two-sided-long-edge");
	if (!strcasecmp(keyword, pwg->sides_2sided_short))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		       NULL, "two-sided-short-edge");
      }
    }
    else
    {
     /*
      * When talking to another CUPS server, send all options...
      */

      cupsEncodeOptions(request, num_options, options);
    }

    if (copies > 1)
      ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies", copies);
  }

  return (request);
}


/*
 * 'password_cb()' - Disable the password prompt for cupsDoFileRequest().
 */

static const char *			/* O - Password  */
password_cb(const char *prompt)		/* I - Prompt (not used) */
{
  (void)prompt;

 /*
  * Remember that we need to authenticate...
  */

  auth_info_required = "username,password";

  if (password && *password && password_tries < 3)
  {
    password_tries ++;

    return (password);
  }
  else
  {
   /*
    * Give up after 3 tries or if we don't have a password to begin with...
    */

    return (NULL);
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
  ipp_attribute_t	*pa,		/* printer-alert */
			*pam,		/* printer-alert-message */
			*psm,		/* printer-state-message */
			*reasons,	/* printer-state-reasons */
			*marker;	/* marker-* attributes */
  const char		*reason;	/* Current reason */
  const char		*prefix;	/* Prefix for STATE: line */
  char			value[1024],	/* State/message string */
			*valptr;	/* Pointer into string */
  static int		ipp_supplies = -1;
					/* Report supply levels? */


 /*
  * Report alerts and messages...
  */

  if ((pa = ippFindAttribute(ipp, "printer-alert", IPP_TAG_TEXT)) != NULL)
    report_attr(pa);

  if ((pam = ippFindAttribute(ipp, "printer-alert-message",
                              IPP_TAG_TEXT)) != NULL)
    report_attr(pam);

  if ((psm = ippFindAttribute(ipp, "printer-state-message",
                              IPP_TAG_TEXT)) != NULL)
  {
    char	*ptr;			/* Pointer into message */


    strlcpy(value, "INFO: ", sizeof(value));
    for (ptr = psm->values[0].string.text, valptr = value + 6;
         *ptr && valptr < (value + sizeof(value) - 6);
	 ptr ++)
    {
      if (*ptr < ' ' && *ptr > 0 && *ptr != '\t')
      {
       /*
        * Substitute "<XX>" for the control character; sprintf is safe because
	* we always leave 6 chars free at the end...
	*/

        sprintf(valptr, "<%02X>", *ptr);
	valptr += 4;
      }
      else
        *valptr++ = *ptr;
    }

    *valptr++ = '\n';
    *valptr   = '\0';

    fputs(value, stderr);
  }

 /*
  * Now report printer-state-reasons, filtering out some of the reasons we never
  * want to set...
  */

  if ((reasons = ippFindAttribute(ipp, "printer-state-reasons",
                                  IPP_TAG_KEYWORD)) == NULL)
    return (0);

  value[0] = '\0';
  prefix   = "STATE: ";

  for (i = 0, count = 0, valptr = value; i < reasons->num_values; i ++)
  {
    reason = reasons->values[i].string.text;

    if (strcmp(reason, "paused") &&
	strcmp(reason, "com.apple.print.recoverable-warning"))
    {
      strlcpy(valptr, prefix, sizeof(value) - (valptr - value) - 1);
      valptr += strlen(valptr);
      strlcpy(valptr, reason, sizeof(value) - (valptr - value) - 1);
      valptr += strlen(valptr);

      prefix  = ",";
    }
  }

  if (value[0])
  {
    *valptr++ = '\n';
    *valptr   = '\0';
    fputs(value, stderr);
  }

 /*
  * Relay the current marker-* attribute values...
  */

  if (ipp_supplies < 0)
  {
    ppd_file_t	*ppd;			/* PPD file */
    ppd_attr_t	*ppdattr;		/* Attribute in PPD file */

    if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL &&
        (ppdattr = ppdFindAttr(ppd, "cupsIPPSupplies", NULL)) != NULL &&
        ppdattr->value && strcasecmp(ppdattr->value, "true"))
      ipp_supplies = 0;
    else
      ipp_supplies = 1;

    ppdClose(ppd);
  }

  if (ipp_supplies > 0)
  {
    if ((marker = ippFindAttribute(ipp, "marker-colors", IPP_TAG_NAME)) != NULL)
      report_attr(marker);
    if ((marker = ippFindAttribute(ipp, "marker-high-levels",
                                   IPP_TAG_INTEGER)) != NULL)
      report_attr(marker);
    if ((marker = ippFindAttribute(ipp, "marker-levels",
                                   IPP_TAG_INTEGER)) != NULL)
      report_attr(marker);
    if ((marker = ippFindAttribute(ipp, "marker-low-levels",
                                   IPP_TAG_INTEGER)) != NULL)
      report_attr(marker);
    if ((marker = ippFindAttribute(ipp, "marker-message",
                                   IPP_TAG_TEXT)) != NULL)
      report_attr(marker);
    if ((marker = ippFindAttribute(ipp, "marker-names", IPP_TAG_NAME)) != NULL)
      report_attr(marker);
    if ((marker = ippFindAttribute(ipp, "marker-types",
                                   IPP_TAG_KEYWORD)) != NULL)
      report_attr(marker);
  }

  return (count);
}


/*
 * 'sigterm_handler()' - Handle 'terminate' signals that stop the backend.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
  (void)sig;	/* remove compiler warnings... */

  if (!job_canceled)
  {
   /*
    * Flag that the job should be cancelled...
    */

    job_canceled = 1;
    return;
  }

  exit(1);
}


/*
 * End of "$Id: ipp.c 7948 2008-09-17 00:04:12Z mike $".
 */
