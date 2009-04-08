/*
 * "$Id: serial.c 7647 2008-06-16 17:39:40Z mike $"
 *
 *   Serial port backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2009 by Apple Inc.
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
 *   main()         - Send a file to the printer or server.
 *   list_devices() - List all serial devices.
 *   side_cb()      - Handle side-channel requests...
 */

/*
 * Include necessary headers.
 */

#include "backend-private.h"

#ifdef __hpux
#  include <sys/modem.h>
#endif /* __hpux */

#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
#  ifdef __hpux
#    include <sys/time.h>
#  else
#    include <sys/select.h>
#  endif /* __hpux */
#  ifdef HAVE_SYS_IOCTL_H
#    include <sys/ioctl.h>
#  endif /* HAVE_SYS_IOCTL_H */
#endif /* WIN32 */

#ifdef __sgi
#  include <invent.h>
#  ifndef INV_EPP_ECP_PLP
#    define INV_EPP_ECP_PLP	6	/* From 6.3/6.4/6.5 sys/invent.h */
#    define INV_ASO_SERIAL	14	/* serial portion of SGI ASO board */
#    define INV_IOC3_DMA	16	/* DMA mode IOC3 serial */
#    define INV_IOC3_PIO	17	/* PIO mode IOC3 serial */
#    define INV_ISA_DMA		19	/* DMA mode ISA serial -- O2 */
#  endif /* !INV_EPP_ECP_PLP */
#endif /* __sgi */

#ifndef CRTSCTS
#  ifdef CNEW_RTSCTS
#    define CRTSCTS CNEW_RTSCTS
#  else
#    define CRTSCTS 0
#  endif /* CNEW_RTSCTS */
#endif /* !CRTSCTS */

#if defined(__APPLE__)
#  include <CoreFoundation/CoreFoundation.h>
#  include <IOKit/IOKitLib.h>
#  include <IOKit/serial/IOSerialKeys.h>
#  include <IOKit/IOBSD.h>
#endif /* __APPLE__ */

#if defined(__linux) && defined(TIOCGSERIAL)
#  include <linux/serial.h>
#  include <linux/ioctl.h>
#endif /* __linux && TIOCGSERIAL */


/*
 * Local functions...
 */

