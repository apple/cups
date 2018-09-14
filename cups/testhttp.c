/*
 * HTTP test program for CUPS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2006 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"


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
  int			port,		/* Port number */
			assemble_port;	/* Port number for httpAssembleURI() */
  http_uri_coding_t	assemble_coding;/* Coding for httpAssembleURI() */
} uri_test_t;


/*
 * Local globals...
 */

static uri_test_t	uri_tests[] =	/* URI test data */
			{
			  /* Start with valid URIs */
			  { HTTP_URI_STATUS_OK, "file:/filename",
			    "file", "", "", "/filename", 0, 0,
			    HTTP_URI_CODING_MOST },
			  { HTTP_URI_STATUS_OK, "file:/filename%20with%20spaces",
			    "file", "", "", "/filename with spaces", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file:///filename",
			    "file", "", "", "/filename", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file:///filename%20with%20spaces",
			    "file", "", "", "/filename with spaces", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file://localhost/filename",
			    "file", "", "localhost", "/filename", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "file://localhost/filename%20with%20spaces",
			    "file", "", "localhost", "/filename with spaces", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://server/",
			    "http", "", "server", "/", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username@server/",
			    "http", "username", "server", "/", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username:passwor%64@server/",
			    "http", "username:password", "server", "/", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username:passwor%64@server:8080/",
			    "http", "username:password", "server", "/", 8080, 8080,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://username:passwor%64@server:8080/directory/filename",
			    "http", "username:password", "server", "/directory/filename", 8080, 8080,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "http://[2000::10:100]:631/ipp",
			    "http", "", "2000::10:100", "/ipp", 631, 631,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "https://username:passwor%64@server/directory/filename",
			    "https", "username:password", "server", "/directory/filename", 443, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://username:passwor%64@[::1]/ipp",
			    "ipp", "username:password", "::1", "/ipp", 631, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "lpd://server/queue?reserve=yes",
			    "lpd", "", "server", "/queue?reserve=yes", 515, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "mailto:user@domain.com",
			    "mailto", "", "", "user@domain.com", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "socket://server/",
			    "socket", "", "server", "/", 9100, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "socket://192.168.1.1:9101/",
			    "socket", "", "192.168.1.1", "/", 9101, 9101,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "tel:8005551212",
			    "tel", "", "", "8005551212", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://username:password@[v1.fe80::200:1234:5678:9abc+eth0]:999/ipp",
			    "ipp", "username:password", "fe80::200:1234:5678:9abc%eth0", "/ipp", 999, 999,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://username:password@[fe80::200:1234:5678:9abc%25eth0]:999/ipp",
			    "ipp", "username:password", "fe80::200:1234:5678:9abc%eth0", "/ipp", 999, 999,
			    (http_uri_coding_t)(HTTP_URI_CODING_MOST | HTTP_URI_CODING_RFC6874) },
			  { HTTP_URI_STATUS_OK, "http://server/admin?DEVICE_URI=usb://HP/Photosmart%25202600%2520series?serial=MY53OK70V10400",
			    "http", "", "server", "/admin?DEVICE_URI=usb://HP/Photosmart%25202600%2520series?serial=MY53OK70V10400", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "lpd://Acme%20Laser%20(01%3A23%3A45).local._tcp._printer/",
			    "lpd", "", "Acme Laser (01:23:45).local._tcp._printer", "/", 515, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://HP%20Officejet%204500%20G510n-z%20%40%20Will's%20MacBook%20Pro%2015%22._ipp._tcp.local./",
			    "ipp", "", "HP Officejet 4500 G510n-z @ Will's MacBook Pro 15\"._ipp._tcp.local.", "/", 631, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_OK, "ipp://%22%23%2F%3A%3C%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D/",
			    "ipp", "", "\"#/:<>?@[\\]^`{|}", "/", 631, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_UNKNOWN_SCHEME, "smb://server/Some%20Printer",
			    "smb", "", "server", "/Some Printer", 0, 0,
			    HTTP_URI_CODING_ALL },

			  /* Missing scheme */
			  { HTTP_URI_STATUS_MISSING_SCHEME, "/path/to/file/index.html",
			    "file", "", "", "/path/to/file/index.html", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_MISSING_SCHEME, "//server/ipp",
			    "ipp", "", "server", "/ipp", 631, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Unknown scheme */
			  { HTTP_URI_STATUS_UNKNOWN_SCHEME, "vendor://server/resource",
			    "vendor", "", "server", "/resource", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Missing resource */
			  { HTTP_URI_STATUS_MISSING_RESOURCE, "socket://[::192.168.2.1]",
			    "socket", "", "::192.168.2.1", "/", 9100, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_MISSING_RESOURCE, "socket://192.168.1.1:9101",
			    "socket", "", "192.168.1.1", "/", 9101, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Bad URI */
			  { HTTP_URI_STATUS_BAD_URI, "",
			    "", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Bad scheme */
			  { HTTP_URI_STATUS_BAD_SCHEME, "://server/ipp",
			    "", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_SCHEME, "bad_scheme://server/resource",
			    "", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Bad username */
			  { HTTP_URI_STATUS_BAD_USERNAME, "http://username:passwor%6@server/resource",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Bad hostname */
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://[/::1]/index.html",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://[",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://serve%7/index.html",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "http://server with spaces/index.html",
			    "http", "", "", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_HOSTNAME, "ipp://\"#/:<>?@[\\]^`{|}/",
			    "ipp", "", "", "", 631, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Bad port number */
			  { HTTP_URI_STATUS_BAD_PORT, "http://127.0.0.1:9999a/index.html",
			    "http", "", "127.0.0.1", "", 0, 0,
			    HTTP_URI_CODING_MOST  },

			  /* Bad resource */
			  { HTTP_URI_STATUS_BAD_RESOURCE, "mailto:\r\nbla",
			    "mailto", "", "", "", 0, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_RESOURCE, "http://server/index.html%",
			    "http", "", "server", "", 80, 0,
			    HTTP_URI_CODING_MOST  },
			  { HTTP_URI_STATUS_BAD_RESOURCE, "http://server/index with spaces.html",
			    "http", "", "server", "", 80, 0,
			    HTTP_URI_CODING_MOST  }
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
  int		i, j, k;		/* Looping vars */
  http_t	*http;			/* HTTP connection */
  http_encryption_t encryption;		/* Encryption type */
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
  const char	*encoding;		/* Negotiated Content-Encoding */
  static const char * const uri_status_strings[] =
		{
		  "HTTP_URI_STATUS_OVERFLOW",
		  "HTTP_URI_STATUS_BAD_ARGUMENTS",
		  "HTTP_URI_STATUS_BAD_RESOURCE",
		  "HTTP_URI_STATUS_BAD_PORT",
		  "HTTP_URI_STATUS_BAD_HOSTNAME",
		  "HTTP_URI_STATUS_BAD_USERNAME",
		  "HTTP_URI_STATUS_BAD_SCHEME",
		  "HTTP_URI_STATUS_BAD_URI",
		  "HTTP_URI_STATUS_OK",
		  "HTTP_URI_STATUS_MISSING_SCHEME",
		  "HTTP_URI_STATUS_UNKNOWN_SCHEME",
		  "HTTP_URI_STATUS_MISSING_RESOURCE"
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
    strlcpy(buffer, httpGetDateString(start), sizeof(buffer));
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
                     (int)strlen(base64_tests[i][0]));
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

#if 0
   /*
    * _httpDigest()
    */

    fputs("_httpDigest(MD5): ", stdout);
    if (!_httpDigest(buffer, sizeof(buffer), "MD5", "Mufasa", "http-auth@example.org", "Circle of Life", "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v", 1, "f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ", "auth", "GET", "/dir/index.html"))
    {
      failures ++;
      puts("FAIL (unable to calculate hash)");
    }
    else if (strcmp(buffer, "8ca523f5e9506fed4657c9700eebdbec"))
    {
      failures ++;
      printf("FAIL (got \"%s\", expected \"8ca523f5e9506fed4657c9700eebdbec\")\n", buffer);
    }
    else
      puts("PASS");

    fputs("_httpDigest(SHA-256): ", stdout);
    if (!_httpDigest(buffer, sizeof(buffer), "SHA-256", "Mufasa", "http-auth@example.org", "Circle of Life", "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v", 1, "f2/wE4q74E6zIJEtWaHKaf5wv/H5QzzpXusqGemxURZJ", "auth", "GET", "/dir/index.html"))
    {
      failures ++;
      puts("FAIL (unable to calculate hash)");
    }
    else if (strcmp(buffer, "753927fa0e85d155564e2e272a28d1802ca10daf4496794697cf8db5856cb6c1"))
    {
      failures ++;
      printf("FAIL (got \"%s\", expected \"753927fa0e85d155564e2e272a28d1802ca10daf4496794697cf8db5856cb6c1\")\n", buffer);
    }
    else
      puts("PASS");
#endif /* 0 */

   /*
    * httpGetHostname()
    */

    fputs("httpGetHostname(): ", stdout);

    if (httpGetHostname(NULL, hostname, sizeof(hostname)))
      printf("PASS (%s)\n", hostname);
    else
    {
      failures ++;
      puts("FAIL");
    }

   /*
    * httpAddrGetList()
    */

    printf("httpAddrGetList(%s): ", hostname);

    addrlist = httpAddrGetList(hostname, AF_UNSPEC, NULL);
    if (addrlist)
    {
      for (i = 0, addr = addrlist; addr; i ++, addr = addr->next)
      {
        char	numeric[1024];		/* Numeric IP address */


	httpAddrString(&(addr->addr), numeric, sizeof(numeric));
	if (!strcmp(numeric, "UNKNOWN"))
	  break;
      }

      if (addr)
        printf("FAIL (bad address for %s)\n", hostname);
      else
        printf("PASS (%d address(es) for %s)\n", i, hostname);

      httpAddrFreeList(addrlist);
    }
    else if (isdigit(hostname[0] & 255))
    {
      puts("FAIL (ignored because hostname is numeric)");
    }
    else
    {
      failures ++;
      puts("FAIL");
    }

   /*
    * Test httpSeparateURI()...
    */

    fputs("httpSeparateURI(): ", stdout);
    for (i = 0, j = 0; i < (int)(sizeof(uri_tests) / sizeof(uri_tests[0])); i ++)
    {
      uri_status = httpSeparateURI(HTTP_URI_CODING_MOST,
				   uri_tests[i].uri, scheme, sizeof(scheme),
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
	         uri_status_strings[uri_status + 8],
		 uri_status_strings[uri_tests[i].result + 8]);

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
    * Test httpAssembleURI()...
    */

    fputs("httpAssembleURI(): ", stdout);
    for (i = 0, j = 0, k = 0;
         i < (int)(sizeof(uri_tests) / sizeof(uri_tests[0]));
	 i ++)
      if (uri_tests[i].result == HTTP_URI_STATUS_OK &&
          !strstr(uri_tests[i].uri, "%64") &&
          strstr(uri_tests[i].uri, "//"))
      {
        k ++;
	uri_status = httpAssembleURI(uri_tests[i].assemble_coding,
				     buffer, sizeof(buffer),
	                             uri_tests[i].scheme,
				     uri_tests[i].username,
	                             uri_tests[i].hostname,
				     uri_tests[i].assemble_port,
				     uri_tests[i].resource);

        if (uri_status != HTTP_URI_STATUS_OK)
	{
          failures ++;

	  if (!j)
	  {
	    puts("FAIL");
	    j = 1;
	  }

          printf("    \"%s\": %s\n", uri_tests[i].uri,
	         uri_status_strings[uri_status + 8]);
        }
	else if (strcmp(buffer, uri_tests[i].uri))
	{
          failures ++;

	  if (!j)
	  {
	    puts("FAIL");
	    j = 1;
	  }

          printf("    \"%s\": assembled = \"%s\"\n", uri_tests[i].uri,
	         buffer);
	}
      }

    if (!j)
      printf("PASS (%d URIs tested)\n", k);

   /*
    * httpAssembleUUID
    */

    fputs("httpAssembleUUID: ", stdout);
    httpAssembleUUID("hostname.example.com", 631, "printer", 12345, buffer,
                     sizeof(buffer));
    if (strncmp(buffer, "urn:uuid:", 9))
    {
      printf("FAIL (%s)\n", buffer);
      failures ++;
    }
    else
      printf("PASS (%s)\n", buffer);

   /*
    * Show a summary and return...
    */

    if (failures)
      printf("\n%d TESTS FAILED!\n", failures);
    else
      puts("\nALL TESTS PASSED!");

    return (failures);
  }
  else if (strstr(argv[1], "._tcp"))
  {
   /*
    * Test resolving an mDNS name.
    */

    char	resolved[1024];		/* Resolved URI */


    printf("_httpResolveURI(%s, _HTTP_RESOLVE_DEFAULT): ", argv[1]);
    fflush(stdout);

    if (!_httpResolveURI(argv[1], resolved, sizeof(resolved),
                         _HTTP_RESOLVE_DEFAULT, NULL, NULL))
    {
      puts("FAIL");
      return (1);
    }
    else
      printf("PASS (%s)\n", resolved);

    printf("_httpResolveURI(%s, _HTTP_RESOLVE_FQDN): ", argv[1]);
    fflush(stdout);

    if (!_httpResolveURI(argv[1], resolved, sizeof(resolved),
                         _HTTP_RESOLVE_FQDN, NULL, NULL))
    {
      puts("FAIL");
      return (1);
    }
    else if (strstr(resolved, ".local:"))
    {
      printf("FAIL (%s)\n", resolved);
      return (1);
    }
    else
    {
      printf("PASS (%s)\n", resolved);
      return (0);
    }
  }
  else if (!strcmp(argv[1], "-u") && argc == 3)
  {
   /*
    * Test URI separation...
    */

    uri_status = httpSeparateURI(HTTP_URI_CODING_ALL, argv[2], scheme,
                                 sizeof(scheme), username, sizeof(username),
				 hostname, sizeof(hostname), &port,
				 resource, sizeof(resource));
    printf("uri_status = %s\n", uri_status_strings[uri_status + 8]);
    printf("scheme     = \"%s\"\n", scheme);
    printf("username   = \"%s\"\n", username);
    printf("hostname   = \"%s\"\n", hostname);
    printf("port       = %d\n", port);
    printf("resource   = \"%s\"\n", resource);

    return (0);
  }

 /*
  * Test HTTP GET requests...
  */

  http = NULL;
  out = stdout;

  for (i = 1; i < argc; i ++)
  {
    int new_auth;

    if (!strcmp(argv[i], "-o"))
    {
      i ++;
      if (i >= argc)
        break;

      out = fopen(argv[i], "wb");
      continue;
    }

    httpSeparateURI(HTTP_URI_CODING_MOST, argv[i], scheme, sizeof(scheme),
                    username, sizeof(username),
                    hostname, sizeof(hostname), &port,
		    resource, sizeof(resource));

    if (!_cups_strcasecmp(scheme, "https") || !_cups_strcasecmp(scheme, "ipps") ||
        port == 443)
      encryption = HTTP_ENCRYPTION_ALWAYS;
    else
      encryption = HTTP_ENCRYPTION_IF_REQUESTED;

    http = httpConnect2(hostname, port, NULL, AF_UNSPEC, encryption, 1, 30000, NULL);
    if (http == NULL)
    {
      perror(hostname);
      continue;
    }

    if (httpIsEncrypted(http))
    {
      cups_array_t *creds;
      char info[1024];
      static const char *trusts[] = { "OK", "Invalid", "Changed", "Expired", "Renewed", "Unknown" };
      if (!httpCopyCredentials(http, &creds))
      {
	cups_array_t *lcreds;
        http_trust_t trust = httpCredentialsGetTrust(creds, hostname);

        httpCredentialsString(creds, info, sizeof(info));

	printf("Count: %d\n", cupsArrayCount(creds));
        printf("Trust: %s\n", trusts[trust]);
        printf("Expiration: %s\n", httpGetDateString(httpCredentialsGetExpiration(creds)));
        printf("IsValidName: %d\n", httpCredentialsAreValidForName(creds, hostname));
        printf("String: \"%s\"\n", info);

	printf("LoadCredentials: %d\n", httpLoadCredentials(NULL, &lcreds, hostname));
	httpCredentialsString(lcreds, info, sizeof(info));
	printf("    Count: %d\n", cupsArrayCount(lcreds));
	printf("    String: \"%s\"\n", info);

        if (lcreds && cupsArrayCount(creds) == cupsArrayCount(lcreds))
        {
          http_credential_t	*cred, *lcred;

          for (i = 1, cred = (http_credential_t *)cupsArrayFirst(creds), lcred = (http_credential_t *)cupsArrayFirst(lcreds);
               cred && lcred;
               i ++, cred = (http_credential_t *)cupsArrayNext(creds), lcred = (http_credential_t *)cupsArrayNext(lcreds))
          {
            if (cred->datalen != lcred->datalen)
              printf("    Credential #%d: Different lengths (saved=%d, current=%d)\n", i, (int)cred->datalen, (int)lcred->datalen);
            else if (memcmp(cred->data, lcred->data, cred->datalen))
              printf("    Credential #%d: Different data\n", i);
            else
              printf("    Credential #%d: Matches\n", i);
          }
        }

        if (trust != HTTP_TRUST_OK)
	{
	  printf("SaveCredentials: %d\n", httpSaveCredentials(NULL, creds, hostname));
	  trust = httpCredentialsGetTrust(creds, hostname);
	  printf("New Trust: %s\n", trusts[trust]);
	}

        httpFreeCredentials(creds);
      }
      else
        puts("No credentials!");
    }

    printf("Checking file \"%s\"...\n", resource);

    new_auth = 0;

    do
    {
      if (!_cups_strcasecmp(httpGetField(http, HTTP_FIELD_CONNECTION), "close"))
      {
	httpClearFields(http);
	if (httpReconnect2(http, 30000, NULL))
	{
          status = HTTP_STATUS_ERROR;
          break;
	}
      }

      if (http->authstring && !strncmp(http->authstring, "Digest ", 7) && !new_auth)
        _httpSetDigestAuthString(http, http->nextnonce, "HEAD", resource);

      httpClearFields(http);
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, httpGetAuthString(http));
      httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");

      if (httpHead(http, resource))
      {
        if (httpReconnect2(http, 30000, NULL))
        {
          status = HTTP_STATUS_ERROR;
          break;
        }
        else
        {
          status = HTTP_STATUS_UNAUTHORIZED;
          continue;
        }
      }

      while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

      new_auth = 0;

      if (status == HTTP_STATUS_UNAUTHORIZED)
      {
       /*
	* Flush any error message...
	*/

	httpFlush(http);

       /*
	* See if we can do authentication...
	*/

        new_auth = 1;

	if (cupsDoAuthentication(http, "HEAD", resource))
	{
	  status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
	  break;
	}

	if (httpReconnect2(http, 30000, NULL))
	{
	  status = HTTP_STATUS_ERROR;
	  break;
	}

	continue;
      }
#ifdef HAVE_SSL
      else if (status == HTTP_STATUS_UPGRADE_REQUIRED)
      {
	/* Flush any error message... */
	httpFlush(http);

	/* Reconnect... */
	if (httpReconnect2(http, 30000, NULL))
	{
	  status = HTTP_STATUS_ERROR;
	  break;
	}

	/* Upgrade with encryption... */
	httpEncryption(http, HTTP_ENCRYPTION_REQUIRED);

	/* Try again, this time with encryption enabled... */
	continue;
      }
#endif /* HAVE_SSL */
    }
    while (status == HTTP_STATUS_UNAUTHORIZED ||
           status == HTTP_STATUS_UPGRADE_REQUIRED);

    if (status == HTTP_STATUS_OK)
      puts("HEAD OK:");
    else
      printf("HEAD failed with status %d...\n", status);

    encoding = httpGetContentEncoding(http);

    printf("Requesting file \"%s\" (Accept-Encoding: %s)...\n", resource,
           encoding ? encoding : "identity");

    new_auth = 0;

    do
    {
      if (!_cups_strcasecmp(httpGetField(http, HTTP_FIELD_CONNECTION), "close"))
      {
	httpClearFields(http);
	if (httpReconnect2(http, 30000, NULL))
	{
          status = HTTP_STATUS_ERROR;
          break;
	}
      }

      if (http->authstring && !strncmp(http->authstring, "Digest ", 7) && !new_auth)
        _httpSetDigestAuthString(http, http->nextnonce, "GET", resource);

      httpClearFields(http);
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, httpGetAuthString(http));
      httpSetField(http, HTTP_FIELD_ACCEPT_LANGUAGE, "en");
      httpSetField(http, HTTP_FIELD_ACCEPT_ENCODING, encoding);

      if (httpGet(http, resource))
      {
        if (httpReconnect2(http, 30000, NULL))
        {
          status = HTTP_STATUS_ERROR;
          break;
        }
        else
        {
          status = HTTP_STATUS_UNAUTHORIZED;
          continue;
        }
      }

      while ((status = httpUpdate(http)) == HTTP_STATUS_CONTINUE);

      new_auth = 0;

      if (status == HTTP_STATUS_UNAUTHORIZED)
      {
       /*
	* Flush any error message...
	*/

	httpFlush(http);

       /*
	* See if we can do authentication...
	*/

        new_auth = 1;

	if (cupsDoAuthentication(http, "GET", resource))
	{
	  status = HTTP_STATUS_CUPS_AUTHORIZATION_CANCELED;
	  break;
	}

	if (httpReconnect2(http, 30000, NULL))
	{
	  status = HTTP_STATUS_ERROR;
	  break;
	}

	continue;
      }
#ifdef HAVE_SSL
      else if (status == HTTP_STATUS_UPGRADE_REQUIRED)
      {
	/* Flush any error message... */
	httpFlush(http);

	/* Reconnect... */
	if (httpReconnect2(http, 30000, NULL))
	{
	  status = HTTP_STATUS_ERROR;
	  break;
	}

	/* Upgrade with encryption... */
	httpEncryption(http, HTTP_ENCRYPTION_REQUIRED);

	/* Try again, this time with encryption enabled... */
	continue;
      }
#endif /* HAVE_SSL */
    }
    while (status == HTTP_STATUS_UNAUTHORIZED || status == HTTP_STATUS_UPGRADE_REQUIRED);

    if (status == HTTP_STATUS_OK)
      puts("GET OK:");
    else
      printf("GET failed with status %d...\n", status);

    start  = time(NULL);
    length = httpGetLength2(http);
    total  = 0;

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
    {
      total += bytes;
      fwrite(buffer, (size_t)bytes, 1, out);
      if (out != stdout)
      {
        current = time(NULL);
        if (current == start)
          current ++;

        printf("\r" CUPS_LLFMT "/" CUPS_LLFMT " bytes ("
	       CUPS_LLFMT " bytes/sec)      ", CUPS_LLCAST total,
	       CUPS_LLCAST length, CUPS_LLCAST (total / (current - start)));
        fflush(stdout);
      }
    }
  }

  if (out != stdout)
    putchar('\n');

  puts("Closing connection to server...");
  httpClose(http);

  if (out != stdout)
    fclose(out);

  return (0);
}
