/*
 * "$Id: util.c,v 1.11 1999/04/21 14:49:50 mike Exp $"
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
 *   cupsCancelJob()   - Cancel a print job.
 *   cupsDoRequest()   - Do an IPP request...
 *   cupsGetClasses()  - Get a list of printer classes.
 *   cupsGetDefault()  - Get the default printer or class.
 *   cupsGetPPD()      - Get the PPD file for a printer.
 *   cupsGetPrinters() - Get a list of printers.
 *   cupsPrintFile()   - Print a file to a printer or class.
 *   cups_connect()    - Connect to the specified host...
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
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local globals...
 */

static http_t	*cups_server = NULL;


/*
 * Local functions...
 */

static char	*cups_connect(char *name, char *printer, char *hostname);


/*
 * 'cupsCancelJob()' - Cancel a print job.
 */

int				/* O - 1 on success, 0 on failure */
cupsCancelJob(char *name,	/* I - Name of printer or class */
              int  job)		/* I - Job ID */
{
  char		printer[HTTP_MAX_URI],	/* Printer name */
		hostname[HTTP_MAX_URI],	/* Hostname */
		uri[HTTP_MAX_URI];	/* Printer URI */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  cups_lang_t	*language;		/* Language info */


 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
    return (NULL);

 /*
  * Build an IPP_CANCEL_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    job-id
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

  sprintf(uri, "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job);

 /*
  * Do the request...
  */

  if ((response = cupsDoRequest(cups_server, request, "/jobs/")) == NULL)
    return (0);

  ippDelete(response);
  return (1);
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
  char		*password,	/* Password string */
		plain[255],	/* Plaintext username:password */
		encode[255];	/* Encoded username:password */
  http_status_t	status;		/* Status of HTTP request */
  char		junk[8192];	/* Junk buffer for error data */
  static char	authstring[255] = "";
				/* Authorization string */


  DEBUG_printf(("cupsDoRequest(%08x, %08s, \'%s\')\n", http, request, resource));

 /*
  * Setup the HTTP variables needed...
  */

  sprintf(length, "%d", ippLength(request));
  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);
  httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
  httpSetField(http, HTTP_FIELD_AUTHORIZATION, authstring);

 /*
  * Try the request...
  */

  if (httpPost(http, resource))
  {
    ippDelete(request);
    return (NULL);
  }

 /*
  * Send the IPP data and wait for the response...
  */

  ippWrite(http, request);

  if ((status = httpUpdate(http)) == HTTP_UNAUTHORIZED)
  {
   /*
    * Flush any error message...
    */

    while (httpRead(http, junk, sizeof(junk)) > 0);

    if ((password = getpass("Password:")) != NULL)
    {
     /*
      * Got a password; now send it to the server...
      */

      sprintf(plain, "%s:%s", cuserid(NULL), password);
      httpEncode64(encode, plain);
      sprintf(authstring, "Basic %s", encode);

      httpClearFields(http);
      httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);
      httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, authstring);

      if (httpPost(http, resource))
        httpPost(http, resource);
      request->state = IPP_IDLE;
      ippWrite(http, request);
      status = httpUpdate(http);
    }
  }

  if (status != HTTP_OK)
  {
   /*
    * Flush any error message...
    */

    while (httpRead(http, junk, sizeof(junk)) > 0);

    response = NULL;
  }
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

     /*
      * Flush any remaining data...
      */

      while (httpRead(http, junk, sizeof(junk)) > 0);
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

int				/* O - Number of classes */
cupsGetClasses(char ***classes)	/* O - Classes */
{
  int		n;		/* Number of classes */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */


 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
    return (NULL);

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_CLASSES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  n        = 0;
  *classes = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/classes/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (strcmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  *classes = malloc(sizeof(char *));
	else
	  *classes = realloc(*classes, sizeof(char *) * (n + 1));

	if (*classes == NULL)
	{
	  ippDelete(response);
	  return (0);
	}

        (*classes)[n] = strdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }

  return (n);
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

  if (!cups_connect("default", NULL, NULL))
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

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(cups_server, request, "/printers/")) != NULL)
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

