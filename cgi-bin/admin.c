/*
 * "$Id$"
 *
 *   Administration CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2007 by Easy Software Products.
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
 *   do_add_rss_subscription() - Add a RSS subscription.
 *   do_am_class()             - Add or modify a class.
 *   do_am_printer()           - Add or modify a printer.
 *   do_cancel_subscription()  - Cancel a subscription.
 *   do_config_printer()       - Configure the default options for a printer.
 *   do_config_server()        - Configure server settings.
 *   do_delete_class()         - Delete a class...
 *   do_delete_printer()       - Delete a printer...
 *   do_export()               - Export printers to Samba...
 *   do_menu()                 - Show the main menu...
 *   do_printer_op()           - Do a printer operation.
 *   do_set_allowed_users()    - Set the allowed/denied users for a queue.
 *   do_set_sharing()          - Set printer-is-shared value...
 *   match_string()            - Return the number of matching characters.
 */

/*
 * Include necessary headers...
 */

#include "cgi-private.h"
#include <cups/adminutil.h>
#include <cups/file.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>


/*
 * Local functions...
 */

static void	do_add_rss_subscription(http_t *http);
static void	do_am_class(http_t *http, int modify);
static void	do_am_printer(http_t *http, int modify);
static void	do_cancel_subscription(http_t *http);
static void	do_config_printer(http_t *http);
static void	do_config_server(http_t *http);
static void	do_delete_class(http_t *http);
static void	do_delete_printer(http_t *http);
static void	do_export(http_t *http);
static void	do_menu(http_t *http);
static void	do_printer_op(http_t *http,
		              ipp_op_t op, const char *title);
static void	do_set_allowed_users(http_t *http);
static void	do_set_sharing(http_t *http);
static int	match_string(const char *a, const char *b);


/*
 * 'main()' - Main entry for CGI.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* Connection to the server */
  const char	*op;			/* Operation name */


 /*
  * Connect to the HTTP server...
  */

  fputs("DEBUG: admin.cgi started...\n", stderr);

  http = httpConnectEncrypt(cupsServer(), ippPort(), cupsEncryption());

  if (!http)
  {
    perror("ERROR: Unable to connect to cupsd");
    fprintf(stderr, "DEBUG: cupsServer()=\"%s\"\n",
            cupsServer() ? cupsServer() : "(null)");
    fprintf(stderr, "DEBUG: ippPort()=%d\n", ippPort());
    fprintf(stderr, "DEBUG: cupsEncryption()=%d\n", cupsEncryption());
    exit(1);
  }

  fprintf(stderr, "DEBUG: http=%p\n", http);

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

    fputs("DEBUG: No form data, showing main menu...\n", stderr);

    do_menu(http);
  }
  else if ((op = cgiGetVariable("OP")) != NULL)
  {
   /*
    * Do the operation...
    */

    fprintf(stderr, "DEBUG: op=\"%s\"...\n", op);

    if (!strcmp(op, "redirect"))
    {
      const char *url;			/* Redirection URL... */
      char	prefix[1024];		/* URL prefix */


      if (getenv("HTTPS"))
        snprintf(prefix, sizeof(prefix), "https://%s:%s",
	         getenv("SERVER_NAME"), getenv("SERVER_PORT"));
      else
        snprintf(prefix, sizeof(prefix), "http://%s:%s",
	         getenv("SERVER_NAME"), getenv("SERVER_PORT"));

      if ((url = cgiGetVariable("URL")) != NULL)
        printf("Location: %s%s\n\n", prefix, url);
      else
        printf("Location: %s/admin\n\n", prefix);
    }
    else if (!strcmp(op, "start-printer"))
      do_printer_op(http, IPP_RESUME_PRINTER, cgiText(_("Start Printer")));
    else if (!strcmp(op, "stop-printer"))
      do_printer_op(http, IPP_PAUSE_PRINTER, cgiText(_("Stop Printer")));
    else if (!strcmp(op, "start-class"))
      do_printer_op(http, IPP_RESUME_PRINTER, cgiText(_("Start Class")));
    else if (!strcmp(op, "stop-class"))
      do_printer_op(http, IPP_PAUSE_PRINTER, cgiText(_("Stop Class")));
    else if (!strcmp(op, "accept-jobs"))
      do_printer_op(http, CUPS_ACCEPT_JOBS, cgiText(_("Accept Jobs")));
    else if (!strcmp(op, "reject-jobs"))
      do_printer_op(http, CUPS_REJECT_JOBS, cgiText(_("Reject Jobs")));
    else if (!strcmp(op, "purge-jobs"))
      do_printer_op(http, IPP_PURGE_JOBS, cgiText(_("Purge Jobs")));
    else if (!strcmp(op, "set-allowed-users"))
      do_set_allowed_users(http);
    else if (!strcmp(op, "set-as-default"))
      do_printer_op(http, CUPS_SET_DEFAULT, cgiText(_("Set As Default")));
    else if (!strcmp(op, "set-sharing"))
      do_set_sharing(http);
    else if (!strcmp(op, "add-class"))
      do_am_class(http, 0);
    else if (!strcmp(op, "add-printer"))
      do_am_printer(http, 0);
    else if (!strcmp(op, "modify-class"))
      do_am_class(http, 1);
    else if (!strcmp(op, "modify-printer"))
      do_am_printer(http, 1);
    else if (!strcmp(op, "delete-class"))
      do_delete_class(http);
    else if (!strcmp(op, "delete-printer"))
      do_delete_printer(http);
    else if (!strcmp(op, "set-printer-options"))
      do_config_printer(http);
    else if (!strcmp(op, "config-server"))
      do_config_server(http);
    else if (!strcmp(op, "export-samba"))
      do_export(http);
    else if (!strcmp(op, "add-rss-subscription"))
      do_add_rss_subscription(http);
    else if (!strcmp(op, "cancel-subscription"))
      do_cancel_subscription(http);
    else
    {
     /*
      * Bad operation code...  Display an error...
      */

      cgiStartHTML(cgiText(_("Administration")));
      cgiCopyTemplateLang("error-op.tmpl");
      cgiEndHTML();
    }
  }
  else
  {
   /*
    * Form data but no operation code...  Display an error...
    */

    cgiStartHTML(cgiText(_("Administration")));
    cgiCopyTemplateLang("error-op.tmpl");
    cgiEndHTML();
  }

 /*
  * Close the HTTP server connection...
  */

  httpClose(http);

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'do_add_rss_subscription()' - Add a RSS subscription.
 */

