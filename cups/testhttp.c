/*
 * "$Id: testhttp.c,v 1.11.2.8 2004/06/29 13:15:09 mike Exp $"
 *
 *   HTTP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2004 by Easy Software Products.
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
 *   main() - Main entry.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
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
  char		buffer[8192];	/* Input buffer */
  long		bytes;		/* Number of bytes read */
  FILE		*out;		/* Output file */
  char		host[HTTP_MAX_URI],
		method[HTTP_MAX_URI],
		username[HTTP_MAX_URI],
		resource[HTTP_MAX_URI];
  int		port;
  long		length, total;
  time_t	start, current;



  http = NULL;
  out = stdout;

  for (i = 1; i < argc; i ++)
  {
    if (strcmp(argv[i], "-o") == 0)
    {
      i ++;
      out = fopen(argv[i], "wb");
      continue;
    }

    httpSeparate(argv[i], method, username, host, &port, resource);

    http = httpConnect(host, port);
    if (http == NULL)
    {
      perror(host);
      continue;
    }
    printf("Requesting file \"%s\"...\n", resource);
    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
    httpGet(http, resource);
    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status == HTTP_OK)
      puts("GET OK:");
    else
      printf("GET failed with status %d...\n", status);


    start = time(NULL);
    length = atoi(httpGetField(http, HTTP_FIELD_CONTENT_LENGTH));
    total  = 0;

    while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
    {
      total += bytes;
      fwrite(buffer, bytes, 1, out);
      if (out != stdout)
      {
        current = time(NULL);
        if (current == start) current ++;
        printf("\r%ld/%ld bytes (%ld bytes/sec)      ", total, length,
               total / (current - start));
        fflush(stdout);
      }
    }
  }

  puts("Closing connection to server...");
  httpClose(http);

  if (out != stdout)
    fclose(out);

  return (0);
}


/*
 * End of "$Id: testhttp.c,v 1.11.2.8 2004/06/29 13:15:09 mike Exp $".
 */
