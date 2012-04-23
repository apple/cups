/*
 * "$Id: socket.c 7881 2008-08-28 20:21:56Z mike $"
 *
 *   AppSocket backend for CUPS.
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
 *   main()    - Send a file to the printer or server.
 *   wait_bc() - Wait for back-channel data...
 */

/*
 * Include necessary headers.
 */

#include <cups/http-private.h>
#include "backend-private.h"
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#  include <winsock.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static int	wait_bc(int device_fd, int secs);


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
		username[255],		/* Username info (not used) */
		resource[1024],		/* Resource info (not used) */
		*options,		/* Pointer to options */
		*name,			/* Name of option */
		*value,			/* Value of option */
		sep;			/* Option separator */
  int		print_fd;		/* Print file */
  int		copies;			/* Number of copies to print */
  time_t	start_time;		/* Time of first connect */
#ifdef __APPLE__
  time_t	current_time,		/* Current time */
		wait_time;		/* Time to wait before shutting down socket */
#endif /* __APPLE__ */
  int		contimeout;		/* Connection timeout */
  int		waiteof;		/* Wait for end-of-file? */
  int		port;			/* Port number */
  char		portname[255];		/* Port name */
  int		delay;			/* Delay for retries... */
  int		device_fd;		/* AppSocket */
  int		error;			/* Error code (if any) */
  http_addrlist_t *addrlist,		/* Address list */
		*addr;			/* Connected address */
  char		addrname[256];		/* Address name */
  int		snmp_fd,		/* SNMP socket */
		start_count,		/* Page count via SNMP at start */
		page_count,		/* Page count via SNMP */
		have_supplies;		/* Printer supports supply levels? */
  ssize_t	bytes = 0,		/* Initial bytes read */
		tbytes;			/* Total number of bytes written */
  char		buffer[1024];		/* Initial print buffer */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Make sure status messages are not buffered...
  */

  setbuf(stderr, NULL);

 /*
  * Ignore SIGPIPE signals...
  */

#ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGSET */

 /*
  * Check command-line...
  */

  if (argc == 1)
  {
    printf("network socket \"Unknown\" \"%s\"\n",
           _cupsLangString(cupsLangDefault(), _("AppSocket/HP JetDirect")));
    return (CUPS_BACKEND_OK);
  }
  else if (argc < 6 || argc > 7)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]"),
                    argv[0]);
    return (CUPS_BACKEND_FAILED);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    print_fd = 0;
    copies   = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((print_fd = open(argv[6], O_RDONLY)) < 0)
    {
      _cupsLangPrintError("ERROR", _("Unable to open print file"));
      return (CUPS_BACKEND_FAILED);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the hostname and port number from the URI...
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

  if (port == 0)
    port = 9100;	/* Default to HP JetDirect/Tektronix PhaserShare */

 /*
  * Get options, if any...
  */

  waiteof    = 1;
  contimeout = 7 * 24 * 60 * 60;

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

      if (!_cups_strcasecmp(name, "waiteof"))
      {
       /*
        * Set the wait-for-eof value...
	*/

        waiteof = !value[0] || !_cups_strcasecmp(value, "on") ||
		  !_cups_strcasecmp(value, "yes") || !_cups_strcasecmp(value, "true");
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

 /*
  * Then try finding the remote host...
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

  if (print_fd == 0)
  {
    if (!backendWaitLoop(snmp_fd, &(addrlist->addr), 1, backendNetworkSideCB))
      return (CUPS_BACKEND_OK);
    else if ((bytes = read(0, buffer, sizeof(buffer))) <= 0)
      return (CUPS_BACKEND_OK);
  }

 /*
  * Connect to the printer...
  */

  fprintf(stderr, "DEBUG: Connecting to %s:%d\n", hostname, port);
  _cupsLangPrintFilter(stderr, "INFO", _("Connecting to printer."));

  for (delay = 5;;)
  {
    if ((addr = httpAddrConnect(addrlist, &device_fd)) == NULL)
    {
      error     = errno;
      device_fd = -1;

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
				   _("The printer is unreachable at this "
				     "time."));
	      break;

	  case ECONNREFUSED :
	  default :
	      _cupsLangPrintFilter(stderr, "WARNING",
	                           _("The printer is in use."));
	      break;
        }

	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else
      {
	_cupsLangPrintFilter(stderr, "ERROR",
	                     _("The printer is not responding."));
	sleep(30);
      }
    }
    else
      break;
  }

  fputs("STATE: -connecting-to-device\n", stderr);
  _cupsLangPrintFilter(stderr, "INFO", _("Connected to printer."));

  fprintf(stderr, "DEBUG: Connected to %s:%d...\n",
	  httpAddrString(&(addr->addr), addrname, sizeof(addrname)),
	  _httpAddrPort(&(addr->addr)));

 /*
  * Print everything...
  */

  tbytes = 0;

  if (bytes > 0)
    tbytes += write(device_fd, buffer, bytes);

  while (copies > 0 && tbytes >= 0)
  {
    copies --;

    if (print_fd != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(print_fd, 0, SEEK_SET);
    }

    tbytes = backendRunLoop(print_fd, device_fd, snmp_fd, &(addrlist->addr), 1,
                            0, backendNetworkSideCB);

    if (print_fd != 0 && tbytes >= 0)
      _cupsLangPrintFilter(stderr, "INFO", _("Print file sent."));
  }

