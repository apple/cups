/*
 * "$Id: parallel.c,v 1.17 2000/03/02 22:04:12 mike Exp $"
 *
 *   Parallel port backend for the Common UNIX Printing System (CUPS).
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
 *   main()         - Send a file to the specified parallel port.
 *   list_devices() - List all parallel devices.
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
    fputs("Usage: parallel job-id user title copies options [file]\n", stderr);
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
  * Open the parallel port device...
  */

  if ((fd = open(resource, O_WRONLY)) == -1)
  {
    perror("ERROR: Unable to open parallel port device file");
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
 * 'list_devices()' - List all parallel devices.
 */

void
list_devices(void)
{
  static char	*funky_hex = "0123456789abcdefghijklmnopqrstuvwxyz";
				/* Funky hex numbering used for some devices */

#ifdef __linux
  int	i;			/* Looping var */
  int	fd;			/* File descriptor */
  char	device[255];		/* Device filename */
  FILE	*probe;			/* /proc/parport/n/autoprobe file */
  char	line[1024],		/* Line from file */
	*delim,			/* Delimiter in file */
	make[IPP_MAX_NAME],	/* Make from file */
	model[IPP_MAX_NAME];	/* Model from file */


  for (i = 0; i < 4; i ++)
  {
    sprintf(device, "/proc/parport/%d/autoprobe", i);
    if ((probe = fopen(device, "r")) != NULL)
    {
      memset(make, 0, sizeof(make));
      memset(model, 0, sizeof(model));
      strcpy(model, "Unknown");

      while (fgets(line, sizeof(line), probe) != NULL)
      {
       /*
        * Strip trailing ; and/or newline.
	*/

        if ((delim = strrchr(line, ';')) != NULL)
	  *delim = '\0';
	else if ((delim = strrchr(line, '\n')) != NULL)
	  *delim = '\0';

       /*
        * Look for MODEL and MANUFACTURER lines...
	*/

        if (strncmp(line, "MODEL:", 6) == 0 &&
	    strncmp(line, "MODEL:Unknown", 13) != 0)
	  strncpy(model, line + 6, sizeof(model) - 1);
	else if (strncmp(line, "MANUFACTURER:", 13) == 0 &&
	         strncmp(line, "MANUFACTURER:Unknown", 20) != 0)
	  strncpy(make, line + 13, sizeof(make) - 1);
      }

      fclose(probe);

      if (make[0])
	printf("direct parallel:/dev/lp%d \"%s %s\" \"Parallel Port #%d\"\n",
	       i, make, model, i + 1);
      else
	printf("direct parallel:/dev/lp%d \"%s\" \"Parallel Port #%d\"\n",
	       i, model, i + 1);
    }
    else
    {
      sprintf(device, "/dev/lp%d", i);
      if ((fd = open(device, O_WRONLY)) >= 0)
      {
	close(fd);
	printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d\"\n", device, i + 1);
      }
      else
      {
	sprintf(device, "/dev/par%d", i);
	if ((fd = open(device, O_WRONLY)) >= 0)
	{
	  close(fd);
	  printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d\"\n", device, i + 1);
	}
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
	printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d,%d\"\n", i, j);
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
#elif defined(FreeBSD) || defined(OpenBSD) || defined(NetBSD)
  int	i;			/* Looping var */
  char	device[255];		/* Device filename */


  for (i = 0; i < 3; i ++)
  {
    sprintf(device, "/dev/lpt%d", i);
    if ((fd = open(device, O_WRONLY)) >= 0)
    {
      close(fd);
      printf("direct parallel:%s \"Unknown\" \"Parallel Port #%d\"\n", device, i + 1);
    }
  }
#endif
}


/*
 * End of "$Id: parallel.c,v 1.17 2000/03/02 22:04:12 mike Exp $".
 */
