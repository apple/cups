/*
 * Destination job support for CUPS.
 *
 * Copyright 2012-2017 by Apple Inc.
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


/*
 * 'cupsCancelDestJob()' - Cancel a job on a destination.
 *
 * The "job_id" is the number returned by cupsCreateDestJob.
 *
 * Returns @code IPP_STATUS_OK@ on success and
 * @code IPP_STATUS_ERROR_NOT_AUTHORIZED@ or
 * @code IPP_STATUS_ERROR_FORBIDDEN@ on failure.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_status_t                            /* O - Status of cancel operation */
cupsCancelDestJob(http_t      *http,	/* I - Connection to destination */
                  cups_dest_t *dest,	/* I - Destination */
                  int         job_id)	/* I - Job ID */
{
  cups_dinfo_t	*info;			/* Destination information */


  if ((info = cupsCopyDestInfo(http, dest)) != NULL)
  {
    ipp_t	*request;		/* Cancel-Job request */

    request = ippNewRequest(IPP_OP_CANCEL_JOB);

    ippSetVersion(request, info->version / 10, info->version % 10);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, info->uri);
    ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job_id);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

    ippDelete(cupsDoRequest(http, request, info->resource));
    cupsFreeDestInfo(info);
  }

  return (cupsLastError());
}


/*
 * 'cupsCloseDestJob()' - Close a job and start printing.
 *
 * Use when the last call to cupsStartDocument passed 0 for "last_document".
 * "job_id" is the job ID returned by cupsCreateDestJob. Returns @code IPP_STATUS_OK@
 * on success.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_status_t				/* O - IPP status code */
cupsCloseDestJob(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *info, 		/* I - Destination information */
    int          job_id)		/* I - Job ID */
{
  int			i;		/* Looping var */
  ipp_t			*request = NULL;/* Close-Job/Send-Document request */
  ipp_attribute_t	*attr;		/* operations-supported attribute */


  DEBUG_printf(("cupsCloseDestJob(http=%p, dest=%p(%s/%s), info=%p, job_id=%d)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info, job_id));

 /*
  * Get the default connection as needed...
  */

  if (!http)
    http = _cupsConnect();

 /*
  * Range check input...
  */

  if (!http || !dest || !info || job_id <= 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    DEBUG_puts("1cupsCloseDestJob: Bad arguments.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

 /*
  * Build a Close-Job or empty Send-Document request...
  */

  if ((attr = ippFindAttribute(info->attrs, "operations-supported",
                               IPP_TAG_ENUM)) != NULL)
  {
    for (i = 0; i < attr->num_values; i ++)
      if (attr->values[i].integer == IPP_OP_CLOSE_JOB)
      {
        request = ippNewRequest(IPP_OP_CLOSE_JOB);
        break;
      }
  }

  if (!request)
    request = ippNewRequest(IPP_OP_SEND_DOCUMENT);

  if (!request)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), 0);
    DEBUG_puts("1cupsCloseDestJob: Unable to create Close-Job/Send-Document "
               "request.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  ippSetVersion(request, info->version / 10, info->version % 10);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, info->uri);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id",
                job_id);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());
  if (ippGetOperation(request) == IPP_OP_SEND_DOCUMENT)
    ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);

 /*
  * Send the request and return the status...
  */

  ippDelete(cupsDoRequest(http, request, info->resource));

  DEBUG_printf(("1cupsCloseDestJob: %s (%s)", ippErrorString(cupsLastError()),
                cupsLastErrorString()));

  return (cupsLastError());
}