static void
do_add_rss_subscription(http_t *http)	/* I - HTTP connection */
{
  ipp_t		*request,		/* IPP request data */
		*response;		/* IPP response data */
  char		rss_uri[1024];		/* RSS notify-recipient URI */
  int		num_events;		/* Number of events */
  const char	*events[12],		/* Subscribed events */
		*subscription_name,	/* Subscription name */
		*printer_uri,		/* Printer URI */
		*ptr,			/* Pointer into name */
		*user;			/* Username */
  int		max_events;		/* Maximum number of events */


 /*
  * See if we have all of the required information...
  */

  subscription_name = cgiGetVariable("SUBSCRIPTION_NAME");
  printer_uri       = cgiGetVariable("PRINTER_URI");
  num_events        = 0;

  if (cgiGetVariable("EVENT_JOB_CREATED"))
    events[num_events ++] = "job-created";
  if (cgiGetVariable("EVENT_JOB_COMPLETED"))
    events[num_events ++] = "job-completed";
  if (cgiGetVariable("EVENT_JOB_STOPPED"))
    events[num_events ++] = "job-stopped";
  if (cgiGetVariable("EVENT_JOB_CONFIG_CHANGED"))
    events[num_events ++] = "job-config-changed";
  if (cgiGetVariable("EVENT_PRINTER_STOPPED"))
    events[num_events ++] = "printer-stopped";
  if (cgiGetVariable("EVENT_PRINTER_ADDED"))
    events[num_events ++] = "printer-added";
  if (cgiGetVariable("EVENT_PRINTER_MODIFIED"))
    events[num_events ++] = "printer-modified";
  if (cgiGetVariable("EVENT_PRINTER_DELETED"))
    events[num_events ++] = "printer-deleted";
  if (cgiGetVariable("EVENT_SERVER_STARTED"))
    events[num_events ++] = "server-started";
  if (cgiGetVariable("EVENT_SERVER_STOPPED"))
    events[num_events ++] = "server-stopped";
  if (cgiGetVariable("EVENT_SERVER_RESTARTED"))
    events[num_events ++] = "server-restarted";
  if (cgiGetVariable("EVENT_SERVER_AUDIT"))
    events[num_events ++] = "server-audit";

  if ((ptr = cgiGetVariable("MAX_EVENTS")) != NULL)
    max_events = atoi(ptr);
  else
    max_events = 0;

  if (!subscription_name || !printer_uri || !num_events ||
      max_events <= 0 || max_events > 9999)
  {
   /*
    * Don't have everything we need, so get the available printers
    * and classes and (re)show the add page...
    */

    request  = ippNewRequest(CUPS_GET_PRINTERS);
    response = cupsDoRequest(http, request, "/");

    cgiSetIPPVars(response, NULL, NULL, NULL, 0);

    ippDelete(response);

    cgiStartHTML(cgiText(_("Add RSS Subscription")));

    cgiCopyTemplateLang("add-rss-subscription.tmpl");

    cgiEndHTML();
    return;
  }

 /*
  * Validate the subscription name...
  */

  for (ptr = subscription_name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' ||
        *ptr == '?' || *ptr == '#')
      break;

  if (*ptr)
  {
    cgiSetVariable("ERROR",
                   cgiText(_("The subscription name may not "
			     "contain spaces, slashes (/), question marks (?), "
			     "or the pound sign (#).")));
    cgiStartHTML(_("Add RSS Subscription"));
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

 /*
  * Add the subscription...
  */

  ptr = subscription_name + strlen(subscription_name) - 4;
  if (ptr < subscription_name || strcmp(ptr, ".rss"))
    httpAssembleURIf(HTTP_URI_CODING_ALL, rss_uri, sizeof(rss_uri), "rss",
                     NULL, NULL, 0, "/%s.rss?max_events=%d", subscription_name,
		     max_events);
  else
    httpAssembleURIf(HTTP_URI_CODING_ALL, rss_uri, sizeof(rss_uri), "rss",
                     NULL, NULL, 0, "/%s?max_events=%d", subscription_name,
		     max_events);

  request = ippNewRequest(IPP_CREATE_PRINTER_SUBSCRIPTION);

  if (!strcasecmp(printer_uri, "#ALL#"))
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/");
  else
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, printer_uri);

  if ((user = getenv("REMOTE_USER")) == NULL)
    user = "guest";

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, user);

  ippAddString(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI,
               "notify-recipient-uri", NULL, rss_uri);
  ippAddStrings(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD, "notify-events",
                num_events, NULL, events);
  ippAddInteger(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                "notify-lease-duration", 0);

  ippDelete(cupsDoRequest(http, request, "/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(_("Add RSS Subscription"));
    cgiShowIPPError(_("Unable to add RSS subscription:"));
  }
  else
  {
   /*
    * Redirect successful updates back to the admin page...
    */

    cgiSetVariable("refresh_page", "5;URL=/admin");
    cgiStartHTML(_("Add RSS Subscription"));
    cgiCopyTemplateLang("subscription-added.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'do_am_class()' - Add or modify a class.
 */

static void
do_am_class(http_t *http,		/* I - HTTP connection */
	    int    modify)		/* I - Modify the printer? */
{
  int		i, j;			/* Looping vars */
  int		element;		/* Element number */
  int		num_printers;		/* Number of printers */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* member-uris attribute */
  char		uri[HTTP_MAX_URI];	/* Device or printer URI */
  const char	*name,			/* Pointer to class name */
		*ptr;			/* Pointer to CGI variable */
  const char	*title;			/* Title of page */
  static const char * const pattrs[] =	/* Requested printer attributes */
		{
		  "member-names",
		  "printer-info",
		  "printer-location"
		};


  title = cgiText(modify ? _("Modify Class") : _("Add Class"));
  name  = cgiGetVariable("PRINTER_NAME");

  if (cgiGetVariable("PRINTER_LOCATION") == NULL)
  {
   /*
    * Build a CUPS_GET_PRINTERS request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    */

    request = ippNewRequest(CUPS_GET_PRINTERS);

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
	if (attr->name && !strcmp(attr->name, "printer-uri-supported"))
	{
	  if ((ptr = strrchr(attr->values[0].string.text, '/')) != NULL &&
	      (!name || strcasecmp(name, ptr + 1)))
	  {
	   /*
	    * Don't show the current class...
	    */

	    cgiSetArray("MEMBER_URIS", element, attr->values[0].string.text);
	    element ++;
	  }
	}

      for (element = 0, attr = response->attrs;
	   attr != NULL;
	   attr = attr->next)
	if (attr->name && !strcmp(attr->name, "printer-name"))
	{
	  if (!name || strcasecmp(name, attr->values[0].string.text))
	  {
	   /*
	    * Don't show the current class...
	    */

	    cgiSetArray("MEMBER_NAMES", element, attr->values[0].string.text);
	    element ++;
	  }
	}

      num_printers = cgiGetSize("MEMBER_URIS");

      ippDelete(response);
    }
    else
      num_printers = 0;

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

      request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

      httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                       "localhost", 0, "/classes/%s", name);
      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                   NULL, uri);

      ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                    "requested-attributes",
		    (int)(sizeof(pattrs) / sizeof(pattrs[0])),
		    NULL, pattrs);

     /*
      * Do the request and get back a response...
      */

      if ((response = cupsDoRequest(http, request, "/")) != NULL)
      {
	if ((attr = ippFindAttribute(response, "member-names",
	                             IPP_TAG_NAME)) != NULL)
	{
	 /*
          * Mark any current members in the class...
	  */

          for (j = 0; j < num_printers; j ++)
	    cgiSetArray("MEMBER_SELECTED", j, "");

          for (i = 0; i < attr->num_values; i ++)
	  {
	    for (j = 0; j < num_printers; j ++)
	    {
	      if (!strcasecmp(attr->values[i].string.text,
	                      cgiGetArray("MEMBER_NAMES", j)))
	      {
		cgiSetArray("MEMBER_SELECTED", j, "SELECTED");
		break;
	      }
            }
          }
	}

	if ((attr = ippFindAttribute(response, "printer-info",
	                             IPP_TAG_TEXT)) != NULL)
	  cgiSetVariable("PRINTER_INFO", attr->values[0].string.text);

	if ((attr = ippFindAttribute(response, "printer-location",
	                             IPP_TAG_TEXT)) != NULL)
	  cgiSetVariable("PRINTER_LOCATION", attr->values[0].string.text);

	ippDelete(response);
      }

     /*
      * Update the location and description of an existing printer...
      */

      cgiStartHTML(title);
      cgiCopyTemplateLang("modify-class.tmpl");
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiStartHTML(title);
      cgiCopyTemplateLang("add-class.tmpl");
    }

    cgiEndHTML();

    return;
  }

  for (ptr = name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' || *ptr == '#')
      break;

  if (*ptr || ptr == name || strlen(name) > 127)
  {
    cgiSetVariable("ERROR",
                   cgiText(_("The class name may only contain up to "
			     "127 printable characters and may not "
			     "contain spaces, slashes (/), or the "
			     "pound sign (#).")));
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

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

  request = ippNewRequest(CUPS_ADD_CLASS);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, "/classes/%s",
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

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(title);
    cgiShowIPPError(modify ? _("Unable to modify class:") :
                             _("Unable to add class:"));
  }
  else
  {
   /*
    * Redirect successful updates back to the class page...
    */

    char	refresh[1024];		/* Refresh URL */

    cgiFormEncode(uri, name, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "5;URL=/admin/?OP=redirect&URL=/classes/%s",
             uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(title);

    if (modify)
      cgiCopyTemplateLang("class-modified.tmpl");
    else
      cgiCopyTemplateLang("class-added.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'do_am_printer()' - Add or modify a printer.
 */

static void
do_am_printer(http_t *http,		/* I - HTTP connection */
	      int    modify)		/* I - Modify the printer? */
{
  int		i;			/* Looping var */
  ipp_attribute_t *attr;		/* Current attribute */
  ipp_t		*request,		/* IPP request */
		*response,		/* IPP response */
		*oldinfo;		/* Old printer information */
  const cgi_file_t *file;		/* Uploaded file, if any */
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


  ptr = cgiGetVariable("DEVICE_URI");
  fprintf(stderr, "DEBUG: do_am_printer: DEVICE_URI=\"%s\"\n",
          ptr ? ptr : "(null)");

  title = cgiText(modify ? _("Modify Printer") : _("Add Printer"));

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

    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s",
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
    cgiStartHTML(title);

    if (modify)
    {
     /*
      * Update the location and description of an existing printer...
      */

      if (oldinfo)
	cgiSetIPPVars(oldinfo, NULL, NULL, NULL, 0);

      cgiCopyTemplateLang("modify-printer.tmpl");
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

      cgiCopyTemplateLang("add-printer.tmpl");
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
    cgiSetVariable("ERROR",
                   cgiText(_("The printer name may only contain up to "
			     "127 printable characters and may not "
			     "contain spaces, slashes (/), or the "
			     "pound sign (#).")));
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

  file = cgiGetFile();

  if (file)
  {
    fprintf(stderr, "DEBUG: file->tempfile=%s\n", file->tempfile);
    fprintf(stderr, "DEBUG: file->name=%s\n", file->name);
    fprintf(stderr, "DEBUG: file->filename=%s\n", file->filename);
    fprintf(stderr, "DEBUG: file->mimetype=%s\n", file->mimetype);
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

    fputs("DEBUG: Getting list of devices...\n", stderr);

    request = ippNewRequest(CUPS_GET_DEVICES);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, "ipp://localhost/printers/");

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      fputs("DEBUG: Got device list!\n", stderr);

      cgiSetIPPVars(response, NULL, NULL, NULL, 0);
      ippDelete(response);
    }
    else
      fprintf(stderr,
              "ERROR: CUPS-Get-Devices request failed with status %x: %s\n",
	      cupsLastError(), cupsLastErrorString());

   /*
    * Let the user choose...
    */

    if ((attr = ippFindAttribute(oldinfo, "device-uri", IPP_TAG_URI)) != NULL)
    {
      strlcpy(uri, attr->values[0].string.text, sizeof(uri));
      if ((uriptr = strchr(uri, ':')) != NULL && strncmp(uriptr, "://", 3) == 0)
        *uriptr = '\0';

      cgiSetVariable("CURRENT_DEVICE_URI", attr->values[0].string.text);
      cgiSetVariable("CURRENT_DEVICE_SCHEME", uri);
    }

    cgiStartHTML(title);
    cgiCopyTemplateLang("choose-device.tmpl");
    cgiEndHTML();
  }
  else if (strchr(var, '/') == NULL)
  {
    if ((attr = ippFindAttribute(oldinfo, "device-uri", IPP_TAG_URI)) != NULL)
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
    cgiCopyTemplateLang("choose-uri.tmpl");
    cgiEndHTML();
  }
  else if (!strncmp(var, "serial:", 7) && !cgiGetVariable("BAUDRATE"))
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
    cgiCopyTemplateLang("choose-serial.tmpl");
    cgiEndHTML();
  }
  else if (!file && (var = cgiGetVariable("PPD_NAME")) == NULL)
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
      http_status_t	get_status;	/* Status of GET */


      /* TODO: Use cupsGetFile() API... */
      snprintf(uri, sizeof(uri), "/printers/%s.ppd", name);

      if (httpGet(http, uri))
        httpGet(http, uri);

      while ((get_status = httpUpdate(http)) == HTTP_CONTINUE);

      if (get_status != HTTP_OK)
      {
        fprintf(stderr, "ERROR: Unable to get PPD file %s: %d - %s\n",
	        uri, get_status, httpStatus(get_status));
      }
      else if ((fd = cupsTempFd(filename, sizeof(filename))) >= 0)
      {
	while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
          write(fd, buffer, bytes);

	close(fd);

        if ((ppd = ppdOpenFile(filename)) != NULL)
	{
	  if (ppd->manufacturer)
	    cgiSetVariable("CURRENT_MAKE", ppd->manufacturer);

	  if (ppd->nickname)
	    cgiSetVariable("CURRENT_MAKE_AND_MODEL", ppd->nickname);

          ppdClose(ppd);
          unlink(filename);
	}
	else
	{
	  fprintf(stderr, "ERROR: Unable to open PPD file %s: %s\n",
	          filename, ppdErrorString(ppdLastError(&bytes)));
	}
      }
      else
      {
        httpFlush(http);

        fprintf(stderr,
	        "ERROR: Unable to create temporary file for PPD file: %s\n",
	        strerror(errno));
      }
    }
    else if ((uriptr = strrchr(cgiGetVariable("DEVICE_URI"), '|')) != NULL)
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

    request = ippNewRequest(CUPS_GET_PPDS);

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

      if (cgiSetIPPVars(response, NULL, NULL, NULL, 0) == 0)
      {
       /*
        * No PPD files with this make, try again with all makes...
	*/

        ippDelete(response);

	request = ippNewRequest(CUPS_GET_PPDS);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                     NULL, "ipp://localhost/printers/");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                     "requested-attributes", NULL, "ppd-make");

	if ((response = cupsDoRequest(http, request, "/")) != NULL)
          cgiSetIPPVars(response, NULL, NULL, NULL, 0);

        cgiStartHTML(title);
	cgiCopyTemplateLang("choose-make.tmpl");
        cgiEndHTML();
      }
      else if (!var)
      {
        cgiStartHTML(title);
	cgiCopyTemplateLang("choose-make.tmpl");
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
	cgiCopyTemplateLang("choose-model.tmpl");
        cgiEndHTML();
      }

      ippDelete(response);
    }
    else
    {
      cgiStartHTML(title);
      cgiShowIPPError(_("Unable to get list of printer drivers:"));
      cgiCopyTemplateLang("error.tmpl");
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

    request = ippNewRequest(CUPS_ADD_PRINTER);

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s",
		     cgiGetVariable("PRINTER_NAME"));
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-location",
                 NULL, cgiGetVariable("PRINTER_LOCATION"));

    ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_TEXT, "printer-info",
                 NULL, cgiGetVariable("PRINTER_INFO"));

    if (!file)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "ppd-name",
                   NULL, cgiGetVariable("PPD_NAME"));

    strlcpy(uri, cgiGetVariable("DEVICE_URI"), sizeof(uri));

   /*
    * Strip make and model from URI...
    */

    if ((uriptr = strrchr(uri, '|')) != NULL)
      *uriptr = '\0';

    if (!strncmp(uri, "serial:", 7))
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

    if (file)
      ippDelete(cupsDoFileRequest(http, request, "/admin/", file->tempfile));
    else
      ippDelete(cupsDoRequest(http, request, "/admin/"));

    if (cupsLastError() > IPP_OK_CONFLICT)
    {
      cgiStartHTML(title);
      cgiShowIPPError(modify ? _("Unable to modify printer:") :
                               _("Unable to add printer:"));
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
	         "5;/admin/?OP=redirect&URL=/printers/%s", uri);
      else
	snprintf(refresh, sizeof(refresh),
	         "5;URL=/admin/?OP=set-printer-options&PRINTER_NAME=%s", uri);

      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(title);

      if (modify)
        cgiCopyTemplateLang("printer-modified.tmpl");
      else
        cgiCopyTemplateLang("printer-added.tmpl");
    }

    cgiEndHTML();
  }

  if (oldinfo)
    ippDelete(oldinfo);
}


