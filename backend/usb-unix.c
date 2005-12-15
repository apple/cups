/*
 * "$Id$"
 *
 *   USB port backend for the Common UNIX Printing System (CUPS).
 *
 *   This file is included from "usb.c" when compiled on UNIX/Linux.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *   main()         - Send a file to the specified USB port.
 *   list_devices() - List all USB devices.
 */

/*
 * Include necessary headers.
 */

#include "ieee1284.c"
#include <sys/select.h>


/*
 * Local functions...
 */

int	open_device(const char *uri);


/*
 * 'print_device()' - Print a file to a USB device.
 */

int					/* O - Exit status */
print_device(const char *uri,		/* I - Device URI */
             const char *hostname,	/* I - Hostname/manufacturer */
             const char *resource,	/* I - Resource/modelname */
	     const char *options,	/* I - Device options/serial number */
	     int        fp,		/* I - File descriptor to print */
	     int        copies)		/* I - Copies to print */
{
  int		fd;			/* USB device */
  int		rbytes;			/* Number of bytes read */
  int		wbytes;			/* Number of bytes written */
  size_t	nbytes,			/* Number of bytes read */
		tbytes;			/* Total number of bytes written */
  char		buffer[8192],		/* Output buffer */
		*bufptr,		/* Pointer into buffer */
		backbuf[1024];		/* Backchannel buffer */
  struct termios opts;			/* Parallel port options */
  fd_set	input,			/* Input set for select() */
		output;			/* Output set for select() */
  int		paperout;		/* Paper out? */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */
#ifdef __linux
  unsigned int	status;			/* Port status (off-line, out-of-paper, etc.) */
#endif /* __linux */


 /*
  * Open the USB port device...
  */

  do
  {
    if ((fd = open_device(uri)) == -1)
    {
      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        fputs("INFO: Unable to open USB device, queuing on next printer in class...\n",
	      stderr);

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (CUPS_BACKEND_FAILED);
      }

      if (errno == EBUSY)
      {
        fputs("INFO: USB port busy; will retry in 30 seconds...\n", stderr);
	sleep(30);
      }
      else if (errno == ENXIO || errno == EIO || errno == ENOENT || errno == ENODEV)
      {
        fputs("INFO: Printer not connected; will retry in 30 seconds...\n", stderr);
	sleep(30);
      }
      else
      {
	fprintf(stderr, "ERROR: Unable to open USB device \"%s\": %s\n",
	        uri, strerror(errno));
	return (CUPS_BACKEND_FAILED);
      }
    }
  }
  while (fd < 0);

 /*
  * Set any options provided...
  */

  tcgetattr(fd, &opts);

  opts.c_lflag &= ~(ICANON | ECHO | ISIG);	/* Raw mode */

  /**** No options supported yet ****/

  tcsetattr(fd, TCSANOW, &opts);

 /*
  * Check printer status...
  */

  paperout = 0;

#if defined(__linux) && defined(LP_POUTPA)
 /*
  * Show the printer status before we send the file...
  */

  while (!ioctl(fd, LPGETSTATUS, &status))
  {
    fprintf(stderr, "DEBUG: LPGETSTATUS returned a port status of %02X...\n", status);

    if (status & LP_POUTPA)
    {
      fputs("WARNING: Media tray empty!\n", stderr);
      fputs("STATUS: +media-tray-empty-error\n", stderr);

      paperout = 1;
    }

    if (!(status & LP_PERRORP))
      fputs("WARNING: Printer fault!\n", stderr);
    else if (!(status & LP_PSELECD))
      fputs("WARNING: Printer off-line.\n", stderr);
    else
      break;

    sleep(5);
  }
