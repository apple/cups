/*
 * "$Id: util.c,v 1.9 1999/04/19 21:13:26 mike Exp $"
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


/*
 * Local globals...
 */

static http_t	*cupsServer = NULL;


/*
 * 'cupsCancelJob()' - Cancel a print job.
 */

int
cupsCancelJob(char *printer,
              int  job)
{
  return (0);
}


/*
 * 'cupsDoRequest()' - Do an IPP request...
 */

ipp_t *				/* O - Response data */
cupsDoRequest(http_t *http,	/* I - HTTP connection to server */
              ipp_t  *request,	/* I - IPP request */
              char   *resource)	/* I - HTTP resource for POST */
{
  ipp_t		*response;	/* IPP response data */
  char		length[255];	/* Content-Length field */


  DEBUG_printf(("cupsDoRequest(%08x, %08s, \'%s\')\n", http, request, resource));

 /*
  * Setup the HTTP variables needed...
  */

  sprintf(length, "%d", ippLength(request));
  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");

 /*
  * Try the request (twice, if needed)...
  */

  if (httpPost(http, resource))
    if (httpPost(http, resource))
    {
      ippDelete(request);
      return (NULL);
    }

 /*
  * Send the IPP data and wait for the response...
  */

  if (ippWrite(http, request) != IPP_DATA)
    response = NULL;
  else if (httpUpdate(http) != HTTP_OK)
    response = NULL;
  else
  {
   /*
    * Read the response...
    */

    response = ippNew();

    if (ippRead(http, response) == IPP_ERROR)
    {
      ippDelete(response);
      response = NULL;
    }
  }

 /*
  * Delete the original request and return the response...
  */
  
  ippDelete(request);

  return (response);
}


/*
 * 'cupsGetClasses()' - Get a list of printer classes.
 */

int
cupsGetClasses(char ***classes)
{
  return (0);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class.
 */

char *				/* O - Default printer or NULL */
cupsGetDefault(void)
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  static char	def_printer[64];/* Default printer */


 /*
  * First see if the LPDEST or PRINTER environment variables are
  * set...
  */

  if (getenv("LPDEST") != NULL)
    return (getenv("LPDEST"));
  else if (getenv("PRINTER") != NULL)
    return (getenv("PRINTER"));

 /*
  * Try to connect to the server...
  */

  if (cupsServer == NULL)
    if ((cupsServer = httpConnect("localhost", ippPort())) == NULL)
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

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  attr = ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(cupsServer, request, "/printers/")) != NULL)
  {
    if ((attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME)) != NULL)
    {
      strcpy(def_printer, attr->values[0].string.text);
      ippDelete(response);
      return (def_printer);
    }

    ippDelete(response);
  }

  return (NULL);
}


/*
 * 'cupsGetPPD()' - Get the PPD file for a printer.
 */

char *
cupsGetPPD(char *printer)
{
}


/*
 * 'cupsGetPrinters()' - Get a list of printers.
 */

int
cupsGetPrinters(char ***printers)
{
  return (0);
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
  ipp_t			*request;		/* IPP request */
  ipp_t			*response;		/* IPP response */
  char			uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t		*language;		/* Language to use */
  ipp_attribute_t	*attr;			/* IPP attribute */
  struct stat		filestats;		/* File information */
  FILE			*fp;			/* File pointer */
  char			buffer[8192];		/* Copy buffer */
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

  if (cupsServer == NULL)
    if ((cupsServer = httpConnect("localhost", ippPort())) == NULL)
    {
      DEBUG_printf(("cupsPrintFile: Unable to open connection - %s.\n",
                    strerror(errno)));
      fclose(fp);
      ippDelete(request);
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

      if (strcmp(name, "job-name") == 0)
        ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAME, name, NULL, val);
      else
        ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, name, NULL, val);
    }
    else if (val != NULL)
    {
     /*
      * Numeric value, range, or resolution...
      */

      if (*s == '-')
      {
        n2   = strtol(s + 1, NULL, 0);
        ippAddRange(request, IPP_TAG_JOB, name, n, n2);

	DEBUG_printf(("cupsPrintJob: Adding range option \'%s\' with value %d-%d...\n",
                      name, n, n2));
      }
      else if (*s == 'x')
      {
        n2 = strtol(s + 1, &s, 0);

	if (strcmp(s, "dpc") == 0)
          ippAddResolution(request, IPP_TAG_JOB, name, IPP_RES_PER_CM, n, n2);
        else if (strcmp(s, "dpi") == 0)
          ippAddResolution(request, IPP_TAG_JOB, name, IPP_RES_PER_INCH, n, n2);
        else
          ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, name, NULL, val);

	DEBUG_printf(("cupsPrintJob: Adding resolution option \'%s\' with value %s...\n",
                      name, val));
      }
      else
      {
        ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, name, n);

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
      ippAddBoolean(request, IPP_TAG_JOB, name, (char)n);
    }
  }

 /*
  * Setup the necessary HTTP fields...
  */

  httpClearFields(cupsServer);
  httpSetField(cupsServer, HTTP_FIELD_CONTENT_TYPE, "application/ipp");

  sprintf(buffer, "%u", ippLength(request) + filestats.st_size);
  httpSetField(cupsServer, HTTP_FIELD_CONTENT_LENGTH, buffer);

 /*
  * Finally, issue a POST request for the printer and send the IPP data and
  * file.
  */

  sprintf(uri, "/printers/%s", printer);

  response = ippNew();

  if (httpPost(cupsServer, uri))
  {
    DEBUG_puts("httpPost() failed.");
    jobid = 0;
  }
  else if (ippWrite(cupsServer, request) == IPP_ERROR)
  {
    DEBUG_puts("ippWrite() failed.");
    jobid = 0;
  }
  else
  {
    while ((i = fread(buffer, 1, sizeof(buffer), fp)) > 0)
      if (httpWrite(cupsServer, buffer, i) < i)
      {
        DEBUG_puts("httpWrite() failed.");

	fclose(fp);
	ippDelete(request);
	ippDelete(response);
	httpClose(cupsServer);
	return (0);
      }

    httpWrite(cupsServer, buffer, 0);

    if (httpUpdate(cupsServer) == HTTP_ERROR)
    {
      DEBUG_printf(("httpUpdate() failed (%d).\n", status));
      jobid = 0;
    }
    else if ((ippRead(cupsServer, response)) == IPP_ERROR)
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

  return (jobid);
}


/*
 * End of "$Id: util.c,v 1.9 1999/04/19 21:13:26 mike Exp $".
 */
