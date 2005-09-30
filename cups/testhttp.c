/*
 * "$Id$"
 *
 *   HTTP test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
#include "string.h"


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j;			/* Looping vars */
  http_t	*http;			/* HTTP connection */
  struct hostent *hostaddr;		/* Host address */
  http_status_t	status;			/* Status of GET command */
  char		buffer[8192];		/* Input buffer */
  long		bytes;			/* Number of bytes read */
  FILE		*out;			/* Output file */
  char		scheme[HTTP_MAX_URI],	/* Scheme from URI */
		host[HTTP_MAX_URI],	/* Hostname from URI */
		username[HTTP_MAX_URI],	/* Username:password from URI */
		resource[HTTP_MAX_URI];	/* Resource from URI */
  int		port;			/* Port number from URI */
  off_t		length, total;		/* Length and total bytes */
  time_t	start, current;		/* Start and end time */


  if (argc == 1)
  {
   /*
    * Do API tests...
    */

    start = time(NULL);
    strcpy(buffer, httpGetDateString(start));
    current = httpGetDateTime(buffer);

    printf("httpGetDateString(%d) returned \"%s\"\n", (int)start, buffer);
    printf("httpGetDateTime(\"%s\") returned %d\n", buffer, (int)current);
    printf("httpGetDateString(%d) returned \"%s\"\n", (int)current,
           httpGetDateString(current));

    i = (int)(current - start);
    if (i < 0)
      i = -i;

    printf("Difference is %d seconds, %02d:%02d:%02d...\n", i, i / 3600,
           (i / 60) % 60, i % 60);

   /*
    * Test address functions...
    */

    printf("httpGetHostname() returned \"%s\"...\n",
           httpGetHostname(buffer, sizeof(buffer)));

    hostaddr = httpGetHostByName("localhost");
    printf("httpGetHostByName(\"localhost\") returned %p...\n", hostaddr);
    if (hostaddr)
    {
      printf("    h_name=\"%s\"\n", hostaddr->h_name);
      if (hostaddr->h_aliases)
      {
        for (i = 0; hostaddr->h_aliases[i]; i ++)
          printf("    h_aliases[%d]=\"%s\"\n", i, hostaddr->h_aliases[i]);
      }
      printf("    h_addrtype=%s\n",
             hostaddr->h_addrtype == AF_INET ? "AF_INET" :
#ifdef AF_INET6
                 hostaddr->h_addrtype == AF_INET6 ? "AF_INET6" :
#endif /* AF_INET6 */
#ifdef AF_LOCAL
                 hostaddr->h_addrtype == AF_LOCAL ? "AF_LOCAL" :
#endif /* AF_LOCAL */
                 "UNKNOWN");
      printf("    h_length=%d\n", hostaddr->h_length);
      for (i = 0; hostaddr->h_addr_list[i]; i ++)
      {
        printf("    h_addr_list[%d]=", i);
	for (j = 0; j < hostaddr->h_length; j ++)
	  printf("%02X", hostaddr->h_addr_list[i][j] & 255);
	putchar('\n');
      }
    }
  }
  else
  {
   /*
    * Test HTTP GET requests...
    */

    http = NULL;
    out = stdout;

    for (i = 1; i < argc; i ++)
    {
      if (!strcmp(argv[i], "-o"))
      {
	i ++;
	out = fopen(argv[i], "wb");
	continue;
      }

      httpSeparate(argv[i], scheme, username, host, &port, resource);

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


      start  = time(NULL);
      length = httpGetLength2(http);
      total  = 0;

      while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
      {
	total += bytes;
	fwrite(buffer, bytes, 1, out);
	if (out != stdout)
	{
          current = time(NULL);
          if (current == start) current ++;
          printf("\r" CUPS_LLFMT "/" CUPS_LLFMT " bytes ("
	         CUPS_LLFMT " bytes/sec)      ", total, length,
        	 total / (current - start));
          fflush(stdout);
	}
      }
    }

    puts("Closing connection to server...");
    httpClose(http);

    if (out != stdout)
      fclose(out);
  }

  return (0);
}


/*
 * End of "$Id$".
 */
