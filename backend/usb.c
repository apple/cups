/*
 * "$Id: usb.c,v 1.2 2000/02/23 03:28:38 mike Exp $"
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
#include <cups/string.h>

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
  int		error;		/* Error code (if any) */
  size_t	nbytes,		/* Number of bytes written */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192];	/* Output buffer */
  struct termios opts;		/* Parallel port options */


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

  if ((fd = open(resource, O_WRONLY)) == -1)
  {
    perror("ERROR: Unable to open USB port device file");
    return (1);
  }

 /*
  * Set any options provided...
  */

  tcgetattr(fd, &opts);

  opts.c_lflag &= ~(ICANON | ECHO | ISIG);	/* Raw mode */

  /**** No options supported yet ****/

  tcsetattr(fd, TCSANOW, &opts);

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

      if (write(fd, buffer, nbytes) < nbytes)
      {
	perror("ERROR: Unable to send print file to printer");
	break;
      }
      else
	tbytes += nbytes;

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
  int	is_printer;		/* Printer device? */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */
  FILE	*probe;			/* /proc/parport/n/autoprobe file */
  char	line[1024],		/* Line from file */
	*delim,			/* Delimiter in file */
	make[IPP_MAX_NAME],	/* Make from file */
	model[IPP_MAX_NAME];	/* Model from file */


  if ((probe = fopen("/proc/bus/usb/devices", "r")) != NULL)
  {
    i          = 0;
    is_printer = 0;

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

      if (strncmp(line, "S:", 2) == 0 && is_printer)
      {
       /*
        * String attribute...
	*/

        if (strncmp(line, "S:  Manufacturer=", 17) == 0)
	  strncpy(make, line + 17, sizeof(make) - 1);
        else if (strncmp(line, "S:  Product=", 12) == 0)
	  strncpy(model, line + 12, sizeof(model) - 1);
      }
      else if (is_printer)
      {
       /*
        * We were processing a printer device; send the info out...
	*/

	if (make[0])
	  printf("direct usb:/dev/usblp%d \"%s %s\" \"USB Printer #%d\"\n",
		 i, make, model, i + 1);
	else if (model[0])
	  printf("direct usb:/dev/usblp%d \"%s\" \"USB Printer #%d\"\n",
		 i, model, i + 1);
	else
	  printf("direct usb:/dev/usblp%d \"Unknown\" \"USB Printer #%d\"\n",
		 i, i + 1);

        is_printer = strstr(line, "Prnt=01") != NULL;
	i ++;

	memset(make, 0, sizeof(make));
	memset(model, 0, sizeof(model));
      }
      else
        is_printer = strstr(line, "Prnt=01") != NULL;

    }

    if (is_printer)
    {
     /*
      * We were processing a printer device; send the info out...
      */

      if (make[0])
	printf("direct usb:/dev/usblp%d \"%s %s\" \"USB Printer #%d\"\n",
	       i, make, model, i + 1);
      else if (model[0])
	printf("direct usb:/dev/usblp%d \"%s\" \"USB Printer #%d\"\n",
	       i, model, i + 1);
      else
	printf("direct usb:/dev/usblp%d \"Unknown\" \"USB Printer #%d\"\n",
	       i, i + 1);
    }

    fclose(probe);
  }
  else
  {
    for (i = 0; i < 8; i ++)
    {
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
#endif
}


/*
 * End of "$Id: usb.c,v 1.2 2000/02/23 03:28:38 mike Exp $".
 */
