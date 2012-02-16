/*
 * "$Id: ipp.c 9759 2011-05-11 03:24:33Z mike $"
 *
 *   IPP backend for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
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
 *   compress_files()       - Compress print files.
 *   monitor_printer()      - Monitor the printer state.
 *   new_request()          - Create a new print creation or validation request.
 *   password_cb()          - Disable the password prompt for
 *                            cupsDoFileRequest().
 *   report_attr()          - Report an IPP attribute value.
 *   report_printer_state() - Report the printer state.
 *   run_as_user()          - Run the IPP backend as the printing user.
 *   timeout_cb()           - Handle HTTP timeouts.
 *   sigterm_handler()      - Handle 'terminate' signals that stop the backend.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#include <cups/array-private.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#if defined(HAVE_GSSAPI) && defined(HAVE_XPC)
#  include <xpc/xpc.h>
#  define kPMPrintUIToolAgent	"com.apple.printuitool.agent"
#  define kPMStartJob		100
#  define kPMWaitForJob		101
extern void	xpc_connection_set_target_uid(xpc_connection_t connection,
		                              uid_t uid);
#endif /* HAVE_GSSAPI && HAVE_XPC */


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
  const char		*job_name;	/* Job name for submitted job */
  http_encryption_t	encryption;	/* Use encryption? */
  ipp_jstate_t		job_state;	/* Current job state */
  ipp_pstate_t		printer_state;	/* Current printer state */
} _cups_monitor_t;


/*
 * Globals...
 */

static const char 	*auth_info_required;
					/* New auth-info-required value */
#if defined(HAVE_GSSAPI) && defined(HAVE_XPC)
static int		child_pid = 0;	/* Child process ID */
#endif /* HAVE_GSSAPI && HAVE_XPC */
static const char * const jattrs[] =	/* Job attributes we want */
{
  "job-impressions-completed",
  "job-media-sheets-completed",
  "job-name",
  "job-originating-user-name",
  "job-state",
  "job-state-reasons"
};
static int		job_canceled = 0;
					/* Job cancelled? */
static char		username[256] = "",
					/* Username for device URI */
			*password = NULL;
					/* Password for device URI */
static int		password_tries = 0;
					/* Password tries */
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
  "multiple-document-handling-supported",
  "operations-supported",
  "printer-alert",
  "printer-alert-description",
  "printer-is-accepting-jobs",
  "printer-state",
  "printer-state-message",
  "printer-state-reasons"
};
static const char * const remote_job_states[] =
{					/* Remote job state keywords */
  "+cups-remote-pending",
  "+cups-remote-pending-held",
  "+cups-remote-processing",
  "+cups-remote-stopped",
  "+cups-remote-canceled",
  "+cups-remote-aborted",
  "+cups-remote-completed"
};
static _cups_mutex_t	report_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex to control access */
static int		num_attr_cache = 0;
					/* Number of cached attributes */
static cups_option_t	*attr_cache = NULL;
					/* Cached attributes */
static cups_array_t	*state_reasons;	/* Array of printe-state-reasons keywords */
static char		tmpfilename[1024] = "";
					/* Temporary spool file name */


/*
 * Local functions...
 */

static void		cancel_job(http_t *http, const char *uri, int id,
			           const char *resource, const char *user,
				   int version);
static ipp_pstate_t	check_printer_state(http_t *http, const char *uri,
		                            const char *resource,
					    const char *user, int version);
#ifdef HAVE_LIBZ
static void		compress_files(int num_files, char **files);
#endif /* HAVE_LIBZ */
static void		*monitor_printer(_cups_monitor_t *monitor);
static ipp_t		*new_request(ipp_op_t op, int version, const char *uri,
			             const char *user, const char *title,
				     int num_options, cups_option_t *options,
				     const char *compression, int copies,
				     const char *format, _ppd_cache_t *pc,
				     ipp_attribute_t *media_col_sup,
				     ipp_attribute_t *doc_handling_sup);
static const char	*password_cb(const char *);
static void		report_attr(ipp_attribute_t *attr);
static void		report_printer_state(ipp_t *ipp);
#if defined(HAVE_GSSAPI) && defined(HAVE_XPC)
static int		run_as_user(int argc, char *argv[], uid_t uid,
			            const char *device_uri, int fd);
#endif /* HAVE_GSSAPI && HAVE_XPC */
static void		sigterm_handler(int sig);
static int		timeout_cb(http_t *http, void *user_data);
static void		update_reasons(ipp_attribute_t *attr, const char *s);


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
  char		**files,		/* Files to print */
		*compatfile = NULL;	/* Compatibility filename */
  off_t		compatsize = 0;		/* Size of compatibility file */
  int		port;			/* Port number (not used) */
  char		portname[255];		/* Port name */
  char		uri[HTTP_MAX_URI];	/* Updated URI without user/pass */
  char		print_job_name[1024];	/* Update job-name for Print-Job */
  http_status_t	http_status;		/* Status of HTTP request */
  ipp_status_t	ipp_status;		/* Status of IPP request */
  http_t	*http;			/* HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response,		/* IPP response */
		*supported;		/* get-printer-attributes response */
  time_t	start_time;		/* Time of first connect */
  int		contimeout;		/* Connection timeout */
  int		delay,			/* Delay for retries */
		prev_delay;		/* Previous delay */
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
  ipp_attribute_t *doc_handling_sup;	/* multiple-document-handling-supported */
  ipp_attribute_t *printer_state;	/* printer-state attribute */
  ipp_attribute_t *printer_accepting;	/* printer-is-accepting-jobs */
  int		create_job = 0,		/* Does printer support Create-Job? */
		send_document = 0,	/* Does printer support Send-Document? */
		validate_job = 0;	/* Does printer support Validate-Job? */
  int		copies,			/* Number of copies for job */
		copies_remaining;	/* Number of copies remaining */
  const char	*content_type,		/* CONTENT_TYPE environment variable */
		*final_content_type,	/* FINAL_CONTENT_TYPE environment var */
		*document_format;	/* document-format value */
  int		fd;			/* File descriptor */
  off_t		bytes = 0;		/* Bytes copied */
  char		buffer[16384];		/* Copy buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		version;		/* IPP version */
  ppd_file_t	*ppd;			/* PPD file */
  _ppd_cache_t	*pc;			/* PPD cache and mapping data */
  fd_set	input;			/* Input set for select() */


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
  * Get the device URI...
  */

  while ((device_uri = cupsBackendDeviceURI(argv)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "INFO", _("Unable to locate printer."));
    sleep(10);

    if (getenv("CLASS") != NULL)
      return (CUPS_BACKEND_FAILED);
  }

  if ((auth_info_required = getenv("AUTH_INFO_REQUIRED")) == NULL)
    auth_info_required = "none";

  state_reasons = _cupsArrayNewStrings(getenv("PRINTER_STATE_REASONS"));

#ifdef HAVE_GSSAPI
 /*
  * For Kerberos, become the printing user (if we can) to get the credentials
  * that way.
  */

  if (!getuid() && (value = getenv("AUTH_UID")) != NULL)
  {
    uid_t	uid = (uid_t)atoi(value);
					/* User ID */

#  ifdef HAVE_XPC
    if (uid > 0)
    {
      if (argc == 6)
        return (run_as_user(argc, argv, uid, device_uri, 0));
      else
      {
        int status = 0;			/* Exit status */

        for (i = 6; i < argc && !status && !job_canceled; i ++)
	{
	  if ((fd = open(argv[i], O_RDONLY)) >= 0)
	  {
	    status = run_as_user(argc, argv, uid, device_uri, fd);
	    close(fd);
	  }
	  else
	  {
	    _cupsLangPrintError("ERROR", _("Unable to open print file"));
	    status = CUPS_BACKEND_FAILED;
	  }
	}

	return (status);
      }
    }

#  else /* No XPC, just try to run as the user ID */
    if (uid > 0)
      seteuid(uid);
#  endif /* HAVE_XPC */
  }