#endif /* __linux && LP_POUTPA */

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  Only ignore SIGTERM if we are printing data from
  * stdin (otherwise you can't cancel raw jobs...)
  */

  if (!fp)
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
  * Finally, send the print file...
  */

  wbytes = 0;

  while (copies > 0)
  {
    copies --;

    if (fp != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(fp, 0, SEEK_SET);
    }

    tbytes = 0;
    while ((nbytes = read(fp, buffer, sizeof(buffer))) > 0)
    {
     /*
      * Write the print data to the printer...
      */

      tbytes += nbytes;
      bufptr = buffer;

      while (nbytes > 0)
      {
       /*
        * See if we are ready to read or write...
	*/

        do
	{
          FD_ZERO(&input);
	  FD_SET(fd, &input);
	  FD_ZERO(&output);
	  FD_SET(fd, &output);
        }
	while (select(fd + 1, &input, &output, NULL, NULL) < 0);

        if (FD_ISSET(fd, &input))
	{
	 /*
	  * Read backchannel data...
	  */

	  if ((rbytes = read(fd, backbuf, sizeof(backbuf))) > 0)
	  {
	    fprintf(stderr, "DEBUG: Received %d bytes of back-channel data!\n",
	            rbytes);
            cupsBackchannelWrite(backbuf, rbytes, 1.0);
          }
	}

        if (FD_ISSET(fd, &output))
	{
	 /*
	  * Write print data...
	  */

	  if ((wbytes = write(fd, bufptr, nbytes)) < 0)
	    if (errno == ENOTTY)
	      wbytes = write(fd, bufptr, nbytes);

	  if (wbytes < 0)
	  {
	   /*
	    * Check for retryable errors...
	    */

	    if (errno == ENOSPC)
	    {
	      paperout = 1;
	      fputs("ERROR: Out of paper!\n", stderr);
	      fputs("STATUS: +media-tray-empty-error\n", stderr);
	    }
	    else if (errno != EAGAIN && errno != EINTR)
	    {
	      perror("ERROR: Unable to send print file to printer");
	      break;
	    }
	  }
	  else
	  {
	   /*
	    * Update count and pointer...
	    */

            if (paperout)
	    {
	      fputs("STATUS: -media-tray-empty-error\n", stderr);
	      paperout = 0;
	    }

	    nbytes -= wbytes;
	    bufptr += wbytes;
	  }
	}
      }

      if (wbytes < 0)
        break;

      if (fp)
	fprintf(stderr, "INFO: Sending print file, %lu bytes...\n",
	        (unsigned long)tbytes);
    }
  }

 /*
  * Close the USB port and return...
  */

  close(fd);

  return (wbytes < 0 ? CUPS_BACKEND_FAILED : CUPS_BACKEND_OK);
}


/*
 * 'list_devices()' - List all USB devices.
 */

void
list_devices(void)
{
#ifdef __linux
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	format[255],		/* Format for device filename */
	device[255],		/* Device filename */
	device_id[1024],	/* Device ID string */
	device_uri[1024],	/* Device URI string */
	make_model[1024];	/* Make and model */


 /*
  * First figure out which USB printer filename to use...
  */

  if (!access("/dev/usblp0", 0))
    strcpy(format, "/dev/usblp%d");
  else if (!access("/dev/usb/usblp0", 0))
    strcpy(format, "/dev/usb/usblp%d");
  else
    strcpy(format, "/dev/usb/lp%d");

 /*
  * Then open each USB device...
  */

  for (i = 0; i < 16; i ++)
  {
    sprintf(device, format, i);

    if ((fd = open(device, O_RDWR | O_EXCL)) >= 0)
    {
      if (!get_device_id(fd, device_id, sizeof(device_id),
                         make_model, sizeof(make_model),
			 "usb", device_uri, sizeof(device_uri)))
	printf("direct %s \"%s\" \"%s USB #%d\" \"%s\"\n", device_uri,
	       make_model, make_model, i + 1, device_id);

      close(fd);
    }
  }
#elif defined(__sgi)
#elif defined(__sun) && defined(ECPPIOC_GETDEVID)
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255],		/* Device filename */
	device_id[1024],	/* Device ID string */
	device_uri[1024],	/* Device URI string */
	make_model[1024];	/* Make and model */
  struct ecpp_device_id did;	/* Device ID buffer */


 /*
  * Open each USB device...
  */

  for (i = 0; i < 8; i ++)
  {
    sprintf(device, "/dev/usb/printer%d", i);

    if ((fd = open(device, O_RDWR | O_EXCL)) >= 0)
    {
      if (!get_device_id(fd, device_id, sizeof(device_id),
                         make_model, sizeof(make_model),
			 "usb", device_uri, sizeof(device_uri)))
	printf("direct %s \"%s\" \"%s USB #%d\" \"%s\"\n", device_uri,
	       make_model, make_model, i + 1, device_id);

      close(fd);
    }
  }
#elif defined(__hpux)
#elif defined(__osf)
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  int   i;                      /* Looping var */
  char  device[255];            /* Device filename */


  for (i = 0; i < 8; i ++)
  {
    sprintf(device, "/dev/ulpt%d", i);
    if (!access(device, 0))
      printf("direct usb:%s \"Unknown\" \"USB Printer #%d\"\n", device, i + 1);

    sprintf(device, "/dev/unlpt%d", i);
    if (!access(device, 0))
      printf("direct usb:%s \"Unknown\" \"USB Printer #%d (no reset)\"\n", device, i + 1);
  }
#endif
}


/*
 * 'open_device()' - Open a USB device...
 */

