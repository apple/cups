/*
 * "$Id$"
 *
 *   Polling daemon for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()           - Open sockets and poll until we are killed...
 *   dequote()        - Remote quotes from a string.
 *   poll_server()    - Poll the server for the given set of printers or
 *                      classes.
 *   sighup_handler() - Handle 'hangup' signals to restart polling.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include <cups/cups.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/language.h>
#include <cups/string.h>
#include <signal.h>


/*
 * Local globals...
 */

static int	restart_polling = 1;


/*
 * Local functions...
 */

static char	*dequote(char *d, const char *s, int dlen);
static int	poll_server(http_t *http, int sock, int port, int interval,
			    const char *prefix);
static void	sighup_handler(int sig);


/*
 * 'main()' - Open sockets and poll until we are killed...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* HTTP connection */
  int		interval;		/* Polling interval */
  int		sock;			/* Browser sock */
  int		port;			/* Browser port */
  int		val;			/* Socket option value */
  int		seconds,		/* Seconds left from poll */
		remain;			/* Total remaining time to sleep */
  char		prefix[1024];		/* Prefix for log messages */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Catch hangup signals for when the network changes...
  */

#ifdef HAVE_SIGSET /* Use System V signals over POSIX to avoid bugs */
  sigset(SIGHUP, sighup_handler);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGHUP);
  action.sa_handler = sighup_handler;
  sigaction(SIGHUP, &action, NULL);
#else
  signal(SIGHUP, sighup_handler);
#endif /* HAVE_SIGSET */

 /*
  * Don't buffer log messages...
  */

  setbuf(stderr, NULL);

 /*
  * The command-line must contain the following:
  *
  *    cups-polld server server-port interval port
  */

  if (argc != 5)
  {
    fputs("Usage: cups-polld server server-port interval port\n", stderr);
    return (1);
  }

  interval = atoi(argv[3]);
  port     = atoi(argv[4]);

  if (interval < 2)
    interval = 2;

  snprintf(prefix, sizeof(prefix), "[cups-polld %s:%d]", argv[1], atoi(argv[2]));

 /*
  * Open a broadcast socket...
  */

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    fprintf(stderr, "ERROR: %s Unable to open broadcast socket: %s\n", prefix,
            strerror(errno));
    return (1);
  }

 /*
  * Set the "broadcast" flag...
  */

  val = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)))
  {
    fprintf(stderr, "ERROR: %s Unable to put socket in broadcast mode: %s\n",
            prefix, strerror(errno));

    close(sock);
    return (1);
  }

 /*
  * Loop forever, asking for available printers and classes...
  */

  for (http = NULL; !ferror(stderr);)
  {
   /*
    * Open a connection to the server...
    */

    if (restart_polling || !http)
    {
      restart_polling = 0;
      httpClose(http);

      if ((http = httpConnectEncrypt(argv[1], atoi(argv[2]),
                                     cupsEncryption())) == NULL)
      {
	fprintf(stderr, "ERROR: %s Unable to connect to %s on port %s: %s\n",
        	prefix, argv[1], argv[2],
		h_errno ? hstrerror(h_errno) : strerror(errno));
      }
    }

   /*
    * Get the printers and classes...
    */

    remain = interval;

    if (http && (seconds = poll_server(http, sock, port, interval, prefix)) > 0)
      remain -= seconds;

   /*
    * Sleep for any remaining time...
    */

    if (remain > 0 && !restart_polling)
      sleep(remain);
  }

  return (1);
}


/*
 * 'dequote()' - Remote quotes from a string.
 */

static char *				/* O - Dequoted string */
dequote(char       *d,			/* I - Destination string */
        const char *s,			/* I - Source string */
	int        dlen)		/* I - Destination length */
{
  char	*dptr;				/* Pointer into destination */


  if (s)
  {
    for (dptr = d, dlen --; *s && dlen > 0; s ++)
      if (*s != '\"')
      {
	*dptr++ = *s;
	dlen --;
      }

    *dptr = '\0';
  }
  else
    *d = '\0';

  return (d);
}


/*
 * 'poll_server()' - Poll the server for the given set of printers or classes.
 */

