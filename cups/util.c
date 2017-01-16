/*
 * Printing utilities for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
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
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * 'cupsCancelJob()' - Cancel a print job on the default server.
 *
 * Pass @code CUPS_JOBID_ALL@ to cancel all jobs or @code CUPS_JOBID_CURRENT@
 * to cancel the current job on the named destination.
 *
 * Use the @link cupsLastError@ and @link cupsLastErrorString@ functions to get
 * the cause of any failure.
 */

int					/* O - 1 on success, 0 on failure */
cupsCancelJob(const char *name,		/* I - Name of printer or class */
              int        job_id)	/* I - Job ID, @code CUPS_JOBID_CURRENT@ for the current job, or @code CUPS_JOBID_ALL@ for all jobs */
{
  return (cupsCancelJob2(CUPS_HTTP_DEFAULT, name, job_id, 0)
              < IPP_STATUS_REDIRECTION_OTHER_SITE);
}


/*
 * 'cupsCancelJob2()' - Cancel or purge a print job.
 *
 * Canceled jobs remain in the job history while purged jobs are removed
 * from the job history.
 *
 * Pass @code CUPS_JOBID_ALL@ to cancel all jobs or @code CUPS_JOBID_CURRENT@
 * to cancel the current job on the named destination.
 *
 * Use the @link cupsLastError@ and @link cupsLastErrorString@ functions to get
 * the cause of any failure.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

ipp_status_t				/* O - IPP status */
cupsCancelJob2(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
               const char *name,	/* I - Name of printer or class */
               int        job_id,	/* I - Job ID, @code CUPS_JOBID_CURRENT@ for the current job, or @code CUPS_JOBID_ALL@ for all jobs */
	       int        purge)	/* I - 1 to purge, 0 to cancel */
{
  char		uri[HTTP_MAX_URI];	/* Job/printer URI */
  ipp_t		*request;		/* IPP request */


 /*
  * Range check input...
  */

  if (job_id < -1 || (!name && job_id == 0))
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (0);
  }

 /*
  * Connect to the default server as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (IPP_STATUS_ERROR_SERVICE_UNAVAILABLE);

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

  request = ippNewRequest(job_id < 0 ? IPP_OP_PURGE_JOBS : IPP_OP_CANCEL_JOB);

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
 * 'cupsCreateJob()' - Create an empty job for streaming.
 *
 * Use this function when you want to stream print data using the
 * @link cupsStartDocument@, @link cupsWriteRequestData@, and
 * @link cupsFinishDocument@ functions.  If you have one or more files to
 * print, use the @link cupsPrintFile2@ or @link cupsPrintFiles2@ function
 * instead.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

int					/* O - Job ID or 0 on error */
cupsCreateJob(
    http_t        *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char    *name,		/* I - Destination name */
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


  DEBUG_printf(("cupsCreateJob(http=%p, name=\"%s\", title=\"%s\", num_options=%d, options=%p)", (void *)http, name, title, num_options, (void *)options));

 /*
  * Range check input...
  */

  if (!name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (0);
  }

 /*
  * Build a Create-Job request...
  */

  if ((request = ippNewRequest(IPP_OP_CREATE_JOB)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), 0);
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
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_OPERATION);
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_JOB);
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_SUBSCRIPTION);

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
 * The document must have been started using @link cupsStartDocument@.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

ipp_status_t				/* O - Status of document submission */
cupsFinishDocument(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                   const char *name)	/* I - Destination name */
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
 * This function is deprecated and no longer returns a list of printer
 * classes - use @link cupsGetDests@ instead.
 *
 * @deprecated@
 */