static void	list_devices(void);
static void	side_cb(int print_fd, int device_fd, int use_bc);


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
		resource[1024],		/* Resource info (device and options) */
		*options,		/* Pointer to options */
		*name,			/* Name of option */
		*value,			/* Value of option */
		sep;			/* Option separator */
  int		port;			/* Port number (not used) */
  int		copies;			/* Number of copies to print */
  int		print_fd,		/* Print file */
		device_fd;		/* Serial device */
  int		nfds;			/* Maximum file descriptor value + 1 */
  fd_set	input,			/* Input set for reading */
		output;			/* Output set for writing */
  ssize_t	print_bytes,		/* Print bytes read */
		bc_bytes,		/* Backchannel bytes read */
		total_bytes,		/* Total bytes written */
		bytes;			/* Bytes written */
  int		dtrdsr;			/* Do dtr/dsr flow control? */
  int		print_size;		/* Size of output buffer for writes */
  char		print_buffer[8192],	/* Print data buffer */
		*print_ptr,		/* Pointer into print data buffer */
		bc_buffer[1024];	/* Back-channel data buffer */
  struct termios opts;			/* Serial port options */
  struct termios origopts;		/* Original port options */
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
    list_devices();
    return (CUPS_BACKEND_OK);
  }
  else if (argc < 6 || argc > 7)
  {
    _cupsLangPrintf(stderr,
                    _("Usage: %s job-id user title copies options [file]\n"),
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
      _cupsLangPrintf(stderr,
                      _("ERROR: Unable to open print file \"%s\": %s\n"),
                      argv[6], strerror(errno));
      return (CUPS_BACKEND_FAILED);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the device name and options from the URI...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, cupsBackendDeviceURI(argv),
                  method, sizeof(method), username, sizeof(username),
		  hostname, sizeof(hostname), &port,
		  resource, sizeof(resource));

 /*
  * See if there are any options...
  */

  if ((options = strchr(resource, '?')) != NULL)
  {
   /*
    * Yup, terminate the device name string and move to the first
    * character of the options...
    */

    *options++ = '\0';
  }

 /*
  * Open the serial port device...
  */

  fputs("STATE: +connecting-to-device\n", stderr);

  do
  {
    if ((device_fd = open(resource, O_RDWR | O_NOCTTY | O_EXCL |
                                    O_NDELAY)) == -1)
    {
      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        _cupsLangPuts(stderr,
	              _("INFO: Unable to contact printer, queuing on next "
			"printer in class...\n"));

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (CUPS_BACKEND_FAILED);
      }

      if (errno == EBUSY)
      {
        _cupsLangPuts(stderr,
	              _("INFO: Printer busy; will retry in 30 seconds...\n"));
	sleep(30);
      }
      else
      {
	_cupsLangPrintf(stderr,
	                _("ERROR: Unable to open device file \"%s\": %s\n"),
			resource, strerror(errno));
	return (CUPS_BACKEND_FAILED);
      }
    }
  }
  while (device_fd < 0);

  fputs("STATE: -connecting-to-device\n", stderr);

 /*
  * Set any options provided...
  */

  tcgetattr(device_fd, &origopts);
  tcgetattr(device_fd, &opts);

  opts.c_lflag &= ~(ICANON | ECHO | ISIG);
					/* Raw mode */
  opts.c_oflag &= ~OPOST;		/* Don't post-process */

  print_size = 96;			/* 9600 baud / 10 bits/char / 10Hz */
  dtrdsr     = 0;			/* No dtr/dsr flow control */

  if (options)
  {
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

      if (!strcasecmp(name, "baud"))
      {
       /*
        * Set the baud rate...
	*/

        print_size = atoi(value) / 100;

#if B19200 == 19200
        cfsetispeed(&opts, atoi(value));
	cfsetospeed(&opts, atoi(value));
#else
        switch (atoi(value))
	{
	  case 1200 :
	      cfsetispeed(&opts, B1200);
	      cfsetospeed(&opts, B1200);
	      break;
	  case 2400 :
	      cfsetispeed(&opts, B2400);
	      cfsetospeed(&opts, B2400);
	      break;
	  case 4800 :
	      cfsetispeed(&opts, B4800);
	      cfsetospeed(&opts, B4800);
	      break;
	  case 9600 :
	      cfsetispeed(&opts, B9600);
	      cfsetospeed(&opts, B9600);
	      break;
	  case 19200 :
	      cfsetispeed(&opts, B19200);
	      cfsetospeed(&opts, B19200);
	      break;
	  case 38400 :
	      cfsetispeed(&opts, B38400);
	      cfsetospeed(&opts, B38400);
	      break;
#  ifdef B57600
	  case 57600 :
	      cfsetispeed(&opts, B57600);
	      cfsetospeed(&opts, B57600);
	      break;
#  endif /* B57600 */
#  ifdef B115200
	  case 115200 :
	      cfsetispeed(&opts, B115200);
	      cfsetospeed(&opts, B115200);
	      break;
#  endif /* B115200 */
#  ifdef B230400
	  case 230400 :
	      cfsetispeed(&opts, B230400);
	      cfsetospeed(&opts, B230400);
	      break;
#  endif /* B230400 */
          default :
	      _cupsLangPrintf(stderr, _("WARNING: Unsupported baud rate %s!\n"),
			      value);
	      break;
	}
#endif /* B19200 == 19200 */
      }
      else if (!strcasecmp(name, "bits"))
      {
       /*
        * Set number of data bits...
	*/

        switch (atoi(value))
	{
	  case 7 :
	      opts.c_cflag &= ~CSIZE;
              opts.c_cflag |= CS7;
	      opts.c_cflag |= PARENB;
              opts.c_cflag &= ~PARODD;
              break;
	  case 8 :
	      opts.c_cflag &= ~CSIZE;
              opts.c_cflag |= CS8;
	      opts.c_cflag &= ~PARENB;
	      break;
	}
      }
      else if (!strcasecmp(name, "parity"))
      {
       /*
	* Set parity checking...
	*/

	if (!strcasecmp(value, "even"))
	{
	  opts.c_cflag |= PARENB;
          opts.c_cflag &= ~PARODD;
	}
	else if (!strcasecmp(value, "odd"))
	{
	  opts.c_cflag |= PARENB;
          opts.c_cflag |= PARODD;
	}
	else if (!strcasecmp(value, "none"))
	  opts.c_cflag &= ~PARENB;
	else if (!strcasecmp(value, "space"))
	{
	 /*
	  * Note: we only support space parity with 7 bits per character...
	  */

	  opts.c_cflag &= ~CSIZE;
          opts.c_cflag |= CS8;
	  opts.c_cflag &= ~PARENB;
        }
	else if (!strcasecmp(value, "mark"))
	{
	 /*
	  * Note: we only support mark parity with 7 bits per character
	  * and 1 stop bit...
	  */

	  opts.c_cflag &= ~CSIZE;
          opts.c_cflag |= CS7;
	  opts.c_cflag &= ~PARENB;
          opts.c_cflag |= CSTOPB;
        }
      }
      else if (!strcasecmp(name, "flow"))
      {
       /*
	* Set flow control...
	*/

	if (!strcasecmp(value, "none"))
	{
	  opts.c_iflag &= ~(IXON | IXOFF);
          opts.c_cflag &= ~CRTSCTS;
	}
	else if (!strcasecmp(value, "soft"))
	{
	  opts.c_iflag |= IXON | IXOFF;
          opts.c_cflag &= ~CRTSCTS;
	}
	else if (!strcasecmp(value, "hard") ||
	         !strcasecmp(value, "rtscts"))
        {
	  opts.c_iflag &= ~(IXON | IXOFF);
          opts.c_cflag |= CRTSCTS;
	}
	else if (!strcasecmp(value, "dtrdsr"))
	{
	  opts.c_iflag &= ~(IXON | IXOFF);
          opts.c_cflag &= ~CRTSCTS;

	  dtrdsr = 1;
	}
      }
      else if (!strcasecmp(name, "stop"))
      {
        switch (atoi(value))
	{
	  case 1 :
	      opts.c_cflag &= ~CSTOPB;
	      break;

	  case 2 :
	      opts.c_cflag |= CSTOPB;
	      break;
	}
      }
    }
  }

  tcsetattr(device_fd, TCSANOW, &opts);
  fcntl(device_fd, F_SETFL, 0);

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  Only ignore SIGTERM if we are printing data from
  * stdin (otherwise you can't cancel raw jobs...)
  */

  if (print_fd != 0)
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
  * Finally, send the print file.  Ordinarily we would just use the
  * backendRunLoop() function, however since we need to use smaller
  * writes and may need to do DSR/DTR flow control, we duplicate much
  * of the code here instead...
  */

  if (print_size > sizeof(print_buffer))
    print_size = sizeof(print_buffer);

  total_bytes = 0;

  while (copies > 0)
  {
    copies --;

    if (print_fd != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(print_fd, 0, SEEK_SET);
    }

   /*
    * Now loop until we are out of data from print_fd...
    */

    for (print_bytes = 0, print_ptr = print_buffer;;)
    {
     /*
      * Use select() to determine whether we have data to copy around...
      */

      FD_ZERO(&input);
      if (!print_bytes)
	FD_SET(print_fd, &input);
      FD_SET(device_fd, &input);
      if (!print_bytes)
        FD_SET(CUPS_SC_FD, &input);

      FD_ZERO(&output);
      if (print_bytes)
	FD_SET(device_fd, &output);

      if (select(nfds, &input, &output, NULL, NULL) < 0)
	continue;			/* Ignore errors here */

     /*
      * Check if we have a side-channel request ready...
      */

      if (FD_ISSET(CUPS_SC_FD, &input))
      {
       /*
	* Do the side-channel request, then start back over in the select
	* loop since it may have read from print_fd...
	*/

        side_cb(print_fd, device_fd, 1);
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
	if ((print_bytes = read(print_fd, print_buffer, print_size)) < 0)
	{
	 /*
          * Read error - bail if we don't see EAGAIN or EINTR...
	  */

	  if (errno != EAGAIN || errno != EINTR)
	  {
	    _cupsLangPrintError(_("ERROR: Unable to read print data"));

            tcsetattr(device_fd, TCSADRAIN, &origopts);

	    close(device_fd);

	    if (print_fd != 0)
	      close(print_fd);

	    return (CUPS_BACKEND_FAILED);
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
      }

     /*
      * Check if the device is ready to receive data and we have data to
      * send...
      */

      if (print_bytes && FD_ISSET(device_fd, &output))
      {
	if (dtrdsr)
	{
	 /*
	  * Check the port and sleep until DSR is set...
	  */

	  int status;


	  if (!ioctl(device_fd, TIOCMGET, &status))
            if (!(status & TIOCM_DSR))
	    {
	     /*
	      * Wait for DSR to go high...
	      */

	      fputs("DEBUG: DSR is low; waiting for device...\n", stderr);

              do
	      {
	       /*
	        * Poll every 100ms...
		*/

		usleep(100000);

		if (ioctl(device_fd, TIOCMGET, &status))
		  break;
	      }
	      while (!(status & TIOCM_DSR));

	      fputs("DEBUG: DSR is high; writing to device...\n", stderr);
            }
	}

	if ((bytes = write(device_fd, print_ptr, print_bytes)) < 0)
	{
	 /*
          * Write error - bail if we don't see an error we can retry...
	  */

	  if (errno != EAGAIN && errno != EINTR && errno != ENOTTY)
	  {
	    _cupsLangPrintError(_("ERROR: Unable to write print data"));

            tcsetattr(device_fd, TCSADRAIN, &origopts);

	    close(device_fd);

	    if (print_fd != 0)
	      close(print_fd);

	    return (CUPS_BACKEND_FAILED);
	  }
	}
	else
	{
          fprintf(stderr, "DEBUG: Wrote %d bytes...\n", (int)bytes);

          print_bytes -= bytes;
	  print_ptr   += bytes;
	  total_bytes += bytes;
	}
      }
    }
  }

 /*
  * Close the serial port and input file and return...
  */

  tcsetattr(device_fd, TCSADRAIN, &origopts);

  close(device_fd);

  if (print_fd != 0)
    close(print_fd);

  return (total_bytes < 0 ? CUPS_BACKEND_FAILED : CUPS_BACKEND_OK);
}


/*
 * 'list_devices()' - List all serial devices.
 */

static void
list_devices(void)
{
#if defined(__hpux) || defined(__sgi) || defined(__sun) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
  static char	*funky_hex = "0123456789abcdefghijklmnopqrstuvwxyz";
					/* Funky hex numbering used for some *
					 * devices                           */
#endif /* __hpux || __sgi || __sun || __FreeBSD__ || __OpenBSD__ || __FreeBSD_kernel__ */


#ifdef __linux
  int			i, j;		/* Looping vars */
  int			fd;		/* File descriptor */
  char			device[255];	/* Device filename */
  char			info[255];	/* Device info/description */
#  ifdef TIOCGSERIAL
  struct serial_struct	serinfo;	/* serial port info */
#  endif /* TIOCGSERIAL */


  for (i = 0; i < 100; i ++)
  {
    sprintf(device, "/dev/ttyS%d", i);

    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
#  ifdef TIOCGSERIAL
     /*
      * See if this port exists...
      */

      serinfo.reserved_char[0] = 0;

      if (!ioctl(fd, TIOCGSERIAL, &serinfo))
      {
	if (serinfo.type == PORT_UNKNOWN)
	{
	 /*
	  * Nope...
	  */

	  close(fd);
	  continue;
	}
      }
#  endif /* TIOCGSERIAL */

      close(fd);

      snprintf(info, sizeof(info),
	       _cupsLangString(cupsLangDefault(), _("Serial Port #%d")), i + 1);

#  if defined(_ARCH_PPC) || defined(powerpc) || defined(__powerpc)
      printf("serial serial:%s?baud=230400 \"Unknown\" \"%s\"\n", device, info);
#  else
      printf("serial serial:%s?baud=115200 \"Unknown\" \"%s\"\n", device, info);
#  endif /* _ARCH_PPC || powerpc || __powerpc */
    }
  }

  for (i = 0; i < 16; i ++)
  {
    snprintf(info, sizeof(info),
	     _cupsLangString(cupsLangDefault(), _("USB Serial Port #%d")),
	     i + 1);

    sprintf(device, "/dev/usb/ttyUSB%d", i);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=230400 \"Unknown\" \"%s\"\n", device, info);
    }

    sprintf(device, "/dev/ttyUSB%d", i);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=230400 \"Unknown\" \"%s\"\n", device, info);
    }
  }

  for (i = 0; i < 64; i ++)
  {
    for (j = 0; j < 8; j ++)
    {
      sprintf(device, "/dev/ttyQ%02de%d", i, j);
      if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
      {
        close(fd);

        printf("serial serial:%s?baud=115200 \"Unknown\" "
	       "\"Equinox ESP %d Port #%d\"\n", device, i, j + 1);
      }
    }
  }
