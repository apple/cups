/*
 * "$Id: serial.c,v 1.32.2.16 2003/09/17 19:26:32 mike Exp $"
 *
 *   Serial port backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()         - Send a file to the printer or server.
 *   list_devices() - List all serial devices.
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/string.h>
#include <signal.h>

#ifdef __hpux
#  include <sys/modem.h>
#endif /* __hpux */

#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
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


/*
 * Local functions...
 */

void	list_devices(void);


/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  char		method[255],	/* Method in URI */
		hostname[1024],	/* Hostname */
		username[255],	/* Username info (not used) */
		resource[1024],	/* Resource info (device and options) */
		*options,	/* Pointer to options */
		name[255],	/* Name of option */
		value[255],	/* Value of option */
		*ptr;		/* Pointer into name or value */
  int		port;		/* Port number (not used) */
  int		fp;		/* Print file */
  int		copies;		/* Number of copies to print */
  int		fd;		/* Parallel device */
  int		wbytes;		/* Number of bytes written */
  size_t	nbytes,		/* Number of bytes read */
		tbytes;		/* Total number of bytes written */
  int		dtrdsr;		/* Do dtr/dsr flow control? */
  int		bufsize;	/* Size of output buffer for writes */
  char		buffer[8192],	/* Output buffer */
		*bufptr;	/* Pointer into buffer */
  struct termios opts;		/* Serial port options */
  struct termios origopts;	/* Original port options */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
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
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fputs("Usage: serial job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    fp     = 0;
    copies = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = open(argv[6], O_RDONLY)) < 0)
    {
      perror("ERROR: unable to open print file");
      return (1);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Extract the device name and options from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

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

  do
  {
    if ((fd = open(resource, O_WRONLY | O_NOCTTY | O_EXCL | O_NDELAY)) == -1)
    {
      if (errno == EBUSY)
      {
        fputs("INFO: Serial port busy; will retry in 30 seconds...\n", stderr);
	sleep(30);
      }
      else
      {
	fprintf(stderr, "ERROR: Unable to open serial port device file \"%s\": %s\n",
	        resource, strerror(errno));
	return (1);
      }
    }
  }
  while (fd < 0);

 /*
  * Set any options provided...
  */

  tcgetattr(fd, &origopts);
  tcgetattr(fd, &opts);

  opts.c_lflag &= ~(ICANON | ECHO | ISIG);	/* Raw mode */
  opts.c_oflag &= ~OPOST;			/* Don't post-process */

  bufsize = 96;		/* 9600 baud / 10 bits/char / 10Hz */
  dtrdsr  = 0;		/* No dtr/dsr flow control */

  if (options != NULL)
    while (*options)
    {
     /*
      * Get the name...
      */

      for (ptr = name; *options && *options != '=';)
        *ptr++ = *options++;
      *ptr = '\0';

      if (*options == '=')
      {
       /*
        * Get the value...
	*/

        options ++;

	for (ptr = value; *options && *options != '+';)
          *ptr++ = *options++;
	*ptr = '\0';

	if (*options == '+')
	  options ++;
      }
      else
        value[0] = '\0';

     /*
      * Process the option...
      */

      if (strcasecmp(name, "baud") == 0)
      {
       /*
        * Set the baud rate...
	*/

        bufsize = atoi(value) / 100;

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
	      fprintf(stderr, "WARNING: Unsupported baud rate %s!\n", value);
	      break;
	}
#endif /* B19200 == 19200 */
      }
      else if (strcasecmp(name, "bits") == 0)
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
      else if (strcasecmp(name, "parity") == 0)
      {
       /*
	* Set parity checking...
	*/

	if (strcasecmp(value, "even") == 0)
	{
	  opts.c_cflag |= PARENB;
          opts.c_cflag &= ~PARODD;
	}
	else if (strcasecmp(value, "odd") == 0)
	{
	  opts.c_cflag |= PARENB;
          opts.c_cflag |= PARODD;
	}
	else if (strcasecmp(value, "none") == 0)
	  opts.c_cflag &= ~PARENB;
      }
      else if (strcasecmp(name, "flow") == 0)
      {
       /*
	* Set flow control...
	*/

	if (strcasecmp(value, "none") == 0)
	{
	  opts.c_iflag &= ~(IXON | IXOFF);
          opts.c_cflag &= ~CRTSCTS;
	}
	else if (strcasecmp(value, "soft") == 0)
	{
	  opts.c_iflag |= IXON | IXOFF;
          opts.c_cflag &= ~CRTSCTS;
	}
	else if (strcasecmp(value, "hard") == 0 ||
	         strcasecmp(value, "rtscts") == 0)
        {
	  opts.c_iflag &= ~(IXON | IXOFF);
          opts.c_cflag |= CRTSCTS;
	}
	else if (strcasecmp(value, "dtrdsr") == 0)
	{
	  opts.c_iflag &= ~(IXON | IXOFF);
          opts.c_cflag &= ~CRTSCTS;

	  dtrdsr = 1;
	}
      }
    }

  tcsetattr(fd, TCSANOW, &opts);
  fcntl(fd, F_SETFL, 0);

 /*
  * Now that we are "connected" to the port, ignore SIGTERM so that we
  * can finish out any page data the driver sends (e.g. to eject the
  * current page...  Only ignore SIGTERM if we are printing data from
  * stdin (otherwise you can't cancel raw jobs...)
  */

  if (argc < 7)
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

  if (bufsize > sizeof(buffer))
    bufsize = sizeof(buffer);

  while (copies > 0)
  {
    copies --;

    if (fp != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(fp, 0, SEEK_SET);
    }

    if (dtrdsr)
    {
     /*
      * Check the port and sleep until DSR is set...
      */

      int status;


      if (!ioctl(fd, TIOCMGET, &status))
        if (!(status & TIOCM_DSR))
	{
	 /*
	  * Wait for DSR to go high...
	  */

	  fputs("DEBUG: DSR is low; waiting for device...\n", stderr);

          do
	  {
	    sleep(1);
	    if (ioctl(fd, TIOCMGET, &status))
	      break;
	  }
	  while (!(status & TIOCM_DSR));

	  fputs("DEBUG: DSR is high; writing to device...\n", stderr);
        }
    }

    tbytes = 0;
    while ((nbytes = read(fp, buffer, bufsize)) > 0)
    {
     /*
      * Write the print data to the printer...
      */

      tbytes += nbytes;
      bufptr = buffer;

      while (nbytes > 0)
      {
	if ((wbytes = write(fd, bufptr, nbytes)) < 0)
	  if (errno == ENOTTY)
	    wbytes = write(fd, bufptr, nbytes);

	if (wbytes < 0)
	{
	  perror("ERROR: Unable to send print file to printer");
	  break;
	}

	nbytes -= wbytes;
	bufptr += wbytes;
      }

      if (wbytes < 0)
        break;

      if (argc > 6)
	fprintf(stderr, "INFO: Sending print file, %lu bytes...\n",
	        (unsigned long)tbytes);
    }
  }

 /*
  * Close the serial port and input file and return...
  */

  tcsetattr(fd, TCSADRAIN, &origopts);

  close(fd);
  if (fp != 0)
    close(fp);

  fputs("INFO: Ready to print.\n", stderr);

  return (0);
}


