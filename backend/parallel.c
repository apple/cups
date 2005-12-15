/*
 * "$Id$"
 *
 *   Parallel port backend for the Common UNIX Printing System (CUPS).
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
 *   main()         - Send a file to the specified parallel port.
 *   list_devices() - List all parallel devices.
 */

/*
 * Include necessary headers.
 */

#include <cups/backend.h>
#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/string.h>
#include <signal.h>
#include <sys/select.h>
#include "ieee1284.c"

#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
#  include <sys/socket.h>
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


/*
 * Local functions...
 */

void	list_devices(void);


/*
 * 'main()' - Send a file to the specified parallel port.
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
		*options;		/* Pointer to options */
  int		port;			/* Port number (not used) */
  int		fp;			/* Print file */
  int		copies;			/* Number of copies to print */
  int		fd;			/* Parallel device */
  int		rbytes;			/* Number of bytes read */
  int		wbytes;			/* Number of bytes written */
  size_t	nbytes,			/* Number of bytes read */
		tbytes;			/* Total number of bytes written */
  char		buffer[8192],		/* Output buffer */
		*bufptr;		/* Pointer into buffer */
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
    fputs("Usage: parallel job-id user title copies options [file]\n", stderr);
    return (CUPS_BACKEND_FAILED);
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
      return (CUPS_BACKEND_FAILED);
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
  * Open the parallel port device...
  */

  do
  {
    if ((fd = open(resource, O_WRONLY | O_EXCL)) == -1)
    {
      if (getenv("CLASS") != NULL)
      {
       /*
        * If the CLASS environment variable is set, the job was submitted
	* to a class and not to a specific queue.  In this case, we want
	* to abort immediately so that the job can be requeued on the next
	* available printer in the class.
	*/

        fputs("INFO: Unable to open parallel port, queuing on next printer in class...\n",
	      stderr);

       /*
        * Sleep 5 seconds to keep the job from requeuing too rapidly...
	*/

	sleep(5);

        return (CUPS_BACKEND_FAILED);
      }

      if (errno == EBUSY)
      {
        fputs("INFO: Parallel port busy; will retry in 30 seconds...\n", stderr);
	sleep(30);
      }
      else if (errno == ENXIO || errno == EIO || errno == ENOENT)
      {
        fputs("INFO: Printer not connected; will retry in 30 seconds...\n", stderr);
	sleep(30);
      }
      else
      {
	fprintf(stderr, "ERROR: Unable to open parallel port device file \"%s\": %s\n",
	        resource, strerror(errno));
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

	  if ((rbytes = read(fd, resource, sizeof(resource))) > 0)
	  {
	    fprintf(stderr, "DEBUG: Received %d bytes of back-channel data!\n",
	            rbytes);
            cupsBackchannelWrite(resource, rbytes, 1.0);
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

      if (argc > 6)
	fprintf(stderr, "INFO: Sending print file, %lu bytes...\n",
	        (unsigned long)tbytes);
    }
  }

 /*
  * Close the socket connection and input file and return...
  */

  close(fd);
  if (fp != 0)
    close(fp);

  return (wbytes < 0 ? CUPS_BACKEND_FAILED : CUPS_BACKEND_OK);
}


/*
 * 'list_devices()' - List all parallel devices.
 */

void
list_devices(void)
{
#if defined(__hpux) || defined(__sgi) || defined(__sun)
  static char	*funky_hex = "0123456789abcdefghijklmnopqrstuvwxyz";
				/* Funky hex numbering used for some devices */
#endif /* __hpux || __sgi || __sun */

#ifdef __linux
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255],		/* Device filename */
	basedevice[255],	/* Base device filename for ports */
	device_id[1024],	/* Device ID string */
	make_model[1024];	/* Make and model */


  if (!access("/dev/parallel/", 0))
    strcpy(basedevice, "/dev/parallel/");
  else if (!access("/dev/printers/", 0))
    strcpy(basedevice, "/dev/printers/");
  else if (!access("/dev/par0", 0))
    strcpy(basedevice, "/dev/par");
  else
    strcpy(basedevice, "/dev/lp");

  for (i = 0; i < 4; i ++)
  {
   /*
    * Open the port, if available...
    */

    sprintf(device, "%s%d", basedevice, i);
    if ((fd = open(device, O_RDWR | O_EXCL)) < 0)
      fd = open(device, O_WRONLY);

    if (fd >= 0)
    {
     /*
      * Now grab the IEEE 1284 device ID string...
      */

      if (!get_device_id(fd, device_id, sizeof(device_id),
                         make_model, sizeof(make_model),
			 NULL, NULL, 0))
	printf("direct parallel:%s \"%s\" \"%s LPT #%d\" \"%s\"\n", device,
	       make_model, make_model, i + 1, device_id);
      else
	printf("direct parallel:%s \"Unknown\" \"LPT #%d\"\n", device, i + 1);

      close(fd);
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
    if (inv->inv_class == INV_PARALLEL &&
        (inv->inv_type == INV_ONBOARD_PLP ||
         inv->inv_type == INV_EPP_ECP_PLP))
    {
     /*
      * Standard parallel port...
      */

      puts("direct parallel:/dev/plp \"Unknown\" \"Onboard Parallel Port\"");
    }
    else if (inv->inv_class == INV_PARALLEL &&
             inv->inv_type == INV_EPC_PLP)
    {
     /*
      * EPC parallel port...
      */

      printf("direct parallel:/dev/plp%d \"Unknown\" \"Integral EPC parallel port, Ebus slot %d\"\n",
             inv->inv_controller, inv->inv_controller);
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
          sprintf(device, "/dev/lpn%d%c", j, funky_hex[n]);
        else if (i == 9)	/* PCI */
          sprintf(device, "/dev/lpp%d%c", j, funky_hex[n]);
        else			/* SCSI */
          sprintf(device, "/dev/lp%d%d%c", i, j, funky_hex[n]);

	if (access(device, 0) == 0)
	{
	  if (i == 8)
	    printf("direct parallel:%s \"Unknown\" \"Central Data EtherLite Parallel Port, ID %d, port %d\"\n",
	           device, j, n);
	  else if (i == 9)
	    printf("direct parallel:%s \"Unknown\" \"Central Data PCI Parallel Port, ID %d, port %d\"\n",
	           device, j, n);
  	  else
	    printf("direct parallel:%s \"Unknown\" \"Central Data SCSI Parallel Port, logical bus %d, ID %d, port %d\"\n",
	           device, i, j, n);
	}
      }
#elif defined(__sun)
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */


 /*
  * Standard parallel ports...
  */

  for (i = 0; i < 10; i ++)
  {
    sprintf(device, "/dev/ecpp%d", i);
    if (access(device, 0) == 0)
      printf("direct parallel:%s \"Unknown\" \"Sun IEEE-1284 Parallel Port #%d\"\n",
             device, i + 1);
  }

  for (i = 0; i < 10; i ++)
  {
    sprintf(device, "/dev/bpp%d", i);
    if (access(device, 0) == 0)
      printf("direct parallel:%s \"Unknown\" \"Sun Standard Parallel Port #%d\"\n",
             device, i + 1);
  }

  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lp%d", i);

    if (access(device, 0) == 0)
      printf("direct parallel:%s \"Unknown\" \"PC Parallel Port #%d\"\n",
             device, i + 1);
  }

 /*
  * MAGMA parallel ports...
  */

  for (i = 0; i < 40; i ++)
  {
    sprintf(device, "/dev/pm%02d", i);
    if (access(device, 0) == 0)
      printf("direct parallel:%s \"Unknown\" \"MAGMA Parallel Board #%d Port #%d\"\n",
             device, (i / 10) + 1, (i % 10) + 1);
  }

 /*
  * Central Data parallel ports...
  */

  for (i = 0; i < 9; i ++)
    for (j = 0; j < 8; j ++)
      for (n = 0; n < 32; n ++)
      {
        if (i == 8)	/* EtherLite */
          sprintf(device, "/dev/sts/lpN%d%c", j, funky_hex[n]);
        else
          sprintf(device, "/dev/sts/lp%c%d%c", i + 'C', j,
                  funky_hex[n]);

	if (access(device, 0) == 0)
	{
	  if (i == 8)
	    printf("direct parallel:%s \"Unknown\" \"Central Data EtherLite Parallel Port, ID %d, port %d\"\n",
	           device, j, n);
  	  else
	    printf("direct parallel:%s \"Unknown\" \"Central Data SCSI Parallel Port, logical bus %d, ID %d, port %d\"\n",
	           device, i, j, n);
	}
      }
#elif defined(__hpux)
  int		i, j, n;	/* Looping vars */
  char		device[255];	/* Device filename */


 /*
  * Standard parallel ports...
  */

  if (access("/dev/rlp", 0) == 0)
    puts("direct parallel:/dev/rlp \"Unknown\" \"Standard Parallel Port (/dev/rlp)\"");

  for (i = 0; i < 7; i ++)
    for (j = 0; j < 7; j ++)
    {
      sprintf(device, "/dev/c%dt%dd0_lp", i, j);
      if (access(device, 0) == 0)
	printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d,%d\"\n",
	       device, i, j);
    }

 /*
  * Central Data parallel ports...
  */

  for (i = 0; i < 9; i ++)
    for (j = 0; j < 8; j ++)
      for (n = 0; n < 32; n ++)
      {
        if (i == 8)	/* EtherLite */
          sprintf(device, "/dev/lpN%d%c", j, funky_hex[n]);
        else
          sprintf(device, "/dev/lp%c%d%c", i + 'C', j,
                  funky_hex[n]);

	if (access(device, 0) == 0)
	{
	  if (i == 8)
	    printf("direct parallel:%s \"Unknown\" \"Central Data EtherLite Parallel Port, ID %d, port %d\"\n",
	           device, j, n);
  	  else
	    printf("direct parallel:%s \"Unknown\" \"Central Data SCSI Parallel Port, logical bus %d, ID %d, port %d\"\n",
	           device, i, j, n);
	}
      }
#elif defined(__osf__)
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */


  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lp%d", i);
    if ((fd = open(device, O_WRONLY)) >= 0)
    {
      close(fd);
      printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d\"\n", device, i + 1);
    }
  }
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */


  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lpt%d", i);
    if ((fd = open(device, O_WRONLY)) >= 0)
    {
      close(fd);
      printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d (interrupt-driven)\"\n", device, i + 1);
    }

    sprintf(device, "/dev/lpa%d", i);
    if ((fd = open(device, O_WRONLY)) >= 0)
    {
      close(fd);
      printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d (polled)\"\n", device, i + 1);
    }
  }
#elif defined(_AIX)
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */


  for (i = 0; i < 8; i ++)
  {
    sprintf(device, "/dev/lp%d", i);
    if ((fd = open(device, O_WRONLY)) >= 0)
    {
      close(fd);
      printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d\"\n", device, i + 1);
    }
  }
#endif
}


/*
 * End of "$Id$".
 */