#elif defined(__sgi)
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */
  inventory_t	*inv;		/* Hardware inventory info */


 /*
  * IRIX maintains a hardware inventory of most devices...
  */

  setinvent();

  while ((inv = getinvent()) != NULL)
  {
    if (inv->inv_class == INV_SERIAL)
    {
     /*
      * Some sort of serial port...
      */

      if (inv->inv_type == INV_CDSIO || inv->inv_type == INV_CDSIO_E)
      {
       /*
        * CDSIO port...
        */

	for (n = 0; n < 6; n ++)
	  printf("serial serial:/dev/ttyd%d?baud=38400 \"Unknown\" \"CDSIO Board %d Serial Port #%d\"\n",
        	 n + 5 + 8 * inv->inv_controller, inv->inv_controller, n + 1);
      }
      else if (inv->inv_type == INV_EPC_SERIAL)
      {
       /*
        * Everest serial port...
        */

	if (inv->inv_unit == 0)
          i = 1;
	else
          i = 41 + 4 * (int)inv->inv_controller;

	for (n = 0; n < (int)inv->inv_state; n ++)
	  printf("serial serial:/dev/ttyd%d?baud=38400 \"Unknown\" \"EPC Serial Port %d, Ebus slot %d\"\n",
        	 n + i, n + 1, (int)inv->inv_controller);
      }
      else if (inv->inv_state > 1)
      {
       /*
        * Standard serial port under IRIX 6.4 and earlier...
        */

	for (n = 0; n < (int)inv->inv_state; n ++)
	  printf("serial serial:/dev/ttyd%d?baud=38400 \"Unknown\" \"Onboard Serial Port %d\"\n",
        	 n + (int)inv->inv_unit + 1, n + (int)inv->inv_unit + 1);
      }
      else
      {
       /*
        * Standard serial port under IRIX 6.5 and beyond...
        */

	printf("serial serial:/dev/ttyd%d?baud=115200 \"Unknown\" \"Onboard Serial Port %d\"\n",
               (int)inv->inv_controller, (int)inv->inv_controller);
      }
    }
  }

  endinvent();

 /*
  * Central Data makes serial and parallel "servers" that can be
  * connected in a number of ways.  Look for ports...
  */

  for (i = 0; i < 10; i ++)
    for (j = 0; j < 8; j ++)
      for (n = 0; n < 32; n ++)
      {
        if (i == 8)		/* EtherLite */
          sprintf(device, "/dev/ttydn%d%c", j, funky_hex[n]);
        else if (i == 9)	/* PCI */
          sprintf(device, "/dev/ttydp%d%c", j, funky_hex[n]);
        else			/* SCSI */
          sprintf(device, "/dev/ttyd%d%d%c", i, j, funky_hex[n]);

	if (access(device, 0) == 0)
	{
	  if (i == 8)
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data EtherLite Serial Port, ID %d, port %d\"\n",
  	           device, j, n);
	  else if (i == 9)
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data PCI Serial Port, ID %d, port %d\"\n",
  	           device, j, n);
  	  else
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data SCSI Serial Port, logical bus %d, ID %d, port %d\"\n",
  	           device, i, j, n);
	}
      }
