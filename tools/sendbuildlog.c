/*
 * "$Id: sendbuildlog.c 12379 2014-12-18 01:32:28Z msweet $"
 *
 *   Short program to send the build log via email or HTTP POST.
 *
 *   Copyright 2007-2014 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Usage:
 *
 *    smtpmail [options] to@host.com build.log
 *
 * Options:
 *
 *    -a attach-filename
 *    -b build-status
 *    -f "from@host.com"
 *    -h server.domain.com
 *    -s "subject"
 *    -v
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <poll.h>


/*
 * Prototypes...
 */

char	*sock_gets(int fd, char *s, int slen);
int	sock_open(const char *hostname, int port);
int	sock_puts(int fd, int verbose, ...);
int	sock_status(int fd, int verbose);
void	usage(void);


/*
 * 'main()' - Collect arguments and send a message.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*opt;			/* Option character */
  int		verbose;		/* Verbose messages? */
  int		net,			/* Network connection */
		port;			/* Port to use */
  char		local[1024],		/* Local hostname */
		data[4096];		/* Message data */
  int		status;			/* Command status code */
  int		build_status;		/* Build status code */
  const char	*server,		/* Server hostname */
		*from,			/* "From" address */
		*to,			/* "To" address */
		*buildlog;		/* Build log to send */
  FILE		*buildfp;		/* Build log file */
  const char	*subject;		/* Subject of message */
  struct utsname unamebuf;		/* Buffer for uname info */
  char 		http_server[1024],	/* Server in URL */
		*http_ptr;		/* Pointer into URL */
  const char	*lf;			/* Line feed to use */
  int		num_attachments = 0;	/* Number of attachments */
  const char	*attachments[10],	/* Attachments */
		*attachname;		/* Base name of attachment */
  struct stat	attachinfo;		/* Info about attachment */
  size_t	total;			/* Length */


 /*
  * Initialize things...
  */

  gethostname(local, sizeof(local));
  uname(&unamebuf);

  server       = "relay.apple.com";
  subject      = "No Subject";
  from         = "noreply@cups.org";
  to           = NULL;
  buildlog     = NULL;
  verbose      = 0;
  build_status = 0;

 /*
  * Loop through the command-line...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
    {
      for (opt = argv[i] + 1; *opt; opt ++)
        switch (*opt)
	{
	  case 'a' :
	      i ++;
	      if (i >= argc)
	        usage();

              if (num_attachments >= (int)(sizeof(attachments) / sizeof(attachments[0])))
              {
                fputs("sendbuildlog: Too many attachments.\n", stderr);
                return (1);
              }

              if (access(argv[i], R_OK))
              {
                perror(argv[i]);
                return (1);
              }

              attachments[num_attachments ++] = argv[i];
              break;

	  case 'b' :
	      i ++;
	      if (i >= argc)
	        usage();

	      build_status = atoi(argv[i]);
	      break;

	  case 'f' :
	      i ++;
	      if (i >= argc)
	        usage();

              from = argv[i];
	      break;

	  case 'h' :
	      i ++;
	      if (i >= argc)
	        usage();

	      server = argv[i];
	      break;

	  case 's' :
	      i ++;
	      if (i >= argc)
	        usage();

	      subject = argv[i];
	      break;

          case 'v' :
	      verbose ++;
	      break;

	  default :
	      usage();
	}
    }
    else if (to == NULL)
      to = argv[i];
    else if (buildlog == NULL)
      buildlog = argv[i];
    else
      usage();

  if (to == NULL || buildlog == NULL)
    usage();

  buildfp = fopen(buildlog, "rb");

 /*
  * Connect to the server and send the build log...
  */

  if (!strncmp(to, "http://", 7))
  {
   /*
    * Send build log via HTTP POST...
    */

    strncpy(http_server, to + 7, sizeof(http_server) - 1);
    http_server[sizeof(http_server) - 1] = '\0';
    server = http_server;

    if ((http_ptr = strchr(http_server, '/')) != NULL)
      *http_ptr = '\0';

    to += strlen(http_server) + 7;

    if ((http_ptr = strrchr(http_server, ':')) != NULL)
    {
      *http_ptr++ = '\0';
      port = atoi(http_ptr);
    }
    else
      port = 80;
  }
  else
  {
   /*
    * Send build log via SMTP mail...
    */

    port = 25;
  }

  if ((net = sock_open(server, port)) < 0)
  {
    perror(server);
    return (1);
  }

  if (port == 25)
  {
   /*
    * Send SMTP header...
    */

    sock_puts(net, verbose, "HELO ", local, "\r\n", NULL);
    status = sock_status(net, verbose);
    if (!status || status >= 500)
      goto net_error;

    sock_puts(net, verbose, "MAIL FROM:", from, "\r\n", NULL);
    status = sock_status(net, verbose);
    if (!status || status >= 500)
      goto net_error;

    sock_puts(net, verbose, "RCPT TO:", to, "\r\n", NULL);
    status = sock_status(net, verbose);
    if (!status || status >= 500)
      goto net_error;

    sock_puts(net, verbose, "DATA\r\n", NULL);
    status = sock_status(net, verbose);
    if (!status || status >= 500)
      goto net_error;

    snprintf(data, sizeof(data),
	     "Subject: %s\r\n"
	     "To: %s\r\n"
	     "X-CUPS-System: %s\r\n"
	     "X-CUPS-BuildStatus: %d\r\n"
	     "X-CUPS-UName: %s %s %s\r\n"
	     "Mime-Version: 1.0\r\n"
	     "Content-Type: multipart/mixed; boundary=\"PART-BOUNDARY\"\r\n"
	     "Content-Transfer-Encoding: 8bit\r\n"
	     "\r\n"
	     "--PART-BOUNDARY\r\n"
	     "Content-Type: text/plain\r\n"
	     "\r\n",
	     subject, to, local, build_status, unamebuf.sysname,
	     unamebuf.release, unamebuf.machine);

    lf = "\r\n";
  }
  else
  {
   /*
    * Send HTTP POST header...
    */

    struct stat	buildstat;		/* Build log file info */
    char	length[255];		/* Content-Length value */

    if (stat(buildlog, &buildstat))
      goto net_error;

    for (total = buildstat.st_size, i = 0; i < num_attachments; i ++)
    {
      stat(attachments[i], &attachinfo);

      if ((attachname = strrchr(attachments[i], '/')) != NULL)
	attachname ++;
      else
	attachname = attachments[i];

      snprintf(data, sizeof(data), "\nATTACHMENT %ld %s\n", (long)attachinfo.st_size, attachname);
      total += strlen(data) + attachinfo.st_size;
    }

    snprintf(data, sizeof(data),
	     "SYSTEM=%s\n"
	     "STATUS=%d\n"
	     "UNAME=%s %s %s\n\n",
	     subject, build_status, unamebuf.sysname,
	     unamebuf.release, unamebuf.machine);
    total += strlen(data);
    snprintf(length, sizeof(length), "%ld", (long)total);

    sock_puts(net, verbose, "POST ", to, " HTTP/1.1\r\n", NULL);
    sock_puts(net, verbose, "Host: ", server, "\r\n", NULL);
    sock_puts(net, verbose, "Content-Type: application/vnd.cups-buildlog\r\n", NULL);
    sock_puts(net, verbose, "Content-Length: ", length, "\r\n", NULL);
    sock_puts(net, verbose, "\r\n", NULL);

    lf = NULL;
  }

  total = 0;

  do
  {
    if (lf && data[strlen(data) - 1] == '\n')
      data[strlen(data) - 1] = '\0';

    sock_puts(net, port != 25 && !total, data, lf, NULL);

    if (port != 25)
      total += strlen(data);
  }
  while (fgets(data, sizeof(data), buildfp));

  fclose(buildfp);

  for (i = 0; i < num_attachments; i ++)
  {
    if ((attachname = strrchr(attachments[i], '/')) != NULL)
      attachname ++;
    else
      attachname = attachments[i];

    if ((buildfp = fopen(attachments[i], "r")) == NULL)
      continue;

    if (port == 25)
    {
     /*
      * Do MIME attachment...
      */

      const char *type = strstr(attachname, ".html") != NULL ? "text/html" : "text/plain";

      snprintf(data, sizeof(data),
               "--PART-BOUNDARY\r\n"
               "Content-Type: %s\r\n"
               "Content-Disposition: attachment; filename=\"%s\"\r\n"
               "\r\n", type, attachname);
      sock_puts(net, 0, data, NULL);
    }
    else
    {
     /*
      * Do buildlog attachment...
      */

      fstat(fileno(buildfp), &attachinfo);
      snprintf(data, sizeof(data), "\nATTACHMENT %ld %s\n", (long)attachinfo.st_size, attachname);
      sock_puts(net, 1, data, NULL);
      total += strlen(data);
    }

    while (fgets(data, sizeof(data), buildfp))
    {
      sock_puts(net, 0, data, NULL);
      total += strlen(data);
    }

    fclose(buildfp);
  }

  if (port == 25)
  {
   /*
    * Finish SMTP request...
    */

    sock_puts(net, 0, "--PART-BOUNDARY--\r\n", NULL);

    sock_puts(net, verbose, ".\r\n", NULL);
    status = sock_status(net, verbose);
    if (!status || status >= 500)
      goto net_error;

    sock_puts(net, verbose, "QUIT\r\n", NULL);
    status = sock_status(net, verbose);
    if (!status || status >= 500)
      goto net_error;
  }
  else
  {
   /*
    * Finish HTTP request...
    */

    printf("Wrote %ld bytes...\n", total);

    status = sock_status(net, verbose);
    do
    {
      if (!sock_gets(net, data, sizeof(data)))
        break;

      if (verbose)
        fputs(data, stdout);
    }
    while (strcmp(data, "\n") && strcmp(data, "\r\n"));

    if (sock_gets(net, data, sizeof(data)) && verbose)
      fputs(data, stdout);

    if (!status || status != 200)
      goto net_error;
  }

  close(net);

  return (0);

 /*
  * When we get here, there was a hard error...
  */

  net_error:

  close(net);
  fclose(buildfp);

  return (1);
}


