/*
 * "$Id: lpd.c 12024 2014-07-15 12:58:39Z msweet $"
 *
 * Line Printer Daemon backend for CUPS.
 *
 * Copyright 2007-2013 by Apple Inc.
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

#include <cups/http-private.h>
#include "backend-private.h"
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef WIN32
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* WIN32 */
#ifdef __APPLE__
#  include <CoreFoundation/CFNumber.h>
#  include <CoreFoundation/CFPreferences.h>
#endif /* __APPLE__ */


/*
 * Globals...
 */

static char	tmpfilename[1024] = "";	/* Temporary spool file name */
static int	abort_job = 0;		/* Non-zero if we get SIGTERM */


/*
 * Print mode...
 */

#define MODE_STANDARD		0	/* Queue a copy */
#define MODE_STREAM		1	/* Stream a copy */


/*
 * The order for control and data files in LPD requests...
 */

#define ORDER_CONTROL_DATA	0	/* Control file first, then data */
#define ORDER_DATA_CONTROL	1	/* Data file first, then control */


/*
 * What to reserve...
 */

#define RESERVE_NONE		0	/* Don't reserve a priviledged port */
#define RESERVE_RFC1179		1	/* Reserve port 721-731 */
#define RESERVE_ANY		2	/* Reserve port 1-1023 */


/*
 * Local functions...
 */

static int	lpd_command(int lpd_fd, char *format, ...);
static int	lpd_queue(const char *hostname, http_addrlist_t *addrlist,
			  const char *printer, int print_fd, int snmp_fd,
			  int mode, const char *user, const char *title,
			  int copies, int banner, int format, int order,
			  int reserve, int manual_copies, int timeout,
			  int contimeout, const char *orighost);
static ssize_t	lpd_write(int lpd_fd, char *buffer, size_t length);
#ifndef HAVE_RRESVPORT_AF
static int	rresvport_af(int *port, int family);
#endif /* !HAVE_RRESVPORT_AF */
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
  const char	*device_uri;		/* Device URI */
  char		scheme[255],		/* Scheme in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info */
		resource[1024],		/* Resource info (printer name) */
		*options,		/* Pointer to options */
		*name,			/* Name of option */
		*value,			/* Value of option */
		sep,			/* Separator character */
		*filename,		/* File to print */
		title[256];		/* Title string */
  int		port;			/* Port number */
  char		portname[256];		/* Port name (string) */
  http_addrlist_t *addrlist;		/* List of addresses for printer */
  int		snmp_enabled = 1;	/* Is SNMP enabled? */
  int		snmp_fd;		/* SNMP socket */
  int		fd;			/* Print file */
  int		status;			/* Status of LPD job */
  int		mode;			/* Print mode */
  int		banner;			/* Print banner page? */
  int		format;			/* Print format */
  int		order;			/* Order of control/data files */
  int		reserve;		/* Reserve priviledged port? */
  int		sanitize_title;		/* Sanitize title string? */
  int		manual_copies,		/* Do manual copies? */
		timeout,		/* Timeout */
		contimeout,		/* Connection timeout */
		copies;			/* Number of copies */
  ssize_t	bytes = 0;		/* Initial bytes read */
  char		buffer[16384];		/* Initial print buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
  int		num_jobopts;		/* Number of job options */
  cups_option_t	*jobopts = NULL;	/* Job options */


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
    printf("network lpd \"Unknown\" \"%s\"\n",
           _cupsLangString(cupsLangDefault(), _("LPD/LPR Host or Printer")));
    return (CUPS_BACKEND_OK);
  }
  else if (argc < 6 || argc > 7)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
                    argv[0]);
    return (CUPS_BACKEND_FAILED);
  }

  num_jobopts = cupsParseOptions(argv[5], 0, &jobopts);

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
    port = 515;				/* Default to port 515 */

  if (!username[0])
  {
   /*
    * If no username is in the device URI, then use the print job user...
    */

    strlcpy(username, argv[2], sizeof(username));
  }

 /*
  * See if there are any options...
  */

  mode          = MODE_STANDARD;
  banner        = 0;
  format        = 'l';
  order         = ORDER_CONTROL_DATA;
  reserve       = RESERVE_ANY;
  manual_copies = 1;
  timeout       = 300;
  contimeout    = 7 * 24 * 60 * 60;

