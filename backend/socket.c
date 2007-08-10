/*
 * "$Id$"
 *
 *   AppSocket backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
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
 *   side_cb() - Handle side-channel requests...
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

static void	side_cb(int print_fd, int device_fd, int use_bc);
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
  char		method[255],		/* Method in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info (not used) */
		resource[1024],		/* Resource info (not used) */
		*options,		/* Pointer to options */
		name[255],		/* Name of option */
		value[255],		/* Value of option */
		*ptr;			/* Pointer into name or value */
  int		print_fd;		/* Print file */
  int		copies;			/* Number of copies to print */
  time_t	start_time;		/* Time of first connect */
  int		recoverable;		/* Recoverable error shown? */
  int		contimeout;		/* Connection timeout */
  int		waiteof;		/* Wait for end-of-file? */
  int		port;			/* Port number */
  char		portname[255];		/* Port name */
  int		delay;			/* Delay for retries... */
  int		device_fd;		/* AppSocket */
  int		error;			/* Error code (if any) */
  http_addrlist_t *addrlist,		/* Address list */
		  *addr;		/* Connected address */
  char		addrname[256];		/* Address name */
  ssize_t	tbytes;			/* Total number of bytes written */
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
    puts("network socket \"Unknown\" \"AppSocket/HP JetDirect\"");
    return (CUPS_BACKEND_OK);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, _("Usage: %s job-id user title copies options [file]\n"),
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
      perror("ERROR: unable to open print file");
      return (CUPS_BACKEND_FAILED);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the hostname and port number from the URI...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, cupsBackendDeviceURI(argv),
                  method, sizeof(method), username, sizeof(username),
		  hostname, sizeof(hostname), &port,
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

      if (!strcasecmp(name, "waiteof"))
      {
       /*
        * Set the wait-for-eof value...
	*/

        waiteof = !value[0] || !strcasecmp(value, "on") ||
		  !strcasecmp(value, "yes") || !strcasecmp(value, "true");
      }
      else if (!strcasecmp(name, "contimeout"))
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
  * Then try to connect to the remote host...
  */

  recoverable = 0;
  start_time  = time(NULL);

  sprintf(portname, "%d", port);

  if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
  {
    fprintf(stderr, _("ERROR: Unable to locate printer \'%s\'!\n"), hostname);
    return (CUPS_BACKEND_STOP);
  }

  fprintf(stderr, _("INFO: Attempting to connect to host %s on port %d\n"),
          hostname, port);

  fputs("STATE: +connecting-to-device\n", stderr);

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

        fputs(_("INFO: Unable to contact printer, queuing on next "
		"printer in class...\n"), stderr);

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
	  fputs(_("ERROR: Printer not responding!\n"), stderr);
	  return (CUPS_BACKEND_FAILED);
	}

        recoverable = 1;

	fprintf(stderr,
	        _("WARNING: recoverable: Network host \'%s\' is busy; will "
		  "retry in %d seconds...\n"),
		hostname, delay);

	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else
      {
        recoverable = 1;

        fprintf(stderr, "DEBUG: Connection error: %s\n", strerror(errno));
	fputs(_("ERROR: recoverable: Unable to connect to printer; will "
	        "retry in 30 seconds...\n"), stderr);
	sleep(30);
      }
    }
    else
      break;
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

  fputs("STATE: -connecting-to-device\n", stderr);
  fprintf(stderr, _("INFO: Connected to %s...\n"), hostname);

#ifdef AF_INET6
  if (addr->addr.addr.sa_family == AF_INET6)
    fprintf(stderr, "DEBUG: Connected to [%s]:%d (IPv6)...\n", 
	    httpAddrString(&addr->addr, addrname, sizeof(addrname)),
	    ntohs(addr->addr.ipv6.sin6_port));
  else
#endif /* AF_INET6 */
    if (addr->addr.addr.sa_family == AF_INET)
      fprintf(stderr, "DEBUG: Connected to %s:%d (IPv4)...\n",
	      httpAddrString(&addr->addr, addrname, sizeof(addrname)),
	      ntohs(addr->addr.ipv4.sin_port));

 /*
  * Print everything...
  */

  tbytes = 0;

  while (copies > 0 && tbytes >= 0)
  {
    copies --;

    if (print_fd != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(print_fd, 0, SEEK_SET);
    }

    tbytes = backendRunLoop(print_fd, device_fd, 1, side_cb);

    if (print_fd != 0 && tbytes >= 0)
      fprintf(stderr,
#ifdef HAVE_LONG_LONG
              _("INFO: Sent print file, %lld bytes...\n"),
#else
              _("INFO: Sent print file, %ld bytes...\n"),
#endif /* HAVE_LONG_LONG */
              CUPS_LLCAST tbytes);
  }

 /*
  * Get any pending back-channel data...
  */

  while (wait_bc(device_fd, 5) > 0);

  if (waiteof)
  {
   /*
    * Shutdown the socket and wait for the other end to finish...
    */

    fputs(_("INFO: Print file sent, waiting for printer to finish...\n"),
          stderr);

    shutdown(device_fd, 1);

    while (wait_bc(device_fd, 90) > 0);
  }

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

  if (tbytes >= 0)
    fputs(_("INFO: Ready to print.\n"), stderr);

  return (tbytes < 0 ? CUPS_BACKEND_FAILED : CUPS_BACKEND_OK);
}


/*
 * 'side_cb()' - Handle side-channel requests...
 */

static void
side_cb(int print_fd,			/* I - Print file */
        int device_fd,			/* I - Device file */
	int use_bc)			/* I - Using back-channel? */
{
  cups_sc_command_t	command;	/* Request command */
  cups_sc_status_t	status;		/* Request/response status */
  char			data[2048];	/* Request/response data */
  int			datalen;	/* Request/response data size */
  const char		*device_id;	/* 1284DEVICEID env var */


  datalen = sizeof(data);

  if (cupsSideChannelRead(&command, &status, data, &datalen, 1.0))
  {
    fputs(_("WARNING: Failed to read side-channel request!\n"), stderr);
    return;
  }

  switch (command)
  {
    case CUPS_SC_CMD_DRAIN_OUTPUT :
       /*
        * Our sockets disable the Nagle algorithm and data is sent immediately.
	*/

        if (backendDrainOutput(print_fd, device_fd))
	  status = CUPS_SC_STATUS_IO_ERROR;
	else 
          status = CUPS_SC_STATUS_OK;

	datalen = 0;
        break;

    case CUPS_SC_CMD_GET_BIDI :
        data[0] = use_bc;
        datalen = 1;
        break;

    case CUPS_SC_CMD_GET_DEVICE_ID :
        if ((device_id = getenv("1284DEVICEID")) != NULL)
	{
	  strlcpy(data, device_id, sizeof(data));
	  datalen = (int)strlen(data);
	  break;
	}

    default :
        status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	datalen = 0;
	break;
  }

  cupsSideChannelWrite(command, status, data, datalen, 1.0);
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
      fprintf(stderr, "DEBUG: Received %d bytes of back-channel data!\n",
	      (int)bytes);
      cupsBackChannelWrite(buffer, bytes, 1.0);
    }

    return (bytes);
  }
  else
    return (-1);
}


/*
 * End of "$Id$".
 */
