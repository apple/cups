/*
 * "$Id$"
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsCancelJob()     - Cancel a print job.
 *   cupsDoFileRequest() - Do an IPP request...
 *   cupsFreeJobs()      - Free memory used by job data.
 *   cupsGetClasses()    - Get a list of printer classes.
 *   cupsGetDefault()    - Get the default printer or class.
 *   cupsGetDefault2()   - Get the default printer or class.
 *   cupsGetJobs()       - Get the jobs from the server.
 *   cupsGetJobs2()      - Get the jobs from the server.
 *   cupsGetPPD()        - Get the PPD file for a printer.
 *   cupsGetPPD2()       - Get the PPD file for a printer.
 *   cupsGetPrinters()   - Get a list of printers.
 *   cupsLastError()     - Return the last IPP error that occurred.
 *   cupsPrintFile()     - Print a file to a printer or class.
 *   cupsPrintFile2()    - Print a file to a printer or class.
 *   cupsPrintFiles()    - Print one or more files to a printer or class.
 *   cupsPrintFiles2()   - Print one or more files to a printer or class.
 *   cups_connect()      - Connect to the specified host...
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

static char	*cups_connect(const char *name, char *printer, char *hostname);


/*
 * 'cupsCancelJob()' - Cancel a print job.
 */

int					/* O - 1 on success, 0 on failure */
cupsCancelJob(const char *name,		/* I - Name of printer or class */
              int        job)		/* I - Job ID */
{
  char		printer[HTTP_MAX_URI],	/* Printer name */
		hostname[HTTP_MAX_URI],	/* Hostname */
		uri[HTTP_MAX_URI];	/* Printer URI */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  cups_lang_t	*language;		/* Language info */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build an IPP_CANCEL_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    job-id
  *    [requesting-user-name]
  */

  request = ippNew();

  request->request.op.operation_id = IPP_CANCEL_JOB;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  cupsLangFree(language);

  snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request...
  */

  if ((response = cupsDoRequest(cg->http, request, "/jobs/")) != NULL)
    ippDelete(response);

  return (cg->last_error < IPP_REDIRECTION_OTHER_SITE);
}


/*
 * 'cupsDoFileRequest()' - Do an IPP request...
 */

