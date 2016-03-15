/*
 * "$Id: runloop.c 11558 2014-02-06 18:33:34Z msweet $"
 *
 * Common run loop APIs for CUPS backends.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 2006-2007 by Easy Software Products, all rights reserved.
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
#include <limits.h>
#include <sys/select.h>


/*
 * 'backendDrainOutput()' - Drain pending print data to the device.
 */

int					/* O - 0 on success, -1 on error */
backendDrainOutput(int print_fd,	/* I - Print file descriptor */
                   int device_fd)	/* I - Device file descriptor */
{
  int		nfds;			/* Maximum file descriptor value + 1 */
  fd_set	input;			/* Input set for reading */
  ssize_t	print_bytes,		/* Print bytes read */
		bytes;			/* Bytes written */
  char		print_buffer[8192],	/* Print data buffer */
		*print_ptr;		/* Pointer into print data buffer */
  struct timeval timeout;		/* Timeout for read... */


  fprintf(stderr, "DEBUG: backendDrainOutput(print_fd=%d, device_fd=%d)\n",
          print_fd, device_fd);

 /*
  * Figure out the maximum file descriptor value to use with select()...
  */

  nfds = (print_fd > device_fd ? print_fd : device_fd) + 1;

 /*
  * Now loop until we are out of data from print_fd...
  */

  for (;;)
  {
   /*
    * Use select() to determine whether we have data to copy around...
    */

    FD_ZERO(&input);
    FD_SET(print_fd, &input);

    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    if (select(nfds, &input, NULL, NULL, &timeout) < 0)
      return (-1);

    if (!FD_ISSET(print_fd, &input))
      return (0);

    if ((print_bytes = read(print_fd, print_buffer,
			    sizeof(print_buffer))) < 0)
    {
     /*
      * Read error - bail if we don't see EAGAIN or EINTR...
      */

      if (errno != EAGAIN || errno != EINTR)
      {
	fprintf(stderr, "DEBUG: Read failed: %s\n", strerror(errno));
	_cupsLangPrintFilter(stderr, "ERROR", _("Unable to read print data."));
	return (-1);
      }

      print_bytes = 0;
    }
    else if (print_bytes == 0)
    {
     /*
      * End of file, return...
      */

      return (0);
    }

    fprintf(stderr, "DEBUG: Read %d bytes of print data...\n",
	    (int)print_bytes);

    for (print_ptr = print_buffer; print_bytes > 0;)
    {
      if ((bytes = write(device_fd, print_ptr, (size_t)print_bytes)) < 0)
      {
       /*
        * Write error - bail if we don't see an error we can retry...
	*/

        if (errno != ENOSPC && errno != ENXIO && errno != EAGAIN &&
	    errno != EINTR && errno != ENOTTY)
	{
	  _cupsLangPrintError("ERROR", _("Unable to write print data"));
	  return (-1);
	}
      }
      else
      {
        fprintf(stderr, "DEBUG: Wrote %d bytes of print data...\n", (int)bytes);

        print_bytes -= bytes;
	print_ptr   += bytes;
      }
    }
  }
}


/*
 * 'backendRunLoop()' - Read and write print and back-channel data.
 */

