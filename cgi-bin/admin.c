/*
 * "$Id$"
 *
 *   Administration CGI for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   main()                    - Main entry for CGI.
 *   do_add_rss_subscription() - Add a RSS subscription.
 *   do_am_class()             - Add or modify a class.
 *   do_am_printer()           - Add or modify a printer.
 *   do_cancel_subscription()  - Cancel a subscription.
 *   do_config_server()        - Configure server settings.
 *   do_delete_class()         - Delete a class.
 *   do_delete_printer()       - Delete a printer.
 *   do_export()               - Export printers to Samba.
 *   do_list_printers()        - List available printers.
 *   do_menu()                 - Show the main menu.
 *   do_printer_op()           - Do a printer operation.
 *   do_set_allowed_users()    - Set the allowed/denied users for a queue.
 *   do_set_options()          - Configure the default options for a queue.
 *   do_set_sharing()          - Set printer-is-shared value.
 *   get_option_value()        - Return the value of an option.
 *   get_points()              - Get a value in points.
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
#include <limits.h>


/*
 * Local functions...
 */

static void	do_add_rss_subscription(http_t *http);
static void	do_am_class(http_t *http, int modify);
static void	do_am_printer(http_t *http, int modify);
static void	do_cancel_subscription(http_t *http);
static void	do_set_options(http_t *http, int is_class);
static void	do_config_server(http_t *http);
static void	do_delete_class(http_t *http);
static void	do_delete_printer(http_t *http);
static void	do_export(http_t *http);
static void	do_list_printers(http_t *http);
static void	do_menu(http_t *http);
static void	do_printer_op(http_t *http,
		              ipp_op_t op, const char *title);
static void	do_set_allowed_users(http_t *http);
static void	do_set_sharing(http_t *http);
static char	*get_option_value(ppd_file_t *ppd, const char *name,
		                  char *buffer, size_t bufsize);
static double	get_points(double number, const char *uval);
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

  if (!cgiInitialize() || !cgiGetVariable("OP"))
  {
   /*
    * Nope, send the administration menu...
    */

    fputs("DEBUG: No form data, showing main menu...\n", stderr);

    do_menu(http);
  }
  else if ((op = cgiGetVariable("OP")) != NULL && cgiIsPOST())
  {
   /*
    * Do the operation...
    */

    fprintf(stderr, "DEBUG: op=\"%s\"...\n", op);

    if (!strcmp(op, "start-printer"))
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
    else if (!strcmp(op, "find-new-printers") ||
             !strcmp(op, "list-available-printers"))
      do_list_printers(http);
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
    else if (!strcmp(op, "set-class-options"))
      do_set_options(http, 1);
    else if (!strcmp(op, "set-printer-options"))
      do_set_options(http, 0);
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
      * Bad operation code - display an error...
      */

      cgiStartHTML(cgiText(_("Administration")));
      cgiCopyTemplateLang("error-op.tmpl");
      cgiEndHTML();
    }
  }
  else if (op && !strcmp(op, "redirect"))
  {
    const char *url;			/* Redirection URL... */
    char	prefix[1024];		/* URL prefix */


    if (getenv("HTTPS"))
      snprintf(prefix, sizeof(prefix), "https://%s:%s",
	       getenv("SERVER_NAME"), getenv("SERVER_PORT"));
    else
      snprintf(prefix, sizeof(prefix), "http://%s:%s",
	       getenv("SERVER_NAME"), getenv("SERVER_PORT"));

    fprintf(stderr, "DEBUG: redirecting with prefix %s!\n", prefix);

    if ((url = cgiGetVariable("URL")) != NULL)
      printf("Location: %s%s\n\n", prefix, url);
    else
      printf("Location: %s/admin\n\n", prefix);
  }
  else
  {
   /*
    * Form data but no operation code - display an error...
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
  * Make sure we have a username...
  */

  if ((user = getenv("REMOTE_USER")) == NULL)
  {
    puts("Status: 401\n");
    exit(0);
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

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, user);

  ippAddString(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI,
               "notify-recipient-uri", NULL, rss_uri);
  ippAddStrings(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD, "notify-events",
                num_events, NULL, events);
  ippAddInteger(request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                "notify-lease-duration", 0);

  ippDelete(cupsDoRequest(http, request, "/"));

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
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

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
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

  file = cgiGetFile();

  if (file)
  {
    fprintf(stderr, "DEBUG: file->tempfile=%s\n", file->tempfile);
    fprintf(stderr, "DEBUG: file->name=%s\n", file->name);
    fprintf(stderr, "DEBUG: file->filename=%s\n", file->filename);
    fprintf(stderr, "DEBUG: file->mimetype=%s\n", file->mimetype);
  }

  if ((name = cgiGetVariable("PRINTER_NAME")) != NULL)
  {
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
  }

  if ((var = cgiGetVariable("DEVICE_URI")) != NULL)
  {
    if ((uriptr = strrchr(var, '|')) != NULL)
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

      if (!cgiGetVariable("CURRENT_MAKE"))
        cgiSetVariable("CURRENT_MAKE", make);

      cgiSetVariable("PPD_MAKE", make);

      if (!cgiGetVariable("CURRENT_MAKE_AND_MODEL"))
        cgiSetVariable("CURRENT_MAKE_AND_MODEL", uriptr);

      if (!modify)
      {
        char	template[128],		/* Template name */
		*tptr;			/* Pointer into template name */

	cgiSetVariable("PRINTER_INFO", uriptr);

	for (tptr = template;
	     tptr < (template + sizeof(template) - 1) && *uriptr;
	     uriptr ++)
	  if (isalnum(*uriptr & 255) || *uriptr == '_' || *uriptr == '-' ||
	      *uriptr == '.')
	    *tptr++ = *uriptr;
	  else if ((*uriptr == ' ' || *uriptr == '/') && tptr[-1] != '_')
	    *tptr++ = '_';
	  else if (*uriptr == '?' || *uriptr == '(')
	    break;

        *tptr = '\0';

        cgiSetVariable("TEMPLATE_NAME", template);
      }
    }
  }

  if (!var)
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
	cgiSetVariable("CURRENT_DEVICE_URI", attr->values[0].string.text);
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
  else if (!name || !cgiGetVariable("PRINTER_LOCATION"))
  {
    cgiStartHTML(title);

    if (modify)
    {
     /*
      * Update the location and description of an existing printer...
      */

      if (oldinfo)
      {
        if ((attr = ippFindAttribute(oldinfo, "printer-info",
	                             IPP_TAG_TEXT)) != NULL)
          cgiSetVariable("PRINTER_INFO", attr->values[0].string.text);

        if ((attr = ippFindAttribute(oldinfo, "printer-location",
	                             IPP_TAG_TEXT)) != NULL)
          cgiSetVariable("PRINTER_LOCATION", attr->values[0].string.text);
      }

      cgiCopyTemplateLang("modify-printer.tmpl");
    }
    else
    {
     /*
      * Get the name, location, and description for a new printer...
      */

#ifdef __APPLE__
      if (!strncmp(var, "usb:", 4))
        cgiSetVariable("printer_is_shared", "1");
      else
#endif /* __APPLE__ */
        cgiSetVariable("printer_is_shared", "0");

      cgiCopyTemplateLang("add-printer.tmpl");
    }

    cgiEndHTML();

    if (oldinfo)
      ippDelete(oldinfo);

    return;
  }
  else if (!file && !cgiGetVariable("PPD_NAME"))
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

    if ((var = cgiGetVariable("CURRENT_MAKE")) == NULL)
      var = cgiGetVariable("PPD_MAKE");
    if (var)
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
      else if (!var || cgiGetVariable("SELECT_MAKE"))
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
    *    printer-is-shared
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
    {
      var = cgiGetVariable("PPD_NAME");
      if (strcmp(var, "__no_change__"))
	ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME, "ppd-name",
		     NULL, var);
    }

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

    var = cgiGetVariable("printer_is_shared");
    ippAddBoolean(request, IPP_TAG_PRINTER, "printer-is-shared",
                  var && (!strcmp(var, "1") || !strcmp(var, "on")));

    ippAddInteger(request, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                  IPP_PRINTER_IDLE);

   /*
    * Do the request and get back a response...
    */

    if (file)
      ippDelete(cupsDoFileRequest(http, request, "/admin/", file->tempfile));
    else
      ippDelete(cupsDoRequest(http, request, "/admin/"));

    if (cupsLastError() == IPP_NOT_AUTHORIZED)
    {
      puts("Status: 401\n");
      exit(0);
    }
    else if (cupsLastError() > IPP_OK_CONFLICT)
    {
      cgiStartHTML(title);
      cgiShowIPPError(modify ? _("Unable to modify printer:") :
                               _("Unable to add printer:"));
    }
    else if (modify)
    {
     /*
      * Redirect successful updates back to the printer page...
      */

      char	refresh[1024];		/* Refresh URL */


      cgiFormEncode(uri, name, sizeof(uri));

      snprintf(refresh, sizeof(refresh),
	       "5;/admin/?OP=redirect&URL=/printers/%s", uri);

      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(title);

      cgiCopyTemplateLang("printer-modified.tmpl");
    }
    else
    {
     /*
      * Set the printer options...
      */

      cgiSetVariable("OP", "set-printer-options");
      do_set_options(http, 0);
      return;
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
  * Require a username...
  */

  if ((user = getenv("REMOTE_USER")) == NULL)
  {
    puts("Status: 401\n");
    exit(0);
  }

 /*
  * Cancel the subscription...
  */

  request = ippNewRequest(IPP_CANCEL_SUBSCRIPTION);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, "ipp://localhost/");
  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                "notify-subscription-id", id);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, user);

  ippDelete(cupsDoRequest(http, request, "/"));

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
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
 * 'do_config_server()' - Configure server settings.
 */

