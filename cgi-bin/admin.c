/*
 * "$Id: admin.c,v 1.22.2.15 2002/08/16 19:20:42 mike Exp $"
 *
 *   Administration CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()              - Main entry for CGI.
 *   do_am_class()       - Add or modify a class.
 *   do_am_printer()     - Add or modify a printer.
 *   do_config_printer() - Configure the default options for a printer.
 *   do_delete_class()   - Delete a class...
 *   do_delete_printer() - Delete a printer...
 *   do_printer_op()     - Do a printer operation.
 *   get_line()          - Get a line that is terminated by a LF, CR, or CR LF.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"
#include <ctype.h>
#include <errno.h>


/*
 * Local functions...
 */

static void	do_am_class(http_t *http, cups_lang_t *language, int modify);
static void	do_am_printer(http_t *http, cups_lang_t *language, int modify);
static void	do_config_printer(http_t *http, cups_lang_t *language);
static void	do_delete_class(http_t *http, cups_lang_t *language);
static void	do_delete_printer(http_t *http, cups_lang_t *language);
static void	do_printer_op(http_t *http, cups_lang_t *language, ipp_op_t op);
static char	*get_line(char *buf, int length, FILE *fp);


/*
 * 'main()' - Main entry for CGI.
 */

int				/* O - Exit status */
main(int  argc,			/* I - Number of command-line arguments */
     char *argv[])		/* I - Command-line arguments */
{
  cups_lang_t	*language;	/* Language information */
  http_t	*http;		/* Connection to the server */
  const char	*op;		/* Operation name */


 /*
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Send a standard header...
  */

  printf("Content-Type: text/html;charset=%s\n\n", cupsLangEncoding(language));

  cgiSetVariable("TITLE", "Admin");
  ippSetServerVersion();

  cgiCopyTemplateLang(stdout, TEMPLATES, "header.tmpl", getenv("LANG"));

 /*
  * See if we have form data...
  */

  if (!cgiInitialize())
  {
   /*
    * Nope, send the administration menu...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "admin.tmpl", getenv("LANG"));
  }
  else if ((op = cgiGetVariable("OP")) != NULL)
  {
   /*
    * Connect to the HTTP server...
    */

    http = httpConnectEncrypt("localhost", ippPort(), cupsEncryption());

   /*
    * Do the operation...
    */

    if (strcmp(op, "start-printer") == 0)
      do_printer_op(http, language, IPP_RESUME_PRINTER);
    else if (strcmp(op, "stop-printer") == 0)
      do_printer_op(http, language, IPP_PAUSE_PRINTER);
    else if (strcmp(op, "accept-jobs") == 0)
      do_printer_op(http, language, CUPS_ACCEPT_JOBS);
    else if (strcmp(op, "reject-jobs") == 0)
      do_printer_op(http, language, CUPS_REJECT_JOBS);
    else if (strcmp(op, "purge-jobs") == 0)
      do_printer_op(http, language, IPP_PURGE_JOBS);
    else if (strcmp(op, "add-class") == 0)
      do_am_class(http, language, 0);
    else if (strcmp(op, "add-printer") == 0)
      do_am_printer(http, language, 0);
    else if (strcmp(op, "modify-class") == 0)
      do_am_class(http, language, 1);
    else if (strcmp(op, "modify-printer") == 0)
      do_am_printer(http, language, 1);
    else if (strcmp(op, "delete-class") == 0)
      do_delete_class(http, language);
    else if (strcmp(op, "delete-printer") == 0)
      do_delete_printer(http, language);
    else if (strcmp(op, "config-printer") == 0)
      do_config_printer(http, language);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "admin-op.tmpl", getenv("LANG"));
    }

   /*
    * Close the HTTP server connection...
    */

    httpClose(http);
  }
  else
  {
   /*
    * Form data but no operation code...  Display an error...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "admin-op.tmpl", getenv("LANG"));
  }

 /*
  * Send the standard trailer...
  */

  cgiCopyTemplateLang(stdout, TEMPLATES, "trailer.tmpl", getenv("LANG"));

 /*
  * Free the request language...
  */

  cupsLangFree(language);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'do_am_class()' - Add or modify a class.
 */

static void
do_am_class(http_t      *http,		/* I - HTTP connection */
            cups_lang_t *language,	/* I - Client's language */
	    int         modify)		/* I - Modify the printer? */
{
  int		i, j;			/* Looping vars */
  int		element;		/* Element number */
  int		num_printers;		/* Number of printers */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* member-uris attribute */
  ipp_status_t	status;			/* Request status */
  char		uri[HTTP_MAX_URI];	/* Device or printer URI */
  const char	*name,			/* Pointer to class name */
		*ptr;			/* Pointer to CGI variable */


  if (cgiGetVariable("PRINTER_LOCATION") == NULL)
  {
    if (modify)
    {
     /*
      * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
      * following attributes:
      *
      *    attributes-charset
      *    attributes-natural-language
      *    printer-uri
      */

      request = ippNew();

      request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
      request->request.op.request_id   = 1;

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL, language->language);

      snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s",
               cgiGetVariable("PRINTER_NAME"));
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                   NULL, uri);

     /*
      * Do the request and get back a response...
      */

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
      {
	ippSetCGIVars(response, NULL, NULL);
	ippDelete(response);
      }

     /*
      * Update the location and description of an existing printer...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "modify-class.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "add-class.tmpl", getenv("LANG"));
    }

    return;
  }

  name = cgiGetVariable("PRINTER_NAME");
  for (ptr = name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/')
      break;

  if (*ptr || ptr == name || strlen(name) > 127)
  {
    cgiSetVariable("ERROR", "The class name may only contain up to 127 printable "
                            "characters.");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

  if (cgiGetVariable("MEMBER_URIS") == NULL)
  {
   /*
    * Build a CUPS_GET_PRINTERS request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_GET_PRINTERS;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/printers");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
     /*
      * Create MEMBER_URIS and MEMBER_NAMES arrays...
      */

      for (element = 0, attr = response->attrs;
	   attr != NULL;
	   attr = attr->next)
	if (attr->name && strcmp(attr->name, "printer-uri-supported") == 0)
	{
	  cgiSetArray("MEMBER_URIS", element, attr->values[0].string.text);
	  element ++;
	}

      for (element = 0, attr = response->attrs;
	   attr != NULL;
	   attr = attr->next)
	if (attr->name && strcmp(attr->name, "printer-name") == 0)
	{
	  cgiSetArray("MEMBER_NAMES", element, attr->values[0].string.text);
	  element ++;
	}

      num_printers = cgiGetSize("MEMBER_URIS");

      ippDelete(response);
    }
    else
      num_printers = 0;

   /*
    * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      if ((attr = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
      {
       /*
        * Mark any current members in the class...
	*/

        for (j = 0; j < num_printers; j ++)
	  cgiSetArray("MEMBER_SELECTED", j, "");

        for (i = 0; i < attr->num_values; i ++)
	  for (j = 0; j < num_printers; j ++)
	    if (strcmp(attr->values[i].string.text, cgiGetArray("MEMBER_URIS", j)) == 0)
	    {
	      cgiSetArray("MEMBER_SELECTED", j, "SELECTED");
	      break;
	    }
      }

      ippDelete(response);
    }

   /*
    * Let the user choose...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-members.tmpl", getenv("LANG"));
  }
  else
  {
   /*
    * Build a CUPS_ADD_CLASS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    printer-location
    *    printer-info
    *    printer-is-accepting-jobs
    *    printer-state
    *    member-uris
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_CLASS;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location",
                 NULL, cgiGetVariable("PRINTER_LOCATION"));

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
                 NULL, cgiGetVariable("PRINTER_INFO"));

    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                  IPP_PRINTER_IDLE);

    if ((num_printers = cgiGetSize("MEMBER_URIS")) > 0)
    {
      attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_URI, "member-uris",
                           num_printers, NULL, NULL);
      for (i = 0; i < num_printers; i ++)
        attr->values[i].string.text = strdup(cgiGetArray("MEMBER_URIS", i));
    }

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
    {
      status = response->request.status.status_code;
      ippDelete(response);
    }
    else
      status = IPP_NOT_AUTHORIZED;

    if (status > IPP_OK_CONFLICT)
    {
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else if (modify)
      cgiCopyTemplateLang(stdout, TEMPLATES, "class-modified.tmpl", getenv("LANG"));
    else
      cgiCopyTemplateLang(stdout, TEMPLATES, "class-added.tmpl", getenv("LANG"));
  }
}


/*
 * 'do_am_printer()' - Add or modify a printer.
 */