ipp_t *					/* O - Response data */
cupsDoFileRequest(http_t     *http,	/* I - HTTP connection to server */
                  ipp_t      *request,	/* I - IPP request */
                  const char *resource,	/* I - HTTP resource for POST */
		  const char *filename)	/* I - File to send or NULL */
{
  ipp_t		*response;		/* IPP response data */
  off_t		length;			/* Content-Length value */
  http_status_t	status;			/* Status of HTTP request */
  FILE		*file;			/* File to send */
  struct stat	fileinfo;		/* File information */
  int		bytes;			/* Number of bytes read/written */
  char		buffer[65536];		/* Output buffer */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  DEBUG_printf(("cupsDoFileRequest(%p, %p, \'%s\', \'%s\')\n",
                http, request, resource ? resource : "(null)",
		filename ? filename : "(null)"));

  if (http == NULL || request == NULL || resource == NULL)
  {
    if (request != NULL)
      ippDelete(request);

    cg->last_error = IPP_INTERNAL_ERROR;
    return (NULL);
  }

 /*
  * See if we have a file to send...
  */

  if (filename != NULL)
  {
    if (stat(filename, &fileinfo))
    {
     /*
      * Can't get file information!
      */

      ippDelete(request);
      cg->last_error = IPP_NOT_FOUND;
      return (NULL);
    }

#ifdef WIN32
    if (fileinfo.st_mode & _S_IFDIR)
#else
    if (S_ISDIR(fileinfo.st_mode))
#endif /* WIN32 */
    {
     /*
      * Can't send a directory...
      */

      ippDelete(request);
      cg->last_error = IPP_NOT_POSSIBLE;
      return (NULL);
    }

    if ((file = fopen(filename, "rb")) == NULL)
    {
     /*
      * Can't open file!
      */

      ippDelete(request);
      cg->last_error = IPP_NOT_FOUND;
      return (NULL);
    }
  }
  else
    file = NULL;

 /*
  * Loop until we can send the request without authorization problems.
  */

  response = NULL;
  status   = HTTP_ERROR;

  while (response == NULL)
  {
    DEBUG_puts("cupsDoFileRequest: setup...");

   /*
    * Setup the HTTP variables needed...
    */

    length = ippLength(request);
    if (filename)
      length += fileinfo.st_size;

    httpClearFields(http);
    httpSetLength(http, length);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);

    DEBUG_printf(("cupsDoFileRequest: authstring=\"%s\"\n", http->authstring));

   /*
    * Try the request...
    */

    DEBUG_puts("cupsDoFileRequest: post...");

    if (httpPost(http, resource))
    {
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }
      else
        continue;
    }

   /*
    * Send the IPP data and wait for the response...
    */

    DEBUG_puts("cupsDoFileRequest: ipp write...");

    request->state = IPP_IDLE;
    status         = HTTP_CONTINUE;

    if (ippWrite(http, request) != IPP_ERROR)
      if (filename != NULL)
      {
        DEBUG_puts("cupsDoFileRequest: file write...");

       /*
        * Send the file...
        */

        rewind(file);

        while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
	{
	  if (httpCheck(http))
	  {
	    if ((status = httpUpdate(http)) != HTTP_CONTINUE)
	      break;
          }

  	  if (httpWrite(http, buffer, bytes) < bytes)
            break;
        }
      }

   /*
    * Get the server's return status...
    */

    DEBUG_puts("cupsDoFileRequest: update...");

    while (status == HTTP_CONTINUE)
      status = httpUpdate(http);

    DEBUG_printf(("cupsDoFileRequest: status = %d\n", status));

    if (status == HTTP_UNAUTHORIZED)
    {
      DEBUG_puts("cupsDoFileRequest: unauthorized...");

     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * See if we can do authentication...
      */

      if (cupsDoAuthentication(http, "POST", resource))
        break;

      httpReconnect(http);

      continue;
    }
    else if (status == HTTP_ERROR)
    {
#ifdef WIN32
      if (http->error != WSAENETDOWN && http->error != WSAENETUNREACH)
#else
      if (http->error != ENETDOWN && http->error != ENETUNREACH)
#endif /* WIN32 */
        continue;
      else
        break;
    }
#ifdef HAVE_SSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush(http);

      /* Reconnect... */
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }

      /* Upgrade with encryption... */
      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);

      /* Try again, this time with encryption enabled... */
      continue;
    }
#endif /* HAVE_SSL */
    else if (status != HTTP_OK)
    {
      DEBUG_printf(("cupsDoFileRequest: error %d...\n", status));

     /*
      * Flush any error message...
      */

      httpFlush(http);
      break;
    }
    else
    {
     /*
      * Read the response...
      */

      DEBUG_puts("cupsDoFileRequest: response...");

      response = ippNew();

      if (ippRead(http, response) == IPP_ERROR)
      {
       /*
        * Delete the response...
	*/

        DEBUG_puts("IPP read error!");
	ippDelete(response);
	response = NULL;

        cg->last_error = IPP_SERVICE_UNAVAILABLE;
	break;
      }
    }
  }

 /*
  * Close the file if needed...
  */

  if (filename != NULL)
    fclose(file);

 /*
  * Flush any remaining data...
  */

  httpFlush(http);

 /*
  * Delete the original request and return the response...
  */
  
  ippDelete(request);

  if (response)
    cg->last_error = response->request.status.status_code;
  else if (status != HTTP_OK)
  {
    switch (status)
    {
      case HTTP_NOT_FOUND :
          cg->last_error = IPP_NOT_FOUND;
	  break;

      case HTTP_UNAUTHORIZED :
          cg->last_error = IPP_NOT_AUTHORIZED;
	  break;

      case HTTP_FORBIDDEN :
          cg->last_error = IPP_FORBIDDEN;
	  break;

      case HTTP_BAD_REQUEST :
          cg->last_error = IPP_BAD_REQUEST;
	  break;

      case HTTP_REQUEST_TOO_LARGE :
          cg->last_error = IPP_REQUEST_VALUE;
	  break;

      case HTTP_NOT_IMPLEMENTED :
          cg->last_error = IPP_OPERATION_NOT_SUPPORTED;
	  break;

      case HTTP_NOT_SUPPORTED :
          cg->last_error = IPP_VERSION_NOT_SUPPORTED;
	  break;

      default :
	  DEBUG_printf(("HTTP error %d mapped to IPP_SERVICE_UNAVAILABLE!\n",
                	status));
	  cg->last_error = IPP_SERVICE_UNAVAILABLE;
	  break;
    }
  }

  return (response);
}