char *				/* O - Filename for PPD file */
cupsGetPPD(char *name)		/* I - Printer name */
{
  FILE		*fp;			/* PPD file */
  int		bytes;			/* Number of bytes read */
  char		buffer[8192];		/* Buffer for file */
  char		printer[HTTP_MAX_URI],	/* Printer name */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource name */
  static char	filename[HTTP_MAX_URI];	/* Local filename */
  char		*tempdir;		/* Temporary file directory */
  struct stat	fileinfo;		/* File information */


 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
    return (NULL);

 /*
  * Then check for the cache file...
  */

#if defined(WIN32) || defined(__EMX__)
  tempdir = "C:/WINDOWS/TEMP";
#else
  if ((tempdir = getenv("TMPDIR")) == NULL)
    tempdir = "/tmp";
#endif /* WIN32 || __EMX__ */

  sprintf(filename, "%s/%s.ppd", tempdir, printer);
  if (stat(filename, &fileinfo))
    memset(&fileinfo, 0, sizeof(fileinfo));

 /*
  * And send a request to the HTTP server using "if-modified-since"...
  */

  sprintf(resource, "/printers/%s.ppd", printer);

  httpClearFields(cups_server);
  httpSetField(cups_server, HTTP_FIELD_HOST, hostname);
  httpSetField(cups_server, HTTP_FIELD_IF_MODIFIED_SINCE,
               httpGetDateString(fileinfo.st_mtime));
  httpGet(cups_server, resource);

  switch (httpUpdate(cups_server))
  {
    case HTTP_OK : /* New file - get it! */
        break;
    case HTTP_NOT_MODIFIED : /* File hasn't been modified; use the current copy */
        return (filename);
    default :
        return (NULL);
  }

 /*
  * OK, we need to copy the file; open the file and copy it...
  */

  unlink(filename);
  if ((fp = fopen(filename, "w")) == NULL)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    httpClose(cups_server);
    cups_server = NULL;
    return (NULL);
  }

  while ((bytes = httpRead(cups_server, buffer, sizeof(buffer))) > 0)
    fwrite(buffer, bytes, 1, fp);

  fclose(fp);

  return (filename);
}


