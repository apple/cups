/*
 * "$Id: usb.c,v 1.18.2.34 2004/04/01 19:00:17 mike Exp $"
 *
 *   USB port backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3142 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   list_devices() - List all available USB devices to stdout.
 *   print_device() - Print a file to a USB device.
 *   main()         - Send a file to the specified USB port.
 */

/*
 * Include necessary headers.
 */

#ifdef __APPLE__
   /* A header order dependency requires this be first */
#  include <ApplicationServices/ApplicationServices.h>
#endif /* __APPLE__ */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/string.h>
#include <signal.h>

#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

void	list_devices(void);
int	print_device(const char *uri, const char *hostname,
	             const char *resource, const char *options,
		     int fp, int copies);


/*
 * Include the vendor-specific USB implementation...
 */

#ifdef __APPLE__
#  include "usb-darwin.c"
#elif defined(__linux) || defined(__sun) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#  include "usb-unix.c"
#else
/*
 * Use dummy functions that do nothing on unsupported platforms...
 * These can be used as templates for implementing USB printing on new
 * platforms...
 */

/*
 * 'list_devices()' - List all available USB devices to stdout.
 */

void
list_devices(void)
{
 /*
  * Don't have any devices to list... Use output of the form:
  *
  *     direct usb:/make/model?serial=foo "Make Model" "USB Printer"
  *
  * Note that "Hewlett Packard" or any other variation MUST be mapped to
  * "HP" for compatibility with the PPD and ICC specs.
  */
}


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
 /*
  * Can't print, so just reference the arguments to eliminate compiler
  * warnings and return and exit status of 1.  Normally you would use the
  * arguments to send a file to the printer and return 0 if everything
  * worked OK and non-zero if there was an error.
  */

  (void)uri;
  (void)hostname;
  (void)resource;
  (void)options;
  (void)fp;
  (void)copies;

  return (1);
}
#endif /* __APPLE__ */


/*
 * 'main()' - Send a file to the specified USB port.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments (6 or 7) */
     char *argv[])			/* I - Command-line arguments */
{
  int		fp;			/* Print file */
  int		copies;			/* Number of copies to print */
  int		status;			/* Exit status */
  int		port;			/* Port number (not used) */
  const char	*uri;			/* Device URI */
  char		method[255],		/* Method in URI */
		hostname[1024],		/* Hostname */
		username[255],		/* Username info (not used) */
		resource[1024],		/* Resource info (device and options) */
		*options;		/* Pointer to options */
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
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fputs("Usage: usb job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * Extract the device name and options from the URI...
  */

  if (strncmp(argv[0], "usb:", 4))
    uri = getenv("DEVICE_URI");
  else
    uri = argv[0];

  if (!uri)
  {
    fputs("ERROR: No device URI found in argv[0] or in DEVICE_URI environment variable!\n", stderr);
    return (1);
  }

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
      fprintf(stderr, "ERROR: unable to open print file %s - %s\n",
              argv[6], strerror(errno));
      return (1);
    }

    copies = atoi(argv[4]);
  }

 /*
  * Finally, send the print file...
  */

  status = print_device(uri, hostname, resource, options, fp, copies);

 /*
  * Close the input file and return...
  */

  if (fp != 0)
    close(fp);

  if (!status)
    fputs("INFO: Ready to print.\n", stderr);

  return (status);
}


/*
 * End of "$Id: usb.c,v 1.18.2.34 2004/04/01 19:00:17 mike Exp $".
 */
