/*
 * "$Id$"
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsCancelJob()        - Cancel a print job on the default server.
 *   cupsCancelJob2()       - Cancel or purge a print job.
 *   cupsCreateJob()        - Create an empty job.
 *   cupsFinishDocument()   - Finish sending a document.
 *   cupsFreeJobs()         - Free memory used by job data.
 *   cupsGetClasses()       - Get a list of printer classes from the default
 *                            server.
 *   cupsGetDefault()       - Get the default printer or class for the default
 *                            server.
 *   cupsGetDefault2()      - Get the default printer or class for the specified
 *                            server.
 *   cupsGetJobs()          - Get the jobs from the default server.
 *   cupsGetJobs2()         - Get the jobs from the specified server.
 *   cupsGetPPD()           - Get the PPD file for a printer on the default
 *                            server.
 *   cupsGetPPD2()          - Get the PPD file for a printer from the specified
 *                            server.
 *   cupsGetPPD3()          - Get the PPD file for a printer on the specified
 *                            server if it has changed.
 *   cupsGetPrinters()      - Get a list of printers from the default server.
 *   cupsGetServerPPD()     - Get an available PPD file from the server.
 *   cupsLastError()        - Return the last IPP status code.
 *   cupsLastErrorString()  - Return the last IPP status-message.
 *   cupsPrintFile()        - Print a file to a printer or class on the default
 *                            server.
 *   cupsPrintFile2()       - Print a file to a printer or class on the
 *                            specified server.
 *   cupsPrintFiles()       - Print one or more files to a printer or class on
 *                            the default server.
 *   cupsPrintFiles2()      - Print one or more files to a printer or class on
 *                            the specified server.
 *   cupsStartDocument()    - Add a document to a job created with
 *                            cupsCreateJob().
 *   _cupsConnect()         - Get the default server connection...
 *   cups_get_printer_uri() - Get the printer-uri-supported attribute for the
 *                            first printer in a class.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local functions...
 */

static int	cups_get_printer_uri(http_t *http, const char *name,
		                     char *host, int hostsize, int *port,
				     char *resource, int resourcesize,
				     int depth);


/*
 * 'cupsCancelJob()' - Cancel a print job on the default server.
 *
 * Use the *link cupsLastError* and *link cupsLastErrorString* functions to get
 * the cause of any failure.
 */

int					/* O - 1 on success, 0 on failure */
cupsCancelJob(const char *name,		/* I - Name of printer or class */
              int        job_id)	/* I - Job ID or 0 for the current job, -1 for all jobs */
{
  return (cupsCancelJob2(CUPS_HTTP_DEFAULT, name, job_id, 0)
              < IPP_REDIRECTION_OTHER_SITE);
}


/*
 * 'cupsCancelJob2()' - Cancel or purge a print job.
 *
 * Canceled jobs remain in the job history while purged jobs are removed
 * from the job history.
 *
 * Use the @link cupsLastError@ and @link cupsLastErrorString@ functions to get
 * the cause of any failure.
 *
 * @since CUPS 1.4@
 */

ipp_status_t				/* O - IPP status */
cupsCancelJob2(http_t     *http,	/* I - HTTP connection or @code CUPS_HTTP_DEFAULT@ */
               const char *name,	/* I - Name of printer or class */
               int        job_id,	/* I - Job ID or 0 for the current job, -1 for all jobs */
	       int        purge)	/* I - 1 to purge, 0 to cancel */
{
  char		uri[HTTP_MAX_URI];	/* Job/printer URI */
  ipp_t		*request;		/* IPP request */


 /*
  * Range check input...
  */

  if (job_id < -1 || (!name && job_id == 0))
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL));
    return (0);
  }

 /*
  * Connect to the default server as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (IPP_SERVICE_UNAVAILABLE);

 /*
  * Build an IPP_CANCEL_JOB or IPP_PURGE_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    job-uri or printer-uri + job-id
  *    requesting-user-name
  *    [purge-job] or [purge-jobs]
  */

  request = ippNewRequest(job_id < 0 ? IPP_PURGE_JOBS : IPP_CANCEL_JOB);

  if (name)
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", ippPort(), "/printers/%s", name);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL,
                 uri);
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
                  job_id);
  }
  else if (job_id > 0)
  {
    snprintf(uri, sizeof(uri), "ipp://localhost/jobs/%d", job_id);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);
  }

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  if (purge && job_id >= 0)
    ippAddBoolean(request, IPP_TAG_OPERATION, "purge-job", 1);
  else if (!purge && job_id < 0)
    ippAddBoolean(request, IPP_TAG_OPERATION, "purge-jobs", 0);

 /*
  * Do the request...
  */

  ippDelete(cupsDoRequest(http, request, "/jobs/"));

  return (cupsLastError());
}


/*
 * 'cupsCreateJob()' - Create an empty job.
 *
 * Submit files for printing to the job using the cupsStartDocument(),
 * cupsWriteRequestData(), and cupsFinishDocument() functions.
 *
 * @since CUPS 1.4@
 */

