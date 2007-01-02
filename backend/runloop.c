/*
 * "$Id$"
 *
 *   Common run loop API for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2006-2007 by Easy Software Products, all rights reserved.
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
 *   backendRunLoop() - Read and write print and back-channel data.
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"
#ifdef __hpux
#  include <sys/time.h>
#else
#  include <sys/select.h>
#endif /* __hpux */


/*
 * 'backendRunLoop()' - Read and write print and back-channel data.
 */

ssize_t					/* O - Total bytes on success, -1 on error */
backendRunLoop(
    int  print_fd,			/* I - Print file descriptor */
    int  device_fd,			/* I - Device file descriptor */
    int  use_bc,			/* I - Use back-channel? */
    void (*side_cb)(int, int, int))	/* I - Side-channel callback */
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
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  fprintf(stderr, "DEBUG: backendRunLoop(print_fd=%d, device_fd=%d, use_bc=%d)\n",
          print_fd, device_fd, use_bc);

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

 /*
  * Figure out the maximum file descriptor value to use with select()...
  */

  nfds = (print_fd > device_fd ? print_fd : device_fd) + 1;

 /*
  * Now loop until we are out of data from print_fd...
  */

  for (print_bytes = 0, print_ptr = print_buffer, offline = 0, paperout = 0, total_bytes = 0;;)
  {
   /*
    * Use select() to determine whether we have data to copy around...
    */

    FD_ZERO(&input);
    if (!print_bytes)
      FD_SET(print_fd, &input);
    if (use_bc)
      FD_SET(device_fd, &input);
    if (side_cb)
      FD_SET(CUPS_SC_FD, &input);

    FD_ZERO(&output);
    if (print_bytes || !use_bc)
      FD_SET(device_fd, &output);

    if (use_bc || side_cb)
    {
      if (select(nfds, &input, &output, NULL, NULL) < 0)
      {
       /*
	* Pause printing to clear any pending errors...
	*/

	if (errno == ENXIO && !offline)
	{
	  fputs("STATE: +offline-error\n", stderr);
	  fputs("INFO: Printer is currently off-line.\n", stderr);
	  offline = 1;
	}

	sleep(1);
	continue;
      }
    }

   /*
    * Check if we have a side-channel request ready...
    */

    if (side_cb && FD_ISSET(CUPS_SC_FD, &input))
      (*side_cb)(print_fd, device_fd, use_bc);

   /*
    * Check if we have back-channel data ready...
    */

    if (FD_ISSET(device_fd, &input))
    {
      if ((bc_bytes = read(device_fd, bc_buffer, sizeof(bc_buffer))) > 0)
      {
	fprintf(stderr,
	        "DEBUG: Received " CUPS_LLFMT " bytes of back-channel data!\n",
	        CUPS_LLCAST bc_bytes);
        cupsBackChannelWrite(bc_buffer, bc_bytes, 1.0);
      }
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
	  perror("ERROR: Unable to read print data");
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
      if ((bytes = write(device_fd, print_ptr, print_bytes)) < 0)
      {
       /*
        * Write error - bail if we don't see an error we can retry...
	*/

        if (errno == ENOSPC)
	{
	  if (!paperout)
	  {
	    fputs("ERROR: Out of paper!\n", stderr);
	    fputs("STATE: +media-empty-error\n", stderr);
	    paperout = 1;
	  }
        }
	else if (errno == ENXIO)
	{
	  if (!offline)
	  {
	    fputs("STATE: +offline-error\n", stderr);
	    fputs("INFO: Printer is currently off-line.\n", stderr);
	    offline = 1;
	  }
	}
	else if (errno != EAGAIN && errno != EINTR && errno != ENOTTY)
	{
	  perror("ERROR: Unable to write print data");
	  return (-1);
	}
      }
      else
      {
        if (paperout)
	{
	  fputs("STATE: -media-empty-error\n", stderr);
	  paperout = 0;
	}

	if (offline)
	{
	  fputs("STATE: -offline-error\n", stderr);
	  fputs("INFO: Printer is now on-line.\n", stderr);
	  offline = 0;
	}

        fprintf(stderr, "DEBUG: Wrote %d bytes of print data...\n", (int)bytes);

        print_bytes -= bytes;
	print_ptr   += bytes;
	total_bytes += bytes;
      }
    }
  }

 /*
  * Return with success...
  */

  return (total_bytes);
}


/*
 * End of "$Id$".
 */
