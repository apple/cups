/*
 * "$Id: ipp.c 12896 2015-10-09 13:15:22Z msweet $"
 *
 * IPP backend for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * "LICENSE" which should have been included with this file.  If this
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
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
#  ifdef HAVE_XPC_PRIVATE_H
#    include <xpc/private.h>
#  else
extern void	xpc_connection_set_target_uid(xpc_connection_t connection,
		                              uid_t uid);
#  endif /* HAVE_XPC_PRIVATE_H */
#endif /* HAVE_GSSAPI && HAVE_XPC */


/*
 * Bits for job-state-reasons we care about...
 */

#define _CUPS_JSR_ACCOUNT_AUTHORIZATION_FAILED	0x01
#define _CUPS_JSR_ACCOUNT_CLOSED		0x02
#define _CUPS_JSR_ACCOUNT_INFO_NEEDED		0x04
#define _CUPS_JSR_ACCOUNT_LIMIT_REACHED		0x08
#define _CUPS_JSR_JOB_PASSWORD_WAIT		0x10
#define _CUPS_JSR_JOB_RELEASE_WAIT		0x20


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
			job_id,		/* Job ID for submitted job */
			job_reasons,	/* Job state reasons bits */
			create_job,	/* Support Create-Job? */
			get_job_attrs;	/* Support Get-Job-Attributes? */
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
static pid_t		child_pid = 0;	/* Child process ID */
#endif /* HAVE_GSSAPI && HAVE_XPC */
static const char * const jattrs[] =	/* Job attributes we want */
{
  "job-id",
  "job-impressions-completed",
  "job-media-sheets-completed",
  "job-name",
  "job-originating-user-name",
  "job-state",
  "job-state-reasons"
};
static int		job_canceled = 0,
					/* Job cancelled? */
			uri_credentials = 0;
					/* Credentials supplied in URI? */
static char		username[256] = "",
					/* Username for device URI */
			*password = NULL;
					/* Password for device URI */