int					/* O - Number of classes */
cupsGetClasses(char ***classes)		/* O - Classes */
{
  if (classes)
    *classes = NULL;

  return (0);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class for the default server.
 *
 * This function returns the default printer or class as defined by
 * the LPDEST or PRINTER environment variables. If these environment
 * variables are not set, the server default destination is returned.
 * Applications should use the @link cupsGetDests@ and @link cupsGetDest@
 * functions to get the user-defined default printer, as this function does
 * not support the lpoptions-defined default printer.
 */

const char *				/* O - Default printer or @code NULL@ */
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
 * Applications should use the @link cupsGetDests@ and @link cupsGetDest@
 * functions to get the user-defined default printer, as this function does
 * not support the lpoptions-defined default printer.
 *
 * @since CUPS 1.1.21/macOS 10.4@
 */

const char *				/* O - Default printer or @code NULL@ */
cupsGetDefault2(http_t *http)		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if we have a user default printer set...
  */

  if (_cupsUserDefault(cg->def_printer, sizeof(cg->def_printer)))
    return (cg->def_printer);

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

  request = ippNewRequest(IPP_OP_CUPS_GET_DEFAULT);

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
 *
 * A "whichjobs" value of @code CUPS_WHICHJOBS_ALL@ returns all jobs regardless
 * of state, while @code CUPS_WHICHJOBS_ACTIVE@ returns jobs that are
 * pending, processing, or held and @code CUPS_WHICHJOBS_COMPLETED@ returns
 * jobs that are stopped, canceled, aborted, or completed.
 */

int					/* O - Number of jobs */
cupsGetJobs(cups_job_t **jobs,		/* O - Job data */
            const char *name,		/* I - @code NULL@ = all destinations, otherwise show jobs for named destination */
            int        myjobs,		/* I - 0 = all users, 1 = mine */
	    int        whichjobs)	/* I - @code CUPS_WHICHJOBS_ALL@, @code CUPS_WHICHJOBS_ACTIVE@, or @code CUPS_WHICHJOBS_COMPLETED@ */
{
 /*
  * Return the jobs...
  */

  return (cupsGetJobs2(CUPS_HTTP_DEFAULT, jobs, name, myjobs, whichjobs));
}



/*
 * 'cupsGetJobs2()' - Get the jobs from the specified server.
 *
 * A "whichjobs" value of @code CUPS_WHICHJOBS_ALL@ returns all jobs regardless
 * of state, while @code CUPS_WHICHJOBS_ACTIVE@ returns jobs that are
 * pending, processing, or held and @code CUPS_WHICHJOBS_COMPLETED@ returns
 * jobs that are stopped, canceled, aborted, or completed.
 *
 * @since CUPS 1.1.21/macOS 10.4@
 */

int					/* O - Number of jobs */
cupsGetJobs2(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
             cups_job_t **jobs,		/* O - Job data */
             const char *name,		/* I - @code NULL@ = all destinations, otherwise show jobs for named destination */
             int        myjobs,		/* I - 0 = all users, 1 = mine */
	     int        whichjobs)	/* I - @code CUPS_WHICHJOBS_ALL@, @code CUPS_WHICHJOBS_ACTIVE@, or @code CUPS_WHICHJOBS_COMPLETED@ */
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
		  "document-format",
		  "job-id",
		  "job-k-octets",
		  "job-name",
		  "job-originating-user-name",
		  "job-printer-uri",
		  "job-priority",
		  "job-state",
		  "time-at-completed",
		  "time-at-creation",
		  "time-at-processing"
		};


 /*
  * Range check input...
  */

  if (!jobs)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);

    return (-1);
  }

 /*
  * Get the right URI...
  */

  if (name)
  {
    if (httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                         "localhost", 0, "/printers/%s",
                         name) < HTTP_URI_STATUS_OK)
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL,
                    _("Unable to create printer-uri"), 1);

      return (-1);
    }
  }
  else
    strlcpy(uri, "ipp://localhost/", sizeof(uri));

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

  request = ippNewRequest(IPP_OP_GET_JOBS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (myjobs)
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);

  if (whichjobs == CUPS_WHICHJOBS_COMPLETED)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "completed");
  else if (whichjobs == CUPS_WHICHJOBS_ALL)
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
      state           = IPP_JSTATE_PENDING;
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
	temp = realloc(*jobs, sizeof(cups_job_t) * (size_t)(n + 1));

      if (!temp)
      {
       /*
        * Ran out of memory!
        */

        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, NULL, 0);

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

  if (n == 0 && cg->last_error >= IPP_STATUS_ERROR_BAD_REQUEST)
    return (-1);
  else
    return (n);
}


/*
 * 'cupsGetPrinters()' - Get a list of printers from the default server.
 *
 * This function is deprecated and no longer returns a list of printers - use
 * @link cupsGetDests@ instead.
 *
 * @deprecated@
 */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers)	/* O - Printers */
{
  if (printers)
    *printers = NULL;

  return (0);
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class on the default server.
 */

int					/* O - Job ID or 0 on error */
cupsPrintFile(const char    *name,	/* I - Destination name */
              const char    *filename,	/* I - File to print */
	      const char    *title,	/* I - Title of job */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsPrintFile(name=\"%s\", filename=\"%s\", title=\"%s\", num_options=%d, options=%p)", name, filename, title, num_options, (void *)options));

  return (cupsPrintFiles2(CUPS_HTTP_DEFAULT, name, 1, &filename, title,
                          num_options, options));
}


/*
 * 'cupsPrintFile2()' - Print a file to a printer or class on the specified
 *                      server.
 *
 * @since CUPS 1.1.21/macOS 10.4@
 */