/*
 * 'cupsCreateDestJob()' - Create a job on a destination.
 *
 * Returns @code IPP_STATUS_OK@ or @code IPP_STATUS_OK_SUBST@ on success, saving the job ID
 * in the variable pointed to by "job_id".
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_status_t				/* O - IPP status code */
cupsCreateDestJob(
    http_t        *http,		/* I - Connection to destination */
    cups_dest_t   *dest,		/* I - Destination */
    cups_dinfo_t  *info, 		/* I - Destination information */
    int           *job_id,		/* O - Job ID or 0 on error */
    const char    *title,		/* I - Job name */
    int           num_options,		/* I - Number of job options */
    cups_option_t *options)		/* I - Job options */
{
  ipp_t			*request,	/* Create-Job request */
			*response;	/* Create-Job response */
  ipp_attribute_t	*attr;		/* job-id attribute */


  DEBUG_printf(("cupsCreateDestJob(http=%p, dest=%p(%s/%s), info=%p, "
                "job_id=%p, title=\"%s\", num_options=%d, options=%p)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info, (void *)job_id, title, num_options, (void *)options));

 /*
  * Get the default connection as needed...
  */

  if (!http)
    http = _cupsConnect();

 /*
  * Range check input...
  */

  if (job_id)
    *job_id = 0;

  if (!http || !dest || !info || !job_id)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    DEBUG_puts("1cupsCreateDestJob: Bad arguments.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

 /*
  * Build a Create-Job request...
  */

  if ((request = ippNewRequest(IPP_OP_CREATE_JOB)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), 0);
    DEBUG_puts("1cupsCreateDestJob: Unable to create Create-Job request.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

  ippSetVersion(request, info->version / 10, info->version % 10);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, info->uri);
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

  response = cupsDoRequest(http, request, info->resource);

  if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) != NULL)
  {
    *job_id = attr->values[0].integer;
    DEBUG_printf(("1cupsCreateDestJob: job-id=%d", *job_id));
  }

  ippDelete(response);

 /*
  * Return the status code from the Create-Job request...
  */

  DEBUG_printf(("1cupsCreateDestJob: %s (%s)", ippErrorString(cupsLastError()),
                cupsLastErrorString()));

  return (cupsLastError());
}


/*
 * 'cupsFinishDestDocument()' - Finish the current document.
 *
 * Returns @code IPP_STATUS_OK@ or @code IPP_STATUS_OK_SUBST@ on success.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

ipp_status_t				/* O - Status of document submission */
cupsFinishDestDocument(
    http_t       *http,			/* I - Connection to destination */
    cups_dest_t  *dest,			/* I - Destination */
    cups_dinfo_t *info) 		/* I - Destination information */
{
  DEBUG_printf(("cupsFinishDestDocument(http=%p, dest=%p(%s/%s), info=%p)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info));

 /*
  * Get the default connection as needed...
  */

  if (!http)
    http = _cupsConnect();

 /*
  * Range check input...
  */

  if (!http || !dest || !info)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    DEBUG_puts("1cupsFinishDestDocument: Bad arguments.");
    return (IPP_STATUS_ERROR_INTERNAL);
  }

 /*
  * Get the response at the end of the document and return it...
  */

  ippDelete(cupsGetResponse(http, info->resource));

  DEBUG_printf(("1cupsFinishDestDocument: %s (%s)",
                ippErrorString(cupsLastError()), cupsLastErrorString()));

  return (cupsLastError());
}


/*
 * 'cupsStartDestDocument()' - Start a new document.
 *
 * "job_id" is the job ID returned by cupsCreateDestJob.  "docname" is the name
 * of the document/file being printed, "format" is the MIME media type for the
 * document (see CUPS_FORMAT_xxx constants), and "num_options" and "options"
 * are the options do be applied to the document. "last_document" should be 1
 * if this is the last document to be submitted in the job.  Returns
 * @code HTTP_CONTINUE@ on success.
 *
 * @since CUPS 1.6/macOS 10.8@
 */

http_status_t				/* O - Status of document creation */
cupsStartDestDocument(
    http_t        *http,		/* I - Connection to destination */
    cups_dest_t   *dest,		/* I - Destination */
    cups_dinfo_t  *info, 		/* I - Destination information */
    int           job_id,		/* I - Job ID */
    const char    *docname,		/* I - Document name */
    const char    *format,		/* I - Document format */
    int           num_options,		/* I - Number of document options */
    cups_option_t *options,		/* I - Document options */
    int           last_document)	/* I - 1 if this is the last document */
{
  ipp_t		*request;		/* Send-Document request */
  http_status_t	status;			/* HTTP status */


  DEBUG_printf(("cupsStartDestDocument(http=%p, dest=%p(%s/%s), info=%p, job_id=%d, docname=\"%s\", format=\"%s\", num_options=%d, options=%p, last_document=%d)", (void *)http, (void *)dest, dest ? dest->name : NULL, dest ? dest->instance : NULL, (void *)info, job_id, docname, format, num_options, (void *)options, last_document));

 /*
  * Get the default connection as needed...
  */

  if (!http)
    http = _cupsConnect();

 /*
  * Range check input...
  */

  if (!http || !dest || !info || job_id <= 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    DEBUG_puts("1cupsStartDestDocument: Bad arguments.");
    return (HTTP_STATUS_ERROR);
  }

 /*
  * Create a Send-Document request...
  */

  if ((request = ippNewRequest(IPP_OP_SEND_DOCUMENT)) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(ENOMEM), 0);
    DEBUG_puts("1cupsStartDestDocument: Unable to create Send-Document "
               "request.");
    return (HTTP_STATUS_ERROR);
  }

  ippSetVersion(request, info->version / 10, info->version % 10);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, info->uri);
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

  cupsEncodeOptions2(request, num_options, options, IPP_TAG_OPERATION);
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_DOCUMENT);

 /*
  * Send and delete the request, then return the status...
  */

  status = cupsSendRequest(http, request, info->resource, CUPS_LENGTH_VARIABLE);

  ippDelete(request);

  return (status);
}
