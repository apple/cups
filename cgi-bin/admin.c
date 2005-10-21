/*
 * "$Id$"
 *
 *   Administration CGI for the Common UNIX Printing System (CUPS).
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
 * Contents:
 *
 *   main()                    - Main entry for CGI.
 *   compare_printer_devices() - Compare two printer devices.
 *   do_am_class()             - Add or modify a class.
 *   do_am_printer()           - Add or modify a printer.
 *   do_config_printer()       - Configure the default options for a printer.
 *   do_config_server()        - Configure server settings.
 *   do_delete_class()         - Delete a class...
 *   do_delete_printer()       - Delete a printer...
 *   do_menu()                 - Show the main menu...
 *   do_printer_op()           - Do a printer operation.
 *   do_set_allowed_users()    - Set the allowed/denied users for a queue.
 *   do_set_sharing()          - Set printer-is-shared value...
 *   match_string()            - Return the number of matching characters.
 */

/*
 * Include necessary headers...
 */

#include "ipp-var.h"
#include <cups/file.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>


/*
 * Local functions...
 */

static void	do_am_class(http_t *http, cups_lang_t *language, int modify);
static void	do_am_printer(http_t *http, cups_lang_t *language, int modify);
static void	do_config_printer(http_t *http, cups_lang_t *language);
static void	do_config_server(http_t *http, cups_lang_t *language);
static void	do_delete_class(http_t *http, cups_lang_t *language);
static void	do_delete_printer(http_t *http, cups_lang_t *language);
static void	do_menu(http_t *http, cups_lang_t *language);
static void	do_printer_op(http_t *http, cups_lang_t *language,
		              ipp_op_t op, const char *title);
static void	do_set_allowed_users(http_t *http, cups_lang_t *language);
static void	do_set_sharing(http_t *http, cups_lang_t *language);
static int	match_string(const char *a, const char *b);


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  cups_lang_t	*language;		/* Language information */
  http_t	*http;			/* Connection to the server */
  const char	*op;			/* Operation name */


 /*
  * Get the request language...
  */

  language = cupsLangDefault();

 /*
  * Connect to the HTTP server...
  */

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

 /*
  * Set the web interface section...
  */

  cgiSetVariable("SECTION", "admin");

 /*
  * See if we have form data...
  */

  if (!cgiInitialize())
  {
   /*
    * Nope, send the administration menu...
    */

    do_menu(http, language);
  }
  else if ((op = cgiGetVariable("OP")) != NULL)
  {
   /*
    * Do the operation...
    */

    if (!strcmp(op, "redirect"))
    {
      const char *url;			/* Redirection URL... */


      if ((url = cgiGetVariable("URL")) != NULL)
        printf("Location: %s\n\n", url);
      else
        puts("Location: /admin\n");
    }
    else if (!strcmp(op, "start-printer"))
      do_printer_op(http, language, IPP_RESUME_PRINTER, "Start Printer");
    else if (!strcmp(op, "stop-printer"))
      do_printer_op(http, language, IPP_PAUSE_PRINTER, "Stop Printer");
    else if (!strcmp(op, "start-class"))
      do_printer_op(http, language, IPP_RESUME_PRINTER, "Start Class");
    else if (!strcmp(op, "stop-class"))
      do_printer_op(http, language, IPP_PAUSE_PRINTER, "Stop Class");
    else if (!strcmp(op, "accept-jobs"))
      do_printer_op(http, language, CUPS_ACCEPT_JOBS, "Accept Jobs");
    else if (!strcmp(op, "reject-jobs"))
      do_printer_op(http, language, CUPS_REJECT_JOBS, "Reject Jobs");
    else if (!strcmp(op, "purge-jobs"))
      do_printer_op(http, language, IPP_PURGE_JOBS, "Purge Jobs");
    else if (!strcmp(op, "set-allowed-users"))
      do_set_allowed_users(http, language);
    else if (!strcmp(op, "set-as-default"))
      do_printer_op(http, language, CUPS_SET_DEFAULT, "Set As Default");
    else if (!strcmp(op, "set-sharing"))
      do_set_sharing(http, language);
    else if (!strcmp(op, "add-class"))
      do_am_class(http, language, 0);
    else if (!strcmp(op, "add-printer"))
      do_am_printer(http, language, 0);
    else if (!strcmp(op, "modify-class"))
      do_am_class(http, language, 1);
    else if (!strcmp(op, "modify-printer"))
      do_am_printer(http, language, 1);
    else if (!strcmp(op, "delete-class"))
      do_delete_class(http, language);
    else if (!strcmp(op, "delete-printer"))
      do_delete_printer(http, language);
    else if (!strcmp(op, "config-printer"))
      do_config_printer(http, language);
    else if (!strcmp(op, "config-server"))
      do_config_server(http, language);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiStartHTML("Error");
      cgiCopyTemplateLang(stdout, TEMPLATES, "admin-op.tmpl", getenv("LANG"));
      cgiEndHTML();
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

    cgiStartHTML("Error");
    cgiCopyTemplateLang(stdout, TEMPLATES, "admin-op.tmpl", getenv("LANG"));
    cgiEndHTML();
  }

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
 * 'compare_printer_devices()' - Compare two printer devices.
 */

