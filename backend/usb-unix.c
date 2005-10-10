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

#ifdef __linux
#  include <sys/ioctl.h>
#  include <linux/lp.h>
#  define IOCNR_GET_DEVICE_ID		1

/*
 * Get device_id string
 */
#  define LPIOC_GET_DEVICE_ID(len)	_IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, len)
#endif /* __linux */

#ifdef __sun
#  ifdef __sparc
#    include <sys/ecppio.h>
#  else
#    include <sys/ioccom.h>
#    include <sys/ecppsys.h>
#  endif /* __sparc */
#endif /* __sun */

#include <sys/select.h>


/*
 * Local functions...
 */

void	decode_device_id(int port, const char *device_id,
	                 char *make_model, int mmsize,
			 char *uri, int urisize);
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

#if defined(__linux) && defined(LP_POUTPA)
 /*
  * Show the printer status before we send the file; normally, we'd
  * do this while we write data to the printer, however at least some
  * Linux kernels have buggy USB drivers which don't like to be
  * queried while sending data to the printer...
  *
  * Also, we're using the 8255 constants instead of the ones that are
  * supposed to be used, as it appears that the USB driver also doesn't
  * follow standards...
  */

  if (ioctl(fd, LPGETSTATUS, &status) == 0)
  {
    fprintf(stderr, "DEBUG: LPGETSTATUS returned a port status of %02X...\n", status);

    if (status & LP_POUTPA)
      fputs("WARNING: Media tray empty!\n", stderr);
    else if (!(status & LP_PERRORP))
      fputs("WARNING: Printer fault!\n", stderr);
    else if (!(status & LP_PSELECD))
      fputs("WARNING: Printer off-line.\n", stderr);
  }
#endif /* __linux && LP_POUTPA */

 /*
  * Finally, send the print file...
  */

  wbytes   = 0;
  paperout = 0;

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
 * 'decode_device_id()' - Decode the IEEE-1284 device ID string.
 */

