/*
 * "$Id: lprm.c,v 1.3 1999/06/18 18:36:05 mike Exp $"
 *
 *   "lprm" command for the Common UNIX Printing System (CUPS).
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
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Parse options and cancel jobs.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <cups/cups.h>
#include <cups/language.h>


/*
 * 'main()' - Parse options and cancel jobs.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  http_t	*http;		/* HTTP connection to server */
  int		i;		/* Looping var */
  int		job_id;		/* Job ID */
  char		*dest;		/* Destination printer */
  char		uri[1024];	/* Printer or job URI */
  ipp_t		*request;	/* IPP request */
  ipp_t		*response;	/* IPP response */
  ipp_op_t	op;		/* Operation */
  cups_lang_t	*language;	/* Language */


 /*
  * Setup to cancel individual print jobs...
  */

  op       = IPP_CANCEL_JOB;
  job_id   = 0;
  dest     = cupsGetDefault();
  response = NULL;

 /*
  * Open a connection to the server...
  */

  if ((http = httpConnect("localhost", ippPort())) == NULL)
  {
    fputs("lprm: Unable to contact server!\n", stderr);
    return (1);
  }

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1] != '\0')
      switch (argv[i][1])
      {
        case 'P' : /* Cancel jobs on a printer */
	    if (argv[i][2])
	      dest = argv[i] + 2;
	    else
	    {
	      i ++;
	      dest = argv[i];
	    }
	    break;

	default :
	    fprintf(stderr, "lprm: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
     /*
      * Cancel a job or printer...
      */

      if (isdigit(argv[i][0]))
      {
        dest   = NULL;
	op     = IPP_CANCEL_JOB;
        job_id = atoi(argv[i]);
      }
      else if (strcmp(argv[i], "-") == 0)
      {
       /*
        * Cancel all jobs
        */

        op = IPP_PURGE_JOBS;
      }
      else
        job_id = 0;

     /*
      * Build an IPP request, which requires the following
      * attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri + job-id *or* job-uri
      *    [requesting-user-name]
      */

      request = ippNew();

      request->request.op.operation_id = op;
      request->request.op.request_id   = 1;

      language = cupsLangDefault();

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
              	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                   "attributes-natural-language", NULL, language->language);

      if (dest)
      {
        sprintf(uri, "ipp://localhost/printers/%s", dest);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	             "printer-uri", NULL, uri);
	ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
	              job_id);
      }
      else
      {
        sprintf(uri, "ipp://localhost/jobs/%d", job_id);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL,
	             uri);
      }

     /*
      * Do the request and get back a response...
      */

      if (op == IPP_PURGE_JOBS)
        response = cupsDoRequest(http, request, "/admin/");
      else
        response = cupsDoRequest(http, request, "/jobs/");

      if (response != NULL)
        ippDelete(response);
      else
      {
        fputs("lprm: Unable to cancel job(s)!\n", stderr);
	return (1);
      }
    }

 /*
  * If nothing has been cancelled yet, cancel the current job on the specified
  * (or default) printer...
  */

  if (response == NULL)
    if (!cupsCancelJob(dest, 0))
    {
      fputs("lprm: Unable to cancel job(s)!\n", stderr);
      return (1);
    }

  return (0);
}


/*
 * End of "$Id: lprm.c,v 1.3 1999/06/18 18:36:05 mike Exp $".
 */
