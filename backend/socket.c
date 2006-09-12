/*
 * "$Id$"
 *
 *   AppSocket backend for the Common UNIX Printing System (CUPS).
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
 *   main() - Send a file to the printer or server.
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
  int		waiteof;		/* Wait for end-of-file? */
  int		port;			/* Port number */
  char		portname[255];		/* Port name */
  int		delay;			/* Delay for retries... */
  int		device_fd;		/* AppSocket */
  int		error;			/* Error code (if any) */
  http_addrlist_t *addrlist;		/* Address list */
  ssize_t	tbytes;			/* Total number of bytes written */
  struct timeval timeout;		/* Timeout for select() */
  fd_set	input;			/* Input set for select() */
  ssize_t	bc_bytes;		/* Number of back-channel bytes read */
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
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
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

  waiteof = 1;

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
    }
  }

 /*
  * Then try to connect to the remote host...
  */

  sprintf(portname, "%d", port);

  if ((addrlist = httpAddrGetList(hostname, AF_UNSPEC, portname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\'!\n", hostname);
    return (CUPS_BACKEND_STOP);
  }

  fprintf(stderr, "INFO: Attempting to connect to host %s on port %d\n",
          hostname, port);

  fputs("STATE: +connecting-to-device\n", stderr);

  for (delay = 5;;)
  {
    if (!httpAddrConnect(addrlist, &device_fd))
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

        fprintf(stderr, "INFO: Unable to connect to \"%s\", queuing on next printer in class...\n",
		hostname);

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (CUPS_BACKEND_FAILED);
      }

      if (error == ECONNREFUSED || error == EHOSTDOWN ||
          error == EHOSTUNREACH)
      {
	fprintf(stderr,
	        "INFO: Network host \'%s\' is busy; will retry in %d seconds...\n",
                hostname, delay);
	sleep(delay);

	if (delay < 30)
	  delay += 5;
      }
      else
      {
	perror("ERROR: Unable to connect to printer (retrying in 30 seconds)");
	sleep(30);
      }
    }
    else
      break;
  }

  fputs("STATE: -connecting-to-device\n", stderr);

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

    tbytes = backendRunLoop(print_fd, device_fd, 1);

    if (print_fd != 0 && tbytes >= 0)
      fprintf(stderr, "INFO: Sent print file, " CUPS_LLFMT " bytes...\n",
	      CUPS_LLCAST tbytes);
  }

  if (waiteof)
  {
   /*
    * Shutdown the socket and wait for the other end to finish...
    */

    fputs("INFO: Print file sent, waiting for printer to finish...\n", stderr);

    shutdown(device_fd, 1);

    for (;;)
    {
     /*
      * Wait a maximum of 90 seconds for backchannel data or a closed
      * connection...
      */

      timeout.tv_sec  = 90;
      timeout.tv_usec = 0;

      FD_ZERO(&input);
      FD_SET(device_fd, &input);

      if (select(device_fd + 1, &input, NULL, NULL, &timeout) > 0)
      {
       /*
	* Grab the data coming back and spit it out to stderr...
	*/

	if ((bc_bytes = read(device_fd, resource, sizeof(resource))) > 0)
	{
	  fprintf(stderr, "DEBUG: Received %d bytes of back-channel data!\n",
		  (int)bc_bytes);
	  cupsBackChannelWrite(resource, bc_bytes, 1.0);
	}
	else
	  break;
      }
      else
	break;
    }
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
    fputs("INFO: Ready to print.\n", stderr);

  return (tbytes < 0 ? CUPS_BACKEND_FAILED : CUPS_BACKEND_OK);
}


/*
 * End of "$Id$".
 */