void
decode_device_id(int        port,		/* I - Port number */
                 const char *device_id,		/* I - 1284 device ID string */
                 char       *make_model,	/* O - Make/model */
		 int        mmsize,		/* I - Size of buffer */
		 char       *uri,		/* O - Device URI */
		 int        urisize)		/* I - Size of buffer */
{
  char	*attr,					/* 1284 attribute */
  	*delim,					/* 1284 delimiter */
	*uriptr,				/* Pointer into URI */
	*mfg,					/* Manufacturer string */
	*mdl,					/* Model string */
	serial_number[1024];			/* Serial number string */


 /*
  * Look for the description field...
  */

  if ((attr = strstr(device_id, "DES:")) != NULL)
    attr += 4;
  else if ((attr = strstr(device_id, "DESCRIPTION:")) != NULL)
    attr += 12;

  if ((mfg = strstr(device_id, "MANUFACTURER:")) != NULL)
    mfg += 13;
  else if ((mfg = strstr(device_id, "MFG:")) != NULL)
    mfg += 4;

  if ((mdl = strstr(device_id, "MODEL:")) != NULL)
    mdl += 6;
  else if ((mdl = strstr(device_id, "MDL:")) != NULL)
    mdl += 4;

  if (attr)
  {
   /*
    * Use description...
    */

    if (!strncasecmp(attr, "Hewlett-Packard hp ", 19))
    {
     /*
      * Check for a common HP bug...
      */

      strlcpy(make_model, "HP ", mmsize);
      strlcpy(make_model + 3, attr + 19, mmsize - 3);
    }
    else if (!strncasecmp(attr, "Hewlett-Packard ", 16))
    {
      strlcpy(make_model, "HP ", mmsize);
      strlcpy(make_model + 3, attr + 16, mmsize - 3);
    }
    else
    {
      strlcpy(make_model, attr, mmsize);
    }
  }
  else if (mfg && mdl)
  {
   /*
    * Build a make-model string from the manufacturer and model attributes...
    */

    if (!strncasecmp(mfg, "Hewlett-Packard", 15))
      strlcpy(make_model, "HP", mmsize);
    else
      strlcpy(make_model, mfg, mmsize);

    if ((delim = strchr(make_model, ';')) != NULL)
      *delim = '\0';

    if (!strncasecmp(make_model, mdl, strlen(make_model)))
    {
     /*
      * Just copy model string, since it has the manufacturer...
      */

      strlcpy(make_model, mdl, mmsize);
    }
    else
    {
     /*
      * Concatenate the make and model...
      */

      strlcat(make_model, " ", mmsize);
      strlcat(make_model, mdl, mmsize);
    }
  }
  else
  {
   /*
    * Use "Unknown" as the printer make and model...
    */

    strlcpy(make_model, "Unknown", mmsize);
  }

  if ((delim = strchr(make_model, ';')) != NULL)
    *delim = '\0';

 /*
  * Look for the serial number field...
  */

  if ((attr = strstr(device_id, "SERN:")) != NULL)
    attr += 5;
  else if ((attr = strstr(device_id, "SERIALNUMBER:")) != NULL)
    attr += 13;
  else if ((attr = strstr(device_id, ";SN:")) != NULL)
    attr += 4;

  if (attr)
  {
    strlcpy(serial_number, attr, sizeof(serial_number));

    if ((delim = strchr(serial_number, ';')) != NULL)
      *delim = '\0';
  }
  else
    serial_number[0] = '\0';

 /*
  * Generate the device URI from the make_model and serial number strings.
  */

  strlcpy(uri, "usb://", urisize);
  for (uriptr = uri + 6, delim = make_model;
       *delim && uriptr < (uri + urisize - 1);
       delim ++)
    if (*delim == ' ')
    {
      delim ++;
      *uriptr++ = '/';
      break;
    }
    else
      *uriptr++ = *delim;

  for (; *delim && uriptr < (uri + urisize - 3); delim ++)
    if (*delim == ' ')
    {
      *uriptr++ = '%';
      *uriptr++ = '2';
      *uriptr++ = '0';
    }
    else
      *uriptr++ = *delim;

  *uriptr = '\0';

  if (serial_number[0])
  {
   /*
    * Add the serial number to the URI...
    */

    strlcat(uri, "?serial=", urisize);
    strlcat(uri, serial_number, urisize);
  }
}


/*
 * 'list_devices()' - List all USB devices.
 */