#endif /* HAVE_GSSAPI */

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

      if (!_cups_strcasecmp(name, "waitjob"))
      {
       /*
        * Wait for job completion?
	*/

        waitjob = !_cups_strcasecmp(value, "on") ||
	          !_cups_strcasecmp(value, "yes") ||
	          !_cups_strcasecmp(value, "true");
      }
      else if (!_cups_strcasecmp(name, "waitprinter"))
      {
       /*
        * Wait for printer idle?
	*/

        waitprinter = !_cups_strcasecmp(value, "on") ||
	              !_cups_strcasecmp(value, "yes") ||
	              !_cups_strcasecmp(value, "true");
      }
      else if (!_cups_strcasecmp(name, "encryption"))
      {
       /*
        * Enable/disable encryption?
	*/

        if (!_cups_strcasecmp(value, "always"))
	  cupsSetEncryption(HTTP_ENCRYPT_ALWAYS);
        else if (!_cups_strcasecmp(value, "required"))
	  cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
        else if (!_cups_strcasecmp(value, "never"))
	  cupsSetEncryption(HTTP_ENCRYPT_NEVER);
        else if (!_cups_strcasecmp(value, "ifrequested"))
	  cupsSetEncryption(HTTP_ENCRYPT_IF_REQUESTED);
	else
	{
	  _cupsLangPrintFilter(stderr, "ERROR",
			       _("Unknown encryption option value: \"%s\"."),
			       value);
        }
      }
      else if (!_cups_strcasecmp(name, "version"))
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
      else if (!_cups_strcasecmp(name, "compression"))
      {
        if (!_cups_strcasecmp(value, "true") || !_cups_strcasecmp(value, "yes") ||
	    !_cups_strcasecmp(value, "on") || !_cups_strcasecmp(value, "gzip"))
	  compression = "gzip";
      }