static int				/* O - Result of comparison */
compare_printer_devices(const void *a,	/* I - First device */
                        const void *b)	/* I - Second device */
{
  return (strcmp(*((char **)a), *((char **)b)));
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
  const char	*title;			/* Title of page */


  title = modify ? "Modify Class" : "Add Class";

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

      httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                       "/classes/%s", cgiGetVariable("PRINTER_NAME"));
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                   NULL, uri);

     /*
      * Do the request and get back a response...
      */

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
      {
	ippSetCGIVars(response, NULL, NULL, NULL, 0);
	ippDelete(response);
      }

     /*
      * Update the location and description of an existing printer...
      */

      cgiStartHTML(title);
      cgiCopyTemplateLang(stdout, TEMPLATES, "modify-class.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiStartHTML(title);
      cgiCopyTemplateLang(stdout, TEMPLATES, "add-class.tmpl", getenv("LANG"));
    }

    cgiEndHTML();

    return;
  }

  name = cgiGetVariable("PRINTER_NAME");
  for (ptr = name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' || *ptr == '#')
      break;

  if (*ptr || ptr == name || strlen(name) > 127)
  {
    cgiSetVariable("ERROR", "The class name may only contain up to 127 printable "
                            "characters and may not contain spaces, slashes (/), "
			    "or the pound sign (#).");
    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
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

    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/classes/%s", cgiGetVariable("PRINTER_NAME"));
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

    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-members.tmpl", getenv("LANG"));
    cgiEndHTML();
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

    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/classes/%s", cgiGetVariable("PRINTER_NAME"));
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
      status = cupsLastError();

    if (status > IPP_OK_CONFLICT)
    {
      cgiStartHTML(title);
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Redirect successful updates back to the class page...
      */

      char	refresh[1024];		/* Refresh URL */

      cgiFormEncode(uri, name, sizeof(uri));
      snprintf(refresh, sizeof(refresh), "2;/admin?OP=redirect&URL=/classes/%s",
               uri);
      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(title);

      if (modify)
        cgiCopyTemplateLang(stdout, TEMPLATES, "class-modified.tmpl", getenv("LANG"));
      else
        cgiCopyTemplateLang(stdout, TEMPLATES, "class-added.tmpl", getenv("LANG"));
    }

    cgiEndHTML();
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
  const char	*name,			/* Pointer to class name */
		*ptr;			/* Pointer to CGI variable */
  const char	*title;			/* Title of page */
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


  title = modify ? "Modify Printer" : "Add Printer";

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

    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", cgiGetVariable("PRINTER_NAME"));
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
    cgiStartHTML(title);

    if (modify)
    {
     /*
      * Update the location and description of an existing printer...
      */

      if (oldinfo)
	ippSetCGIVars(oldinfo, NULL, NULL, NULL, 0);

      cgiCopyTemplateLang(stdout, TEMPLATES, "modify-printer.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiCopyTemplateLang(stdout, TEMPLATES, "add-printer.tmpl", getenv("LANG"));
    }

    cgiEndHTML();

    if (oldinfo)
      ippDelete(oldinfo);

    return;
  }

  for (ptr = name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' || *ptr == '#')
      break;

  if (*ptr || ptr == name || strlen(name) > 127)
  {
    cgiSetVariable("ERROR", "The printer name may only contain up to 127 printable "
                            "characters and may not contain spaces, slashes (/), "
			    "or the pound sign (#).");
    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
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
      ippSetCGIVars(response, NULL, NULL, NULL, 0);
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

    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-device.tmpl", getenv("LANG"));
    cgiEndHTML();
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

    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-uri.tmpl", getenv("LANG"));
    cgiEndHTML();
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

    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "choose-serial.tmpl", getenv("LANG"));
    cgiEndHTML();
  }
  else if ((var = cgiGetVariable("PPD_NAME")) == NULL)
  {
    if (modify)
    {
     /*
      * Get the PPD file...
      */

      int		fd;		/* PPD file */
      char		filename[1024];	/* PPD filename */
      ppd_file_t	*ppd;		/* PPD information */
      char		buffer[1024];	/* Buffer */
      int		bytes;		/* Number of bytes */


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
    else if ((uriptr = strrchr(cgiGetVariable("DEVICE_URI"), ';')) != NULL)
    {
     /*
      * Extract make and make/model from device URI string...
      */

      char	make[1024],		/* Make string */
		*makeptr;		/* Pointer into make */


      *uriptr++ = '\0';

      strlcpy(make, uriptr, sizeof(make));

      if ((makeptr = strchr(make, ' ')) != NULL)
        *makeptr = '\0';
      else if ((makeptr = strchr(make, '-')) != NULL)
        *makeptr = '\0';
      else if (!strncasecmp(make, "laserjet", 8) ||
               !strncasecmp(make, "deskjet", 7) ||
               !strncasecmp(make, "designjet", 9))
        strcpy(make, "HP");
      else if (!strncasecmp(make, "phaser", 6))
        strcpy(make, "Xerox");
      else if (!strncasecmp(make, "stylus", 6))
        strcpy(make, "Epson");
      else
        strcpy(make, "Generic");

      cgiSetVariable("CURRENT_MAKE", make);
      cgiSetVariable("PPD_MAKE", make);
      cgiSetVariable("CURRENT_MAKE_AND_MODEL", uriptr);
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


    if ((var = cgiGetVariable("PPD_MAKE")) != NULL)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                   "ppd-make", NULL, var);
    else
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                   "requested-attributes", NULL, "ppd-make");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
     /*
      * Got the list of PPDs, see if the user has selected a make...
      */

      ippSetCGIVars(response, NULL, NULL, NULL, 0);

      if (var == NULL)
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

        cgiStartHTML(title);
	cgiCopyTemplateLang(stdout, TEMPLATES, "choose-make.tmpl",
	                    getenv("LANG"));
        cgiEndHTML();
      }
      else
      {
       /*
	* Let the user choose a model...
	*/

        const char	*make_model;	/* Current make/model string */


        if ((make_model = cgiGetVariable("CURRENT_MAKE_AND_MODEL")) != NULL)
	{
	 /*
	  * Scan for "close" matches...
	  */

          int		match,		/* Current match */
			best_match,	/* Best match so far */
			count;		/* Number of drivers */
	  const char	*best,		/* Best matching string */
			*current;	/* Current string */


          count = cgiGetSize("PPD_MAKE_AND_MODEL");

	  for (i = 0, best_match = 0, best = NULL; i < count; i ++)
	  {
	    current = cgiGetArray("PPD_MAKE_AND_MODEL", i);
	    match   = match_string(make_model, current);

	    if (match > best_match)
	    {
	      best_match = match;
	      best       = current;
	    }
	  }

          if (best_match > strlen(var))
	  {
	   /*
	    * Found a match longer than the make...
	    */

            cgiSetVariable("CURRENT_MAKE_AND_MODEL", best);
	  }
	}

        cgiStartHTML(title);
	cgiCopyTemplateLang(stdout, TEMPLATES, "choose-model.tmpl",
	                    getenv("LANG"));
        cgiEndHTML();
      }

      
      ippDelete(response);
    }
    else
    {
      char message[1024];


      snprintf(message, sizeof(message), "Unable to get list of printer drivers: %s",
               ippErrorString(cupsLastError()));
      cgiSetVariable("ERROR", message);
      cgiStartHTML(title);
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
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

    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", cgiGetVariable("PRINTER_NAME"));
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
      status = cupsLastError();

    if (status > IPP_OK_CONFLICT)
    {
      cgiStartHTML(title);
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Redirect successful updates back to the printer or set-options pages...
      */

      char	refresh[1024];		/* Refresh URL */


      cgiFormEncode(uri, name, sizeof(uri));

      if (modify)
	snprintf(refresh, sizeof(refresh),
	         "2;/admin?OP=redirect&URL=/printers/%s", uri);
      else
	snprintf(refresh, sizeof(refresh),
	         "2;/admin?OP=config-printer&PRINTER_NAME=%s", uri);

      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(title);

      if (modify)
        cgiCopyTemplateLang(stdout, TEMPLATES, "printer-modified.tmpl", getenv("LANG"));
      else
        cgiCopyTemplateLang(stdout, TEMPLATES, "printer-added.tmpl", getenv("LANG"));
    }

    cgiEndHTML();
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
  int		i, j, k, m;		/* Looping vars */
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
  cups_file_t	*in,			/* Input file */
		*out;			/* Output file */
  char		line[1024];		/* Line from PPD file */
  char		keyword[1024],		/* Keyword from Default line */
		*keyptr;		/* Pointer into keyword... */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_group_t	*group;			/* Option group */
  ppd_option_t	*option;		/* Option */
  ppd_attr_t	*protocol;		/* cupsProtocol attribute */


 /*
  * Get the printer name...
  */

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiStartHTML("Set Printer Options");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

 /*
  * Get the PPD file...
  */

  if ((filename = cupsGetPPD(printer)) == NULL)
  {
    if (cupsLastError() == IPP_NOT_FOUND)
    {
     /*
      * No PPD file for this printer, so we can't configure it!
      */

      cgiSetVariable("ERROR", ippErrorString(IPP_NOT_POSSIBLE));
      cgiStartHTML("Set Printer Options");
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
    }
    else
    {
     /*
      * Unable to access the PPD file for some reason...
      */

      cgiSetVariable("ERROR", ippErrorString(cupsLastError()));
      cgiStartHTML("Set Printer Options");
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
    }
    return;
  }

  if ((ppd = ppdOpenFile(filename)) == NULL)
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_DEVICE_ERROR));
    cgiStartHTML("Set Printer Options");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

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

    cgiStartHTML("Set Printer Options");
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
      if (!strcmp(group->name, "InstallableOptions"))
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
        if (!strcmp(option->keyword, "PageRegion"))
	  continue;

        cgiSetVariable("KEYWORD", option->keyword);
        cgiSetVariable("KEYTEXT", option->text);
	    
	if (option->conflicted)
	  cgiSetVariable("CONFLICTED", "1");
	else
	  cgiSetVariable("CONFLICTED", "0");

	cgiSetSize("CHOICES", 0);
	cgiSetSize("TEXT", 0);
	for (k = 0, m = 0; k < option->num_choices; k ++)
	{
	 /*
	  * Hide custom option values...
	  */

	  if (!strcmp(option->choices[k].choice, "Custom"))
	    continue;

	  cgiSetArray("CHOICES", m, option->choices[k].choice);
	  cgiSetArray("TEXT", m, option->choices[k].text);

          m ++;

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

    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", printer);
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

      if (ippFindAttribute(response, "printer-error-policy-supported",
                           IPP_TAG_ZERO) ||
          ippFindAttribute(response, "printer-op-policy-supported",
	                   IPP_TAG_ZERO))
      {
       /*
	* Add the error and operation policy options...
	*/

	cgiSetVariable("GROUP", "Policies");
	cgiCopyTemplateLang(stdout, TEMPLATES, "option-header.tmpl",
                            getenv("LANG"));

       /*
        * Error policy...
	*/

        attr = ippFindAttribute(response, "printer-error-policy-supported",
	                        IPP_TAG_ZERO);

        if (attr)
	{
	  cgiSetSize("CHOICES", attr->num_values);
	  cgiSetSize("TEXT", attr->num_values);
	  for (k = 0; k < attr->num_values; k ++)
	  {
	    cgiSetArray("CHOICES", k, attr->values[k].string.text);
	    cgiSetArray("TEXT", k, attr->values[k].string.text);
	  }

          attr = ippFindAttribute(response, "printer-error-policy",
	                          IPP_TAG_ZERO);

          cgiSetVariable("KEYWORD", "printer_error_policy");
          cgiSetVariable("KEYTEXT", "Error Policy");
          cgiSetVariable("DEFCHOICE", attr == NULL ?
	                              "" : attr->values[0].string.text);
        }

	cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickone.tmpl",
	                    getenv("LANG"));

       /*
        * Operation policy...
	*/

        attr = ippFindAttribute(response, "printer-op-policy-supported",
	                        IPP_TAG_ZERO);

        if (attr)
	{
	  cgiSetSize("CHOICES", attr->num_values);
	  cgiSetSize("TEXT", attr->num_values);
	  for (k = 0; k < attr->num_values; k ++)
	  {
	    cgiSetArray("CHOICES", k, attr->values[k].string.text);
	    cgiSetArray("TEXT", k, attr->values[k].string.text);
	  }

          attr = ippFindAttribute(response, "printer-op-policy", IPP_TAG_ZERO);

          cgiSetVariable("KEYWORD", "printer_op_policy");
          cgiSetVariable("KEYTEXT", "Operation Policy");
          cgiSetVariable("DEFCHOICE", attr == NULL ?
	                              "" : attr->values[0].string.text);

	  cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickone.tmpl",
	                      getenv("LANG"));
        }

	cgiCopyTemplateLang(stdout, TEMPLATES, "option-trailer.tmpl",
                            getenv("LANG"));
      }

      ippDelete(response);
    }

   /*
    * Binary protocol support...
    */

    if (ppd->protocols && strstr(ppd->protocols, "BCP"))
    {
      protocol = ppdFindAttr(ppd, "cupsProtocol", NULL);

      cgiSetVariable("GROUP", "PS Binary Protocol");
      cgiCopyTemplateLang(stdout, TEMPLATES, "option-header.tmpl",
                          getenv("LANG"));

      cgiSetSize("CHOICES", 2);
      cgiSetSize("TEXT", 2);
      cgiSetArray("CHOICES", 0, "None");
      cgiSetArray("TEXT", 0, "None");

      if (strstr(ppd->protocols, "TBCP"))
      {
	cgiSetArray("CHOICES", 1, "TBCP");
	cgiSetArray("TEXT", 1, "TBCP");
      }
      else
      {
	cgiSetArray("CHOICES", 1, "BCP");
	cgiSetArray("TEXT", 1, "BCP");
      }

      cgiSetVariable("KEYWORD", "protocol");
      cgiSetVariable("KEYTEXT", "PS Binary Protocol");
      cgiSetVariable("DEFCHOICE", protocol ? protocol->value : "None");

      cgiCopyTemplateLang(stdout, TEMPLATES, "option-pickone.tmpl",
	                  getenv("LANG"));

      cgiCopyTemplateLang(stdout, TEMPLATES, "option-trailer.tmpl",
                          getenv("LANG"));
    }

    cgiCopyTemplateLang(stdout, TEMPLATES, "config-printer2.tmpl",
                        getenv("LANG"));
    cgiEndHTML();
  }
  else
  {
   /*
    * Set default options...
    */

    out = cupsTempFile2(tempfile, sizeof(tempfile));
    in  = cupsFileOpen(filename, "r");

    if (!in || !out)
    {
      cgiSetVariable("ERROR", strerror(errno));
      cgiStartHTML("Set Printer Options");
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();

      if (in)
        cupsFileClose(in);

      if (out)
      {
        cupsFileClose(out);
	unlink(tempfile);
      }

      unlink(filename);
      return;
    }

    while (cupsFileGets(in, line, sizeof(line)))
    {
      if (!strncmp(line, "*cupsProtocol:", 14) && cgiGetVariable("protocol"))
        continue;
      else if (strncmp(line, "*Default", 8))
        cupsFilePrintf(out, "%s\n", line);
      else
      {
       /*
        * Get default option name...
	*/

        strlcpy(keyword, line + 8, sizeof(keyword));

	for (keyptr = keyword; *keyptr; keyptr ++)
	  if (*keyptr == ':' || isspace(*keyptr & 255))
	    break;

        *keyptr = '\0';

        if (!strcmp(keyword, "PageRegion"))
	  var = cgiGetVariable("PageSize");
	else
	  var = cgiGetVariable(keyword);

        if (var != NULL)
	  cupsFilePrintf(out, "*Default%s: %s\n", keyword, var);
	else
	  cupsFilePrintf(out, "%s\n", line);
      }
    }

    if ((var = cgiGetVariable("protocol")) != NULL)
      cupsFilePrintf(out, "*cupsProtocol: %s\n", cgiGetVariable("protocol"));

    cupsFileClose(in);
    cupsFileClose(out);

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

    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
                         "job-sheets-default", 2, NULL, NULL);
    attr->values[0].string.text = strdup(cgiGetVariable("job_sheets_start"));
    attr->values[1].string.text = strdup(cgiGetVariable("job_sheets_end"));

    if ((var = cgiGetVariable("printer_error_policy")) != NULL)
      attr = ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
                          "printer-error-policy", NULL, var);

    if ((var = cgiGetVariable("printer_op_policy")) != NULL)
      attr = ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
                          "printer-op-policy", NULL, var);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoFileRequest(http, request, "/admin/", tempfile)) != NULL)
    {
      status = response->request.status.status_code;
      ippDelete(response);
    }
    else
      status = cupsLastError();

    if (status > IPP_OK_CONFLICT)
    {
      cgiStartHTML("Set Printer Options");
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Redirect successful updates back to the printer page...
      */

      char	refresh[1024];		/* Refresh URL */

      cgiFormEncode(uri, printer, sizeof(uri));
      snprintf(refresh, sizeof(refresh), "2;/admin?OP=redirect&URL=/printers/%s",
               uri);
      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML("Set Printer Options");

      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-configured.tmpl", getenv("LANG"));
    }

    cgiEndHTML();

    unlink(tempfile);
  }

  unlink(filename);
}