/*
 * 'list_devices()' - List all serial devices.
 */

void
list_devices(void)
{
#if defined(__hpux) || defined(__sgi) || defined(__sun) || defined(__FreeBSD__) || defined(__OpenBSD__)
  static char	*funky_hex = "0123456789abcdefghijklmnopqrstuvwxyz";
				/* Funky hex numbering used for some devices */
#endif /* __hpux || __sgi || __sun || __FreeBSD__ || __OpenBSD__ */

#if defined(__linux) || defined(linux) || defined(__linux__)
  int	i;		/* Looping var */
  int	fd;		/* File descriptor */
  char	device[255];	/* Device filename */


  for (i = 0; i < 100; i ++)
  {
    sprintf(device, "/dev/ttyS%d", i);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
#  if defined(_ARCH_PPC) || defined(powerpc) || defined(__powerpc)
      printf("serial serial:%s?baud=230400 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
#  else
      printf("serial serial:%s?baud=115200 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
#  endif /* _ARCH_PPC || powerpc || __powerpc */
    }
  }

  for (i = 0; i < 16; i ++)
  {
    sprintf(device, "/dev/usb/ttyUSB%d", i);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=230400 \"Unknown\" \"USB Serial Port #%d\"\n",
             device, i + 1);
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
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */


 /*
  * Standard serial ports...
  */

  for (i = 0; i < 26; i ++)
  {
    sprintf(device, "/dev/cua/%c", 'a' + i);
    if (access(device, 0) == 0)
#ifdef B115200
      printf("serial serial:%s?baud=115200 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
#else
      printf("serial serial:%s?baud=38400 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
#endif /* B115200 */
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
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  int	i, j;		/* Looping vars */
  int	fd;		/* File descriptor */
  char	device[255];	/* Device filename */


 /*
  * SIO ports...
  */

  for (i = 0; i < 32; i ++)
  {
    sprintf(device, "/dev/ttyd%c", funky_hex[i]);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=115200 \"Unknown\" \"Standard Serial Port #%d\"\n",
             device, i + 1);
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
  int	i, j;		/* Looping vars */
  int	fd;		/* File descriptor */
  char	device[255];	/* Device filename */


 /*
  * Standard serial ports...
  */

  for (i = 0; i < 4; i ++)
  {
    sprintf(device, "/dev/tty%02d", i);
    if ((fd = open(device, O_WRONLY | O_NOCTTY | O_NDELAY)) >= 0)
    {
      close(fd);
      printf("serial serial:%s?baud=115200 \"Unknown\" \"Serial Port #%d\"\n",
             device, i + 1);
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

  printf("serial serial \"Unknown\" \"Serial Printer (serial)\"\n");

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
	char		serialName[128];
	char		bsdPath[1024];
	Boolean		result;


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
		printf("serial serial:%s?baud=115200 \"Unknown\" \"%s\"\n", bsdPath,
		       serialName);
	    }
	  }
	}

	IOObjectRelease(serialService);
      }

      IOObjectRelease(serialPortIterator);    /* Release the iterator. */
    }
  }
#endif
}


/*
 * End of "$Id: serial.c,v 1.32.2.16 2003/09/17 19:26:32 mike Exp $".
 */