static const char * const pattrs[] =	/* Printer attributes we want */
{
#ifdef HAVE_LIBZ
  "compression-supported",
#endif /* HAVE_LIBZ */
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
  "print-color-mode-supported",
  "printer-alert",
  "printer-alert-description",
  "printer-is-accepting-jobs",
  "printer-mandatory-job-attributes",
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
static char		mandatory_attrs[1024] = "";
					/* cupsMandatory value */


/*
 * Local functions...
 */

static void		cancel_job(http_t *http, const char *uri, int id,
			           const char *resource, const char *user,
				   int version);
static ipp_pstate_t	check_printer_state(http_t *http, const char *uri,
		                            const char *resource,
					    const char *user, int version);
static void		*monitor_printer(_cups_monitor_t *monitor);
static ipp_t		*new_request(ipp_op_t op, int version, const char *uri,
			             const char *user, const char *title,
				     int num_options, cups_option_t *options,
				     const char *compression, int copies,
				     const char *format, _ppd_cache_t *pc,
				     ppd_file_t *ppd,
				     ipp_attribute_t *media_col_sup,
				     ipp_attribute_t *doc_handling_sup,
				     ipp_attribute_t *print_color_mode_sup);
static const char	*password_cb(const char *prompt, http_t *http,
			             const char *method, const char *resource,
			             int *user_data);
static const char	*quote_string(const char *s, char *q, size_t qsize);
static void		report_attr(ipp_attribute_t *attr);
static void		report_printer_state(ipp_t *ipp);
#if defined(HAVE_GSSAPI) && defined(HAVE_XPC)
static int		run_as_user(char *argv[], uid_t uid,
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
  int		password_tries = 0;	/* Password tries */
  http_addrlist_t *addrlist;		/* Address of printer */
  int		snmp_enabled = 1;	/* Is SNMP enabled? */
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
		waitjob_tries = 0,	/* Number of times we've waited */
		waitprinter;		/* Wait for printer ready? */
  _cups_monitor_t monitor;		/* Monitoring data */
  ipp_attribute_t *job_id_attr;		/* job-id attribute */
  int		job_id;			/* job-id value */
  ipp_attribute_t *job_sheets;		/* job-media-sheets-completed */
  ipp_attribute_t *job_state;		/* job-state */
#ifdef HAVE_LIBZ
  ipp_attribute_t *compression_sup;	/* compression-supported */
#endif /* HAVE_LIBZ */
  ipp_attribute_t *copies_sup;		/* copies-supported */
  ipp_attribute_t *cups_version;	/* cups-version */
  ipp_attribute_t *format_sup;		/* document-format-supported */
  ipp_attribute_t *job_auth;		/* job-authorization-uri */
  ipp_attribute_t *media_col_sup;	/* media-col-supported */
  ipp_attribute_t *operations_sup;	/* operations-supported */
  ipp_attribute_t *doc_handling_sup;	/* multiple-document-handling-supported */
  ipp_attribute_t *printer_state;	/* printer-state attribute */
  ipp_attribute_t *printer_accepting;	/* printer-is-accepting-jobs */
  ipp_attribute_t *print_color_mode_sup;/* Does printer support print-color-mode? */
  int		create_job = 0,		/* Does printer support Create-Job? */
		get_job_attrs = 0,	/* Does printer support Get-Job-Attributes? */
		send_document = 0,	/* Does printer support Send-Document? */
		validate_job = 0,	/* Does printer support Validate-Job? */
		copies,			/* Number of copies for job */
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
  ppd_file_t	*ppd = NULL;		/* PPD file */
  _ppd_cache_t	*pc = NULL;		/* PPD cache and mapping data */
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

  state_reasons = _cupsArrayNewStrings(getenv("PRINTER_STATE_REASONS"), ',');

#ifdef HAVE_GSSAPI
 /*
  * For Kerberos, become the printing user (if we can) to get the credentials
  * that way.
  */

  if (!getuid() && (value = getenv("AUTH_UID")) != NULL &&
      !getenv("AUTH_PASSWORD"))
  {
    uid_t	uid = (uid_t)atoi(value);
					/* User ID */

#  ifdef HAVE_XPC
    if (uid > 0)
    {
      if (argc == 6)
        return (run_as_user(argv, uid, device_uri, 0));
      else
      {
        int status = 0;			/* Exit status */

        for (i = 6; i < argc && !status && !job_canceled; i ++)
	{
	  if ((fd = open(argv[i], O_RDONLY)) >= 0)
	  {
	    status = run_as_user(argv, uid, device_uri, fd);
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

  if (!strcmp(scheme, "https") || !strcmp(scheme, "ipps"))
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
      else if (!_cups_strcasecmp(name, "snmp"))
      {
        /*
         * Enable/disable SNMP stuff...
         */

         snmp_enabled = !value[0] || !_cups_strcasecmp(value, "on") ||
                        !_cups_strcasecmp(value, "yes") ||
                        !_cups_strcasecmp(value, "true");
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
        else if (!_cups_strcasecmp(value, "deflate"))
	  compression = "deflate";
        else if (!_cups_strcasecmp(value, "false") ||
                 !_cups_strcasecmp(value, "no") ||
		 !_cups_strcasecmp(value, "off") ||
		 !_cups_strcasecmp(value, "none"))
	  compression = "none";
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

    fprintf(stderr, "DEBUG: %d files to send in job...\n", num_files);
  }

 /*
  * Set the authentication info, if any...
  */

  cupsSetPasswordCB2((cups_password_cb2_t)password_cb, &password_tries);

  if (username[0])
  {
   /*
    * Use authentication information in the device URI...
    */

    if ((password = strchr(username, ':')) != NULL)
      *password++ = '\0';

    cupsSetUser(username);
    uri_credentials = 1;
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

    if (job_canceled)
      return (CUPS_BACKEND_OK);
  }

  http = httpConnect2(hostname, port, addrlist, AF_UNSPEC, cupsEncryption(), 1,
                      0, NULL);
  httpSetTimeout(http, 30.0, timeout_cb, NULL);

  if (httpIsEncrypted(http))
  {
   /*
    * Validate TLS credentials...
    */

    cups_array_t	*creds;		/* TLS credentials */
    cups_array_t	*lcreds = NULL;	/* Loaded credentials */
    http_trust_t	trust;		/* Trust level */
    static const char	*trusts[] = { NULL, "+cups-pki-invalid", "+cups-pki-changed", "+cups-pki-expired", NULL, "+cups-pki-unknown" };
					/* Trust keywords */

    if (!httpCopyCredentials(http, &creds))
    {
      trust = httpCredentialsGetTrust(creds, hostname);

      update_reasons(NULL, "-cups-pki-invalid,cups-pki-changed,cups-pki-expired,cups-pki-unknown");
      if (trusts[trust])
      {
        update_reasons(NULL, trusts[trust]);
        return (CUPS_BACKEND_STOP);
      }

      if (httpLoadCredentials(NULL, &lcreds, hostname))
      {
       /*
        * Could not load the credentials, let's save the ones we have so we
        * can detect changes...
        */

        httpSaveCredentials(NULL, creds, hostname);
      }

      httpFreeCredentials(lcreds);
      httpFreeCredentials(creds);
    }
  }

 /*
  * See if the printer supports SNMP...
  */

  if (snmp_enabled)
    snmp_fd = _cupsSNMPOpen(addrlist->addr.addr.sa_family);
  else
    snmp_fd = -1;

  if (snmp_fd >= 0)
    have_supplies = !backendSNMPSupplies(snmp_fd, &(addrlist->addr),
                                         &start_count, NULL);
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
	                           _("The printer is in use."));
	      break;
        }

	sleep((unsigned)delay);

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

  if (job_canceled)
    return (CUPS_BACKEND_OK);
  else if (!http)
    return (CUPS_BACKEND_FAILED);

  update_reasons(NULL, "-connecting-to-device");
  _cupsLangPrintFilter(stderr, "INFO", _("Connected to printer."));

  fprintf(stderr, "DEBUG: Connected to %s:%d...\n",
	  httpAddrString(http->hostaddr, addrname, sizeof(addrname)),
	  httpAddrPort(http->hostaddr));

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

#ifdef HAVE_LIBZ
  compression_sup      = NULL;
#endif /* HAVE_LIBZ */
  copies_sup           = NULL;
  cups_version         = NULL;
  format_sup           = NULL;
  media_col_sup        = NULL;
  supported            = NULL;
  operations_sup       = NULL;
  doc_handling_sup     = NULL;
  print_color_mode_sup = NULL;

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
    ippSetVersion(request, version / 10, version % 10);
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

	_cupsLangPrintFilter(stderr, "INFO", _("The printer is in use."));

        report_printer_state(supported);

	sleep((unsigned)delay);

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
	  _cupsLangPrintFilter(stderr, "INFO", _("Preparing to print."));
	  fprintf(stderr,
	          "DEBUG: The printer does not support IPP/%d.%d, trying "
	          "IPP/1.1.\n", version / 10, version % 10);
	  version = 11;
	}
	else
	{
	  _cupsLangPrintFilter(stderr, "INFO", _("Preparing to print."));
	  fprintf(stderr,
	          "DEBUG: The printer does not support IPP/%d.%d, trying "
	          "IPP/1.0.\n", version / 10, version % 10);
	  version = 10;
        }

	httpReconnect(http);
      }
      else if (ipp_status == IPP_NOT_FOUND)
      {
        _cupsLangPrintFilter(stderr, "ERROR",
			     _("The printer configuration is incorrect or the "
			       "printer no longer exists."));

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

	httpReconnect(http);
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
					    IPP_TAG_KEYWORD)) == NULL)
      {
        update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			     "cups-ipp-missing-printer-state-reasons");
      }
      else if (!busy)
      {
	for (i = 0; i < printer_state->num_values; i ++)
	{
	  if (!strcmp(printer_state->values[0].string.text,
	              "spool-area-full") ||
	      !strncmp(printer_state->values[0].string.text, "spool-area-full-",
		       16))
	  {
	    busy = 1;
	    break;
	  }
	}
      }

      if (busy)
      {
	_cupsLangPrintFilter(stderr, "INFO", _("The printer is in use."));

	report_printer_state(supported);

	sleep((unsigned)delay);

	delay = _cupsNextDelay(delay, &prev_delay);

	ippDelete(supported);
	supported  = NULL;
	ipp_status = IPP_STATUS_ERROR_BUSY;
	continue;
      }
    }

   /*
    * Check for supported attributes...
    */