ssize_t					/* O - Total bytes on success, -1 on error */
backendRunLoop(
    int          print_fd,		/* I - Print file descriptor */
    int          device_fd,		/* I - Device file descriptor */
    int          snmp_fd,		/* I - SNMP socket or -1 if none */
    http_addr_t  *addr,			/* I - Address of device */
    int          use_bc,		/* I - Use back-channel? */
    int          update_state,		/* I - Update printer-state-reasons? */
    _cups_sccb_t side_cb)		/* I - Side-channel callback */
{
  int		nfds;			/* Maximum file descriptor value + 1 */
  fd_set	input,			/* Input set for reading */
		output;			/* Output set for writing */
  ssize_t	print_bytes,		/* Print bytes read */
		bc_bytes,		/* Backchannel bytes read */
		total_bytes,		/* Total bytes written */
		bytes;			/* Bytes written */
  int		paperout;		/* "Paper out" status */
  int		offline;		/* "Off-line" status */
  char		print_buffer[8192],	/* Print data buffer */
		*print_ptr,		/* Pointer into print data buffer */
		bc_buffer[1024];	/* Back-channel data buffer */
  struct timeval timeout;		/* Timeout for select() */
  time_t	curtime,		/* Current time */
		snmp_update = 0;
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  fprintf(stderr,
          "DEBUG: backendRunLoop(print_fd=%d, device_fd=%d, snmp_fd=%d, "
	  "addr=%p, use_bc=%d, side_cb=%p)\n",
          print_fd, device_fd, snmp_fd, addr, use_bc, side_cb);

 /*
  * If we are printing data from a print driver on stdin, ignore SIGTERM
  * so that the driver can finish out any page data, e.g. to eject the
  * current page.  We only do this for stdin printing as otherwise there
  * is no way to cancel a raw print job...
  */

  if (!print_fd)
  {
#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
    sigset(SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGTERM, &action, NULL);
#else
    signal(SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */
  }
  else if (print_fd < 0)
  {
   /*
    * Copy print data from stdin, but don't mess with the signal handlers...
    */

    print_fd = 0;
  }

 /*
  * Figure out the maximum file descriptor value to use with select()...
  */

  nfds = (print_fd > device_fd ? print_fd : device_fd) + 1;

 /*
  * Now loop until we are out of data from print_fd...
  */

  for (print_bytes = 0, print_ptr = print_buffer, offline = -1,
           paperout = -1, total_bytes = 0;;)
  {
   /*
    * Use select() to determine whether we have data to copy around...
    */

    FD_ZERO(&input);
    if (!print_bytes)
      FD_SET(print_fd, &input);
    if (use_bc)
      FD_SET(device_fd, &input);
    if (!print_bytes && side_cb)
      FD_SET(CUPS_SC_FD, &input);

    FD_ZERO(&output);
    if (print_bytes || (!use_bc && !side_cb))
      FD_SET(device_fd, &output);

    if (use_bc || side_cb)
    {
      timeout.tv_sec  = 5;
      timeout.tv_usec = 0;

      if (select(nfds, &input, &output, NULL, &timeout) < 0)
      {
       /*
	* Pause printing to clear any pending errors...
	*/

	if (errno == ENXIO && offline != 1 && update_state)
	{
	  fputs("STATE: +offline-report\n", stderr);
	  _cupsLangPrintFilter(stderr, "INFO",
	                       _("The printer is not connected."));
	  offline = 1;
	}
	else if (errno == EINTR && total_bytes == 0)
	{
	  fputs("DEBUG: Received an interrupt before any bytes were "
	        "written, aborting.\n", stderr);
          return (0);
	}

	sleep(1);
	continue;
      }
    }

   /*
    * Check if we have a side-channel request ready...
    */

    if (side_cb && FD_ISSET(CUPS_SC_FD, &input))
    {
     /*
      * Do the side-channel request, then start back over in the select
      * loop since it may have read from print_fd...
      */

      if ((*side_cb)(print_fd, device_fd, snmp_fd, addr, use_bc))
        side_cb = NULL;
      continue;
    }

   /*
    * Check if we have back-channel data ready...
    */

    if (FD_ISSET(device_fd, &input))
    {
      if ((bc_bytes = read(device_fd, bc_buffer, sizeof(bc_buffer))) > 0)
      {
	fprintf(stderr,
	        "DEBUG: Received " CUPS_LLFMT " bytes of back-channel data\n",
	        CUPS_LLCAST bc_bytes);
        cupsBackChannelWrite(bc_buffer, (size_t)bc_bytes, 1.0);
      }
      else if (bc_bytes < 0 && errno != EAGAIN && errno != EINTR)
      {
        fprintf(stderr, "DEBUG: Error reading back-channel data: %s\n",
	        strerror(errno));
	use_bc = 0;
      }
      else if (bc_bytes == 0)
        use_bc = 0;
    }

   /*
    * Check if we have print data ready...
    */

    if (FD_ISSET(print_fd, &input))
    {
      if ((print_bytes = read(print_fd, print_buffer,
                              sizeof(print_buffer))) < 0)
      {
       /*
        * Read error - bail if we don't see EAGAIN or EINTR...
	*/

	if (errno != EAGAIN || errno != EINTR)
	{
	  fprintf(stderr, "DEBUG: Read failed: %s\n", strerror(errno));
	  _cupsLangPrintFilter(stderr, "ERROR",
	                       _("Unable to read print data."));
	  return (-1);
	}

        print_bytes = 0;
      }
      else if (print_bytes == 0)
      {
       /*
        * End of file, break out of the loop...
	*/

        break;
      }

      print_ptr = print_buffer;

      fprintf(stderr, "DEBUG: Read %d bytes of print data...\n",
              (int)print_bytes);
    }

   /*
    * Check if the device is ready to receive data and we have data to
    * send...
    */

    if (print_bytes && FD_ISSET(device_fd, &output))
    {
      if ((bytes = write(device_fd, print_ptr, (size_t)print_bytes)) < 0)
      {
       /*
        * Write error - bail if we don't see an error we can retry...
	*/

        if (errno == ENOSPC)
	{
	  if (paperout != 1 && update_state)
	  {
	    fputs("STATE: +media-empty-warning\n", stderr);
	    fputs("DEBUG: Out of paper\n", stderr);
	    paperout = 1;
	  }
        }
	else if (errno == ENXIO)
	{
	  if (offline != 1 && update_state)
	  {
	    fputs("STATE: +offline-report\n", stderr);
	    _cupsLangPrintFilter(stderr, "INFO",
	                         _("The printer is not connected."));
	    offline = 1;
	  }
	}
	else if (errno != EAGAIN && errno != EINTR && errno != ENOTTY)
	{
	  _cupsLangPrintError("ERROR", _("Unable to write print data"));
	  return (-1);
	}
      }
      else
      {
        if (paperout && update_state)
	{
	  fputs("STATE: -media-empty-warning\n", stderr);
	  paperout = 0;
	}

	if (offline && update_state)
	{
	  fputs("STATE: -offline-report\n", stderr);
	  _cupsLangPrintFilter(stderr, "INFO",
	                       _("The printer is now connected."));
	  offline = 0;
	}

        fprintf(stderr, "DEBUG: Wrote %d bytes of print data...\n", (int)bytes);

        print_bytes -= bytes;
	print_ptr   += bytes;
	total_bytes += bytes;
      }
    }

   /*
    * Do SNMP updates periodically...
    */

    if (snmp_fd >= 0 && time(&curtime) >= snmp_update)
    {
      if (backendSNMPSupplies(snmp_fd, addr, NULL, NULL))
        snmp_update = INT_MAX;
      else
        snmp_update = curtime + 5;
    }
  }

 /*
  * Return with success...
  */

  return (total_bytes);
}


/*
 * 'backendWaitLoop()' - Wait for input from stdin while handling side-channel
 *                       queries.
 */

int					/* O - 1 if data is ready, 0 if not */
backendWaitLoop(
    int          snmp_fd,		/* I - SNMP socket or -1 if none */
    http_addr_t  *addr,			/* I - Address of device */
    int          use_bc,		/* I - Use back-channel? */
    _cups_sccb_t side_cb)		/* I - Side-channel callback */
{
  int			nfds;		/* Number of file descriptors */
  fd_set		input;		/* Input set for reading */
  time_t		curtime = 0,	/* Current time */
			snmp_update = 0;/* Last SNMP status update */
  struct timeval	timeout;	/* Timeout for select() */


  fprintf(stderr, "DEBUG: backendWaitLoop(snmp_fd=%d, addr=%p, side_cb=%p)\n",
	  snmp_fd, addr, side_cb);

 /*
  * Now loop until we receive data from stdin...
  */

  if (snmp_fd >= 0)
    snmp_update = time(NULL) + 5;

  for (;;)
  {
   /*
    * Use select() to determine whether we have data to copy around...
    */

    FD_ZERO(&input);
    FD_SET(0, &input);
    if (side_cb)
      FD_SET(CUPS_SC_FD, &input);

    if (snmp_fd >= 0)
    {
      curtime         = time(NULL);
      timeout.tv_sec  = curtime >= snmp_update ? 0 : snmp_update - curtime;
      timeout.tv_usec = 0;

      nfds = select(CUPS_SC_FD + 1, &input, NULL, NULL, &timeout);
    }
    else
      nfds = select(CUPS_SC_FD + 1, &input, NULL, NULL, NULL);

    if (nfds < 0)
    {
     /*
      * Pause printing to clear any pending errors...
      */

      if (errno == EINTR)
      {
	fputs("DEBUG: Received an interrupt before any bytes were "
	      "written, aborting.\n", stderr);
	return (0);
      }

      sleep(1);
      continue;
    }

   /*
    * Check for input on stdin...
    */

    if (FD_ISSET(0, &input))
      break;

   /*
    * Check if we have a side-channel request ready...
    */

    if (side_cb && FD_ISSET(CUPS_SC_FD, &input))
    {
     /*
      * Do the side-channel request, then start back over in the select
      * loop since it may have read from print_fd...
      */

      if ((*side_cb)(0, -1, snmp_fd, addr, use_bc))
        side_cb = NULL;
      continue;
    }

   /*
    * Do SNMP updates periodically...
    */

    if (snmp_fd >= 0 && curtime >= snmp_update)
    {
      if (backendSNMPSupplies(snmp_fd, addr, NULL, NULL))
        snmp_fd = -1;
      else
        snmp_update = curtime + 5;
    }
  }

 /*
  * Return with success...
  */

  return (1);
}


/*
 * End of "$Id: runloop.c 11558 2014-02-06 18:33:34Z msweet $".
 */
