/*
 * "$Id: util.c,v 1.7 1999/03/21 02:10:08 mike Exp $"
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
#include <stdlib.h>
#include <ctype.h>
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
  int			n, n2;			/* Attribute values */
  char			*name,			/* Name of option */
			*val,			/* Pointer to option value */
			*s;			/* Pointer into option value */
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

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL,
                      language != NULL ? language->language : "C");

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                      NULL, uri);

  attr = ippAddString(request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
                      NULL, "application/octet-stream");

#if defined(WIN32) || defined(__EMX__)
  attr = ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAME, "requesting-user-name",
                      NULL, "WindowsUser");
#else
  attr = ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAME, "requesting-user-name",
                      NULL, cuserid(NULL));
#endif /* WIN32 || __EMX__ */

 /*
  * Then add all options on the command-line...
  */

  for (i = 0; i < num_options; i ++)
  {
   /*
    * See what the option value is; for compatibility with older interface
    * scripts, we have to support single-argument options as well as
    * option=value, option=low-high, and option=MxN.
    */

    name = options[i].name;
    val  = options[i].value;

    if (*val == '\0')
      val = NULL;

    if (val != NULL)
    {
      if (strcasecmp(val, "true") == 0 ||
          strcasecmp(val, "on") == 0 ||
	  strcasecmp(val, "yes") == 0)
      {
       /*
	* Boolean value - true...
	*/

	n   = 1;
	val = "";
      }
      else if (strcasecmp(val, "false") == 0 ||
               strcasecmp(val, "off") == 0 ||
	       strcasecmp(val, "no") == 0)
      {
       /*
	* Boolean value - false...
	*/

	n   = 0;
	val = "";
      }

      n = strtol(val, &s, 0);
    }
    else
    {
      if (strncmp(name, "no", 2) == 0)
      {
	name += 2;
	n    = 0;
      }
      else
        n = 1;

      s = "";
    }

    if (*s != '\0' && *s != '-' && (*s != 'x' || s == val))
    {
     /*
      * String value(s)...
      */

      DEBUG_printf(("cupsPrintJob: Adding string option \'%s\' with value \'%s\'...\n",
                    name, val));

      attr = ippAddString(request, IPP_TAG_JOB, IPP_TAG_STRING, name, NULL, val);
    }
    else if (val != NULL)
    {
     /*
      * Numeric value, range, or resolution...
      */

      if (*s == '-')
      {
        n2   = strtol(s + 1, NULL, 0);
        attr = ippAddRange(request, IPP_TAG_JOB, name, n, n2);

	DEBUG_printf(("cupsPrintJob: Adding range option \'%s\' with value %d-%d...\n",
                      name, n, n2));
      }
      else if (*s == 'x')
      {
        n2 = strtol(s + 1, &s, 0);

	if (strcmp(s, "dpc") == 0)
          attr = ippAddResolution(request, IPP_TAG_JOB, name,
	                          IPP_RES_PER_CM, n, n2);
        else if (strcmp(s, "dpi") == 0)
          attr = ippAddResolution(request, IPP_TAG_JOB, name,
	                          IPP_RES_PER_INCH, n, n2);
        else
          attr = ippAddString(request, IPP_TAG_JOB, IPP_TAG_STRING, name, NULL, val);

	DEBUG_printf(("cupsPrintJob: Adding resolution option \'%s\' with value %s...\n",
                      name, val));
      }
      else
      {
        attr = ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, name, n);

	DEBUG_printf(("cupsPrintJob: Adding integer option \'%s\' with value %d...\n",
                      name, n));
      }
    }
    else
    {
     /*
      * Boolean value...
      */

      DEBUG_printf(("cupsPrintJob: Adding boolean option \'%s\' with value %d...\n",
                    name, n));
      attr = ippAddBoolean(request, IPP_TAG_JOB, name, (char)n);
    }
  }

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
    DEBUG_puts("httpPost() failed.");
    jobid = 0;
  }
  else if (ippWrite(http, request) == IPP_ERROR)
  {
    DEBUG_puts("ippWrite() failed.");
    jobid = 0;
  }
  else
  {
    while ((i = fread(buffer, 1, sizeof(buffer), fp)) > 0)
      if (httpWrite(http, buffer, i) < i)
      {
        DEBUG_puts("httpWrite() failed.");

	fclose(fp);
	ippDelete(request);
	ippDelete(response);
	httpClose(http);
	return (0);
      }

    httpWrite(http, buffer, 0);

    if ((status = httpUpdate(http)) == HTTP_ERROR)
    {
      DEBUG_printf(("httpUpdate() failed (%d).\n", status));
      jobid = 0;
    }
    else if ((ippRead(http, response)) == IPP_ERROR)
    {
      DEBUG_puts("ippRead() failed.");
      jobid = 0;
    }
    else if (response->request.status.status_code > IPP_OK_CONFLICT)
    {
      DEBUG_printf(("IPP response code was 0x%x!\n",
                    response->request.status.status_code));
      jobid = 0;
    }
    else if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      DEBUG_puts("No job ID!");
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
 * End of "$Id: util.c,v 1.7 1999/03/21 02:10:08 mike Exp $".
 */