int					/* O - Job ID or 0 on error */
cupsCreateJob(
    http_t        *http,		/* I - HTTP connection or CUPS_HTTP_DEFAULT */
    const char    *name,		/* I - Printer or class name */
    const char    *title,		/* I - Title of job */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  char		printer_uri[1024],	/* Printer URI */
		resource[1024];		/* Printer resource */
  ipp_t		*request,		/* Create-Job request */
		*response;		/* Create-Job response */
  ipp_attribute_t *attr;		/* job-id attribute */
  int		job_id = 0;		/* job-id value */


  DEBUG_printf(("cupsCreateJob(http=%p, name=\"%s\", title=\"%s\", "
                "num_options=%d, options=%p)\n",
                http, name, title, num_options, options));

 /*
  * Range check input...
  */

  if (!name)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);
    return (0);
  }

 /*
  * Build a Create-Job request...
  */

  if ((request = ippNewRequest(IPP_CREATE_JOB)) == NULL)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);
    return (0);
  }

  httpAssembleURIf(HTTP_URI_CODING_ALL, printer_uri, sizeof(printer_uri), "ipp",
                   NULL, "localhost", ippPort(), "/printers/%s", name);
  snprintf(resource, sizeof(resource), "/printers/%s", name);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, printer_uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());
  if (title)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
                 title);
  cupsEncodeOptions(request, num_options, options);

 /*
  * Send the request and get the job-id...
  */

  response = cupsDoRequest(http, request, resource);

  if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
    job_id = attr->values[0].integer;

  ippDelete(response);

 /*
  * Return it...
  */

  return (job_id);
}


/*
 * 'cupsFinishDocument()' - Finish sending a document.
 *
 * @since CUPS 1.4@
 */

ipp_status_t				/* O - Status of document submission */
cupsFinishDocument(http_t     *http,	/* I - HTTP connection or CUPS_HTTP_DEFAULT */
                   const char *name)	/* I - Printer or class name */
{
  char	resource[1024];			/* Printer resource */


  snprintf(resource, sizeof(resource), "/printers/%s", name);

  ippDelete(cupsGetResponse(http, resource));

  return (cupsLastError());
}


/*
 * 'cupsFreeJobs()' - Free memory used by job data.
 */

void
cupsFreeJobs(int        num_jobs,	/* I - Number of jobs */
             cups_job_t *jobs)		/* I - Jobs */
{
  int		i;			/* Looping var */
  cups_job_t	*job;			/* Current job */


  if (num_jobs <= 0 || !jobs)
    return;

  for (i = num_jobs, job = jobs; i > 0; i --, job ++)
  {
    _cupsStrFree(job->dest);
    _cupsStrFree(job->user);
    _cupsStrFree(job->format);
    _cupsStrFree(job->title);
  }

  free(jobs);
}


/*
 * 'cupsGetClasses()' - Get a list of printer classes from the default server.
 *
 * This function is deprecated - use cupsGetDests() instead.
 *
 * @deprecated@
 */

int					/* O - Number of classes */
cupsGetClasses(char ***classes)		/* O - Classes */
{
  int		n;			/* Number of classes */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  char		**temp;			/* Temporary pointer */
  http_t	*http;			/* Connection to server */


  if (!classes)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    return (0);
  }

  *classes = NULL;

  if ((http = _cupsConnect()) == NULL)
    return (0);

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNewRequest(CUPS_GET_CLASSES);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

 /*
  * Do the request and get back a response...
  */

  n = 0;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  temp = malloc(sizeof(char *));
	else
	  temp = realloc(*classes, sizeof(char *) * (n + 1));

	if (temp == NULL)
	{
	 /*
	  * Ran out of memory!
	  */

          while (n > 0)
	  {
	    n --;
	    free((*classes)[n]);
	  }

	  free(*classes);
	  ippDelete(response);
	  return (0);
	}

        *classes = temp;
        temp[n]  = strdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }

  return (n);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class for the default server.
 *
 * This function returns the default printer or class as defined by
 * the LPDEST or PRINTER environment variables. If these environment
 * variables are not set, the server default destination is returned.
 * Applications should use the cupsGetDests() and cupsGetDest() functions
 * to get the user-defined default printer, as this function does not
 * support the lpoptions-defined default printer.
 */

const char *				/* O - Default printer or NULL */
cupsGetDefault(void)
{
 /*
  * Return the default printer...
  */

  return (cupsGetDefault2(CUPS_HTTP_DEFAULT));
}


/*
 * 'cupsGetDefault2()' - Get the default printer or class for the specified server.
 *
 * This function returns the default printer or class as defined by
 * the LPDEST or PRINTER environment variables. If these environment
 * variables are not set, the server default destination is returned.
 * Applications should use the cupsGetDests() and cupsGetDest() functions
 * to get the user-defined default printer, as this function does not
 * support the lpoptions-defined default printer.
 *
 * @since CUPS 1.1.21@
 */

