/*
 * "$Id$"
 *
 *   "cancel" command for the Common UNIX Printing System (CUPS).
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
#include <cups/i18n.h>


/*
 * 'main()' - Parse options and cancel jobs.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* HTTP connection to server */
  int		i;			/* Looping var */
  int		job_id;			/* Job ID */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dests;			/* Destinations */
  char		*dest,			/* Destination printer */
		*job,			/* Job ID pointer */
		*user;			/* Cancel jobs for a user */
  int		purge;			/* Purge or cancel jobs? */
  char		uri[1024];		/* Printer or job URI */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_op_t	op;			/* Operation */
  cups_lang_t	*language;		/* Language */


 /*
  * Setup to cancel individual print jobs...
  */

  op         = IPP_CANCEL_JOB;
  purge      = 0;
  job_id     = 0;
  dest       = NULL;
  user       = NULL;
  http       = NULL;
  num_dests  = 0;
  dests      = NULL;
  language   = cupsLangDefault();


 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1])
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);

	    if (http)
	      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr, language,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'a' : /* Cancel all jobs */
	    purge = 1;
	    op    = IPP_PURGE_JOBS;
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
	        _cupsLangPuts(stderr, language,
		              _("cancel: Error - expected hostname after "
			        "\'-h\' option!\n"));
		return (1);
              }
	      else
                cupsSetServer(argv[i]);
	    }
	    break;

        case 'u' : /* Username */
	    op = IPP_PURGE_JOBS;

	    if (argv[i][2] != '\0')
	      user = argv[i] + 2;
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr, language,
		              _("cancel: Error - expected username after "
			        "\'-u\' option!\n"));
		return (1);
              }
	      else
		user = argv[i];
	    }
	    break;

	default :
	    _cupsLangPrintf(stderr, language,
	                    _("cancel: Unknown option \'%c\'!\n"), argv[i][1]);
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
      else if ((job = strrchr(argv[i], '-')) != NULL && isdigit(job[1] & 255))
      {
       /*
        * Delete the specified job ID.
	*/

        dest   = NULL;
	op     = IPP_CANCEL_JOB;
        job_id = atoi(job + 1);
      }
      else if (isdigit(argv[i][0] & 255))
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

        _cupsLangPrintf(stderr, language,
	                _("cancel: Unknown destination \"%s\"!\n"), argv[i]);
	return (1);
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
	                               cupsEncryption())) == NULL)
	{
	  _cupsLangPuts(stderr, language,
	                _("cancel: Unable to contact server!\n"));
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

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
              	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                   "attributes-natural-language", NULL, language->language);

      if (dest)
      {
	httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                	 "/printers/%s", dest);
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

      if (user)
      {
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                     "requesting-user-name", NULL, user);
	ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
      }
      else
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                     "requesting-user-name", NULL, cupsUser());

      if (op == IPP_PURGE_JOBS)
	ippAddBoolean(request, IPP_TAG_OPERATION, "purge-jobs", purge);

     /*
      * Do the request and get back a response...
      */

      if (op == IPP_PURGE_JOBS && (!user || strcasecmp(user, cupsUser())))
        response = cupsDoRequest(http, request, "/admin/");
      else
        response = cupsDoRequest(http, request, "/jobs/");

      if (response == NULL ||
          response->request.status.status_code > IPP_OK_CONFLICT)
      {
	_cupsLangPrintf(stderr, language, _("cancel: %s failed: %s\n"),
	        	op == IPP_PURGE_JOBS ? "purge-jobs" : "cancel-job",
        		response ? ippErrorString(response->request.status.status_code) :
		        	   ippErrorString(cupsLastError()));

	if (response)
	  ippDelete(response);

	return (1);
      }

      ippDelete(response);
    }

  if (num_dests == 0 && op == IPP_PURGE_JOBS)
  {
   /*
    * Open a connection to the server...
    */

    if (http == NULL)
      if ((http = httpConnectEncrypt(cupsServer(), ippPort(),
	                             cupsEncryption())) == NULL)
      {
	_cupsLangPuts(stderr, language, _("cancel: Unable to contact server!\n"));
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

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
	         "printer-uri", NULL, "ipp://localhost/printers/");

    if (user)
    {
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, user);
      ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);
    }
    else
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser());

    ippAddBoolean(request, IPP_TAG_OPERATION, "purge-jobs", purge);

   /*
    * Do the request and get back a response...
    */

    response = cupsDoRequest(http, request, "/admin/");

    if (response == NULL ||
        response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, language, _("cancel: %s failed: %s\n"),
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
 * End of "$Id$".
 */