#ifdef HAVE_LIBZ
    if ((compression_sup = ippFindAttribute(supported, "compression-supported",
                                            IPP_TAG_KEYWORD)) != NULL)
    {
     /*
      * Check whether the requested compression is supported and/or default to
      * compression if supported...
      */

      if (compression && !ippContainsString(compression_sup, compression))
      {
        fprintf(stderr, "DEBUG: Printer does not support the requested "
                        "compression value \"%s\".\n", compression);
        compression = NULL;
      }
      else if (!compression)
      {
        if (ippContainsString(compression_sup, "gzip"))
          compression = "gzip";
        else if (ippContainsString(compression_sup, "deflate"))
          compression = "deflate";

        if (compression)
          fprintf(stderr, "DEBUG: Automatically using \"%s\" compression.\n",
                  compression);
      }
    }
#endif /* HAVE_LIBZ */

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

    print_color_mode_sup = ippFindAttribute(supported, "print-color-mode-supported", IPP_TAG_KEYWORD);

    if ((operations_sup = ippFindAttribute(supported, "operations-supported",
					   IPP_TAG_ENUM)) != NULL)
    {
      fprintf(stderr, "DEBUG: operations-supported (%d values)\n",
              operations_sup->num_values);
      for (i = 0; i < operations_sup->num_values; i ++)
        fprintf(stderr, "DEBUG: [%d] = %s\n", i,
                ippOpString(operations_sup->values[i].integer));

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
        else if (operations_sup->values[i].integer == IPP_GET_JOB_ATTRIBUTES)
	  get_job_attrs = 1;
      }

      if (create_job && !send_document)
      {
        fputs("DEBUG: Printer supports Create-Job but not Send-Document.\n",
              stderr);
        create_job = 0;

	update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
                             "cups-ipp-missing-send-document");
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
  while (!job_canceled && ipp_status > IPP_OK_CONFLICT);

  if (job_canceled)
    return (CUPS_BACKEND_OK);

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
    copies_remaining = 1;
  else
    copies_remaining = copies;

 /*
  * Prepare remaining printing options...
  */

  options = NULL;

  if (send_options)
  {
    num_options = cupsParseOptions(argv[5], 0, &options);

    if (!cups_version && media_col_sup)
    {
     /*
      * Load the PPD file and generate PWG attribute mapping information...
      */

      ppd_attr_t *mandatory;		/* cupsMandatory value */

      ppd = ppdOpenFile(getenv("PPD"));
      pc  = _ppdCacheCreateWithPPD(ppd);

      ppdMarkDefaults(ppd);
      cupsMarkOptions(ppd, num_options, options);

      if ((mandatory = ppdFindAttr(ppd, "cupsMandatory", NULL)) != NULL)
        strlcpy(mandatory_attrs, mandatory->value, sizeof(mandatory_attrs));
    }
  }
  else
    num_options = 0;

  document_format = NULL;

  if (format_sup != NULL)
  {
    for (i = 0; i < format_sup->num_values; i ++)
      if (!_cups_strcasecmp(final_content_type,
                            format_sup->values[i].string.text))
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

  fprintf(stderr, "DEBUG: final_content_type=\"%s\", document_format=\"%s\"\n",
          final_content_type, document_format ? document_format : "(null)");

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

    if ((compatsize = write(fd, buffer, (size_t)bytes)) < 0)
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
  * If the printer only claims to support IPP/1.0, or if the user specifically
  * included version=1.0 in the URI, then do not try to use Create-Job or
  * Send-Document.  This is another dreaded compatibility hack, but
  * unfortunately there are enough broken printers out there that we need
  * this for now...
  */

  if (version == 10)
    create_job = send_document = 0;

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
  monitor.create_job    = create_job;
  monitor.get_job_attrs = get_job_attrs;
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
			  copies_sup ? copies : 1, document_format, pc, ppd,
			  media_col_sup, doc_handling_sup, print_color_mode_sup);

    response = cupsDoRequest(http, request, resource);

    ipp_status = cupsLastError();

    fprintf(stderr, "DEBUG: Validate-Job: %s (%s)\n",
            ippErrorString(ipp_status), cupsLastErrorString());

    if ((job_auth = ippFindAttribute(response, "job-authorization-uri",
				     IPP_TAG_URI)) != NULL)
      num_options = cupsAddOption("job-authorization-uri",
                                  ippGetString(job_auth, 0, NULL), num_options,
                                  &options);

    ippDelete(response);

    if (job_canceled)
      break;

    if (ipp_status == IPP_STATUS_ERROR_SERVICE_UNAVAILABLE ||
        ipp_status == IPP_STATUS_ERROR_BUSY)
    {
      _cupsLangPrintFilter(stderr, "INFO", _("The printer is in use."));
      sleep(10);
    }
    else if (ipp_status == IPP_STATUS_ERROR_DOCUMENT_FORMAT_NOT_SUPPORTED ||
             ipp_status == IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_INFO_NEEDED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_CLOSED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_LIMIT_REACHED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_AUTHORIZATION_FAILED)
      goto cleanup;
    else if (ipp_status == IPP_STATUS_ERROR_FORBIDDEN ||
             ipp_status == IPP_STATUS_ERROR_NOT_AUTHORIZED ||
	     ipp_status == IPP_STATUS_ERROR_CUPS_AUTHENTICATION_CANCELED)
    {
      const char *www_auth = httpGetField(http, HTTP_FIELD_WWW_AUTHENTICATE);
					/* WWW-Authenticate field value */

      if (!strncmp(www_auth, "Negotiate", 9))
	auth_info_required = "negotiate";
      else if (www_auth[0])
	auth_info_required = "username,password";

      goto cleanup;
    }
    else if (ipp_status == IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED)
    {
     /*
      * This is all too common...
      */

      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-missing-validate-job");
      break;
    }
    else if (ipp_status < IPP_REDIRECTION_OTHER_SITE ||
             ipp_status == IPP_BAD_REQUEST)
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
			  document_format, pc, ppd, media_col_sup,
			  doc_handling_sup, print_color_mode_sup);

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
	if (compression && strcmp(compression, "none"))
	  httpSetField(http, HTTP_FIELD_CONTENT_ENCODING, compression);

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
	  http_status = cupsWriteRequestData(http, buffer, (size_t)bytes);
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
	  FD_SET(CUPS_SC_FD, &input);

          while (select(fd > snmp_fd ? fd + 1 : snmp_fd + 1, &input, NULL, NULL,
	                NULL) <= 0 && !job_canceled);

	  if (FD_ISSET(snmp_fd, &input))
	    backendCheckSideChannel(snmp_fd, http->hostaddr);

          if (FD_ISSET(fd, &input))
          {
            if ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
            {
	      fprintf(stderr, "DEBUG: Read %d bytes...\n", (int)bytes);

	      if ((http_status = cupsWriteRequestData(http, buffer, (size_t)bytes))
	              != HTTP_CONTINUE)
		break;
	    }
	    else if (bytes == 0 || (errno != EINTR && errno != EAGAIN))
	      break;
	  }
	}

	if (http_status == HTTP_ERROR)
	  fprintf(stderr, "DEBUG: Error writing document data for "
			  "Print-Job: %s\n", strerror(httpError(http)));

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

      if (ipp_status == IPP_STATUS_ERROR_SERVICE_UNAVAILABLE ||
          ipp_status == IPP_STATUS_ERROR_NOT_POSSIBLE ||
	  ipp_status == IPP_STATUS_ERROR_BUSY)
      {
	_cupsLangPrintFilter(stderr, "INFO", _("The printer is in use."));
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
      else if (ipp_status == IPP_STATUS_ERROR_JOB_CANCELED ||
               ipp_status == IPP_STATUS_ERROR_NOT_AUTHORIZED ||
               ipp_status == IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES ||
	       ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_INFO_NEEDED ||
	       ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_CLOSED ||
	       ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_LIMIT_REACHED ||
	       ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_AUTHORIZATION_FAILED)
        goto cleanup;
      else
      {
       /*
	* Update auth-info-required as needed...
	*/

        _cupsLangPrintFilter(stderr, "ERROR",
	                     _("Print job was not accepted."));

        if (ipp_status == IPP_STATUS_ERROR_FORBIDDEN ||
            ipp_status == IPP_STATUS_ERROR_CUPS_AUTHENTICATION_CANCELED)
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
      fputs("DEBUG: Print job accepted - job ID unknown.\n", stderr);
      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-missing-job-id");
      job_id = 0;
    }
    else
    {
      password_tries = 0;
      monitor.job_id = job_id = job_id_attr->values[0].integer;
      fprintf(stderr, "DEBUG: Print job accepted - job ID %d.\n", job_id);
    }

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
	ippSetVersion(request, version / 10, version % 10);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	     NULL, uri);

        ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
	              job_id);

	if (argv[2][0])
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                       "requesting-user-name", NULL, argv[2]);

	ippAddBoolean(request, IPP_TAG_OPERATION, "last-document",
        	      (i + 1) >= num_files);

	if (document_format)
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
		       "document-format", NULL, document_format);

        if (compression)
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
		       "compression", NULL, compression);

	fprintf(stderr, "DEBUG: Sending file %d using chunking...\n", i + 1);
	http_status = cupsSendRequest(http, request, resource, 0);
	if (http_status == HTTP_CONTINUE && request->state == IPP_DATA)
	{
	  if (compression && strcmp(compression, "none"))
	    httpSetField(http, HTTP_FIELD_CONTENT_ENCODING, compression);

	  if (num_files == 0)
	  {
	    fd          = 0;
	    http_status = cupsWriteRequestData(http, buffer, (size_t)bytes);
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
	  while (!job_canceled && http_status == HTTP_CONTINUE &&
	         (bytes = read(fd, buffer, sizeof(buffer))) > 0)
	  {
	    if ((http_status = cupsWriteRequestData(http, buffer, (size_t)bytes))
	            != HTTP_CONTINUE)
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

        if (http_status == HTTP_ERROR)
          fprintf(stderr, "DEBUG: Error writing document data for "
                          "Send-Document: %s\n", strerror(httpError(http)));

	ippDelete(cupsGetResponse(http, resource));
	ippDelete(request);

	fprintf(stderr, "DEBUG: Send-Document: %s (%s)\n",
		ippErrorString(cupsLastError()), cupsLastErrorString());

	if (cupsLastError() > IPP_OK_CONFLICT && !job_canceled)
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

    if (job_canceled)
      break;

    if (ipp_status <= IPP_OK_CONFLICT && argc > 6)
    {
      fprintf(stderr, "PAGE: 1 %d\n", copies_sup ? atoi(argv[4]) : 1);
      copies_remaining --;
    }
    else if ((ipp_status == IPP_STATUS_ERROR_DOCUMENT_FORMAT_ERROR || ipp_status == IPP_STATUS_ERROR_DOCUMENT_UNPRINTABLE) &&
             argc == 6 &&
             document_format && strcmp(document_format, "image/pwg-raster") && strcmp(document_format, "image/urf"))
    {
     /*
      * Need to reprocess the job as raster...
      */

      fputs("JOBSTATE: cups-retry-as-raster\n", stderr);
      if (job_id > 0)
	cancel_job(http, uri, job_id, resource, argv[2], version);

      goto cleanup;
    }
    else if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
             ipp_status == IPP_NOT_POSSIBLE ||
	     ipp_status == IPP_PRINTER_BUSY)
    {
      if (argc == 6)
      {
       /*
        * Need to reprocess the entire job; if we have a job ID, cancel the
        * job first...
        */

	if (job_id > 0)
	  cancel_job(http, uri, job_id, resource, argv[2], version);

        goto cleanup;
      }
      continue;
    }
    else if (ipp_status == IPP_REQUEST_VALUE ||
             ipp_status == IPP_ERROR_JOB_CANCELED ||
             ipp_status == IPP_NOT_AUTHORIZED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_INFO_NEEDED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_CLOSED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_LIMIT_REACHED ||
             ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_AUTHORIZATION_FAILED ||
             ipp_status == IPP_INTERNAL_ERROR)
    {
     /*
      * Print file is too large, job was canceled, we need new
      * authentication data, or we had some sort of error...
      */

      goto cleanup;
    }
    else if (ipp_status == IPP_STATUS_ERROR_CUPS_UPGRADE_REQUIRED)
    {
     /*
      * Server is configured incorrectly; the policy for Create-Job and
      * Send-Document has to be the same (auth or no auth, encryption or
      * no encryption).  Force the queue to stop since printing will never
      * work.
      */

      fputs("DEBUG: The server or printer is configured incorrectly.\n",
            stderr);
      fputs("DEBUG: The policy for Create-Job and Send-Document must have the "
            "same authentication and encryption requirements.\n", stderr);

      ipp_status = IPP_STATUS_ERROR_INTERNAL;

      if (job_id > 0)
	cancel_job(http, uri, job_id, resource, argv[2], version);

      goto cleanup;
    }
    else if (ipp_status == IPP_NOT_FOUND)
    {
     /*
      * Printer does not actually implement support for Create-Job/
      * Send-Document, so log the conformance issue and stop the printer.
      */

      fputs("DEBUG: This printer claims to support Create-Job and "
            "Send-Document, but those operations failed.\n", stderr);
      fputs("DEBUG: Add '?version=1.0' to the device URI to use legacy "
            "compatibility mode.\n", stderr);
      update_reasons(NULL, "+cups-ipp-conformance-failure-report,"
			   "cups-ipp-missing-send-document");

      ipp_status = IPP_INTERNAL_ERROR;	/* Force queue to stop */

      goto cleanup;
    }
    else
      copies_remaining --;

   /*
    * Wait for the job to complete...
    */

    if (!job_id || !waitjob || !get_job_attrs)
      continue;

    fputs("STATE: +cups-waiting-for-job-completed\n", stderr);

    _cupsLangPrintFilter(stderr, "INFO", _("Waiting for job to complete."));

    for (delay = _cupsNextDelay(0, &prev_delay); !job_canceled;)
    {
     /*
      * Check for side-channel requests...
      */

      backendCheckSideChannel(snmp_fd, http->hostaddr);

     /*
      * Check printer state...
      */

      check_printer_state(http, uri, resource, argv[2], version);

      if (cupsLastError() <= IPP_OK_CONFLICT)
        password_tries = 0;

     /*
      * Build an IPP_GET_JOB_ATTRIBUTES request...
      */

      request = ippNewRequest(IPP_GET_JOB_ATTRIBUTES);
      ippSetVersion(request, version / 10, version % 10);

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

      if (ipp_status == IPP_NOT_FOUND || ipp_status == IPP_NOT_POSSIBLE)
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
	    ipp_status != IPP_PRINTER_BUSY)
	{
	  ippDelete(response);
          ipp_status = IPP_OK;
          break;
	}
	else if (ipp_status == IPP_INTERNAL_ERROR)
	{
	  waitjob_tries ++;

	  if (waitjob_tries > 4)
	  {
	    ippDelete(response);
	    ipp_status = IPP_OK;
	    break;
	  }
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

	  if ((job_sheets = ippFindAttribute(response, "job-impressions-completed", IPP_TAG_INTEGER)) == NULL)
	    job_sheets = ippFindAttribute(response, "job-media-sheets-completed", IPP_TAG_INTEGER);

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

      sleep((unsigned)delay);

      delay = _cupsNextDelay(delay, &prev_delay);
    }
  }

 /*
  * Cancel the job as needed...
  */

  if (job_canceled > 0 && job_id > 0)
  {
    cancel_job(http, uri, job_id, resource, argv[2], version);

    if (cupsLastError() > IPP_OK_CONFLICT)
      _cupsLangPrintFilter(stderr, "ERROR", _("Unable to cancel print job."));
  }

 /*
  * Check the printer state and report it if necessary...
  */

  check_printer_state(http, uri, resource, argv[2], version);

  if (cupsLastError() <= IPP_OK_CONFLICT)
    password_tries = 0;

 /*
  * Collect the final page count as needed...
  */

  if (have_supplies &&
      !backendSNMPSupplies(snmp_fd, &(http->addrlist->addr), &page_count,
                           NULL) &&
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
  ppdClose(ppd);

  httpClose(http);

  ippDelete(supported);

 /*
  * Remove the temporary file(s) if necessary...
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

 /*
  * Return the queue status...
  */

  if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN ||
      ipp_status == IPP_AUTHENTICATION_CANCELED ||
      ipp_status <= IPP_OK_CONFLICT)
    fprintf(stderr, "ATTR: auth-info-required=%s\n", auth_info_required);

  if (ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_INFO_NEEDED)
    fputs("JOBSTATE: account-info-needed\n", stderr);
  else if (ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_CLOSED)
    fputs("JOBSTATE: account-closed\n", stderr);
  else if (ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_LIMIT_REACHED)
    fputs("JOBSTATE: account-limit-reached\n", stderr);
  else if (ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_AUTHORIZATION_FAILED)
    fputs("JOBSTATE: account-authorization-failed\n", stderr);

  if (ipp_status == IPP_NOT_AUTHORIZED || ipp_status == IPP_FORBIDDEN ||
      ipp_status == IPP_AUTHENTICATION_CANCELED)
    return (CUPS_BACKEND_AUTH_REQUIRED);
  else if (ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_LIMIT_REACHED ||
	   ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_INFO_NEEDED ||
	   ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_CLOSED ||
	   ipp_status == IPP_STATUS_ERROR_CUPS_ACCOUNT_AUTHORIZATION_FAILED)
    return (CUPS_BACKEND_HOLD);
  else if (ipp_status == IPP_INTERNAL_ERROR)
    return (CUPS_BACKEND_STOP);
  else if (ipp_status == IPP_CONFLICT)
    return (CUPS_BACKEND_FAILED);
  else if (ipp_status == IPP_REQUEST_VALUE ||
	   ipp_status == IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES ||
           ipp_status == IPP_DOCUMENT_FORMAT || job_canceled < 0)
  {
    if (ipp_status == IPP_REQUEST_VALUE)
      _cupsLangPrintFilter(stderr, "ERROR", _("Print job too large."));
    else if (ipp_status == IPP_DOCUMENT_FORMAT)
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Printer cannot print supplied content."));
    else if (ipp_status == IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES)
      _cupsLangPrintFilter(stderr, "ERROR",
                           _("Printer cannot print with supplied options."));
    else
      _cupsLangPrintFilter(stderr, "ERROR", _("Print job canceled at printer."));

    return (CUPS_BACKEND_CANCEL);
  }
  else if (ipp_status > IPP_OK_CONFLICT && ipp_status != IPP_ERROR_JOB_CANCELED)
    return (CUPS_BACKEND_RETRY_CURRENT);
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


  _cupsLangPrintFilter(stderr, "INFO", _("Canceling print job."));

  request = ippNewRequest(IPP_CANCEL_JOB);
  ippSetVersion(request, version / 10, version % 10);

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
  ippSetVersion(request, version / 10, version % 10);

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

 /*
  * Return the printer-state value...
  */

  return (printer_state);
}


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
  int		password_tries = 0;	/* Password tries */


 /*
  * Make a copy of the printer connection...
  */

  http = httpConnect2(monitor->hostname, monitor->port, NULL, AF_UNSPEC,
                      monitor->encryption, 1, 0, NULL);
  httpSetTimeout(http, 30.0, timeout_cb, NULL);
  if (username[0])
    cupsSetUser(username);

  cupsSetPasswordCB2((cups_password_cb2_t)password_cb, &password_tries);

 /*
  * Loop until the job is canceled, aborted, or completed.
  */

  delay = _cupsNextDelay(0, &prev_delay);

  monitor->job_reasons = 0;

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
      if (cupsLastError() <= IPP_OK_CONFLICT)
        password_tries = 0;

      if (monitor->job_id == 0 && monitor->create_job)
      {
       /*
        * No job-id yet, so continue...
	*/

        goto monitor_disconnect;
      }

     /*
      * Check the status of the job itself...
      */

      job_op  = (monitor->job_id > 0 && monitor->get_job_attrs) ?
                    IPP_GET_JOB_ATTRIBUTES : IPP_GET_JOBS;
      request = ippNewRequest(job_op);
      ippSetVersion(request, monitor->version / 10, monitor->version % 10);

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

      fprintf(stderr, "DEBUG: (monitor) %s: %s (%s)\n", ippOpString(job_op),
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
              job_state = (ipp_jstate_t)attr->values[0].integer;
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

      fprintf(stderr, "DEBUG: (monitor) job-state = %s\n",
              ippEnumString("job-state", monitor->job_state));

      if (!job_canceled &&
          (monitor->job_state == IPP_JOB_CANCELED ||
	   monitor->job_state == IPP_JOB_ABORTED))
      {
	job_canceled = -1;
	fprintf(stderr, "DEBUG: (monitor) job_canceled = -1\n");
      }

      if ((attr = ippFindAttribute(response, "job-state-reasons",
                                   IPP_TAG_KEYWORD)) != NULL)
      {
        int	i, new_reasons = 0;	/* Looping var, new reasons */

        for (i = 0; i < attr->num_values; i ++)
        {
          if (!strcmp(attr->values[i].string.text,
                      "account-authorization-failed"))
            new_reasons |= _CUPS_JSR_ACCOUNT_AUTHORIZATION_FAILED;
          else if (!strcmp(attr->values[i].string.text, "account-closed"))
            new_reasons |= _CUPS_JSR_ACCOUNT_CLOSED;
          else if (!strcmp(attr->values[i].string.text, "account-info-needed"))
            new_reasons |= _CUPS_JSR_ACCOUNT_INFO_NEEDED;
          else if (!strcmp(attr->values[i].string.text,
                           "account-limit-reached"))
            new_reasons |= _CUPS_JSR_ACCOUNT_LIMIT_REACHED;
          else if (!strcmp(attr->values[i].string.text, "job-password-wait"))
            new_reasons |= _CUPS_JSR_JOB_PASSWORD_WAIT;
          else if (!strcmp(attr->values[i].string.text, "job-release-wait"))
            new_reasons |= _CUPS_JSR_JOB_RELEASE_WAIT;
	  if (!job_canceled &&
	      (!strncmp(attr->values[i].string.text, "job-canceled-", 13) || !strcmp(attr->values[i].string.text, "aborted-by-system")))
            job_canceled = 1;
        }

        if (new_reasons != monitor->job_reasons)
        {
	  if (new_reasons & _CUPS_JSR_ACCOUNT_AUTHORIZATION_FAILED)
	    fputs("JOBSTATE: account-authorization-failed\n", stderr);
	  else if (new_reasons & _CUPS_JSR_ACCOUNT_CLOSED)
	    fputs("JOBSTATE: account-closed\n", stderr);
	  else if (new_reasons & _CUPS_JSR_ACCOUNT_INFO_NEEDED)
	    fputs("JOBSTATE: account-info-needed\n", stderr);
	  else if (new_reasons & _CUPS_JSR_ACCOUNT_LIMIT_REACHED)
	    fputs("JOBSTATE: account-limit-reached\n", stderr);
	  else if (new_reasons & _CUPS_JSR_JOB_PASSWORD_WAIT)
	    fputs("JOBSTATE: job-password-wait\n", stderr);
	  else if (new_reasons & _CUPS_JSR_JOB_RELEASE_WAIT)
	    fputs("JOBSTATE: job-release-wait\n", stderr);
	  else
	    fputs("JOBSTATE: job-printing\n", stderr);

	  monitor->job_reasons = new_reasons;
        }
      }

      ippDelete(response);

      fprintf(stderr, "DEBUG: (monitor) job-state = %s\n",
              ippEnumString("job-state", monitor->job_state));

      if (!job_canceled &&
          (monitor->job_state == IPP_JOB_CANCELED ||
	   monitor->job_state == IPP_JOB_ABORTED))
	job_canceled = -1;

     /*
      * Disconnect from the printer - we'll reconnect on the next poll...
      */

      monitor_disconnect:

      _httpDisconnect(http);
    }

   /*
    * Sleep for N seconds...
    */

    sleep((unsigned)delay);

    delay = _cupsNextDelay(delay, &prev_delay);
  }

 /*
  * Cancel the job if necessary...
  */

  if (job_canceled > 0 && monitor->job_id > 0)
  {
    if (!httpReconnect(http))
    {
      cancel_job(http, monitor->uri, monitor->job_id, monitor->resource,
                 monitor->user, monitor->version);

      if (cupsLastError() > IPP_OK_CONFLICT)
      {
	fprintf(stderr, "DEBUG: (monitor) cancel_job() = %s\n", cupsLastErrorString());
	_cupsLangPrintFilter(stderr, "ERROR", _("Unable to cancel print job."));
      }
    }
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
    const char      *format,		/* I - document-format value or NULL */
    _ppd_cache_t    *pc,		/* I - PPD cache and mapping data */
    ppd_file_t      *ppd,		/* I - PPD file data */
    ipp_attribute_t *media_col_sup,	/* I - media-col-supported values */
    ipp_attribute_t *doc_handling_sup,  /* I - multiple-document-handling-supported values */
    ipp_attribute_t *print_color_mode_sup)
					/* I - Printer supports print-color-mode */
{
  ipp_t		*request;		/* Request data */
  const char	*keyword;		/* PWG keyword */
  ipp_tag_t	group;			/* Current group */
  ipp_attribute_t *attr;		/* Current attribute */
  char		buffer[1024];		/* Value buffer */


 /*
  * Create the IPP request...
  */

  request = ippNewRequest(op);
  ippSetVersion(request, version / 10, version % 10);

  fprintf(stderr, "DEBUG: %s IPP/%d.%d\n",
	  ippOpString(request->request.op.operation_id),
	  request->request.op.version[0],
	  request->request.op.version[1]);

 /*
  * Add standard attributes...
  */

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);
  fprintf(stderr, "DEBUG: printer-uri=\"%s\"\n", uri);

  if (user && *user)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, user);
    fprintf(stderr, "DEBUG: requesting-user-name=\"%s\"\n", user);
  }

  if (title && *title)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, title);
    fprintf(stderr, "DEBUG: job-name=\"%s\"\n", title);
  }

  if (format && op != IPP_CREATE_JOB)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format", NULL, format);
    fprintf(stderr, "DEBUG: document-format=\"%s\"\n", format);
  }