int					/* O - File descriptor or -1 on error */
open_device(const char *uri)		/* I - Device URI */
{
 /*
  * The generic implementation just treats the URI as a device filename...
  * Specific operating systems may also support using the device serial
  * number and/or make/model.
  */

  if (!strncmp(uri, "usb:/dev/", 9))
#ifdef __linux
    return (-1); /* Do not allow direct devices anymore */
  else if (!strncmp(uri, "usb://", 6))
  {
   /*
    * For Linux, try looking up the device serial number or model...
    */

    int		i;			/* Looping var */
    int		busy;			/* Are any ports busy? */
    int		fd;			/* File descriptor */
    char	format[255],		/* Format for device filename */
		device[255],		/* Device filename */
		device_id[1024],	/* Device ID string */
		make_model[1024],	/* Make and model */
		device_uri[1024];	/* Device URI string */


   /*
    * First figure out which USB printer filename to use...
    */

    if (!access("/dev/usblp0", 0))
      strcpy(format, "/dev/usblp%d");
    else if (!access("/dev/usb/usblp0", 0))
      strcpy(format, "/dev/usb/usblp%d");
    else
      strcpy(format, "/dev/usb/lp%d");

   /*
    * Then find the correct USB device...
    */

    do
    {
      for (busy = 0, i = 0; i < 16; i ++)
      {
	sprintf(device, format, i);

	if ((fd = open(device, O_RDWR | O_EXCL)) >= 0)
	{
	  get_device_id(fd, device_id, sizeof(device_id),
                        make_model, sizeof(make_model),
			"usb", device_uri, sizeof(device_uri));
	}
	else
	{
	 /*
	  * If the open failed because it was busy, flag it so we retry
	  * as needed...
	  */

	  if (errno == EBUSY)
	    busy = 1;

	  device_uri[0] = '\0';
        }

        if (!strcmp(uri, device_uri))
	{
	 /*
	  * Yes, return this file descriptor...
	  */

	  fprintf(stderr, "DEBUG: Printer using device file \"%s\"...\n", device);

	  return (fd);
	}

       /*
	* This wasn't the one...
	*/

        if (fd >= 0)
	  close(fd);
      }

     /*
      * If we get here and at least one of the printer ports showed up
      * as "busy", then sleep for a bit and retry...
      */

      if (busy)
      {
	fputs("INFO: USB printer is busy; will retry in 5 seconds...\n",
	      stderr);
	sleep(5);
      }
    }
    while (busy);

   /*
    * Couldn't find the printer, return "no such device or address"...
    */

    errno = ENODEV;

    return (-1);
  }
#elif defined(__sun) && defined(ECPPIOC_GETDEVID)
    return (-1); /* Do not allow direct devices anymore */
  else if (!strncmp(uri, "usb://", 6))
  {
   /*
    * For Solaris, try looking up the device serial number or model...
    */

    int		i;			/* Looping var */
    int		busy;			/* Are any ports busy? */
    int		fd;			/* File descriptor */
    char	device[255],		/* Device filename */
		device_id[1024],	/* Device ID string */
		make_model[1024],	/* Make and model */
		device_uri[1024];	/* Device URI string */
    struct ecpp_device_id did;		/* Device ID buffer */


   /*
    * Find the correct USB device...
    */

    do
    {
      for (i = 0, busy = 0; i < 8; i ++)
      {
	sprintf(device, "/dev/usb/printer%d", i);

	if ((fd = open(device, O_RDWR | O_EXCL)) >= 0)
	  get_device_id(fd, device_id, sizeof(device_id),
                        make_model, sizeof(make_model),
			"usb", device_uri, sizeof(device_uri));
	else
	{
	 /*
	  * If the open failed because it was busy, flag it so we retry
	  * as needed...
	  */

	  if (errno == EBUSY)
	    busy = 1;

	  device_uri[0] = '\0';
        }

        if (!strcmp(uri, device_uri))
	  return (fd);	/* Yes, return this file descriptor... */

       /*
	* This wasn't the one...
	*/

        if (fd >= 0)
	  close(fd);
      }

     /*
      * If we get here and at least one of the printer ports showed up
      * as "busy", then sleep for a bit and retry...
      */

      if (busy)
      {
	fputs("INFO: USB printer is busy; will retry in 5 seconds...\n",
	      stderr);
	sleep(5);
      }
    }
    while (busy);

   /*
    * Couldn't find the printer, return "no such device or address"...
    */

    errno = ENODEV;

    return (-1);
  }
#else
    return (open(uri + 4, O_RDWR | O_EXCL));
#endif /* __linux */
  else
  {
    errno = ENODEV;
    return (-1);
  }
}


/*
 * End of "$Id$".
 */