#elif defined(__sun)
  int		i, j, n;		/* Looping vars */
  char		device[255];		/* Device filename */
  char		info[255];		/* Device info/description */


 /*
  * Standard serial ports...
  */

  for (i = 0; i < 26; i ++)
  {
    sprintf(device, "/dev/cua/%c", 'a' + i);
    if (!access(device, 0))
    {
      snprintf(info, sizeof(info),
	       _cupsLangString(cupsLangDefault(), _("Serial Port #%d")), i + 1);

#  ifdef B115200
      printf("serial serial:%s?baud=115200 \"Unknown\" \"%s\"\n", device, info);
#  else
      printf("serial serial:%s?baud=38400 \"Unknown\" \"%s\"\n", device, info);
#  endif /* B115200 */
    }
  }

 /*
  * MAGMA serial ports...
  */

  for (i = 0; i < 40; i ++)
  {
    sprintf(device, "/dev/term/%02d", i);
    if (access(device, 0) == 0)
      printf("serial serial:%s?baud=38400 \"Unknown\" \"MAGMA Serial Board #%d Port #%d\"\n",
             device, (i / 10) + 1, (i % 10) + 1);
  }

 /*
  * Central Data serial ports...
  */

  for (i = 0; i < 9; i ++)
    for (j = 0; j < 8; j ++)
      for (n = 0; n < 32; n ++)
      {
        if (i == 8)	/* EtherLite */
          sprintf(device, "/dev/sts/ttyN%d%c", j, funky_hex[n]);
        else
          sprintf(device, "/dev/sts/tty%c%d%c", i + 'C', j,
                  funky_hex[n]);

	if (access(device, 0) == 0)
	{
	  if (i == 8)
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data EtherLite Serial Port, ID %d, port %d\"\n",
  	           device, j, n);
  	  else
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data SCSI Serial Port, logical bus %d, ID %d, port %d\"\n",
  	           device, i, j, n);
	}
      }
