/*
 * "$Id: parallel.c 7810 2008-07-29 01:11:15Z mike $"
 *
 *   Parallel port backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   main()         - Send a file to the specified parallel port.
 *   list_devices() - List all parallel devices.
 *   side_cb()      - Handle side-channel requests...
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

static void	list_devices(void);
static int	side_cb(int print_fd, int device_fd, int snmp_fd,
		        http_addr_t *addr, int use_bc);


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
  int		print_fd,		/* Print file */
		device_fd,		/* Parallel device */
		use_bc;			/* Read back-channel data? */
  int		copies;			/* Number of copies to print */
  size_t	tbytes;			/* Total number of bytes written */
  struct termios opts;			/* Parallel port options */
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
  * Open the parallel port device...
  */

  fputs("STATE: +connecting-to-device\n", stderr);

  do
  {
#if defined(__linux) || defined(__FreeBSD__)
   /*
    * The Linux and FreeBSD parallel port drivers currently are broken WRT
    * select() and bidirection I/O...
    */

    device_fd = open(resource, O_WRONLY | O_EXCL);
    use_bc    = 0;

#else
    if ((device_fd = open(resource, O_RDWR | O_EXCL)) < 0)
    {
      device_fd = open(resource, O_WRONLY | O_EXCL);
      use_bc    = 0;
    }
    else
      use_bc = 1;
#endif /* __linux || __FreeBSD__ */

    if (device_fd == -1)
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
      else if (errno == ENXIO || errno == EIO || errno == ENOENT)
      {
        _cupsLangPuts(stderr,
	              _("INFO: Printer not connected; will retry in 30 "
		        "seconds...\n"));
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

  tcgetattr(device_fd, &opts);

  opts.c_lflag &= ~(ICANON | ECHO | ISIG);	/* Raw mode */

  /**** No options supported yet ****/

  tcsetattr(device_fd, TCSANOW, &opts);

 /*
  * Finally, send the print file...
  */

  tbytes = 0;

  while (copies > 0 && tbytes >= 0)
  {
    copies --;

    if (print_fd != 0)
    {
      fputs("PAGE: 1 1\n", stderr);
      lseek(print_fd, 0, SEEK_SET);
    }

    tbytes = backendRunLoop(print_fd, device_fd, -1, NULL, use_bc, side_cb);

    if (print_fd != 0 && tbytes >= 0)
      _cupsLangPrintf(stderr,
#ifdef HAVE_LONG_LONG
		      _("INFO: Sent print file, %lld bytes...\n"),
#else
		      _("INFO: Sent print file, %ld bytes...\n"),
#endif /* HAVE_LONG_LONG */
		      CUPS_LLCAST tbytes);
  }

 /*
  * Close the socket connection and input file and return...
  */

  close(device_fd);

  if (print_fd != 0)
    close(print_fd);

  return (tbytes < 0 ? CUPS_BACKEND_FAILED : CUPS_BACKEND_OK);
}


/*
 * 'list_devices()' - List all parallel devices.
 */

static void
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
	make_model[1024],	/* Make and model */
	info[1024],		/* Info string */
	uri[1024];		/* Device URI */


  if (!access("/dev/parallel/", 0))
    strcpy(basedevice, "/dev/parallel/");
  else if (!access("/dev/printers/", 0))
    strcpy(basedevice, "/dev/printers/");
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

      snprintf(uri, sizeof(uri), "parallel:%s", device);

      if (!backendGetDeviceID(fd, device_id, sizeof(device_id),
                              make_model, sizeof(make_model),
			      NULL, uri, sizeof(uri)))
      {
        snprintf(info, sizeof(info), "%s LPT #%d", make_model, i + 1);
	cupsBackendReport("direct", uri, make_model, info, device_id, NULL);
      }
      else
      {
        snprintf(info, sizeof(info), "LPT #%d", i + 1);
	cupsBackendReport("direct", uri, NULL, info, NULL, NULL);
      }

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
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
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
 * 'side_cb()' - Handle side-channel requests...
 */

static int				/* O - 0 on success, -1 on error */
side_cb(int         print_fd,		/* I - Print file */
        int         device_fd,		/* I - Device file */
        int         snmp_fd,		/* I - SNMP socket (unused) */
	http_addr_t *addr,		/* I - Device address (unused) */
	int         use_bc)		/* I - Using back-channel? */
{
  cups_sc_command_t	command;	/* Request command */
  cups_sc_status_t	status;		/* Request/response status */
  char			data[2048];	/* Request/response data */
  int			datalen;	/* Request/response data size */


  (void)snmp_fd;
  (void)addr;

  datalen = sizeof(data);

  if (cupsSideChannelRead(&command, &status, data, &datalen, 1.0))
    return (-1);

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

    case CUPS_SC_CMD_GET_DEVICE_ID :
        memset(data, 0, sizeof(data));

        if (backendGetDeviceID(device_fd, data, sizeof(data) - 1,
	                       NULL, 0, NULL, NULL, 0))
        {
	  status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	  datalen = 0;
	}
	else
        {
	  status  = CUPS_SC_STATUS_OK;
	  datalen = strlen(data);
	}
        break;

    default :
        status  = CUPS_SC_STATUS_NOT_IMPLEMENTED;
	datalen = 0;
	break;
  }

  return (cupsSideChannelWrite(command, status, data, datalen, 1.0));
}


/*
 * End of "$Id: parallel.c 7810 2008-07-29 01:11:15Z mike $".
 */