/*
 * 'do_cancel_subscription()' - Cancel a subscription.
 */

static void
do_cancel_subscription(http_t *http)/* I - HTTP connection */
{
  ipp_t		*request;		/* IPP request data */
  const char	*var,			/* Form variable */
		*user;			/* Username */
  int		id;			/* Subscription ID */


 /*
  * See if we have all of the required information...
  */

  if ((var = cgiGetVariable("NOTIFY_SUBSCRIPTION_ID")) != NULL)
    id = atoi(var);
  else
    id = 0;

  if (id <= 0)
  {
    cgiSetVariable("ERROR", cgiText(_("Bad subscription ID!")));
    cgiStartHTML(_("Cancel RSS Subscription"));
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

 /*
  * Cancel the subscription...
  */

  request = ippNewRequest(IPP_CANCEL_SUBSCRIPTION);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, "ipp://localhost/");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                "notify-subscription-id", id);

  if ((user = getenv("REMOTE_USER")) == NULL)
    user = "guest";

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, user);

  ippDelete(cupsDoRequest(http, request, "/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(_("Cancel RSS Subscription"));
    cgiShowIPPError(_("Unable to cancel RSS subscription:"));
  }
  else
  {
   /*
    * Redirect successful updates back to the admin page...
    */

    cgiSetVariable("refresh_page", "5;URL=/admin");
    cgiStartHTML(_("Cancel RSS Subscription"));
    cgiCopyTemplateLang("subscription-canceled.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'do_config_printer()' - Configure the default options for a printer.
 */

static void
do_config_printer(http_t *http)		/* I - HTTP connection */
{
  int		i, j, k, m;		/* Looping vars */
  int		have_options;		/* Have options? */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*var;			/* Variable value */
  const char	*printer;		/* Printer printer name */
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
  const char	*title;			/* Page title */


  title = cgiText(_("Set Printer Options"));

  fprintf(stderr, "DEBUG: do_config_printer(http=%p)\n", http);

 /*
  * Get the printer name...
  */

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s", printer);
  else
  {
    cgiSetVariable("ERROR", cgiText(_("Missing form variable!")));
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

  fprintf(stderr, "DEBUG: printer=\"%s\", uri=\"%s\"...\n", printer, uri);

 /*
  * Get the PPD file...
  */

  if ((filename = cupsGetPPD2(http, printer)) == NULL)
  {
    fputs("DEBUG: No PPD file!?!\n", stderr);

    cgiStartHTML(title);
    cgiShowIPPError(_("Unable to get PPD file!"));
    cgiEndHTML();
    return;
  }

  fprintf(stderr, "DEBUG: Got PPD file: \"%s\"\n", filename);

  if ((ppd = ppdOpenFile(filename)) == NULL)
  {
    cgiSetVariable("ERROR", ppdErrorString(ppdLastError(&i)));
    cgiSetVariable("MESSAGE", cgiText(_("Unable to open PPD file:")));
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
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

    for (j = group->num_options, option = group->options;
         j > 0;
	 j --, option ++)
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

    fputs("DEBUG: Showing options...\n", stderr);

    ppdLocalize(ppd);

    cgiStartHTML(cgiText(_("Set Printer Options")));
    cgiCopyTemplateLang("set-printer-options-header.tmpl");

    if (ppdConflicts(ppd))
    {
      for (i = ppd->num_groups, k = 0, group = ppd->groups;
           i > 0;
	   i --, group ++)
	for (j = group->num_options, option = group->options;
	     j > 0;
	     j --, option ++)
          if (option->conflicted)
	  {
	    cgiSetArray("ckeyword", k, option->keyword);
	    cgiSetArray("ckeytext", k, option->text);
	    k ++;
	  }

      cgiCopyTemplateLang("option-conflict.tmpl");
    }

    for (i = ppd->num_groups, group = ppd->groups;
	 i > 0;
	 i --, group ++)
    {
      if (!strcmp(group->name, "InstallableOptions"))
	cgiSetVariable("GROUP", cgiText(_("Options Installed")));
      else
	cgiSetVariable("GROUP", group->text);

      cgiCopyTemplateLang("option-header.tmpl");
      
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
              cgiCopyTemplateLang("option-boolean.tmpl");
              break;
	  case PPD_UI_PICKONE :
              cgiCopyTemplateLang("option-pickone.tmpl");
              break;
	  case PPD_UI_PICKMANY :
              cgiCopyTemplateLang("option-pickmany.tmpl");
              break;
	}
      }

      cgiCopyTemplateLang("option-trailer.tmpl");
    }

   /*
    * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    */

    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s", printer);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      if ((attr = ippFindAttribute(response, "job-sheets-supported",
                                   IPP_TAG_ZERO)) != NULL)
      {
       /*
	* Add the job sheets options...
	*/

	cgiSetVariable("GROUP", cgiText(_("Banners")));
	cgiCopyTemplateLang("option-header.tmpl");

	cgiSetSize("CHOICES", attr->num_values);
	cgiSetSize("TEXT", attr->num_values);
	for (k = 0; k < attr->num_values; k ++)
	{
	  cgiSetArray("CHOICES", k, attr->values[k].string.text);
	  cgiSetArray("TEXT", k, attr->values[k].string.text);
	}

        attr = ippFindAttribute(response, "job-sheets-default", IPP_TAG_ZERO);

        cgiSetVariable("KEYWORD", "job_sheets_start");
	cgiSetVariable("KEYTEXT", cgiText(_("Starting Banner")));
        cgiSetVariable("DEFCHOICE", attr == NULL ?
	                            "" : attr->values[0].string.text);

	cgiCopyTemplateLang("option-pickone.tmpl");

        cgiSetVariable("KEYWORD", "job_sheets_end");
	cgiSetVariable("KEYTEXT", cgiText(_("Ending Banner")));
        cgiSetVariable("DEFCHOICE", attr == NULL && attr->num_values > 1 ?
	                            "" : attr->values[1].string.text);

	cgiCopyTemplateLang("option-pickone.tmpl");

	cgiCopyTemplateLang("option-trailer.tmpl");
      }

      if (ippFindAttribute(response, "printer-error-policy-supported",
                           IPP_TAG_ZERO) ||
          ippFindAttribute(response, "printer-op-policy-supported",
	                   IPP_TAG_ZERO))
      {
       /*
	* Add the error and operation policy options...
	*/

	cgiSetVariable("GROUP", cgiText(_("Policies")));
	cgiCopyTemplateLang("option-header.tmpl");

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
	  cgiSetVariable("KEYTEXT", cgiText(_("Error Policy")));
          cgiSetVariable("DEFCHOICE", attr == NULL ?
	                              "" : attr->values[0].string.text);
        }

	cgiCopyTemplateLang("option-pickone.tmpl");

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
	  cgiSetVariable("KEYTEXT", cgiText(_("Operation Policy")));
          cgiSetVariable("DEFCHOICE", attr == NULL ?
	                              "" : attr->values[0].string.text);

	  cgiCopyTemplateLang("option-pickone.tmpl");
        }

	cgiCopyTemplateLang("option-trailer.tmpl");
      }

      ippDelete(response);
    }

   /*
    * Binary protocol support...
    */

    if (ppd->protocols && strstr(ppd->protocols, "BCP"))
    {
      protocol = ppdFindAttr(ppd, "cupsProtocol", NULL);

      cgiSetVariable("GROUP", cgiText(_("PS Binary Protocol")));
      cgiCopyTemplateLang("option-header.tmpl");

      cgiSetSize("CHOICES", 2);
      cgiSetSize("TEXT", 2);
      cgiSetArray("CHOICES", 0, "None");
      cgiSetArray("TEXT", 0, cgiText(_("None")));

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
      cgiSetVariable("KEYTEXT", cgiText(_("PS Binary Protocol")));
      cgiSetVariable("DEFCHOICE", protocol ? protocol->value : "None");

      cgiCopyTemplateLang("option-pickone.tmpl");

      cgiCopyTemplateLang("option-trailer.tmpl");
    }

    cgiCopyTemplateLang("set-printer-options-trailer.tmpl");
    cgiEndHTML();
  }
  else
  {
   /*
    * Set default options...
    */

    fputs("DEBUG: Setting options...\n", stderr);

    out = cupsTempFile2(tempfile, sizeof(tempfile));
    in  = cupsFileOpen(filename, "r");

    if (!in || !out)
    {
      cgiSetVariable("ERROR", strerror(errno));
      cgiStartHTML(cgiText(_("Set Printer Options")));
      cgiCopyTemplateLang("error.tmpl");
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

        if (!strcmp(keyword, "PageRegion") ||
	    !strcmp(keyword, "PaperDimension") ||
	    !strcmp(keyword, "ImageableArea"))
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

    request = ippNewRequest(CUPS_ADD_PRINTER);

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s",
		     cgiGetVariable("PRINTER_NAME"));
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

    ippDelete(cupsDoFileRequest(http, request, "/admin/", tempfile));

    if (cupsLastError() > IPP_OK_CONFLICT)
    {
      cgiStartHTML(title);
      cgiShowIPPError(_("Unable to set options:"));
    }
    else
    {
     /*
      * Redirect successful updates back to the printer page...
      */

      char	refresh[1024];		/* Refresh URL */


      cgiFormEncode(uri, printer, sizeof(uri));
      snprintf(refresh, sizeof(refresh),
               "5;URL=/admin/?OP=redirect&URL=/printers/%s", uri);
      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(title);

      cgiCopyTemplateLang("printer-configured.tmpl");
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
do_config_server(http_t *http)		/* I - HTTP connection */
{
  if (cgiIsPOST() && !cgiGetVariable("CUPSDCONF"))
  {
   /*
    * Save basic setting changes...
    */

    int			num_settings;	/* Number of server settings */
    cups_option_t	*settings;	/* Server settings */
    const char		*debug_logging,	/* DEBUG_LOGGING value */
			*remote_admin,	/* REMOTE_ADMIN value */
			*remote_any,	/* REMOTE_ANY value */
			*remote_printers,
					/* REMOTE_PRINTERS value */
			*share_printers,/* SHARE_PRINTERS value */
#ifdef HAVE_GSSAPI
			*default_auth_type,
					/* DefaultAuthType value */
#endif /* HAVE_GSSAPI */
			*user_cancel_any;
					/* USER_CANCEL_ANY value */


   /*
    * Get the checkbox values from the form...
    */

    debug_logging     = cgiGetVariable("DEBUG_LOGGING") ? "1" : "0";
    remote_admin      = cgiGetVariable("REMOTE_ADMIN") ? "1" : "0";
    remote_any        = cgiGetVariable("REMOTE_ANY") ? "1" : "0";
    remote_printers   = cgiGetVariable("REMOTE_PRINTERS") ? "1" : "0";
    share_printers    = cgiGetVariable("SHARE_PRINTERS") ? "1" : "0";
    user_cancel_any   = cgiGetVariable("USER_CANCEL_ANY") ? "1" : "0";
#ifdef HAVE_GSSAPI
    default_auth_type = cgiGetVariable("KERBEROS") ? "Negotiate" : "Basic";

    fprintf(stderr, "DEBUG: DefaultAuthType %s\n", default_auth_type);
#endif /* HAVE_GSSAPI */

   /*
    * Get the current server settings...
    */

    if (!cupsAdminGetServerSettings(http, &num_settings, &settings))
    {
      cgiStartHTML(cgiText(_("Change Settings")));
      cgiSetVariable("MESSAGE",
                     cgiText(_("Unable to change server settings:")));
      cgiSetVariable("ERROR", cupsLastErrorString());
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();
      return;
    }

   /*
    * See if the settings have changed...
    */

    if (strcmp(debug_logging, cupsGetOption(CUPS_SERVER_DEBUG_LOGGING,
                                            num_settings, settings)) ||
        strcmp(remote_admin, cupsGetOption(CUPS_SERVER_REMOTE_ADMIN,
                                           num_settings, settings)) ||
        strcmp(remote_any, cupsGetOption(CUPS_SERVER_REMOTE_ANY,
                                         num_settings, settings)) ||
        strcmp(remote_printers, cupsGetOption(CUPS_SERVER_REMOTE_PRINTERS,
                                              num_settings, settings)) ||
        strcmp(share_printers, cupsGetOption(CUPS_SERVER_SHARE_PRINTERS,
                                             num_settings, settings)) ||
#ifdef HAVE_GSSAPI
        !cupsGetOption("DefaultAuthType", num_settings, settings) ||
	strcmp(default_auth_type, cupsGetOption("DefaultAuthType",
	                                        num_settings, settings)) ||
#endif /* HAVE_GSSAPI */
        strcmp(user_cancel_any, cupsGetOption(CUPS_SERVER_USER_CANCEL_ANY,
                                              num_settings, settings)))
    {
     /*
      * Settings *have* changed, so save the changes...
      */

      cupsFreeOptions(num_settings, settings);

      num_settings = 0;
      num_settings = cupsAddOption(CUPS_SERVER_DEBUG_LOGGING,
                                   debug_logging, num_settings, &settings);
      num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ADMIN,
                                   remote_admin, num_settings, &settings);
      num_settings = cupsAddOption(CUPS_SERVER_REMOTE_ANY,
                                   remote_any, num_settings, &settings);
      num_settings = cupsAddOption(CUPS_SERVER_REMOTE_PRINTERS,
                                   remote_printers, num_settings, &settings);
      num_settings = cupsAddOption(CUPS_SERVER_SHARE_PRINTERS,
                                   share_printers, num_settings, &settings);
      num_settings = cupsAddOption(CUPS_SERVER_USER_CANCEL_ANY,
                                   user_cancel_any, num_settings, &settings);
#ifdef HAVE_GSSAPI
      num_settings = cupsAddOption("DefaultAuthType", default_auth_type,
                                   num_settings, &settings);
#endif /* HAVE_GSSAPI */

      if (!cupsAdminSetServerSettings(http, num_settings, settings))
      {
	cgiStartHTML(cgiText(_("Change Settings")));
	cgiSetVariable("MESSAGE",
                       cgiText(_("Unable to change server settings:")));
	cgiSetVariable("ERROR", cupsLastErrorString());
	cgiCopyTemplateLang("error.tmpl");
      }
      else
      {
	cgiSetVariable("refresh_page", "5;URL=/admin/?OP=redirect");
	cgiStartHTML(cgiText(_("Change Settings")));
	cgiCopyTemplateLang("restart.tmpl");
      }
    }
    else
    {
     /*
      * No changes...
      */

      cgiSetVariable("refresh_page", "5;URL=/admin/?OP=redirect");
      cgiStartHTML(cgiText(_("Change Settings")));
      cgiCopyTemplateLang("norestart.tmpl");
    }

    cupsFreeOptions(num_settings, settings);

    cgiEndHTML();
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
      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiSetVariable("MESSAGE", cgiText(_("Unable to create temporary file:")));
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();
      
      perror(tempfile);
      return;
    }

    if ((temp = cupsFileOpenFd(tempfd, "w")) == NULL)
    {
      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiSetVariable("MESSAGE", cgiText(_("Unable to create temporary file:")));
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang("error.tmpl");
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
      cgiSetVariable("MESSAGE",
                     cgiText(_("Unable to upload cupsd.conf file:")));
      cgiSetVariable("ERROR", httpStatus(status));

      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiCopyTemplateLang("error.tmpl");
    }
    else
    {
      cgiSetVariable("refresh_page", "5;URL=/admin/?OP=redirect");

      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiCopyTemplateLang("restart.tmpl");
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
      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiSetVariable("MESSAGE",
                     cgiText(_("Unable to access cupsd.conf file:")));
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();

      perror(filename);
      return;
    }

    if (info.st_size > (1024 * 1024))
    {
      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiSetVariable("MESSAGE",
                     cgiText(_("Unable to access cupsd.conf file:")));
      cgiSetVariable("ERROR",
                     cgiText(_("Unable to edit cupsd.conf files larger than "
		               "1MB!")));
      cgiCopyTemplateLang("error.tmpl");
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

      cgiStartHTML(cgiText(_("Edit Configuration File")));
      cgiSetVariable("MESSAGE",
                     cgiText(_("Unable to access cupsd.conf file:")));
      cgiSetVariable("ERROR", strerror(errno));
      cgiCopyTemplateLang("error.tmpl");
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

    cgiStartHTML(cgiText(_("Edit Configuration File")));

    printf("<!-- \"%s\" -->\n", filename);

    cgiCopyTemplateLang("edit-config.tmpl");

    cgiEndHTML();
  }
}


/*
 * 'do_delete_class()' - Delete a class...
 */

static void
do_delete_class(http_t *http)		/* I - HTTP connection */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*pclass;		/* Printer class name */


 /*
  * Get form variables...
  */

  if (cgiGetVariable("CONFIRM") == NULL)
  {
    cgiStartHTML(cgiText(_("Delete Class")));
    cgiCopyTemplateLang("class-confirm.tmpl");
    cgiEndHTML();
    return;
  }

  if ((pclass = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/classes/%s", pclass);
  else
  {
    cgiStartHTML(cgiText(_("Delete Class")));
    cgiSetVariable("ERROR", cgiText(_("Missing form variable!")));
    cgiCopyTemplateLang("error.tmpl");
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

  request = ippNewRequest(CUPS_DELETE_CLASS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/admin/"));

 /*
  * Show the results...
  */

  if (cupsLastError() <= IPP_OK_CONFLICT)
  {
   /*
    * Redirect successful updates back to the classes page...
    */

    cgiSetVariable("refresh_page", "5;URL=/admin/?OP=redirect&URL=/classes");
  }

  cgiStartHTML(cgiText(_("Delete Class")));

  if (cupsLastError() > IPP_OK_CONFLICT)
    cgiShowIPPError(_("Unable to delete class:"));
  else
    cgiCopyTemplateLang("class-deleted.tmpl");

  cgiEndHTML();
}


/*
 * 'do_delete_printer()' - Delete a printer...
 */

static void
do_delete_printer(http_t *http)		/* I - HTTP connection */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* Job URI */
  const char	*printer;		/* Printer printer name */


 /*
  * Get form variables...
  */

  if (cgiGetVariable("CONFIRM") == NULL)
  {
    cgiStartHTML(cgiText(_("Delete Printer")));
    cgiCopyTemplateLang("printer-confirm.tmpl");
    cgiEndHTML();
    return;
  }

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, "/printers/%s", printer);
  else
  {
    cgiStartHTML(cgiText(_("Delete Printer")));
    cgiSetVariable("ERROR", cgiText(_("Missing form variable!")));
    cgiCopyTemplateLang("error.tmpl");
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

  request = ippNewRequest(CUPS_DELETE_PRINTER);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/admin/"));

 /*
  * Show the results...
  */

  if (cupsLastError() <= IPP_OK_CONFLICT)
  {
   /*
    * Redirect successful updates back to the printers page...
    */

    cgiSetVariable("refresh_page", "5;URL=/admin/?OP=redirect&URL=/printers");
  }

  cgiStartHTML(cgiText(_("Delete Printer")));

  if (cupsLastError() > IPP_OK_CONFLICT)
    cgiShowIPPError(_("Unable to delete printer:"));
  else
    cgiCopyTemplateLang("printer-deleted.tmpl");

  cgiEndHTML();
}


/*
 * 'do_export()' - Export printers to Samba...
 */

static void
do_export(http_t *http)			/* I - HTTP connection */
{
  int		i, j;			/* Looping vars */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  const char	*username,		/* Samba username */
		*password,		/* Samba password */
		*export_all;		/* Export all printers? */
  int		export_count,		/* Number of printers to export */
		printer_count;		/* Number of available printers */
  const char	*name,			/* What name to pull */
		*dest;			/* Current destination */
  char		ppd[1024];		/* PPD file */


 /*
  * Get form data...
  */

  username     = cgiGetVariable("USERNAME");
  password     = cgiGetVariable("PASSWORD");
  export_all   = cgiGetVariable("EXPORT_ALL");
  export_count = cgiGetSize("EXPORT_NAME");

 /*
  * Get list of available printers...
  */

  cgiSetSize("PRINTER_NAME", 0);
  cgiSetSize("PRINTER_EXPORT", 0);

  request = ippNewRequest(CUPS_GET_PRINTERS);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type", 0);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_ENUM,
                "printer-type-mask", CUPS_PRINTER_CLASS | CUPS_PRINTER_REMOTE |
		                     CUPS_PRINTER_IMPLICIT);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    cgiSetIPPVars(response, NULL, NULL, NULL, 0);
    ippDelete(response);

    if (!export_all)
    {
      printer_count = cgiGetSize("PRINTER_NAME");

      for (i = 0; i < printer_count; i ++)
      {
        dest = cgiGetArray("PRINTER_NAME", i);

        for (j = 0; j < export_count; j ++)
	  if (!strcasecmp(dest, cgiGetArray("EXPORT_NAME", j)))
            break;

        cgiSetArray("PRINTER_EXPORT", i, j < export_count ? "Y" : "");
      }
    }
  }

 /*
  * Export or get the printers to export...
  */

  if (username && *username && password && *password &&
      (export_all || export_count > 0))
  {
   /*
    * Do export...
    */

    fputs("DEBUG: Export printers...\n", stderr);

    if (export_all)
    {
      name         = "PRINTER_NAME";
      export_count = cgiGetSize("PRINTER_NAME");
    }
    else
      name = "EXPORT_NAME";

    for (i = 0; i < export_count; i ++)
    {
      dest = cgiGetArray(name, i);

      if (!cupsAdminCreateWindowsPPD(http, dest, ppd, sizeof(ppd)))
        break;

      j = cupsAdminExportSamba(dest, ppd, "localhost", username, password,
                               stderr);

      unlink(ppd);

      if (!j)
        break;
    }

    if (i < export_count)
      cgiSetVariable("ERROR", cupsLastErrorString());
    else
    {
      cgiStartHTML(cgiText(_("Export Printers to Samba")));
      cgiCopyTemplateLang("samba-exported.tmpl");
      cgiEndHTML();
      return;
    }
  }
  else if (username && !*username)
    cgiSetVariable("ERROR",
                   cgiText(_("A Samba username is required to export "
		             "printer drivers!")));
  else if (username && (!password || !*password))
    cgiSetVariable("ERROR",
                   cgiText(_("A Samba password is required to export "
		             "printer drivers!")));

 /*
  * Show form...
  */

  cgiStartHTML(cgiText(_("Export Printers to Samba")));
  cgiCopyTemplateLang("samba-export.tmpl");
  cgiEndHTML();
}


/*
 * 'do_menu()' - Show the main menu...
 */

static void
do_menu(http_t *http)			/* I - HTTP connection */
{
  int		num_settings;		/* Number of server settings */
  cups_option_t	*settings;		/* Server settings */
  const char	*val;			/* Setting value */
  char		filename[1024];		/* Temporary filename */
  const char	*datadir;		/* Location of data files */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */


 /*
  * Get the current server settings...
  */

  if (!cupsAdminGetServerSettings(http, &num_settings, &settings))
  {
    cgiSetVariable("SETTINGS_MESSAGE",
                   cgiText(_("Unable to open cupsd.conf file:")));
    cgiSetVariable("SETTINGS_ERROR", cupsLastErrorString());
  }

  if ((val = cupsGetOption(CUPS_SERVER_DEBUG_LOGGING, num_settings,
                           settings)) != NULL && atoi(val))
    cgiSetVariable("DEBUG_LOGGING", "CHECKED");

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_ADMIN, num_settings,
                           settings)) != NULL && atoi(val))
    cgiSetVariable("REMOTE_ADMIN", "CHECKED");

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_ANY, num_settings,
                           settings)) != NULL && atoi(val))
    cgiSetVariable("REMOTE_ANY", "CHECKED");

  if ((val = cupsGetOption(CUPS_SERVER_REMOTE_PRINTERS, num_settings,
                           settings)) != NULL && atoi(val))
    cgiSetVariable("REMOTE_PRINTERS", "CHECKED");

  if ((val = cupsGetOption(CUPS_SERVER_SHARE_PRINTERS, num_settings,
                           settings)) != NULL && atoi(val))
    cgiSetVariable("SHARE_PRINTERS", "CHECKED");

  if ((val = cupsGetOption(CUPS_SERVER_USER_CANCEL_ANY, num_settings,
                           settings)) != NULL && atoi(val))
    cgiSetVariable("USER_CANCEL_ANY", "CHECKED");