/*
 * 'sock_gets()' - Get a line from the socket...
 */

char *					/* O - String pointer or NULL on error */
sock_gets(int  fd,			/* I - Socket */
          char *s,			/* I - String buffer */
          int  slen)			/* I - Size of string buffer */
{
  int		bytes;			/* Bytes read */
  char		*bufptr;		/* Pointer into buffer */
  static int	bufused = 0;		/* Bytes used in buffer */
  static char	buffer[10240];		/* Line buffer */


  while (bufused == 0 || (bufptr = strchr(buffer, '\n')) == NULL)
  {
    bytes = sizeof(buffer) - 1 - bufused;

    if (bytes == 0)
    {
      if (bufused > (slen - 1))
        bytes = slen - 1;
      else
        bytes = bufused;

      strncpy(s, buffer, bytes);
      s[bytes] = '\0';

      if (bytes < bufused)
        memcpy(buffer, buffer + bytes, bufused - bytes);

      bufused -= bytes;

      return (s);
    }

    if ((bytes = read(fd, buffer + bufused, bytes)) <= 0)
      return (NULL);

    bufused += bytes;
    buffer[bufused] = '\0';
  }

  bufptr ++;
  bytes = bufptr - buffer;
  if (bytes > (slen - 1))
    bytes = slen - 1;

  strncpy(s, buffer, bytes);
  s[bytes] = '\0';

  if (bytes < bufused)
    memcpy(buffer, buffer + bytes, bufused - bytes);

  bufused -= bytes;

  return (s);
}