void
list_devices(void)
{
#ifdef __linux
  int	i;			/* Looping var */
  int	length;			/* Length of device ID info */
  int	fd;			/* File descriptor */
  char	format[255],		/* Format for device filename */
	device[255],		/* Device filename */
	device_id[1024],	/* Device ID string */
	device_uri[1024],	/* Device URI string */
	make_model[1024];	/* Make and model */


 /*
  * First figure out which USB printer filename to use...
  */

  if (access("/dev/usblp0", 0) == 0)
    strcpy(format, "/dev/usblp%d");
  else if (access("/dev/usb/usblp0", 0) == 0)
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
      if (ioctl(fd, LPIOC_GET_DEVICE_ID(sizeof(device_id)), device_id) == 0)
      {
	length = (((unsigned)device_id[0] & 255) << 8) +
	         ((unsigned)device_id[1] & 255);

       /*
        * Check to see if the length is larger than our buffer; first
	* assume that the vendor incorrectly implemented the 1284 spec,
	* and then limit the length to the size of our buffer...
	*/

        if (length > (sizeof(device_id) - 2))
	  length = (((unsigned)device_id[1] & 255) << 8) +
	           ((unsigned)device_id[0] & 255);

        if (length > (sizeof(device_id) - 2))
	  length = sizeof(device_id) - 2;

	memmove(device_id, device_id + 2, length);
	device_id[length] = '\0';
      }
      else
        device_id[0] = '\0';

      close(fd);
    }
    else
      device_id[0] = '\0';

    if (device_id[0])
    {
      decode_device_id(i, device_id, make_model, sizeof(make_model),
		       device_uri, sizeof(device_uri));

      printf("direct %s \"%s\" \"USB Printer #%d\"\n", device_uri,
	     make_model, i + 1);
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
      did.mode = ECPP_CENTRONICS;
      did.len  = sizeof(device_id);
      did.rlen = 0;
      did.addr = device_id;

      if (ioctl(fd, ECPPIOC_GETDEVID, &did) == 0)
      {
        if (did.rlen < (sizeof(device_id) - 1))
	  device_id[did.rlen] = '\0';
        else
	  device_id[sizeof(device_id) - 1] = '\0';
      }
      else
        device_id[0] = '\0';

      close(fd);
    }
    else
      device_id[0] = '\0';

    if (device_id[0])
    {
      decode_device_id(i, device_id, make_model, sizeof(make_model),
		       device_uri, sizeof(device_uri));

      printf("direct %s \"%s\" \"USB Printer #%d\"\n", device_uri,
	     make_model, i + 1);
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

  if (strncmp(uri, "usb:/dev/", 9) == 0)
#ifdef __linux
    return (-1); /* Do not allow direct devices anymore */
  else if (strncmp(uri, "usb://", 6) == 0)
  {
   /*
    * For Linux, try looking up the device serial number or model...
    */

    int		i;			/* Looping var */
    int		busy;			/* Are any ports busy? */
    int		length;			/* Length of device ID info */
    int		fd;			/* File descriptor */
    char	format[255],		/* Format for device filename */
		device[255],		/* Device filename */
		device_id[1024],	/* Device ID string */
		make_model[1024],	/* Make and model */
		device_uri[1024];	/* Device URI string */


   /*
    * First figure out which USB printer filename to use...
    */

    if (access("/dev/usblp0", 0) == 0)
      strcpy(format, "/dev/usblp%d");
    else if (access("/dev/usb/usblp0", 0) == 0)
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
	  if (ioctl(fd, LPIOC_GET_DEVICE_ID(sizeof(device_id)), device_id) == 0)
	  {
	    length = (((unsigned)device_id[0] & 255) << 8) +
	             ((unsigned)device_id[1] & 255);
	    memmove(device_id, device_id + 2, length);
	    device_id[length] = '\0';
	  }
	  else
            device_id[0] = '\0';
	}
	else
	{
	 /*
	  * If the open failed because it was busy, flag it so we retry
	  * as needed...
	  */

	  if (errno == EBUSY)
	    busy = 1;

	  device_id[0] = '\0';
        }

	if (device_id[0])
	{
	 /*
          * Got the device ID - is this the one?
	  */

	  decode_device_id(i, device_id, make_model, sizeof(make_model),
                	   device_uri, sizeof(device_uri));

          if (strcmp(uri, device_uri) == 0)
	  {
	   /*
	    * Yes, return this file descriptor...
	    */

	    fprintf(stderr, "DEBUG: Printer using device file \"%s\"...\n", device);

	    return (fd);
	  }
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
  else if (strncmp(uri, "usb://", 6) == 0)
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
	{
	  did.mode = ECPP_CENTRONICS;
	  did.len  = sizeof(device_id);
	  did.rlen = 0;
	  did.addr = device_id;

	  if (ioctl(fd, ECPPIOC_GETDEVID, &did) == 0)
	  {
            if (did.rlen < (sizeof(device_id) - 1))
	      device_id[did.rlen] = '\0';
            else
	      device_id[sizeof(device_id) - 1] = '\0';
	  }
	  else
            device_id[0] = '\0';
	}
	else
	{
	 /*
	  * If the open failed because it was busy, flag it so we retry
	  * as needed...
	  */

	  if (errno == EBUSY)
	    busy = 1;

	  device_id[0] = '\0';
        }

	if (device_id[0])
	{
	 /*
          * Got the device ID - is this the one?
	  */

	  decode_device_id(i, device_id, make_model, sizeof(make_model),
                	   device_uri, sizeof(device_uri));

          if (strcmp(uri, device_uri) == 0)
	    return (fd);	/* Yes, return this file descriptor... */
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