#ifdef __APPLE__
 /*
  * We want to pass UTF-8 characters by default, not re-map them (3071945)
  */

  sanitize_title = 0;
#else
 /*
  * Otherwise we want to re-map UTF-8 to "safe" characters by default...
  */

  sanitize_title = 1;
#endif /* __APPLE__ */

  if ((options = strchr(resource, '?')) != NULL)
  {
   /*
    * Yup, terminate the device name string and move to the first
    * character of the options...
    */

    *options++ = '\0';

   /*
    * Parse options...
    */

    while (*options)
    {
     /*
      * Get the name...
      */

      name = options;

      while (*options && *options != '=' && *options != '+' && *options != '&')
        options ++;

      if ((sep = *options) != '\0')
        *options++ = '\0';

      if (sep == '=')
      {
       /*
        * Get the value...
	*/

        value = options;

	while (*options && *options != '+' && *options != '&')
	  options ++;

        if (*options)
	  *options++ = '\0';
      }
      else
        value = (char *)"";

     /*
      * Process the option...
      */

      if (!_cups_strcasecmp(name, "banner"))
      {
       /*
        * Set the banner...
	*/

        banner = !value[0] || !_cups_strcasecmp(value, "on") ||
		 !_cups_strcasecmp(value, "yes") || !_cups_strcasecmp(value, "true");
      }
      else if (!_cups_strcasecmp(name, "format") && value[0])
      {
       /*
        * Set output format...
	*/

        if (strchr("cdfglnoprtv", value[0]))
	  format = value[0];
	else
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unknown format character: \"%c\"."),
			       value[0]);
      }
      else if (!_cups_strcasecmp(name, "mode") && value[0])
      {
       /*
        * Set control/data order...
	*/

        if (!_cups_strcasecmp(value, "standard"))
	  mode = MODE_STANDARD;
	else if (!_cups_strcasecmp(value, "stream"))
	  mode = MODE_STREAM;
	else
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unknown print mode: \"%s\"."), value);
      }
      else if (!_cups_strcasecmp(name, "order") && value[0])
      {
       /*
        * Set control/data order...
	*/

        if (!_cups_strcasecmp(value, "control,data"))
	  order = ORDER_CONTROL_DATA;
	else if (!_cups_strcasecmp(value, "data,control"))
	  order = ORDER_DATA_CONTROL;
	else
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unknown file order: \"%s\"."), value);
      }
      else if (!_cups_strcasecmp(name, "reserve"))
      {
       /*
        * Set port reservation mode...
	*/

        if (!value[0] || !_cups_strcasecmp(value, "on") ||
	    !_cups_strcasecmp(value, "yes") ||
	    !_cups_strcasecmp(value, "true") ||
	    !_cups_strcasecmp(value, "rfc1179"))
	  reserve = RESERVE_RFC1179;
	else if (!_cups_strcasecmp(value, "any"))
	  reserve = RESERVE_ANY;
	else
	  reserve = RESERVE_NONE;
      }
      else if (!_cups_strcasecmp(name, "manual_copies"))
      {
       /*
        * Set manual copies...
	*/

        manual_copies = !value[0] || !_cups_strcasecmp(value, "on") ||
	 		!_cups_strcasecmp(value, "yes") ||
	 		!_cups_strcasecmp(value, "true");
      }
      else if (!_cups_strcasecmp(name, "sanitize_title"))
      {
       /*
        * Set sanitize title...
	*/

        sanitize_title = !value[0] || !_cups_strcasecmp(value, "on") ||
	 		 !_cups_strcasecmp(value, "yes") ||
	 		 !_cups_strcasecmp(value, "true");
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
      else if (!_cups_strcasecmp(name, "timeout"))
      {
       /*
        * Set the timeout...
	*/

	if (atoi(value) > 0)
	  timeout = atoi(value);
      }
      else if (!_cups_strcasecmp(name, "contimeout"))
      {
       /*
        * Set the connection timeout...
	*/

	if (atoi(value) > 0)
	  contimeout = atoi(value);
      }
    }
  }

  if (mode == MODE_STREAM)
    order = ORDER_CONTROL_DATA;

 /*
  * Find the printer...
  */

  snprintf(portname, sizeof(portname), "%d", port);

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
      exit(CUPS_BACKEND_FAILED);
    }
  }

  if (snmp_enabled)
    snmp_fd = _cupsSNMPOpen(addrlist->addr.addr.sa_family);
  else
    snmp_fd = -1;

 /*
  * Wait for data from the filter...
  */

  if (argc == 6)
  {
    if (!backendWaitLoop(snmp_fd, &(addrlist->addr), 0, backendNetworkSideCB))
      return (CUPS_BACKEND_OK);
    else if (mode == MODE_STANDARD &&
             (bytes = read(0, buffer, sizeof(buffer))) <= 0)
      return (CUPS_BACKEND_OK);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, copy stdin to a temporary file and print the temporary
  * file.
  */

  if (argc == 6 && mode == MODE_STANDARD)
  {
   /*
    * Copy stdin to a temporary file...
    */

    if ((fd = cupsTempFd(tmpfilename, sizeof(tmpfilename))) < 0)
    {
      perror("DEBUG: Unable to create temporary file");
      return (CUPS_BACKEND_FAILED);
    }

    _cupsLangPrintFilter(stderr, "INFO", _("Copying print data."));

    if (bytes > 0)
      write(fd, buffer, (size_t)bytes);

    backendRunLoop(-1, fd, snmp_fd, &(addrlist->addr), 0, 0,
		   backendNetworkSideCB);
  }
  else if (argc == 6)
  {
   /*
    * Stream from stdin...
    */

    filename = NULL;
    fd       = 0;
  }
  else
  {
    filename = argv[6];
    fd       = open(filename, O_RDONLY);

    if (fd == -1)
    {
      _cupsLangPrintError("ERROR", _("Unable to open print file"));
      return (CUPS_BACKEND_FAILED);
    }
  }

 /*
  * Sanitize the document title...
  */

  strlcpy(title, argv[3], sizeof(title));

  if (sanitize_title)
  {
   /*
    * Sanitize the title string so that we don't cause problems on
    * the remote end...
    */

    char *ptr;

    for (ptr = title; *ptr; ptr ++)
      if (!isalnum(*ptr & 255) && !isspace(*ptr & 255))
	*ptr = '_';
  }

 /*
  * Queue the job...
  */

  if (argc > 6)
  {
    if (manual_copies)
    {
      manual_copies = atoi(argv[4]);
      copies        = 1;
    }
    else
    {
      manual_copies = 1;
      copies        = atoi(argv[4]);
    }

    status = lpd_queue(hostname, addrlist, resource + 1, fd, snmp_fd, mode,
                       username, title, copies, banner, format, order, reserve,
		       manual_copies, timeout, contimeout,
		       cupsGetOption("job-originating-host-name", num_jobopts,
		                     jobopts));

    if (!status)
      fprintf(stderr, "PAGE: 1 %d\n", atoi(argv[4]));
  }
  else
    status = lpd_queue(hostname, addrlist, resource + 1, fd, snmp_fd, mode,
                       username, title, 1, banner, format, order, reserve, 1,
		       timeout, contimeout,
		       cupsGetOption("job-originating-host-name", num_jobopts,
		                     jobopts));

 /*
  * Remove the temporary file if necessary...
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

  if (fd)
    close(fd);

  if (snmp_fd >= 0)
    _cupsSNMPClose(snmp_fd);

 /*
  * Return the queue status...
  */

  return (status);
}