#ifdef HAVE_LIBZ
  if (compression && op != IPP_OP_CREATE_JOB && op != IPP_OP_VALIDATE_JOB)
  {
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "compression", NULL, compression);
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
     /*
      * Send standard IPP attributes...
      */

      fputs("DEBUG: Adding standard IPP operation/job attributes.\n", stderr);

      copies = _cupsConvertOptions(request, ppd, pc, media_col_sup, doc_handling_sup, print_color_mode_sup, user, format, copies, num_options, options);

     /*
      * Map FaxOut options...
      */

      if ((keyword = cupsGetOption("phone", num_options, options)) != NULL)
      {
	ipp_t	*destination;		/* destination collection */
	char	phone[1024],		/* Phone number string */
		*ptr,			/* Pointer into string */
		tel_uri[1024];		/* tel: URI */
        static const char * const allowed = "0123456789#*-+.()";
					/* Allowed characters */

        destination = ippNew();

       /*
        * Unescape and filter out spaces and other characters that are not
        * allowed in a tel: URI.
        */

        _httpDecodeURI(phone, keyword, sizeof(phone));
        for (ptr = phone; *ptr;)
	{
	  if (!strchr(allowed, *ptr))
	    _cups_strcpy(ptr, ptr + 1);
	  else
	    ptr ++;
        }

        httpAssembleURI(HTTP_URI_CODING_ALL, tel_uri, sizeof(tel_uri), "tel", NULL, NULL, 0, phone);
        ippAddString(destination, IPP_TAG_JOB, IPP_TAG_URI, "destination-uri", NULL, tel_uri);

	if ((keyword = cupsGetOption("faxPrefix", num_options,
	                             options)) != NULL && *keyword)
        {
	  char	predial[1024];		/* Pre-dial string */

	  _httpDecodeURI(predial, keyword, sizeof(predial));
	  ippAddString(destination, IPP_TAG_JOB, IPP_TAG_TEXT, "pre-dial-string", NULL, predial);
	}

        ippAddCollection(request, IPP_TAG_JOB, "destination-uris", destination);
        ippDelete(destination);
      }
    }
    else
    {
     /*
      * When talking to another CUPS server, send all options...
      */

      fputs("DEBUG: Adding all operation/job attributes.\n", stderr);
      cupsEncodeOptions2(request, num_options, options, IPP_TAG_OPERATION);
      cupsEncodeOptions2(request, num_options, options, IPP_TAG_JOB);
    }

    if (copies > 1 && (!pc || copies <= pc->max_copies))
      ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies", copies);
  }

  fprintf(stderr, "DEBUG: IPP/%d.%d %s #%d\n", version / 10, version % 10,
          ippOpString(ippGetOperation(request)), ippGetRequestId(request));
  for (group = IPP_TAG_ZERO, attr = ippFirstAttribute(request);
       attr;
       attr = ippNextAttribute(request))
  {
    const char *name = ippGetName(attr);

    if (!name)
    {
      group = IPP_TAG_ZERO;
      continue;
    }

    if (group != ippGetGroupTag(attr))
    {
      group = ippGetGroupTag(attr);
      fprintf(stderr, "DEBUG: ---- %s ----\n", ippTagString(group));
    }

    ippAttributeString(attr, buffer, sizeof(buffer));
    fprintf(stderr, "DEBUG: %s %s%s %s\n", name,
            ippGetCount(attr) > 1 ? "1setOf " : "",
            ippTagString(ippGetValueTag(attr)), buffer);
  }

  fprintf(stderr, "DEBUG: ---- %s ----\n", ippTagString(IPP_TAG_END));

  return (request);
}