const char *				/* O - Default printer or NULL */
cupsGetDefault2(http_t *http)		/* I - HTTP connection */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*var;			/* Environment variable */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * First see if the LPDEST or PRINTER environment variables are
  * set...  However, if PRINTER is set to "lp", ignore it to work
  * around a "feature" in most Linux distributions - the default
  * user login scripts set PRINTER to "lp"...
  */

  if ((var = getenv("LPDEST")) != NULL)
    return (var);
  else if ((var = getenv("PRINTER")) != NULL && strcmp(var, "lp") != 0)
    return (var);

 /*
  * Connect to the server as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (NULL);

 /*
  * Build a CUPS_GET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNewRequest(CUPS_GET_DEFAULT);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if ((attr = ippFindAttribute(response, "printer-name",
                                 IPP_TAG_NAME)) != NULL)
    {
      strlcpy(cg->def_printer, attr->values[0].string.text,
              sizeof(cg->def_printer));
      ippDelete(response);
      return (cg->def_printer);
    }

    ippDelete(response);
  }

  return (NULL);
}


/*
 * 'cupsGetJobs()' - Get the jobs from the default server.
 */

int					/* O - Number of jobs */
cupsGetJobs(cups_job_t **jobs,		/* O - Job data */
            const char *mydest,		/* I - NULL = all destinations,       *
	                                 *     otherwise show jobs for mydest */
            int        myjobs,		/* I - 0 = all users, 1 = mine */
	    int        completed)	/* I - -1 = show all, 0 = active, *
	                                 *     1 = completed jobs         */
{
 /*
  * Return the jobs...
  */

  return (cupsGetJobs2(CUPS_HTTP_DEFAULT, jobs, mydest, myjobs, completed));
}



/*
 * 'cupsGetJobs2()' - Get the jobs from the specified server.
 *
 * @since CUPS 1.1.21@
 */

int					/* O - Number of jobs */
cupsGetJobs2(http_t     *http,		/* I - HTTP connection or CUPS_HTTP_DEFAULT */
             cups_job_t **jobs,		/* O - Job data */
             const char *mydest,	/* I - NULL = all destinations,       *
	                                 *     otherwise show jobs for mydest */
             int        myjobs,		/* I - 0 = all users, 1 = mine */
	     int        completed)	/* I - -1 = show all, 0 = active, *
	                                 *     1 = completed jobs         */
{
  int		n;			/* Number of jobs */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_job_t	*temp;			/* Temporary pointer */
  int		id,			/* job-id */
		priority,		/* job-priority */
		size;			/* job-k-octets */
  ipp_jstate_t	state;			/* job-state */
  time_t	completed_time,		/* time-at-completed */
		creation_time,		/* time-at-creation */
		processing_time;	/* time-at-processing */
  const char	*dest,			/* job-printer-uri */
		*format,		/* document-format */
		*title,			/* job-name */
		*user;			/* job-originating-user-name */
  char		uri[HTTP_MAX_URI];	/* URI for jobs */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */
  static const char * const attrs[] =	/* Requested attributes */
		{
		  "job-id",
		  "job-priority",
		  "job-k-octets",
		  "job-state",
		  "time-at-completed",
		  "time-at-creation",
		  "time-at-processing",
		  "job-printer-uri",
		  "document-format",
		  "job-name",
		  "job-originating-user-name"
		};


 /*
  * Range check input...
  */

  if (!jobs)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    return (-1);
  }

 /*
  * Get the right URI...
  */

  if (mydest)
  {
    if (httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                         "localhost", 0, "/printers/%s", mydest) != HTTP_URI_OK)
    {
      _cupsSetError(IPP_INTERNAL_ERROR, NULL);

      return (-1);
    }
  }
  else
    strcpy(uri, "ipp://localhost/jobs");

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (-1);

 /*
  * Build an IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    which-jobs
  *    my-jobs
  *    requested-attributes
  */

  request = ippNewRequest(IPP_GET_JOBS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (myjobs)
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);

  if (completed > 0)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "completed");
  else if (completed < 0)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "all");

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(attrs) / sizeof(attrs[0]),
		NULL, attrs);

 /*
  * Do the request and get back a response...
  */

  n     = 0;
  *jobs = NULL;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (!attr)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      id              = 0;
      size            = 0;
      priority        = 50;
      state           = IPP_JOB_PENDING;
      user            = "unknown";
      dest            = NULL;
      format          = "application/octet-stream";
      title           = "untitled";
      creation_time   = 0;
      completed_time  = 0;
      processing_time = 0;

      while (attr && attr->group_tag == IPP_TAG_JOB)
      {
        if (!strcmp(attr->name, "job-id") &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  id = attr->values[0].integer;
        else if (!strcmp(attr->name, "job-state") &&
	         attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_jstate_t)attr->values[0].integer;
        else if (!strcmp(attr->name, "job-priority") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  priority = attr->values[0].integer;
        else if (!strcmp(attr->name, "job-k-octets") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer;
        else if (!strcmp(attr->name, "time-at-completed") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  completed_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "time-at-creation") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  creation_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "time-at-processing") &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  processing_time = attr->values[0].integer;
        else if (!strcmp(attr->name, "job-printer-uri") &&
	         attr->value_tag == IPP_TAG_URI)
	{
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;
        }
        else if (!strcmp(attr->name, "job-originating-user-name") &&
	         attr->value_tag == IPP_TAG_NAME)
	  user = attr->values[0].string.text;
        else if (!strcmp(attr->name, "document-format") &&
	         attr->value_tag == IPP_TAG_MIMETYPE)
	  format = attr->values[0].string.text;
        else if (!strcmp(attr->name, "job-name") &&
	         (attr->value_tag == IPP_TAG_TEXT ||
		  attr->value_tag == IPP_TAG_NAME))
	  title = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (!dest || !id)
      {
        if (!attr)
	  break;
	else
          continue;
      }

     /*
      * Allocate memory for the job...
      */

      if (n == 0)
        temp = malloc(sizeof(cups_job_t));
      else
	temp = realloc(*jobs, sizeof(cups_job_t) * (n + 1));

      if (!temp)
      {
       /*
        * Ran out of memory!
        */

        _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));

	cupsFreeJobs(n, *jobs);
	*jobs = NULL;

        ippDelete(response);

	return (-1);
      }

      *jobs = temp;
      temp  += n;
      n ++;

     /*
      * Copy the data over...
      */

      temp->dest            = _cupsStrAlloc(dest);
      temp->user            = _cupsStrAlloc(user);
      temp->format          = _cupsStrAlloc(format);
      temp->title           = _cupsStrAlloc(title);
      temp->id              = id;
      temp->priority        = priority;
      temp->state           = state;
      temp->size            = size;
      temp->completed_time  = completed_time;
      temp->creation_time   = creation_time;
      temp->processing_time = processing_time;

      if (!attr)
        break;
    }

    ippDelete(response);
  }

  if (n == 0 && cg->last_error >= IPP_BAD_REQUEST)
    return (-1);
  else
    return (n);
}