#ifdef HAVE_GSSAPI
  cgiSetVariable("HAVE_GSSAPI", "1");

  if ((val = cupsGetOption("DefaultAuthType", num_settings,
                           settings)) != NULL && !strcasecmp(val, "Negotiate"))
    cgiSetVariable("KERBEROS", "CHECKED");
#endif /* HAVE_GSSAPI */

  cupsFreeOptions(num_settings, settings);

 /*
  * Get the list of printers and their devices...
  */

  request = ippNewRequest(CUPS_GET_PRINTERS);

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
    cups_array_t *printer_devices;	/* Printer devices for local printers */
    char	*printer_device;	/* Current printer device */


   /*
    * Allocate an array and copy the device strings...
    */

    printer_devices = cupsArrayNew((cups_array_func_t)strcmp, NULL);

    for (attr = ippFindAttribute(response, "device-uri", IPP_TAG_URI);
         attr;
	 attr = ippFindNextAttribute(response, "device-uri", IPP_TAG_URI))
    {
      cupsArrayAdd(printer_devices, strdup(attr->values[0].string.text));
    }

   /*
    * Free the printer list and get the device list...
    */

    ippDelete(response);

    request = ippNewRequest(CUPS_GET_DEVICES);

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
          if (!strcmp(attr->name, "device-info") &&
	      attr->value_tag == IPP_TAG_TEXT)
	    device_info = attr->values[0].string.text;

          if (!strcmp(attr->name, "device-make-and-model") &&
	      attr->value_tag == IPP_TAG_TEXT)
	    device_make_and_model = attr->values[0].string.text;

          if (!strcmp(attr->name, "device-uri") &&
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

          if (!cupsArrayFind(printer_devices, (void *)device_uri))
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
	    * We use the device-info string first, then device-uri,
	    * and finally device-make-and-model to come up with a
	    * suitable name.
	    */

	    strcpy(options, "TEMPLATE_NAME=");
	    options_ptr = options + strlen(options);

            if (strncasecmp(device_info, "unknown", 7))
	      ptr = device_info;
            else if ((ptr = strstr(device_uri, "://")) != NULL)
	      ptr += 3;
	    else
	      ptr = device_make_and_model;

	    for (;
	         options_ptr < (options + sizeof(options) - 1) && *ptr;
		 ptr ++)
	      if (isalnum(*ptr & 255) || *ptr == '_' || *ptr == '-' ||
	          *ptr == '.')
	        *options_ptr++ = *ptr;
	      else if ((*ptr == ' ' || *ptr == '/') && options_ptr[-1] != '_')
	        *options_ptr++ = '_';
	      else if (*ptr == '?' || *ptr == '(')
	        break;

           /*
	    * Then add the make and model in the printer info, so
	    * that MacOS clients see something reasonable...
	    */

            strlcpy(options_ptr, "&PRINTER_LOCATION=Local+Printer"
	                         "&PRINTER_INFO=",
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
	      *options_ptr++ = '|';
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

      ippDelete(response);

     /*
      * Free the device list...
      */

      for (printer_device = (char *)cupsArrayFirst(printer_devices);
           printer_device;
	   printer_device = (char *)cupsArrayNext(printer_devices))
        free(printer_device);

      cupsArrayDelete(printer_devices);
    }
  }

 /*
  * See if Samba and the Windows drivers are installed...
  */

  if ((datadir = getenv("CUPS_DATADIR")) == NULL)
    datadir = CUPS_DATADIR;

  snprintf(filename, sizeof(filename), "%s/drivers/pscript5.dll", datadir);
  if (!access(filename, R_OK))
  {
   /*
    * Found Windows 2000 driver file, see if we have smbclient and
    * rpcclient...
    */

    if (cupsFileFind("smbclient", getenv("PATH"), 1, filename,
                     sizeof(filename)) &&
        cupsFileFind("rpcclient", getenv("PATH"), 1, filename,
	             sizeof(filename)))
      cgiSetVariable("HAVE_SAMBA", "Y");
    else
    {
      if (!cupsFileFind("smbclient", getenv("PATH"), 1, filename,
                        sizeof(filename)))
        fputs("ERROR: smbclient not found!\n", stderr);

      if (!cupsFileFind("rpcclient", getenv("PATH"), 1, filename,
                        sizeof(filename)))
        fputs("ERROR: rpcclient not found!\n", stderr);
    }
  }
  else
    perror(filename);

 /*
  * Subscriptions...
  */

  request = ippNewRequest(IPP_GET_SUBSCRIPTIONS);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, "ipp://localhost/");

  if ((response = cupsDoRequest(http, request, "/")) != NULL)
  {
    cgiSetIPPVars(response, NULL, NULL, NULL, 0);
    ippDelete(response);
  }

 /*
  * Finally, show the main menu template...
  */

  cgiStartHTML(cgiText(_("Administration")));

  cgiCopyTemplateLang("admin.tmpl");

  cgiEndHTML();
}