/*
 * 'lpd_command()' - Send an LPR command sequence and wait for a reply.
 */

static int			/* O - Status of command */
lpd_command(int  fd,		/* I - Socket connection to LPD host */
            char *format,	/* I - printf()-style format string */
            ...)		/* I - Additional args as necessary */
{
  va_list	ap;		/* Argument pointer */
  char		buf[1024];	/* Output buffer */
  ssize_t	bytes;		/* Number of bytes to output */
  char		status;		/* Status from command */


 /*
  * Don't try to send commands if the job has been canceled...
  */

  if (abort_job)
    return (-1);

 /*
  * Format the string...
  */

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  fprintf(stderr, "DEBUG: lpd_command %2.2x %s", buf[0], buf + 1);

 /*
  * Send the command...
  */

  fprintf(stderr, "DEBUG: Sending command string (" CUPS_LLFMT " bytes)...\n", CUPS_LLCAST bytes);

  if (lpd_write(fd, buf, (size_t)bytes) < bytes)
  {
    perror("DEBUG: Unable to send LPD command");
    return (-1);
  }

 /*
  * Read back the status from the command and return it...
  */

  fputs("DEBUG: Reading command status...\n", stderr);

  if (recv(fd, &status, 1, 0) < 1)
  {
    _cupsLangPrintFilter(stderr, "WARNING", _("The printer did not respond."));
    status = (char)errno;
  }

  fprintf(stderr, "DEBUG: lpd_command returning %d\n", status);

  return (status);
}