/*
 * 'cupsGetPPD()' - Get the PPD file for a printer on the default server.
 *
 * For classes, cupsGetPPD() returns the PPD file for the first printer
 * in the class.
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD(const char *name)		/* I - Printer name */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */
  time_t	modtime = 0;		/* Modification time */


 /*
  * Return the PPD file...
  */

  cg->ppd_filename[0] = '\0';

  if (cupsGetPPD3(CUPS_HTTP_DEFAULT, name, &modtime, cg->ppd_filename,
                  sizeof(cg->ppd_filename)) == HTTP_OK)
    return (cg->ppd_filename);
  else
    return (NULL);
}


/*
 * 'cupsGetPPD2()' - Get the PPD file for a printer from the specified server.
 *
 * For classes, cupsGetPPD2() returns the PPD file for the first printer
 * in the class.
 *
 * @since CUPS 1.1.21@
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD2(http_t     *http,		/* I - HTTP connection or CUPS_HTTP_DEFAULT */
            const char *name)		/* I - Printer name */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */
  time_t	modtime = 0;		/* Modification time */


  cg->ppd_filename[0] = '\0';

  if (cupsGetPPD3(http, name, &modtime, cg->ppd_filename,
                  sizeof(cg->ppd_filename)) == HTTP_OK)
    return (cg->ppd_filename);
  else
    return (NULL);
}


/*
 * 'cupsGetPPD3()' - Get the PPD file for a printer on the specified
 *                   server if it has changed.
 *
 * The "modtime" parameter contains the modification time of any
 * locally-cached content and is updated with the time from the PPD file on
 * the server.
 *
 * The "buffer" parameter contains the local PPD filename.  If it contains
 * the empty string, a new temporary file is created, otherwise the existing
 * file will be overwritten as needed.
 *
 * On success, HTTP_OK is returned for a new PPD file and HTTP_NOT_MODIFIED
 * if the existing PPD file is up-to-date.  Any other status is an error.
 */

http_status_t				/* O  - HTTP status */
cupsGetPPD3(http_t     *http,		/* I  - HTTP connection or CUPS_HTTP_DEFAULT */
            const char *name,		/* I  - Printer name */
	    time_t     *modtime,	/* IO - Modification time */
	    char       *buffer,		/* I  - Filename buffer */
	    size_t     bufsize)		/* I  - Size of filename buffer */
{
  int		http_port;		/* Port number */
  char		http_hostname[HTTP_MAX_HOST];
					/* Hostname associated with connection */
  http_t	*http2;			/* Alternate HTTP connection */
  int		fd;			/* PPD file */
  char		localhost[HTTP_MAX_URI],/* Local hostname */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource name */
  int		port;			/* Port number */
  http_status_t	status;			/* HTTP status from server */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsGetPPD3(http=%p, name=\"%s\", modtime=%p(%d), buffer=%p, "
                "bufsize=%d)\n", http, name ? name : "(null)", modtime,
		modtime ? *modtime : 0, buffer, (int)bufsize));

  if (!name)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, "No printer name!");
    return (HTTP_NOT_ACCEPTABLE);
  }

  if (!modtime)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, "No modification time!");
    return (HTTP_NOT_ACCEPTABLE);
  }

  if (!buffer || bufsize <= 1)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, "Bad filename buffer!");
    return (HTTP_NOT_ACCEPTABLE);
  }

 /*
  * Try finding a printer URI for this printer...
  */

  if (!http)
    http = _cupsConnect();

  if (!cups_get_printer_uri(http, name, hostname, sizeof(hostname), &port,
                            resource, sizeof(resource), 0))
    return (HTTP_NOT_FOUND);

  DEBUG_printf(("Printer hostname=\"%s\", port=%d\n", hostname, port));

 /*
  * Remap local hostname to localhost...
  */

  httpGetHostname(NULL, localhost, sizeof(localhost));

  DEBUG_printf(("Local hostname=\"%s\"\n", localhost));

  if (!strcasecmp(localhost, hostname))
    strcpy(hostname, "localhost");

 /*
  * Get the hostname and port number we are connected to...
  */

  httpGetHostname(http, http_hostname, sizeof(http_hostname));