/*
 * 'password_cb()' - Disable the password prompt for cupsDoFileRequest().
 */

static const char *			/* O - Password  */
password_cb(const char *prompt,		/* I - Prompt (not used) */
            http_t     *http,		/* I - Connection */
            const char *method,		/* I - Request method (not used) */
            const char *resource,	/* I - Resource path (not used) */
            int        *password_tries)	/* I - Password tries */
{
  char	def_username[HTTP_MAX_VALUE];	/* Default username */


  fprintf(stderr, "DEBUG: password_cb(prompt=\"%s\", http=%p, method=\"%s\", "
                  "resource=\"%s\", password_tries=%p(%d)), password=%p\n",
          prompt, http, method, resource, password_tries, *password_tries,
          password);

  (void)prompt;
  (void)method;
  (void)resource;

  if (!uri_credentials)
  {
   /*
    * Remember that we need to authenticate...
    */

    auth_info_required = "username,password";

    if (httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "username",
			def_username))
    {
      char	quoted[HTTP_MAX_VALUE * 2 + 4];
					  /* Quoted string */

      fprintf(stderr, "ATTR: auth-info-default=%s,\n",
	      quote_string(def_username, quoted, sizeof(quoted)));
    }
  }

  if (password && *password && *password_tries < 3)
  {
    (*password_tries) ++;

    cupsSetUser(username);

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
 * 'quote_string()' - Quote a string value.
 */

static const char *			/* O - Quoted string */
quote_string(const char *s,		/* I - String */
             char       *q,		/* I - Quoted string buffer */
             size_t     qsize)		/* I - Size of quoted string buffer */
{
  char	*qptr,				/* Pointer into string buffer */
	*qend;				/* End of string buffer */


  qptr = q;
  qend = q + qsize - 5;

  if (qend < q)
  {
    *q = '\0';
    return (q);
  }

  *qptr++ = '\'';
  *qptr++ = '\"';

  while (*s && qptr < qend)
  {
    if (*s == '\\' || *s == '\"' || *s == '\'')
    {
      if (qptr < (qend - 4))
      {
	*qptr++ = '\\';
	*qptr++ = '\\';
	*qptr++ = '\\';
      }
      else
        break;
    }

    *qptr++ = *s++;
  }

  *qptr++ = '\"';
  *qptr++ = '\'';
  *qptr   = '\0';

  return (q);
}


/*
 * 'report_attr()' - Report an IPP attribute value.
 */

static void
report_attr(ipp_attribute_t *attr)	/* I - Attribute */
{
  int		i;			/* Looping var */
  char		value[1024],		/* Value string */
		*valptr;		/* Pointer into value string */
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
          snprintf(valptr, sizeof(value) - (size_t)(valptr - value), "%d", attr->values[i].integer);
	  valptr += strlen(valptr);
	  break;

      case IPP_TAG_TEXT :
      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
          quote_string(attr->values[i].string.text, valptr, (size_t)(value + sizeof(value) - valptr));
          valptr += strlen(valptr);
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
			*pmja,		/* printer-mandatory-job-attributes */
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

  if ((pmja = ippFindAttribute(ipp, "printer-mandatory-job-attributes", IPP_TAG_KEYWORD)) != NULL)
  {
    int	i,				/* Looping var */
	count = ippGetCount(pmja);	/* Number of values */

    for (i = 0, valptr = value; i < count; i ++, valptr += strlen(valptr))
    {
      if (i)
        snprintf(valptr, sizeof(value) - (size_t)(valptr - value), " %s", ippGetString(pmja, i, NULL));
      else
        strlcpy(value, ippGetString(pmja, i, NULL), sizeof(value));
    }

    if (strcmp(value, mandatory_attrs))
    {
      strlcpy(mandatory_attrs, value, sizeof(mandatory_attrs));
      fprintf(stderr, "PPD: cupsMandatory=\"%s\"\n", value);
    }
  }

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
run_as_user(char       *argv[],		/* I - Command-line arguments */
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
    child_pid = (pid_t)xpc_dictionary_get_int64(response, "child-pid");

    xpc_release(response);

    if (child_pid)
      fprintf(stderr, "DEBUG: Child PID=%d.\n", (int)child_pid);
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
    status = (int)xpc_dictionary_get_int64(response, "status");

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

  write(2, "DEBUG: Got SIGTERM.\n", 20);

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

    write(2, "DEBUG: sigterm_handler: job_canceled = 1.\n", 25);

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

    new_reasons = _cupsArrayNewStrings(s, ',');
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
	      snprintf(remptr, sizeof(rem) - (size_t)(remptr - rem), "%s%s", remprefix, temp);
	      remptr    += strlen(remptr);
	      remprefix = ",";

	      cupsArrayRemove(state_reasons, temp);
	      break;
	    }

	  cupsArrayRestore(state_reasons);
	}

        cupsArrayAdd(state_reasons, reason);

        snprintf(addptr, sizeof(add) - (size_t)(addptr - add), "%s%s", addprefix, reason);
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
	snprintf(remptr, sizeof(rem) - (size_t)(remptr - rem), "%s%s", remprefix, reason);
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
	snprintf(remptr, sizeof(rem) - (size_t)(remptr - rem), "%s%s", remprefix, reason);
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

        snprintf(addptr, sizeof(add) - (size_t)(addptr - add), "%s%s", addprefix, reason);
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
 * End of "$Id: ipp.c 12896 2015-10-09 13:15:22Z msweet $".
 */
