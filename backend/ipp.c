/*
 * "$Id: ipp.c,v 1.20 2000/02/02 00:50:41 mike Exp $"
 *
 *   IPP backend for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main() - Send a file to the printer or server.
 */

/*
 * Include necessary headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/string.h>


/*
 * 'main()' - Send a file to the printer or server.
 *
 * Usage:
 *
 *    printer-uri job-id user title copies options [file]
 */

int			/* O - Exit status */
main(int  argc,		/* I - Number of command-line arguments (6 or 7) */
     char *argv[])	/* I - Command-line arguments */
{
  int		i;		/* Looping var */
  int		n, n2;		/* Attribute values */
  char		*option,	/* Name of option */
		*val,		/* Pointer to option value */
		*s;		/* Pointer into option value */
  int		num_options;	/* Number of printer options */
  cups_option_t	*options;	/* Printer options */
  char		method[255],	/* Method in URI */
		hostname[1024],	/* Hostname */
		username[255],	/* Username info */
		resource[1024],	/* Resource info (printer name) */
		filename[1024];	/* File to print */
  int		port;		/* Port number (not used) */
  char		password[255],	/* Password info */
		uri[HTTP_MAX_URI];/* Updated URI without user/pass */
  http_status_t	status;		/* Status of HTTP job */
  ipp_status_t	ipp_status;	/* Status of IPP request */
  FILE		*fp;		/* File to print */
  http_t	*http;		/* HTTP connection */
  ipp_t		*request,	/* IPP request */
		*response;	/* IPP response */
  ipp_attribute_t *job_id;	/* job-id attribute */
  ipp_attribute_t *copies_sup;	/* copies-supported attribute */
  cups_lang_t	*language;	/* Default language */
  struct stat	fileinfo;	/* File statistics */
  size_t	nbytes,		/* Number of bytes written */
		tbytes;		/* Total bytes written */
  char		buffer[8192];	/* Output buffer */
  int		copies;		/* Number of copies remaining */


  if (argc == 1)
  {
    puts("network ipp \"\" \"Internet Printing Protocol\"");
    return (0);
  }
  else if (argc < 6 || argc > 7)
  {
    fprintf(stderr, "Usage: %s job-id user title copies options [file]\n",
            argv[0]);
    return (1);
  }

 /*
  * If we have 7 arguments, print the file named on the command-line.
  * Otherwise, copy stdin to a temporary file and print the temporary
  * file.
  */

  if (argc == 6)
  {
   /*
    * Copy stdin to a temporary file...
    */

    FILE *fp;		/* Temporary file */
    char buffer[8192];	/* Buffer for copying */
    int  bytes;		/* Number of bytes read */


    if ((fp = fopen(cupsTempFile(filename, sizeof(filename)), "w")) == NULL)
    {
      perror("ERROR: unable to create temporary file");
      return (1);
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), stdin)) > 0)
      if (fwrite(buffer, 1, bytes, fp) < bytes)
      {
        perror("ERROR: unable to write to temporary file");
	fclose(fp);
	unlink(filename);
	return (1);
      }

    fclose(fp);
  }
  else
  {
    strncpy(filename, argv[6], sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
  }

 /*
  * Open the print file...
  */

  if ((fp = fopen(filename, "rb")) == NULL)
  {
    perror("ERROR: Unable to open print file");
    return (1);
  }
  else
    stat(filename, &fileinfo);

 /*
  * Extract the hostname and printer name from the URI...
  */

  httpSeparate(argv[0], method, username, hostname, &port, resource);

 /*
  * Try connecting to the remote server...
  */

  do
  {
    fprintf(stderr, "INFO: Connecting to %s...\n", hostname);

    if ((http = httpConnect(hostname, port)) == NULL)
      if (errno == ECONNREFUSED)
      {
	fprintf(stderr, "INFO: Network host \'%s\' is busy; will retry in 30 seconds...",
                hostname);
	sleep(30);
      }
      else
      {
	perror("ERROR: Unable to connect to IPP host");
	sleep(30);
      }
  }
  while (http == NULL);

 /*
  * Build a URI for the printer and fill the standard IPP attributes for
  * an IPP_PRINT_FILE request.  We can't use the URI in argv[0] because it
  * might contain username:password information...
  */

  snprintf(uri, sizeof(uri), "%s://%s:%d%s", method, hostname, port, resource);

 /*
  * First validate the destination and see if the device supports multiple
  * copies.  We have to do this because some IPP servers (e.g. HP JetDirect)
  * don't support the copies attribute...
  */

  language   = cupsLangDefault();
  copies_sup = NULL;

  do
  {
   /*
    * Build the IPP request...
    */

    request = ippNew();
    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL,
        	 language != NULL ? language->language : "C");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

   /*
    * Now fill in the HTTP request stuff...
    */

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    if (username[0])
    {
      httpEncode64(password, username);
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, password);
    }

    sprintf(buffer, "%u", ippLength(request));
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, buffer);

   /*
    * Do the request...
    */

    for (;;)
    {
     /*
      * POST the request, retrying as needed...
      */

      if (httpPost(http, resource))
      {
	fputs("INFO: Unable to POST get-printer-attributes request; retrying...\n", stderr);
	sleep(10);
	httpReconnect(http);
	continue;
      }

      fputs("INFO: POST successful, sending IPP request...\n", stderr);

     /*
      * Send the IPP request...
      */

      request->state = IPP_IDLE;

      if (ippWrite(http, request) == IPP_ERROR)
      {
	fputs("ERROR: Unable to send IPP request!\n", stderr);
	status = HTTP_ERROR;
	break;
      }

      fputs("INFO: IPP request sent, getting status...\n", stderr);

     /*
      * Finally, check the status from the HTTP server...
      */

      while ((status = httpUpdate(http)) == HTTP_CONTINUE);

      if (status == HTTP_OK)
      {
	response = ippNew();
	ippRead(http, response);

	ipp_status = response->request.status.status_code;
       
	if (ipp_status > IPP_OK_CONFLICT)
	{
	  if (ipp_status == IPP_PRINTER_BUSY ||
	      ipp_status == IPP_SERVICE_UNAVAILABLE)
	  {
	    fputs("INFO: Printer busy; will retry in 10 seconds...\n", stderr);
	    sleep(10);
	  }
	  else
	  {
	    fprintf(stderr, "ERROR: Printer will not accept print file (%x)!\n",
	            ipp_status);
            status = HTTP_ERROR;
	  }
	}
	else if ((copies_sup = ippFindAttribute(response, "copies-supported",
	                                        IPP_TAG_RANGE)) != NULL)
        {
	 /*
	  * Has the "copies-supported" attribute - does it have an upper
	  * bound > 1?
	  */

	  if (copies_sup->values[0].range.upper <= 1)
	    copies_sup = NULL; /* No */
        }

	ippDelete(response);
      }
      else
      {
	if (status == HTTP_ERROR)
	{
          fprintf(stderr, "WARNING: Did not receive the IPP response (%d)\n",
	          errno);
	  status     = HTTP_OK;
	  ipp_status = IPP_PRINTER_BUSY;
	}
	else
          fprintf(stderr, "ERROR: Validate request was not accepted (%d)!\n", status);
      }

      httpFlush(http);

      break;
    }

    if (status != HTTP_OK)
    {
      if (fp != stdin)
	fclose(fp);

      httpClose(http);

      return (1);
    }
  }
  while (ipp_status > IPP_OK_CONFLICT);

 /*
  * See if the printer supports multiple copies...
  */

  if (copies_sup)
    copies = 1;
  else
    copies = atoi(argv[4]);

 /*
  * Then issue the print-job request...
  */

  while (copies > 0)
  {
   /*
    * Build the IPP request...
    */

    request = ippNew();
    request->request.op.operation_id = IPP_PRINT_JOB;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL,
        	 language != NULL ? language->language : "C");

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    fprintf(stderr, "DEBUG: printer-uri = \"%s\"\n", uri);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	 NULL, argv[2]);

    fprintf(stderr, "DEBUG: requesting-user-name = \"%s\"\n", argv[2]);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
        	 argv[3]);

    fprintf(stderr, "DEBUG: job-name = \"%s\"\n", argv[3]);

   /*
    * Handle options on the command-line...
    */

    options     = NULL;
    num_options = cupsParseOptions(argv[5], 0, &options);

    if (cupsGetOption("raw", num_options, options))
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	   NULL, "application/vnd.cups-raw");
    else
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	   NULL, "application/octet-stream");

    if (copies_sup)
      ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, "copies", atoi(argv[4]));

    for (i = 0; i < num_options; i ++)
    {
     /*
      * Skip the "raw" option - handled above...
      */

      if (strcasecmp(options[i].name, "raw") == 0)
	continue;

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
	if (strncasecmp(option, "no", 2) == 0)
	{
	  option += 2;
	  n      = 0;
	}
	else
          n = 1;

	s = "";
      }

      if (*s != '\0' && *s != '-' && (*s != 'x' || s == val))
       /*
	* String value(s)...
	*/
	ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, option, NULL, val);
      else if (val != NULL)
      {
       /*
	* Numeric value, range, or resolution...
	*/

	if (*s == '-')
	{
          n2 = strtol(s + 1, NULL, 0);
          ippAddRange(request, IPP_TAG_JOB, option, n, n2);
	}
	else if (*s == 'x')
	{
          n2 = strtol(s + 1, &s, 0);

	  if (strcasecmp(s, "dpc") == 0)
            ippAddResolution(request, IPP_TAG_JOB, option, IPP_RES_PER_CM, n, n2);
          else if (strcasecmp(s, "dpi") == 0)
            ippAddResolution(request, IPP_TAG_JOB, option, IPP_RES_PER_INCH, n, n2);
          else
            ippAddString(request, IPP_TAG_JOB, IPP_TAG_KEYWORD, option, NULL, val);
	}
	else
          ippAddInteger(request, IPP_TAG_JOB, IPP_TAG_INTEGER, option, n);
      }
      else
       /*
	* Boolean value...
	*/
	ippAddBoolean(request, IPP_TAG_JOB, option, (char)n);
    }

   /*
    * Now fill in the HTTP request stuff...
    */

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    if (username[0])
    {
      httpEncode64(password, username);
      httpSetField(http, HTTP_FIELD_AUTHORIZATION, password);
    }

    sprintf(buffer, "%u", ippLength(request) + (size_t)fileinfo.st_size);
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, buffer);

   /*
    * Do the request...
    */

    for (;;)
    {
     /*
      * POST the request, retrying as needed...
      */

      httpReconnect(http);

      if (httpPost(http, resource))
      {
	fputs("INFO: Unable to POST print request; retrying...\n", stderr);
	sleep(10);
	continue;
      }

      fputs("INFO: POST successful, sending IPP request...\n", stderr);

     /*
      * Send the IPP request...
      */

      request->state = IPP_IDLE;

      if (ippWrite(http, request) == IPP_ERROR)
      {
	fputs("ERROR: Unable to send IPP request!\n", stderr);
	status = HTTP_ERROR;
	break;
      }

      fputs("INFO: IPP request sent, sending print file...\n", stderr);

     /*
      * Then send the file...
      */

      rewind(fp);

      tbytes = 0;
      while ((nbytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
      {
	tbytes += nbytes;
	fprintf(stderr, "INFO: Sending print file, %uk...\n", tbytes / 1024);

	if (httpWrite(http, buffer, nbytes) < nbytes)
	{
          perror("ERROR: Unable to send print file to printer");
	  status = HTTP_ERROR;
          break;
	}
      }

      fputs("INFO: Print file sent; checking status...\n", stderr);

     /*
      * Finally, check the status from the HTTP server...
      */

      while ((status = httpUpdate(http)) == HTTP_CONTINUE);

      if (status == HTTP_OK)
      {
	response = ippNew();
	ippRead(http, response);

	if ((ipp_status = response->request.status.status_code) > IPP_OK_CONFLICT)
	{
          if (ipp_status == IPP_SERVICE_UNAVAILABLE ||
	      ipp_status == IPP_PRINTER_BUSY)
	  {
	    fputs("INFO: Printer is busy; retrying print job...\n", stderr);
	    sleep(10);
	  }
	  else
            fprintf(stderr, "ERROR: Print file was not accepted (%04x)!\n",
	            response->request.status.status_code);
	}
	else if ((job_id = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
          fputs("INFO: Print file accepted - job ID unknown.\n", stderr);
	else
          fprintf(stderr, "INFO: Print file accepted - job ID %d.\n",
	          job_id->values[0].integer);
      }
      else
      {
	response   = NULL;
	ipp_status = IPP_PRINTER_BUSY;

	if (status == HTTP_ERROR)
	{
          fprintf(stderr, "WARNING: Did not receive the IPP response (%d)\n",
	          errno);
	  status = HTTP_OK;
	}
	else
          fprintf(stderr, "ERROR: Print request was not accepted (%d)!\n", status);
      }

      httpFlush(http);

      break;
    }

    if (request != NULL)
      ippDelete(request);
    if (response != NULL)
      ippDelete(response);

    if (ipp_status <= IPP_OK_CONFLICT)
    {
      fprintf(stderr, "PAGE: 1 %d\n", copies_sup ? atoi(argv[4]) : 1);
      copies --;
    }
    else if (ipp_status != IPP_SERVICE_UNAVAILABLE &&
	     ipp_status != IPP_PRINTER_BUSY)
      break;
  }

 /*
  * Free memory...
  */

  httpClose(http);

 /*
  * Close and remove the temporary file if necessary...
  */

  fclose(fp);

  if (argc < 7)
    unlink(filename);

 /*
  * Return the queue status...
  */

  return (status != HTTP_OK);
}


/*
 * End of "$Id: ipp.c,v 1.20 2000/02/02 00:50:41 mike Exp $".
 */