/*
 * 'do_config_server()' - Configure server settings.
 */

static void
do_config_server(http_t      *http,	/* I - HTTP connection */
                 cups_lang_t *language)	/* I - Client's language */
{
  if (cgiIsPOST() && !cgiGetVariable("CUPSDCONF"))
  {
   /*
    * Save basic setting changes...
    */

    http_status_t status;		/* PUT status */
    cups_file_t	*cupsd;			/* cupsd.conf file */
    char	tempfile[1024];		/* Temporary new cupsd.conf */
    int		tempfd;			/* Temporary file descriptor */
    cups_file_t	*temp;			/* Temporary file */
    char	line[1024],		/* Line from cupsd.conf file */
		*value;			/* Value on line */
    const char	*server_root;		/* Location of config files */
    int		linenum,		/* Line number in file */
		in_policy,		/* In a policy section? */
		in_cancel_job,		/* In a cancel-job section? */
		in_admin_location,	/* In the /admin location? */
		in_conf_location,	/* In the /admin/conf location? */
		in_root_location;	/* In the / location? */
    int		remote_printers,	/* Show remote printers */
		share_printers,		/* Share local printers */
		remote_admin,		/* Remote administration allowed? */
		user_cancel_any,	/* Cancel-job policy set? */
		debug_logging;		/* LogLevel debug set? */
    int		wrote_port_listen,	/* Wrote the port/listen lines? */
		wrote_browsing,		/* Wrote the browsing lines? */
		wrote_policy,		/* Wrote the policy? */
		wrote_loglevel,		/* Wrote the LogLevel line? */
		wrote_admin_location,	/* Wrote the /admin location? */
		wrote_conf_location,	/* Wrote the /admin/conf location? */
		wrote_root_location;	/* Wrote the / location? */
    int		indent;			/* Indentation */


   /*
    * Get form variables...
    */

    remote_printers = cgiGetVariable("REMOTE_PRINTERS") != NULL;
    share_printers  = cgiGetVariable("SHARE_PRINTERS") != NULL;
    remote_admin    = cgiGetVariable("REMOTE_ADMIN") != NULL;
    user_cancel_any = cgiGetVariable("USER_CANCEL_ANY") != NULL;
    debug_logging   = cgiGetVariable("DEBUG_LOGGING") != NULL;

   /*
    * Locate the cupsd.conf file...
    */

    if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
      server_root = CUPS_SERVERROOT;

    snprintf(line, sizeof(line), "%s/cupsd.conf", server_root);

   /*
    * Open the cupsd.conf file...
    */

    if ((cupsd = cupsFileOpen(line, "r")) == NULL)
    {
     /*
      * Unable to open - log an error...
      */

      cgiStartHTML("Change Settings");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
      
      perror(line);
      return;
    }

   /*
    * Create a temporary file for the new cupsd.conf file...
    */

    if ((tempfd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      cgiStartHTML("Change Settings");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
      
      perror(tempfile);
      cupsFileClose(cupsd);
      return;
    }

    if ((temp = cupsFileOpenFd(tempfd, "w")) == NULL)
    {
      cgiStartHTML("Change Settings");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
      
      perror(tempfile);
      close(tempfd);
      unlink(tempfile);
      cupsFileClose(cupsd);
      return;
    }

   /*
    * Copy the old file to the new, making changes along the way...
    */

    in_admin_location    = 0;
    in_cancel_job        = 0;
    in_conf_location     = 0;
    in_policy            = 0;
    in_root_location     = 0;
    linenum              = 0;
    wrote_admin_location = 0;
    wrote_browsing       = 0;
    wrote_conf_location  = 0;
    wrote_loglevel       = 0;
    wrote_policy         = 0;
    wrote_port_listen    = 0;
    wrote_root_location  = 0;
    indent               = 0;

    while (cupsFileGetConf(cupsd, line, sizeof(line), &value, &linenum))
    {
      if (!strcasecmp(line, "Port") || !strcasecmp(line, "Listen"))
      {
	if (!wrote_port_listen)
	{
          wrote_port_listen = 1;

	  if (share_printers || remote_admin)
	  {
	    cupsFilePuts(temp, "# Allow remote access\n");
	    cupsFilePrintf(temp, "Listen *:%d\n", ippPort());
	  }
	  else
	  {
	    cupsFilePuts(temp, "# Only listen for connections from the local machine.\n");
	    cupsFilePrintf(temp, "Listen localhost:%d\n", ippPort());
	  }

#ifdef CUPS_DEFAULT_DOMAINSOCKET
          cupsFilePuts(temp, "Listen " CUPS_DEFAULT_DOMAINSOCKET "\n");
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
	}
      }
      else if (!strcasecmp(line, "Browsing") ||
               !strcasecmp(line, "BrowseAddress") ||
               !strcasecmp(line, "BrowseAllow") ||
               !strcasecmp(line, "BrowseDeny") ||
               !strcasecmp(line, "BrowseOrder"))
      {
	if (!wrote_browsing)
	{
          wrote_browsing = 1;

          if (remote_printers || share_printers)
	  {
	    if (remote_printers && share_printers)
	      cupsFilePuts(temp, "# Enable printer sharing and shared printers.\n");
	    else if (remote_printers)
	      cupsFilePuts(temp, "# Show shared printers on the local network.\n");
	    else
	      cupsFilePuts(temp, "# Share local printers on the local network.\n");

	    cupsFilePuts(temp, "Browsing On\n");
	    cupsFilePuts(temp, "BrowseOrder allow,deny\n");

	    if (remote_printers)
	      cupsFilePuts(temp, "BrowseAllow @LOCAL\n");

	    if (share_printers)
	      cupsFilePuts(temp, "BrowseAddress @LOCAL\n");
          }
	  else
	  {
	    cupsFilePuts(temp, "# Disable printer sharing and shared printers.\n");
	    cupsFilePuts(temp, "Browsing Off\n");
	  }
	}
      }
      else if (!strcasecmp(line, "LogLevel"))
      {
        wrote_loglevel = 1;

	if (debug_logging)
	{
          cupsFilePuts(temp, "# Show troubleshooting information in error_log.\n");
	  cupsFilePuts(temp, "LogLevel debug\n");
	}
	else
	{
          cupsFilePuts(temp, "# Show general information in error_log.\n");
	  cupsFilePuts(temp, "LogLevel info\n");
	}
      }
      else if (!strcasecmp(line, "<Policy") && !strcasecmp(value, "default"))
      {
	in_policy = 1;

	cupsFilePrintf(temp, "%s %s>\n", line, value);
	indent += 2;
      }
      else if (!strcasecmp(line, "</Policy>"))
      {
	indent -= 2;
        if (!wrote_policy)
	{
	  wrote_policy = 1;

          if (!user_cancel_any)
	    cupsFilePuts(temp, "  # Only the owner or an administrator can cancel a job...\n"
	                       "  <Limit Cancel-Job>\n"
	                       "    Order deny,allow\n"
	                       "    Allow @SYSTEM\n"
			       "    Allow @OWNER\n"
			       "  </Limit>\n");
        }

	in_policy = 0;

	cupsFilePuts(temp, "</Policy>\n");
      }
      else if (!strcasecmp(line, "<Location"))
      {
	indent += 2;
        if (!strcmp(value, "/admin"))
	  in_admin_location = 1;
        if (!strcmp(value, "/admin/conf"))
	  in_conf_location = 1;
	else if (!strcmp(value, "/"))
	  in_root_location = 1;

	cupsFilePrintf(temp, "%s %s>\n", line, value);
      }
      else if (!strcasecmp(line, "</Location>"))
      {
	indent -= 2;
        if (in_admin_location)
	{
	  wrote_admin_location = 1;

	  if (remote_admin)
            cupsFilePuts(temp, "  # Allow remote administration...\n");
	  else
            cupsFilePuts(temp, "  # Restrict access to the admin pages...\n");

          cupsFilePuts(temp, "  Order allow,deny\n");

	  if (remote_admin)
	    cupsFilePuts(temp, "  Allow @LOCAL\n");
	  else
	    cupsFilePuts(temp, "  Allow localhost\n");
	}
        else if (in_conf_location)
	{
	  wrote_conf_location = 1;

	  if (remote_admin)
            cupsFilePuts(temp, "  # Allow remote access to the configuration files...\n");
	  else
            cupsFilePuts(temp, "  # Restrict access to the configuration files...\n");

          cupsFilePuts(temp, "  Order allow,deny\n");

	  if (remote_admin)
	    cupsFilePuts(temp, "  Allow @LOCAL\n");
	  else
	    cupsFilePuts(temp, "  Allow localhost\n");
	}
	else if (in_root_location)
	{
	  wrote_root_location = 1;

	  if (remote_admin && share_printers)
            cupsFilePuts(temp, "  # Allow shared printing and remote administration...\n");
	  else if (remote_admin)
            cupsFilePuts(temp, "  # Allow remote administration...\n");
	  else if (share_printers)
            cupsFilePuts(temp, "  # Allow shared printing...\n");
	  else
            cupsFilePuts(temp, "  # Restrict access to the server...\n");

          cupsFilePuts(temp, "  Order allow,deny\n");

	  if (remote_admin || share_printers)
	    cupsFilePuts(temp, "  Allow @LOCAL\n");
	  else
	    cupsFilePuts(temp, "  Allow localhost\n");
	}

	in_admin_location = 0;
	in_conf_location  = 0;
        in_root_location  = 0;

	cupsFilePuts(temp, "</Location>\n");
      }
      else if (!strcasecmp(line, "<Limit") && in_policy)
      {
       /*
	* See if the policy limit is for the Cancel-Job operation...
	*/

	char	*valptr;		/* Pointer into value */


	indent += 2;

        if (!strcasecmp(value, "cancel-job"))
	{
	 /*
	  * Don't write anything for this limit section...
	  */

	  in_cancel_job = 2;
	}
	else
	{
	  cupsFilePrintf(temp, "  %s", line);

	  while (*value)
	  {
	    for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

	    if (*valptr)
	      *valptr++ = '\0';

            if (!strcasecmp(value, "cancel-job"))
	    {
	     /*
	      * Write everything except for this definition...
	      */

	      in_cancel_job = 1;
	    }
	    else
	      cupsFilePrintf(temp, " %s", value);

            for (value = valptr; isspace(*value & 255); value ++);
	  }

	  cupsFilePuts(temp, ">\n");
        }
      }
      else if (!strcasecmp(line, "</Limit>") && in_cancel_job)
      {
	indent -= 2;

        if (in_cancel_job == 1)
          cupsFilePuts(temp, "  </Limit>\n");

        wrote_policy = 1;

        if (!user_cancel_any)
	  cupsFilePuts(temp, "  # Only the owner or an administrator can cancel a job...\n"
	                     "  <Limit Cancel-Job>\n"
	                     "    Order deny,allow\n"
	                     "    Require user @OWNER @SYSTEM\n"
			     "  </Limit>\n");

	in_cancel_job = 0;
      }
      else if ((in_admin_location || in_conf_location || in_root_location) &&
               (!strcasecmp(line, "Allow") || !strcasecmp(line, "Deny") ||
	        !strcasecmp(line, "Order")))
	continue;
      else if (in_cancel_job == 2)
        continue;
      else if (!strcasecmp(line, "<Limit")  && value)
	cupsFilePrintf(temp, "  %s %s>\n", line, value);
      else if (line[0] == '<')
      {
        if (value)
	{
          cupsFilePrintf(temp, "%*s%s %s>\n", indent, "", line, value);
	  indent += 2;
	}
	else
	{
	  if (line[1] == '/')
	    indent -= 2;

	  cupsFilePrintf(temp, "%*s%s\n", indent, "", line);
	}
      }
      else if (value)
	cupsFilePrintf(temp, "%*s%s %s\n", indent, "", line, value);
      else
	cupsFilePrintf(temp, "%*s%s\n", indent, "", line);
    }

   /*
    * Write any missing info...
    */

    if (!wrote_browsing)
    {
      if (remote_printers || share_printers)
      {
	if (remote_printers && share_printers)
	  cupsFilePuts(temp, "# Enable printer sharing and shared printers.\n");
	else if (remote_printers)
	  cupsFilePuts(temp, "# Show shared printers on the local network.\n");
	else
	  cupsFilePuts(temp, "# Share local printers on the local network.\n");

	cupsFilePuts(temp, "Browsing On\n");
	cupsFilePuts(temp, "BrowseOrder allow,deny\n");

	if (remote_printers)
	  cupsFilePuts(temp, "BrowseAllow @LOCAL\n");

	if (share_printers)
	  cupsFilePuts(temp, "BrowseAddress @LOCAL\n");
      }
      else
      {
	cupsFilePuts(temp, "# Disable printer sharing and shared printers.\n");
	cupsFilePuts(temp, "Browsing Off\n");
      }
    }

    if (!wrote_loglevel)
    {
      if (debug_logging)
      {
        cupsFilePuts(temp, "# Show troubleshooting information in error_log.\n");
	cupsFilePuts(temp, "LogLevel debug\n");
      }
      else
      {
        cupsFilePuts(temp, "# Show general information in error_log.\n");
	cupsFilePuts(temp, "LogLevel info\n");
      }
    }

    if (!wrote_port_listen)
    {
      if (share_printers || remote_admin)
      {
	cupsFilePuts(temp, "# Allow remote access\n");
	cupsFilePrintf(temp, "Listen *:%d\n", ippPort());
      }
      else
      {
	cupsFilePuts(temp, "# Only listen for connections from the local machine.\n");
	cupsFilePrintf(temp, "Listen localhost:%d\n", ippPort());
      }

#ifdef CUPS_DEFAULT_DOMAINSOCKET
      cupsFilePuts(temp, "Listen " CUPS_DEFAULT_DOMAINSOCKET "\n");
#endif /* CUPS_DEFAULT_DOMAINSOCKET */
    }

    if (!wrote_root_location)
    {
      if (remote_admin && share_printers)
        cupsFilePuts(temp, "# Allow shared printing and remote administration...\n");
      else if (remote_admin)
        cupsFilePuts(temp, "# Allow remote administration...\n");
      else if (share_printers)
        cupsFilePuts(temp, "# Allow shared printing...\n");
      else
        cupsFilePuts(temp, "# Restrict access to the server...\n");

      cupsFilePuts(temp, "<Location />\n"
                         "  Order allow,deny\n");

      if (remote_admin || share_printers)
	cupsFilePuts(temp, "  Allow @LOCAL\n");
      else
	cupsFilePuts(temp, "  Allow localhost\n");

      cupsFilePuts(temp, "</Location>\n");
    }

    if (!wrote_admin_location)
    {
      if (remote_admin)
        cupsFilePuts(temp, "# Allow remote administration...\n");
      else
        cupsFilePuts(temp, "# Restrict access to the admin pages...\n");

      cupsFilePuts(temp, "<Location /admin>\n"
                         "  Order allow,deny\n");

      if (remote_admin)
	cupsFilePuts(temp, "  Allow @LOCAL\n");
      else
	cupsFilePuts(temp, "  Allow localhost\n");

      cupsFilePuts(temp, "</Location>\n");
    }

    if (!wrote_conf_location)
    {
      if (remote_admin)
        cupsFilePuts(temp, "# Allow remote access to the configuration files...\n");
      else
        cupsFilePuts(temp, "# Restrict access to the configuration files...\n");

      cupsFilePuts(temp, "<Location /admin/conf>\n"
                         "  AuthType Basic\n"
                         "  Require user @SYSTEM\n"
                         "  Order allow,deny\n");

      if (remote_admin)
	cupsFilePuts(temp, "  Allow @LOCAL\n");
      else
	cupsFilePuts(temp, "  Allow localhost\n");

      cupsFilePuts(temp, "</Location>\n");
    }

    if (!wrote_policy)
    {
      cupsFilePuts(temp, "<Policy default>\n"
                         "  # Job-related operations must be done by the owner or an adminstrator...\n"
                         "  <Limit Send-Document Send-URI Hold-Job Release-Job "
			 "Restart-Job Purge-Jobs Set-Job-Attributes "
			 "Create-Job-Subscription Renew-Subscription "
			 "Cancel-Subscription Get-Notifications Reprocess-Job "
			 "Cancel-Current-Job Suspend-Current-Job Resume-Job "
			 "CUPS-Move-Job>\n"
                         "    Require user @OWNER @SYSTEM\n"
                         "    Order deny,allow\n"
                         "  </Limit>\n"
                         "  # All administration operations require an adminstrator to authenticate...\n"
			 "  <Limit Pause-Printer Resume-Printer "
                         "Set-Printer-Attributes Enable-Printer "
			 "Disable-Printer Pause-Printer-After-Current-Job "
			 "Hold-New-Jobs Release-Held-New-Jobs Deactivate-Printer "
			 "Activate-Printer Restart-Printer Shutdown-Printer "
			 "Startup-Printer Promote-Job Schedule-Job-After "
			 "CUPS-Add-Printer CUPS-Delete-Printer "
			 "CUPS-Add-Class CUPS-Delete-Class "
			 "CUPS-Accept-Jobs CUPS-Reject-Jobs "
			 "CUPS-Set-Default CUPS-Add-Device CUPS-Delete-Device>\n"
                         "    AuthType Basic\n"
			 "    Require user @SYSTEM\n"
                         "    Order deny,allow\n"
                         "</Limit>\n");

      if (!user_cancel_any)
	cupsFilePuts(temp, "  # Only the owner or an administrator can cancel a job...\n"
	                   "  <Limit Cancel-Job>\n"
	                   "    Require user @OWNER @SYSTEM\n"
	                   "    Order deny,allow\n"
			   "  </Limit>\n");

      cupsFilePuts(temp, "  <Limit All>\n"
                         "  Order deny,allow\n"
                         "  </Limit>\n"
			 "</Policy>\n");
    }

    cupsFileClose(cupsd);
    cupsFileClose(temp);

   /*
    * Upload the configuration file to the server...
    */

    status = cupsPutFile(http, "/admin/conf/cupsd.conf", tempfile);

    if (status != HTTP_CREATED)
    {
      cgiSetVariable("ERROR", cupsLangString(language, status));
      cgiStartHTML("Change Settings");
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
    {
      cgiSetVariable("refresh_page", "10;/admin?OP=redirect");

      cgiStartHTML("Change Settings");
      cgiCopyTemplateLang(stdout, TEMPLATES, "restart.tmpl", getenv("LANG"));
    }

    cgiEndHTML();

    unlink(tempfile);
  }
  else if (cgiIsPOST())
  {
   /*
    * Save hand-edited config file...
    */

    http_status_t status;		/* PUT status */
    char	tempfile[1024];		/* Temporary new cupsd.conf */
    int		tempfd;			/* Temporary file descriptor */
    cups_file_t	*temp;			/* Temporary file */
    const char	*start,			/* Start of line */
		*end;			/* End of line */


   /*
    * Create a temporary file for the new cupsd.conf file...
    */

    if ((tempfd = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      cgiStartHTML("Edit Configuration File");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
      
      perror(tempfile);
      return;
    }

    if ((temp = cupsFileOpenFd(tempfd, "w")) == NULL)
    {
      cgiStartHTML("Edit Configuration File");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();
      
      perror(tempfile);
      close(tempfd);
      unlink(tempfile);
      return;
    }

   /*
    * Copy the cupsd.conf text from the form variable...
    */

    start = cgiGetVariable("CUPSDCONF");
    while (start)
    {
      if ((end = strstr(start, "\r\n")) == NULL)
        if ((end = strstr(start, "\n")) == NULL)
	  end = start + strlen(start);

      cupsFileWrite(temp, start, end - start);
      cupsFilePutChar(temp, '\n');

      if (*end == '\r')
        start = end + 2;
      else if (*end == '\n')
        start = end + 1;
      else
        start = NULL;
    }

    cupsFileClose(temp);

   /*
    * Upload the configuration file to the server...
    */

    status = cupsPutFile(http, "/admin/conf/cupsd.conf", tempfile);

    if (status != HTTP_CREATED)
    {
      cgiSetVariable("ERROR", cupsLangString(language, status));
      cgiStartHTML("Edit Configuration File");
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
    {
      cgiSetVariable("refresh_page", "10;/admin?OP=redirect");

      cgiStartHTML("Edit Configuration File");
      cgiCopyTemplateLang(stdout, TEMPLATES, "restart.tmpl", getenv("LANG"));
    }

    cgiEndHTML();

    unlink(tempfile);
  }
  else
  {
    struct stat	info;			/* cupsd.conf information */
    cups_file_t	*cupsd;			/* cupsd.conf file */
    char	*buffer;		/* Buffer for entire file */
    char	filename[1024];		/* Filename */
    const char	*server_root;		/* Location of config files */


   /*
    * Locate the cupsd.conf file...
    */

    if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
      server_root = CUPS_SERVERROOT;

    snprintf(filename, sizeof(filename), "%s/cupsd.conf", server_root);

   /*
    * Figure out the size...
    */

    if (stat(filename, &info))
    {
      cgiStartHTML("Edit Configuration File");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();

      perror(filename);
      return;
    }

    if (info.st_size > (1024 * 1024))
    {
      cgiStartHTML("Edit Configuration File");
      cgiSetVariable("ERROR", "Unable to edit cupsd.conf files larger than 1MB!");
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();

      fprintf(stderr, "ERROR: \"%s\" too large (%ld) to edit!\n", filename,
              (long)info.st_size);
      return;
    }

   /*
    * Open the cupsd.conf file...
    */

    if ((cupsd = cupsFileOpen(filename, "r")) == NULL)
    {
     /*
      * Unable to open - log an error...
      */

      cgiStartHTML("Edit Configuration File");
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
      cgiEndHTML();

      perror(filename);
      return;
    }

   /*
    * Allocate memory and load the file into a string buffer...
    */

    buffer = calloc(1, info.st_size + 1);

    cupsFileRead(cupsd, buffer, info.st_size);
    cupsFileClose(cupsd);

    cgiSetVariable("CUPSDCONF", buffer);
    free(buffer);

   /*
    * Show the current config file...
    */

    cgiStartHTML("Edit Configuration File");

    printf("<!-- \"%s\" -->\n", filename);

    cgiCopyTemplateLang(stdout, TEMPLATES, "edit-config.tmpl", getenv("LANG"));

    cgiEndHTML();
  }
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
    cgiStartHTML("Delete Class");
    cgiCopyTemplateLang(stdout, TEMPLATES, "class-confirm.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

  if ((pclass = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/classes/%s", pclass);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiStartHTML("Delete Class");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
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
    status = cupsLastError();

  cgiStartHTML("Delete Class");

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "class-deleted.tmpl", getenv("LANG"));

  cgiEndHTML();
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
    cgiStartHTML("Delete Printer");
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-confirm.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiStartHTML("Delete Printer");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
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
    status = cupsLastError();

  cgiStartHTML("Delete Printer");

  if (status > IPP_OK_CONFLICT)
  {
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-deleted.tmpl", getenv("LANG"));

  cgiEndHTML();
}


/*
 * 'do_menu()' - Show the main menu...
 */

static void
do_menu(http_t      *http,		/* I - HTTP connection */
        cups_lang_t *language)		/* I - Client's language */
{
  cups_file_t	*cupsd;			/* cupsd.conf file */
  char		line[1024],		/* Line from cupsd.conf file */
		*value;			/* Value on line */
  const char	*server_root;		/* Location of config files */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */


 /*
  * Locate the cupsd.conf file...
  */

  if ((server_root = getenv("CUPS_SERVERROOT")) == NULL)
    server_root = CUPS_SERVERROOT;

  snprintf(line, sizeof(line), "%s/cupsd.conf", server_root);

  cgiStartHTML("Administration");

  printf("<!-- \"%s\" -->\n", line);

 /*
  * Open the cupsd.conf file...
  */

  if ((cupsd = cupsFileOpen(line, "r")) == NULL)
  {
   /*
    * Unable to open - log an error...
    */

    cgiStartHTML("Administration");
    cgiSetVariable("ERROR", strerror(errno));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();

    perror(line);
  }
  else
  {
   /*
    * Read the file, keeping track of what settings are enabled...
    */

    int		remote_access = 0,	/* Remote access allowed? */
		remote_admin = 0,	/* Remote administration allowed? */
		browsing = 1,		/* Browsing enabled? */
		browse_allow = 1,	/* Browse address set? */
		browse_address = 0,	/* Browse address set? */
		cancel_policy = 1,	/* Cancel-job policy set? */
		debug_logging = 0;	/* LogLevel debug set? */
    int		linenum = 0,		/* Line number in file */
		in_policy = 0,		/* In a policy section? */
		in_cancel_job = 0,	/* In a cancel-job section? */
		in_admin_location = 0;	/* In the /admin location? */


    while (cupsFileGetConf(cupsd, line, sizeof(line), &value, &linenum))
    {
      if (!strcasecmp(line, "Port"))
      {
        remote_access = 1;
      }
      else if (!strcasecmp(line, "Listen"))
      {
        char	*port;			/* Pointer to port number, if any */


	if ((port = strrchr(value, ':')) != NULL)
	  *port = '\0';

        if (strcasecmp(value, "localhost") && strcmp(value, "127.0.0.1"))
	  remote_access = 1;
      }
      else if (!strcasecmp(line, "Browsing"))
      {
        browsing = !strcasecmp(value, "yes") || !strcasecmp(value, "on") ||
	           !strcasecmp(value, "true");
      }
      else if (!strcasecmp(line, "BrowseAddress"))
      {
        browse_address = 1;
      }
      else if (!strcasecmp(line, "BrowseAllow"))
      {
        browse_allow = 1;
      }
      else if (!strcasecmp(line, "BrowseOrder"))
      {
        browse_allow = !strncasecmp(value, "deny,", 5);
      }
      else if (!strcasecmp(line, "LogLevel"))
      {
        debug_logging = !strncasecmp(value, "debug", 5);
      }
      else if (!strcasecmp(line, "<Policy") && !strcasecmp(value, "default"))
      {
        in_policy = 1;
      }
      else if (!strcasecmp(line, "</Policy>"))
      {
        in_policy = 0;
      }
      else if (!strcasecmp(line, "<Limit") && in_policy)
      {
       /*
        * See if the policy limit is for the Cancel-Job operation...
	*/

        char	*valptr;		/* Pointer into value */


        while (*value)
	{
	  for (valptr = value; !isspace(*valptr & 255) && *valptr; valptr ++);

	  if (*valptr)
	    *valptr++ = '\0';

          if (!strcasecmp(value, "cancel-job") || !strcasecmp(value, "all"))
	  {
	    in_cancel_job = 1;
	    break;
	  }

          for (value = valptr; isspace(*value & 255); value ++);
        }
      }
      else if (!strcasecmp(line, "</Limit>"))
      {
        in_cancel_job = 0;
      }
      else if (!strcasecmp(line, "Require") && in_cancel_job)
      {
        cancel_policy = 0;
      }
      else if (!strcasecmp(line, "<Location") && !strcasecmp(value, "/admin"))
      {
        in_admin_location = 1;
      }
      else if (!strcasecmp(line, "</Location>"))
      {
        in_admin_location = 0;
      }
      else if (!strcasecmp(line, "Allow") && in_admin_location &&
               strcasecmp(value, "localhost") && strcasecmp(value, "127.0.0.1"))
      {
        remote_admin = 1;
      }
    }

    cupsFileClose(cupsd);

    if (browsing && browse_allow)
      cgiSetVariable("REMOTE_PRINTERS", "CHECKED");

    if (remote_access && browsing && browse_address)
      cgiSetVariable("SHARE_PRINTERS", "CHECKED");

    if (remote_access && remote_admin)
      cgiSetVariable("REMOTE_ADMIN", "CHECKED");

    if (cancel_policy)
      cgiSetVariable("USER_CANCEL_ANY", "CHECKED");

    if (debug_logging)
      cgiSetVariable("DEBUG_LOGGING", "CHECKED");
  }

 /*
  * Get the list of printers and their devices...
  */

  request = ippNew();
  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "device-uri");

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type",
                CUPS_PRINTER_LOCAL);
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM, "printer-type-mask",
                CUPS_PRINTER_LOCAL);

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
   /*
    * Got the printer list, now load the devices...
    */

    int		i;			/* Looping var */
    int		num_printer_devices;	/* Number of devices for local printers */
    char	**printer_devices;	/* Printer devices for local printers */


   /*
    * Count the number of printers we have...
    */

    for (num_printer_devices = 0,
             attr = ippFindAttribute(response, "device-uri", IPP_TAG_URI);
         attr;
	 num_printer_devices ++,
	     attr = ippFindNextAttribute(response, "device-uri", IPP_TAG_URI));

    if (num_printer_devices > 0)
    {
     /*
      * Allocate an array and copy the device strings...
      */

      printer_devices = calloc(num_printer_devices, sizeof(char *));

      for (i = 0, attr = ippFindAttribute(response, "device-uri", IPP_TAG_URI);
           attr;
	   i ++,  attr = ippFindNextAttribute(response, "device-uri", IPP_TAG_URI))
      {
	printer_devices[i] = strdup(attr->values[0].string.text);
      }

     /*
      * Sort the printer devices as needed...
      */

      if (num_printer_devices > 1)
        qsort(printer_devices, num_printer_devices, sizeof(char *),
	      compare_printer_devices);
    }
    else
      printer_devices = NULL;

   /*
    * Free the printer list and get the device list...
    */

    ippDelete(response);

    request = ippNew();
    request->request.op.operation_id = CUPS_GET_DEVICES;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
     /*
      * Got the device list, let's parse it...
      */

      const char *device_uri,		/* device-uri attribute value */
		*device_make_and_model,	/* device-make-and-model value */
		*device_info;		/* device-info value */


      for (i = 0, attr = response->attrs; attr; attr = attr->next)
      {
       /*
        * Skip leading attributes until we hit a device...
	*/

	while (attr && attr->group_tag != IPP_TAG_PRINTER)
          attr = attr->next;

	if (!attr)
          break;

       /*
	* Pull the needed attributes from this device...
	*/

	device_info           = NULL;
	device_make_and_model = NULL;
	device_uri            = NULL;

	while (attr && attr->group_tag == IPP_TAG_PRINTER)
	{
          if (strcmp(attr->name, "device-info") == 0 &&
	      attr->value_tag == IPP_TAG_TEXT)
	    device_info = attr->values[0].string.text;

          if (strcmp(attr->name, "device-make-and-model") == 0 &&
	      attr->value_tag == IPP_TAG_TEXT)
	    device_make_and_model = attr->values[0].string.text;

          if (strcmp(attr->name, "device-uri") == 0 &&
	      attr->value_tag == IPP_TAG_URI)
	    device_uri = attr->values[0].string.text;

          attr = attr->next;
	}

       /*
	* See if we have everything needed...
	*/

	if (device_info && device_make_and_model && device_uri &&
	    strcasecmp(device_make_and_model, "unknown") &&
	    strchr(device_uri, ':'))
	{
	 /*
	  * Yes, now see if there is already a printer for this
	  * device...
	  */

          if (!bsearch(&device_uri, printer_devices, num_printer_devices,
	               sizeof(char *), compare_printer_devices))
          {
	   /*
	    * Not found, so it must be a new printer...
	    */

            char	options[1024],	/* Form variables for this device */
			*options_ptr;	/* Pointer into string */
	    const char	*ptr;		/* Pointer into device string */


           /*
	    * Format the printer name variable for this device...
	    *
	    * TODO: check for existing names, add number/address...
	    */

	    strcpy(options, "PRINTER_NAME=");
	    options_ptr = options + strlen(options);

	    for (ptr = device_make_and_model;
	         options_ptr < (options + sizeof(options) - 1) && *ptr;
		 ptr ++)
	      if (isalnum(*ptr & 255) || *ptr == '_' || *ptr == '-')
	        *options_ptr++ = *ptr;
	      else if (*ptr == ' ')
	        *options_ptr++ = '_';

           /*
	    * Then add the make and model in the printer info, so
	    * that MacOS clients see something reasonable...
	    */

            strlcpy(options_ptr, "&PRINTER_LOCATION=&PRINTER_INFO=",
	            sizeof(options) - (options_ptr - options));
	    options_ptr += strlen(options_ptr);

            cgiFormEncode(options_ptr, device_make_and_model,
	                sizeof(options) - (options_ptr - options));
	    options_ptr += strlen(options_ptr);

           /*
	    * Then copy the device URI...
	    */

	    strlcpy(options_ptr, "&DEVICE_URI=",
	            sizeof(options) - (options_ptr - options));
	    options_ptr += strlen(options_ptr);

            cgiFormEncode(options_ptr, device_uri,
	                sizeof(options) - (options_ptr - options));
	    options_ptr += strlen(options_ptr);

            if (options_ptr < (options + sizeof(options) - 1))
	    {
	      *options_ptr++ = ';';
	      cgiFormEncode(options_ptr, device_make_and_model,
	                  sizeof(options) - (options_ptr - options));
	    }

           /*
	    * Finally, set the form variables for this printer...
	    */

	    cgiSetArray("device_info", i, device_info);
	    cgiSetArray("device_make_and_model", i, device_make_and_model);
	    cgiSetArray("device_options", i, options);
            cgiSetArray("device_uri", i, device_uri);
	    i ++;
	  }
	}

        if (!attr)
	  break;
      }

     /*
      * Free the device list...
      */

      ippDelete(response);

      if (num_printer_devices)
        free(printer_devices);
    }
  }

 /*
  * Finally, show the main menu template...
  */

  cgiCopyTemplateLang(stdout, TEMPLATES, "admin.tmpl", getenv("LANG"));

  cgiEndHTML();
}


/*
 * 'do_printer_op()' - Do a printer operation.
 */

static void
do_printer_op(http_t      *http,	/* I - HTTP connection */
              cups_lang_t *language,	/* I - Client's language */
	      ipp_op_t    op,		/* I - Operation to perform */
	      const char  *title)	/* I - Title of page */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer;		/* Printer name (purge-jobs) */
  ipp_status_t	status;			/* Operation status... */


  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiStartHTML(title);
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
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
    status = cupsLastError();

  if (status > IPP_OK_CONFLICT)
  {
    cgiStartHTML(title);
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
  {
   /*
    * Redirect successful updates back to the printer page...
    */

    char	refresh[1024];		/* Refresh URL */

    cgiFormEncode(uri, printer, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "2;/admin?OP=redirect&URL=/printers/%s",
             uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(title);

    if (op == IPP_PAUSE_PRINTER)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-stop.tmpl", getenv("LANG"));
    else if (op == IPP_RESUME_PRINTER)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-start.tmpl", getenv("LANG"));
    else if (op == CUPS_ACCEPT_JOBS)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-accept.tmpl", getenv("LANG"));
    else if (op == CUPS_REJECT_JOBS)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-reject.tmpl", getenv("LANG"));
    else if (op == IPP_PURGE_JOBS)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-purge.tmpl", getenv("LANG"));
    else if (op == CUPS_SET_DEFAULT)
      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-default.tmpl", getenv("LANG"));
  }

  cgiEndHTML();
}


/*
 * 'do_set_allowed_users()' - Set the allowed/denied users for a queue.
 */

static void
do_set_allowed_users(
    http_t      *http,			/* I - HTTP connection */
    cups_lang_t *language)		/* I - Language */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer,		/* Printer name (purge-jobs) */
		*users,			/* List of users or groups */
		*type;			/* Allow/deny type */
  int		num_users;		/* Number of users */
  char		*ptr,			/* Pointer into users string */
		*end,			/* Pointer to end of users string */
		quote;			/* Quote character */
  ipp_attribute_t *attr;		/* Attribute */
  ipp_status_t	status;			/* Operation status... */
  static const char * const attrs[] =	/* Requested attributes */
		{
		  "requesting-user-name-allowed",
		  "requesting-user-name-denied"
		};


  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiStartHTML("Set Allowed Users");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

  users = cgiGetVariable("users");
  type  = cgiGetVariable("type");

  if (!users || !type ||
      (strcmp(type, "requesting-user-name-allowed") &&
       strcmp(type, "requesting-user-name-denied")))
  {
   /*
    * Build a Get-Printer-Attributes request, which requires the following
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

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes",
		  (int)(sizeof(attrs) / sizeof(attrs[0])), NULL, attrs);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
    {
      status = response->request.status.status_code;

      ippSetCGIVars(response, NULL, NULL, NULL, 0);

      ippDelete(response);
    }
    else
      status = cupsLastError();

    cgiStartHTML("Set Allowed Users");

    if (status > IPP_OK_CONFLICT)
    {
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
      cgiCopyTemplateLang(stdout, TEMPLATES, "users.tmpl", getenv("LANG"));

    cgiEndHTML();
  }
  else
  {
   /*
    * Save the changes...
    */

    for (num_users = 0, ptr = (char *)users; *ptr; num_users ++)
    {
     /*
      * Skip whitespace and commas...
      */

      while (*ptr == ',' || isspace(*ptr & 255))
	ptr ++;

      if (*ptr == '\'' || *ptr == '\"')
      {
       /*
	* Scan quoted name...
	*/

	quote = *ptr++;

	for (end = ptr; *end; end ++)
	  if (*end == quote)
	    break;
      }
      else
      {
       /*
	* Scan space or comma-delimited name...
	*/

        for (end = ptr; *end; end ++)
	  if (isspace(*end & 255) || *end == ',')
	    break;
      }

     /*
      * Advance to the next name...
      */

      ptr = end;
    }

   /*
    * Build a CUPS-Add-Printer request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    requesting-user-name-{allowed,denied}
    */

    request = ippNew();

    request->request.op.operation_id = CUPS_ADD_PRINTER;
    request->request.op.request_id   = 1;

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	 "attributes-charset", NULL, cupsLangEncoding(language));

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	 "attributes-natural-language", NULL, language->language);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    if (num_users == 0)
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name-allowed", NULL, "all");
    else
    {
      attr = ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                           type, num_users, NULL, NULL);

      for (i = 0, ptr = (char *)users; *ptr; i ++)
      {
       /*
        * Skip whitespace and commas...
	*/

        while (*ptr == ',' || isspace(*ptr & 255))
	  ptr ++;

        if (*ptr == '\'' || *ptr == '\"')
	{
	 /*
	  * Scan quoted name...
	  */

	  quote = *ptr++;

	  for (end = ptr; *end; end ++)
	    if (*end == quote)
	      break;
	}
	else
	{
	 /*
	  * Scan space or comma-delimited name...
	  */

          for (end = ptr; *end; end ++)
	    if (isspace(*end & 255) || *end == ',')
	      break;
        }

       /*
        * Terminate the name...
	*/

        if (*end)
          *end++ = '\0';

       /*
        * Add the name...
	*/

        attr->values[i].string.text = strdup(ptr);

       /*
        * Advance to the next name...
	*/

        ptr = end;
      }
    }

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
    {
      status = response->request.status.status_code;

      ippSetCGIVars(response, NULL, NULL, NULL, 0);

      ippDelete(response);
    }
    else
      status = cupsLastError();

    if (status > IPP_OK_CONFLICT)
    {
      cgiStartHTML("Set Allowed Users");
      cgiSetVariable("ERROR", ippErrorString(status));
      cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    }
    else
    {
     /*
      * Redirect successful updates back to the printer page...
      */

      char	refresh[1024];		/* Refresh URL */

      cgiFormEncode(uri, printer, sizeof(uri));
      snprintf(refresh, sizeof(refresh), "2;/admin?OP=redirect&URL=/printers/%s",
               uri);
      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML("Set Allowed Users");

      cgiCopyTemplateLang(stdout, TEMPLATES, "printer-modified.tmpl", getenv("LANG"));
    }

    cgiEndHTML();
  }
}


