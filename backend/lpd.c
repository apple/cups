/*
 * "$Id: lpd.c,v 1.3 1999/03/06 20:26:05 mike Exp $"
 *
 *   Line Printer Daemon backend for the Common UNIX Printing System (CUPS).
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

extern int	rresvport(int *port);	/* Hello?  No prototype for this... */


/*
 * Local functions...
 */

static int	lpd_command(int lpd_fd, char *format, ...);
static int	lpd_queue(char *hostname, char *printer, char *filename,
		          char *user, int copies);


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
  char	method[255],	/* Method in URI */
	hostname[1024],	/* Hostname */
	username[255],	/* Username info (not used) */
	resource[1024],	/* Resource info (printer name) */
	filename[1024];	/* File to print */
  int	port;		/* Port number (not used) */
  int	status;		/* Status of LPD job */


  if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, copy stdin to a temporary file and print the temporary
  * file.
  */

  if (argc == 6)
  {
   /*
    * Copy stdin to a temporary file...
    */

    FILE *fp;		/* Temporary file */
    char buffer[8192];	/* Buffer for copying */
    int  bytes;		/* Number of bytes read */


    if ((fp = fopen(tmpnam(filename), "w")) == NULL)
    {
      perror("lpd: unable to create temporary file - ");
      return (1);
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (fwrite(buffer, 1, bytes, fp) < bytes)
      {
        perror("lpd: unable to write to temporary file - ");
	fclose(fp);
	unlink(filename);
	return (1);
      }

    fclose(fp);
  }
  else
    strcpy(filename, argv[6]);

 /*
  * Extract the hostname and printer name from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

 /*
  * Queue the job...
  */

  status = lpd_queue(hostname, resource + 1, filename,
                     argv[2] /* user */, atoi(argv[4]) /* copies */);

 /*
  * Remove the temporary file if necessary...
  */

  if (argc < 7)
    unlink(filename);

 /*
  * Return the queue status...
  */

  return (status);
}


/*
 * 'lpd_command()' - Send an LPR command sequence and wait for a reply.
 */

static int			/* O - Status of command */
lpd_command(int  fd,		/* I - Socket connection to LPD host */
            char *format,	/* I - printf()-style format string */
            ...)		/* I - Additional args as necessary */
{
  va_list	ap;		/* Argument pointer */
  char		buf[1024];	/* Output buffer */
  int		bytes;		/* Number of bytes to output */
  char		status;		/* Status from command */


 /*
  * Format the string...
  */

  va_start(ap, format);
  bytes = vsprintf(buf, format, ap);
  va_end(ap);

  fprintf(stderr, "lpd: lpd_command %02.2x %s", buf[0], buf + 1);

 /*
  * Send the command...
  */

  if (send(fd, buf, bytes, 0) < bytes)
    return (-1);

 /*
  * Read back the status from the command and return it...
  */

  if (recv(fd, &status, 1, 0) < 1)
    return (-1);

  fprintf(stderr, "lpd: lpd_command returning %d\n", status);

  return (status);
}


/*
 * 'lpd_queue()' - Queue a file using the Line Printer Daemon protocol.
 */