/*
 * 'cupsFreeJobs()' - Free memory used by job data.
 */

void
cupsFreeJobs(int        num_jobs,	/* I - Number of jobs */
             cups_job_t *jobs)		/* I - Jobs */
{
  int	i;				/* Looping var */


  if (num_jobs <= 0 || jobs == NULL)
    return;

  for (i = 0; i < num_jobs; i ++)
  {
    free(jobs[i].dest);
    free(jobs[i].user);
    free(jobs[i].format);
    free(jobs[i].title);
  }

  free(jobs);
}


/*
 * 'cupsGetClasses()' - Get a list of printer classes.
 */

int					/* O - Number of classes */
cupsGetClasses(char ***classes)		/* O - Classes */
{
  int		n;			/* Number of classes */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  char		**temp;			/* Temporary pointer */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (classes == NULL)
  {
    cg->last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_CLASSES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  cupsLangFree(language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

 /*
  * Do the request and get back a response...
  */

  n        = 0;
  *classes = NULL;

  if ((response = cupsDoRequest(cg->http, request, "/")) != NULL)
  {
    cg->last_error = response->request.status.status_code;

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
  else
    cg->last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class.
 */

const char *				/* O - Default printer or NULL */
cupsGetDefault(void)
{
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
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }

 /*
  * Return the default printer...
  */

  return (cupsGetDefault2(cg->http));
}


/*
 * 'cupsGetDefault2()' - Get the default printer or class.
 */

const char *				/* O - Default printer or NULL */
cupsGetDefault2(http_t *http)		/* I - HTTP connection */
{
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
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
  * Range check input...
  */

  if (!http)
    return (NULL);

 /*
  * Build a CUPS_GET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_DEFAULT;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  cupsLangFree(language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    cg->last_error = response->request.status.status_code;

    if ((attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME)) != NULL)
    {
      strlcpy(cg->def_printer, attr->values[0].string.text, sizeof(cg->def_printer));
      ippDelete(response);
      return (cg->def_printer);
    }

    ippDelete(response);
  }
  else
    cg->last_error = IPP_BAD_REQUEST;

  return (NULL);
}


/*
 * 'cupsGetJobs()' - Get the jobs from the server.
 */

int					/* O - Number of jobs */
cupsGetJobs(cups_job_t **jobs,		/* O - Job data */
            const char *mydest,		/* I - Only show jobs for dest? */
            int        myjobs,		/* I - Only show my jobs? */
	    int        completed)	/* I - Only show completed jobs? */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (-1);
  }

 /*
  * Return the jobs...
  */

  return (cupsGetJobs2(cg->http, jobs, mydest, myjobs, completed));
}



/*
 * 'cupsGetJobs2()' - Get the jobs from the server.
 */

int					/* O - Number of jobs */
cupsGetJobs2(http_t     *http,		/* I - HTTP connection */
             cups_job_t **jobs,		/* O - Job data */
             const char *mydest,	/* I - Only show jobs for dest? */
             int        myjobs,		/* I - Only show my jobs? */
	     int        completed)	/* I - Only show completed jobs? */
{
  int		n;			/* Number of jobs */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
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


  if (!http || !jobs)
  {
    cg->last_error = IPP_INTERNAL_ERROR;
    return (-1);
  }

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

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  cupsLangFree(language);

  if (mydest)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", mydest);
  else
    strcpy(uri, "ipp://localhost/jobs");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (myjobs)
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);

  if (completed)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "completed");

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
    cg->last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (attr == NULL)
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

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  id = attr->values[0].integer;
        else if (strcmp(attr->name, "job-state") == 0 &&
	         attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_jstate_t)attr->values[0].integer;
        else if (strcmp(attr->name, "job-priority") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  priority = attr->values[0].integer;
        else if (strcmp(attr->name, "job-k-octets") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer;
        else if (strcmp(attr->name, "time-at-completed") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  completed_time = attr->values[0].integer;
        else if (strcmp(attr->name, "time-at-creation") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  creation_time = attr->values[0].integer;
        else if (strcmp(attr->name, "time-at-processing") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  processing_time = attr->values[0].integer;
        else if (strcmp(attr->name, "job-printer-uri") == 0 &&
	         attr->value_tag == IPP_TAG_URI)
	{
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;
        }
        else if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	         attr->value_tag == IPP_TAG_NAME)
	  user = attr->values[0].string.text;
        else if (strcmp(attr->name, "document-format") == 0 &&
	         attr->value_tag == IPP_TAG_MIMETYPE)
	  format = attr->values[0].string.text;
        else if (strcmp(attr->name, "job-name") == 0 &&
	         (attr->value_tag == IPP_TAG_TEXT ||
		  attr->value_tag == IPP_TAG_NAME))
	  title = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (dest == NULL || id == 0)
      {
        if (attr == NULL)
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

      if (temp == NULL)
      {
       /*
        * Ran out of memory!
        */

	cupsFreeJobs(n, *jobs);
	*jobs = NULL;

        ippDelete(response);
	return (0);
      }

      *jobs = temp;
      temp  += n;
      n ++;

     /*
      * Copy the data over...
      */

      temp->dest            = strdup(dest);
      temp->user            = strdup(user);
      temp->format          = strdup(format);
      temp->title           = strdup(title);
      temp->id              = id;
      temp->priority        = priority;
      temp->state           = state;
      temp->size            = size;
      temp->completed_time  = completed_time;
      temp->creation_time   = creation_time;
      temp->processing_time = processing_time;

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    cg->last_error = IPP_BAD_REQUEST;

  if (n == 0 && cg->last_error >= IPP_BAD_REQUEST)
    return (-1);
  else
    return (n);
}


/*
 * 'cupsGetPPD()' - Get the PPD file for a printer.
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD(const char *name)		/* I - Printer name */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */

 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, NULL, NULL))
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }

 /*
  * Return the PPD file...
  */

  return (cupsGetPPD2(cg->http, name));
}


/*
 * 'cupsGetPPD2()' - Get the PPD file for a printer.
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD2(http_t     *http,		/* I - HTTP connection */
            const char *name)		/* I - Printer name */
{
  int		i;			/* Looping var */
  int		http_port;		/* Port number */
  http_t	*http2;			/* Alternate HTTP connection */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Local language */
  int		fd;			/* PPD file */
  char		uri[HTTP_MAX_URI],	/* Printer URI */
		printer[HTTP_MAX_URI],	/* Printer name */
		method[HTTP_MAX_URI],	/* Method/scheme name */
		username[HTTP_MAX_URI],	/* Username:password */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource name */
  int		port;			/* Port number */
  http_status_t	status;			/* HTTP status from server */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */
  static const char * const requested_attrs[] =
		{			/* Requested attributes */
		  "printer-uri-supported",
		  "printer-type",
		  "member-uris"
		};


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsGetPPD2(http=%p, name=\"%s\")\n", http,
                name ? name : "(null)"));

  if (!http || !name)
  {
    cg->last_error = IPP_INTERNAL_ERROR;
    return (NULL);
  }

 /*
  * Get the port number we are connect to...
  */