static void
do_am_printer(http_t      *http,	/* I - HTTP connection */
              cups_lang_t *language,	/* I - Client's language */
	      int         modify)	/* I - Modify the printer? */
{
  int		i;			/* Looping var */
  int		element;		/* Element number */
  ipp_attribute_t *attr,		/* Current attribute */
		*last;			/* Last attribute */
  ipp_t		*request,		/* IPP request */
		*response,		/* IPP response */
		*oldinfo;		/* Old printer information */
  ipp_status_t	status;			/* Request status */
  const char	*var;			/* CGI variable */
  char		uri[HTTP_MAX_URI],	/* Device or printer URI */
		*uriptr;		/* Pointer into URI */
  int		maxrate;		/* Maximum baud rate */
  char		baudrate[255];		/* Baud rate string */
  char		make[255];		/* Make string */
  const char	*name,			/* Pointer to class name */
		*ptr;			/* Pointer to CGI variable */
  static int	baudrates[] =		/* Baud rates */
		{
		  1200,
		  2400,
		  4800,
		  9600,
		  19200,
		  38400,
		  57600,
		  115200,
		  230400,
		  460800
		};


  if (modify)
  {
   /*
    * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

   /*
    * Do the request and get back a response...
    */

    oldinfo = cupsDoRequest(http, request, "/");
  }
  else
    oldinfo = NULL;

  if ((name = cgiGetVariable("PRINTER_NAME")) == NULL ||
      cgiGetVariable("PRINTER_LOCATION") == NULL)
  {
    if (modify)
    {
     /*
      * Update the location and description of an existing printer...
      */

      if (oldinfo)
	ippSetCGIVars(oldinfo, NULL, NULL);

      cgiCopyTemplateLang(stdout, TEMPLATES, "modify-printer.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "add-printer.tmpl", getenv("LANG"));
    }

    if (oldinfo)
      ippDelete(oldinfo);

    return;
  }

  for (ptr = name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/')
      break;

  if (*ptr || ptr == name || strlen(name) > 127)
  {
    cgiSetVariable("ERROR", "The printer name may only contain up to 127 printable "
                            "characters.");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

  if ((var = cgiGetVariable("DEVICE_URI")) == NULL)
  {
   /*
    * Build a CUPS_GET_DEVICES request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_GET_DEVICES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/printers/");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      ippSetCGIVars(response, NULL, NULL);
      ippDelete(response);
    }

   /*
    * Let the user choose...
    */

    if (oldinfo &&
        (attr = ippFindAttribute(oldinfo, "device-uri", IPP_TAG_URI)) != NULL)
    {
      strlcpy(uri, attr->values[0].string.text, sizeof(uri));
      if ((uriptr = strchr(uri, ':')) != NULL && strncmp(uriptr, "://", 3) == 0)
        *uriptr = '\0';

      cgiSetVariable("CURRENT_DEVICE_URI", uri);
    }

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-device.tmpl", getenv("LANG"));
  }
  else if (strchr(var, '/') == NULL)
  {
    if (oldinfo &&
        (attr = ippFindAttribute(oldinfo, "device-uri", IPP_TAG_URI)) != NULL)
    {
     /*
      * Set the current device URI for the form to the old one...
      */

      if (strncmp(attr->values[0].string.text, var, strlen(var)) == 0)
	cgiSetVariable("DEVICE_URI", attr->values[0].string.text);
    }

   /*
    * User needs to set the full URI...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-uri.tmpl", getenv("LANG"));
  }
  else if (strncmp(var, "serial:", 7) == 0 && cgiGetVariable("BAUDRATE") == NULL)
  {
   /*
    * Need baud rate, parity, etc.
    */

    if ((var = strchr(var, '?')) != NULL &&
        strncmp(var, "?baud=", 6) == 0)
      maxrate = atoi(var + 6);
    else
      maxrate = 19200;

    for (i = 0; i < 10; i ++)
      if (baudrates[i] > maxrate)
        break;
      else
      {
        sprintf(baudrate, "%d", baudrates[i]);
	cgiSetArray("BAUDRATES", i, baudrate);
      }

    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-serial.tmpl", getenv("LANG"));
  }
  else if ((var = cgiGetVariable("PPD_NAME")) == NULL)
  {
    if (modify)
    {
     /*
      * Get the PPD file...
      */

      int		fd;			/* PPD file */
      char		filename[1024];		/* PPD filename */
      ppd_file_t	*ppd;			/* PPD information */
      char		buffer[1024];		/* Buffer */
      int		bytes;			/* Number of bytes */


      snprintf(uri, sizeof(uri), "/printers/%s.ppd", name);

      if (httpGet(http, uri))
        httpGet(http, uri);

      while (httpUpdate(http) == HTTP_CONTINUE);

      if ((fd = cupsTempFd(filename, sizeof(filename))) >= 0)
      {
	while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
          write(fd, buffer, bytes);

	close(fd);

        if ((ppd = ppdOpenFile(filename)) != NULL)
	{
	  if (ppd->manufacturer)
	    cgiSetVariable("CURRENT_MAKE", ppd->manufacturer);
	  if (ppd->nickname)
	    cgiSetVariable("CURRENT_MAKE_AND_MODEL", ppd->nickname);

          ppdClose(ppd);
	}

        unlink(filename);
      }
      else
        httpFlush(http);
    }

   /*
    * Build a CUPS_GET_PPDS request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_GET_PPDS;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/printers/");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      if ((var = cgiGetVariable("PPD_MAKE")) == NULL)
      {
       /*
	* Let the user choose a make...
	*/

        for (element = 0, attr = response->attrs, last = NULL;
	     attr != NULL;
	     attr = attr->next)
	  if (attr->name && strcmp(attr->name, "ppd-make") == 0)
	    if (last == NULL ||
	        strcasecmp(last->values[0].string.text,
		           attr->values[0].string.text) != 0)
	    {
	      cgiSetArray("PPD_MAKE", element, attr->values[0].string.text);
	      element ++;
	      last = attr;
	    }

	cgiCopyTemplateLang(stdout, TEMPLATES, "choose-make.tmpl",
	                    getenv("LANG"));
      }
      else
      {
       /*
	* Let the user choose a model...
	*/

        strlcpy(make, var, sizeof(make));

        ippSetCGIVars(response, "ppd-make", make);
	cgiCopyTemplateLang(stdout, TEMPLATES, "choose-model.tmpl",
	                    getenv("LANG"));
      }

      ippDelete(response);
    }
    else
    {
      char message[1024];


      snprintf(message, sizeof(message), "Unable to get list of printer drivers: %s",
               ippErrorString(cupsLastError()));
      cgiSetVariable("ERROR", message);
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
  }
  else
  {
   /*
    * Build a CUPS_ADD_PRINTER request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    printer-location
    *    printer-info
    *    ppd-name
    *    device-uri
    *    printer-is-accepting-jobs
    *    printer-state
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_PRINTER;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location",
                 NULL, cgiGetVariable("PRINTER_LOCATION"));

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
                 NULL, cgiGetVariable("PRINTER_INFO"));

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "ppd-name",
                 NULL, cgiGetVariable("PPD_NAME"));

    strlcpy(uri, cgiGetVariable("DEVICE_URI"), sizeof(uri));
    if (strncmp(uri, "serial:", 7) == 0)
    {
     /*
      * Update serial port URI to include baud rate, etc.
      */

      if ((uriptr = strchr(uri, '?')) == NULL)
        uriptr = uri + strlen(uri);

      snprintf(uriptr, sizeof(uri) - (uriptr - uri),
               "?baud=%s+bits=%s+parity=%s+flow=%s",
               cgiGetVariable("BAUDRATE"), cgiGetVariable("BITS"),
	       cgiGetVariable("PARITY"), cgiGetVariable("FLOW"));
    }

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_URI, "device-uri",
                 NULL, uri);

    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-accepting-jobs", 1);

    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                  IPP_PRINTER_IDLE);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
    {
      status = response->request.status.status_code;
      ippDelete(response);
    }
    else
      status = IPP_NOT_AUTHORIZED;

    if (status > IPP_OK_CONFLICT)
    {
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else if (modify)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-modified.tmpl", getenv("LANG"));
    else
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-added.tmpl", getenv("LANG"));
  }

  if (oldinfo)
    ippDelete(oldinfo);
}