#ifdef AF_INET6
  if (http->hostaddr->addr.sa_family == AF_INET6)
    http_port = ntohs(http->hostaddr->ipv6.sin6_port);
  else
#endif /* AF_INET6 */
  if (http->hostaddr->addr.sa_family == AF_INET)
    http_port = ntohs(http->hostaddr->ipv4.sin_port);
  else
    http_port = ippPort(); 

  DEBUG_printf(("Connection hostname=\"%s\", port=%d\n", http_hostname,
                http_port));

 /*
  * Reconnect to the correct server as needed...
  */

  if (!strcasecmp(http_hostname, hostname) && port == http_port)
    http2 = http;
  else if ((http2 = httpConnectEncrypt(hostname, port,
                                       cupsEncryption())) == NULL)
  {
    DEBUG_puts("Unable to connect to server!");

    return (HTTP_SERVICE_UNAVAILABLE);
  }

 /*
  * Get a temp file...
  */

  if (buffer[0])
    fd = open(buffer, O_CREAT | O_TRUNC | O_WRONLY, 0600);
  else
    fd = cupsTempFd(buffer, bufsize);

  if (fd < 0)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));

    if (http2 != http)
      httpClose(http2);

    return (HTTP_SERVER_ERROR);
  }

 /*
  * And send a request to the HTTP server...
  */

  strlcat(resource, ".ppd", sizeof(resource));

  if (*modtime > 0)
    httpSetField(http2, HTTP_FIELD_IF_MODIFIED_SINCE,
                 httpGetDateString(*modtime));

  status = cupsGetFd(http2, resource, fd);

  close(fd);

 /*
  * See if we actually got the file or an error...
  */

  if (status == HTTP_OK)
    *modtime = httpGetDateTime(httpGetField(http2, HTTP_FIELD_DATE));
  else if (status != HTTP_NOT_MODIFIED)
  {
    switch (status)
    {
      case HTTP_NOT_FOUND :
          _cupsSetError(IPP_NOT_FOUND, httpStatus(status));
	  break;

      case HTTP_UNAUTHORIZED :
          _cupsSetError(IPP_NOT_AUTHORIZED, httpStatus(status));
	  break;

      default :
	  DEBUG_printf(("HTTP error %d mapped to IPP_SERVICE_UNAVAILABLE!\n",
                	status));
	  _cupsSetError(IPP_SERVICE_UNAVAILABLE, httpStatus(status));
	  break;
    }

    unlink(cg->ppd_filename);
  }

  if (http2 != http)
    httpClose(http2);

 /*
  * Return the PPD file...
  */

  return (status);
}


/*
 * 'cupsGetPrinters()' - Get a list of printers from the default server.
 *
 * This function is deprecated - use cupsGetDests() instead.
 *
 * @deprecated@
 */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers)	/* O - Printers */
{
  int		n;			/* Number of printers */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  char		**temp;			/* Temporary pointer */
  http_t	*http;			/* Connection to server */


 /*
  * Range check input...
  */

  if (!printers)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    return (0);
  }

  *printers = NULL;

 /*
  * Try to connect to the server...
  */

  if ((http = _cupsConnect()) == NULL)
    return (0);

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type", 0);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type-mask", CUPS_PRINTER_CLASS);

 /*
  * Do the request and get back a response...
  */

  n = 0;

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  temp = malloc(sizeof(char *));
	else
	  temp = realloc(*printers, sizeof(char *) * (n + 1));

	if (temp == NULL)
	{
	 /*
	  * Ran out of memory!
	  */

	  while (n > 0)
	  {
	    n --;
	    free((*printers)[n]);
	  }

	  free(*printers);
	  ippDelete(response);
	  return (0);
	}

        *printers = temp;
        temp[n]   = strdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }

  return (n);
}