/*
 * 'lpd_queue()' - Queue a file using the Line Printer Daemon protocol.
 */

static int				/* O - Zero on success, non-zero on failure */
lpd_queue(const char      *hostname,	/* I - Host to connect to */
          http_addrlist_t *addrlist,	/* I - List of host addresses */
          const char      *printer,	/* I - Printer/queue name */
	  int             print_fd,	/* I - File to print */
	  int             snmp_fd,	/* I - SNMP socket */
	  int             mode,		/* I - Print mode */
          const char      *user,	/* I - Requesting user */
	  const char      *title,	/* I - Job title */
	  int             copies,	/* I - Number of copies */
	  int             banner,	/* I - Print LPD banner? */
          int             format,	/* I - Format specifier */
          int             order,	/* I - Order of data/control files */
	  int             reserve,	/* I - Reserve ports? */
	  int             manual_copies,/* I - Do copies by hand... */
	  int             timeout,	/* I - Timeout... */
	  int             contimeout,	/* I - Connection timeout */
	  const char      *orighost)	/* I - job-originating-host-name */
{
  char			localhost[255];	/* Local host name */
  int			error;		/* Error number */
  struct stat		filestats;	/* File statistics */
  int			lport;		/* LPD connection local port */
  int			fd;		/* LPD socket */
  char			control[10240],	/* LPD control 'file' */
			*cptr;		/* Pointer into control file string */
  char			status;		/* Status byte from command */
  int			delay;		/* Delay for retries... */
  char			addrname[256];	/* Address name */
  http_addrlist_t	*addr;		/* Socket address */
  int			have_supplies;	/* Printer supports supply levels? */
  int			copy;		/* Copies written */
  time_t		start_time;	/* Time of first connect */
  ssize_t		nbytes;		/* Number of bytes written */
  off_t			tbytes;		/* Total bytes written */
  char			buffer[32768];	/* Output buffer */
#ifdef WIN32
  DWORD			tv;		/* Timeout in milliseconds */
#else
  struct timeval	tv;		/* Timeout in secs and usecs */
#endif /* WIN32 */


 /*
  * Remember when we started trying to connect to the printer...
  */

  start_time = time(NULL);

 /*
  * Loop forever trying to print the file...
  */

  while (!abort_job)
  {
   /*
    * First try to reserve a port for this connection...
    */

    fprintf(stderr, "DEBUG: Connecting to %s:%d for printer %s\n", hostname,
            httpAddrPort(&(addrlist->addr)), printer);
    _cupsLangPrintFilter(stderr, "INFO", _("Connecting to printer."));

    for (lport = reserve == RESERVE_RFC1179 ? 732 : 1024, addr = addrlist,
             delay = 5;;
         addr = addr->next)
    {
     /*
      * Stop if this job has been canceled...
      */

      if (abort_job)
        return (CUPS_BACKEND_FAILED);

     /*
      * Choose the next priviledged port...
      */

      if (!addr)
        addr = addrlist;

      lport --;

      if (lport < 721 && reserve == RESERVE_RFC1179)
	lport = 731;
      else if (lport < 1)
	lport = 1023;

#ifdef HAVE_GETEUID
      if (geteuid() || !reserve)
#else
      if (getuid() || !reserve)
#endif /* HAVE_GETEUID */
      {
       /*
	* Just create a regular socket...
	*/

	if ((fd = socket(addr->addr.addr.sa_family, SOCK_STREAM, 0)) < 0)
	{
	  perror("DEBUG: Unable to create socket");
	  sleep(1);

          continue;
	}

        lport = 0;
      }
      else
      {
       /*
	* We're running as root and want to comply with RFC 1179.  Reserve a
	* priviledged lport between 721 and 731...
	*/

	if ((fd = rresvport_af(&lport, addr->addr.addr.sa_family)) < 0)
	{
	  perror("DEBUG: Unable to reserve port");
	  sleep(1);

	  continue;
	}
      }

     /*
      * Connect to the printer or server...
      */

      if (abort_job)
      {
	close(fd);

	return (CUPS_BACKEND_FAILED);
      }

      if (!connect(fd, &(addr->addr.addr), (socklen_t)httpAddrLength(&(addr->addr))))
	break;

      error = errno;
      close(fd);

      if (addr->next)
        continue;

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

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "DEBUG: Connection error: %s\n", strerror(error));

      if (error == ECONNREFUSED || error == EHOSTDOWN ||
          error == EHOSTUNREACH)
      {
        if (contimeout && (time(NULL) - start_time) > contimeout)
	{
	  _cupsLangPrintFilter(stderr, "ERROR",
			       _("The printer is not responding."));
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
			           _("The printer is unreachable at "
				     "this time."));
	      break;

	  case ECONNREFUSED :
	  default :
	      _cupsLangPrintFilter(stderr, "WARNING",
	                           _("The printer is in use."));
	      break;
        }

	sleep((unsigned)delay);

	if (delay < 30)
	  delay += 5;
      }
      else if (error == EADDRINUSE)
      {
       /*
	* Try on another port...
	*/

	sleep(1);
      }
      else
      {
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("The printer is not responding."));
	sleep(30);
      }
    }

   /*
    * Set the timeout...
    */