#ifdef __APPLE__
 /*
  * Wait up to 5 seconds to get any pending back-channel data...
  */

  wait_time = time(NULL) + 5;
  while (wait_time >= time(&current_time))
    if (wait_bc(device_fd, wait_time - current_time) <= 0)
      break;
#endif /* __APPLE__ */

  if (waiteof)
  {
   /*
    * Shutdown the socket and wait for the other end to finish...
    */

    _cupsLangPrintFilter(stderr, "INFO", _("Waiting for printer to finish."));

    shutdown(device_fd, 1);

    while (wait_bc(device_fd, 90) > 0);
  }

 /*
  * Collect the final page count as needed...
  */

  if (have_supplies &&
      !backendSNMPSupplies(snmp_fd, &(addrlist->addr), &page_count, NULL) &&
      page_count > start_count)
    fprintf(stderr, "PAGE: total %d\n", page_count - start_count);

 /*
  * Close the socket connection...
  */

  close(device_fd);

  httpAddrFreeList(addrlist);

 /*
  * Close the input file and return...
  */

  if (print_fd != 0)
    close(print_fd);

  return (CUPS_BACKEND_OK);
}


/*
 * 'wait_bc()' - Wait for back-channel data...
 */

static int				/* O - # bytes read or -1 on error */
wait_bc(int device_fd,			/* I - Socket */
        int secs)			/* I - Seconds to wait */
{
  struct timeval timeout;		/* Timeout for select() */
  fd_set	input;			/* Input set for select() */
  ssize_t	bytes;			/* Number of back-channel bytes read */
  char		buffer[1024];		/* Back-channel buffer */


 /*
  * Wait up to "secs" seconds for backchannel data...
  */

  timeout.tv_sec  = secs;
  timeout.tv_usec = 0;

  FD_ZERO(&input);
  FD_SET(device_fd, &input);

  if (select(device_fd + 1, &input, NULL, NULL, &timeout) > 0)
  {
   /*
    * Grab the data coming back and spit it out to stderr...
    */

    if ((bytes = read(device_fd, buffer, sizeof(buffer))) > 0)
    {
      fprintf(stderr, "DEBUG: Received %d bytes of back-channel data\n",
	      (int)bytes);
      cupsBackChannelWrite(buffer, bytes, 1.0);
    }

    return (bytes);
  }
  else
    return (-1);
}


/*
 * End of "$Id: socket.c 7881 2008-08-28 20:21:56Z mike $".
 */