/*
 * 'cupsGetServerPPD()' - Get an available PPD file from the server.
 *
 * This function returns the named PPD file from the server.  The
 * list of available PPDs is provided by the IPP CUPS_GET_PPDS
 * operation.
 *
 * You must remove (unlink) the PPD file when you are finished with
 * it. The PPD filename is stored in a static location that will be
 * overwritten on the next call to cupsGetPPD(), cupsGetPPD2(), or
 * cupsGetServerPPD().
 *
 * @since CUPS 1.3@
 */

char *					/* O - Name of PPD file or NULL on error */
cupsGetServerPPD(http_t     *http,	/* I - HTTP connection or CUPS_HTTP_DEFAULT */
                 const char *name)	/* I - Name of PPD file ("ppd-name") */
{
  int			fd;		/* PPD file descriptor */
  ipp_t			*request;	/* IPP request */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */


 /*
  * Range check input...
  */

  if (!name)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, "No PPD name!");

    return (NULL);
  }

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (NULL);

 /*
  * Get a temp file...
  */

  if ((fd = cupsTempFd(cg->ppd_filename, sizeof(cg->ppd_filename))) < 0)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    _cupsSetError(IPP_INTERNAL_ERROR, strerror(errno));

    return (NULL);
  }

 /*
  * Get the PPD file...
  */

  request = ippNewRequest(CUPS_GET_PPD);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "ppd-name", NULL,
               name);

  ippDelete(cupsDoIORequest(http, request, "/", -1, fd));

  close(fd);

  if (cupsLastError() != IPP_OK)
  {
    unlink(cg->ppd_filename);
    return (NULL);
  }
  else
    return (cg->ppd_filename);
}


/*
 * 'cupsLastError()' - Return the last IPP status code.
 */

ipp_status_t				/* O - IPP status code from last request */
cupsLastError(void)
{
  return (_cupsGlobals()->last_error);
}


/*
 * 'cupsLastErrorString()' - Return the last IPP status-message.
 *
 * @since CUPS 1.2@
 */

const char *				/* O - status-message text from last request */
cupsLastErrorString(void)
{
  return (_cupsGlobals()->last_status_message);
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class on the default server.
 */

int					/* O - Job ID */
cupsPrintFile(const char    *name,	/* I - Printer or class name */
              const char    *filename,	/* I - File to print */
	      const char    *title,	/* I - Title of job */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsPrintFile(name=\"%s\", filename=\"%s\", "
                "title=\"%s\", num_options=%d, options=%p)\n",
                name, filename, title, num_options, options));

  return (cupsPrintFiles2(CUPS_HTTP_DEFAULT, name, 1, &filename, title,
                          num_options, options));
}


/*
 * 'cupsPrintFile2()' - Print a file to a printer or class on the specified server.
 *
 * @since CUPS 1.1.21@
 */

int					/* O - Job ID */
cupsPrintFile2(
    http_t        *http,		/* I - HTTP connection */
    const char    *name,		/* I - Printer or class name */
    const char    *filename,		/* I - File to print */
    const char    *title,		/* I - Title of job */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  DEBUG_printf(("cupsPrintFile2(http=%p, name=\"%s\", filename=\"%s\", "
                "title=\"%s\", num_options=%d, options=%p)\n",
                http, name, filename, title, num_options, options));

  return (cupsPrintFiles2(http, name, 1, &filename, title, num_options,
                          options));
}


/*
 * 'cupsPrintFiles()' - Print one or more files to a printer or class on the
 *                      default server.
 */

int					/* O - Job ID */
cupsPrintFiles(
    const char    *name,		/* I - Printer or class name */
    int           num_files,		/* I - Number of files */
    const char    **files,		/* I - File(s) to print */
    const char    *title,		/* I - Title of job */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  DEBUG_printf(("cupsPrintFiles(name=\"%s\", num_files=%d, "
                "files=%p, title=\"%s\", num_options=%d, options=%p)\n",
                name, num_files, files, title, num_options, options));


 /*
  * Print the file(s)...
  */

  return (cupsPrintFiles2(CUPS_HTTP_DEFAULT, name, num_files, files, title,
                          num_options, options));
}


/*
 * 'cupsPrintFiles2()' - Print one or more files to a printer or class on the
 *                       specified server.
 *
 * @since CUPS 1.1.21@
 */