#endif /* HAVE_LIBZ */
      else if (!_cups_strcasecmp(name, "contimeout"))
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
    files        = NULL;
    send_options = !_cups_strcasecmp(final_content_type, "application/pdf") ||
                   !_cups_strcasecmp(final_content_type, "application/vnd.cups-pdf") ||
                   !_cups_strncasecmp(final_content_type, "image/", 6);

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
  else
  {
   /*
    * Try loading authentication information from the environment.
    */

    const char *ptr = getenv("AUTH_USERNAME");

    if (ptr)
    {
      strlcpy(username, ptr, sizeof(username));
      cupsSetUser(ptr);
    }

    password = getenv("AUTH_PASSWORD");
  }

 /*
  * Try finding the remote server...
  */

  start_time = time(NULL);

  sprintf(portname, "%d", port);

  update_reasons(NULL, "+connecting-to-device");
  fprintf(stderr, "DEBUG: Looking up \"%s\"...\n", hostname);

  while ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
  {
    _cupsLangPrintFilter(stderr, "INFO",
                         _("Unable to locate printer \"%s\"."), hostname);
    sleep(10);

    if (getenv("CLASS") != NULL)
    {
      update_reasons(NULL, "-connecting-to-device");
      return (CUPS_BACKEND_STOP);
    }
  }

  http = _httpCreate(hostname, port, addrlist, cupsEncryption(), AF_UNSPEC);
  httpSetTimeout(http, 30.0, timeout_cb, NULL);

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
  {
    if (!backendWaitLoop(snmp_fd, &(addrlist->addr), 0, backendNetworkSideCB))
      return (CUPS_BACKEND_OK);
    else if ((bytes = read(0, buffer, sizeof(buffer))) <= 0)
      return (CUPS_BACKEND_OK);
  }

 /*
  * Try connecting to the remote server...
  */

  delay = _cupsNextDelay(0, &prev_delay);

  do
  {
    fprintf(stderr, "DEBUG: Connecting to %s:%d\n", hostname, port);
    _cupsLangPrintFilter(stderr, "INFO", _("Connecting to printer."));

    if (httpReconnect(http))
    {
      int error = errno;		/* Connection error */

      if (http->status == HTTP_PKI_ERROR)
	update_reasons(NULL, "+cups-certificate-error");

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

	update_reasons(NULL, "-connecting-to-device");

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
	  update_reasons(NULL, "-connecting-to-device");
	  return (CUPS_BACKEND_FAILED);
	}

	switch (error)
	{
	  case EHOSTDOWN :
	      _cupsLangPrintFilter(stderr, "WARNING",
			           _("The printer may not exist or "
				     "is unavailable at this time."));
	      break;

	  case EHOSTUNREACH :
	      _cupsLangPrintFilter(stderr, "WARNING",
				   _("The printer is unreachable at this "
				     "time."));
	      break;

	  case ECONNREFUSED :
	  default :
	      _cupsLangPrintFilter(stderr, "WARNING",
	                           _("The printer is busy."));
	      break;
        }

	sleep(delay);

        delay = _cupsNextDelay(delay, &prev_delay);
      }
      else
      {
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("The printer is not responding."));
	sleep(30);
      }

      if (job_canceled)
	break;
    }
    else
      update_reasons(NULL, "-cups-certificate-error");
  }
  while (http->fd < 0);

  if (job_canceled || !http)
    return (CUPS_BACKEND_FAILED);

  update_reasons(NULL, "-connecting-to-device");
  _cupsLangPrintFilter(stderr, "INFO", _("Connected to printer."));

  fprintf(stderr, "DEBUG: Connected to %s:%d...\n",
	  httpAddrString(http->hostaddr, addrname, sizeof(addrname)),
	  _httpAddrPort(http->hostaddr));

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

  copies_sup       = NULL;
  cups_version     = NULL;
  format_sup       = NULL;
  media_col_sup    = NULL;
  supported        = NULL;
  operations_sup   = NULL;
  doc_handling_sup = NULL;

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
      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-wrong-http-version");
    }

    supported  = cupsDoRequest(http, request, resource);
    ipp_status = cupsLastError();

    fprintf(stderr, "DEBUG: Get-Printer-Attributes: %s (%s)\n",
            ippErrorString(ipp_status), cupsLastErrorString());

    if (ipp_status <= IPP_OK_CONFLICT)
      password_tries = 0;
    else
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

	_cupsLangPrintFilter(stderr, "INFO", _("The printer is busy."));

        report_printer_state(supported);

	sleep(delay);

        delay = _cupsNextDelay(delay, &prev_delay);
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

	ippDelete(supported);

	return (CUPS_BACKEND_STOP);
      }
      else if (ipp_status == IPP_FORBIDDEN ||
               ipp_status == IPP_AUTHENTICATION_CANCELED)
      {
        const char *www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);
        				/* WWW-Authenticate field value */

	if (!strncmp(www_auth, "Negotiate", 9))
	  auth_info_required = "negotiate";
        else if (www_auth[0])
          auth_info_required = "username,password";

	fprintf(stderr, "ATTR: auth-info-required=%s\n", auth_info_required);
	return (CUPS_BACKEND_AUTH_REQUIRED);
      }
      else if (ipp_status != IPP_NOT_AUTHORIZED)
      {
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("Unable to get printer status."));
        sleep(10);
      }

      ippDelete(supported);
      supported = NULL;
      continue;
    }

    if (!getenv("CLASS"))
    {
     /*
      * Check printer-is-accepting-jobs = false and printer-state-reasons for the
      * "spool-area-full" keyword...
      */

      int busy = 0;

      if ((printer_accepting = ippFindAttribute(supported,
						"printer-is-accepting-jobs",
						IPP_TAG_BOOLEAN)) != NULL &&
	  !printer_accepting->values[0].boolean)
        busy = 1;
      else if (!printer_accepting)
        update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-printer-is-accepting-jobs");

      if ((printer_state = ippFindAttribute(supported,
					    "printer-state-reasons",
					    IPP_TAG_KEYWORD)) != NULL && !busy)
      {
	for (i = 0; i < printer_state->num_values; i ++)
	  if (!strcmp(printer_state->values[0].string.text,
	              "spool-area-full") ||
	      !strncmp(printer_state->values[0].string.text, "spool-area-full-",
		       16))
	  {
	    busy = 1;
	    break;
	  }
      }
      else
        update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-printer-state-reasons");

      if (busy)
      {
	_cupsLangPrintFilter(stderr, "INFO", _("The printer is busy."));

	report_printer_state(supported);

	sleep(delay);

	delay = _cupsNextDelay(delay, &prev_delay);

	ippDelete(supported);
	supported = NULL;
	continue;
      }
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
        if (operations_sup->values[i].integer == IPP_PRINT_JOB)
	  break;

      if (i >= operations_sup->num_values)
	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-print-job");

      for (i = 0; i < operations_sup->num_values; i ++)
        if (operations_sup->values[i].integer == IPP_CANCEL_JOB)
	  break;

      if (i >= operations_sup->num_values)
	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-cancel-job");

      for (i = 0; i < operations_sup->num_values; i ++)
        if (operations_sup->values[i].integer == IPP_GET_JOB_ATTRIBUTES)
	  break;

      if (i >= operations_sup->num_values)
	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
                             "cups-ipp-missing-get-job-attributes");

      for (i = 0; i < operations_sup->num_values; i ++)
        if (operations_sup->values[i].integer == IPP_GET_PRINTER_ATTRIBUTES)
	  break;

      if (i >= operations_sup->num_values)
	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-get-printer-attributes");

      for (i = 0; i < operations_sup->num_values; i ++)
      {
        if (operations_sup->values[i].integer == IPP_VALIDATE_JOB)
	  validate_job = 1;
        else if (operations_sup->values[i].integer == IPP_CREATE_JOB)
	  create_job = 1;
        else if (operations_sup->values[i].integer == IPP_SEND_DOCUMENT)
	  send_document = 1;
      }

      if (!send_document)
      {
        fputs("DEBUG: Printer supports Create-Job but not Send-Document.\n",
              stderr);
        create_job = 0;
      }

      if (!validate_job)
	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
                             "cups-ipp-missing-validate-job");
    }
    else
      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-missing-operations-supported");

    doc_handling_sup = ippFindAttribute(supported,
					"multiple-document-handling-supported",
					IPP_TAG_KEYWORD);

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

    if (argc < 7 && !_cups_strncasecmp(final_content_type, "image/", 6))
      copies = 1;
  }
  else
    copies_remaining = copies;

 /*
  * Prepare remaining printing options...
  */

  options = NULL;
  pc      = NULL;

  if (send_options)
  {
    num_options = cupsParseOptions(argv[5], 0, &options);

    if (!cups_version && media_col_sup)
    {
     /*
      * Load the PPD file and generate PWG attribute mapping information...
      */

      ppd = ppdOpenFile(getenv("PPD"));
      pc  = _ppdCacheCreateWithPPD(ppd);

      ppdClose(ppd);
    }
  }
  else
    num_options = 0;

  document_format = NULL;

  if (format_sup != NULL)
  {
    for (i = 0; i < format_sup->num_values; i ++)
      if (!_cups_strcasecmp(final_content_type, format_sup->values[i].string.text))
      {
        document_format = final_content_type;
	break;
      }

    if (!document_format)
    {
      for (i = 0; i < format_sup->num_values; i ++)
	if (!_cups_strcasecmp("application/octet-stream",
	                format_sup->values[i].string.text))
	{
	  document_format = "application/octet-stream";
	  break;
	}
    }
  }

 /*
  * If the printer does not support HTTP/1.1 (which IPP requires), copy stdin
  * to a temporary file so that we can do a HTTP/1.0 submission...
  *
  * (I hate compatibility hacks!)
  */

  if (http->version < HTTP_1_1 && num_files == 0)
  {
    if ((fd = cupsTempFd(tmpfilename, sizeof(tmpfilename))) < 0)
    {
      perror("DEBUG: Unable to create temporary file");
      return (CUPS_BACKEND_FAILED);
    }

    _cupsLangPrintFilter(stderr, "INFO", _("Copying print data."));

    if ((compatsize = write(fd, buffer, bytes)) < 0)
    {
      perror("DEBUG: Unable to write temporary file");
      return (CUPS_BACKEND_FAILED);
    }

    if ((bytes = backendRunLoop(-1, fd, snmp_fd, &(addrlist->addr), 0, 0,
		                backendNetworkSideCB)) < 0)
      return (CUPS_BACKEND_FAILED);

    compatsize += bytes;

    close(fd);

    compatfile = tmpfilename;
    files      = &compatfile;
    num_files  = 1;
  }
  else if (http->version < HTTP_1_1 && num_files == 1)
  {
    struct stat	fileinfo;		/* File information */

    if (!stat(files[0], &fileinfo))
      compatsize = fileinfo.st_size;
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

  if (create_job)
  {
    monitor.job_name = argv[3];
  }
  else
  {
    snprintf(print_job_name, sizeof(print_job_name), "%s - %s", argv[1],
             argv[3]);
    monitor.job_name = print_job_name;
  }

  _cupsThreadCreate((_cups_thread_func_t)monitor_printer, &monitor);

 /*
  * Validate access to the printer...
  */

  while (!job_canceled && validate_job)
  {
    request = new_request(IPP_VALIDATE_JOB, version, uri, argv[2],
                          monitor.job_name, num_options, options, compression,
			  copies_sup ? copies : 1, document_format, pc,
			  media_col_sup, doc_handling_sup);

    ippDelete(cupsDoRequest(http, request, resource));

    ipp_status = cupsLastError();

    fprintf(stderr, "DEBUG: Validate-Job: %s (%s)\n",
            ippErrorString(ipp_status), cupsLastErrorString());

    if (job_canceled)
      break;

    if (ipp_status == IPP_SERVICE_UNAVAILABLE || ipp_status == IPP_PRINTER_BUSY)
    {
      _cupsLangPrintFilter(stderr, "INFO", _("The printer is busy."));
      sleep(10);
    }
    else if (ipp_status == IPP_FORBIDDEN ||
	     ipp_status == IPP_AUTHENTICATION_CANCELED)
    {
      const char *www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);
					/* WWW-Authenticate field value */

      if (!strncmp(www_auth, "Negotiate", 9))
	auth_info_required = "negotiate";
      else if (www_auth[0])
	auth_info_required = "username,password";

      goto cleanup;
    }
    else if (ipp_status == IPP_OPERATION_NOT_SUPPORTED)
    {
     /*
      * This is all too common...
      */

      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-missing-validate-job");
      break;
    }
    else if (ipp_status < IPP_REDIRECTION_OTHER_SITE)
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

    request = new_request((num_files > 1 || create_job) ? IPP_CREATE_JOB :
                                                          IPP_PRINT_JOB,
			  version, uri, argv[2], monitor.job_name, num_options,
			  options, compression, copies_sup ? copies : 1,
			  document_format, pc, media_col_sup, doc_handling_sup);

   /*
    * Do the request...
    */

    if (num_files > 1 || create_job)
      response = cupsDoRequest(http, request, resource);
    else
    {
      size_t	length = 0;		/* Length of request */

      if (compatsize > 0)
      {
        fputs("DEBUG: Sending file using HTTP/1.0 Content-Length...\n", stderr);
        length = ippLength(request) + (size_t)compatsize;
      }
      else
        fputs("DEBUG: Sending file using HTTP/1.1 chunking...\n", stderr);

      http_status = cupsSendRequest(http, request, resource, length);
      if (http_status == HTTP_CONTINUE && request->state == IPP_DATA)
      {
        if (num_files == 1)
        {
	  if ((fd = open(files[0], O_RDONLY)) < 0)
	  {
	    _cupsLangPrintError("ERROR", _("Unable to open print file"));
	    return (CUPS_BACKEND_FAILED);
	  }
	}
	else
	{
	  fd          = 0;
	  http_status = cupsWriteRequestData(http, buffer, bytes);
        }

        while (http_status == HTTP_CONTINUE &&
               (!job_canceled || compatsize > 0))
	{
	 /*
	  * Check for side-channel requests and more print data...
	  */

          FD_ZERO(&input);
	  FD_SET(fd, &input);
	  FD_SET(snmp_fd, &input);

          while (select(fd > snmp_fd ? fd + 1 : snmp_fd + 1, &input, NULL, NULL,
	                NULL) <= 0 && !job_canceled);

	  if (FD_ISSET(snmp_fd, &input))
	    backendCheckSideChannel(snmp_fd, http->hostaddr);

          if (FD_ISSET(fd, &input))
          {
            if ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
            {
	      fprintf(stderr, "DEBUG: Read %d bytes...\n", (int)bytes);

	      if (cupsWriteRequestData(http, buffer, bytes) != HTTP_CONTINUE)
		break;
	    }
	    else if (bytes == 0 || (errno != EINTR && errno != EAGAIN))
	      break;
	  }
	}

        if (num_files == 1)
	  close(fd);
      }

      response = cupsGetResponse(http, resource);
      ippDelete(request);
    }

    ipp_status = cupsLastError();

    fprintf(stderr, "DEBUG: %s: %s (%s)\n",
            (num_files > 1 || create_job) ? "Create-Job" : "Print-Job",
            ippErrorString(ipp_status), cupsLastErrorString());

    if (ipp_status > IPP_OK_CONFLICT)
    {
      job_id = 0;

      if (job_canceled)
        break;

      if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
          ipp_status == IPP_NOT_POSSIBLE ||
	  ipp_status == IPP_PRINTER_BUSY)
      {
	_cupsLangPrintFilter(stderr, "INFO", _("The printer is busy."));
	sleep(10);

	if (num_files == 0)
	{
	 /*
	  * We can't re-submit when we have no files to print, so exit
	  * immediately with the right status code...
	  */

	  goto cleanup;
	}
      }
      else if (ipp_status == IPP_ERROR_JOB_CANCELED)
        goto cleanup;
      else if (ipp_status == IPP_NOT_AUTHORIZED)
        continue;
      else
      {
       /*
	* Update auth-info-required as needed...
	*/

        _cupsLangPrintFilter(stderr, "ERROR",
	                     _("Print file was not accepted."));

        if (ipp_status == IPP_FORBIDDEN ||
            ipp_status == IPP_AUTHENTICATION_CANCELED)
	{
	  const char *www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);
					/* WWW-Authenticate field value */

	  if (!strncmp(www_auth, "Negotiate", 9))
	    auth_info_required = "negotiate";
	  else if (www_auth[0])
	    auth_info_required = "username,password";
	}
	else if (ipp_status == IPP_REQUEST_VALUE)
	{
	 /*
	  * Print file is too large, abort this job...
	  */

	  goto cleanup;
	}
	else
	  sleep(10);

	if (num_files == 0)
	{
	 /*
	  * We can't re-submit when we have no files to print, so exit
	  * immediately with the right status code...
	  */

	  goto cleanup;
	}
      }
    }
    else if ((job_id_attr = ippFindAttribute(response, "job-id",
                                             IPP_TAG_INTEGER)) == NULL)
    {
      _cupsLangPrintFilter(stderr, "INFO",
			   _("Print file accepted - job ID unknown."));
      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-missing-job-id");
      job_id = 0;
    }
    else
    {
      password_tries = 0;
      monitor.job_id = job_id = job_id_attr->values[0].integer;
      _cupsLangPrintFilter(stderr, "INFO",
                           _("Print file accepted - job ID %d."), job_id);
    }

    fprintf(stderr, "DEBUG: job-id=%d\n", job_id);
    ippDelete(response);

    if (job_canceled)
      break;

    if (job_id && (num_files > 1 || create_job))
    {
      for (i = 0; num_files == 0 || i < num_files; i ++)
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

        if ((i + 1) >= num_files)
	  ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);

        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
	             "document-format", NULL, document_format);

	fprintf(stderr, "DEBUG: Sending file %d using chunking...\n", i + 1);
	http_status = cupsSendRequest(http, request, resource, 0);
	if (http_status == HTTP_CONTINUE && request->state == IPP_DATA)
	{
	  if (num_files == 0)
	  {
	    fd          = 0;
	    http_status = cupsWriteRequestData(http, buffer, bytes);
	  }
	  else
	  {
	    if ((fd = open(files[i], O_RDONLY)) < 0)
	    {
	      _cupsLangPrintError("ERROR", _("Unable to open print file"));
	      return (CUPS_BACKEND_FAILED);
	    }
	  }
	}
	else
	  fd = -1;

	if (fd >= 0)
	{
	  while (!job_canceled &&
	         (bytes = read(fd, buffer, sizeof(buffer))) > 0)
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

          if (fd > 0)
	    close(fd);
	}

	ippDelete(cupsGetResponse(http, resource));
	ippDelete(request);

	fprintf(stderr, "DEBUG: Send-Document: %s (%s)\n",
		ippErrorString(cupsLastError()), cupsLastErrorString());

	if (cupsLastError() > IPP_OK_CONFLICT)
	{
	  ipp_status = cupsLastError();

	  _cupsLangPrintFilter(stderr, "ERROR",
			       _("Unable to add document to print job."));
	  break;
	}
	else
	{
	  password_tries = 0;

	  if (num_files == 0 || fd < 0)
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
             ipp_status == IPP_NOT_POSSIBLE ||
	     ipp_status == IPP_PRINTER_BUSY)
      continue;
    else if (ipp_status == IPP_REQUEST_VALUE)
    {
     /*
      * Print file is too large, abort this job...
      */

      goto cleanup;
    }
    else
      copies_remaining --;

   /*
    * Wait for the job to complete...
    */

    if (!job_id || !waitjob)
      continue;

    _cupsLangPrintFilter(stderr, "INFO", _("Waiting for job to complete."));

    for (delay = _cupsNextDelay(0, &prev_delay); !job_canceled;)
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

      httpReconnect(http);
      response   = cupsDoRequest(http, request, resource);
      ipp_status = cupsLastError();

      if (ipp_status == IPP_NOT_FOUND)
      {
       /*
        * Job has gone away and/or the server has no job history...
	*/

	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-job-history");
        ippDelete(response);

	ipp_status = IPP_OK;
        break;
      }

      fprintf(stderr, "DEBUG: Get-Job-Attributes: %s (%s)\n",
	      ippErrorString(ipp_status), cupsLastErrorString());

      if (ipp_status <= IPP_OK_CONFLICT)
	password_tries = 0;
      else
      {
	if (ipp_status != IPP_SERVICE_UNAVAILABLE &&
	    ipp_status != IPP_NOT_POSSIBLE &&
	    ipp_status != IPP_PRINTER_BUSY)
	{
	  ippDelete(response);
          ipp_status = IPP_OK;
          break;
	}
      }

      if (response)
      {
	if ((job_state = ippFindAttribute(response, "job-state",
	                                  IPP_TAG_ENUM)) != NULL)
	{
         /*
	  * Reflect the remote job state in the local queue...
	  */

	  if (cups_version &&
	      job_state->values[0].integer >= IPP_JOB_PENDING &&
	      job_state->values[0].integer <= IPP_JOB_COMPLETED)
	    update_reasons(NULL,
	                   remote_job_states[job_state->values[0].integer -
			                     IPP_JOB_PENDING]);

	  if ((job_sheets = ippFindAttribute(response,
					     "job-media-sheets-completed",
					     IPP_TAG_INTEGER)) == NULL)
	    job_sheets = ippFindAttribute(response,
					  "job-impressions-completed",
					  IPP_TAG_INTEGER);

	  if (job_sheets)
	    fprintf(stderr, "PAGE: total %d\n",
		    job_sheets->values[0].integer);

	 /*
          * Stop polling if the job is finished or pending-held...
	  */

          if (job_state->values[0].integer > IPP_JOB_STOPPED)
	  {
	    ippDelete(response);
	    break;
	  }
	}
	else if (ipp_status != IPP_SERVICE_UNAVAILABLE &&
		 ipp_status != IPP_NOT_POSSIBLE &&
		 ipp_status != IPP_PRINTER_BUSY)
	{
	 /*
	  * If the printer does not return a job-state attribute, it does not
	  * conform to the IPP specification - break out immediately and fail
	  * the job...
	  */

	  update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			       "cups-ipp-missing-job-state");
	  ipp_status = IPP_INTERNAL_ERROR;
	  break;
	}
      }

      ippDelete(response);

     /*
      * Wait before polling again...
      */

      sleep(delay);

      delay = _cupsNextDelay(delay, &prev_delay);
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

  check_printer_state(http, uri, resource, argv[2], version);

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
  _ppdCacheDestroy(pc);

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

 /*
  * Return the queue status...
  */

  if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN ||
      ipp_status == IPP_AUTHENTICATION_CANCELED ||
      ipp_status <= IPP_OK_CONFLICT)
    fprintf(stderr, "ATTR: auth-info-required=%s\n", auth_info_required);

  if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN ||
      ipp_status == IPP_AUTHENTICATION_CANCELED)
    return (CUPS_BACKEND_AUTH_REQUIRED);
  else if (ipp_status == IPP_INTERNAL_ERROR)
    return (CUPS_BACKEND_STOP);
  else if (ipp_status == IPP_DOCUMENT_FORMAT ||
           ipp_status == IPP_CONFLICT)
    return (CUPS_BACKEND_FAILED);
  else if (ipp_status == IPP_REQUEST_VALUE)
  {
    _cupsLangPrintFilter(stderr, "ERROR", _("Print job too large."));
    return (CUPS_BACKEND_CANCEL);
  }
  else if (ipp_status > IPP_OK_CONFLICT && ipp_status != IPP_ERROR_JOB_CANCELED)
    return (CUPS_BACKEND_RETRY_CURRENT);
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
    _cupsLangPrintFilter(stderr, "ERROR", _("Unable to cancel print job."));
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
    int         version)		/* I - IPP version */
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
    report_printer_state(response);

    if ((attr = ippFindAttribute(response, "printer-state",
				 IPP_TAG_ENUM)) != NULL)
      printer_state = (ipp_pstate_t)attr->values[0].integer;

    ippDelete(response);
  }

  fprintf(stderr, "DEBUG: Get-Printer-Attributes: %s (%s)\n",
	  ippErrorString(cupsLastError()), cupsLastErrorString());

  if (cupsLastError() <= IPP_OK_CONFLICT)
    password_tries = 0;

 /*
  * Return the printer-state value...
  */

  return (printer_state);
}