/*
 * 'cupsGetPrinters()' - Get a list of printers.
 */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers)	/* O - Printers */
{
  int		n;		/* Number of printers */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */


 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
    return (NULL);

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                      "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                      "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  n         = 0;
  *printers = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/printers/")) != NULL)
  {
    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (strcmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  *printers = malloc(sizeof(char *));
	else
	  *printers = realloc(*printers, sizeof(char *) * (n + 1));

	if (*printers == NULL)
	{
	  ippDelete(response);
	  return (0);
	}

        (*printers)[n] = strdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }

  return (n);
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFile(char          *name,	/* I - Printer or class name */
              char          *filename,	/* I - File to print */
	      char          *title,	/* I - Title of job */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  int		i;			/* Looping var */
  int		n, n2;			/* Attribute values */
  char		*option,		/* Name of option */
		*val,			/* Pointer to option value */
		*s;			/* Pointer into option value */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP job-id attribute */
  char		hostname[HTTP_MAX_URI],	/* Hostname */
		printer[HTTP_MAX_URI],	/* Printer or class name */
		uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t	*language;		/* Language to use */
  struct stat	filestats;		/* File information */
  FILE		*fp;			/* File pointer */
  char		buffer[8192];		/* Copy buffer */
  int		jobid;			/* New job ID */


  DEBUG_printf(("cupsPrintFile(\'%s\', \'%s\', %d, %08x)\n",
                printer, filename, num_options, options));

  if (name == NULL || filename == NULL)
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

  if (!cups_connect(name, printer, hostname))
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

  sprintf(uri, "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddString(request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
               NULL, "application/octet-stream");

#if defined(WIN32) || defined(__EMX__)
  ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAME, "requesting-user-name",
               NULL, "WindowsUser");
#else
  ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAME, "requesting-user-name",
               NULL, cuserid(NULL));
#endif /* WIN32 || __EMX__ */

  if (title)
    ippAddString(request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL, title);

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

    option = options[i].name;
    val    = options[i].value;

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
      if (strncmp(option, "no", 2) == 0)
      {
	option += 2;
	n      = 0;
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
                    option, val));

      ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, option, NULL, val);
    }
    else if (val != NULL)
    {
     /*
      * Numeric value, range, or resolution...
      */

      if (*s == '-')
      {
        n2 = strtol(s + 1, NULL, 0);
        ippAddRange(request, IPP_TAG_JOB, option, n, n2);

	DEBUG_printf(("cupsPrintJob: Adding range option \'%s\' with value %d-%d...\n",
                      option, n, n2));
      }
      else if (*s == 'x')
      {
        n2 = strtol(s + 1, &s, 0);

	if (strcmp(s, "dpc") == 0)
          ippAddResolution(request, IPP_TAG_JOB, option, IPP_RES_PER_CM, n, n2);
        else if (strcmp(s, "dpi") == 0)
          ippAddResolution(request, IPP_TAG_JOB, option, IPP_RES_PER_INCH, n, n2);
        else
          ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, option, NULL, val);

	DEBUG_printf(("cupsPrintJob: Adding resolution option \'%s\' with value %s...\n",
                      option, val));
      }
      else
      {
        ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, option, n);

	DEBUG_printf(("cupsPrintJob: Adding integer option \'%s\' with value %d...\n",
                      option, n));
      }
    }
    else
    {
     /*
      * Boolean value...
      */

      DEBUG_printf(("cupsPrintJob: Adding boolean option \'%s\' with value %d...\n",
                    option, n));
      ippAddBoolean(request, IPP_TAG_JOB, option, (char)n);
    }
  }

 /*
  * Setup the necessary HTTP fields...
  */

  httpClearFields(cups_server);
  httpSetField(cups_server, HTTP_FIELD_CONTENT_TYPE, "application/ipp");

  sprintf(buffer, "%u", (unsigned)(ippLength(request) + filestats.st_size));
  httpSetField(cups_server, HTTP_FIELD_CONTENT_LENGTH, buffer);

 /*
  * Finally, issue a POST request for the printer and send the IPP data and
  * file.
  */

  sprintf(uri, "/printers/%s", printer);

  response = ippNew();

  if (httpPost(cups_server, uri))
  {
    DEBUG_puts("httpPost() failed.");
    jobid = 0;
  }
  else if (ippWrite(cups_server, request) == IPP_ERROR)
  {
    DEBUG_puts("ippWrite() failed.");
    jobid = 0;
  }
  else
  {
    while ((i = fread(buffer, 1, sizeof(buffer), fp)) > 0)
      if (httpWrite(cups_server, buffer, i) < i)
      {
        DEBUG_puts("httpWrite() failed.");

	fclose(fp);
	ippDelete(request);
	ippDelete(response);
	httpClose(cups_server);
	return (0);
      }

    httpWrite(cups_server, buffer, 0);

    if (httpUpdate(cups_server) == HTTP_ERROR)
    {
      DEBUG_printf(("httpUpdate() failed (%d).\n", status));
      jobid = 0;
    }
    else if ((ippRead(cups_server, response)) == IPP_ERROR)
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
 * 'cups_connect()' - Connect to the specified host...
 */

static char *			/* I - Printer name if success, NULL if fail */
cups_connect(char *name,	/* I - Destination (printer[@host]) */
	     char *printer,	/* O - Printer name */
             char *hostname)	/* O - Hostname */
{
  char		hostbuf[HTTP_MAX_URI];
				/* Name of host */
  static char	printerbuf[HTTP_MAX_URI];
				/* Name of printer or class */


  if (name == NULL)
    return (NULL);

  if (sscanf(name, "%[^@]@%s", printerbuf, hostbuf) == 1)
    strcpy(hostbuf, "localhost");

  if (hostname != NULL)
    strcpy(hostname, hostbuf);
  else
    hostname = hostbuf;

  if (printer != NULL)
    strcpy(printer, printerbuf);
  else
    printer = printerbuf;

  if (cups_server != NULL)
  {
    if (strcasecmp(cups_server->hostname, hostname) == 0)
      return (printer);

    httpClose(cups_server);
  }

  if ((cups_server = httpConnect(hostname, ippPort())) == NULL)
    return (NULL);
  else
    return (printer);
}


/*
 * End of "$Id: util.c,v 1.11 1999/04/21 14:49:50 mike Exp $".
 */
