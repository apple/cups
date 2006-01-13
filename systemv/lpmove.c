/*
 * "$Id$"
 *
 *   "lpmove" command for the Common UNIX Printing System (CUPS).
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
 *   main()     - Parse options and move jobs.
 *   move_job() - Move a job.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cups/string.h>
#include <cups/cups.h>
#include <cups/i18n.h>
#include <cups/debug.h>


/*
 * Local functions...
 */

static void	move_job(http_t *, int, const char *);


/*
 * 'main()' - Parse options and show status information.
 */

int
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		i;			/* Looping var */
  http_t	*http;			/* Connection to server */
  const char	*job;			/* Job name */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dests;			/* Destinations */
  const char	*dest;			/* New destination */


  http      = NULL;
  job       = NULL;
  dest      = NULL;
  num_dests = 0;
  dests     = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);

	    if (http)
	      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
#else
            _cupsLangPrintf(stderr,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'h' : /* Connect to host */
	    if (http)
	    {
	      httpClose(http);
	      http = NULL;
	    }

	    if (argv[i][2] != '\0')
	      cupsSetServer(argv[i] + 2);
	    else
	    {
	      i ++;

	      if (i >= argc)
	      {
	        _cupsLangPuts(stderr,
		              _("Error: need hostname after \'-h\' option!\n"));
		return (1);
              }

	      cupsSetServer(argv[i]);
	    }
	    break;

	default :
	    _cupsLangPrintf(stderr, _("lpmove: Unknown option \'%c\'!\n"),
	                    argv[i][1]);
	    return (1);
      }
    else if (job == NULL)
    {
      if (num_dests == 0)
        num_dests = cupsGetDests(&dests);

      if ((job = strrchr(argv[i], '-')) != NULL &&
          cupsGetDest(argv[i], NULL, num_dests, dests) == NULL)
        job ++;
      else
        job = argv[i];
    }
    else if (dest == NULL)
      dest = argv[i];
    else
    {
      _cupsLangPrintf(stderr, _("lpmove: Unknown argument \'%s\'!\n"),
                      argv[i]);
      return (1);
    }

  if (job == NULL || dest == NULL)
  {
    _cupsLangPuts(stdout, _("Usage: lpmove job dest\n"));
    return (1);
  }

  if (!http)
  {
    http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

    if (http == NULL)
    {
      _cupsLangPrintf(stderr,
                      _("lpmove: Unable to connect to server: %s\n"),
		      strerror(errno));
      return (1);
    }
  }

  move_job(http, atoi(job), dest);

  return (0);
}


/*
 * 'move_job()' - Move a job.
 */

static void
move_job(http_t     *http,		/* I - HTTP connection to server */
         int        jobid,		/* I - Job ID */
	 const char *dest)		/* I - Destination */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  cups_lang_t	*language;		/* Default language */
  char		job_uri[HTTP_MAX_URI],	/* job-uri */
		printer_uri[HTTP_MAX_URI];
					/* job-printer-uri */


  if (http == NULL)
    return;

 /*
  * Build a CUPS_MOVE_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri
  *    job-printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_MOVE_JOB;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  snprintf(job_uri, sizeof(job_uri), "ipp://localhost/jobs/%d", jobid);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, job_uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  httpAssembleURIf(printer_uri, sizeof(printer_uri), "ipp", NULL, "localhost", 0,
                   "/printers/%s", dest);
  ippAddString(request, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri",
               NULL, printer_uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/jobs")) != NULL)
  {
    if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      _cupsLangPrintf(stderr, _("lpmove: move-job failed: %s\n"),
        	      ippErrorString(response->request.status.status_code));
      ippDelete(response);
      return;
    }

    ippDelete(response);
  }
  else
    _cupsLangPrintf(stderr, _("lpmove: move-job failed: %s\n"),
        	    ippErrorString(cupsLastError()));
}


/*
 * End of "$Id$".
 */
