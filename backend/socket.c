/*
 * "$Id: socket.c,v 1.4 1999/03/21 02:09:58 mike Exp $"
 *
 *   AppSocket backend for the Common UNIX Printing System (CUPS).
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(WIN32) || defined(__EMX__)
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
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
		resource[1024];	/* Resource info (not used) */
  FILE		*fp;		/* Print file */
  int		port;		/* Port number */
  int		fd;		/* AppSocket */
  int		error;		/* Error code (if any) */
  struct sockaddr_in addr;	/* Socket address */
  struct hostent *hostaddr;	/* Host address */
  size_t	nbytes,		/* Number of bytes written */
		tbytes;		/* Total number of bytes written */
  char		buffer[8192];	/* Output buffer */
  struct timeval timeout;	/* Timeout for select() */
  fd_set	input;		/* Input set for select() */


  if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
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
      perror("ERROR: unable to open print file - ");
      return (1);
    }
  }

 /*
  * Extract the hostname and port number from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

  if (port == 0)
    port = 9100;	/* Default to HP JetDirect/Tektronix PhaserShare */

 /*
  * Then try to connect to the remote host...
  */

  if ((hostaddr = gethostbyname(hostname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\' - %s",
            hostname, strerror(errno));
    return (1);
  }

  fprintf(stderr, "INFO: Attempting to connect to host %s on port %d\n",
          hostname, port);

  memset(&addr, 0, sizeof(addr));
  memcpy(&(addr.sin_addr), hostaddr->h_addr, hostaddr->h_length);
  addr.sin_family = hostaddr->h_addrtype;
  addr.sin_port   = htons(port);

  for (;;)
  {
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("ERROR: Unable to connect to printer - ");
      return (1);
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      error = errno;
      close(fd);
      fd = -1;

      if (error == ECONNREFUSED)
      {
	fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in 30 seconds...",
                hostname);
	sleep(30);
      }
      else
      {
	perror("ERROR: Unable to connect to printer - ");
        return (1);
      }
    }
    else
      break;
  }

 /*
  * Finally, send the print file...
  */

  tbytes = 0;
  while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
  {
   /*
    * Write the print data to the printer...
    */

    if (send(fd, buffer, nbytes, 0) < nbytes)
    {
      perror("ERROR: Unable to send print file to printer - ");
      break;
    }
    else
      tbytes += nbytes;

   /*
    * Check for possible data coming back from the printer...
    */

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO(&input);
    FD_SET(fd, &input);
    if (select(fd + 1, &input, NULL, NULL, &timeout) > 0)
    {
     /*
      * Grab the data coming back and spit it out to stderr...
      */

      if ((nbytes = recv(fd, buffer, sizeof(buffer), 0)) > 0)
	fprintf(stderr, "INFO: Received %u bytes of back-channel data!\n",
	        nbytes);
    }
    else
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
 * End of "$Id: socket.c,v 1.4 1999/03/21 02:09:58 mike Exp $".
 */