#ifdef AF_INET6
  if (http->hostaddr.addr.sa_family == AF_INET6)
    http_port = ntohs(http->hostaddr.ipv6.sin6_port);
  else
#endif /* AF_INET6 */
  if (http->hostaddr.addr.sa_family == AF_INET)
    http_port = ntohs(http->hostaddr.ipv4.sin_port);
  else
    http_port = ippPort(); 

  port = http_port; 

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  cupsLangFree(language);

  snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", name);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  DEBUG_printf(("cupsGetPPD2: printer-uri=\"%s\"\n", uri));

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                "requested-attributes",
		sizeof(requested_attrs) / sizeof(requested_attrs[0]),
		NULL, requested_attrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    cg->last_error  = response->request.status.status_code;
    printer[0]  = '\0';
    hostname[0] = '\0';

    if ((attr = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
    {
     /*
      * Get the first actual server and printer name in the class...
      */

      for (i = 0; i < attr->num_values; i ++)
      {
	httpSeparate(attr->values[0].string.text, method, username, hostname,
	             &port, resource);
	if (!strncmp(resource, "/printers/", 10))
	{
	 /*
	  * Found a printer!
	  */

	  strlcpy(printer, resource + 10, sizeof(printer));
	  break;
	}
      }
    }
    else if ((attr = ippFindAttribute(response, "printer-uri-supported",
                                      IPP_TAG_URI)) != NULL)
    {
     /*
      * Get the actual server and printer names...
      */

      httpSeparate(attr->values[0].string.text, method, username, hostname,
	           &port, resource);
      strlcpy(printer, strrchr(resource, '/') + 1, sizeof(printer));
    }

    ippDelete(response);

   /*
    * Remap local hostname to localhost...
    */

    httpGetHostname(uri, sizeof(uri));

    if (!strcasecmp(uri, hostname))
      strcpy(hostname, "localhost");
  }

  if (!printer[0])
  {
    cg->last_error = IPP_NOT_FOUND;
    return (NULL);
  }

 /*
  * Reconnect to the correct server as needed...
  */

  if (!strcasecmp(http->hostname, hostname) && port == http_port)
    http2 = http;
  else if ((http2 = httpConnectEncrypt(hostname, port,
                                       cupsEncryption())) == NULL)
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }

 /*
  * Get a temp file...
  */

  if ((fd = cupsTempFd(cg->ppd_filename, sizeof(cg->ppd_filename))) < 0)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    cg->last_error = IPP_INTERNAL_ERROR;

    if (http2 != http)
      httpClose(http2);

    return (NULL);
  }

 /*
  * And send a request to the HTTP server...
  */

  snprintf(resource, sizeof(resource), "/printers/%s.ppd", printer);

  status = cupsGetFd(http2, resource, fd);

  close(fd);

  if (http2 != http)
    httpClose(http2);

 /*
  * See if we actually got the file or an error...
  */

  if (status != HTTP_OK)
  {
    switch (status)
    {
      case HTTP_NOT_FOUND :
          cg->last_error = IPP_NOT_FOUND;
	  break;
      case HTTP_ERROR :
          DEBUG_puts("Mapping HTTP error to IPP_ERROR");
          cg->last_error = IPP_ERROR;
	  break;
      case HTTP_UNAUTHORIZED :
          cg->last_error = IPP_NOT_AUTHORIZED;
	  break;
      default :
          cg->last_error = IPP_INTERNAL_ERROR;
	  break;
    }

    unlink(cg->ppd_filename);

    return (NULL);
  }

 /*
  * Return the PPD file...
  */

  return (cg->ppd_filename);
}