#ifdef HAVE_LIBZ
/*
 * 'compress_files()' - Compress print files.
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
 * 'monitor_printer()' - Monitor the printer state.
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
		prev_delay;		/* Previous delay */
  ipp_op_t	job_op;			/* Operation to use */
  int		job_id;			/* Job ID */
  const char	*job_name;		/* Job name */
  ipp_jstate_t	job_state;		/* Job state */
  const char	*job_user;		/* Job originating user name */


 /*
  * Make a copy of the printer connection...
  */

  http = _httpCreate(monitor->hostname, monitor->port, NULL, monitor->encryption,
                     AF_UNSPEC);
  httpSetTimeout(http, 30.0, timeout_cb, NULL);
  if (username[0])
    cupsSetUser(username);
  cupsSetPasswordCB(password_cb);

 /*
  * Loop until the job is canceled, aborted, or completed.
  */

  delay = _cupsNextDelay(0, &prev_delay);

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
						   monitor->version);

     /*
      * Check the status of the job itself...
      */

      job_op  = monitor->job_id > 0 ? IPP_GET_JOB_ATTRIBUTES : IPP_GET_JOBS;
      request = ippNewRequest(job_op);
      request->request.op.version[0] = monitor->version / 10;
      request->request.op.version[1] = monitor->version % 10;

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
		   NULL, monitor->uri);
      if (job_op == IPP_GET_JOB_ATTRIBUTES)
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

      fprintf(stderr, "DEBUG: %s: %s (%s)\n", ippOpString(job_op),
	      ippErrorString(cupsLastError()), cupsLastErrorString());

      if (cupsLastError() <= IPP_OK_CONFLICT)
        password_tries = 0;

      if (job_op == IPP_GET_JOB_ATTRIBUTES)
      {
	if ((attr = ippFindAttribute(response, "job-state",
				     IPP_TAG_ENUM)) != NULL)
	  monitor->job_state = (ipp_jstate_t)attr->values[0].integer;
	else
	  monitor->job_state = IPP_JOB_COMPLETED;
      }
      else if (response)
      {
        for (attr = response->attrs; attr; attr = attr->next)
        {
          job_id    = 0;
          job_name  = NULL;
          job_state = IPP_JOB_PENDING;
          job_user  = NULL;

          while (attr && attr->group_tag != IPP_TAG_JOB)
            attr = attr->next;

          if (!attr)
            break;

          while (attr && attr->group_tag == IPP_TAG_JOB)
          {
            if (!strcmp(attr->name, "job-id") &&
                attr->value_tag == IPP_TAG_INTEGER)
              job_id = attr->values[0].integer;
            else if (!strcmp(attr->name, "job-name") &&
		     (attr->value_tag == IPP_TAG_NAME ||
		      attr->value_tag == IPP_TAG_NAMELANG))
              job_name = attr->values[0].string.text;
            else if (!strcmp(attr->name, "job-state") &&
		     attr->value_tag == IPP_TAG_ENUM)
              job_state = attr->values[0].integer;
            else if (!strcmp(attr->name, "job-originating-user-name") &&
		     (attr->value_tag == IPP_TAG_NAME ||
		      attr->value_tag == IPP_TAG_NAMELANG))
              job_user = attr->values[0].string.text;

            attr = attr->next;
          }

          if (job_id > 0 && job_name && !strcmp(job_name, monitor->job_name) &&
              job_user && monitor->user && !strcmp(job_user, monitor->user))
          {
            monitor->job_id    = job_id;
            monitor->job_state = job_state;
            break;
          }

          if (!attr)
            break;
        }
      }

      ippDelete(response);

     /*
      * Disconnect from the printer - we'll reconnect on the next poll...
      */

      _httpDisconnect(http);
    }

   /*
    * Sleep for N seconds...
    */

    sleep(delay);

    delay = _cupsNextDelay(delay, &prev_delay);
  }

 /*
  * Cancel the job if necessary...
  */

  if (job_canceled && monitor->job_id > 0)
    if (!httpReconnect(http))
      cancel_job(http, monitor->uri, monitor->job_id, monitor->resource,
                 monitor->user, monitor->version);

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
    const char      *format,		/* I - document-format value or NULL */
    _ppd_cache_t    *pc,		/* I - PPD cache and mapping data */
    ipp_attribute_t *media_col_sup,	/* I - media-col-supported values */
    ipp_attribute_t *doc_handling_sup)  /* I - multiple-document-handling-supported values */
{
  int		i;			/* Looping var */
  ipp_t		*request;		/* Request data */
  const char	*keyword;		/* PWG keyword */
  _pwg_size_t	*size;			/* PWG media size */
  ipp_t		*media_col,		/* media-col value */
		*media_size;		/* media-size value */
  const char	*media_source,		/* media-source value */
		*media_type,		/* media-type value */
		*collate_str;		/* multiple-document-handling value */


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
    if (pc)
    {
      int	num_finishings = 0,	/* Number of finishing values */
		finishings[10];		/* Finishing enum values */

     /*
      * Send standard IPP attributes...
      */

      if ((keyword = cupsGetOption("PageSize", num_options, options)) == NULL)
	keyword = cupsGetOption("media", num_options, options);

      if ((size = _ppdCacheGetSize(pc, keyword)) != NULL)
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

	media_source = _ppdCacheGetSource(pc, cupsGetOption("InputSlot",
							    num_options,
							    options));
	media_type   = _ppdCacheGetType(pc, cupsGetOption("MediaType",
						          num_options,
							  options));

	for (i = 0; i < media_col_sup->num_values; i ++)
	{
	  if (!strcmp(media_col_sup->values[i].string.text,
		      "media-left-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-left-margin", size->left);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-bottom-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-bottom-margin", size->bottom);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-right-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-right-margin", size->right);
	  else if (!strcmp(media_col_sup->values[i].string.text,
			   "media-top-margin"))
	    ippAddInteger(media_col, IPP_TAG_ZERO, IPP_TAG_INTEGER,
			  "media-top-margin", size->top);
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
	keyword = _ppdCacheGetBin(pc, cupsGetOption("OutputBin", num_options,
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
	if (!_cups_strcasecmp(keyword, "Gray"))
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
	if (!_cups_strcasecmp(keyword, "draft"))
	  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
			IPP_QUALITY_DRAFT);
	else if (!_cups_strcasecmp(keyword, "normal"))
	  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
			IPP_QUALITY_NORMAL);
	else if (!_cups_strcasecmp(keyword, "high"))
	  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_ENUM, "print-quality",
			IPP_QUALITY_HIGH);
      }

      if ((keyword = cupsGetOption("sides", num_options, options)) != NULL)
	ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		     NULL, keyword);
      else if (pc->sides_option &&
               (keyword = cupsGetOption(pc->sides_option, num_options,
					options)) != NULL)
      {
	if (!_cups_strcasecmp(keyword, pc->sides_1sided))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		       NULL, "one-sided");
	else if (!_cups_strcasecmp(keyword, pc->sides_2sided_long))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		       NULL, "two-sided-long-edge");
	if (!_cups_strcasecmp(keyword, pc->sides_2sided_short))
	  ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, "sides",
		       NULL, "two-sided-short-edge");
      }

      if (doc_handling_sup &&
          (!format || _cups_strncasecmp(format, "image/", 6)) &&
 	  (keyword = cupsGetOption("collate", num_options, options)) != NULL)
      {
        if (!_cups_strcasecmp(keyword, "true"))
	  collate_str = "separate-documents-collated-copies";
	else
	  collate_str = "separate-documents-uncollated-copies";

        for (i = 0; i < doc_handling_sup->num_values; i ++)
	  if (!strcmp(doc_handling_sup->values[i].string.text, collate_str))
	  {
	    ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD,
			 "multiple-document-handling", NULL, collate_str);
	    break;
          }

        if (i >= doc_handling_sup->num_values)
          copies = 1;
      }

     /*
      * Map finishing options...
      */

      num_finishings = _ppdCacheGetFinishingValues(pc, num_options, options,
                                                   (int)(sizeof(finishings) /
                                                         sizeof(finishings[0])),
                                                   finishings);
      if (num_finishings > 0)
	ippAddIntegers(request, IPP_TAG_JOB, IPP_TAG_ENUM, "finishings",
		       num_finishings, finishings);

     /*
      * Map FaxOut options...
      */

      if ((keyword = cupsGetOption("phone", num_options, options)) != NULL)
      {
	ipp_t	*destination;		/* destination collection */
	char	tel_uri[1024];		/* tel: URI */

        destination = ippNew();

        httpAssembleURI(HTTP_URI_CODING_ALL, tel_uri, sizeof(tel_uri), "tel",
                        NULL, NULL, 0, keyword);
        ippAddString(destination, IPP_TAG_JOB, IPP_TAG_URI, "destination-uri",
                     NULL, tel_uri);

	if ((keyword = cupsGetOption("faxPrefix", num_options,
	                             options)) != NULL && *keyword)
	  ippAddString(destination, IPP_TAG_JOB, IPP_TAG_TEXT,
	               "pre-dial-string", NULL, keyword);

        ippAddCollection(request, IPP_TAG_JOB, "destination-uris", destination);
        ippDelete(destination);
      }
    }
    else
    {
     /*
      * When talking to another CUPS server, send all options...
      */

      cupsEncodeOptions(request, num_options, options);
    }

    if (copies > 1 && copies <= pc->max_copies)
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
  fprintf(stderr, "DEBUG: password_cb(prompt=\"%s\"), password=%p, "
          "password_tries=%d\n", prompt, password, password_tries);

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
  int		i;			/* Looping var */
  char		value[1024],		/* Value string */
		*valptr,		/* Pointer into value string */
		*attrptr;		/* Pointer into attribute value */
  const char	*cached;		/* Cached attribute */


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
          *valptr++ = '\'';
          *valptr++ = '\"';
	  for (attrptr = attr->values[i].string.text;
	       *attrptr && valptr < (value + sizeof(value) - 10);
	       attrptr ++)
	  {
	    if (*attrptr == '\\' || *attrptr == '\"' || *attrptr == '\'')
	    {
	      *valptr++ = '\\';
	      *valptr++ = '\\';
	      *valptr++ = '\\';
	    }

	    *valptr++ = *attrptr;
	  }
          *valptr++ = '\"';
          *valptr++ = '\'';
          break;

      default :
         /*
	  * Unsupported value type...
	  */

          return;
    }
  }

  *valptr = '\0';

  _cupsMutexLock(&report_mutex);

  if ((cached = cupsGetOption(attr->name, num_attr_cache,
                              attr_cache)) == NULL || strcmp(cached, value))
  {
   /*
    * Tell the scheduler about the new values...
    */

    num_attr_cache = cupsAddOption(attr->name, value, num_attr_cache,
                                   &attr_cache);
    fprintf(stderr, "ATTR: %s=%s\n", attr->name, value);
  }

  _cupsMutexUnlock(&report_mutex);
}


