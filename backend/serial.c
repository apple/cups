/*
 * "$Id: serial.c,v 1.5 1999/04/21 15:02:01 mike Exp $"
 *
 *   Serial port backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Send a file to the printer or server.
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
  FILE		*fp;		/* Print file */
  int		fd;		/* Parallel device */
  int		error;		/* Error code (if any) */
  size_t	nbytes,		/* Number of bytes written */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192];	/* Output buffer */
  struct termios opts;		/* Parallel port options */


  if (argc < 6 || argc > 7)
  {
    fputs("Usage: serial job-id user title copies options [file]\n", stderr);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, send stdin instead...
  */

  if (argc == 6)
    fp = stdin;
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
    perror("ERROR: Unable to open serial port device file");
    return (1);
  }

 /*
  * Set any options provided...
  */

  tcgetattr(fd, &opts);

  opts.c_lflag &= ~(ICANON | ECHO | ISIG);	/* Raw mode */

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
    }

  tcsetattr(fd, TCSANOW, &opts);

 /*
  * Finally, send the print file...
  */

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

 /*
  * Close the socket connection and input file and return...
  */

  close(fd);
  if (fp != stdin)
    fclose(fp);

  return (0);
}


/*
 * End of "$Id: serial.c,v 1.5 1999/04/21 15:02:01 mike Exp $".
 */