int					/* O - Job ID or 0 on error */
cupsPrintFile2(
    http_t        *http,		/* I - Connection to server */
    const char    *name,		/* I - Destination name */
    const char    *filename,		/* I - File to print */
    const char    *title,		/* I - Title of job */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  DEBUG_printf(("cupsPrintFile2(http=%p, name=\"%s\", filename=\"%s\",  title=\"%s\", num_options=%d, options=%p)", (void *)http, name, filename, title, num_options, (void *)options));

  return (cupsPrintFiles2(http, name, 1, &filename, title, num_options,
                          options));
}


/*
 * 'cupsPrintFiles()' - Print one or more files to a printer or class on the
 *                      default server.
 */

int					/* O - Job ID or 0 on error */
cupsPrintFiles(
    const char    *name,		/* I - Destination name */
    int           num_files,		/* I - Number of files */
    const char    **files,		/* I - File(s) to print */
    const char    *title,		/* I - Title of job */
    int           num_options,		/* I - Number of options */
    cups_option_t *options)		/* I - Options */
{
  DEBUG_printf(("cupsPrintFiles(name=\"%s\", num_files=%d, files=%p, title=\"%s\", num_options=%d, options=%p)", name, num_files, (void *)files, title, num_options, (void *)options));

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
 * @since CUPS 1.1.21/macOS 10.4@
 */

int					/* O - Job ID or 0 on error */
cupsPrintFiles2(
    http_t        *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char    *name,		/* I - Destination name */
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
  _cups_globals_t *cg = _cupsGlobals();	/* Global data */
  ipp_status_t	cancel_status;		/* Status code to preserve */
  char		*cancel_message;	/* Error message to preserve */


  DEBUG_printf(("cupsPrintFiles2(http=%p, name=\"%s\", num_files=%d, files=%p, title=\"%s\", num_options=%d, options=%p)", (void *)http, name, num_files, (void *)files, title, num_options, (void *)options));

 /*
  * Range check input...
  */

  if (!name || num_files < 1 || !files)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);

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

      _cupsSetError(IPP_STATUS_ERROR_DOCUMENT_ACCESS, NULL, 0);
      goto cancel_job;
    }

    status = cupsStartDocument(http, name, job_id, docname, format,
			       i == (num_files - 1));

    while (status == HTTP_STATUS_CONTINUE &&
	   (bytes = cupsFileRead(fp, buffer, sizeof(buffer))) > 0)
      status = cupsWriteRequestData(http, buffer, (size_t)bytes);

    cupsFileClose(fp);

    if (status != HTTP_STATUS_CONTINUE || cupsFinishDocument(http, name) != IPP_STATUS_OK)
    {
     /*
      * Unable to queue, cancel the job and return...
      */

      goto cancel_job;
    }
  }

  return (job_id);

 /*
  * If we get here, something happened while sending the print job so we need
  * to cancel the job without setting the last error (since we need to preserve
  * the current error...
  */

  cancel_job:

  cancel_status  = cg->last_error;
  cancel_message = cg->last_status_message ?
                       _cupsStrRetain(cg->last_status_message) : NULL;

  cupsCancelJob2(http, name, job_id, 0);

  cg->last_error          = cancel_status;
  cg->last_status_message = cancel_message;

  return (0);
}


/*
 * 'cupsStartDocument()' - Add a document to a job created with cupsCreateJob().
 *
 * Use @link cupsWriteRequestData@ to write data for the document and
 * @link cupsFinishDocument@ to finish the document and get the submission status.
 *
 * The MIME type constants @code CUPS_FORMAT_AUTO@, @code CUPS_FORMAT_PDF@,
 * @code CUPS_FORMAT_POSTSCRIPT@, @code CUPS_FORMAT_RAW@, and
 * @code CUPS_FORMAT_TEXT@ are provided for the "format" argument, although
 * any supported MIME type string can be supplied.
 *
 * @since CUPS 1.4/macOS 10.6@
 */

http_status_t				/* O - HTTP status of request */
cupsStartDocument(
    http_t     *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char *name,			/* I - Destination name */
    int        job_id,			/* I - Job ID from @link cupsCreateJob@ */
    const char *docname,		/* I - Name of document */
    const char *format,			/* I - MIME type or @code CUPS_FORMAT_foo@ */
    int        last_document)		/* I - 1 for last document in job, 0 otherwise */
{
  char		resource[1024],		/* Resource for destinatio */
		printer_uri[1024];	/* Printer URI */
  ipp_t		*request;		/* Send-Document request */
  http_status_t	status;			/* HTTP status */


 /*
  * Create a Send-Document request...
  */

  if ((request = ippNewRequest(IPP_OP_SEND_DOCUMENT)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), 0);
    return (HTTP_STATUS_ERROR);
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
  ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", (char)last_document);

 /*
  * Send and delete the request, then return the status...
  */

  status = cupsSendRequest(http, request, resource, CUPS_LENGTH_VARIABLE);

  ippDelete(request);

  return (status);
}
