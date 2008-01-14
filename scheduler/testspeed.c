/*
 * "$Id$"
 *
 *   Scheduler speed test for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()    - Send multiple IPP requests and report on the average response
 *               time.
 *   do_test() - Run a test on a specific host...
 *   usage()   - Show program usage...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>
#include <errno.h>


/*
 * Local functions...
 */

static int	do_test(const char *server, http_encryption_t encryption,
			int requests, int verbose);
static void	usage(void);


/*
 * 'main()' - Send multiple IPP requests and report on the average response
 *            time.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  const char	*server;		/* Server to use */
  http_encryption_t encryption;		/* Encryption to use */
  int		requests;		/* Number of requests to send */
  int		children;		/* Number of children to fork */
  int		pid;			/* Child PID */
  int		status;			/* Child status */
  time_t	start,			/* Start time */
		end;			/* End time */
  double	elapsed;		/* Elapsed time */
  int		verbose;		/* Verbosity */


 /*
  * Parse command-line options...
  */

  requests   = 100;
  children   = 5;
  server     = cupsServer();
  encryption = HTTP_ENCRYPT_IF_REQUESTED;
  verbose    = 0;

  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-c"))
    {
      i ++;
      if (i >= argc)
        usage();

      children = atoi(argv[i]);
    }
    else if (!strcmp(argv[i], "-r"))
    {
      i ++;
      if (i >= argc)
        usage();

      requests = atoi(argv[i]);
    }
    else if (!strcmp(argv[i], "-E"))
      encryption = HTTP_ENCRYPT_REQUIRED;
    else if (!strcmp(argv[i], "-v"))
      verbose ++;
    else if (argv[i][0] == '-')
      usage();
    else
      server = argv[i];

 /*
  * Then create child processes to act as clients...
  */

  printf("testspeed: Simulating %d clients with %d requests to %s with %s encryption...\n",
         children, requests, server,
	 encryption == HTTP_ENCRYPT_IF_REQUESTED ? "no" : "");

  start = time(NULL);

  if (children == 1)
  {
    do_test(server, encryption, requests, verbose);
  }
  else
  {
    for (i = 0; i < children; i ++)
      if ((pid = fork()) == 0)
      {
       /*
	* Child goes here...
	*/

	exit(do_test(server, encryption, requests, verbose));
      }
      else if (pid < 0)
      {
	perror("fork failed");
	break;
      }
      else
	printf("testspeed(%d): Started...\n", pid);

   /*
    * Wait for children to finish...
    */

    for (;;)
    {
      pid = wait(&status);

      if (pid < 0 && errno != EINTR)
	break;

      printf("testspeed(%d): Ended (%d)...\n", pid, status);
    }
  }

 /*
  * Compute the total run time...
  */

  end     = time(NULL);
  elapsed = end - start;
  i       = children * requests;

  printf("testspeed: %dx%d=%d requests in %.1fs (%.3fs/r, %.1fr/s)\n",
         children, requests, i, elapsed, elapsed / i, i / elapsed);

 /*
  * Exit with no errors...
  */

  return (0);
}


/*
 * 'do_test()' - Run a test on a specific host...
 */

static int				/* O - Exit status */
do_test(const char        *server,	/* I - Server to use */
        http_encryption_t encryption,	/* I - Encryption to use */
	int               requests,	/* I - Number of requests to send */
	int               verbose)	/* I - Verbose output? */
{
  int		i;			/* Looping var */
  http_t	*http;			/* Connection to server */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  struct timeval start,			/* Start time */
		end;			/* End time */
  double	elapsed;		/* Elapsed time */
  static ipp_op_t ops[4] =		/* Operations to test... */
		{
		  IPP_PRINT_JOB,
		  CUPS_GET_PRINTERS,
		  CUPS_GET_CLASSES,
		  IPP_GET_JOBS
		};


 /*
  * Connect to the server...
  */

  http = httpConnectEncrypt(server, ippPort(), encryption);

  if (http == NULL)
  {
    perror("testspeed: unable to connect to server");
    return (1);
  }

  language = cupsLangDefault();

 /*
  * Do multiple requests...
  */

  for (elapsed = 0.0, i = 0; i < requests; i ++)
  {
    if (verbose && (i % 10) == 0)
      printf("testspeed(%d): %d%% complete...\n", (int)getpid(),
             i * 100 / requests);

   /*
    * Build a request which requires the following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *
    * In addition, IPP_GET_JOBS needs a printer-uri attribute.
    */

    request = ippNew();

    request->request.op.operation_id = ops[i & 3];
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    gettimeofday(&start, NULL);

    switch (request->request.op.operation_id)
    {
      case IPP_GET_JOBS :
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                       NULL, "ipp://localhost/printers/");

      default :
	  response = cupsDoRequest(http, request, "/");
          break;

      case IPP_PRINT_JOB :
	  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                       NULL, "ipp://localhost/printers/test");
	  response = cupsDoFileRequest(http, request, "/printers/test",
	                               "../data/testprint.ps");
          break;
    }

    gettimeofday(&end, NULL);

    if (response != NULL)
      ippDelete(response);

    elapsed += (end.tv_sec - start.tv_sec) +
               0.000001 * (end.tv_usec - start.tv_usec);
  }

  cupsLangFree(language);
  httpClose(http);

  printf("testspeed(%d): %d requests in %.1fs (%.3fs/r, %.1fr/s)\n",
         (int)getpid(), i, elapsed, elapsed / i, i / elapsed);

  return (0);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(void)
{
  puts("Usage: testspeed [-c children] [-h] [-r requests] [-v] [-E] hostname");
  exit(0);
}



/*
 * End of "$Id$".
 */