/*
 * 'do_printer_op()' - Do a printer operation.
 */

static void
do_printer_op(http_t      *http,	/* I - HTTP connection */
	      ipp_op_t    op,		/* I - Operation to perform */
	      const char  *title)	/* I - Title of page */
{
  ipp_t		*request;		/* IPP request */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer,		/* Printer name (purge-jobs) */
		*is_class;		/* Is a class? */


  is_class = cgiGetVariable("IS_CLASS");
  printer  = cgiGetVariable("PRINTER_NAME");

  if (!printer)
  {
    cgiSetVariable("ERROR", cgiText(_("Missing form variable!")));
    cgiStartHTML(title);
    cgiCopyTemplateLang("error.tmpl");
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

  request = ippNewRequest(op);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, is_class ? "/classes/%s" : "/printers/%s",
		   printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

 /*
  * Do the request and get back a response...
  */

  ippDelete(cupsDoRequest(http, request, "/admin/"));

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(title);
    cgiShowIPPError(_("Unable to change printer:"));
  }
  else
  {
   /*
    * Redirect successful updates back to the printer page...
    */

    char	url[1024],		/* Printer/class URL */
		refresh[1024];		/* Refresh URL */


    cgiRewriteURL(uri, url, sizeof(url), NULL);
    cgiFormEncode(uri, url, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "5;URL=/admin/?OP=redirect&URL=%s", uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(title);

    if (op == IPP_PAUSE_PRINTER)
      cgiCopyTemplateLang("printer-stop.tmpl");
    else if (op == IPP_RESUME_PRINTER)
      cgiCopyTemplateLang("printer-start.tmpl");
    else if (op == CUPS_ACCEPT_JOBS)
      cgiCopyTemplateLang("printer-accept.tmpl");
    else if (op == CUPS_REJECT_JOBS)
      cgiCopyTemplateLang("printer-reject.tmpl");
    else if (op == IPP_PURGE_JOBS)
      cgiCopyTemplateLang("printer-purge.tmpl");
    else if (op == CUPS_SET_DEFAULT)
      cgiCopyTemplateLang("printer-default.tmpl");
  }

  cgiEndHTML();
}


/*
 * 'do_set_allowed_users()' - Set the allowed/denied users for a queue.
 */

static void
do_set_allowed_users(http_t *http)	/* I - HTTP connection */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer,		/* Printer name (purge-jobs) */
		*is_class,		/* Is a class? */
		*users,			/* List of users or groups */
		*type;			/* Allow/deny type */
  int		num_users;		/* Number of users */
  char		*ptr,			/* Pointer into users string */
		*end,			/* Pointer to end of users string */
		quote;			/* Quote character */
  ipp_attribute_t *attr;		/* Attribute */
  static const char * const attrs[] =	/* Requested attributes */
		{
		  "requesting-user-name-allowed",
		  "requesting-user-name-denied"
		};


  is_class = cgiGetVariable("IS_CLASS");
  printer  = cgiGetVariable("PRINTER_NAME");

  if (!printer)
  {
    cgiSetVariable("ERROR", cgiText(_("Missing form variable!")));
    cgiStartHTML(cgiText(_("Set Allowed Users")));
    cgiCopyTemplateLang("error.tmpl");
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

    request = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, is_class ? "/classes/%s" : "/printers/%s",
		     printer);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  "requested-attributes",
		  (int)(sizeof(attrs) / sizeof(attrs[0])), NULL, attrs);

   /*
    * Do the request and get back a response...
    */

    if ((response = cupsDoRequest(http, request, "/")) != NULL)
    {
      cgiSetIPPVars(response, NULL, NULL, NULL, 0);

      ippDelete(response);
    }

    cgiStartHTML(cgiText(_("Set Allowed Users")));

    if (cupsLastError() > IPP_OK_CONFLICT)
      cgiShowIPPError(_("Unable to get printer attributes:"));
    else
      cgiCopyTemplateLang("users.tmpl");

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
    * Build a CUPS-Add-Printer/Class request, which requires the following
    * attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    requesting-user-name-{allowed,denied}
    */

    request = ippNewRequest(is_class ? CUPS_ADD_CLASS : CUPS_ADD_PRINTER);

    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, is_class ? "/classes/%s" : "/printers/%s",
		     printer);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
        	 NULL, uri);

    if (num_users == 0)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
                   "requesting-user-name-allowed", NULL, "all");
    else
    {
      attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
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

    ippDelete(cupsDoRequest(http, request, "/admin/"));

    if (cupsLastError() > IPP_OK_CONFLICT)
    {
      cgiStartHTML(cgiText(_("Set Allowed Users")));
      cgiShowIPPError(_("Unable to change printer:"));
    }
    else
    {
     /*
      * Redirect successful updates back to the printer page...
      */

      char	url[1024],		/* Printer/class URL */
		refresh[1024];		/* Refresh URL */


      cgiRewriteURL(uri, url, sizeof(url), NULL);
      cgiFormEncode(uri, url, sizeof(uri));
      snprintf(refresh, sizeof(refresh), "5;URL=/admin/?OP=redirect&URL=%s",
               uri);
      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(cgiText(_("Set Allowed Users")));

      cgiCopyTemplateLang(is_class ? "class-modified.tmpl" :
                                     "printer-modified.tmpl");
    }

    cgiEndHTML();
  }
}