#elif defined(__hpux)
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */


 /*
  * Standard serial ports...
  */

  for (i = 0; i < 10; i ++)
  {
    sprintf(device, "/dev/tty%dp0", i);
    if (access(device, 0) == 0)
      printf("serial serial:%s?baud=38400 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
  }

 /*
  * Central Data serial ports...
  */

  for (i = 0; i < 9; i ++)
    for (j = 0; j < 8; j ++)
      for (n = 0; n < 32; n ++)
      {
        if (i == 8)	/* EtherLite */
          sprintf(device, "/dev/ttyN%d%c", j, funky_hex[n]);
        else
          sprintf(device, "/dev/tty%c%d%c", i + 'C', j,
                  funky_hex[n]);

	if (access(device, 0) == 0)
	{
	  if (i == 8)
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data EtherLite Serial Port, ID %d, port %d\"\n",
  	           device, j, n);
  	  else
	    printf("serial serial:%s?baud=38400 \"Unknown\" \"Central Data SCSI Serial Port, logical bus %d, ID %d, port %d\"\n",
  	           device, i, j, n);
	}
      }
#elif defined(__osf__)
  int		i;		/* Looping var */
  char		device[255];	/* Device filename */


 /*
  * Standard serial ports...
  */

  for (i = 0; i < 100; i ++)
  {
    sprintf(device, "/dev/tty%02d", i);
    if (access(device, 0) == 0)
      printf("serial serial:%s?baud=38400 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
  }
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
  int	i, j;				/* Looping vars */
  int	fd;				/* File descriptor */
  char	device[255];			/* Device filename */
  char	info[255];			/* Device info/description */


 /*
  * SIO ports...
  */

  for (i = 0; i < 32; i ++)
  {
    sprintf(device, "/dev/ttyd%c", funky_hex[i]);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);

      snprintf(info, sizeof(info),
	       _cupsLangString(cupsLangDefault(), _("Serial Port #%d")), i + 1);

      printf("serial serial:%s?baud=115200 \"Unknown\" \"%s\"\n", device, info);
    }
  }

 /*
  * Cyclades ports...
  */

  for (i = 0; i < 16; i ++) /* Should be up to 65536 boards... */
    for (j = 0; j < 32; j ++)
    {
      sprintf(device, "/dev/ttyc%d%c", i, funky_hex[j]);
      if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
      {
	close(fd);
	printf("serial serial:%s?baud=115200 \"Unknown\" \"Cyclades #%d Serial Port #%d\"\n",
               device, i, j + 1);
      }

      sprintf(device, "/dev/ttyC%d%c", i, funky_hex[j]);
      if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
      {
	close(fd);
	printf("serial serial:%s?baud=115200 \"Unknown\" \"Cyclades #%d Serial Port #%d\"\n",
               device, i, j + 1);
      }
    }

 /*
  * Digiboard ports...
  */

  for (i = 0; i < 16; i ++) /* Should be up to 65536 boards... */
    for (j = 0; j < 32; j ++)
    {
      sprintf(device, "/dev/ttyD%d%c", i, funky_hex[j]);
      if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
      {
	close(fd);
	printf("serial serial:%s?baud=115200 \"Unknown\" \"Digiboard #%d Serial Port #%d\"\n",
               device, i, j + 1);
      }
    }

 /*
  * Stallion ports...
  */

  for (i = 0; i < 32; i ++)
  {
    sprintf(device, "/dev/ttyE%c", funky_hex[i]);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=115200 \"Unknown\" \"Stallion Serial Port #%d\"\n",
             device, i + 1);
    }
  }

 /*
  * SX ports...
  */

  for (i = 0; i < 128; i ++)
  {
    sprintf(device, "/dev/ttyA%d", i + 1);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=115200 \"Unknown\" \"SX Serial Port #%d\"\n",
             device, i + 1);
    }
  }
