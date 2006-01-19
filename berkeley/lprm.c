/*
 * "$Id$"
 *
 *   "lprm" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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

#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/string.h>


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
  const char	*dest;		/* Destination printer */
  char		*instance;	/* Pointer to instance name */
  char		uri[1024];	/* Printer or job URI */
  ipp_t		*request;	/* IPP request */
  ipp_t		*response;	/* IPP response */
  ipp_op_t	op;		/* Operation */
  cups_lang_t	*language;	/* Language */
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests;		/* Destinations */
  http_encryption_t encryption;	/* Encryption? */


 /*
  * Setup to cancel individual print jobs...
  */

  op         = IPP_CANCEL_JOB;
  job_id     = 0;
  dest       = NULL;
  response   = NULL;
  http       = NULL;
  encryption = cupsEncryption();
  language   = cupsLangDefault();
  num_dests  = cupsGetDests(&dests);

  for (i = 0; i < num_dests; i ++)
    if (dests[i].is_default)
      dest = dests[i].name;

 /*
  * Open a connection to the server...
  */

  if ((http = httpConnectEncrypt(cupsServer(), ippPort(), encryption)) == NULL)
  {
    _cupsLangPuts(stderr, _("lprm: Unable to contact server!\n"));
    cupsFreeDests(num_dests, dests);
    return (1);
  }

 /*
  * Process command-line arguments...
  */

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-' && argv[i][1] != '\0')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    encryption = HTTP_ENCRYPT_REQUIRED;

	    httpEncryption(http, encryption);
#else
            _cupsLangPrintf(stderr,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'P' : /* Cancel jobs on a printer */
	    if (argv[i][2])
	      dest = argv[i] + 2;
	    else
	    {
	      i ++;
	      dest = argv[i];
	    }

	    if ((instance = strchr(dest, '/')) != NULL)
	      *instance = '\0';

	    if (cupsGetDest(dest, NULL, num_dests, dests) == NULL)
	    {
	      _cupsLangPrintf(stderr,
	                      _("%s: Error - unknown destination \"%s\"!\n"),
			      argv[0], dest);
              cupsFreeDests(num_dests, dests);
	      httpClose(http);
	      return(1);
	    }
	    break;

        case 'U' : /* Username */
	    if (argv[i][2] != '\0')
	      cupsSetUser(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        _cupsLangPrintf(stderr,
		                _("%s: Error - expected username after "
				  "\'-U\' option!\n"),
		        	argv[0]);
	        return (1);
	      }

              cupsSetUser(argv[i]);
	    }
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
	        _cupsLangPrintf(stderr,
		        	_("%s: Error - expected hostname after "
			          "\'-h\' option!\n"),
				argv[0]);
		return (1);
              }
	      else
                cupsSetServer(argv[i]);
	    }
	    break;

	default :
	    _cupsLangPrintf(stderr,
	                    _("%s: Error - unknown option \'%c\'!\n"),
			    argv[0], argv[i][1]);
            cupsFreeDests(num_dests, dests);
	    httpClose(http);
	    return (1);
      }
    else
    {
     /*
      * Cancel a job or printer...
      */

      if (isdigit(argv[i][0] & 255) &&
          cupsGetDest(argv[i], NULL, num_dests, dests) == NULL)
      {
        dest   = NULL;
	op     = IPP_CANCEL_JOB;
        job_id = atoi(argv[i]);
      }
      else if (!strcmp(argv[i], "-"))
      {
       /*
        * Cancel all jobs
        */

        op = IPP_PURGE_JOBS;
      }
      else
      {
        dest   = argv[i];
        job_id = 0;
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

      request = ippNewRequest(op);

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

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser());

     /*
      * Do the request and get back a response...
      */

      if (op == IPP_PURGE_JOBS)
        response = cupsDoRequest(http, request, "/admin/");
      else
        response = cupsDoRequest(http, request, "/jobs/");

      ippDelete(response);

      if (cupsLastError() > IPP_OK_CONFLICT)
      {
        _cupsLangPrintf(stderr, "%s: %s\n", argv[0], cupsLastErrorString());

        cupsFreeDests(num_dests, dests);
        httpClose(http);
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
      _cupsLangPrintf(stderr, "%s: %s\n", argv[0], cupsLastErrorString());
      cupsFreeDests(num_dests, dests);
      httpClose(http);
      return (1);
    }

  cupsFreeDests(num_dests, dests);
  httpClose(http);

  return (0);
}


/*
 * End of "$Id$".
 */
