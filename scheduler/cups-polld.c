/*
 * "$Id$"
 *
 *   Polling daemon for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()        - Open sockets and poll until we are killed...
 *   poll_server() - Poll the server for the given set of printers or classes.
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


/*
 * Local functions...
 */

int	poll_server(http_t *http, cups_lang_t *language, ipp_op_t op,
	            int sock, int port, int interval, const char *prefix);


/*
 * 'main()' - Open sockets and poll until we are killed...
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t		*http;		/* HTTP connection */
  cups_lang_t		*language;	/* Language info */
  int			interval;	/* Polling interval */
  int			sock;		/* Browser sock */
  int			port;		/* Browser port */
  int			val;		/* Socket option value */
  int			seconds,	/* Seconds left from poll */
			remain;		/* Total remaining time to sleep */
  char			prefix[1024];	/* Prefix for log messages */


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
  * Open a connection to the server...
  */

  while ((http = httpConnectEncrypt(argv[1], atoi(argv[2]),
                                    cupsEncryption())) == NULL)
  {
    fprintf(stderr, "ERROR: %s Unable to connect to %s on port %s: %s\n",
            prefix, argv[1], argv[2],
	    h_errno ? hstrerror(h_errno) : strerror(errno));
    sleep (interval);
  }

 /*
  * Loop forever, asking for available printers and classes...
  */

  language = cupsLangDefault();

  for (;;)
  {
   /*
    * Get the printers, then the classes...
    */

    remain = interval;

    if ((seconds = poll_server(http, language, CUPS_GET_PRINTERS, sock, port,
                               interval, prefix)) > 0)
      remain -= seconds;

   /*
    * Sleep for any remaining time...
    */

    if (remain > 0) 
      sleep(remain);
  }
}


/*
 * 'poll_server()' - Poll the server for the given set of printers or classes.
 */

int					/* O - Number of seconds or -1 on error */
poll_server(http_t      *http,		/* I - HTTP connection */
            cups_lang_t *language,	/* I - Language */
	    ipp_op_t    op,		/* I - Operation code */
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
  const char		*uri,		/* printer-uri */
			*info,		/* printer-info */
			*location,	/* printer-location */
			*make_model;	/* printer-make-and-model */
  cups_ptype_t		type;		/* printer-type */
  ipp_pstate_t		state;		/* printer-state */
  int			accepting;	/* printer-is-accepting-jobs */
  struct sockaddr_in	addr;		/* Broadcast address */
  char			packet[1540];	/* Data packet */
  static const char * const attrs[] =	/* Requested attributes */
			{
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
  * Build a CUPS_GET_PRINTERS or CUPS_GET_CLASSES request, which requires
  * only the attributes-charset and attributes-natural-language attributes.
  */

  request = ippNew();

  request->request.op.operation_id = op;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", sizeof(attrs) / sizeof(attrs[0]),
	       NULL, attrs);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type", CUPS_PRINTER_SHARED);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type-mask",
		CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT |
		CUPS_PRINTER_SHARED);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      fprintf(stderr, "ERROR: %s get-%s failed: %s\n", prefix,
              op == CUPS_GET_PRINTERS ? "printers" : "classes",
              ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return (-1);
    }

   /*
    * Figure out how many printers/classes we have...
    */

    for (attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME),
             max_count = 0;
	 attr != NULL;
	 attr = ippFindNextAttribute(response, "printer-name", IPP_TAG_NAME),
	     max_count ++);

    fprintf(stderr, "DEBUG: %s found %d %s.\n", prefix, max_count,
            op == CUPS_GET_PRINTERS ? "printers" : "classes");

    count     = 0;
    seconds   = time(NULL);
    max_count = max_count / interval + 1;

   /*
    * Loop through the printers or classes returned in the list...
    */

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a printer...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this printer...
      */

      uri        = NULL;
      info       = "";
      location   = "";
      make_model = "";
      type       = CUPS_PRINTER_REMOTE;
      accepting  = 1;
      state      = IPP_PRINTER_IDLE;

      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
      {
        if (strcmp(attr->name, "printer-uri-supported") == 0 &&
	    attr->value_tag == IPP_TAG_URI)
	  uri = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-info") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  info = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-is-accepting-jobs") == 0 &&
	    attr->value_tag == IPP_TAG_BOOLEAN)
	  accepting = attr->values[0].boolean;

        if (strcmp(attr->name, "printer-location") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  location = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-make-and-model") == 0 &&
	    attr->value_tag == IPP_TAG_TEXT)
	  make_model = attr->values[0].string.text;

        if (strcmp(attr->name, "printer-state") == 0 &&
	    attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_pstate_t)attr->values[0].integer;

        if (strcmp(attr->name, "printer-type") == 0 &&
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

      snprintf(packet, sizeof(packet), "%x %x %s \"%s\" \"%s\" \"%s\"\n",
               type, state, uri, location, info, make_model);

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

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
  {
    fprintf(stderr, "ERROR: %s get-%s failed: %s\n", prefix,
            op == CUPS_GET_PRINTERS ? "printers" : "classes",
            ippErrorString(cupsLastError()));
    return (-1);
  }

 /*
  * Return the number of seconds we used...
  */

  return (time(NULL) - seconds);
}


/*
 * End of "$Id$".
 */
