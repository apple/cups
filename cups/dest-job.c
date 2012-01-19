/*
 * "$Id$"
 *
 *   Destination job support for CUPS.
 *
 *   Copyright 2012 by Apple Inc.
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
﻿ *   cupsCancelDestJob()      - Cancel a job on a destination.
 *   cupsCloseDestJob()       - Close a job and start printing.
 *   cupsCreateDestJob()      - Create a job on a destination.
 *   cupsFinishDestDocument() - Finish the current document.
 *   cupsStartDestDocument()  - Start a new document.
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
 * Returns IPP_OK on success and IPP_NOT_AUTHORIZED or IPP_FORBIDDEN on
 * failure.
 *
 * @since CUPS 1.6@
 */

ipp_status_t
cupsCancelDestJob(http_t      *http,	/* I - Connection to destination */
                  cups_dest_t *dest,	/* I - Destination */
                  int         job_id)	/* I - Job ID */
{
  return (IPP_NOT_FOUND);
}


/*
 * 'cupsCloseDestJob()' - Close a job and start printing.
 *
 * Use when the last call to cupsStartDocument passed 0 for "last_document".
 * "job_id" is the job ID returned by cupsCreateDestJob. Returns IPP_OK on
 * success.
 *
 * @since CUPS 1.6@
 */

ipp_status_t
cupsCloseDestJob(
    http_t      *http,			/* I - Connection to destination */
    cups_dest_t *dest,			/* I - Destination */
    int         job_id)			/* I - Job ID */
{
  return (IPP_NOT_FOUND);
}


/*
 * 'cupsCreateDestJob()' - Create a job on a destination.
 *
 * Returns IPP_OK or IPP_OK_SUBST on success, saving the job ID in the variable
 * pointed to by "job_id".
 *
 * @since CUPS 1.6@
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
  *job_id = 0;

  return (IPP_NOT_POSSIBLE);
}


/*
 * 'cupsFinishDestDocument()' - Finish the current document.
 *
 * Returns IPP_OK on success.
 *
 * @since CUPS 1.6@
 */

ipp_status_t
cupsFinishDestDocument(
    http_t      *http,			/* I - Connection to destination */
    cups_dest_t *dest)			/* I - Destination */
{
  return (IPP_NOT_FOUND);
}


/*
 * 'cupsStartDestDocument()' - Start a new document.
 *
 * "job_id" is the job ID returned by cupsCreateDestJob.  "docname" is the name
 * of the document/file being printed, "format" is the MIME media type for the
 * document (see CUPS_FORMAT_xxx constants), and "num_options" and "options"
 * are the options do be applied to the document. "last_document" should be 1
 * if this is the last document to be submitted in the job.  Returns
 * HTTP_CONTINUE on success.
 *
 * @since CUPS 1.6@
 */

http_status_t
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
  return (HTTP_CONTINUE);
}


/*
 * End of "$Id$".
 */