/*
 * 'cupsGetPrinters()' - Get a list of printers.
 */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers)	/* O - Printers */
{
  int		n;			/* Number of printers */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  char		**temp;			/* Temporary pointer */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  if (printers == NULL)
  {
    cg->last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  cupsLangFree(language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

 /*
  * Do the request and get back a response...
  */

  n         = 0;
  *printers = NULL;

  if ((response = cupsDoRequest(cg->http, request, "/")) != NULL)
  {
    cg->last_error = response->request.status.status_code;

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
  else
    cg->last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsLastError()' - Return the last IPP error that occurred.
 */

ipp_status_t				/* O - IPP error code */
cupsLastError(void)
{
  return (_cupsGlobals()->last_error);
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class.
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

  return (cupsPrintFiles(name, 1, &filename, title, num_options, options));
}


/*
 * 'cupsPrintFile2()' - Print a file to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFile2(http_t        *http,	/* I - HTTP connection */
               const char    *name,	/* I - Printer or class name */
               const char    *filename,	/* I - File to print */
	       const char    *title,	/* I - Title of job */
               int           num_options,
					/* I - Number of options */
	       cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsPrintFile2(http=%p, name=\"%s\", filename=\"%s\", "
                "title=\"%s\", num_options=%d, options=%p)\n",
                http, name, filename, title, num_options, options));

  return (cupsPrintFiles2(http, name, 1, &filename, title, num_options, options));
}


/*
 * 'cupsPrintFiles()' - Print one or more files to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFiles(const char    *name,	/* I - Printer or class name */
               int           num_files,	/* I - Number of files */
               const char    **files,	/* I - File(s) to print */
	       const char    *title,	/* I - Title of job */
               int           num_options,
					/* I - Number of options */
	       cups_option_t *options)	/* I - Options */
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */

  DEBUG_printf(("cupsPrintFiles(name=\"%s\", num_files=%d, "
                "files=%p, title=\"%s\", num_options=%d, options=%p)\n",
                name, num_files, files, title, num_options, options));


 /*
  * Setup a connection and request data...
  */

  if (!cups_connect(name, NULL, NULL))
  {
    DEBUG_printf(("cupsPrintFiles: Unable to open connection - %s.\n",
                  strerror(errno)));
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Print the file(s)...
  */

  return (cupsPrintFiles2(cg->http, name, num_files, files, title,
                          num_options, options));
}



/*
 * 'cupsPrintFiles2()' - Print one or more files to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFiles2(http_t        *http,	/* I - HTTP connection */
                const char    *name,	/* I - Printer or class name */
                int           num_files,/* I - Number of files */
                const char    **files,	/* I - File(s) to print */
	        const char    *title,	/* I - Title of job */
                int           num_options,
					/* I - Number of options */
	        cups_option_t *options)	/* I - Options */
{
  int		i;			/* Looping var */
  const char	*val;			/* Pointer to option value */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP job-id attribute */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t	*language;		/* Language to use */
  int		jobid;			/* New job ID */
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


  DEBUG_printf(("cupsPrintFiles(http=%p, name=\"%s\", num_files=%d, "
                "files=%p, title=\"%s\", num_options=%d, options=%p)\n",
                http, name, num_files, files, title, num_options, options));

 /*
  * Range check input...
  */

  if (!http || !name || num_files < 1 || files == NULL)
    return (0);

 /*
  * Setup the request data...
  */

  language = cupsLangDefault();

 /*
  * Build a standard CUPS URI for the printer and fill the standard IPP
  * attributes...
  */

  if ((request = ippNew()) == NULL)
    return (0);

  request->request.op.operation_id = num_files == 1 ? IPP_PRINT_JOB :
                                                      IPP_CREATE_JOB;
  request->request.op.request_id   = 1;

  snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", http->hostname,
           ippPort(), name);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  if (title)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
                 title);

 /*
  * Then add all options...
  */

  cupsEncodeOptions(request, num_options, options);

 /*
  * Do the request...
  */

  snprintf(uri, sizeof(uri), "/printers/%s", name);

  if (num_files == 1)
    response = cupsDoFileRequest(http, request, uri, *files);
  else
    response = cupsDoRequest(http, request, uri);

  if (response == NULL)
    jobid = 0;
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    DEBUG_printf(("IPP response code was 0x%x!\n",
                  response->request.status.status_code));
    jobid = 0;
  }
  else if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
  {
    DEBUG_puts("No job ID!");

    cg->last_error = IPP_SERVICE_UNAVAILABLE;

    jobid = 0;
  }
  else
    jobid = attr->values[0].integer;

  if (response != NULL)
    ippDelete(response);

 /*
  * Handle multiple file jobs if the create-job operation worked...
  */

  if (jobid > 0 && num_files > 1)
    for (i = 0; i < num_files; i ++)
    {
     /*
      * Build a standard CUPS URI for the job and fill the standard IPP
      * attributes...
      */

      if ((request = ippNew()) == NULL)
	return (0);

      request->request.op.operation_id = IPP_SEND_DOCUMENT;
      request->request.op.request_id   = 1;

      snprintf(uri, sizeof(uri), "ipp://%s:%d/jobs/%d", http->hostname,
               ippPort(), jobid);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL,
        	   language != NULL ? language->language : "C");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
        	   NULL, uri);

     /*
      * Handle raw print files...
      */

      if (cupsGetOption("raw", num_options, options))
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	     NULL, "application/vnd.cups-raw");
      else if ((val = cupsGetOption("document-format", num_options, options)) != NULL)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	     NULL, val);
      else
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	     NULL, "application/octet-stream");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	   NULL, cupsUser());

     /*
      * Is this the last document?
      */

      if (i == (num_files - 1))
        ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);

     /*
      * Send the file...
      */

      snprintf(uri, sizeof(uri), "/printers/%s", name);

      if ((response = cupsDoFileRequest(http, request, uri,
                                        files[i])) != NULL)
	ippDelete(response);
    }

  cupsLangFree(language);

  return (jobid);
}


