/*
 * "$Id: testspeed.c,v 1.2 2000/01/04 13:46:10 mike Exp $"
 *
 *   Scheduler speed test for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/debug.h>


/*
 * 'main()' - Send multiple IPP requests and report on the average response
 *            time.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  http_t	*http;		/* Connection to server */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  cups_lang_t	*language;	/* Default language */
  struct timeval start,		/* Start time */
		end;		/* End time */
  double	elapsed;	/* Elapsed time */


  if (argc > 1)
    http = httpConnect(argv[1], ippPort());
  else
    http = httpConnect("localhost", ippPort());

  if (http == NULL)
  {
    perror("testspeed: unable to connect to server");
    return (1);
  }

  language = cupsLangDefault();

 /*
  * Do requests 100 times...
  */

  printf("Testing: ");

  for (elapsed = 0.0, i = 0; i < 100; i ++)
  {
    putchar('>');
    fflush(stdout);

   /*
    * Build a CUPS_GET_PRINTERS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_GET_PRINTERS;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    gettimeofday(&start, NULL);
    response = cupsDoRequest(http, request, "/printers/");
    gettimeofday(&end, NULL);

    putchar('<');

    if (response != NULL)
      ippDelete(response);

    elapsed += (end.tv_sec - start.tv_sec) +
               0.000001 * (end.tv_usec - start.tv_usec);
  }

  puts("");
  printf("Total elapsed time for %d requests was %.1fs (%.3fs/r)\n",
         i, elapsed, elapsed / i);

  return (0);
}


/*
 * End of "$Id: testspeed.c,v 1.2 2000/01/04 13:46:10 mike Exp $".
 */