#ifdef WIN32
    tv = (DWORD)(timeout * 1000);

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
#else
    tv.tv_sec  = timeout;
    tv.tv_usec = 0;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif /* WIN32 */

    fputs("STATE: -connecting-to-device\n", stderr);
    _cupsLangPrintFilter(stderr, "INFO", _("Connected to printer."));

    fprintf(stderr, "DEBUG: Connected to %s:%d (local port %d)...\n",
	    httpAddrString(&(addr->addr), addrname, sizeof(addrname)),
	    httpAddrPort(&(addr->addr)), lport);

   /*
    * See if the printer supports SNMP...
    */

    if (snmp_fd >= 0)
      have_supplies = !backendSNMPSupplies(snmp_fd, &(addrlist->addr), NULL,
                                           NULL);
    else
      have_supplies = 0;

   /*
    * Check for side-channel requests...
    */

    backendCheckSideChannel(snmp_fd, &(addrlist->addr));

   /*
    * Next, open the print file and figure out its size...
    */

    if (print_fd)
    {
     /*
      * Use the size from the print file...
      */

      if (fstat(print_fd, &filestats))
      {
	close(fd);

	perror("DEBUG: unable to stat print file");
	return (CUPS_BACKEND_FAILED);
      }

      filestats.st_size *= manual_copies;
    }
    else
    {
     /*
      * Use a "very large value" for the size so that the printer will
      * keep printing until we close the connection...
      */

#ifdef _LARGEFILE_SOURCE
      filestats.st_size = (size_t)(999999999999.0);
#else
      filestats.st_size = 2147483647;
#endif /* _LARGEFILE_SOURCE */
    }

   /*
    * Send a job header to the printer, specifying no banner page and
    * literal output...
    */

    if (lpd_command(fd, "\002%s\n",
                    printer))		/* Receive print job(s) */
    {
      close(fd);
      return (CUPS_BACKEND_FAILED);
    }

    if (orighost && _cups_strcasecmp(orighost, "localhost"))
      strlcpy(localhost, orighost, sizeof(localhost));
    else
      httpGetHostname(NULL, localhost, sizeof(localhost));

    snprintf(control, sizeof(control),
             "H%.31s\n"		/* RFC 1179, Section 7.2 - host name <= 31 chars */
	     "P%.31s\n"		/* RFC 1179, Section 7.2 - user name <= 31 chars */
	     "J%.99s\n",	/* RFC 1179, Section 7.2 - job name <= 99 chars */
	     localhost, user, title);
    cptr = control + strlen(control);

    if (banner)
    {
      snprintf(cptr, sizeof(control) - (size_t)(cptr - control),
               "C%.31s\n"	/* RFC 1179, Section 7.2 - class name <= 31 chars */
	       "L%s\n",
               localhost, user);
      cptr   += strlen(cptr);
    }

    while (copies > 0)
    {
      snprintf(cptr, sizeof(control) - (size_t)(cptr - control), "%cdfA%03d%.15s\n",
               format, (int)getpid() % 1000, localhost);
      cptr   += strlen(cptr);
      copies --;
    }

    snprintf(cptr, sizeof(control) - (size_t)(cptr - control),
             "UdfA%03d%.15s\n"
	     "N%.131s\n",	/* RFC 1179, Section 7.2 - sourcefile name <= 131 chars */
             (int)getpid() % 1000, localhost, title);

    fprintf(stderr, "DEBUG: Control file is:\n%s", control);

    if (order == ORDER_CONTROL_DATA)
    {
     /*
      * Check for side-channel requests...
      */

      backendCheckSideChannel(snmp_fd, &(addr->addr));

     /*
      * Send the control file...
      */

      if (lpd_command(fd, "\002%d cfA%03.3d%.15s\n", strlen(control),
                      (int)getpid() % 1000, localhost))
      {
	close(fd);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "DEBUG: Sending control file (%u bytes)\n",
	      (unsigned)strlen(control));

      if ((size_t)lpd_write(fd, control, strlen(control) + 1) < (strlen(control) + 1))
      {
	status = (char)errno;
	perror("DEBUG: Unable to write control file");

      }
      else
      {
        if (read(fd, &status, 1) < 1)
	{
	  _cupsLangPrintFilter(stderr, "WARNING",
	                       _("The printer did not respond."));
	  status = (char)errno;
	}
      }

      if (status != 0)
	_cupsLangPrintFilter(stderr, "ERROR",
			     _("Remote host did not accept control file (%d)."),
			     status);
      else
	_cupsLangPrintFilter(stderr, "INFO",
	                     _("Control file sent successfully."));
    }
    else
      status = 0;

    if (status == 0)
    {
     /*
      * Check for side-channel requests...
      */

      backendCheckSideChannel(snmp_fd, &(addr->addr));

     /*
      * Send the print file...
      */

      if (lpd_command(fd, "\003" CUPS_LLFMT " dfA%03.3d%.15s\n",
                      CUPS_LLCAST filestats.st_size, (int)getpid() % 1000,
		      localhost))
      {
	close(fd);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "DEBUG: Sending data file (" CUPS_LLFMT " bytes)\n",
	      CUPS_LLCAST filestats.st_size);

      tbytes = 0;
      for (copy = 0; copy < manual_copies; copy ++)
      {
	lseek(print_fd, 0, SEEK_SET);

	while ((nbytes = read(print_fd, buffer, sizeof(buffer))) > 0)
	{
	  _cupsLangPrintFilter(stderr, "INFO",
			       _("Spooling job, %.0f%% complete."),
			       100.0 * tbytes / filestats.st_size);

	  if (lpd_write(fd, buffer, (size_t)nbytes) < nbytes)
	  {
	    perror("DEBUG: Unable to send print file to printer");
            break;
	  }
	  else
            tbytes += nbytes;
	}
      }

      if (mode == MODE_STANDARD)
      {
	if (tbytes < filestats.st_size)
	  status = (char)errno;
	else if (lpd_write(fd, "", 1) < 1)
	{
	  perror("DEBUG: Unable to send trailing nul to printer");
	  status = (char)errno;
	}
	else
	{
	 /*
          * Read the status byte from the printer; if we can't read the byte
	  * back now, we should set status to "errno", however at this point
	  * we know the printer got the whole file and we don't necessarily
	  * want to requeue it over and over...
	  */

          if (recv(fd, &status, 1, 0) < 1)
	  {
	    _cupsLangPrintFilter(stderr, "WARNING",
			         _("The printer did not respond."));
	    status = 0;
          }
	}
      }
      else
        status = 0;

      if (status != 0)
	_cupsLangPrintFilter(stderr, "ERROR",
			     _("Remote host did not accept data file (%d)."),
			     status);
      else
	_cupsLangPrintFilter(stderr, "INFO",
	                     _("Data file sent successfully."));
    }

    if (status == 0 && order == ORDER_DATA_CONTROL)
    {
     /*
      * Check for side-channel requests...
      */

      backendCheckSideChannel(snmp_fd, &(addr->addr));

     /*
      * Send control file...
      */

      if (lpd_command(fd, "\002%d cfA%03.3d%.15s\n", strlen(control),
                      (int)getpid() % 1000, localhost))
      {
	close(fd);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "DEBUG: Sending control file (%lu bytes)\n",
	      (unsigned long)strlen(control));

      if ((size_t)lpd_write(fd, control, strlen(control) + 1) < (strlen(control) + 1))
      {
	status = (char)errno;
	perror("DEBUG: Unable to write control file");
      }
      else
      {
        if (read(fd, &status, 1) < 1)
	{
	  _cupsLangPrintFilter(stderr, "WARNING",
			       _("The printer did not respond."));
	  status = (char)errno;
	}
      }

      if (status != 0)
	_cupsLangPrintFilter(stderr, "ERROR",
			     _("Remote host did not accept control file (%d)."),
			     status);
      else
	_cupsLangPrintFilter(stderr, "INFO",
	                     _("Control file sent successfully."));
    }

    fputs("STATE: +cups-waiting-for-job-completed\n", stderr);

   /*
    * Collect the final supply levels as needed...
    */

    if (have_supplies)
      backendSNMPSupplies(snmp_fd, &(addr->addr), NULL, NULL);

   /*
    * Close the socket connection and input file...
    */

    close(fd);

    if (status == 0)
      return (CUPS_BACKEND_OK);

   /*
    * Waiting for a retry...
    */

    sleep(30);
  }

 /*
  * If we get here, then the job has been canceled...
  */

  return (CUPS_BACKEND_FAILED);
}