static int				/* O - Number of seconds or -1 on error */
poll_server(http_t      *http,		/* I - HTTP connection */
	    int         sock,		/* I - Broadcast sock */
	    int         port,		/* I - Broadcast port */
	    int         interval,	/* I - Polling interval */
	    const char	*prefix)	/* I - Prefix for log messages */
{
  int			seconds;	/* Number of seconds */
  int			count,		/* Current number of printers/classes */
			max_count;	/* Maximum printers/classes per second */
  ipp_t			*request,	/* Request data */
			*response;	/* Response data */
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*uri;		/* printer-uri */
  char			info[1024],	/* printer-info */
			job_sheets[1024],/* job-sheets-default */
			location[1024],	/* printer-location */
			make_model[1024];
					/* printer-make-and-model */
  cups_ptype_t		type;		/* printer-type */
  ipp_pstate_t		state;		/* printer-state */
  int			accepting;	/* printer-is-accepting-jobs */
  struct sockaddr_in	addr;		/* Broadcast address */
  char			packet[1540];	/* Data packet */
  static const char * const attrs[] =	/* Requested attributes */
			{
			  "job-sheets-default",
			  "printer-info",
			  "printer-is-accepting-jobs",
			  "printer-location",
			  "printer-make-and-model",
			  "printer-name",
			  "printer-state",
			  "printer-type",
			  "printer-uri-supported"
			};


 /*
  * Broadcast to 127.0.0.1 (localhost)
  */

  memset(&addr, 0, sizeof(addr));
  addr.sin_addr.s_addr = htonl(0x7f000001);
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port);

 /*
  * Build a CUPS_GET_PRINTERS request and pass along a list of the
  * attributes we are interested in along with the types of printers
  * (and classes) we want.
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", sizeof(attrs) / sizeof(attrs[0]),
	       NULL, attrs);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type", 0);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type-mask",
		CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT |
		CUPS_PRINTER_NOT_SHARED);

 /*
  * Do the request and get back a response...
  */

  seconds  = time(NULL);
  response = cupsDoRequest(http, request, "/");

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    fprintf(stderr, "ERROR: %s CUPS-Get-Printers failed: %s\n", prefix,
            cupsLastErrorString());
    ippDelete(response);
    return (-1);
  }

  if (response)
  {
   /*
    * Figure out how many printers/classes we have...
    */

    for (attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME),
             max_count = 0;
	 attr != NULL;
	 attr = ippFindNextAttribute(response, "printer-name", IPP_TAG_NAME),
	     max_count ++);

    fprintf(stderr, "DEBUG: %s Found %d printers.\n", prefix, max_count);

    count     = 0;
    max_count = max_count / interval + 1;

   /*
    * Loop through the printers or classes returned in the list...
    */

    for (attr = response->attrs; attr; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a printer...
      */

      while (attr && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (!attr)
        break;

     /*
      * Pull the needed attributes from this printer...
      */

      uri           = NULL;
      info[0]       = '\0';
      job_sheets[0] = '\0';
      location[0]   = '\0';
      make_model[0] = '\0';
      type          = CUPS_PRINTER_REMOTE;
      accepting     = 1;
      state         = IPP_PRINTER_IDLE;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (!strcmp(attr->name, "job-sheets-default") &&
	    (attr->value_tag == IPP_TAG_NAME ||
	     attr->value_tag == IPP_TAG_KEYWORD))
	{
	  if (attr->num_values == 1)
	    snprintf(job_sheets, sizeof(job_sheets), " job-sheets=%s",
	             attr->values[0].string.text);
          else
	    snprintf(job_sheets, sizeof(job_sheets), " job-sheets=%s,%s",
	             attr->values[0].string.text,
	             attr->values[1].string.text);
	}
        else if (!strcmp(attr->name, "printer-uri-supported") &&
	         attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;
        else if (!strcmp(attr->name, "printer-info") &&
		 attr->value_tag == IPP_TAG_TEXT)
	  dequote(info, attr->values[0].string.text, sizeof(info));
        else if (!strcmp(attr->name, "printer-is-accepting-jobs") &&
	         attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;
        else if (!strcmp(attr->name, "printer-location") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  dequote(location, attr->values[0].string.text, sizeof(location));
        else if (!strcmp(attr->name, "printer-make-and-model") &&
	         attr->value_tag == IPP_TAG_TEXT)
	  dequote(make_model, attr->values[0].string.text, sizeof(location));
        else if (!strcmp(attr->name, "printer-state") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_pstate_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "printer-type") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  type = (cups_ptype_t)attr->values[0].integer;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (uri == NULL)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

     /*
      * Send the printer information...
      */

      type |= CUPS_PRINTER_REMOTE;

      if (!accepting)
	type |= CUPS_PRINTER_REJECTING;

      snprintf(packet, sizeof(packet),
               "%x %x %s \"%s\" \"%s\" \"%s\" lease-duration=%d%s\n",
               type, state, uri, location, info, make_model, interval * 2,
	       job_sheets);

      fprintf(stderr, "DEBUG2: %s Sending %s", prefix, packet);

      if (sendto(sock, packet, strlen(packet), 0,
	         (struct sockaddr *)&addr, sizeof(addr)) <= 0)
      {
	ippDelete(response);
	perror("cups-polld");
	return (-1);
      }

     /*
      * Throttle the local broadcasts as needed so that we don't
      * overwhelm the local server...
      */

      count ++;
      if (count >= max_count)
      {
       /*
	* Sleep for a second...
	*/

	count = 0;

	sleep(1);
      }

      if (!attr || restart_polling)
        break;
    }

    ippDelete(response);
  }

 /*
  * Return the number of seconds we used...
  */

  return (time(NULL) - seconds);
}


/*
 * 'sighup_handler()' - Handle 'hangup' signals to restart polling.
 */

static void
sighup_handler(int sig)			/* I - Signal number */
{
  (void)sig;

  restart_polling = 1;

#if !defined(HAVE_SIGSET) && !defined(HAVE_SIGACTION)
  signal(SIGHUP, sighup_handler);
#endif /* !HAVE_SIGSET && !HAVE_SIGACTION */
}


/*
 * End of "$Id$".
 */