/*
 * 'do_config_printer()' - Configure the default options for a printer.
 */

static void
do_config_printer(http_t      *http,	/* I - HTTP connection */
                  cups_lang_t *language)/* I - Client's language */
{
  int		i, j, k;		/* Looping vars */
  int		have_options;		/* Have options? */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*var;			/* Variable value */
  const char	*printer;		/* Printer printer name */
  ipp_status_t	status;			/* Operation status... */
  const char	*filename;		/* PPD filename */
  char		tempfile[1024];		/* Temporary filename */
  FILE		*in,			/* Input file */
		*out;			/* Output file */
  int		outfd;			/* Output file descriptor */
  char		line[1024];		/* Line from PPD file */
  char		keyword[1024],		/* Keyword from Default line */
		*keyptr;		/* Pointer into keyword... */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_group_t	*group;			/* Option group */
  ppd_option_t	*option;		/* Option */


 /*
  * Get the printer name...
  */

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

 /*
  * Get the PPD file...
  */

  if ((filename = cupsGetPPD(printer)) == NULL)
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

  ppd = ppdOpenFile(filename);

  if (cgiGetVariable("job_sheets_start") != NULL ||
      cgiGetVariable("job_sheets_end") != NULL)
    have_options = 1;
  else
    have_options = 0;

  ppdMarkDefaults(ppd);

  DEBUG_printf(("<P>ppd->num_groups = %d\n"
                "<UL>\n", ppd->num_groups));

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
  {
    DEBUG_printf(("<LI>%s<UL>\n", group->text));

    for (j = group->num_options, option = group->options; j > 0; j --, option ++)
      if ((var = cgiGetVariable(option->keyword)) != NULL)
      {
        DEBUG_printf(("<LI>%s = \"%s\"</LI>\n", option->keyword, var));
        have_options = 1;
	ppdMarkOption(ppd, option->keyword, var);
      }
#ifdef DEBUG
      else
        printf("<LI>%s not defined!</LI>\n", option->keyword);
#endif /* DEBUG */

    DEBUG_puts("</UL></LI>");
  }

  DEBUG_printf(("</UL>\n"
                "<P>ppdConflicts(ppd) = %d\n", ppdConflicts(ppd)));

  if (!have_options || ppdConflicts(ppd))
  {
   /*
    * Show the options to the user...
    */

    cgiCopyTemplateLang(stdout, TEMPLATES, "config-printer.tmpl",
                        getenv("LANG"));

    if (ppdConflicts(ppd))
    {
      for (i = ppd->num_groups, k = 0, group = ppd->groups; i > 0; i --, group ++)
	for (j = group->num_options, option = group->options; j > 0; j --, option ++)
          if (option->conflicted)
	  {
	    cgiSetArray("ckeyword", k, option->keyword);
	    cgiSetArray("ckeytext", k, option->text);
	    k ++;
	  }

      cgiCopyTemplateLang(stdout, TEMPLATES, "option-conflict.tmpl",
                          getenv("LANG"));
    }

    for (i = ppd->num_groups, group = ppd->groups;
	 i > 0;
	 i --, group ++)
    {
      if (strcmp(group->name, "InstallableOptions") == 0)
	cgiSetVariable("GROUP",
	               cupsLangString(language, CUPS_MSG_OPTIONS_INSTALLED));
      else
	cgiSetVariable("GROUP", group->text);

      cgiCopyTemplateLang(stdout, TEMPLATES, "option-header.tmpl",
                          getenv("LANG"));
      
      for (j = group->num_options, option = group->options;
           j > 0;
	   j --, option ++)
      {
        if (strcmp(option->keyword, "PageRegion") == 0)
	  continue;

        cgiSetVariable("KEYWORD", option->keyword);
        cgiSetVariable("KEYTEXT", option->text);
	    
	if (option->conflicted)
	  cgiSetVariable("CONFLICTED", "1");
	else
	  cgiSetVariable("CONFLICTED", "0");

	cgiSetSize("CHOICES", option->num_choices);
	cgiSetSize("TEXT", option->num_choices);
	for (k = 0; k < option->num_choices; k ++)
	{
	  cgiSetArray("CHOICES", k, option->choices[k].choice);
	  cgiSetArray("TEXT", k, option->choices[k].text);

          if (option->choices[k].marked)
	    cgiSetVariable("DEFCHOICE", option->choices[k].choice);
	}

        switch (option->ui)
	{
	  case PPD_UI_BOOLEAN :
              cgiCopyTemplateLang(stdout, TEMPLATES, "option-boolean.tmpl",
	                          getenv("LANG"));
              break;
	  case PPD_UI_PICKONE :
              cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickone.tmpl",
	                          getenv("LANG"));
              break;
	  case PPD_UI_PICKMANY :
              cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickmany.tmpl",
	                          getenv("LANG"));
              break;
	}
      }

      cgiCopyTemplateLang(stdout, TEMPLATES, "option-trailer.tmpl",
                          getenv("LANG"));
    }

   /*
    * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNew();

    request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      if ((attr = ippFindAttribute(response, "job-sheets-supported", IPP_TAG_ZERO)) != NULL)
      {
       /*
	* Add the job sheets options...
	*/

	cgiSetVariable("GROUP", "Banners");
	cgiCopyTemplateLang(stdout, TEMPLATES, "option-header.tmpl",
                            getenv("LANG"));

	cgiSetSize("CHOICES", attr->num_values);
	cgiSetSize("TEXT", attr->num_values);
	for (k = 0; k < attr->num_values; k ++)
	{
	  cgiSetArray("CHOICES", k, attr->values[k].string.text);
	  cgiSetArray("TEXT", k, attr->values[k].string.text);
	}

        attr = ippFindAttribute(response, "job-sheets-default", IPP_TAG_ZERO);

        cgiSetVariable("KEYWORD", "job_sheets_start");
        cgiSetVariable("KEYTEXT", "Starting Banner");
        cgiSetVariable("DEFCHOICE", attr == NULL ?
	                            "" : attr->values[0].string.text);

	cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickone.tmpl",
	                    getenv("LANG"));

        cgiSetVariable("KEYWORD", "job_sheets_end");
        cgiSetVariable("KEYTEXT", "Ending Banner");
        cgiSetVariable("DEFCHOICE", attr == NULL && attr->num_values > 1 ?
	                            "" : attr->values[1].string.text);

	cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickone.tmpl",
	                    getenv("LANG"));

	cgiCopyTemplateLang(stdout, TEMPLATES, "option-trailer.tmpl",
                            getenv("LANG"));
      }

      ippDelete(response);
    }

    cgiCopyTemplateLang(stdout, TEMPLATES, "config-printer2.tmpl",
                        getenv("LANG"));
  }
  else
  {
   /*
    * Set default options...
    */

    outfd = cupsTempFd(tempfile, sizeof(tempfile));
    in    = fopen(filename, "rb");
    out   = fdopen(outfd, "wb");

    if (outfd < 0 || in == NULL || out == NULL)
    {
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      unlink(filename);
      return;
    }

    while (get_line(line, sizeof(line), in) != NULL)
    {
      if (strncmp(line, "*Default", 8) != 0)
        fprintf(out, "%s\n", line);
      else
      {
       /*
        * Get default option name...
	*/

        strlcpy(keyword, line + 8, sizeof(keyword));

	for (keyptr = keyword; *keyptr; keyptr ++)
	  if (*keyptr == ':' || isspace(*keyptr))
	    break;

        *keyptr = '\0';

        if (strcmp(keyword, "PageRegion") == 0)
	  var = cgiGetVariable("PageSize");
	else
	  var = cgiGetVariable(keyword);

        if (var != NULL)
	  fprintf(out, "*Default%s: %s\n", keyword, var);
	else
	  fprintf(out, "%s\n", line);
      }
    }

    fclose(in);
    fclose(out);
    close(outfd);

   /*
    * Build a CUPS_ADD_PRINTER request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    job-sheets-default
    *    [ppd file]
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_PRINTER;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s",
             cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
                         "job-sheets-default", 2, NULL, NULL);
    attr->values[0].string.text = strdup(cgiGetVariable("job_sheets_start"));
    attr->values[1].string.text = strdup(cgiGetVariable("job_sheets_end"));

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoFileRequest(http, request, "/admin/", tempfile)) != NULL)
    {
      status = response->request.status.status_code;
      ippDelete(response);
    }
    else
      status = IPP_NOT_AUTHORIZED;

    if (status > IPP_OK_CONFLICT)
    {
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-configured.tmpl", getenv("LANG"));

    unlink(tempfile);
  }

  unlink(filename);
}


/*
 * 'do_delete_class()' - Delete a class...
 */

static void
do_delete_class(http_t      *http,	/* I - HTTP connection */
                cups_lang_t *language)	/* I - Client's language */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*pclass;		/* Printer class name */
  ipp_status_t	status;			/* Operation status... */


  if (cgiGetVariable("CONFIRM") == NULL)
  {
    cgiCopyTemplateLang(stdout, TEMPLATES, "class-confirm.tmpl", getenv("LANG"));
    return;
  }

  if ((pclass = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/classes/%s", pclass);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

 /*
  * Build a CUPS_DELETE_CLASS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_DELETE_CLASS;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
  {
    status = response->request.status.status_code;

    ippDelete(response);
  }
  else
    status = IPP_GONE;

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "class-deleted.tmpl", getenv("LANG"));
}


/*
 * 'do_delete_printer()' - Delete a printer...
 */

static void
do_delete_printer(http_t      *http,	/* I - HTTP connection */
                  cups_lang_t *language)/* I - Client's language */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*printer;		/* Printer printer name */
  ipp_status_t	status;			/* Operation status... */


  if (cgiGetVariable("CONFIRM") == NULL)
  {
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-confirm.tmpl", getenv("LANG"));
    return;
  }

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

 /*
  * Build a CUPS_DELETE_PRINTER request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_DELETE_PRINTER;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
  {
    status = response->request.status.status_code;

    ippDelete(response);
  }
  else
    status = IPP_GONE;

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-deleted.tmpl", getenv("LANG"));
}


/*
 * 'do_printer_op()' - Do a printer operation.
 */

static void
do_printer_op(http_t      *http,	/* I - HTTP connection */
              cups_lang_t *language,	/* I - Client's language */
	      ipp_op_t    op)		/* I - Operation to perform */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer;		/* Printer name (purge-jobs) */
  ipp_status_t	status;			/* Operation status... */


  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    return;
  }

 /*
  * Build a printer request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  */

  request = ippNew();

  request->request.op.operation_id = op;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
  {
    status = response->request.status.status_code;

    ippDelete(response);
  }
  else
    status = IPP_GONE;

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else if (op == IPP_PAUSE_PRINTER)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-stop.tmpl", getenv("LANG"));
  else if (op == IPP_RESUME_PRINTER)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-start.tmpl", getenv("LANG"));
  else if (op == CUPS_ACCEPT_JOBS)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-accept.tmpl", getenv("LANG"));
  else if (op == CUPS_REJECT_JOBS)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-reject.tmpl", getenv("LANG"));
  else if (op == IPP_PURGE_JOBS)
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-purge.tmpl", getenv("LANG"));
}


/*
 * 'get_line()' - Get a line that is terminated by a LF, CR, or CR LF.
 */

static char *		/* O - Pointer to buf or NULL on EOF */
get_line(char *buf,	/* I - Line buffer */
         int  length,	/* I - Length of buffer */
	 FILE *fp)	/* I - File to read from */
{
  char	*bufptr;	/* Pointer into buffer */
  int	ch;		/* Character from file */


  length --;
  bufptr = buf;

  while ((ch = getc(fp)) != EOF)
  {
    if (ch == '\n')
      break;
    else if (ch == '\r')
    {
     /*
      * Look for LF...
      */

      ch = getc(fp);
      if (ch != '\n' && ch != EOF)
        ungetc(ch, fp);

      break;
    }

    *bufptr++ = ch;
    length --;
    if (length == 0)
      break;
  }

  *bufptr = '\0';

  if (ch == EOF)
    return (NULL);
  else
    return (buf);
}


/*
 * End of "$Id: admin.c,v 1.22.2.15 2002/08/16 19:20:42 mike Exp $".
 */
