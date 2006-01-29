/*
 * "$Id$"
 *
 *   Line Printer Daemon backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   main()            - Send a file to the printer or server.
 *   lpd_command()     - Send an LPR command sequence and wait for a reply.
 *   lpd_queue()       - Queue a file using the Line Printer Daemon protocol.
 *   lpd_timeout()     - Handle timeout alarms...
 *   lpd_write()       - Write a buffer of data to an LPD server.
 *   rresvport_af()    - A simple implementation of rresvport_af().
 *   sigterm_handler() - Handle 'terminate' signals that stop the backend.
 */

/*
 * Include necessary headers.
 */

#include <cups/backend.h>
#include <cups/http-private.h>
#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

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

static int	lpd_command(int lpd_fd, int timeout, char *format, ...);
static int	lpd_queue(const char *hostname, int port, const char *printer,
		          const char *filename,
		          const char *user, const char *title, int copies,
			  int banner, int format, int order, int reserve,
			  int manual_copies, int timeout, int contimeout);
static void	lpd_timeout(int sig);
static int	lpd_write(int lpd_fd, char *buffer, int length);
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
  char			method[255],	/* Method in URI */
			hostname[1024],	/* Hostname */
			username[255],	/* Username info */
			resource[1024],	/* Resource info (printer name) */
			*options,	/* Pointer to options */
			name[255],	/* Name of option */
			value[255],	/* Value of option */
			*ptr,		/* Pointer into name or value */
			*filename,	/* File to print */
			title[256];	/* Title string */
  int			port;		/* Port number */
  int			status;		/* Status of LPD job */
  int			banner;		/* Print banner page? */
  int			format;		/* Print format */
  int			order;		/* Order of control/data files */
  int			reserve;	/* Reserve priviledged port? */
  int			sanitize_title;	/* Sanitize title string? */
  int			manual_copies,	/* Do manual copies? */
			timeout,	/* Timeout */
			contimeout,	/* Connection timeout */
			copies;		/* Number of copies */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


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
    puts("network lpd \"Unknown\" \"LPD/LPR Host or Printer\"");
    return (CUPS_BACKEND_OK);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (CUPS_BACKEND_FAILED);
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

    int  fd;		/* Temporary file */
    char buffer[8192];	/* Buffer for copying */
    int  bytes;		/* Number of bytes read */


    if ((fd = cupsTempFd(tmpfilename, sizeof(tmpfilename))) < 0)
    {
      perror("ERROR: unable to create temporary file");
      return (CUPS_BACKEND_FAILED);
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (write(fd, buffer, bytes) < bytes)
      {
        perror("ERROR: unable to write to temporary file");
	close(fd);
	unlink(tmpfilename);
	return (CUPS_BACKEND_FAILED);
      }

    close(fd);
    filename = tmpfilename;
  }
  else
    filename = argv[6];

 /*
  * Extract the hostname and printer name from the URI...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, cupsBackendDeviceURI(argv),
                  method, sizeof(method), username, sizeof(username),
		  hostname, sizeof(hostname), &port,
		  resource, sizeof(resource));

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

  banner        = 0;
  format        = 'l';
  order         = ORDER_CONTROL_DATA;
  reserve       = RESERVE_ANY;
  manual_copies = 1;
  timeout       = 300;
  contimeout    = 7 * 24 * 60 * 60;

#ifdef __APPLE__
  /* We want to pass utf-8 characters, not re-map them (3071945) */
  sanitize_title = 0;

  {
    CFPropertyListRef	pvalue;		/* Preference value */
    SInt32		toval;		/* Timeout value */


    pvalue = CFPreferencesCopyValue(CFSTR("timeout"),
                                    CFSTR("com.apple.print.backends"),
				    kCFPreferencesAnyUser,
				    kCFPreferencesCurrentHost);
    if (pvalue)
    {
      if (CFGetTypeID(pvalue) == CFNumberGetTypeID())
      {
	CFNumberGetValue(pvalue, kCFNumberSInt32Type, &toval);
	contimeout = (int)toval;
      }

      CFRelease(pvalue);
    }
  }
