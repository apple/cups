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
 * Types and structures...
 */

typedef struct uri_test_s		/**** URI test cases ****/
{
  http_uri_status_t	result;		/* Expected return value */
  const char		*uri,		/* URI */
			*scheme,	/* Scheme string */
			*username,	/* Username:password string */
			*hostname,	/* Hostname string */
			*resource;	/* Resource string */
  int			port;		/* Port number */
} uri_test_t;


/*
 * Local globals...
 */

static uri_test_t	uri_tests[] =	/* URI test data */
			{
			  /* Start with valid URIs */
			  { HTTP_URI_OK, "file:/filename",
			    "file", "", "", "/filename", 0 },
			  { HTTP_URI_OK, "file:/filename%20with%20spaces",
			    "file", "", "", "/filename with spaces", 0 },
			  { HTTP_URI_OK, "file:///filename",
			    "file", "", "", "/filename", 0 },
			  { HTTP_URI_OK, "file:///filename%20with%20spaces",
			    "file", "", "", "/filename with spaces", 0 },
			  { HTTP_URI_OK, "file://localhost/filename",
			    "file", "", "localhost", "/filename", 0 },
			  { HTTP_URI_OK, "file://localhost/filename%20with%20spaces",
			    "file", "", "localhost", "/filename with spaces", 0 },
			  { HTTP_URI_OK, "http://server/",
			    "http", "", "server", "/", 80 },
			  { HTTP_URI_OK, "http://username@server/",
			    "http", "username", "server", "/", 80 },
			  { HTTP_URI_OK, "http://username:passwor%64@server/",
			    "http", "username:password", "server", "/", 80 },
			  { HTTP_URI_OK, "http://username:passwor%64@server:8080/",
			    "http", "username:password", "server", "/", 8080 },
			  { HTTP_URI_OK, "http://username:passwor%64@server:8080/directory/filename",
			    "http", "username:password", "server", "/directory/filename", 8080 },
			  { HTTP_URI_OK, "https://username:passwor%64@server/directory/filename",
			    "https", "username:password", "server", "/directory/filename", 443 },
			  { HTTP_URI_OK, "ipp://username:passwor%64@[::1]/ipp",
			    "ipp", "username:password", "[::1]", "/ipp", 631 },
			  { HTTP_URI_OK, "lpd://server/queue?reserve=yes",
			    "lpd", "", "server", "/queue?reserve=yes", 515 },
			  { HTTP_URI_OK, "mailto:user@domain.com",
			    "mailto", "", "", "user@domain.com", 0 },
			  { HTTP_URI_OK, "socket://server/",
			    "socket", "", "server", "/", 9100 },

			  /* Missing scheme */
			  { HTTP_URI_MISSING_SCHEME, "/path/to/file/index.html",
			    "file", "", "", "/path/to/file/index.html", 0 },
			  { HTTP_URI_MISSING_SCHEME, "//server/ipp",
			    "ipp", "", "server", "/ipp", 631 },

			  /* Unknown scheme */
			  { HTTP_URI_UNKNOWN_SCHEME, "vendor://server/resource",
			    "vendor", "", "server", "/resource", 0 },

			  /* Missing resource */
			  { HTTP_URI_MISSING_RESOURCE, "socket://[::192.168.2.1]",
			    "socket", "", "[::192.168.2.1]", "/", 9100 },

			  /* Bad URI */
			  { HTTP_URI_BAD_URI, "",
			    "", "", "", "", 0 },

			  /* Bad scheme */
			  { HTTP_URI_BAD_SCHEME, "bad_scheme://server/resource",
			    "", "", "", "", 0 },

			  /* Bad username */
			  { HTTP_URI_BAD_USERNAME, "http://username:passwor%6@server/resource",
			    "http", "", "", "", 80 },

			  /* Bad hostname */
			  { HTTP_URI_BAD_HOSTNAME, "http://[/::1]/index.html",
			    "http", "", "", "", 80 },
			  { HTTP_URI_BAD_HOSTNAME, "http://[",
			    "http", "", "", "", 80 },
			  { HTTP_URI_BAD_HOSTNAME, "http://%7erver/index.html",
			    "http", "", "", "", 80 },

			  /* Bad port number */
			  { HTTP_URI_BAD_PORT, "http://127.0.0.1:9999a/index.html",
			    "http", "", "127.0.0.1", "", 0 },

			  /* Bad resource */
			  { HTTP_URI_BAD_RESOURCE, "http://server/index.html%",
			    "http", "", "server", "", 80 }
			};