#elif defined(__NetBSD__)
  int	i, j;				/* Looping vars */
  int	fd;				/* File descriptor */
  char	device[255];			/* Device filename */
  char	info[255];			/* Device info/description */


 /*
  * Standard serial ports...
  */

  for (i = 0; i < 4; i ++)
  {
    sprintf(device, "/dev/tty%02d", i);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);

      snprintf(info, sizeof(info),
	       _cupsLangString(cupsLangDefault(), _("Serial Port #%d")), i + 1);

      printf("serial serial:%s?baud=115200 \"Unknown\" \"%s\"\n", device, info);
    }
  }

 /*
  * Cyclades-Z ports...
  */

  for (i = 0; i < 16; i ++) /* Should be up to 65536 boards... */
    for (j = 0; j < 64; j ++)
    {
      sprintf(device, "/dev/ttyCZ%02d%02d", i, j);
      if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
      {
	close(fd);
	printf("serial serial:%s?baud=115200 \"Unknown\" \"Cyclades #%d Serial Prt #%d\"\n",
	       device, i, j + 1);
      }
    }
#elif defined(__APPLE__)
 /*
  * Standard serial ports on MacOS X...
  */

  kern_return_t			kernResult;
  mach_port_t			masterPort;
  io_iterator_t			serialPortIterator;
  CFMutableDictionaryRef	classesToMatch;
  io_object_t			serialService;


  kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
  if (KERN_SUCCESS != kernResult)
    return;

 /*
  * Serial devices are instances of class IOSerialBSDClient.
  */

  classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
  if (classesToMatch != NULL)
  {
    CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey),
                         CFSTR(kIOSerialBSDRS232Type));

    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch,
                                              &serialPortIterator);
    if (kernResult == KERN_SUCCESS)
    {
      while ((serialService = IOIteratorNext(serialPortIterator)))
      {
	CFTypeRef	serialNameAsCFString;
	CFTypeRef	bsdPathAsCFString;
	CFTypeRef	hiddenVal;
	char		serialName[128];
	char		bsdPath[1024];
	Boolean		result;


	/* Check if hidden... */
	hiddenVal = IORegistryEntrySearchCFProperty(serialService, 
						    kIOServicePlane,
						    CFSTR("HiddenPort"),
						    kCFAllocatorDefault,
						    kIORegistryIterateRecursively | 
						    kIORegistryIterateParents);
	if (hiddenVal)
	  CFRelease(hiddenVal);	/* This interface should not be used */
	else
	{
	  serialNameAsCFString =
	      IORegistryEntryCreateCFProperty(serialService,
					      CFSTR(kIOTTYDeviceKey),
					      kCFAllocatorDefault, 0);
	  if (serialNameAsCFString)
	  {
	    result = CFStringGetCString(serialNameAsCFString, serialName,
					sizeof(serialName),
					kCFStringEncodingASCII);
	    CFRelease(serialNameAsCFString);
  
	    if (result)
	    {
	      bsdPathAsCFString =
		  IORegistryEntryCreateCFProperty(serialService,
						  CFSTR(kIOCalloutDeviceKey),
						  kCFAllocatorDefault, 0);
	      if (bsdPathAsCFString)
	      {
		result = CFStringGetCString(bsdPathAsCFString, bsdPath,
					    sizeof(bsdPath),
					    kCFStringEncodingASCII);
		CFRelease(bsdPathAsCFString);
  
		if (result)
		  printf("serial serial:%s?baud=115200 \"Unknown\" \"%s\"\n",
			 bsdPath, serialName);
	      }
	    }
	  }
	}

	IOObjectRelease(serialService);
      }

     /*
      * Release the iterator.
      */

      IOObjectRelease(serialPortIterator);
    }
  }
#endif
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


  datalen = sizeof(data);

  if (cupsSideChannelRead(&command, &status, data, &datalen, 1.0))
  {
    _cupsLangPuts(stderr,
                  _("WARNING: Failed to read side-channel request!\n"));
    return;
  }

  switch (command)
  {
    case CUPS_SC_CMD_DRAIN_OUTPUT :
        if (backendDrainOutput(print_fd, device_fd))
	  status = CUPS_SC_STATUS_IO_ERROR;
	else if (tcdrain(device_fd))
	  status = CUPS_SC_STATUS_IO_ERROR;
	else
	  status = CUPS_SC_STATUS_OK;

	datalen = 0;
        break;

    case CUPS_SC_CMD_GET_BIDI :
	status  = CUPS_SC_STATUS_OK;
        data[0] = use_bc;
        datalen = 1;
        break;

    default :
        status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	datalen = 0;
	break;
  }

  cupsSideChannelWrite(command, status, data, datalen, 1.0);
}


/*
 * End of "$Id: serial.c 7647 2008-06-16 17:39:40Z mike $".
 */