static void
do_config_server(http_t *http)		/* I - HTTP connection */
{
  if (cgiGetVariable("CHANGESETTINGS"))
  {
   /*
    * Save basic setting changes...
    */

    int			num_settings;	/* Number of server settings */
    cups_option_t	*settings;	/* Server settings */
    int			advanced,	/* Advanced settings shown? */
			changed;	/* Have settings changed? */
    const char		*debug_logging,	/* DEBUG_LOGGING value */
			*remote_admin,	/* REMOTE_ADMIN value */
			*remote_any,	/* REMOTE_ANY value */
			*remote_printers,
					/* REMOTE_PRINTERS value */
			*share_printers,/* SHARE_PRINTERS value */
			*user_cancel_any,
					/* USER_CANCEL_ANY value */
			*browse_web_if,	/* BrowseWebIF value */
			*preserve_job_history,
					/* PreserveJobHistory value */
			*preserve_job_files,
					/* PreserveJobFiles value */
			*max_clients,	/* MaxClients value */
			*max_jobs,	/* MaxJobs value */
			*max_log_size;	/* MaxLogSize value */
    char		local_protocols[255],
					/* BrowseLocalProtocols */
			remote_protocols[255];
					/* BrowseRemoteProtocols */
#ifdef HAVE_GSSAPI
    char		default_auth_type[255];
					/* DefaultAuthType value */
    const char		*val;		/* Setting value */ 
#endif /* HAVE_GSSAPI */


   /*
    * Get the checkbox values from the form...
    */

    debug_logging        = cgiGetVariable("DEBUG_LOGGING") ? "1" : "0";
    remote_admin         = cgiGetVariable("REMOTE_ADMIN") ? "1" : "0";
    remote_any           = cgiGetVariable("REMOTE_ANY") ? "1" : "0";
    remote_printers      = cgiGetVariable("REMOTE_PRINTERS") ? "1" : "0";
    share_printers       = cgiGetVariable("SHARE_PRINTERS") ? "1" : "0";
    user_cancel_any      = cgiGetVariable("USER_CANCEL_ANY") ? "1" : "0";

    advanced = cgiGetVariable("ADVANCEDSETTINGS") != NULL;
    if (advanced)
    {
     /*
      * Get advanced settings...
      */

      browse_web_if        = cgiGetVariable("BROWSE_WEB_IF") ? "Yes" : "No";
      preserve_job_history = cgiGetVariable("PRESERVE_JOB_HISTORY") ? "Yes" : "No";
      preserve_job_files   = cgiGetVariable("PRESERVE_JOB_FILES") ? "Yes" : "No";
      max_clients          = cgiGetVariable("MAX_CLIENTS");
      max_jobs             = cgiGetVariable("MAX_JOBS");
      max_log_size         = cgiGetVariable("MAX_LOG_SIZE");

      if (!max_clients || atoi(max_clients) <= 0)
	max_clients = "100";

      if (!max_jobs || atoi(max_jobs) <= 0)
	max_jobs = "500";

      if (!max_log_size || atof(max_log_size) <= 0.0)
	max_log_size = "1m";

      if (cgiGetVariable("BROWSE_LOCAL_CUPS"))
	strcpy(local_protocols, "cups");
      else
	local_protocols[0] = '\0';

#ifdef HAVE_DNSSD
      if (cgiGetVariable("BROWSE_LOCAL_DNSSD"))
      {
	if (local_protocols[0])
	  strcat(local_protocols, " dnssd");
	else
	  strcat(local_protocols, "dnssd");
      }
#endif /* HAVE_DNSSD */

#ifdef HAVE_LDAP
      if (cgiGetVariable("BROWSE_LOCAL_LDAP"))
      {
	if (local_protocols[0])
	  strcat(local_protocols, " ldap");
	else
	  strcat(local_protocols, "ldap");
      }
#endif /* HAVE_LDAP */

#ifdef HAVE_LIBSLP
      if (cgiGetVariable("BROWSE_LOCAL_SLP"))
      {
	if (local_protocols[0])
	  strcat(local_protocols, " slp");
	else
	  strcat(local_protocols, "slp");
      }
#endif /* HAVE_SLP */
      
      if (cgiGetVariable("BROWSE_REMOTE_CUPS"))
	strcpy(remote_protocols, "cups");
      else
	remote_protocols[0] = '\0';

#ifdef HAVE_LDAP
      if (cgiGetVariable("BROWSE_REMOTE_LDAP"))
      {
	if (remote_protocols[0])
	  strcat(remote_protocols, " ldap");
	else
	  strcat(remote_protocols, "ldap");
      }
#endif /* HAVE_LDAP */

#ifdef HAVE_LIBSLP
      if (cgiGetVariable("BROWSE_REMOTE_SLP"))
      {
	if (remote_protocols[0])
	  strcat(remote_protocols, " slp");
	else
	  strcat(remote_protocols, "slp");
      }
#endif /* HAVE_SLP */
    }

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

#ifdef HAVE_GSSAPI
   /*
    * Get authentication settings...
    */

    if (cgiGetVariable("KERBEROS"))
      strlcpy(default_auth_type, "Negotiate", sizeof(default_auth_type));
    else
    {
      val = cupsGetOption("DefaultAuthType", num_settings, settings);

      if (!val || !strcasecmp(val, "Negotiate"))
        strlcpy(default_auth_type, "Basic", sizeof(default_auth_type));
      else
        strlcpy(default_auth_type, val, sizeof(default_auth_type));
    }

    fprintf(stderr, "DEBUG: DefaultAuthType %s\n", default_auth_type);
#endif /* HAVE_GSSAPI */

   /*
    * See if the settings have changed...
    */

    changed = strcmp(debug_logging, cupsGetOption(CUPS_SERVER_DEBUG_LOGGING,
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
						    num_settings, settings));

    if (advanced && !changed)
      changed = cupsGetOption("BrowseLocalProtocols", num_settings, settings) ||
	        strcasecmp(local_protocols,
		           CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS) ||
		cupsGetOption("BrowseRemoteProtocols", num_settings,
		              settings) ||
		strcasecmp(remote_protocols,
		           CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS) ||
		cupsGetOption("BrowseWebIF", num_settings, settings) ||
		strcasecmp(browse_web_if, "No") ||
		cupsGetOption("PreserveJobHistory", num_settings, settings) ||
		strcasecmp(preserve_job_history, "Yes") ||
		cupsGetOption("PreserveJobFiles", num_settings, settings) ||
		strcasecmp(preserve_job_files, "No") ||
		cupsGetOption("MaxClients", num_settings, settings) ||
		strcasecmp(max_clients, "100") ||
		cupsGetOption("MaxJobs", num_settings, settings) ||
		strcasecmp(max_jobs, "500") ||
		cupsGetOption("MaxLogSize", num_settings, settings) ||
		strcasecmp(max_log_size, "1m");

    if (changed)
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

      if (advanced)
      {
       /*
        * Add advanced settings...
	*/

	if (cupsGetOption("BrowseLocalProtocols", num_settings, settings) ||
	    strcasecmp(local_protocols, CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS))
	  num_settings = cupsAddOption("BrowseLocalProtocols", local_protocols,
				       num_settings, &settings);
	if (cupsGetOption("BrowseRemoteProtocols", num_settings, settings) ||
	    strcasecmp(remote_protocols, CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS))
	  num_settings = cupsAddOption("BrowseRemoteProtocols", remote_protocols,
				       num_settings, &settings);
	if (cupsGetOption("BrowseWebIF", num_settings, settings) ||
	    strcasecmp(browse_web_if, "No"))
	  num_settings = cupsAddOption("BrowseWebIF", browse_web_if,
				       num_settings, &settings);
	if (cupsGetOption("PreserveJobHistory", num_settings, settings) ||
	    strcasecmp(preserve_job_history, "Yes"))
	  num_settings = cupsAddOption("PreserveJobHistory",
	                               preserve_job_history, num_settings,
				       &settings);
	if (cupsGetOption("PreserveJobFiles", num_settings, settings) ||
	    strcasecmp(preserve_job_files, "No"))
	  num_settings = cupsAddOption("PreserveJobFiles", preserve_job_files,
	                               num_settings, &settings);
        if (cupsGetOption("MaxClients", num_settings, settings) ||
	    strcasecmp(max_clients, "100"))
	  num_settings = cupsAddOption("MaxClients", max_clients, num_settings,
	                               &settings);
        if (cupsGetOption("MaxJobs", num_settings, settings) ||
	    strcasecmp(max_jobs, "500"))
	  num_settings = cupsAddOption("MaxJobs", max_jobs, num_settings,
	                               &settings);
        if (cupsGetOption("MaxLogSize", num_settings, settings) ||
	    strcasecmp(max_log_size, "1m"))
	  num_settings = cupsAddOption("MaxLogSize", max_log_size, num_settings,
	                               &settings);
      }

      if (!cupsAdminSetServerSettings(http, num_settings, settings))
      {
        if (cupsLastError() == IPP_NOT_AUTHORIZED)
	{
	  puts("Status: 401\n");
	  exit(0);
	}

	cgiStartHTML(cgiText(_("Change Settings")));
	cgiSetVariable("MESSAGE",
                       cgiText(_("Unable to change server settings:")));
	cgiSetVariable("ERROR", cupsLastErrorString());
	cgiCopyTemplateLang("error.tmpl");
      }
      else
      {
        if (advanced)
	  cgiSetVariable("refresh_page", "5;URL=/admin/?OP=redirect&URL=/admin/?ADVANCEDSETTINGS=YES");
        else
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
  else if (cgiGetVariable("SAVECHANGES") && cgiGetVariable("CUPSDCONF"))
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

    if (status == HTTP_UNAUTHORIZED)
    {
      puts("Status: 401\n");
      unlink(tempfile);
      exit(0);
    }
    else if (status != HTTP_CREATED)
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
    char	*buffer,		/* Buffer for entire file */
		*bufptr,		/* Pointer into buffer */
		*bufend;		/* End of buffer */
    int		ch;			/* Character from file */
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

    if ((buffer = calloc(1, info.st_size + 1)) != NULL)
    {
      cupsFileRead(cupsd, buffer, info.st_size);
      cgiSetVariable("CUPSDCONF", buffer);
      free(buffer);
    }

    cupsFileClose(cupsd);

   /*
    * Then get the default cupsd.conf file and put that into a string as
    * well...
    */

    strlcat(filename, ".default", sizeof(filename));

    if (!stat(filename, &info) && info.st_size < (1024 * 1024) &&
        (cupsd = cupsFileOpen(filename, "r")) != NULL)
    {
      if ((buffer = calloc(1, 2 * info.st_size + 1)) != NULL)
      {
	bufend = buffer + 2 * info.st_size - 1;

	for (bufptr = buffer;
	     bufptr < bufend && (ch = cupsFileGetChar(cupsd)) != EOF;)
	{
	  if (ch == '\\' || ch == '\"')
	  {
	    *bufptr++ = '\\';
	    *bufptr++ = ch;
	  }
	  else if (ch == '\n')
	  {
	    *bufptr++ = '\\';
	    *bufptr++ = 'n';
	  }
	  else if (ch == '\t')
	  {
	    *bufptr++ = '\\';
	    *bufptr++ = 't';
	  }
	  else if (ch >= ' ')
	    *bufptr++ = ch;
	}

	*bufptr = '\0';

	cgiSetVariable("CUPSDCONF_DEFAULT", buffer);
	free(buffer);
      }

      cupsFileClose(cupsd);
    }

   /*
    * Show the current config file...
    */

    cgiStartHTML(cgiText(_("Edit Configuration File")));

    cgiCopyTemplateLang("edit-config.tmpl");

    cgiEndHTML();
  }
}


/*
 * 'do_delete_class()' - Delete a class.
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

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() <= IPP_OK_CONFLICT)
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
 * 'do_delete_printer()' - Delete a printer.
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

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() <= IPP_OK_CONFLICT)
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
 * 'do_export()' - Export printers to Samba.
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
 * 'do_list_printers()' - List available printers.
 */

static void
do_list_printers(http_t *http)		/* I - HTTP connection */
{
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP attribute */


  cgiStartHTML(cgiText(_("List Available Printers")));
  fflush(stdout);

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

            char	option[1024],	/* Form variables for this device */
			*option_ptr;	/* Pointer into string */
	    const char	*ptr;		/* Pointer into device string */


           /*
	    * Format the printer name variable for this device...
	    *
	    * We use the device-info string first, then device-uri,
	    * and finally device-make-and-model to come up with a
	    * suitable name.
	    */

            if (strncasecmp(device_info, "unknown", 7))
	      ptr = device_info;
            else if ((ptr = strstr(device_uri, "://")) != NULL)
	      ptr += 3;
	    else
	      ptr = device_make_and_model;

	    for (option_ptr = option;
	         option_ptr < (option + sizeof(option) - 1) && *ptr;
		 ptr ++)
	      if (isalnum(*ptr & 255) || *ptr == '_' || *ptr == '-' ||
	          *ptr == '.')
	        *option_ptr++ = *ptr;
	      else if ((*ptr == ' ' || *ptr == '/') && option_ptr[-1] != '_')
	        *option_ptr++ = '_';
	      else if (*ptr == '?' || *ptr == '(')
	        break;

            *option_ptr = '\0';

            cgiSetArray("TEMPLATE_NAME", i, option);

           /*
	    * Finally, set the form variables for this printer...
	    */

	    cgiSetArray("device_info", i, device_info);
	    cgiSetArray("device_make_and_model", i, device_make_and_model);
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
  * Finally, show the printer list...
  */

  cgiCopyTemplateLang("list-available-printers.tmpl");

  cgiEndHTML();
}


/*
 * 'do_menu()' - Show the main menu.
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

#ifdef HAVE_DNSSD
  cgiSetVariable("HAVE_DNSSD", "1");
#endif /* HAVE_DNSSD */

#ifdef HAVE_LDAP
  cgiSetVariable("HAVE_LDAP", "1");
#endif /* HAVE_LDAP */

#ifdef HAVE_LIBSLP
  cgiSetVariable("HAVE_LIBSLP", "1");
#endif /* HAVE_LIBSLP */

  if ((val = cupsGetOption("BrowseRemoteProtocols", num_settings,
                           settings)) == NULL)
    if ((val = cupsGetOption("BrowseProtocols", num_settings,
                           settings)) == NULL)
      val = CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS;

  if (strstr(val, "cups") || strstr(val, "CUPS"))
    cgiSetVariable("BROWSE_REMOTE_CUPS", "CHECKED");

  if (strstr(val, "ldap") || strstr(val, "LDAP"))
    cgiSetVariable("BROWSE_REMOTE_LDAP", "CHECKED");

  if (strstr(val, "slp") || strstr(val, "SLP"))
    cgiSetVariable("BROWSE_REMOTE_SLP", "CHECKED");

  if ((val = cupsGetOption("BrowseLocalProtocols", num_settings,
                           settings)) == NULL)
    if ((val = cupsGetOption("BrowseProtocols", num_settings,
                           settings)) == NULL)
      val = CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS;

  if (strstr(val, "cups") || strstr(val, "CUPS"))
    cgiSetVariable("BROWSE_LOCAL_CUPS", "CHECKED");

  if (strstr(val, "dnssd") || strstr(val, "DNSSD") ||
      strstr(val, "dns-sd") || strstr(val, "DNS-SD") ||
      strstr(val, "bonjour") || strstr(val, "BONJOUR"))
    cgiSetVariable("BROWSE_LOCAL_DNSSD", "CHECKED");

  if (strstr(val, "ldap") || strstr(val, "LDAP"))
    cgiSetVariable("BROWSE_LOCAL_LDAP", "CHECKED");

  if (strstr(val, "slp") || strstr(val, "SLP"))
    cgiSetVariable("BROWSE_LOCAL_SLP", "CHECKED");

  if ((val = cupsGetOption("BrowseWebIF", num_settings,
                           settings)) == NULL)
    val = "No";

  if (!strcasecmp(val, "yes") || !strcasecmp(val, "on") ||
      !strcasecmp(val, "true"))
    cgiSetVariable("BROWSE_WEB_IF", "CHECKED");

  if ((val = cupsGetOption("PreserveJobHistory", num_settings,
                           settings)) == NULL)
    val = "Yes";

  if (!strcasecmp(val, "yes") || !strcasecmp(val, "on") ||
      !strcasecmp(val, "true"))
  {
    cgiSetVariable("PRESERVE_JOB_HISTORY", "CHECKED");

    if ((val = cupsGetOption("PreserveJobFiles", num_settings,
			     settings)) == NULL)
      val = "No";

    if (!strcasecmp(val, "yes") || !strcasecmp(val, "on") ||
	!strcasecmp(val, "true"))
      cgiSetVariable("PRESERVE_JOB_FILES", "CHECKED");
  }

  if ((val = cupsGetOption("MaxClients", num_settings, settings)) == NULL)
    val = "100";

  cgiSetVariable("MAX_CLIENTS", val);

  if ((val = cupsGetOption("MaxJobs", num_settings, settings)) == NULL)
    val = "500";

  cgiSetVariable("MAX_JOBS", val);

  if ((val = cupsGetOption("MaxLogSize", num_settings, settings)) == NULL)
    val = "1m";

  cgiSetVariable("MAX_LOG_SIZE", val);

  cupsFreeOptions(num_settings, settings);

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

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
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

    if (cupsLastError() == IPP_NOT_AUTHORIZED)
    {
      puts("Status: 401\n");
      exit(0);
    }
    else if (cupsLastError() > IPP_OK_CONFLICT)
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

      if (!*ptr)
        break;

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

        if (!*ptr)
	  break;

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

    if (cupsLastError() == IPP_NOT_AUTHORIZED)
    {
      puts("Status: 401\n");
      exit(0);
    }
    else if (cupsLastError() > IPP_OK_CONFLICT)
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
 * 'do_set_options()' - Configure the default options for a queue.
 */

static void
do_set_options(http_t *http,		/* I - HTTP connection */
               int    is_class)		/* I - Set options for class? */
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
  char		line[1024],		/* Line from PPD file */
		value[1024],		/* Option value */
		keyword[1024],		/* Keyword from Default line */
		*keyptr;		/* Pointer into keyword... */
  ppd_file_t	*ppd;			/* PPD file */
  ppd_group_t	*group;			/* Option group */
  ppd_option_t	*option;		/* Option */
  ppd_coption_t	*coption;		/* Custom option */
  ppd_cparam_t	*cparam;		/* Custom parameter */
  ppd_attr_t	*ppdattr;		/* PPD attribute */
  const char	*title;			/* Page title */


  title = cgiText(is_class ? _("Set Class Options") : _("Set Printer Options"));

  fprintf(stderr, "DEBUG: do_set_options(http=%p, is_class=%d)\n", http,
          is_class);

 /*
  * Get the printer name...
  */

  if ((printer = cgiGetVariable("PRINTER_NAME")) != NULL)
    httpAssembleURIf(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                     "localhost", 0, is_class ? "/classes/%s" : "/printers/%s",
		     printer);
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
  * If the user clicks on the Auto-Configure button, send an AutoConfigure
  * command file to the printer...
  */

  if (cgiGetVariable("AUTOCONFIGURE"))
  {
    int			job_id;		/* Command file job */
    char		refresh[1024];	/* Refresh URL */
    http_status_t	status;		/* Document status */
    static const char	*autoconfigure =/* Command file */
			"#CUPS-COMMAND\n"
			"AutoConfigure\n";


    if ((job_id = cupsCreateJob(CUPS_HTTP_DEFAULT, printer, "Auto-Configure",
                                0, NULL)) < 1)
    {
      cgiSetVariable("ERROR", cgiText(_("Unable to send auto-configure command "
                                        "to printer driver!")));
      cgiStartHTML(title);
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();
      return;
    }

    status = cupsStartDocument(CUPS_HTTP_DEFAULT, printer, job_id,
                               "AutoConfigure.command", CUPS_FORMAT_COMMAND, 1);
    if (status == HTTP_CONTINUE)
      status = cupsWriteRequestData(CUPS_HTTP_DEFAULT, autoconfigure,
                                    strlen(autoconfigure));
    if (status == HTTP_CONTINUE)
      cupsFinishDocument(CUPS_HTTP_DEFAULT, printer);

    if (cupsLastError() >= IPP_REDIRECTION_OTHER_SITE)
    {
      cgiSetVariable("ERROR", cupsLastErrorString());
      cgiStartHTML(title);
      cgiCopyTemplateLang("error.tmpl");
      cgiEndHTML();

      cupsCancelJob(printer, job_id);
      return;
    }

   /*
    * Redirect successful updates back to the printer page...
    */

    cgiFormEncode(uri, printer, sizeof(uri));
    snprintf(refresh, sizeof(refresh), "5;URL=/admin/?OP=redirect&URL=/%s/%s",
	     is_class ? "classes" : "printers", uri);
    cgiSetVariable("refresh_page", refresh);

    cgiStartHTML(title);

    cgiCopyTemplateLang("printer-configured.tmpl");
    cgiEndHTML();
    return;
  }

 /*
  * Get the PPD file...
  */

  if (is_class)
    filename = NULL;
  else
    filename = cupsGetPPD2(http, printer);

  if (filename)
  {
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
  }
  else
  {
    fputs("DEBUG: No PPD file\n", stderr);
    ppd = NULL;
  }

  if (cgiGetVariable("job_sheets_start") != NULL ||
      cgiGetVariable("job_sheets_end") != NULL)
    have_options = 1;
  else
    have_options = 0;

  if (ppd)
  {
    ppdMarkDefaults(ppd);

    for (option = ppdFirstOption(ppd);
         option;
	 option = ppdNextOption(ppd))
      if ((var = cgiGetVariable(option->keyword)) != NULL)
      {
	have_options = 1;
	ppdMarkOption(ppd, option->keyword, var);
      }
  }

  if (!have_options || ppdConflicts(ppd))
  {
   /*
    * Show the options to the user...
    */

    fputs("DEBUG: Showing options...\n", stderr);

    if (ppd)
    {
      if (ppd->num_filters == 0 ||
          ((ppdattr = ppdFindAttr(ppd, "cupsCommands", NULL)) != NULL &&
           ppdattr->value && strstr(ppdattr->value, "AutoConfigure")))
        cgiSetVariable("HAVE_AUTOCONFIGURE", "YES");
      else 
      {
        for (i = 0; i < ppd->num_filters; i ++)
	  if (!strncmp(ppd->filters[i], "application/vnd.cups-postscript", 31))
	  {
	    cgiSetVariable("HAVE_AUTOCONFIGURE", "YES");
	    break;
	  }
      }
    }

    cgiStartHTML(cgiText(_("Set Printer Options")));
    cgiCopyTemplateLang("set-printer-options-header.tmpl");

    if (ppd)
    {
      ppdLocalize(ppd);

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
	    cgiSetArray("CHOICES", m, option->choices[k].choice);
	    cgiSetArray("TEXT", m, option->choices[k].text);

	    m ++;

	    if (option->choices[k].marked)
	      cgiSetVariable("DEFCHOICE", option->choices[k].choice);
	  }

	  cgiSetSize("PARAMS", 0);
	  cgiSetSize("PARAMTEXT", 0);
	  cgiSetSize("PARAMVALUE", 0);
	  cgiSetSize("INPUTTYPE", 0);

	  if ((coption = ppdFindCustomOption(ppd, option->keyword)))
	  {
            const char *units = NULL;	/* Units value, if any */


	    cgiSetVariable("ISCUSTOM", "1");

	    for (cparam = ppdFirstCustomParam(coption), m = 0;
		 cparam;
		 cparam = ppdNextCustomParam(coption), m ++)
	    {
	      if (!strcasecmp(option->keyword, "PageSize") &&
	          strcasecmp(cparam->name, "Width") &&
		  strcasecmp(cparam->name, "Height"))
              {
	        m --;
		continue;
              }

	      cgiSetArray("PARAMS", m, cparam->name);
	      cgiSetArray("PARAMTEXT", m, cparam->text);
	      cgiSetArray("INPUTTYPE", m, "text");

	      switch (cparam->type)
	      {
		case PPD_CUSTOM_POINTS :
		    if (!strncasecmp(option->defchoice, "Custom.", 7))
		    {
		      units = option->defchoice + strlen(option->defchoice) - 2;

		      if (strcmp(units, "mm") && strcmp(units, "cm") &&
		          strcmp(units, "in") && strcmp(units, "ft"))
		      {
		        if (units[1] == 'm')
			  units ++;
			else
			  units = "pt";
		      }
		    }
		    else
		      units = "pt";

                    if (!strcmp(units, "mm"))
		      snprintf(value, sizeof(value), "%g",
		               cparam->current.custom_points / 72.0 * 25.4);
                    else if (!strcmp(units, "cm"))
		      snprintf(value, sizeof(value), "%g",
		               cparam->current.custom_points / 72.0 * 2.54);
                    else if (!strcmp(units, "in"))
		      snprintf(value, sizeof(value), "%g",
		               cparam->current.custom_points / 72.0);
                    else if (!strcmp(units, "ft"))
		      snprintf(value, sizeof(value), "%g",
		               cparam->current.custom_points / 72.0 / 12.0);
                    else if (!strcmp(units, "m"))
		      snprintf(value, sizeof(value), "%g",
		               cparam->current.custom_points / 72.0 * 0.0254);
                    else
		      snprintf(value, sizeof(value), "%g",
		               cparam->current.custom_points);
		    cgiSetArray("PARAMVALUE", m, value);
		    break;

		case PPD_CUSTOM_CURVE :
		case PPD_CUSTOM_INVCURVE :
		case PPD_CUSTOM_REAL :
		    snprintf(value, sizeof(value), "%g",
		             cparam->current.custom_real);
		    cgiSetArray("PARAMVALUE", m, value);
		    break;

		case PPD_CUSTOM_INT:
		    snprintf(value, sizeof(value), "%d",
		             cparam->current.custom_int);
		    cgiSetArray("PARAMVALUE", m, value);
		    break;

		case PPD_CUSTOM_PASSCODE:
		case PPD_CUSTOM_PASSWORD:
		    if (cparam->current.custom_password)
		      cgiSetArray("PARAMVALUE", m,
		                  cparam->current.custom_password);
		    else
		      cgiSetArray("PARAMVALUE", m, "");
		    cgiSetArray("INPUTTYPE", m, "password");
		    break;

		case PPD_CUSTOM_STRING:
		    if (cparam->current.custom_string)
		      cgiSetArray("PARAMVALUE", m,
		                  cparam->current.custom_string);
		    else
		      cgiSetArray("PARAMVALUE", m, "");
		    break;
	      }
	    }

            if (units)
	    {
	      cgiSetArray("PARAMS", m, "Units");
	      cgiSetArray("PARAMTEXT", m, cgiText(_("Units")));
	      cgiSetArray("PARAMVALUE", m, units);
	    }
	  }
	  else
	    cgiSetVariable("ISCUSTOM", "0");

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
        cgiSetVariable("DEFCHOICE", attr != NULL ?
	                            attr->values[0].string.text : "");

	cgiCopyTemplateLang("option-pickone.tmpl");

        cgiSetVariable("KEYWORD", "job_sheets_end");
	cgiSetVariable("KEYTEXT", cgiText(_("Ending Banner")));
        cgiSetVariable("DEFCHOICE", attr != NULL && attr->num_values > 1 ?
	                            attr->values[1].string.text : "");

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

    if (ppd && ppd->protocols && strstr(ppd->protocols, "BCP"))
    {
      ppdattr = ppdFindAttr(ppd, "cupsProtocol", NULL);

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
      cgiSetVariable("DEFCHOICE", ppdattr ? ppdattr->value : "None");

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

    if (filename)
    {
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
	    var = get_option_value(ppd, "PageSize", value, sizeof(value));
	  else
	    var = get_option_value(ppd, keyword, value, sizeof(value));

	  if (!var)
	    cupsFilePrintf(out, "%s\n", line);
	  else
	    cupsFilePrintf(out, "*Default%s: %s\n", keyword, var);
	}
      }

     /*
      * TODO: We need to set the port-monitor attribute!
      */

      if ((var = cgiGetVariable("protocol")) != NULL)
	cupsFilePrintf(out, "*cupsProtocol: %s\n", var);

      cupsFileClose(in);
      cupsFileClose(out);
    }
    else
    {
     /*
      * Make sure temporary filename is cleared when there is no PPD...
      */

      tempfile[0] = '\0';
    }

   /*
    * Build a CUPS_ADD_MODIFY_CLASS/PRINTER request, which requires the
    * following attributes:
    *
    *    attributes-charset
    *    attributes-natural-language
    *    printer-uri
    *    job-sheets-default
    *    printer-error-policy
    *    printer-op-policy
    *    [ppd file]
    */

    request = ippNewRequest(is_class ? CUPS_ADD_MODIFY_CLASS :
                                       CUPS_ADD_MODIFY_PRINTER);

    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                 NULL, uri);

    attr = ippAddStrings(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
                         "job-sheets-default", 2, NULL, NULL);
    attr->values[0].string.text = _cupsStrAlloc(cgiGetVariable("job_sheets_start"));
    attr->values[1].string.text = _cupsStrAlloc(cgiGetVariable("job_sheets_end"));

    if ((var = cgiGetVariable("printer_error_policy")) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
		   "printer-error-policy", NULL, var);

    if ((var = cgiGetVariable("printer_op_policy")) != NULL)
      ippAddString(request, IPP_TAG_PRINTER, IPP_TAG_NAME,
		   "printer-op-policy", NULL, var);

   /*
    * Do the request and get back a response...
    */

    if (filename)
      ippDelete(cupsDoFileRequest(http, request, "/admin/", tempfile));
    else
      ippDelete(cupsDoRequest(http, request, "/admin/"));

    if (cupsLastError() == IPP_NOT_AUTHORIZED)
    {
      puts("Status: 401\n");
      exit(0);
    }
    else if (cupsLastError() > IPP_OK_CONFLICT)
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
      snprintf(refresh, sizeof(refresh), "5;URL=/admin/?OP=redirect&URL=/%s/%s",
	       is_class ? "classes" : "printers", uri);
      cgiSetVariable("refresh_page", refresh);

      cgiStartHTML(title);

      cgiCopyTemplateLang("printer-configured.tmpl");
    }

    cgiEndHTML();

    if (filename)
      unlink(tempfile);
  }

  if (filename)
    unlink(filename);
}