/*
 * 'lpd_write()' - Write a buffer of data to an LPD server.
 */

static ssize_t				/* O - Number of bytes written or -1 on error */
lpd_write(int     lpd_fd,		/* I - LPD socket */
          char    *buffer,		/* I - Buffer to write */
	  size_t  length)		/* I - Number of bytes to write */
{
  ssize_t	bytes,			/* Number of bytes written */
		total;			/* Total number of bytes written */


  if (abort_job)
    return (-1);

  total = 0;
  while ((bytes = send(lpd_fd, buffer, length - (size_t)total, 0)) >= 0)
  {
    total  += bytes;
    buffer += bytes;

    if ((size_t)total == length)
      break;
  }

  if (bytes < 0)
    return (-1);
  else
    return (total);
}


#ifndef HAVE_RRESVPORT_AF
/*
 * 'rresvport_af()' - A simple implementation of rresvport_af().
 */

static int				/* O  - Socket or -1 on error */
rresvport_af(int *port,			/* IO - Port number to bind to */
             int family)		/* I  - Address family */
{
  http_addr_t	addr;			/* Socket address */
  int		fd;			/* Socket file descriptor */


 /*
  * Try to create an IPv4 socket...
  */

  if ((fd = socket(family, SOCK_STREAM, 0)) < 0)
    return (-1);

 /*
  * Initialize the address buffer...
  */

  memset(&addr, 0, sizeof(addr));
  addr.addr.sa_family = family;

 /*
  * Try to bind the socket to a reserved port...
  */

  while (*port > 511)
  {
   /*
    * Set the port number...
    */

    _httpAddrSetPort(&addr, *port);

   /*
    * Try binding the port to the socket; return if all is OK...
    */

    if (!bind(fd, (struct sockaddr *)&addr, httpAddrLength(&addr)))
      return (fd);

   /*
    * Stop if we have any error other than "address already in use"...
    */

    if (errno != EADDRINUSE)
    {
      httpAddrClose(NULL, fd);

      return (-1);
    }

   /*
    * Try the next port...
    */

    (*port)--;
  }

 /*
  * Wasn't able to bind to a reserved port, so close the socket and return
  * -1...
  */

#  ifdef WIN32
  closesocket(fd);
#  else
  close(fd);
#  endif /* WIN32 */

  return (-1);
}
#endif /* !HAVE_RRESVPORT_AF */


/*
 * 'sigterm_handler()' - Handle 'terminate' signals that stop the backend.
 */

static void
sigterm_handler(int sig)		/* I - Signal */
{
  (void)sig;	/* remove compiler warnings... */

  abort_job = 1;
}


/*
 * End of "$Id: lpd.c 12024 2014-07-15 12:58:39Z msweet $".
 */