#else
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

      for (ptr = name; *options && *options != '=';)
        if (ptr < (name + sizeof(name) - 1))
          *ptr++ = *options++;
      *ptr = '\0';

      if (*options == '=')
      {
       /*
        * Get the value...
	*/

        options ++;

	for (ptr = value; *options && *options != '+' && *options != '&';)
          if (ptr < (value + sizeof(value) - 1))
            *ptr++ = *options++;
	*ptr = '\0';

	if (*options == '+' || *options == '&')
	  options ++;
      }
      else
        value[0] = '\0';

     /*
      * Process the option...
      */

      if (!strcasecmp(name, "banner"))
      {
       /*
        * Set the banner...
	*/

        banner = !value[0] || !strcasecmp(value, "on") ||
		 !strcasecmp(value, "yes") || !strcasecmp(value, "true");
      }
      else if (!strcasecmp(name, "format") && value[0])
      {
       /*
        * Set output format...
	*/

        if (strchr("cdfglnoprtv", value[0]))
	  format = value[0];
	else
	  fprintf(stderr, "ERROR: Unknown format character \"%c\"\n", value[0]);
      }
      else if (!strcasecmp(name, "order") && value[0])
      {
       /*
        * Set control/data order...
	*/

        if (!strcasecmp(value, "control,data"))
	  order = ORDER_CONTROL_DATA;
	else if (!strcasecmp(value, "data,control"))
	  order = ORDER_DATA_CONTROL;
	else
	  fprintf(stderr, "ERROR: Unknown file order \"%s\"\n", value);
      }
      else if (!strcasecmp(name, "reserve"))
      {
       /*
        * Set port reservation mode...
	*/

        if (!value[0] || !strcasecmp(value, "on") ||
	    !strcasecmp(value, "yes") || !strcasecmp(value, "true") ||
	    !strcasecmp(value, "rfc1179"))
	  reserve = RESERVE_RFC1179;
	else if (!strcasecmp(value, "any"))
	  reserve = RESERVE_ANY;
	else
	  reserve = RESERVE_NONE;
      }
      else if (!strcasecmp(name, "manual_copies"))
      {
       /*
        * Set manual copies...
	*/

        manual_copies = !value[0] || !strcasecmp(value, "on") ||
	 		!strcasecmp(value, "yes") || !strcasecmp(value, "true");
      }
      else if (!strcasecmp(name, "sanitize_title"))
      {
       /*
        * Set sanitize title...
	*/

        sanitize_title = !value[0] || !strcasecmp(value, "on") ||
	 		!strcasecmp(value, "yes") || !strcasecmp(value, "true");
      }
      else if (!strcasecmp(name, "timeout"))
      {
       /*
        * Set the timeout...
	*/

	if (atoi(value) > 0)
	  timeout = atoi(value);
      }
      else if (!strcasecmp(name, "contimeout"))
      {
       /*
        * Set the timeout...
	*/

	if (atoi(value) > 0)
	  contimeout = atoi(value);
      }
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

    status = lpd_queue(hostname, port, resource + 1, filename,
                       username, title, copies,
		       banner, format, order, reserve, manual_copies,
		       timeout, contimeout);

    if (!status)
      fprintf(stderr, "PAGE: 1 %d\n", atoi(argv[4]));
  }
  else
    status = lpd_queue(hostname, port, resource + 1, filename,
                       username, title, 1,
		       banner, format, order, reserve, 1,
		       timeout, contimeout);

 /*
  * Remove the temporary file if necessary...
  */

  if (tmpfilename[0])
    unlink(tmpfilename);

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
            int  timeout,	/* I - Seconds to wait for a response */
            char *format,	/* I - printf()-style format string */
            ...)		/* I - Additional args as necessary */
{
  va_list	ap;		/* Argument pointer */
  char		buf[1024];	/* Output buffer */
  int		bytes;		/* Number of bytes to output */
  char		status;		/* Status from command */


 /*
  * Don't try to send commands if the job has been cancelled...
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

  fprintf(stderr, "DEBUG: Sending command string (%d bytes)...\n", bytes);

  if (lpd_write(fd, buf, bytes) < bytes)
  {
    perror("ERROR: Unable to send LPD command");
    return (-1);
  }

 /*
  * Read back the status from the command and return it...
  */

  fprintf(stderr, "DEBUG: Reading command status...\n");

  alarm(timeout);

  if (recv(fd, &status, 1, 0) < 1)
  {
    fprintf(stderr, "WARNING: Remote host did not respond with command "
	            "status byte after %d seconds!\n", timeout);
    status = errno;
  }

  alarm(0);

  fprintf(stderr, "DEBUG: lpd_command returning %d\n", status);

  return (status);
}


/*
 * 'lpd_queue()' - Queue a file using the Line Printer Daemon protocol.
 */

