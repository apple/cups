/*
 * "$Id: testhttp.c,v 1.2 1999/01/29 22:01:49 mike Exp $"
 *
 *   HTTP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <config.h>
#include "http.h"


/*
 * 'main()' - Main entry.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* HTTP connection */
  http_status_t	status;		/* Status of GET command */
  char		buffer[1024];	/* Input buffer */
  int		bytes;		/* Number of bytes read */


  puts("Connecting to dns.easysw.com...");

  httpInitialize();
  http = httpConnect("dns.easysw.com", 80);
  if (http == NULL)
  {
    puts("Unable to connect to dns.easysw.com!");
    return (1);
  }

  puts("Connected to dns.easysw.com...");

  for (i = 1; i < argc; i ++)
  {
    printf("Requesting file \"%s\"...\n", argv[i]);
    httpClearFields(http);
    httpGet(http, argv[i]);
    status = httpUpdate(http);

    if (status == HTTP_OK)
      puts("GET OK:");
    else
      printf("GET failed with status %d...\n", status);

    while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
      fwrite(buffer, bytes, 1, stdout);
  }

  puts("Closing connection to server...");
  httpClose(http);

  return (0);
}


/*
 * End of "$Id: testhttp.c,v 1.2 1999/01/29 22:01:49 mike Exp $".
 */