/*
 * 'cups_connect()' - Connect to the specified host...
 */

static char *				/* I - Printer name or NULL */
cups_connect(const char *name,		/* I - Destination (printer[@host]) */
	     char       *printer,	/* O - Printer name [HTTP_MAX_URI] */
             char       *hostname)	/* O - Hostname [HTTP_MAX_URI] */
{
  char	hostbuf[HTTP_MAX_URI];		/* Name of host */
  _cups_globals_t  *cg = _cupsGlobals();	/* Pointer to library globals */


  DEBUG_printf(("cups_connect(\"%s\", %p, %p)\n", name, printer, hostname));

  if (name == NULL)
  {
    cg->last_error = IPP_BAD_REQUEST;
    return (NULL);
  }

 /*
  * All jobs are now queued to cupsServer() to avoid hostname
  * resolution problems and to ensure that the user sees all
  * locally queued jobs locally.
  */

  strlcpy(hostbuf, cupsServer(), sizeof(hostbuf));

  if (hostname != NULL)
    strlcpy(hostname, hostbuf, HTTP_MAX_URI);
  else
    hostname = hostbuf;

  if (printer != NULL)
    strlcpy(printer, name, HTTP_MAX_URI);
  else
    printer = (char *)name;

  if (cg->http != NULL)
  {
    if (strcasecmp(cg->http->hostname, hostname) == 0)
      return (printer);

    httpClose(cg->http);
  }

  DEBUG_printf(("connecting to %s on port %d...\n", hostname, ippPort()));

  if ((cg->http = httpConnectEncrypt(hostname, ippPort(),
                                        cupsEncryption())) == NULL)
  {
    DEBUG_puts("Unable to connect to server!");
    cg->last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }
  else
    return (printer);
}


/*
 * End of "$Id$".
 */