static int				/* O - Zero on success, non-zero on failure */
lpd_queue(const char *hostname,		/* I - Host to connect to */
          int        port,		/* I - Port to connect on */
          const char *printer,		/* I - Printer/queue name */
	  const char *filename,		/* I - File to print */
          const char *user,		/* I - Requesting user */
	  const char *title,		/* I - Job title */
	  int        copies,		/* I - Number of copies */
	  int        banner,		/* I - Print LPD banner? */
          int        format,		/* I - Format specifier */
          int        order,		/* I - Order of data/control files */
	  int        reserve,		/* I - Reserve ports? */
	  int        manual_copies,	/* I - Do copies by hand... */
	  int        timeout,		/* I - Timeout... */
	  int        contimeout)	/* I - Connection timeout */
{
  FILE			*fp;		/* Job file */
  char			localhost[255];	/* Local host name */
  int			error;		/* Error number */
  struct stat		filestats;	/* File statistics */
  int			lport;		/* LPD connection local port */
  int			fd;		/* LPD socket */
  char			control[10240],	/* LPD control 'file' */
			*cptr;		/* Pointer into control file string */
  char			status;		/* Status byte from command */
  char			portname[255];	/* Port name */
  http_addrlist_t	*addrlist,	/* Address list */
			*addr;		/* Socket address */
  int			copy;		/* Copies written */
  time_t		start_time;	/* Time of first connect */
#ifdef __APPLE__
  int			recoverable;	/* Recoverable error shown? */
#endif /* __APPLE__ */
  size_t		nbytes;		/* Number of bytes written */
  off_t			tbytes;		/* Total bytes written */
  char			buffer[65536];	/* Output buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction	action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Setup an alarm handler for timeouts...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGALRM, lpd_timeout);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  action.sa_handler = lpd_timeout;
  sigaction(SIGALRM, &action, NULL);
#else
  signal(SIGALRM, lpd_timeout);
#endif /* HAVE_SIGSET */

 /*
  * Find the printer...
  */

  sprintf(portname, "%d", port);

  if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\'!\n",
            hostname);
    return (CUPS_BACKEND_STOP);
  }

 /*
  * Remember when we starting trying to connect to the printer...
  */

#ifdef __APPLE__
  recoverable = 0;
