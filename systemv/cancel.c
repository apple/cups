/*
 * "$Id: cancel.c,v 1.19.2.7 2002/08/21 20:00:17 mike Exp $"
 *
 *   "cancel" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
#include <cups/string.h>
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
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests;		/* Destinations */
  char		*dest,		/* Destination printer */
		*job;		/* Job ID pointer */
  char		uri[1024];	/* Printer or job URI */
  ipp_t		*request;	/* IPP request */
  ipp_t		*response;	/* IPP response */
  ipp_op_t	op;		/* Operation */
  cups_lang_t	*language;	/* Language */
  http_encryption_t encryption;	/* Encryption? */


 /*
  * Setup to cancel individual print jobs...
  */

  op         = IPP_CANCEL_JOB;
  job_id     = 0;
  dest       = NULL;
  http       = NULL;
  encryption = cupsEncryption();
  num_dests  = 0;
  dests      = NULL;

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1])
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_LIBSSL
	    encryption = HTTP_ENCRYPT_REQUIRED;

	    if (http)
	      httpEncryption(http, encryption);
#else
            fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	            argv[0]);
#endif /* HAVE_LIBSSL */
	    break;

        case 'a' : /* Cancel all jobs */
	    op = IPP_PURGE_JOBS;
	    break;

        case 'h' : /* Connect to host */
	    if (http != NULL)
	      httpClose(http);

	    if (argv[i][2] != '\0')
              cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("cancel: Error - expected hostname after \'-h\' option!\n", stderr);
		return (1);
              }
	      else
                cupsSetServer(argv[i]);
	    }
	    break;

        case 'u' : /* Username */
	    if (argv[i][2] != '\0')
	      cupsSetUser(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        fputs("cancel: Error - expected username after \'-u\' option!\n", stderr);
		return (1);
              }
	      else
		cupsSetUser(argv[i]);
	    }
	    break;

	default :
	    fprintf(stderr, "cancel: Unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else
    {
     /*
      * Cancel a job or printer...
      */

      if (num_dests == 0)
        num_dests = cupsGetDests(&dests);

      if (strcmp(argv[i], "-") == 0)
      {
       /*
        * Delete the current job...
	*/

        dest   = "";
	job_id = 0;
      }
      else if (cupsGetDest(argv[i], NULL, num_dests, dests) != NULL)
      {
       /*
        * Delete the current job on the named destination...
	*/

        dest   = argv[i];
	job_id = 0;
      }
      else if ((job = strrchr(argv[i], '-')) != NULL && isdigit(job[1]))
      {
       /*
        * Delete the specified job ID.
	*/

        dest   = NULL;
	op     = IPP_CANCEL_JOB;
        job_id = atoi(job + 1);
      }
      else if (isdigit(argv[i][0]))
      {
       /*
        * Delete the specified job ID.
	*/

        dest   = NULL;
	op     = IPP_CANCEL_JOB;
        job_id = atoi(argv[i]);
      }
      else
      {
       /*
        * Bad printer name!
	*/

        fprintf(stderr, "cancel: Unknown destination \"%s\"!\n", argv[i]);
      }

     /*
      * For Solaris LP compatibility, ignore a destination name after
      * cancelling a specific job ID...
      */

      if (job_id && (i + 1) < argc &&
          cupsGetDest(argv[i + 1], NULL, num_dests, dests) != NULL)
        i ++;

     /*
      * Open a connection to the server...
      */

      if (http == NULL)
	if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
	                               encryption)) == NULL)
	{
	  fputs("cancel: Unable to contact server!\n", stderr);
	  return (1);
	}

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
        snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", dest);
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

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser());

     /*
      * Do the request and get back a response...
      */

      if (op == IPP_PURGE_JOBS)
        response = cupsDoRequest(http, request, "/admin/");
      else
        response = cupsDoRequest(http, request, "/jobs/");

      if (response == NULL ||
          response->request.status.status_code > IPP_OK_CONFLICT)
      {
	fprintf(stderr, "cancel: %s failed: %s\n",
	        op == IPP_PURGE_JOBS ? "purge-jobs" : "cancel-job",
        	response ? ippErrorString(response->request.status.status_code) :
		           ippErrorString(cupsLastError()));

	if (response)
	  ippDelete(response);

	return (1);
      }

      ippDelete(response);
    }

  return (0);
}


/*
 * End of "$Id: cancel.c,v 1.19.2.7 2002/08/21 20:00:17 mike Exp $".
 */