static int			/* O - Zero on success, non-zero on failure */
lpd_queue(char *hostname,	/* I - Host to connect to */
          char *printer,	/* I - Printer/queue name */
	  char *filename,	/* I - File to print */
          char *user,		/* I - Requesting user */
	  int  copies)		/* I - Number of copies */
{
  FILE			*fp;		/* Job file */
  char			localhost[255];	/* Local host name */
  int			error;		/* Error number */
  struct stat		filestats;	/* File statistics */
  int			port;		/* LPD connection port */
  int			fd;		/* LPD socket */
  char			control[10240],	/* LPD control 'file' */
			*cptr;		/* Pointer into control file string */
  char			status;		/* Status byte from command */
  struct sockaddr_in	addr;		/* Socket address */
  struct hostent	*hostaddr;	/* Host address */
  size_t		nbytes,		/* Number of bytes written */
			tbytes;		/* Total bytes written */
  char			buffer[8192];	/* Output buffer */


 /*
  * First try to reserve a port for this connection...
  */

  if ((hostaddr = gethostbyname(hostname)) == NULL)
  {
    fprintf(stderr, "ERROR: Unable to locate printer \'%s\' - %s",
            hostname, strerror(errno));
    return (1);
  }

  fprintf(stderr, "INFO: Attempting to connect to host %s for printer %s\n",
          hostname, printer);

  memset(&addr, 0, sizeof(addr));
  memcpy(&(addr.sin_addr), hostaddr->h_addr, hostaddr->h_length);
  addr.sin_family = hostaddr->h_addrtype;
  addr.sin_port   = htons(515);	/* LPD/printer service */

  for (port = 732;;)
  {
    if ((fd = rresvport(&port)) < 0)
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
      else if (error == EADDRINUSE)
      {
	port --;
	if (port < 721)
	  port = 732;
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
  * Next, open the print file and figure out its size...
  */

  if (stat(filename, &filestats))
  {
    perror("lpd: unable to stat print file - ");
    return (1);
  }

  if ((fp = fopen(filename, "rb")) == NULL)
  {
    perror("lpd: unable to open print file for reading - ");
    return (1);
  }

 /*
  * Send a job header to the printer, specifying no banner page and
  * literal output...
  */

  lpd_command(fd, "\002%s\n", printer);	/* Receive print job(s) */

  gethostname(localhost, sizeof(localhost));
  if (strchr(localhost, '.') != NULL)
    *strchr(localhost, '.') = '\0';

  sprintf(control, "H%s\nP%s\n", localhost, user);
  cptr = control + strlen(control);

  while (copies > 0)
  {
    sprintf(cptr, "ldfA%03.3d%s\n", getpid() % 1000, localhost);
    cptr   += strlen(cptr);
    copies --;
  }

  sprintf(cptr, "UdfA%03.3d%s\nNdfA%03.3d%s\n",
          getpid() % 1000, localhost,
          getpid() % 1000, localhost);

  fprintf(stderr, "lpd: Control file is:\n%s", control);

  lpd_command(fd, "\002%d cfA%03.3d%s\n", strlen(control), getpid() % 1000,
              localhost);

  fprintf(stderr, "lpd: Sending control file (%d bytes)\n", strlen(control));

  if (send(fd, control, strlen(control) + 1, 0) < (strlen(control) + 1))
  {
    perror("ERROR: Unable to write control file - ");
    status = 1;
  }
  else if (read(fd, &status, 1) < 1 || status != 0)
    fprintf(stderr, "ERROR: Remote host did not accept control file (%d)\n",
            status);
  else
  {
   /*
    * Send the print file...
    */

    fputs("lpd: Control file sent successfully\n", stderr);

    lpd_command(fd, "\003%d dfA%03.3d%s\n", filestats.st_size,
                getpid() % 1000, localhost);

    fprintf(stderr, "lpd: Sending data file (%u bytes)\n", filestats.st_size);

    tbytes = 0;
    while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
      fprintf(stderr, "INFO: Spooling LPR job, %u%% complete...\n",
              100 * tbytes / filestats.st_size);

      if (send(fd, buffer, nbytes, 0) < nbytes)
      {
        perror("ERROR: Unable to send print file to printer - ");
        break;
      }
      else
        tbytes += nbytes;
    }

    send(fd, "", 1, 0);

    if (tbytes < filestats.st_size)
      status = 1;
    else if (recv(fd, &status, 1, 0) < 1 || status != 0)
      fprintf(stderr, "ERROR: Remote host did not accept data file (%d)\n",
              status);
    else
      fputs("lpd: Data file sent successfully\n", stderr);
  }

 /*
  * Close the socket connection and input file and return...
  */

  close(fd);
  fclose(fp);
  
  return (status);
}


/*
 * End of "$Id: lpd.c,v 1.3 1999/03/06 20:26:05 mike Exp $".
 */