#endif /* __APPLE__ */
  start_time  = time(NULL);

 /*
  * Loop forever trying to print the file...
  */

  while (!abort_job)
  {
   /*
    * First try to reserve a port for this connection...
    */

    fprintf(stderr, "INFO: Attempting to connect to host %s for printer %s\n",
            hostname, printer);

    for (lport = reserve == RESERVE_RFC1179 ? 732 : 1024, addr = addrlist;;
         addr = addr->next)
    {
     /*
      * Stop if this job has been cancelled...
      */

      if (abort_job)
      {
        httpAddrFreeList(addrlist);

        return (CUPS_BACKEND_FAILED);
      }

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
          perror("ERROR: Unable to create socket");
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
	  perror("ERROR: Unable to reserve port");
	  sleep(1);

	  continue;
	}
      }

     /*
      * Connect to the printer or server...
      */

      if (abort_job)
      {
        httpAddrFreeList(addrlist);

	close(fd);

	return (CUPS_BACKEND_FAILED);
      }

      if (!connect(fd, &(addr->addr.addr), httpAddrLength(&(addr->addr))))
	break;

      error = errno;
      close(fd);
      fd = -1;

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

        fprintf(stderr, "INFO: Unable to connect to %s, queuing on next printer in class...\n",
		hostname);

        httpAddrFreeList(addrlist);

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (CUPS_BACKEND_FAILED);
      }

      if (error == ECONNREFUSED || error == EHOSTDOWN ||
          error == EHOSTUNREACH)
      {
        if (contimeout && (time(NULL) - start_time) > contimeout)
	{
	  fputs("ERROR: Printer not responding!\n", stderr);
	  return (CUPS_BACKEND_FAILED);
	}

#ifdef __APPLE__
        recoverable = 1;
	fprintf(stderr, "WARNING: recoverable: "
#else
	fprintf(stderr, "WARNING: "
#endif /* __APPLE__ */
	                "Network host \'%s\' is busy, down, or "
	                "unreachable; will retry in 30 seconds...\n",
                hostname);
	sleep(30);
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
#ifdef __APPLE__
        recoverable = 1;
	perror("ERROR: recoverable: "
#else
	perror("ERROR: "
#endif /* __APPLE__ */
	       "Unable to connect to printer; will retry in 30 seconds...");
        sleep(30);
      }
    }

#ifdef __APPLE__
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
#endif /* __APPLE__ */

    fprintf(stderr, "INFO: Connected to %s...\n", hostname);
    fprintf(stderr, "DEBUG: Connected on ports %d (local %d)...\n", port,
            lport);

   /*
    * Next, open the print file and figure out its size...
    */

    if (stat(filename, &filestats))
    {
      httpAddrFreeList(addrlist);
      close(fd);

      perror("ERROR: unable to stat print file");
      return (CUPS_BACKEND_FAILED);
    }

    filestats.st_size *= manual_copies;

    if ((fp = fopen(filename, "rb")) == NULL)
    {
      httpAddrFreeList(addrlist);
      close(fd);

      perror("ERROR: unable to open print file for reading");
      return (CUPS_BACKEND_FAILED);
    }

   /*
    * Send a job header to the printer, specifying no banner page and
    * literal output...
    */

    if (lpd_command(fd, timeout, "\002%s\n",
                    printer))		/* Receive print job(s) */
    {
      httpAddrFreeList(addrlist);
      close(fd);
      return (CUPS_BACKEND_FAILED);
    }

    httpGetHostname(localhost, sizeof(localhost));

    snprintf(control, sizeof(control),
             "H%.31s\n"		/* RFC 1179, Section 7.2 - host name <= 31 chars */
	     "P%.31s\n"		/* RFC 1179, Section 7.2 - user name <= 31 chars */
	     "J%.99s\n",	/* RFC 1179, Section 7.2 - job name <= 99 chars */
	     localhost, user, title);
    cptr = control + strlen(control);

    if (banner)
    {
      snprintf(cptr, sizeof(control) - (cptr - control),
               "C%.31s\n"	/* RFC 1179, Section 7.2 - class name <= 31 chars */
	       "L%s\n",
               localhost, user);
      cptr   += strlen(cptr);
    }

    while (copies > 0)
    {
      snprintf(cptr, sizeof(control) - (cptr - control), "%cdfA%03d%.15s\n",
               format, (int)getpid() % 1000, localhost);
      cptr   += strlen(cptr);
      copies --;
    }

    snprintf(cptr, sizeof(control) - (cptr - control),
             "UdfA%03d%.15s\n"
	     "N%.131s\n",	/* RFC 1179, Section 7.2 - sourcefile name <= 131 chars */
             (int)getpid() % 1000, localhost, title);

    fprintf(stderr, "DEBUG: Control file is:\n%s", control);

    if (order == ORDER_CONTROL_DATA)
    {
      if (lpd_command(fd, timeout, "\002%d cfA%03.3d%.15s\n", strlen(control),
                      (int)getpid() % 1000, localhost))
      {
        httpAddrFreeList(addrlist);
	close(fd);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "INFO: Sending control file (%u bytes)\n",
              (unsigned)strlen(control));

      if (lpd_write(fd, control, strlen(control) + 1) < (strlen(control) + 1))
      {
	status = errno;
	perror("ERROR: Unable to write control file");
      }
      else
      {
        alarm(timeout);

        if (read(fd, &status, 1) < 1)
	{
	  fprintf(stderr, "WARNING: Remote host did not respond with control "
	                  "status byte after %d seconds!\n", timeout);
	  status = errno;
	}

        alarm(0);
      }

      if (status != 0)
	fprintf(stderr, "ERROR: Remote host did not accept control file (%d)\n",
        	status);
      else
	fputs("INFO: Control file sent successfully\n", stderr);
    }
    else
      status = 0;

    if (status == 0)
    {
     /*
      * Send the print file...
      */

      if (lpd_command(fd, timeout, "\003" CUPS_LLFMT " dfA%03.3d%.15s\n",
                      CUPS_LLCAST filestats.st_size, (int)getpid() % 1000,
		      localhost))
      {
        httpAddrFreeList(addrlist);
	close(fd);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "INFO: Sending data file (" CUPS_LLFMT " bytes)\n",
              CUPS_LLCAST filestats.st_size);

      tbytes = 0;
      for (copy = 0; copy < manual_copies; copy ++)
      {
	rewind(fp);

	while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
	{
	  fprintf(stderr, "INFO: Spooling LPR job, %.0f%% complete...\n",
        	  100.0 * tbytes / filestats.st_size);

	  if (lpd_write(fd, buffer, nbytes) < nbytes)
	  {
            perror("ERROR: Unable to send print file to printer");
            break;
	  }
	  else
            tbytes += nbytes;
	}
      }

      if (tbytes < filestats.st_size)
	status = errno;
      else if (lpd_write(fd, "", 1) < 1)
      {
        perror("ERROR: Unable to send trailing nul to printer");
	status = errno;
      }
      else
      {
       /*
        * Read the status byte from the printer; if we can't read the byte
	* back now, we should set status to "errno", however at this point
	* we know the printer got the whole file and we don't necessarily
	* want to requeue it over and over...
	*/

	alarm(timeout);

        if (recv(fd, &status, 1, 0) < 1)
	{
	  fprintf(stderr, "WARNING: Remote host did not respond with data "
	                  "status byte after %d seconds!\n", timeout);
	  status = 0;
        }

	alarm(0);
      }

      if (status != 0)
	fprintf(stderr, "ERROR: Remote host did not accept data file (%d)\n",
        	status);
      else
	fputs("INFO: Data file sent successfully\n", stderr);
    }

    if (status == 0 && order == ORDER_DATA_CONTROL)
    {
      if (lpd_command(fd, timeout, "\002%d cfA%03.3d%.15s\n", strlen(control),
                      (int)getpid() % 1000, localhost))
      {
        httpAddrFreeList(addrlist);
	close(fd);

        return (CUPS_BACKEND_FAILED);
      }

      fprintf(stderr, "INFO: Sending control file (%lu bytes)\n",
              (unsigned long)strlen(control));

      if (lpd_write(fd, control, strlen(control) + 1) < (strlen(control) + 1))
      {
	status = errno;
	perror("ERROR: Unable to write control file");
      }
      else
      {
        alarm(timeout);

        if (read(fd, &status, 1) < 1)
	{
	  fprintf(stderr, "WARNING: Remote host did not respond with control "
	                  "status byte after %d seconds!\n", timeout);
	  status = errno;
	}

	alarm(0);
      }

      if (status != 0)
	fprintf(stderr, "ERROR: Remote host did not accept control file (%d)\n",
        	status);
      else
	fputs("INFO: Control file sent successfully\n", stderr);
    }

   /*
    * Close the socket connection and input file...
    */

    close(fd);
    fclose(fp);

    if (status == 0)
    {
      httpAddrFreeList(addrlist);

      return (CUPS_BACKEND_OK);
    }

   /*
    * Waiting for a retry...
    */

    sleep(30);
  }

  httpAddrFreeList(addrlist);

 /*
  * If we get here, then the job has been cancelled...
  */

  return (CUPS_BACKEND_FAILED);
}