int					/* O - Job ID */
cupsPrintFiles2(
    http_t        *http,		/* I - HTTP connection or CUPS_HTTP_DEFAULT */
    const char    *name,		/* I - Printer or class name */
    int           num_files,		/* I - Number of files */
    const char    **files,		/* I - File(s) to print */
    const char    *title,		/* I - Title of job */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  int		i;			/* Looping var */
  int		job_id;			/* New job ID */
  const char	*docname;		/* Basename of current filename */
  const char	*format;		/* Document format */
  cups_file_t	*fp;			/* Current file */
  char		buffer[8192];		/* Copy buffer */
  ssize_t	bytes;			/* Bytes in buffer */
  http_status_t	status;			/* Status of write */


  DEBUG_printf(("cupsPrintFiles2(http=%p, name=\"%s\", num_files=%d, "
                "files=%p, title=\"%s\", num_options=%d, options=%p)\n",
                http, name, num_files, files, title, num_options, options));

 /*
  * Range check input...
  */

  if (!name || num_files < 1 || !files)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    return (0);
  }

 /*
  * Create the print job...
  */

  if ((job_id = cupsCreateJob(http, name, title, num_options, options)) == 0)
    return (0);

 /*
  * Send each of the files...
  */

  if (cupsGetOption("raw", num_options, options))
    format = CUPS_FORMAT_RAW;
  else if ((format = cupsGetOption("document-format", num_options,
				   options)) == NULL)
    format = CUPS_FORMAT_AUTO;

  for (i = 0; i < num_files; i ++)
  {
   /*
    * Start the next file...
    */

    if ((docname = strrchr(files[i], '/')) != NULL)
      docname ++;
    else
      docname = files[i];

    if ((fp = cupsFileOpen(files[i], "rb")) == NULL)
    {
     /*
      * Unable to open print file, cancel the job and return...
      */

      cupsCancelJob2(http, name, job_id, 0);
      return (0);
    }

    status = cupsStartDocument(http, name, job_id, docname, format,
                               i == (num_files - 1));

    while (status == HTTP_CONTINUE &&
           (bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
      status = cupsWriteRequestData(http, buffer, bytes);

    cupsFileClose(fp);

    if (status != HTTP_CONTINUE || cupsFinishDocument(http, name) != IPP_OK)
    {
     /*
      * Unable to queue, cancel the job and return...
      */

      cupsCancelJob2(http, name, job_id, 0);
      return (0);
    }
  }

  return (job_id);
}


/*
 * 'cupsStartDocument()' - Add a document to a job created with cupsCreateJob().
 *
 * Use cupsWriteRequestData() to write data for the document and
 * cupsFinishDocument() to finish the document and get the submission status.
 *
 * The MIME type constants CUPS_FORMAT_AUTO, CUPS_FORMAT_PDF,
 * CUPS_FORMAT_POSTSCRIPT, CUPS_FORMAT_RAW, and CUPS_FORMAT_TEXT are provided
 * for the "format" argument, although any supported MIME type string can be
 * supplied.
 *
 * @since CUPS 1.4@
 */

http_status_t				/* O - HTTP status of request */
cupsStartDocument(
    http_t     *http,			/* I - HTTP connection or CUPS_HTTP_DEFAULT */
    const char *name,			/* I - Printer or class name */
    int        job_id,			/* I - Job ID from cupsCreateJob() */
    const char *docname,		/* I - Name of document */
    const char *format,			/* I - MIME type or CUPS_FORMAT_foo */
    int        last_document)		/* I - 1 for last document in job, 0 otherwise */
{
  char		resource[1024],		/* Resource for destinatio */
		printer_uri[1024];	/* Printer URI */
  ipp_t		*request;		/* Send-Document request */
  http_status_t	status;			/* HTTP status */


 /*
  * Create a Send-Document request...
  */

  if ((request = ippNewRequest(IPP_SEND_DOCUMENT)) == NULL)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, NULL);
    return (0);
  }

  httpAssembleURIf(HTTP_URI_CODING_ALL, printer_uri, sizeof(printer_uri), "ipp",
                   NULL, "localhost", ippPort(), "/printers/%s", name);
  snprintf(resource, sizeof(resource), "/printers/%s", name);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, printer_uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());
  if (docname)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "document-name",
                 NULL, docname);
  if (format)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
                 "document-format", NULL, format);
  ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", last_document);

 /*
  * Send and delete the request, then return the status...
  */

  status = cupsSendRequest(http, request, resource, CUPS_LENGTH_VARIABLE);

  ippDelete(request);

  return (status);
}


/*
 * '_cupsConnect()' - Get the default server connection...
 */

http_t *				/* O - HTTP connection */
_cupsConnect(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if we are connected to the same server...
  */

  if (cg->http)
  {
    int	port;				/* Port for connection */


   /*
    * Get the port associated with the current connection...
    */

#ifdef AF_INET6
    if (cg->http->hostaddr->addr.sa_family == AF_INET6)
      port = ntohs(cg->http->hostaddr->ipv6.sin6_port);
    else
#endif /* AF_INET6 */
    if (cg->http->hostaddr->addr.sa_family == AF_INET)
      port = ntohs(cg->http->hostaddr->ipv4.sin_port);
    else
      port = cg->ipp_port;

   /*
    * Compare the connection hostname, port, and encryption settings to
    * the cached defaults; these were initialized the first time we
    * connected...
    */

    if (strcmp(cg->http->hostname, cg->server) || cg->ipp_port != port ||
        (cg->http->encryption != cg->encryption &&
	 cg->http->encryption == HTTP_ENCRYPT_NEVER))
    {
     /*
      * Need to close the current connection because something has changed...
      */

      httpClose(cg->http);
      cg->http = NULL;
    }
  }

 /*
  * (Re)connect as needed...
  */

  if (!cg->http)
  {
    if ((cg->http = httpConnectEncrypt(cupsServer(), ippPort(),
                                       cupsEncryption())) == NULL)
      _cupsSetError(IPP_SERVICE_UNAVAILABLE,
                    errno ? strerror(errno) : "Unable to connect to host.");
  }

 /*
  * Return the cached connection...
  */

  return (cg->http);
}


