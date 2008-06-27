/*
 * "$Id: lpmove.c 7219 2008-01-14 22:00:02Z mike $"
 *
 *   "lpmove" command for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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

static int	move_job(http_t *http, const char *src, int jobid,
		         const char *dest);


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
  int		jobid;			/* Job ID */
  int		num_dests;		/* Number of destinations */
  cups_dest_t	*dests;			/* Destinations */
  const char	*src,			/* Original queue */
		*dest;			/* New destination */


  _cupsSetLocale(argv);

  dest      = NULL;
  dests     = NULL;
  job       = NULL;
  jobid     = 0;
  num_dests = 0;
  src       = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);

#else
            _cupsLangPrintf(stderr,
	                    _("%s: Sorry, no encryption support compiled in!\n"),
	                    argv[0]);
#endif /* HAVE_SSL */
	    break;

        case 'h' : /* Connect to host */
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
    else if (!jobid && !src)
    {
      if (num_dests == 0)
        num_dests = cupsGetDests(&dests);

      if ((job = strrchr(argv[i], '-')) != NULL &&
          cupsGetDest(argv[i], NULL, num_dests, dests) == NULL)
        jobid = atoi(job + 1);
      else if (isdigit(argv[i][0] & 255) &&
               !cupsGetDest(argv[i], NULL, num_dests, dests))
        jobid = atoi(argv[i]);
      else
        src = argv[i];
    }
    else if (dest == NULL)
      dest = argv[i];
    else
    {
      _cupsLangPrintf(stderr, _("lpmove: Unknown argument \'%s\'!\n"),
                      argv[i]);
      return (1);
    }

  if ((!jobid && !src) || !dest)
  {
    _cupsLangPuts(stdout, _("Usage: lpmove job/src dest\n"));
    return (1);
  }

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

  if (http == NULL)
  {
    _cupsLangPrintf(stderr,
		    _("lpmove: Unable to connect to server: %s\n"),
		    strerror(errno));
    return (1);
  }

  return (move_job(http, src, jobid, dest));
}


/*
 * 'move_job()' - Move a job.
 */

static int				/* O - 0 on success, 1 on error */
move_job(http_t     *http,		/* I - HTTP connection to server */
         const char *src,		/* I - Source queue */
         int        jobid,		/* I - Job ID */
	 const char *dest)		/* I - Destination queue */
{
  ipp_t	*request;			/* IPP Request */
  char	job_uri[HTTP_MAX_URI],		/* job-uri */
	printer_uri[HTTP_MAX_URI];	/* job-printer-uri */


  if (!http)
    return (1);

 /*
  * Build a CUPS_MOVE_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri/printer-uri
  *    job-printer-uri
  *    requesting-user-name
  */

  request = ippNewRequest(CUPS_MOVE_JOB);

  if (jobid)
  {
    snprintf(job_uri, sizeof(job_uri), "ipp://localhost/jobs/%d", jobid);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL,
        	 job_uri);
  }
  else
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, job_uri, sizeof(job_uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s", src);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
        	 job_uri);
  }

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  httpAssembleURIf(HTTP_URI_CODING_ALL, printer_uri, sizeof(printer_uri),
                   "ipp", NULL, "localhost", 0, "/printers/%s", dest);
  ippAddString(request, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri",
               NULL, printer_uri);

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/jobs"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    _cupsLangPrintf(stderr, "lpmove: %s\n", cupsLastErrorString());
    return (1);
  }
  else
    return (0);
}


/*
 * End of "$Id: lpmove.c 7219 2008-01-14 22:00:02Z mike $".
 */
