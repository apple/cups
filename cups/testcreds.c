/*
 * HTTP credentials test program for CUPS.
 *
 * Copyright 2007-2016 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
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
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* HTTP connection */
  char		scheme[HTTP_MAX_URI],	/* Scheme from URI */
		hostname[HTTP_MAX_URI],	/* Hostname from URI */
		username[HTTP_MAX_URI],	/* Username:password from URI */
		resource[HTTP_MAX_URI];	/* Resource from URI */
  int		port;			/* Port number from URI */
  http_trust_t	trust;			/* Trust evaluation for connection */
  cups_array_t	*hcreds,		/* Credentials from connection */
		*tcreds;		/* Credentials from trust store */
  char		hinfo[1024],		/* String for connection credentials */
		tinfo[1024];		/* String for trust store credentials */
  static const char *trusts[] =		/* Trust strings */
  { "OK", "Invalid", "Changed", "Expired", "Renewed", "Unknown" };


 /*
  * Check command-line...
  */

  if (argc != 2)
  {
    puts("Usage: ./testcreds hostname");
    puts("       ./testcreds https://hostname[:port]");
    return (1);
  }

  if (!strncmp(argv[1], "https://", 8))
  {
   /*
    * Connect to the host and validate credentials...
    */

    if (httpSeparateURI(HTTP_URI_CODING_MOST, argv[1], scheme, sizeof(scheme), username, sizeof(username), hostname, sizeof(hostname), &port, resource, sizeof(resource)) < HTTP_URI_STATUS_OK)
    {
      printf("ERROR: Bad URI \"%s\".\n", argv[1]);
      return (1);
    }

    if ((http = httpConnect2(hostname, port, NULL, AF_UNSPEC, HTTP_ENCRYPTION_ALWAYS, 1, 30000, NULL)) == NULL)
    {
      printf("ERROR: Unable to connect to \"%s\" on port %d: %s\n", hostname, port, cupsLastErrorString());
      return (1);
    }

    puts("HTTP Credentials:");
    if (!httpCopyCredentials(http, &hcreds))
    {
      trust = httpCredentialsGetTrust(hcreds, hostname);

      httpCredentialsString(hcreds, hinfo, sizeof(hinfo));

      printf("    Certificate Count: %d\n", cupsArrayCount(hcreds));
      if (trust == HTTP_TRUST_OK)
        puts("    Trust: OK");
      else
        printf("    Trust: %s (%s)\n", trusts[trust], cupsLastErrorString());
      printf("    Expiration: %s\n", httpGetDateString(httpCredentialsGetExpiration(hcreds)));
      printf("    IsValidName: %d\n", httpCredentialsAreValidForName(hcreds, hostname));
      printf("    String: \"%s\"\n", hinfo);

      httpFreeCredentials(hcreds);
    }
    else
      puts("    Not present (error).");

    puts("");
  }
  else
  {
   /*
    * Load stored credentials...
    */

    strlcpy(hostname, argv[1], sizeof(hostname));
  }

  printf("Trust Store for \"%s\":\n", hostname);

  if (!httpLoadCredentials(NULL, &tcreds, hostname))
  {
    httpCredentialsString(tcreds, tinfo, sizeof(tinfo));

    printf("    Certificate Count: %d\n", cupsArrayCount(tcreds));
    printf("    Expiration: %s\n", httpGetDateString(httpCredentialsGetExpiration(tcreds)));
    printf("    IsValidName: %d\n", httpCredentialsAreValidForName(tcreds, hostname));
    printf("    String: \"%s\"\n", tinfo);

    httpFreeCredentials(tcreds);
  }
  else
    puts("    Not present.");

  return (0);
}