/*
 * 'do_set_sharing()' - Set printer-is-shared value...
 */

static void
do_set_sharing(http_t      *http,	/* I - HTTP connection */
               cups_lang_t *language)	/* I - Language */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer,		/* Printer name */
		*shared;		/* Sharing value */
  ipp_status_t	status;			/* Operation status... */


  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(uri, sizeof(uri), "ipp", NULL, "localhost", 0,
                     "/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", ippErrorString(IPP_NOT_FOUND));
    cgiStartHTML("Set Publishing");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

  if ((shared = cgiGetVariable("SHARED")) == NULL)
  {
    cgiSetVariable("ERROR", "Missing SHARED parameter");
    cgiStartHTML("Set Publishing");
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
    cgiEndHTML();
    return;
  }

 /*
  * Build a CUPS-Add-Printer request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    printer-is-shared
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_ADD_PRINTER;
  request->request.op.request_id   = 1;

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddBoolean(request, IPP_TAG_OPERATION, "printer-is-shared", atoi(shared));

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
  {
    status = response->request.status.status_code;

    ippSetCGIVars(response, NULL, NULL, NULL, 0);

    ippDelete(response);
  }
  else
    status = cupsLastError();

  if (status > IPP_OK_CONFLICT)
  {
    cgiStartHTML("Set Publishing");
    cgiSetVariable("ERROR", ippErrorString(status));
    cgiCopyTemplateLang(stdout, TEMPLATES, "error.tmpl", getenv("LANG"));
  }
  else
  {
   /*
    * Redirect successful updates back to the printer page...
    */

    char	refresh[1024];		/* Refresh URL */

    cgiFormEncode(uri, printer, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "2;/admin?OP=redirect&URL=/printers/%s",
             uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML("Set Publishing");

    cgiCopyTemplateLang(stdout, TEMPLATES, "printer-modified.tmpl", getenv("LANG"));
  }

  cgiEndHTML();
}


/*
 * 'match_string()' - Return the number of matching characters.
 */

static int				/* O - Number of matching characters */
match_string(const char *a,		/* I - First string */
             const char *b)		/* I - Second string */
{
  int	count;				/* Number of matching characters */


 /*
  * Loop through both strings until we hit the end of either or we find
  * a non-matching character.  For the purposes of comparison, we ignore
  * whitespace and do a case-insensitive comparison so that we have a
  * better chance of finding a match...
  */

  for (count = 0; *a && *b; a++, b++, count ++)
  {
   /*
    * Skip leading whitespace characters...
    */

    while (isspace(*a & 255))
      a ++;

    while (isspace(*b & 255))
      b ++;

   /*
    * Break out if we run out of characters...
    */

    if (!*a || !*b)
      break;

   /*
    * Do a case-insensitive comparison of the next two chars...
    */

    if (tolower(*a & 255) != tolower(*b & 255))
      break;
  }

  return (count);
}

    
/*
 * End of "$Id$".
 */
