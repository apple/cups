/*
 * "$Id: usb.c,v 1.12 2000/10/16 23:02:09 mike Exp $"
 *
 *   USB port backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   main()         - Send a file to the specified USB port.
 *   list_devices() - List all USB devices.
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

#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local functions...
 */

void	list_devices(void);


/*
 * 'main()' - Send a file to the specified USB port.
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
		*options;	/* Pointer to options */
  int		port;		/* Port number (not used) */
  FILE		*fp;		/* Print file */
  int		copies;		/* Number of copies to print */
  int		fd;		/* Parallel device */
  int		wbytes;		/* Number of bytes written */
  size_t	nbytes,		/* Number of bytes read */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192],	/* Output buffer */
		*bufptr;	/* Pointer into buffer */
  struct termios opts;		/* Parallel port options */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  if (argc == 1)
  {
    list_devices();
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fputs("Usage: USB job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
  {
    fp     = stdin;
    copies = 1;
  }
  else
  {
   /*
    * Try to open the print file...
    */

    if ((fp = fopen(argv[6], "rb")) == NULL)
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
  * Open the USB port device...
  */

  do
  {
    if ((fd = open(resource, O_WRONLY | O_EXCL)) == -1)
    {
      if (errno == EBUSY)
      {
        fputs("INFO: USB port busy; will retry in 30 seconds...\n", stderr);
	sleep(30);
      }
      else
      {
	perror("ERROR: Unable to open USB port device file");
	return (1);
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
  * current page...
  */

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

 /*
  * Finally, send the print file...
  */

  while (copies > 0)
  {
    copies --;

    if (fp != stdin)
    {
      fputs("PAGE: 1 1\n", stderr);
      rewind(fp);
    }

    tbytes = 0;
    while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
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

      if (argc > 6)
	fprintf(stderr, "INFO: Sending print file, %u bytes...\n", tbytes);
    }
  }

 /*
  * Close the socket connection and input file and return...
  */

  close(fd);
  if (fp != stdin)
    fclose(fp);

  return (0);
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
  char	device[255];		/* Device filename */
  FILE	*probe;			/* /proc/bus/usb/devices file */
  char	line[1024],		/* Line from file */
	*delim,			/* Delimiter in file */
	make[IPP_MAX_NAME],	/* Make from file */
	model[IPP_MAX_NAME];	/* Model from file */


 /*
  * First try opening one of the USB devices to load the driver
  * module as needed...
  */

  if ((fd = open("/dev/usb/lp0", O_WRONLY)) >= 0)
    close(fd); /* 2.3.x and 2.4.x */
  else if ((fd = open("/dev/usb/usblp0", O_WRONLY)) >= 0)
    close(fd); /* Mandrake 7.x */
  else if ((fd = open("/dev/usblp0", O_WRONLY)) >= 0)
    close(fd); /* 2.2.x */

 /*
  * Then look at the device list for the USB bus...
  */

  if ((probe = fopen("/proc/bus/usb/devices", "r")) != NULL)
  {
   /*
    * Scan the device list...
    */

    i = 0;

    memset(make, 0, sizeof(make));
    memset(model, 0, sizeof(model));

    while (fgets(line, sizeof(line), probe) != NULL)
    {
     /*
      * Strip trailing newline.
      */

      if ((delim = strrchr(line, '\n')) != NULL)
	*delim = '\0';

     /*
      * See if it is a printer device ("P: ...")
      */

      if (strncmp(line, "S:", 2) == 0)
      {
       /*
        * String attribute...
	*/

        if (strncmp(line, "S:  Manufacturer=", 17) == 0)
	{
	  strncpy(make, line + 17, sizeof(make) - 1);
	  if (strcmp(make, "Hewlett-Packard") == 0)
	    strcpy(make, "HP");
	}
        else if (strncmp(line, "S:  Product=", 12) == 0)
	  strncpy(model, line + 12, sizeof(model) - 1);
      }
      else if (strncmp(line, "I:", 2) == 0 &&
               (strstr(line, "Driver=printer") != NULL ||
	        strstr(line, "Driver=usblp") != NULL) &&
	       make[0] && model[0])
      {
       /*
        * We were processing a printer device; send the info out...
	*/

        sprintf(device, "/dev/usb/lp%d", i);
	if (access(device, 0))
	{
	  sprintf(device, "/dev/usb/usblp%d", i);

	  if (access(device, 0))
	    sprintf(device, "/dev/usblp%d", i);
	}

	printf("direct usb:%s \"%s %s\" \"USB Printer #%d\"\n",
	       device, make, model, i + 1);

	i ++;

	memset(make, 0, sizeof(make));
	memset(model, 0, sizeof(model));
      }
    }

    fclose(probe);
  }
  else
  {
   /*
    * Just probe manually for USB devices...
    */

    for (i = 0; i < 8; i ++)
    {
      sprintf(device, "/dev/usb/lp%d", i);
      if ((fd = open(device, O_WRONLY)) >= 0)
      {
	close(fd);
	printf("direct usb:%s \"Unknown\" \"USB Printer #%d\"\n", device, i + 1);
      }

      sprintf(device, "/dev/usb/usblp%d", i);
      if ((fd = open(device, O_WRONLY)) >= 0)
      {
	close(fd);
	printf("direct usb:%s \"Unknown\" \"USB Printer #%d\"\n", device, i + 1);
      }

      sprintf(device, "/dev/usblp%d", i);
      if ((fd = open(device, O_WRONLY)) >= 0)
      {
	close(fd);
	printf("direct usb:%s \"Unknown\" \"USB Printer #%d\"\n", device, i + 1);
      }
    }
  }
#elif defined(__sgi)
#elif defined(__sun)
#elif defined(__hpux)
#elif defined(__osf)
#elif defined(FreeBSD) || defined(OpenBSD) || defined(NetBSD)
  int   i;                      /* Looping var */
  int   fd;                     /* File descriptor */
  char  device[255];            /* Device filename */

  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/ulpt%d", i);
    if ((fd = open(device, O_WRONLY)) >= 0)
    {
      close(fd);
      printf("direct usb:%s \"Unknown\" \"USB Port #%d\"\n", device, i + 1);
    }
  }
#endif
}


/*
 * End of "$Id: usb.c,v 1.12 2000/10/16 23:02:09 mike Exp $".
 */