/*
 * 'lpd_timeout()' - Handle timeout alarms...
 */

static void
lpd_timeout(int sig)			/* I - Signal number */
{
  (void)sig;

#if !defined(HAVE_SIGSET) && !defined(HAVE_SIGACTION)
  signal(SIGALRM, lpd_timeout);
#endif /* !HAVE_SIGSET && !HAVE_SIGACTION */
}


/*
 * 'lpd_write()' - Write a buffer of data to an LPD server.
 */

static int				/* O - Number of bytes written or -1 on error */
lpd_write(int  lpd_fd,			/* I - LPD socket */
          char *buffer,			/* I - Buffer to write */
	  int  length)			/* I - Number of bytes to write */
{
  int	bytes,				/* Number of bytes written */
	total;				/* Total number of bytes written */


  if (abort_job)
    return (-1);

  total = 0;
  while ((bytes = send(lpd_fd, buffer, length - total, 0)) >= 0)
  {
    total  += bytes;
    buffer += bytes;

    if (total == length)
      break;
  }

  if (bytes < 0)
    return (-1);
  else
    return (length);
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

#  ifdef AF_INET6
    if (family == AF_INET6)
      addr.ipv6.sin6_port = htons(*port);
    else
#  endif /* AF_INET6 */
    addr.ipv4.sin_port = htons(*port);

   /*
    * Try binding the port to the socket; return if all is OK...
    */

    if (!bind(fd, (struct sockaddr *)&addr, sizeof(addr)))
      return (fd);

   /*
    * Stop if we have any error other than "address already in use"...
    */

    if (errno != EADDRINUSE)
    {
#  ifdef WIN32
      closesocket(fd);
#  else
      close(fd);
#  endif /* WIN32 */

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
 * End of "$Id$".
 */