/*
 * 'report_printer_state()' - Report the printer state.
 */

static void
report_printer_state(ipp_t *ipp)	/* I - IPP response */
{
  ipp_attribute_t	*pa,		/* printer-alert */
			*pam,		/* printer-alert-message */
			*psm,		/* printer-state-message */
			*reasons,	/* printer-state-reasons */
			*marker;	/* marker-* attributes */
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
    return;

  update_reasons(reasons, NULL);

 /*
  * Relay the current marker-* attribute values...
  */

  if (ipp_supplies < 0)
  {
    ppd_file_t	*ppd;			/* PPD file */
    ppd_attr_t	*ppdattr;		/* Attribute in PPD file */

    if ((ppd = ppdOpenFile(getenv("PPD"))) != NULL &&
        (ppdattr = ppdFindAttr(ppd, "cupsIPPSupplies", NULL)) != NULL &&
        ppdattr->value && _cups_strcasecmp(ppdattr->value, "true"))
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
}


#if defined(HAVE_GSSAPI) && defined(HAVE_XPC)
/*
 * 'run_as_user()' - Run the IPP backend as the printing user.
 *
 * This function uses an XPC-based user agent to run the backend as the printing
 * user. We need to do this in order to have access to the user's Kerberos
 * credentials.
 */

static int				/* O - Exit status */
run_as_user(int        argc,		/* I - Number of command-line args */
	    char       *argv[],		/* I - Command-line arguments */
	    uid_t      uid,		/* I - User ID */
	    const char *device_uri,	/* I - Device URI */
	    int        fd)		/* I - File to print */
{
  const char		*auth_negotiate;/* AUTH_NEGOTIATE env var */
  xpc_connection_t	conn;		/* Connection to XPC service */
  xpc_object_t		request;	/* Request message dictionary */
  __block xpc_object_t	response;	/* Response message dictionary */
  dispatch_semaphore_t	sem;		/* Semaphore for waiting for response */
  int			status = CUPS_BACKEND_FAILED;
					/* Status of request */


  fprintf(stderr, "DEBUG: Running IPP backend as UID %d.\n", (int)uid);

 /*
  * Connect to the user agent for the specified UID...
  */

  conn = xpc_connection_create_mach_service(kPMPrintUIToolAgent,
                                            dispatch_get_global_queue(0, 0), 0);
  if (!conn)
  {
    _cupsLangPrintFilter(stderr, "ERROR",
                         _("Unable to start backend process."));
    fputs("DEBUG: Unable to create connection to agent.\n", stderr);
    goto cleanup;
  }

  xpc_connection_set_event_handler(conn,
                                   ^(xpc_object_t event)
				   {
				     xpc_type_t messageType = xpc_get_type(event);

				     if (messageType == XPC_TYPE_ERROR)
				     {
				       if (event == XPC_ERROR_CONNECTION_INTERRUPTED)
					 fprintf(stderr, "DEBUG: Interrupted connection to service %s.\n",
					         xpc_connection_get_name(conn));
				       else if (event == XPC_ERROR_CONNECTION_INVALID)
				         fprintf(stderr, "DEBUG: Connection invalid for service %s.\n",
					         xpc_connection_get_name(conn));
				       else
				         fprintf(stderr, "DEBUG: Unxpected error for service %s: %s\n",
					         xpc_connection_get_name(conn),
						 xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
				     }
				   });
  xpc_connection_set_target_uid(conn, uid);
  xpc_connection_resume(conn);

 /*
  * Try starting the backend...
  */

  request = xpc_dictionary_create(NULL, NULL, 0);
  xpc_dictionary_set_int64(request, "command", kPMStartJob);
  xpc_dictionary_set_string(request, "device-uri", device_uri);
  xpc_dictionary_set_string(request, "job-id", argv[1]);
  xpc_dictionary_set_string(request, "user", argv[2]);
  xpc_dictionary_set_string(request, "title", argv[3]);
  xpc_dictionary_set_string(request, "copies", argv[4]);
  xpc_dictionary_set_string(request, "options", argv[5]);
  xpc_dictionary_set_string(request, "auth-info-required",
                            getenv("AUTH_INFO_REQUIRED"));
  if ((auth_negotiate = getenv("AUTH_NEGOTIATE")) != NULL)
    xpc_dictionary_set_string(request, "auth-negotiate", auth_negotiate);
  xpc_dictionary_set_fd(request, "stdin", fd);
  xpc_dictionary_set_fd(request, "stderr", 2);
  xpc_dictionary_set_fd(request, "side-channel", CUPS_SC_FD);

  sem      = dispatch_semaphore_create(0);
  response = NULL;

  xpc_connection_send_message_with_reply(conn, request,
                                         dispatch_get_global_queue(0,0),
					 ^(xpc_object_t reply)
					 {
					   /* Save the response and wake up */
					   if (xpc_get_type(reply)
					           == XPC_TYPE_DICTIONARY)
					     response = xpc_retain(reply);

					   dispatch_semaphore_signal(sem);
					 });

  dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
  xpc_release(request);
  dispatch_release(sem);

  if (response)
  {
    child_pid = xpc_dictionary_get_int64(response, "child-pid");

    xpc_release(response);

    if (child_pid)
      fprintf(stderr, "DEBUG: Child PID=%d.\n", child_pid);
    else
    {
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Unable to start backend process."));
      fputs("DEBUG: No child PID.\n", stderr);
      goto cleanup;
    }
  }
  else
  {
    _cupsLangPrintFilter(stderr, "ERROR",
                         _("Unable to start backend process."));
    fputs("DEBUG: No reply from agent.\n", stderr);
    goto cleanup;
  }

 /*
  * Then wait for the backend to finish...
  */

  request = xpc_dictionary_create(NULL, NULL, 0);
  xpc_dictionary_set_int64(request, "command", kPMWaitForJob);
  xpc_dictionary_set_fd(request, "stderr", 2);

  sem      = dispatch_semaphore_create(0);
  response = NULL;

  xpc_connection_send_message_with_reply(conn, request,
                                         dispatch_get_global_queue(0,0),
					 ^(xpc_object_t reply)
					 {
					   /* Save the response and wake up */
					   if (xpc_get_type(reply)
					           == XPC_TYPE_DICTIONARY)
					     response = xpc_retain(reply);

					   dispatch_semaphore_signal(sem);
					 });

  dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
  xpc_release(request);
  dispatch_release(sem);

  if (response)
  {
    status = xpc_dictionary_get_int64(response, "status");

    if (status == SIGTERM || status == SIGKILL || status == SIGPIPE)
    {
      fprintf(stderr, "DEBUG: Child terminated on signal %d.\n", status);
      status = CUPS_BACKEND_FAILED;
    }
    else if (WIFSIGNALED(status))
    {
      fprintf(stderr, "DEBUG: Child crashed on signal %d.\n", status);
      status = CUPS_BACKEND_STOP;
    }
    else if (WIFEXITED(status))
    {
      status = WEXITSTATUS(status);
      fprintf(stderr, "DEBUG: Child exited with status %d.\n", status);
    }

    xpc_release(response);
  }
  else
    _cupsLangPrintFilter(stderr, "ERROR",
                         _("Unable to get backend exit status."));

  cleanup:

  if (conn)
  {
    xpc_connection_suspend(conn);
    xpc_connection_cancel(conn);
    xpc_release(conn);
  }

  return (status);
}
#endif /* HAVE_GSSAPI && HAVE_XPC */


/*
 * 'sigterm_handler()' - Handle 'terminate' signals that stop the backend.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
  (void)sig;	/* remove compiler warnings... */

#if defined(HAVE_GSSAPI) && defined(HAVE_XPC)
  if (child_pid)
  {
    kill(child_pid, sig);
    child_pid = 0;
  }
#endif /* HAVE_GSSAPI && HAVE_XPC */

  if (!job_canceled)
  {
   /*
    * Flag that the job should be canceled...
    */

    job_canceled = 1;
    return;
  }

 /*
  * The scheduler already tried to cancel us once, now just terminate
  * after removing our temp file!
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

  exit(1);
}


/*
 * 'timeout_cb()' - Handle HTTP timeouts.
 */

static int				/* O - 1 to continue, 0 to cancel */
timeout_cb(http_t *http,		/* I - Connection to server (unused) */
           void   *user_data)		/* I - User data (unused) */
{
  (void)http;
  (void)user_data;

  return (!job_canceled);
}


/*
 * 'update_reasons()' - Update the printer-state-reasons values.
 */

static void
update_reasons(ipp_attribute_t *attr,	/* I - printer-state-reasons or NULL */
               const char      *s)	/* I - STATE: string or NULL */
{
  char		op;			/* Add (+), remove (-), replace (\0) */
  cups_array_t	*new_reasons;		/* New reasons array */
  char		*reason,		/* Current reason */
		add[2048],		/* Reasons added string */
		*addptr,		/* Pointer into add string */
		rem[2048],		/* Reasons removed string */
		*remptr;		/* Pointer into remove string */
  const char	*addprefix,		/* Current add string prefix */
		*remprefix;		/* Current remove string prefix */


  fprintf(stderr, "DEBUG: update_reasons(attr=%d(%s%s), s=\"%s\")\n",
	  attr ? attr->num_values : 0, attr ? attr->values[0].string.text : "",
	  attr && attr->num_values > 1 ? ",..." : "", s ? s : "(null)");

 /*
  * Create an array of new reason keyword strings...
  */

  if (attr)
  {
    int	i;				/* Looping var */

    new_reasons = cupsArrayNew((cups_array_func_t)strcmp, NULL);
    op          = '\0';

    for (i = 0; i < attr->num_values; i ++)
    {
      reason = attr->values[i].string.text;

      if (strcmp(reason, "none") &&
	  strcmp(reason, "none-report") &&
	  strcmp(reason, "paused") &&
	  strncmp(reason, "spool-area-full", 15) &&
	  strcmp(reason, "com.apple.print.recoverable-warning") &&
	  strncmp(reason, "cups-", 5))
	cupsArrayAdd(new_reasons, reason);
    }
  }
  else if (s)
  {
    if (*s == '+' || *s == '-')
      op = *s++;
    else
      op = '\0';

    new_reasons = _cupsArrayNewStrings(s);
  }
  else
    return;

 /*
  * Compute the changes...
  */

  add[0]    = '\0';
  addprefix = "STATE: +";
  addptr    = add;
  rem[0]    = '\0';
  remprefix = "STATE: -";
  remptr    = rem;

  fprintf(stderr, "DEBUG2: op='%c', new_reasons=%d, state_reasons=%d\n",
          op ? op : ' ', cupsArrayCount(new_reasons),
	  cupsArrayCount(state_reasons));

  _cupsMutexLock(&report_mutex);

  if (op == '+')
  {
   /*
    * Add reasons...
    */

    for (reason = (char *)cupsArrayFirst(new_reasons);
	 reason;
	 reason = (char *)cupsArrayNext(new_reasons))
    {
      if (!cupsArrayFind(state_reasons, reason))
      {
        if (!strncmp(reason, "cups-remote-", 12))
	{
	 /*
	  * If we are setting cups-remote-xxx, remove all other cups-remote-xxx
	  * keywords...
	  */

	  char	*temp;		/* Current reason in state_reasons */

	  cupsArraySave(state_reasons);

	  for (temp = (char *)cupsArrayFirst(state_reasons);
	       temp;
	       temp = (char *)cupsArrayNext(state_reasons))
	    if (!strncmp(temp, "cups-remote-", 12))
	    {
	      snprintf(remptr, sizeof(rem) - (remptr - rem), "%s%s", remprefix,
	               temp);
	      remptr    += strlen(remptr);
	      remprefix = ",";

	      cupsArrayRemove(state_reasons, temp);
	      break;
	    }

	  cupsArrayRestore(state_reasons);
	}

        cupsArrayAdd(state_reasons, reason);

        snprintf(addptr, sizeof(add) - (addptr - add), "%s%s", addprefix,
	         reason);
	addptr    += strlen(addptr);
	addprefix = ",";
      }
    }
  }
  else if (op == '-')
  {
   /*
    * Remove reasons...
    */

    for (reason = (char *)cupsArrayFirst(new_reasons);
	 reason;
	 reason = (char *)cupsArrayNext(new_reasons))
    {
      if (cupsArrayFind(state_reasons, reason))
      {
	snprintf(remptr, sizeof(rem) - (remptr - rem), "%s%s", remprefix,
		 reason);
	remptr    += strlen(remptr);
	remprefix = ",";

        cupsArrayRemove(state_reasons, reason);
      }
    }
  }
  else
  {
   /*
    * Replace reasons...
    */

    for (reason = (char *)cupsArrayFirst(state_reasons);
	 reason;
	 reason = (char *)cupsArrayNext(state_reasons))
    {
      if (strncmp(reason, "cups-", 5) && !cupsArrayFind(new_reasons, reason))
      {
	snprintf(remptr, sizeof(rem) - (remptr - rem), "%s%s", remprefix,
		 reason);
	remptr    += strlen(remptr);
	remprefix = ",";

        cupsArrayRemove(state_reasons, reason);
      }
    }

    for (reason = (char *)cupsArrayFirst(new_reasons);
	 reason;
	 reason = (char *)cupsArrayNext(new_reasons))
    {
      if (!cupsArrayFind(state_reasons, reason))
      {
        cupsArrayAdd(state_reasons, reason);

        snprintf(addptr, sizeof(add) - (addptr - add), "%s%s", addprefix,
	         reason);
	addptr    += strlen(addptr);
	addprefix = ",";
      }
    }
  }

  _cupsMutexUnlock(&report_mutex);

 /*
  * Report changes and return...
  */

  if (add[0] && rem[0])
    fprintf(stderr, "%s\n%s\n", add, rem);
  else if (add[0])
    fprintf(stderr, "%s\n", add);
  else if (rem[0])
    fprintf(stderr, "%s\n", rem);
}

/*
 * End of "$Id: ipp.c 9759 2011-05-11 03:24:33Z mike $".
 */