/*
 * 'sock_open()' - Open a TCP/IP socket and connect to the named host.
 */

int					/* O - FD or -1 on error */
sock_open(const char *hostname,		/* I - Hostname */
          int        port)		/* I - Port number */
{
  int		fd;			/* New socket */
  struct hostent *hostaddr;		/* Host address data */
  struct sockaddr_in addr;		/* Socket address */


  if ((hostaddr = gethostbyname(hostname)) == NULL)
    return (-1);

  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);

  memcpy(&(addr.sin_addr), hostaddr->h_addr, sizeof(addr.sin_addr));

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return (-1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    close(fd);
    return (-1);
  }

  return (fd);
}


/*
 * 'sock_puts()' - Put one or more strings to the socket.
 */

int					/* O - Number of bytes written or -1 on error */
sock_puts(int fd,			/* I - Socket to write to */
          int verbose,			/* I - Verbose logging */
          ...)				/* I - One or more strings, NULL terminated */
{
  int		total,			/* Total bytes written */
		bytes,			/* Number of bytes written */
		len;			/* Length of string */
  va_list	ap;			/* Argument pointer */
  const char	*s;			/* String to print */


  total = 0;
  va_start(ap, verbose);

  while ((s = va_arg(ap, const char *)) != NULL)
  {
    if (verbose)
      fputs(s, stdout);

    len = strlen(s);

    while (len > 0)
    {
      if ((bytes = write(fd, s, len)) < 0)
        return (-1);
      else
      {
        total += bytes;
        len   -= bytes;
        s     += bytes;
      }
    }
  }

  return (total);
}


/*
 * 'sock_status()' - Wait for status from the socket.
 */

int					/* O - Status code */
sock_status(int fd,			/* I - Socket */
            int verbose)		/* I - Verbose output? */
{
  struct pollfd	polldata;		/* poll() data */
  int		timeout;		/* Timeout to use */
  char		response[1024];		/* Response from server */
  int		status;			/* Status code */


  timeout = 30000;
  status  = 0;

  do
  {
    polldata.fd     = fd;
    polldata.events = POLLIN;

    if (poll(&polldata, 1, timeout) > 0)
    {
      if (!sock_gets(fd, response, sizeof(response)))
	return (status);

      if (verbose)
	fputs(response, stdout);

      if (!strncmp(response, "HTTP/", 5))
      {
        status = atoi(strchr(response, ' '));
	break;
      }
      else
        status = atoi(response);
    }
    else
      break;

    timeout = 1000;
  }
  while (status < 500);

  return (status);
}


/*
 * 'usage()' - Show program usage...
 */

void
usage(void)
{
  puts("Usage:");
  puts("");
  puts("    smtpmail [options] to@host.com build.log");
  puts("");
  puts(" Options:");
  puts("");
  puts("    -b build-status");
  puts("    -f \"from@host.com\"");
  puts("    -h server.domain.com");
  puts("    -s \"subject\"");
  puts("    -v");
  exit(1);
}


/*
 * End of "$Id: sendbuildlog.c 12379 2014-12-18 01:32:28Z msweet $".
 */
