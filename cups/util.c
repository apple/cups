/*
 * "$Id: util.c,v 1.5 1999/03/01 22:24:24 mike Exp $"
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

  attr = ippAddString(request, IPP_TAG_JOB, "requesting-user-name",
                      cuserid(NULL));
  attr->value_tag = IPP_TAG_NAME;

 /*
  * Then add all options on the command-line...
  */

  for (i = 0; i < num_options; i ++)
  {
   /*
    * Ignore option names that don't start with a letter...
    */

    if (!isalpha(options[i].name[0]))
      continue;

   /*
    * See what the option value is; for compatibility with older interface
    * scripts, we have to support single-argument options, option SPACE value,
    * option=value, option=low-high, and option=MxN.
    */

    name = options[i].name;
    val  = options[i].value;

    if (val == NULL || *val == '\0')
    {
      if ((i + 1) < num_options &&
          !isalpha(options[i + 1].name[0]))
      {
        i ++;

        val = options[i].name;
      }
      else
        val = NULL;
    }

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

      attr = ippAddString(request, IPP_TAG_JOB, name, val);
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

	if (strcmp(s, "dpc") == 0 || strcmp(s, "cm") == 0)
          attr = ippAddResolution(request, IPP_TAG_JOB, name, n, n2,
	                          IPP_RES_PER_CM);
        else
          attr = ippAddResolution(request, IPP_TAG_JOB, name, n, n2,
	                          IPP_RES_PER_INCH);

	DEBUG_printf(("cupsPrintJob: Adding resolution option \'%s\' with value %dx%d%s...\n",
                      name, n, n2, s));
      }
      else
      {
        attr = ippAddInteger(request, IPP_TAG_JOB, name, n);

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
      attr = ippAddBoolean(request, IPP_TAG_JOB, name, n);
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
 * End of "$Id: util.c,v 1.5 1999/03/01 22:24:24 mike Exp $".
 */
