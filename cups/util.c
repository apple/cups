/*
 * "$Id: util.c,v 1.3 1999/02/26 22:00:52 mike Exp $"
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products.
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
 *       44145 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ipp.h"
#include "language.h"
#include "string.h"
#include "debug.h"
#include <errno.h>
#include <sys/stat.h>


int
cupsCancelJob(char *printer,
              int  job)
{
}


int
cupsGetClasses(char ***classes)
{
}


char *
cupsGetDefault(void)
{
  return ("LJ4000");
}


char *
cupsGetPPD(char *printer)
{
}


int
cupsGetPrinters(char ***printers)
{
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFile(char          *printer,	/* I - Printer or class name */
              char          *filename,	/* I - File to print */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  int			i;			/* Looping var */
  http_t		*http;			/* HTTP connection */
  ipp_t			*request;		/* IPP request */
  ipp_t			*response;		/* IPP response */
  char			uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t		*language;		/* Language to use */
  ipp_attribute_t	*attr;			/* IPP attribute */
  struct stat		filestats;		/* File information */
  FILE			*fp;			/* File pointer */
  char			buffer[8192];		/* Copy buffer */
  http_status_t		status;			/* HTTP status of request */
  int			jobid;			/* New job ID */


  DEBUG_printf(("cupsPrintFile(\'%s\', \'%s\', %d, %08x)\n",
                printer, filename, num_options, options));

  if (printer == NULL || filename == NULL)
    return (0);

 /*
  * See if the file exists and is readable...
  */

  if (stat(filename, &filestats))
    return (0);

  if ((fp = fopen(filename, "rb")) == NULL)
  {
    DEBUG_puts("cupsPrintFile: Unable to open file!");
    return (0);
  }

 /*
  * Setup a connection and request data...
  */

  if ((request = ippNew()) == NULL)
  {
    fclose(fp);
    return (0);
  }

  if ((response = ippNew()) == NULL)
  {
    fclose(fp);
    ippDelete(request);
    return (0);
  }

  if ((http = httpConnect("localhost", ippPort())) == NULL)
  {
    DEBUG_printf(("cupsPrintFile: Unable to open connection - %s.\n",
                  strerror(errno)));
    fclose(fp);
    ippDelete(request);
    ippDelete(response);
    return (0);
  }

 /*
  * Build a standard CUPS URI for the printer and fill the standard IPP
  * attributes...
  */

  request->request.op.operation_id = IPP_PRINT_JOB;
  request->request.op.request_id   = 1;

  sprintf(uri, "http://localhost:%d/printers/%s", ippPort(), printer);

  language = cupsLangDefault();

  attr = ippAddString(request, IPP_TAG_OPERATION, "attributes-charset",
                      cupsLangEncoding(language));
  attr->value_tag = IPP_TAG_CHARSET;

  attr = ippAddString(request, IPP_TAG_OPERATION, "attributes-natural-language",
                      language != NULL ? language->language : "C");
  attr->value_tag = IPP_TAG_LANGUAGE;

  attr = ippAddString(request, IPP_TAG_OPERATION, "printer-uri", uri);
  attr->value_tag = IPP_TAG_URI;

  attr = ippAddString(request, IPP_TAG_JOB, "document-format",
                      "application/octet-stream");
  attr->value_tag = IPP_TAG_MIMETYPE;

 /**** ADD USERNAME ****/

 /*
  * Then add all options on the command-line...
  */

#if 0
  for (i = 0; i < num_options; i ++)
  {
  }
#endif /* 0 */

 /*
  * Setup the necessary HTTP fields...
  */

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");

  sprintf(buffer, "%u", ippLength(request) + filestats.st_size);
  httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, buffer);

 /*
  * Finally, issue a POST request for the printer and send the IPP data and
  * file.
  */

  sprintf(uri, "/printers/%s", printer);

  if (httpPost(http, uri))
  {
    fputs("httpPost() failed.\n", stderr);
    jobid = 0;
  }
  else if (ippWrite(http, request) == IPP_ERROR)
  {
    fputs("ippWrite() failed.\n", stderr);
    jobid = 0;
  }
  else
  {
    while ((i = fread(buffer, 1, sizeof(buffer), fp)) > 0)
      if (httpWrite(http, buffer, i) < i)
      {
        fputs("httpWrite() failed.\n", stderr);

	fclose(fp);
	ippDelete(request);
	ippDelete(response);
	httpClose(http);
	return (0);
      }

    httpWrite(http, buffer, 0);

    while ((status = httpUpdate(http)) != HTTP_CONTINUE);

    if (status == HTTP_ERROR)
    {
      fprintf(stderr, "httpUpdate() failed (%d).\n", status);
      jobid = 0;
    }
    else if ((ippRead(http, response)) == IPP_ERROR)
    {
      fputs("ippRead() failed.\n", stderr);
      jobid = 0;
    }
    else if (response->request.status.status_code != IPP_OK)
    {
      fprintf(stderr, "IPP response code was 0x%x!\n",
              response->request.status.status_code);
      jobid = 0;
    }
    else if ((attr = ippFindAttribute(response, "job-id")) == NULL)
    {
      fputs("No job ID!\n", stderr);
      jobid = 0;
    }
    else
      jobid = attr->values[0].integer;
  }

  fclose(fp);
  ippDelete(request);
  ippDelete(response);
  httpClose(http);

  return (jobid);
}


/*
 * End of "$Id: util.c,v 1.3 1999/02/26 22:00:52 mike Exp $".
 */