/*
 * 'do_set_sharing()' - Set printer-is-shared value.
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

  if (cupsLastError() == IPP_NOT_AUTHORIZED)
  {
    puts("Status: 401\n");
    exit(0);
  }
  else if (cupsLastError() > IPP_OK_CONFLICT)
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
 * 'get_option_value()' - Return the value of an option.
 *
 * This function also handles generation of custom option values.
 */

static char *				/* O - Value string or NULL on error */
get_option_value(
    ppd_file_t    *ppd,			/* I - PPD file */
    const char    *name,		/* I - Option name */
    char          *buffer,		/* I - String buffer */
    size_t        bufsize)		/* I - Size of buffer */
{
  char		*bufptr,		/* Pointer into buffer */
		*bufend;		/* End of buffer */
  ppd_coption_t *coption;		/* Custom option */
  ppd_cparam_t	*cparam;		/* Current custom parameter */
  char		keyword[256];		/* Parameter name */
  const char	*val,			/* Parameter value */
		*uval;			/* Units value */
  long		integer;		/* Integer value */
  double	number,			/* Number value */
		number_points;		/* Number in points */


 /*
  * See if we have a custom option choice...
  */

  if ((val = cgiGetVariable(name)) == NULL)
  {
   /*
    * Option not found!
    */

    return (NULL);
  }
  else if (strcasecmp(val, "Custom") ||
           (coption = ppdFindCustomOption(ppd, name)) == NULL)
  {
   /*
    * Not a custom choice...
    */

    strlcpy(buffer, val, bufsize);
    return (buffer);
  }

 /*
  * OK, we have a custom option choice, format it...
  */

  *buffer = '\0';

  if (!strcmp(coption->keyword, "PageSize"))
  {
    const char	*lval;			/* Length string value */
    double	width,			/* Width value */
		width_points,		/* Width in points */
		length,			/* Length value */
		length_points;		/* Length in points */


    val  = cgiGetVariable("PageSize.Width");
    lval = cgiGetVariable("PageSize.Height");
    uval = cgiGetVariable("PageSize.Units");

    if (!val || !lval || !uval ||
        (width = strtod(val, NULL)) == 0.0 ||
        (length = strtod(lval, NULL)) == 0.0 ||
        (strcmp(uval, "pt") && strcmp(uval, "in") && strcmp(uval, "ft") &&
	 strcmp(uval, "cm") && strcmp(uval, "mm") && strcmp(uval, "m")))
      return (NULL);

    width_points  = get_points(width, uval);
    length_points = get_points(length, uval);

    if (width_points < ppd->custom_min[0] ||
        width_points > ppd->custom_max[0] ||
        length_points < ppd->custom_min[1] ||
	length_points > ppd->custom_max[1])
      return (NULL);

    snprintf(buffer, bufsize, "Custom.%gx%g%s", width, length, uval);
  }
  else if (cupsArrayCount(coption->params) == 1) 
  {
    cparam = ppdFirstCustomParam(coption);
    snprintf(keyword, sizeof(keyword), "%s.%s", coption->keyword, cparam->name);

    if ((val = cgiGetVariable(keyword)) == NULL)
      return (NULL);

    switch (cparam->type)
    {
      case PPD_CUSTOM_CURVE :
      case PPD_CUSTOM_INVCURVE :
      case PPD_CUSTOM_REAL :
	  if ((number = strtod(val, NULL)) == 0.0 ||
	      number < cparam->minimum.custom_real ||
	      number > cparam->maximum.custom_real)
	    return (NULL);

          snprintf(buffer, bufsize, "Custom.%g", number);
          break;

      case PPD_CUSTOM_INT :
          if (!*val || (integer = strtol(val, NULL, 10)) == LONG_MIN ||
	      integer == LONG_MAX ||
	      integer < cparam->minimum.custom_int ||
	      integer > cparam->maximum.custom_int)
            return (NULL);

          snprintf(buffer, bufsize, "Custom.%ld", integer);
          break;

      case PPD_CUSTOM_POINTS :
          snprintf(keyword, sizeof(keyword), "%s.Units", coption->keyword);

	  if ((number = strtod(val, NULL)) == 0.0 ||
	      (uval = cgiGetVariable(keyword)) == NULL ||
	      (strcmp(uval, "pt") && strcmp(uval, "in") && strcmp(uval, "ft") &&
	       strcmp(uval, "cm") && strcmp(uval, "mm") && strcmp(uval, "m")))
	    return (NULL);

	  number_points = get_points(number, uval);
	  if (number_points < cparam->minimum.custom_points ||
	      number_points > cparam->maximum.custom_points)
	    return (NULL);

	  snprintf(buffer, bufsize, "Custom.%g%s", number, uval);
          break;

      case PPD_CUSTOM_PASSCODE :
          for (uval = val; *uval; uval ++)
	    if (!isdigit(*uval & 255))
	      return (NULL);

      case PPD_CUSTOM_PASSWORD :
      case PPD_CUSTOM_STRING :
          integer = (long)strlen(val);
	  if (integer < cparam->minimum.custom_string ||
	      integer > cparam->maximum.custom_string)
	    return (NULL);

          snprintf(buffer, bufsize, "Custom.%s", val);
	  break;
    }
  }
  else
  {
    const char *prefix = "{";		/* Prefix string */


    bufptr = buffer;
    bufend = buffer + bufsize;

    for (cparam = ppdFirstCustomParam(coption);
	 cparam;
	 cparam = ppdNextCustomParam(coption))
    {
      snprintf(keyword, sizeof(keyword), "%s.%s", coption->keyword,
               cparam->name);

      if ((val = cgiGetVariable(keyword)) == NULL)
	return (NULL);

      snprintf(bufptr, bufend - bufptr, "%s%s=", prefix, cparam->name);
      bufptr += strlen(bufptr);
      prefix = " ";

      switch (cparam->type)
      {
	case PPD_CUSTOM_CURVE :
	case PPD_CUSTOM_INVCURVE :
	case PPD_CUSTOM_REAL :
	    if ((number = strtod(val, NULL)) == 0.0 ||
		number < cparam->minimum.custom_real ||
		number > cparam->maximum.custom_real)
	      return (NULL);

	    snprintf(bufptr, bufend - bufptr, "%g", number);
	    break;

	case PPD_CUSTOM_INT :
	    if (!*val || (integer = strtol(val, NULL, 10)) == LONG_MIN ||
		integer == LONG_MAX ||
		integer < cparam->minimum.custom_int ||
		integer > cparam->maximum.custom_int)
	      return (NULL);

	    snprintf(bufptr, bufend - bufptr, "%ld", integer);
	    break;

	case PPD_CUSTOM_POINTS :
	    snprintf(keyword, sizeof(keyword), "%s.Units", coption->keyword);

	    if ((number = strtod(val, NULL)) == 0.0 ||
		(uval = cgiGetVariable(keyword)) == NULL ||
		(strcmp(uval, "pt") && strcmp(uval, "in") &&
		 strcmp(uval, "ft") && strcmp(uval, "cm") &&
		 strcmp(uval, "mm") && strcmp(uval, "m")))
	      return (NULL);

	    number_points = get_points(number, uval);
	    if (number_points < cparam->minimum.custom_points ||
		number_points > cparam->maximum.custom_points)
	      return (NULL);

	    snprintf(bufptr, bufend - bufptr, "%g%s", number, uval);
	    break;

	case PPD_CUSTOM_PASSCODE :
	    for (uval = val; *uval; uval ++)
	      if (!isdigit(*uval & 255))
		return (NULL);

	case PPD_CUSTOM_PASSWORD :
	case PPD_CUSTOM_STRING :
	    integer = (long)strlen(val);
	    if (integer < cparam->minimum.custom_string ||
		integer > cparam->maximum.custom_string)
	      return (NULL);

	    if ((bufptr + 2) > bufend)
	      return (NULL);

	    bufend --;
	    *bufptr++ = '\"';

	    while (*val && bufptr < bufend)
	    {
	      if (*val == '\\' || *val == '\"')
	      {
		if ((bufptr + 1) >= bufend)
		  return (NULL);

		*bufptr++ = '\\';
	      }

	      *bufptr++ = *val++;
	    }

	    if (bufptr >= bufend)
	      return (NULL);

	    *bufptr++ = '\"';
	    *bufptr   = '\0';
	    bufend ++;
	    break;
      }

      bufptr += strlen(bufptr);
    }

    if (bufptr == buffer || (bufend - bufptr) < 2)
      return (NULL);

    strcpy(bufptr, "}");
  }

  return (buffer);
}


/*
 * 'get_points()' - Get a value in points.
 */

static double				/* O - Number in points */
get_points(double     number,		/* I - Original number */
           const char *uval)		/* I - Units */
{
  if (!strcmp(uval, "mm"))		/* Millimeters */
    return (number * 72.0 / 25.4);
  else if (!strcmp(uval, "cm"))		/* Centimeters */
    return (number * 72.0 / 2.54);
  else if (!strcmp(uval, "in"))		/* Inches */
    return (number * 72.0);
  else if (!strcmp(uval, "ft"))		/* Feet */
    return (number * 72.0 * 12.0);
  else if (!strcmp(uval, "m"))		/* Meters */
    return (number * 72.0 / 0.0254);
  else					/* Points */
    return (number);
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