/*
 * 'cups_get_printer_uri()' - Get the printer-uri-supported attribute for the first printer in a class.
 */

static int				/* O - 1 on success, 0 on failure */
cups_get_printer_uri(
    http_t     *http,			/* I - HTTP connection */
    const char *name,			/* I - Name of printer or class */
    char       *host,			/* I - Hostname buffer */
    int        hostsize,		/* I - Size of hostname buffer */
    int        *port,			/* O - Port number */
    char       *resource,		/* I - Resource buffer */
    int        resourcesize,		/* I - Size of resource buffer */
    int        depth)			/* I - Depth of query */
{
  int		i;			/* Looping var */
  int		http_port;		/* Port number */
  http_t	*http2;			/* Alternate HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Current attribute */
  char		uri[HTTP_MAX_URI],	/* printer-uri attribute */
		scheme[HTTP_MAX_URI],	/* Scheme name */
		username[HTTP_MAX_URI],	/* Username:password */
		classname[255],		/* Temporary class name */
		http_hostname[HTTP_MAX_HOST];
					/* Hostname associated with connection */
  static const char * const requested_attrs[] =
		{			/* Requested attributes */
		  "printer-uri-supported",
		  "printer-type",
		  "member-uris"
		};


  DEBUG_printf(("cups_get_printer_uri(http=%p, name=\"%s\", host=%p, "
                "hostsize=%d, resource=%p, resourcesize=%d, depth=%d)\n",
		http, name ? name : "(null)", host, hostsize,
		resource, resourcesize, depth));

 /*
  * Setup the printer URI...
  */

  if (httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                       "localhost", 0, "/printers/%s", name) != HTTP_URI_OK)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, "Unable to create printer-uri!");

    *host     = '\0';
    *resource = '\0';

    return (0);
  }

  DEBUG_printf(("cups_get_printer_uri: printer-uri=\"%s\"\n", uri));

 /*
  * Get the hostname and port number we are connected to...
  */

  httpGetHostname(http, http_hostname, sizeof(http_hostname));

#ifdef AF_INET6
  if (http->hostaddr->addr.sa_family == AF_INET6)
    http_port = ntohs(http->hostaddr->ipv6.sin6_port);
  else
#endif /* AF_INET6 */
  if (http->hostaddr->addr.sa_family == AF_INET)
    http_port = ntohs(http->hostaddr->ipv4.sin_port);
  else
    http_port = ippPort(); 

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requested-attributes
  */

  request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                "requested-attributes",
		sizeof(requested_attrs) / sizeof(requested_attrs[0]),
		NULL, requested_attrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    if ((attr = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
    {
     /*
      * Get the first actual printer name in the class...
      */

      for (i = 0; i < attr->num_values; i ++)
      {
	httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[i].string.text,
	                scheme, sizeof(scheme), username, sizeof(username),
			host, hostsize, port, resource, resourcesize);
	if (!strncmp(resource, "/printers/", 10))
	{
	 /*
	  * Found a printer!
	  */

          ippDelete(response);

	  return (1);
	}
      }

     /*
      * No printers in this class - try recursively looking for a printer,
      * but not more than 3 levels deep...
      */

      if (depth < 3)
      {
	for (i = 0; i < attr->num_values; i ++)
	{
	  httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[i].string.text,
	                  scheme, sizeof(scheme), username, sizeof(username),
			  host, hostsize, port, resource, resourcesize);
	  if (!strncmp(resource, "/classes/", 9))
	  {
	   /*
	    * Found a class!  Connect to the right server...
	    */

	    if (!strcasecmp(http_hostname, host) && *port == http_port)
	      http2 = http;
	    else if ((http2 = httpConnectEncrypt(host, *port,
						 cupsEncryption())) == NULL)
	    {
	      DEBUG_puts("Unable to connect to server!");

	      continue;
	    }

           /*
	    * Look up printers on that server...
	    */

            strlcpy(classname, resource + 9, sizeof(classname));

            cups_get_printer_uri(http2, classname, host, hostsize, port,
	                         resource, resourcesize, depth + 1);

           /*
	    * Close the connection as needed...
	    */

	    if (http2 != http)
	      httpClose(http2);

            if (*host)
	      return (1);
	  }
	}
      }
    }
    else if ((attr = ippFindAttribute(response, "printer-uri-supported",
                                      IPP_TAG_URI)) != NULL)
    {
      httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[0].string.text,
                      scheme, sizeof(scheme), username, sizeof(username),
		      host, hostsize, port, resource, resourcesize);
      ippDelete(response);

      return (1);
    }

    ippDelete(response);
  }

  if (cupsLastError() != IPP_NOT_FOUND)
    _cupsSetError(IPP_INTERNAL_ERROR, "No printer-uri found!");

  *host     = '\0';
  *resource = '\0';

  return (0);
}


/*
 * End of "$Id$".
 */