/*
 * 'do_set_sharing()' - Set printer-is-shared value...
 */

static void
do_set_sharing(http_t *http)		/* I - HTTP connection */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  char		uri[HTTP_MAX_URI];	/* Printer URI */
  const char	*printer,		/* Printer name */
		*is_class,		/* Is a class? */
		*shared;		/* Sharing value */


  is_class = cgiGetVariable("IS_CLASS");
  printer  = cgiGetVariable("PRINTER_NAME");
  shared   = cgiGetVariable("SHARED");

  if (!printer || !shared)
  {
    cgiSetVariable("ERROR", cgiText(_("Missing form variable!")));
    cgiStartHTML(cgiText(_("Set Publishing")));
    cgiCopyTemplateLang("error.tmpl");
    cgiEndHTML();
    return;
  }

 /*
  * Build a CUPS-Add-Printer/CUPS-Add-Class request, which requires the
  * following attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    printer-is-shared
  */

  request = ippNewRequest(is_class ? CUPS_ADD_CLASS : CUPS_ADD_PRINTER);

  httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                   "localhost", 0, is_class ? "/classes/%s" : "/printers/%s",
		   printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddBoolean(request, IPP_TAG_OPERATION, "printer-is-shared", atoi(shared));

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(http, request, "/admin/")) != NULL)
  {
    cgiSetIPPVars(response, NULL, NULL, NULL, 0);

    ippDelete(response);
  }

  if (cupsLastError() > IPP_OK_CONFLICT)
  {
    cgiStartHTML(cgiText(_("Set Publishing")));
    cgiShowIPPError(_("Unable to change printer-is-shared attribute:"));
  }
  else
  {
   /*
    * Redirect successful updates back to the printer page...
    */

    char	url[1024],		/* Printer/class URL */
		refresh[1024];		/* Refresh URL */


    cgiRewriteURL(uri, url, sizeof(url), NULL);
    cgiFormEncode(uri, url, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "5;URL=/admin/?OP=redirect&URL=%s", uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(cgiText(_("Set Publishing")));
    cgiCopyTemplateLang(is_class ? "class-modified.tmpl" :
                                   "printer-modified.tmpl");
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