static const char * const base64_tests[][2] =
			{
			  { "A", "QQ==" },
			  /* 010000 01 */
			  { "AB", "QUI=" },
			  /* 010000 010100 0010 */
			  { "ABC", "QUJD" },
			  /* 010000 010100 001001 000011 */
			  { "ABCD", "QUJDRA==" },
			  /* 010000 010100 001001 000011 010001 00 */
			  { "ABCDE", "QUJDREU=" },
			  /* 010000 010100 001001 000011 010001 000100 0101 */
			  { "ABCDEF", "QUJDREVG" },
			  /* 010000 010100 001001 000011 010001 000100 010101 000110 */
			};


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i, j;			/* Looping vars */
  http_t	*http;			/* HTTP connection */
  http_status_t	status;			/* Status of GET command */
  int		failures;		/* Number of test failures */
  char		buffer[8192];		/* Input buffer */
  long		bytes;			/* Number of bytes read */
  FILE		*out;			/* Output file */
  char		encode[256],		/* Base64-encoded string */
		decode[256];		/* Base64-decoded string */
  int		decodelen;		/* Length of decoded string */
  char		scheme[HTTP_MAX_URI],	/* Scheme from URI */
		hostname[HTTP_MAX_URI],	/* Hostname from URI */
		username[HTTP_MAX_URI],	/* Username:password from URI */
		resource[HTTP_MAX_URI];	/* Resource from URI */
  int		port;			/* Port number from URI */
  http_uri_status_t uri_status;		/* Status of URI separation */
  http_addrlist_t *addrlist,		/* Address list */
		*addr;			/* Current address */
  off_t		length, total;		/* Length and total bytes */
  time_t	start, current;		/* Start and end time */
  static const char * const uri_status_strings[] =
		{
		  "HTTP_URI_BAD_ARGUMENTS",
		  "HTTP_URI_BAD_RESOURCE",
		  "HTTP_URI_BAD_PORT",
		  "HTTP_URI_BAD_HOSTNAME",
		  "HTTP_URI_BAD_USERNAME",
		  "HTTP_URI_BAD_SCHEME",
		  "HTTP_URI_BAD_URI",
		  "HTTP_URI_OK",
		  "HTTP_URI_MISSING_SCHEME",
		  "HTTP_URI_UNKNOWN_SCHEME",
		  "HTTP_URI_MISSING_RESOURCE"
		};


 /*
  * Do API tests if we don't have a URL on the command-line...
  */

  if (argc == 1)
  {
    failures = 0;

   /*
    * httpGetDateString()/httpGetDateTime()
    */

    fputs("httpGetDateString()/httpGetDateTime(): ", stdout);

    start = time(NULL);
    strcpy(buffer, httpGetDateString(start));
    current = httpGetDateTime(buffer);

    i = (int)(current - start);
    if (i < 0)
      i = -i;

    if (!i)
      puts("PASS");
    else
    {
      failures ++;
      puts("FAIL");
      printf("    Difference is %d seconds, %02d:%02d:%02d...\n", i, i / 3600,
             (i / 60) % 60, i % 60);
      printf("    httpGetDateString(%d) returned \"%s\"\n", (int)start, buffer);
      printf("    httpGetDateTime(\"%s\") returned %d\n", buffer, (int)current);
      printf("    httpGetDateString(%d) returned \"%s\"\n", (int)current,
             httpGetDateString(current));
    }

   /*
    * httpGetDateString()/httpGetDateTime()
    */

    fputs("httpGetDateString()/httpGetDateTime(): ", stdout);

    start = time(NULL);
    strcpy(buffer, httpGetDateString(start));
    current = httpGetDateTime(buffer);

    i = (int)(current - start);
    if (i < 0)
      i = -i;

    if (!i)
      puts("PASS");
    else
    {
      failures ++;
      puts("FAIL");
      printf("    Difference is %d seconds, %02d:%02d:%02d...\n", i, i / 3600,
             (i / 60) % 60, i % 60);
      printf("    httpGetDateString(%d) returned \"%s\"\n", (int)start, buffer);
      printf("    httpGetDateTime(\"%s\") returned %d\n", buffer, (int)current);
      printf("    httpGetDateString(%d) returned \"%s\"\n", (int)current,
             httpGetDateString(current));
    }

   /*
    * httpDecode64_2()/httpEncode64_2()
    */

    fputs("httpDecode64_2()/httpEncode64_2(): ", stdout);

    for (i = 0, j = 0; i < (int)(sizeof(base64_tests) / sizeof(base64_tests[0])); i ++)
    {
      httpEncode64_2(encode, sizeof(encode), base64_tests[i][0],
                     strlen(base64_tests[i][0]));
      decodelen = (int)sizeof(decode);
      httpDecode64_2(decode, &decodelen, base64_tests[i][1]);

      if (strcmp(decode, base64_tests[i][0]))
      {
        failures ++;

        if (j)
	{
	  puts("FAIL");
	  j = 1;
	}

        printf("    httpDecode64_2() returned \"%s\", expected \"%s\"...\n",
	       decode, base64_tests[i][0]);
      }

      if (strcmp(encode, base64_tests[i][1]))
      {
        failures ++;

        if (j)
	{
	  puts("FAIL");
	  j = 1;
	}

        printf("    httpEncode64_2() returned \"%s\", expected \"%s\"...\n",
	       encode, base64_tests[i][1]);
      }
    }

    if (!j)
      puts("PASS");

   /*
    * httpGetHostname()
    */

    fputs("httpGetHostname(): ", stdout);

    if (httpGetHostname(hostname, sizeof(hostname)))
      printf("PASS (%s)\n", hostname);
    else
    {
      failures ++;
      puts("FAIL");
    }

   /*
    * httpAddrGetList()
    */

    fputs("httpAddrGetList(): ", stdout);

    addrlist = httpAddrGetList(hostname, AF_UNSPEC, NULL);
    if (addrlist)
    {
      for (i = 0, addr = addrlist; addr; i ++, addr = addr->next);

      printf("PASS (%d address(es) for %s)\n", i, hostname);
      httpAddrFreeList(addrlist);
    }
    else
    {
      failures ++;
      puts("FAIL");
    }

   /*
    * Test httpSeparate3()...
    */

    fputs("httpSeparate3(): ", stdout);
    for (i = 0, j = 0; i < (int)(sizeof(uri_tests) / sizeof(uri_tests[0])); i ++)
    {
      uri_status = httpSeparate3(uri_tests[i].uri, scheme, sizeof(scheme),
                                 username, sizeof(username),
				 hostname, sizeof(hostname), &port,
				 resource, sizeof(resource));
      if (uri_status != uri_tests[i].result ||
          strcmp(scheme, uri_tests[i].scheme) ||
	  strcmp(username, uri_tests[i].username) ||
	  strcmp(hostname, uri_tests[i].hostname) ||
	  port != uri_tests[i].port ||
	  strcmp(resource, uri_tests[i].resource))
      {
        failures ++;

	if (!j)
	{
	  puts("FAIL");
	  j = 1;
	}

        printf("    \"%s\":\n", uri_tests[i].uri);

	if (uri_status != uri_tests[i].result)
	  printf("        Returned %s instead of %s\n",
	         uri_status_strings[uri_status + 7],
		 uri_status_strings[uri_tests[i].result + 7]);

        if (strcmp(scheme, uri_tests[i].scheme))
	  printf("        Scheme \"%s\" instead of \"%s\"\n",
	         scheme, uri_tests[i].scheme);

	if (strcmp(username, uri_tests[i].username))
	  printf("        Username \"%s\" instead of \"%s\"\n",
	         username, uri_tests[i].username);

	if (strcmp(hostname, uri_tests[i].hostname))
	  printf("        Hostname \"%s\" instead of \"%s\"\n",
	         hostname, uri_tests[i].hostname);

	if (port != uri_tests[i].port)
	  printf("        Port %d instead of %d\n",
	         port, uri_tests[i].port);

	if (strcmp(resource, uri_tests[i].resource))
	  printf("        Resource \"%s\" instead of \"%s\"\n",
	         resource, uri_tests[i].resource);
      }
    }

    if (!j)
      printf("PASS (%d URIs tested)\n",
             (int)(sizeof(uri_tests) / sizeof(uri_tests[0])));

   /*
    * Show a summary and return...
    */

    if (failures)
      printf("\n%d TESTS FAILED!\n", failures);
    else
      puts("\nALL TESTS PASSED!");

    return (failures);
  }

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
      if (i >= argc)
        break;

      out = fopen(argv[i], "wb");
      continue;
    }

    httpSeparate3(argv[i], scheme, sizeof(scheme), username, sizeof(username),
                  hostname, sizeof(hostname), &port,
		  resource, sizeof(resource));

    http = httpConnectEncrypt(hostname, port, HTTP_ENCRYPT_IF_REQUESTED);
    if (http == NULL)
    {
      perror(hostname);
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

  return (0);
}


/*
 * End of "$Id$".
 */
