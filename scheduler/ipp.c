/*
 * "$Id: ipp.c,v 1.127.2.70 2003/05/13 14:50:54 mike Exp $"
 *
 *   IPP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *   ProcessIPPRequest()         - Process an incoming IPP request...
 *   accept_jobs()               - Accept print jobs to a printer.
 *   add_class()                 - Add a class to the system.
 *   add_file()                  - Add a file to a job.
 *   add_job_state_reasons()     - Add the "job-state-reasons" attribute based
 *                                 upon the job and printer state...
 *   add_printer()               - Add a printer to the system.
 *   add_printer_state_reasons() - Add the "printer-state-reasons" attribute
 *                                 based upon the printer state...
 *   add_queued_job_count()      - Add the "queued-job-count" attribute for
 *   cancel_all_jobs()           - Cancel all print jobs.
 *   cancel_job()                - Cancel a print job.
 *   check_quotas()              - Check quotas for a printer and user.
 *   copy_attribute()            - Copy a single attribute.
 *   copy_attrs()                - Copy attributes from one request to another.
 *   copy_banner()               - Copy a banner file to the requests directory
 *                                 for the specified job.
 *   copy_file()                 - Copy a PPD file or interface script...
 *   copy_model()                - Copy a PPD model file, substituting default
 *                                 values as needed...
 *   create_job()                - Print a file to a printer or class.
 *   delete_printer()            - Remove a printer or class from the system.
 *   get_default()               - Get the default destination.
 *   get_devices()               - Get the list of available devices on the
 *                                 local system.
 *   get_jobs()                  - Get a list of jobs for the specified printer.
 *   get_job_attrs()             - Get job attributes.
 *   get_ppds()                  - Get the list of PPD files on the local
 *                                 system.
 *   get_printer_attrs()         - Get printer attributes.
 *   get_printers()              - Get a list of printers.
 *   hold_job()                  - Hold a print job.
 *   move_job()                  - Move a job to a new destination.
 *   ppd_add_default()           - Add a PPD default choice.
 *   ppd_parse_line()            - Parse a PPD default line.
 *   print_job()                 - Print a file to a printer or class.
 *   read_ps_line()              - Read a line from a PS file...
 *   read_ps_job_ticket()        - Reads a job ticket embedded in a PS file.
 *   reject_jobs()               - Reject print jobs to a printer.
 *   release_job()               - Release a held print job.
 *   restart_job()               - Restart an old print job.
 *   send_document()             - Send a file to a printer or class.
 *   send_ipp_error()            - Send an error status back to the IPP client.
 *   set_default()               - Set the default destination...
 *   set_job_attrs()             - Set job attributes.
 *   start_printer()             - Start a printer.
 *   stop_printer()              - Stop a printer.
 *   validate_job()              - Validate printer options and destination.
 *   validate_user()             - Validate the user for the request.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_LIBPAPER
#  include <paper.h>
#endif /* HAVE_LIBPAPER */


/*
 * PPD default choice structure...
 */

typedef struct
{
  char	option[PPD_MAX_NAME];		/* Main keyword (option name) */
  char	choice[PPD_MAX_NAME];		/* Option keyword (choice name) */
} ppd_default_t;



/*
 * Local functions...
 */

static void	accept_jobs(client_t *con, ipp_attribute_t *uri);
static void	add_class(client_t *con, ipp_attribute_t *uri);
static int	add_file(client_t *con, job_t *job, mime_type_t *filetype,
		         int compression);
static void	add_job_state_reasons(client_t *con, job_t *job);
static void	add_printer(client_t *con, ipp_attribute_t *uri);
static void	add_printer_state_reasons(client_t *con, printer_t *p);
static void	add_queued_job_count(client_t *con, printer_t *p);
static void	cancel_all_jobs(client_t *con, ipp_attribute_t *uri);
static void	cancel_job(client_t *con, ipp_attribute_t *uri);
static int	check_quotas(client_t *con, printer_t *p);
static ipp_attribute_t	*copy_attribute(ipp_t *to, ipp_attribute_t *attr,
		                        int quickcopy);
static void	copy_attrs(ipp_t *to, ipp_t *from, ipp_attribute_t *req,
		           ipp_tag_t group, int quickcopy);
static int	copy_banner(client_t *con, job_t *job, const char *name);
static int	copy_file(const char *from, const char *to);
static int	copy_model(const char *from, const char *to);
static void	create_job(client_t *con, ipp_attribute_t *uri);
static void	delete_printer(client_t *con, ipp_attribute_t *uri);
static void	get_default(client_t *con);
static void	get_devices(client_t *con);
static void	get_jobs(client_t *con, ipp_attribute_t *uri);
static void	get_job_attrs(client_t *con, ipp_attribute_t *uri);
static void	get_ppds(client_t *con);
static void	get_printers(client_t *con, int type);
static void	get_printer_attrs(client_t *con, ipp_attribute_t *uri);
static void	hold_job(client_t *con, ipp_attribute_t *uri);
static void	move_job(client_t *con, ipp_attribute_t *uri);
static int	ppd_add_default(const char *option, const char *choice,
		                int num_defaults, ppd_default_t **defaults);
static int	ppd_parse_line(const char *line, char *option, int olen,
		               char *choice, int clen);
static void	print_job(client_t *con, ipp_attribute_t *uri);
static void	read_ps_job_ticket(client_t *con);
static void	reject_jobs(client_t *con, ipp_attribute_t *uri);
static void	release_job(client_t *con, ipp_attribute_t *uri);
static void	restart_job(client_t *con, ipp_attribute_t *uri);
static void	send_document(client_t *con, ipp_attribute_t *uri);
static void	send_ipp_error(client_t *con, ipp_status_t status);
static void	set_default(client_t *con, ipp_attribute_t *uri);
static void	set_job_attrs(client_t *con, ipp_attribute_t *uri);
static void	start_printer(client_t *con, ipp_attribute_t *uri);
static void	stop_printer(client_t *con, ipp_attribute_t *uri);
static void	validate_job(client_t *con, ipp_attribute_t *uri);
static int	validate_user(client_t *con, const char *owner, char *username,
		              int userlen);


/*
 * 'ProcessIPPRequest()' - Process an incoming IPP request...
 */

int					/* O - 1 on success, 0 on failure */
ProcessIPPRequest(client_t *con)	/* I - Client connection */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */
  ipp_attribute_t	*username;	/* requesting-user-name attr */


  DEBUG_printf(("ProcessIPPRequest(%08x)\n", con));
  DEBUG_printf(("ProcessIPPRequest: operation_id = %04x\n",
                con->request->request.op.operation_id));

 /*
  * First build an empty response message for this request...
  */

  con->response = ippNew();

  con->response->request.status.version[0] = con->request->request.op.version[0];
  con->response->request.status.version[1] = con->request->request.op.version[1];
  con->response->request.status.request_id = con->request->request.op.request_id;

 /*
  * Then validate the request header and required attributes...
  */
  
  if (con->request->request.any.version[0] != 1)
  {
   /*
    * Return an error, since we only support IPP 1.x.
    */

    LogMessage(L_ERROR, "ProcessIPPRequest: bad request version (%d.%d)!",
               con->request->request.any.version[0],
	       con->request->request.any.version[1]);

    send_ipp_error(con, IPP_VERSION_NOT_SUPPORTED);
  }  
  else if (con->request->attrs == NULL)
  {
    LogMessage(L_ERROR, "ProcessIPPRequest: no attributes in request!");
    send_ipp_error(con, IPP_BAD_REQUEST);
  }
  else
  {
   /*
    * Make sure that the attributes are provided in the correct order and
    * don't repeat groups...
    */

    for (attr = con->request->attrs, group = attr->group_tag;
	 attr != NULL;
	 attr = attr->next)
      if (attr->group_tag < group)
      {
       /*
	* Out of order; return an error...
	*/

	LogMessage(L_ERROR, "ProcessIPPRequest: attribute groups are out of order!");
	send_ipp_error(con, IPP_BAD_REQUEST);
	break;
      }
      else
	group = attr->group_tag;

    if (attr == NULL)
    {
     /*
      * Then make sure that the first three attributes are:
      *
      *     attributes-charset
      *     attributes-natural-language
      *     printer-uri/job-uri
      */

      attr = con->request->attrs;
      if (attr != NULL && strcmp(attr->name, "attributes-charset") == 0 &&
	  attr->value_tag == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      if (attr)
        attr = attr->next;
      if (attr != NULL && strcmp(attr->name, "attributes-natural-language") == 0 &&
	  attr->value_tag == IPP_TAG_LANGUAGE)
	language = attr;
      else
	language = NULL;

      if ((attr = ippFindAttribute(con->request, "printer-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(con->request, "job-uri", IPP_TAG_URI)) != NULL)
	uri = attr;
      else
	uri = NULL;

      if (charset)
	ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	     "attributes-charset", NULL, charset->values[0].string.text);
      else
	ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	     "attributes-charset", NULL, DefaultCharset);

      if (language)
	ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL,
		     language->values[0].string.text);
      else
	ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, DefaultLanguage);

      if (charset == NULL || language == NULL ||
	  (uri == NULL &&
	   con->request->request.op.operation_id != CUPS_GET_DEFAULT &&
	   con->request->request.op.operation_id != CUPS_GET_PRINTERS &&
	   con->request->request.op.operation_id != CUPS_GET_CLASSES &&
	   con->request->request.op.operation_id != CUPS_GET_DEVICES &&
	   con->request->request.op.operation_id != CUPS_GET_PPDS))
      {
       /*
	* Return an error, since attributes-charset,
	* attributes-natural-language, and printer-uri/job-uri are required
	* for all operations.
	*/

        if (charset == NULL)
	  LogMessage(L_ERROR, "ProcessIPPRequest: missing attributes-charset attribute!");

        if (language == NULL)
	  LogMessage(L_ERROR, "ProcessIPPRequest: missing attributes-natural-language attribute!");

        if (uri == NULL)
	  LogMessage(L_ERROR, "ProcessIPPRequest: missing printer-uri or job-uri attribute!");

	send_ipp_error(con, IPP_BAD_REQUEST);
      }
      else
      {
       /*
	* OK, all the checks pass so far; make sure requesting-user-name is
	* not "root" from a remote host...
	*/

        if ((username = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
	{
	 /*
	  * Check for root user...
	  */

	  if (strcmp(username->values[0].string.text, "root") == 0 &&
	      strcasecmp(con->http.hostname, "localhost") != 0 &&
	      strcmp(con->username, "root") != 0)
	  {
	   /*
	    * Remote unauthenticated user masquerading as local root...
	    */

	    free(username->values[0].string.text);
	    username->values[0].string.text = strdup(RemoteRoot);
	  }
	}

       /*
        * Then try processing the operation...
	*/

	switch (con->request->request.op.operation_id)
	{
	  case IPP_PRINT_JOB :
              print_job(con, uri);
              break;

	  case IPP_VALIDATE_JOB :
              validate_job(con, uri);
              break;

	  case IPP_CREATE_JOB :
              create_job(con, uri);
              break;

	  case IPP_SEND_DOCUMENT :
              send_document(con, uri);
              break;

	  case IPP_CANCEL_JOB :
              cancel_job(con, uri);
              break;

	  case IPP_GET_JOB_ATTRIBUTES :
              get_job_attrs(con, uri);
              break;

	  case IPP_GET_JOBS :
              get_jobs(con, uri);
              break;

	  case IPP_GET_PRINTER_ATTRIBUTES :
              get_printer_attrs(con, uri);
              break;

	  case IPP_HOLD_JOB :
              hold_job(con, uri);
              break;

	  case IPP_RELEASE_JOB :
              release_job(con, uri);
              break;

	  case IPP_RESTART_JOB :
              restart_job(con, uri);
              break;

	  case IPP_PAUSE_PRINTER :
              stop_printer(con, uri);
	      break;

	  case IPP_RESUME_PRINTER :
              start_printer(con, uri);
	      break;

	  case IPP_PURGE_JOBS :
              cancel_all_jobs(con, uri);
              break;

	  case IPP_SET_JOB_ATTRIBUTES :
              set_job_attrs(con, uri);
              break;

	  case CUPS_GET_DEFAULT :
              get_default(con);
              break;

	  case CUPS_GET_PRINTERS :
              get_printers(con, 0);
              break;

	  case CUPS_GET_CLASSES :
              get_printers(con, CUPS_PRINTER_CLASS);
              break;

	  case CUPS_ADD_PRINTER :
              add_printer(con, uri);
              break;

	  case CUPS_DELETE_PRINTER :
              delete_printer(con, uri);
              break;

	  case CUPS_ADD_CLASS :
              add_class(con, uri);
              break;

	  case CUPS_DELETE_CLASS :
              delete_printer(con, uri);
              break;

	  case CUPS_ACCEPT_JOBS :
	  case IPP_ENABLE_PRINTER :
              accept_jobs(con, uri);
              break;

	  case CUPS_REJECT_JOBS :
	  case IPP_DISABLE_PRINTER :
              reject_jobs(con, uri);
              break;

	  case CUPS_SET_DEFAULT :
              set_default(con, uri);
              break;

	  case CUPS_GET_DEVICES :
              get_devices(con);
              break;

	  case CUPS_GET_PPDS :
              get_ppds(con);
              break;

	  case CUPS_MOVE_JOB :
              move_job(con, uri);
              break;

	  default :
              send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
	}
      }
    }
  }

  LogMessage(L_DEBUG, "ProcessIPPRequest: %d status_code=%x",
             con->http.fd, con->response->request.status.status_code);

  if (SendHeader(con, HTTP_OK, "application/ipp"))
  {
#if 0
    if (con->http.version == HTTP_1_1)
    {
      con->http.data_encoding = HTTP_ENCODE_CHUNKED;

      httpPrintf(HTTP(con), "Transfer-Encoding: chunked\r\n\r\n");
    }
    else
#endif /* 0 */
    {
      con->http.data_encoding  = HTTP_ENCODE_LENGTH;
      con->http.data_remaining = ippLength(con->response);

      httpPrintf(HTTP(con), "Content-Length: %d\r\n\r\n",
        	 con->http.data_remaining);
    }

    LogMessage(L_DEBUG2, "ProcessIPPRequest: Adding fd %d to OutputSet...",
               con->http.fd);

    FD_SET(con->http.fd, OutputSet);

   /*
    * Tell the caller the response header was sent successfully...
    */

    return (1);
  }
  else
  {
   /*
    * Tell the caller the response header could not be sent...
    */

    return (0);
  }
}


/*
 * 'accept_jobs()' - Accept print jobs to a printer.
 */

static void
accept_jobs(client_t        *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Printer or class URI */
{
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  const char		*name;		/* Printer name */
  printer_t		*printer;	/* Printer data */


  LogMessage(L_DEBUG2, "accept_jobs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "accept_jobs: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((name = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "accept_jobs: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Accept jobs sent to the printer...
  */

  if (dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  printer->accepting        = 1;
  printer->state_message[0] = '\0';

  AddPrinterHistory(printer);

  if (dtype & CUPS_PRINTER_CLASS)
    SaveAllClasses();
  else
    SaveAllPrinters();

  LogMessage(L_INFO, "Printer \'%s\' now accepting jobs (\'%s\').", name,
             con->username);

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_class()' - Add a class to the system.
 */

static void
add_class(client_t        *con,		/* I - Client connection */
          ipp_attribute_t *uri)		/* I - URI of class */
{
  int			i;		/* Looping var */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*pclass;	/* Class */
  cups_ptype_t		dtype;		/* Destination type */
  const char		*dest;		/* Printer or class name */
  ipp_attribute_t	*attr;		/* Printer attribute */
  int			modify;		/* Non-zero if we just modified */


  LogMessage(L_DEBUG2, "add_class(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "add_class: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

  DEBUG_printf(("add_class(%08x, %08x)\n", con, uri));

 /*
  * Do we have a valid URI?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if (strncmp(resource, "/classes/", 9) != 0 || strlen(resource) == 9)
  {
   /*
    * No, return an error...
    */

    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * See if the class already exists; if not, create a new class...
  */

  if ((pclass = FindClass(resource + 9)) == NULL)
  {
   /*
    * Class doesn't exist; see if we have a printer of the same name...
    */

    if ((pclass = FindPrinter(resource + 9)) != NULL &&
        !(pclass->type & CUPS_PRINTER_REMOTE))
    {
     /*
      * Yes, return an error...
      */

      send_ipp_error(con, IPP_NOT_POSSIBLE);
      return;
    }

   /*
    * No, add the pclass...
    */

    pclass = AddClass(resource + 9);
    modify = 0;
  }
  else if (pclass->type & CUPS_PRINTER_IMPLICIT)
  {
   /*
    * Rename the implicit class to "AnyClass" or remove it...
    */

    if (ImplicitAnyClasses)
    {
      snprintf(pclass->name, sizeof(pclass->name), "Any%s", resource + 9);
      SortPrinters();
    }
    else
      DeletePrinter(pclass, 1);

   /*
    * Add the class as a new local class...
    */

    pclass = AddClass(resource + 9);
    modify = 0;
  }
  else if (pclass->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Rename the remote class to "Class"...
    */

    DeletePrinterFilters(pclass);
    snprintf(pclass->name, sizeof(pclass->name), "%s@%s", resource + 9,
             pclass->hostname);
    SetPrinterAttrs(pclass);
    SortPrinters();

   /*
    * Add the class as a new local class...
    */

    pclass = AddClass(resource + 9);
    modify = 0;
  }
  else
    modify = 1;

 /*
  * Look for attributes and copy them over as needed...
  */

  if ((attr = ippFindAttribute(con->request, "printer-location", IPP_TAG_TEXT)) != NULL)
    SetString(&pclass->location, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "printer-info", IPP_TAG_TEXT)) != NULL)
    SetString(&pclass->info, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    LogMessage(L_INFO, "Setting %s printer-is-accepting-jobs to %d (was %d.)",
               pclass->name, attr->values[0].boolean, pclass->accepting);

    pclass->accepting = attr->values[0].boolean;
    AddPrinterHistory(pclass);
  }
  if ((attr = ippFindAttribute(con->request, "printer-state", IPP_TAG_ENUM)) != NULL)
  {
    if (attr->values[0].integer != IPP_PRINTER_IDLE &&
        attr->values[0].integer == IPP_PRINTER_STOPPED)
    {
      LogMessage(L_ERROR, "Attempt to set %s printer-state to bad value %d!",
                 pclass->name, attr->values[0].integer);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    LogMessage(L_INFO, "Setting %s printer-state to %d (was %d.)", pclass->name,
               attr->values[0].integer, pclass->state);

    SetPrinterState(pclass, attr->values[0].integer, 0);
  }
  if ((attr = ippFindAttribute(con->request, "printer-state-message", IPP_TAG_TEXT)) != NULL)
  {
    strlcpy(pclass->state_message, attr->values[0].string.text,
            sizeof(pclass->state_message));
    AddPrinterHistory(pclass);
  }
  if ((attr = ippFindAttribute(con->request, "job-sheets-default", IPP_TAG_ZERO)) != NULL &&
      !Classification)
  {
    SetString(&pclass->job_sheets[0], attr->values[0].string.text);
    if (attr->num_values > 1)
      SetString(&pclass->job_sheets[1], attr->values[1].string.text);
    else
      SetString(&pclass->job_sheets[1], "none");
  }
  if ((attr = ippFindAttribute(con->request, "requesting-user-name-allowed",
                               IPP_TAG_ZERO)) != NULL)
  {
    FreePrinterUsers(pclass);

    pclass->deny_users = 0;
    if (attr->value_tag == IPP_TAG_NAME &&
        (attr->num_values > 1 ||
	 strcmp(attr->values[0].string.text, "all") != 0))
      for (i = 0; i < attr->num_values; i ++)
	AddPrinterUser(pclass, attr->values[i].string.text);
  }
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name-denied",
                                    IPP_TAG_ZERO)) != NULL)
  {
    FreePrinterUsers(pclass);

    pclass->deny_users = 1;
    if (attr->value_tag == IPP_TAG_NAME &&
        (attr->num_values > 1 ||
	 strcmp(attr->values[0].string.text, "none") != 0))
      for (i = 0; i < attr->num_values; i ++)
	AddPrinterUser(pclass, attr->values[i].string.text);
  }
  if ((attr = ippFindAttribute(con->request, "job-quota-period",
                               IPP_TAG_INTEGER)) != NULL)
  {
    LogMessage(L_DEBUG, "add_class: Setting job-quota-period to %d...",
               attr->values[0].integer);
    FreeQuotas(pclass);
    pclass->quota_period = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-k-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    LogMessage(L_DEBUG, "add_class: Setting job-k-limit to %d...",
               attr->values[0].integer);
    FreeQuotas(pclass);
    pclass->k_limit = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-page-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    LogMessage(L_DEBUG, "add_class: Setting job-page-limit to %d...",
               attr->values[0].integer);
    FreeQuotas(pclass);
    pclass->page_limit = attr->values[0].integer;
  }

  if ((attr = ippFindAttribute(con->request, "member-uris", IPP_TAG_URI)) != NULL)
  {
   /*
    * Clear the printer array as needed...
    */

    if (pclass->num_printers > 0)
    {
      free(pclass->printers);
      pclass->num_printers = 0;
    }

   /*
    * Add each printer or class that is listed...
    */

    for (i = 0; i < attr->num_values; i ++)
    {
     /*
      * Search for the printer or class URI...
      */

      httpSeparate(attr->values[i].string.text, method, username, host,
                   &port, resource);

      if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
      {
       /*
	* Bad URI...
	*/

        LogMessage(L_ERROR, "add_class: resource name \'%s\' no good!", resource);
	send_ipp_error(con, IPP_NOT_FOUND);
	return;
      }

     /*
      * Add it to the class...
      */

      if (dtype & CUPS_PRINTER_CLASS)
        AddPrinterToClass(pclass, FindClass(dest));
      else
        AddPrinterToClass(pclass, FindPrinter(dest));
    }
  }

 /*
  * Update the printer class attributes and return...
  */

  SetPrinterAttrs(pclass);
  SaveAllClasses();
  CheckJobs();

  WritePrintcap();

  if (modify)
    LogMessage(L_INFO, "Class \'%s\' modified by \'%s\'.", pclass->name,
               con->username);
  else
  {
    AddPrinterHistory(pclass);

    LogMessage(L_INFO, "New class \'%s\' added by \'%s\'.", pclass->name,
               con->username);
  }

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_file()' - Add a file to a job.
 */

static int				/* O - 0 on success, -1 on error */
add_file(client_t    *con,		/* I - Connection to client */
         job_t       *job,		/* I - Job to add to */
         mime_type_t *filetype,		/* I - Type of file */
	 int         compression)	/* I - Compression */
{
  mime_type_t	**filetypes;		/* New filetypes array... */
  int		*compressions;		/* New compressions array... */


  LogMessage(L_DEBUG2, "add_file(con=%p[%d], job=%d, filetype=%s/%s, compression=%d)\n",
             con, con->http.fd, job->id, filetype->super, filetype->type,
	     compression);

 /*
  * Add the file to the job...
  */

  if (job->num_files == 0)
  {
    compressions = (int *)malloc(sizeof(int));
    filetypes    = (mime_type_t **)malloc(sizeof(mime_type_t *));
  }
  else
  {
    compressions = (int *)realloc(job->compressions,
                                  (job->num_files + 1) * sizeof(int));
    filetypes    = (mime_type_t **)realloc(job->filetypes,
                                           (job->num_files + 1) *
					   sizeof(mime_type_t *));
  }

  if (compressions == NULL || filetypes == NULL)
  {
    CancelJob(job->id, 1);
    LogMessage(L_ERROR, "add_file: unable to allocate memory for file types!");
    send_ipp_error(con, IPP_INTERNAL_ERROR);
    return (-1);
  }

  job->compressions                 = compressions;
  job->compressions[job->num_files] = compression;
  job->filetypes                    = filetypes;
  job->filetypes[job->num_files]    = filetype;

  job->num_files ++;

  return (0);
}


/*
 * 'add_job_state_reasons()' - Add the "job-state-reasons" attribute based
 *                             upon the job and printer state...
 */

static void
add_job_state_reasons(client_t *con,	/* I - Client connection */
                      job_t    *job)	/* I - Job info */
{
  printer_t	*dest;			/* Destination printer */


  LogMessage(L_DEBUG2, "add_job_state_reasons(%d, %d)\n", con->http.fd,
             job->id);

  switch (job->state->values[0].integer)
  {
    case IPP_JOB_PENDING :
        if (job->dtype & CUPS_PRINTER_CLASS)
	  dest = FindClass(job->dest);
	else
	  dest = FindPrinter(job->dest);

        if (dest != NULL && dest->state == IPP_PRINTER_STOPPED)
          ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	               "job-state-reasons", NULL, "printer-stopped");
        else
          ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	               "job-state-reasons", NULL, "none");
        break;

    case IPP_JOB_HELD :
        if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD) != NULL ||
	    ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME) != NULL)
          ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	               "job-state-reasons", NULL, "job-hold-until-specified");
        else
          ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	               "job-state-reasons", NULL, "job-incoming");
        break;

    case IPP_JOB_PROCESSING :
        ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	             "job-state-reasons", NULL, "job-printing");
        break;

    case IPP_JOB_STOPPED :
        ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	             "job-state-reasons", NULL, "job-stopped");
        break;

    case IPP_JOB_CANCELLED :
        ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	             "job-state-reasons", NULL, "job-canceled-by-user");
        break;

    case IPP_JOB_ABORTED :
        ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	             "job-state-reasons", NULL, "aborted-by-system");
        break;

    case IPP_JOB_COMPLETED :
        ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	             "job-state-reasons", NULL, "job-completed-successfully");
        break;
  }
}


/*
 * 'add_printer()' - Add a printer to the system.
 */

static void
add_printer(client_t        *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - URI of printer */
{
  int			i;		/* Looping var */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*printer;	/* Printer/class */
  ipp_attribute_t	*attr;		/* Printer attribute */
  cups_file_t		*fp;		/* Script/PPD file */
  char			line[1024];	/* Line from file... */
  char			srcfile[1024],	/* Source Script/PPD file */
			dstfile[1024];	/* Destination Script/PPD file */
  int			modify;		/* Non-zero if we are modifying */


  LogMessage(L_DEBUG2, "add_printer(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "add_printer: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Do we have a valid URI?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if (strncmp(resource, "/printers/", 10) != 0 || strlen(resource) == 10)
  {
   /*
    * No, return an error...
    */

    LogMessage(L_ERROR, "add_printer: bad printer URI \"%s\"!",
               uri->values[0].string.text);
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * See if the printer already exists; if not, create a new printer...
  */

  if ((printer = FindPrinter(resource + 10)) == NULL)
  {
   /*
    * Printer doesn't exist; see if we have a class of the same name...
    */

    if ((printer = FindClass(resource + 10)) != NULL &&
        !(printer->type & CUPS_PRINTER_REMOTE))
    {
     /*
      * Yes, return an error...
      */

      LogMessage(L_ERROR, "add_printer: \"%s\" already exists as a class!",
        	 resource + 10);
      send_ipp_error(con, IPP_NOT_POSSIBLE);
      return;
    }

   /*
    * No, add the printer...
    */

    printer = AddPrinter(resource + 10);
    modify  = 0;
  }
  else if (printer->type & CUPS_PRINTER_IMPLICIT)
  {
   /*
    * Rename the implicit printer to "AnyPrinter" or delete it...
    */

    if (ImplicitAnyClasses)
    {
      snprintf(printer->name, sizeof(printer->name), "Any%s", resource + 10);
      SortPrinters();
    }
    else
      DeletePrinter(printer, 1);

   /*
    * Add the printer as a new local printer...
    */

    printer = AddPrinter(resource + 10);
    modify  = 0;
  }
  else if (printer->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Rename the remote printer to "Printer@server"...
    */

    DeletePrinterFilters(printer);
    snprintf(printer->name, sizeof(printer->name), "%s@%s", resource + 10,
             printer->hostname);
    SetPrinterAttrs(printer);
    SortPrinters();

   /*
    * Add the printer as a new local printer...
    */

    printer = AddPrinter(resource + 10);
    modify  = 0;
  }
  else
    modify = 1;

 /*
  * Look for attributes and copy them over as needed...
  */

  if ((attr = ippFindAttribute(con->request, "printer-location", IPP_TAG_TEXT)) != NULL)
    SetString(&printer->location, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "printer-info", IPP_TAG_TEXT)) != NULL)
    SetString(&printer->info, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "device-uri", IPP_TAG_URI)) != NULL)
  {
    ipp_attribute_t	*device;	/* Current device */
    int			methodlen;	/* Length of method string */


   /*
    * Do we have a valid device URI?
    */

    httpSeparate(attr->values[0].string.text, method, username, host,
                 &port, resource);
    methodlen = strlen(method);

    if (strcmp(method, "file") == 0)
    {
     /*
      * See if the administrator has enabled file devices...
      */

      if (!FileDevice && strcmp(resource, "/dev/null"))
      {
       /*
        * File devices are disabled and the URL is not file:/dev/null...
	*/

	LogMessage(L_ERROR, "add_printer: File device URIs have been disabled! "
	                    "To enable, see the FileDevice directive in cupsd.conf.");
	send_ipp_error(con, IPP_NOT_POSSIBLE);
	return;
      }
    }
    else
    {
     /*
      * See if the backend is listed as a device...
      */

      for (device = ippFindAttribute(Devices, "device-uri", IPP_TAG_URI);
           device != NULL;
	   device = ippFindNextAttribute(Devices, "device-uri", IPP_TAG_URI))
        if (strncmp(method, device->values[0].string.text, methodlen) == 0 &&
	    (device->values[0].string.text[methodlen] == ':' ||
	     device->values[0].string.text[methodlen] == '\0'))
	  break;

      if (device == NULL)
      {
       /*
        * Could not find device in list!
	*/

	LogMessage(L_ERROR, "add_printer: bad device-uri attribute \'%s\'!",
        	   attr->values[0].string.text);
	send_ipp_error(con, IPP_NOT_POSSIBLE);
	return;
      }
    }

    LogMessage(L_INFO, "Setting %s device-uri to \"%s\" (was \"%s\".)",
               printer->name, attr->values[0].string.text, printer->device_uri);

    SetString(&printer->device_uri, attr->values[0].string.text);
  }

  if ((attr = ippFindAttribute(con->request, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    LogMessage(L_INFO, "Setting %s printer-is-accepting-jobs to %d (was %d.)",
               printer->name, attr->values[0].boolean, printer->accepting);

    printer->accepting = attr->values[0].boolean;
    AddPrinterHistory(printer);
  }
  if ((attr = ippFindAttribute(con->request, "printer-state", IPP_TAG_ENUM)) != NULL)
  {
    if (attr->values[0].integer != IPP_PRINTER_IDLE &&
        attr->values[0].integer == IPP_PRINTER_STOPPED)
    {
      LogMessage(L_ERROR, "Attempt to set %s printer-state to bad value %d!",
                 printer->name, attr->values[0].integer);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    LogMessage(L_INFO, "Setting %s printer-state to %d (was %d.)", printer->name,
               attr->values[0].integer, printer->state);

    SetPrinterState(printer, attr->values[0].integer, 0);
  }
  if ((attr = ippFindAttribute(con->request, "printer-state-message", IPP_TAG_TEXT)) != NULL)
  {
    strlcpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message));
    AddPrinterHistory(printer);
  }
  if ((attr = ippFindAttribute(con->request, "job-sheets-default", IPP_TAG_ZERO)) != NULL &&
      !Classification)
  {
    SetString(&printer->job_sheets[0], attr->values[0].string.text);
    if (attr->num_values > 1)
      SetString(&printer->job_sheets[1], attr->values[1].string.text);
    else
      SetString(&printer->job_sheets[1], "none");
  }
  if ((attr = ippFindAttribute(con->request, "requesting-user-name-allowed",
                               IPP_TAG_ZERO)) != NULL)
  {
    FreePrinterUsers(printer);

    printer->deny_users = 0;
    if (attr->value_tag == IPP_TAG_NAME &&
        (attr->num_values > 1 ||
	 strcmp(attr->values[0].string.text, "all") != 0))
      for (i = 0; i < attr->num_values; i ++)
	AddPrinterUser(printer, attr->values[i].string.text);
  }
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name-denied",
                                    IPP_TAG_ZERO)) != NULL)
  {
    FreePrinterUsers(printer);

    printer->deny_users = 1;
    if (attr->value_tag == IPP_TAG_NAME &&
        (attr->num_values > 1 ||
	 strcmp(attr->values[0].string.text, "none") != 0))
      for (i = 0; i < attr->num_values; i ++)
	AddPrinterUser(printer, attr->values[i].string.text);
  }
  if ((attr = ippFindAttribute(con->request, "job-quota-period",
                               IPP_TAG_INTEGER)) != NULL)
  {
    LogMessage(L_DEBUG, "add_printer: Setting job-quota-period to %d...",
               attr->values[0].integer);
    FreeQuotas(printer);
    printer->quota_period = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-k-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    LogMessage(L_DEBUG, "add_printer: Setting job-k-limit to %d...",
               attr->values[0].integer);
    FreeQuotas(printer);
    printer->k_limit = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-page-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    LogMessage(L_DEBUG, "add_printer: Setting job-page-limit to %d...",
               attr->values[0].integer);
    FreeQuotas(printer);
    printer->page_limit = attr->values[0].integer;
  }

 /*
  * See if we have all required attributes...
  */

  if (!printer->device_uri)
    SetString(&printer->device_uri, "file:/dev/null");

 /*
  * See if we have an interface script or PPD file attached to the request...
  */

  if (con->filename)
  {
    strlcpy(srcfile, con->filename, sizeof(srcfile));

    if ((fp = cupsFileOpen(srcfile, "rb")) != NULL)
    {
     /*
      * Yes; get the first line from it...
      */

      line[0] = '\0';
      cupsFileGets(fp, line, sizeof(line));
      cupsFileClose(fp);

     /*
      * Then see what kind of file it is...
      */

      snprintf(dstfile, sizeof(dstfile), "%s/interfaces/%s", ServerRoot,
               printer->name);

      if (strncmp(line, "*PPD-Adobe", 10) == 0)
      {
       /*
	* The new file is a PPD file, so remove any old interface script
	* that might be lying around...
	*/

	unlink(dstfile);
      }
      else
      {
       /*
	* This must be an interface script, so move the file over to the
	* interfaces directory and make it executable...
	*/

	if (copy_file(srcfile, dstfile))
	{
          LogMessage(L_ERROR, "add_printer: Unable to copy interface script from %s to %s - %s!",
	             srcfile, dstfile, strerror(errno));
          send_ipp_error(con, IPP_INTERNAL_ERROR);
	  return;
	}
	else
	{
          LogMessage(L_DEBUG, "add_printer: Copied interface script successfully!");
          chmod(dstfile, 0755);
	}
      }

      snprintf(dstfile, sizeof(dstfile), "%s/ppd/%s.ppd", ServerRoot,
               printer->name);

      if (strncmp(line, "*PPD-Adobe", 10) == 0)
      {
       /*
	* The new file is a PPD file, so move the file over to the
	* ppd directory and make it readable by all...
	*/

	if (copy_file(srcfile, dstfile))
	{
          LogMessage(L_ERROR, "add_printer: Unable to copy PPD file from %s to %s - %s!",
	             srcfile, dstfile, strerror(errno));
          send_ipp_error(con, IPP_INTERNAL_ERROR);
	  return;
	}
	else
	{
          LogMessage(L_DEBUG, "add_printer: Copied PPD file successfully!");
          chmod(dstfile, 0644);
	}
      }
      else
      {
       /*
	* This must be an interface script, so remove any old PPD file that
	* may be lying around...
	*/

	unlink(dstfile);
      }
    }
  }
  else if ((attr = ippFindAttribute(con->request, "ppd-name", IPP_TAG_NAME)) != NULL)
  {
    if (strcmp(attr->values[0].string.text, "raw") == 0)
    {
     /*
      * Raw driver, remove any existing PPD or interface script files.
      */

      snprintf(dstfile, sizeof(dstfile), "%s/interfaces/%s", ServerRoot,
               printer->name);
      unlink(dstfile);

      snprintf(dstfile, sizeof(dstfile), "%s/ppd/%s.ppd", ServerRoot,
               printer->name);
      unlink(dstfile);
    }
    else
    {
     /*
      * PPD model file...
      */

      snprintf(srcfile, sizeof(srcfile), "%s/model/%s", DataDir,
               attr->values[0].string.text);

      snprintf(dstfile, sizeof(dstfile), "%s/interfaces/%s", ServerRoot,
               printer->name);
      unlink(dstfile);

      snprintf(dstfile, sizeof(dstfile), "%s/ppd/%s.ppd", ServerRoot,
               printer->name);

      if (copy_model(srcfile, dstfile))
      {
        LogMessage(L_ERROR, "add_printer: Unable to copy PPD file from %s to %s - %s!",
	           srcfile, dstfile, strerror(errno));
        send_ipp_error(con, IPP_INTERNAL_ERROR);
	return;
      }
      else
      {
        LogMessage(L_DEBUG, "add_printer: Copied PPD file successfully!");
        chmod(dstfile, 0644);
      }
    }
  }

 /*
  * Make this printer the default if there is none...
  */

  if (DefaultPrinter == NULL)
    DefaultPrinter = printer;

 /*
  * Update the printer attributes and return...
  */

  SetPrinterAttrs(printer);
  SaveAllPrinters();

  if (printer->job != NULL)
  {
    job_t *job;

   /*
    * Stop the current job and then restart it below...
    */

    job = (job_t *)printer->job;

    StopJob(job->id, 1);
    job->state->values[0].integer = IPP_JOB_PENDING;
  }

  CheckJobs();

  WritePrintcap();

  if (modify)
    LogMessage(L_INFO, "Printer \'%s\' modified by \'%s\'.", printer->name,
               con->username);
  else
  {
    AddPrinterHistory(printer);

    LogMessage(L_INFO, "New printer \'%s\' added by \'%s\'.", printer->name,
               con->username);
  }

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_printer_state_reasons()' - Add the "printer-state-reasons" attribute
 *                                 based upon the printer state...
 */

static void
add_printer_state_reasons(client_t  *con,	/* I - Client connection */
                          printer_t *p)		/* I - Printer info */
{
  LogMessage(L_DEBUG2, "add_printer_state_reasons(%d, %s)\n", con->http.fd,
             p->name);

  if (p->num_reasons == 0)
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                 "printer-state-reasons", NULL,
		 p->state == IPP_PRINTER_STOPPED ? "paused" : "none");
  else
    ippAddStrings(con->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                  "printer-state-reasons", p->num_reasons, NULL,
		  (const char * const *)p->reasons);
}


/*
 * 'add_queued_job_count()' - Add the "queued-job-count" attribute for
 *                            the specified printer or class.
 */

static void
add_queued_job_count(client_t  *con,	/* I - Client connection */
                     printer_t *p)	/* I - Printer or class */
{
  int		count;			/* Number of jobs on destination */


  LogMessage(L_DEBUG2, "add_queued_job_count(%d, %s)\n", con->http.fd,
             p->name);

  count = GetPrinterJobCount(p->name);

  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "queued-job-count", count);
}


/*
 * 'cancel_all_jobs()' - Cancel all print jobs.
 */

static void
cancel_all_jobs(client_t        *con,	/* I - Client connection */
	        ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			userpass[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  ipp_attribute_t	*attr;		/* Attribute in request */
  const char		*username;	/* Username */
  int			purge;		/* Purge? */


  LogMessage(L_DEBUG2, "cancel_all_jobs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "cancel_all_jobs: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * See if we have a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") != 0)
  {
    LogMessage(L_ERROR, "cancel_all_jobs: bad %s attribute \'%s\'!",
               uri->name, uri->values[0].string.text);
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * Get the username (if any) for the jobs we want to cancel (only if
  * "my-jobs" is specified...
  */

  if ((attr = ippFindAttribute(con->request, "my-jobs", IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
  {
    if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
      username = attr->values[0].string.text;
    else
    {
      LogMessage(L_ERROR, "cancel_all_jobs: missing requesting-user-name attribute!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
  }
  else
    username = NULL;

 /*
  * Look for the "purge-jobs" attribute...
  */

  if ((attr = ippFindAttribute(con->request, "purge-jobs", IPP_TAG_BOOLEAN)) != NULL)
    purge = attr->values[0].boolean;
  else
    purge = 1;

 /*
  * And if the destination is valid...
  */

  httpSeparate(uri->values[0].string.text, method, userpass, host, &port,
               resource);

  if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI?
    */

    if (strcmp(resource, "/printers/") != 0)
    {
      LogMessage(L_ERROR, "cancel_all_jobs: resource name \'%s\' no good!", resource);
      send_ipp_error(con, IPP_NOT_FOUND);
      return;
    }

   /*
    * Cancel all jobs on all printers...
    */

    CancelJobs(NULL, username, purge);

    LogMessage(L_INFO, "All jobs were %s by \'%s\'.",
               purge ? "purged" : "cancelled", con->username);
  }
  else
  {
   /*
    * Cancel all of the jobs on the named printer...
    */

    CancelJobs(dest, username, purge);

    LogMessage(L_INFO, "All jobs on \'%s\' were %s by \'%s\'.", dest,
               purge ? "purged" : "cancelled", con->username);
  }

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'cancel_job()' - Cancel a print job.
 */

static void
cancel_job(client_t        *con,	/* I - Client connection */
	   ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			jobid;		/* Job ID */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  job_t			*job;		/* Job information */
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  printer_t		*printer;	/* Printer data */


  LogMessage(L_DEBUG2, "cancel_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/jobs/", 5) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "cancel_job: cancel request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "cancel_job: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    if ((jobid = attr->values[0].integer) == 0)
    {
     /*
      * Find the current job on the specified printer...
      */

      httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

      if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
      {
       /*
	* Bad URI...
	*/

	LogMessage(L_ERROR, "cancel_job: resource name \'%s\' no good!", resource);
	send_ipp_error(con, IPP_NOT_FOUND);
	return;
      }

      if (dtype & CUPS_PRINTER_CLASS)
        printer = FindClass(dest);
      else
        printer = FindPrinter(dest);

     /*
      * See if the printer is currently printing a job...
      */

      if (printer->job)
        jobid = ((job_t *)printer->job)->id;
      else
      {
       /*
        * No, see if there are any pending jobs...
	*/

        for (job = Jobs; job != NULL; job = job->next)
	  if (job->state->values[0].integer <= IPP_JOB_PROCESSING &&
	      strcasecmp(job->dest, dest) == 0)
	    break;

	if (job != NULL)
	  jobid = job->id;
	else
	{
	  LogMessage(L_ERROR, "cancel_job: No active jobs on %s!", dest);
	  send_ipp_error(con, IPP_NOT_POSSIBLE);
	  return;
	}
      }
    }
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "cancel_job: bad job-uri attribute \'%s\'!",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "cancel_job: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "cancel_job: \"%s\" not authorized to delete job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

 /*
  * See if the job is already completed, cancelled, or aborted; if so,
  * we can't cancel...
  */

  if (job->state->values[0].integer >= IPP_JOB_CANCELLED)
  {
    LogMessage(L_ERROR, "cancel_job: job id %d is %s - can't cancel!",
               jobid,
	       job->state->values[0].integer == IPP_JOB_CANCELLED ? "cancelled" :
	       job->state->values[0].integer == IPP_JOB_ABORTED ? "aborted" :
	       "completed");
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * Cancel the job and return...
  */

  CancelJob(jobid, 0);
  CheckJobs();

  LogMessage(L_INFO, "Job %d was cancelled by \'%s\'.", jobid, username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'check_quotas()' - Check quotas for a printer and user.
 */

static int			/* O - 1 if OK, 0 if not */
check_quotas(client_t  *con,	/* I - Client connection */
             printer_t *p)	/* I - Printer or class */
{
  int		i, j;		/* Looping vars */
  ipp_attribute_t *attr;	/* Current attribute */
  char		username[33];	/* Username */
  quota_t	*q;		/* Quota data */
  struct passwd	*pw;		/* User password data */
  struct group	*grp;		/* Group data */


  LogMessage(L_DEBUG2, "check_quotas(%d, %s)\n", con->http.fd, p->name);

 /*
  * Check input...
  */

  if (con == NULL || p == NULL)
    return (0);

 /*
  * Figure out who is printing...
  */

  attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME);

  if (con->username[0])
    strlcpy(username, con->username, sizeof(username));
  else if (attr != NULL)
  {
    LogMessage(L_DEBUG, "check_quotas: requesting-user-name = \'%s\'",
               attr->values[0].string.text);

    strlcpy(username, attr->values[0].string.text, sizeof(username));
  }
  else
    strcpy(username, "anonymous");

 /*
  * Check global active job limits for printers and users...
  */

  if (MaxJobsPerPrinter)
  {
   /*
    * Check if there are too many pending jobs on this printer...
    */

    if (GetPrinterJobCount(p->name) >= MaxJobsPerPrinter)
    {
      LogMessage(L_INFO, "Too many jobs for printer \"%s\"...", p->name);
      return (0);
    }
  }

  if (MaxJobsPerUser)
  {
   /*
    * Check if there are too many pending jobs for this user...
    */

    if (GetUserJobCount(username) >= MaxJobsPerUser)
    {
      LogMessage(L_INFO, "Too many jobs for user \"%s\"...", username);
      return (0);
    }
  }

 /*
  * Check against users...
  */

  if (p->num_users == 0 && p->k_limit == 0 && p->page_limit == 0)
    return (1);

  if (p->num_users)
  {
    pw = getpwnam(username);
    endpwent();

    for (i = 0; i < p->num_users; i ++)
      if (p->users[i][0] == '@')
      {
       /*
        * Check group membership...
	*/

        grp = getgrnam(p->users[i] + 1);
	endgrent();

        if (grp)
	{
	 /*
	  * Check primary group...
	  */

	  if (pw && grp->gr_gid == pw->pw_gid)
	    break;

         /*
	  * Check usernames in group...
	  */

          for (j = 0; grp->gr_mem[j]; j ++)
	    if (!strcmp(username, grp->gr_mem[j]))
	      break;

          if (grp->gr_mem[j])
	    break;
	}
      }
      else if (!strcasecmp(username, p->users[i]))
	break;

    if ((i < p->num_users) == p->deny_users)
    {
      LogMessage(L_INFO, "Denying user \"%s\" access to printer \"%s\"...",
        	 username, p->name);
      return (0);
    }
  }

 /*
  * Check quotas...
  */

  if (p->k_limit || p->page_limit)
  {
    if ((q = UpdateQuota(p, username, 0, 0)) == NULL)
    {
      LogMessage(L_ERROR, "Unable to allocate quota data for user \"%s\"!",
                 username);
      return (0);
    }

    if ((q->k_count >= p->k_limit && p->k_limit) ||
        (q->page_count >= p->page_limit && p->page_limit))
    {
      LogMessage(L_INFO, "User \"%s\" is over the quota limit...",
                 username);
      return (0);
    }
  }

 /*
  * If we have gotten this far, we're done!
  */

  return (1);
}


/*
 * 'copy_attribute()' - Copy a single attribute.
 */

static ipp_attribute_t *		/* O - New attribute */
copy_attribute(ipp_t           *to,	/* O - Destination request/response */
               ipp_attribute_t *attr,	/* I - Attribute to copy */
               int             quickcopy)/* I - Do a quick copy? */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*toattr;	/* Destination attribute */


  LogMessage(L_DEBUG2, "copy_attribute(%p, %s)\n", to,
             attr->name ? attr->name : "(null)");

  switch (attr->value_tag & ~IPP_TAG_COPY)
  {
    case IPP_TAG_ZERO :
        toattr = ippAddSeparator(to);
	break;

    case IPP_TAG_INTEGER :
    case IPP_TAG_ENUM :
        toattr = ippAddIntegers(to, attr->group_tag, attr->value_tag,
	                        attr->name, attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	  toattr->values[i].integer = attr->values[i].integer;
        break;

    case IPP_TAG_BOOLEAN :
        toattr = ippAddBooleans(to, attr->group_tag, attr->name,
	                        attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	  toattr->values[i].boolean = attr->values[i].boolean;
        break;

    case IPP_TAG_STRING :
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
        toattr = ippAddStrings(to, attr->group_tag,
	                       (ipp_tag_t)(attr->value_tag | quickcopy),
	                       attr->name, attr->num_values, NULL, NULL);

        if (quickcopy)
	{
          for (i = 0; i < attr->num_values; i ++)
	    toattr->values[i].string.text = attr->values[i].string.text;
        }
	else
	{
          for (i = 0; i < attr->num_values; i ++)
	    toattr->values[i].string.text = strdup(attr->values[i].string.text);
	}
        break;

    case IPP_TAG_DATE :
        toattr = ippAddDate(to, attr->group_tag, attr->name,
	                    attr->values[0].date);
        break;

    case IPP_TAG_RESOLUTION :
        toattr = ippAddResolutions(to, attr->group_tag, attr->name,
	                           attr->num_values, IPP_RES_PER_INCH,
				   NULL, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].resolution.xres  = attr->values[i].resolution.xres;
	  toattr->values[i].resolution.yres  = attr->values[i].resolution.yres;
	  toattr->values[i].resolution.units = attr->values[i].resolution.units;
	}
        break;

    case IPP_TAG_RANGE :
        toattr = ippAddRanges(to, attr->group_tag, attr->name,
	                      attr->num_values, NULL, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].range.lower = attr->values[i].range.lower;
	  toattr->values[i].range.upper = attr->values[i].range.upper;
	}
        break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
        toattr = ippAddStrings(to, attr->group_tag,
	                       (ipp_tag_t)(attr->value_tag | quickcopy),
	                       attr->name, attr->num_values, NULL, NULL);

        if (quickcopy)
	{
          for (i = 0; i < attr->num_values; i ++)
	  {
            toattr->values[i].string.charset = attr->values[i].string.charset;
	    toattr->values[i].string.text    = attr->values[i].string.text;
          }
        }
	else
	{
          for (i = 0; i < attr->num_values; i ++)
	  {
	    if (!i)
              toattr->values[i].string.charset =
	          strdup(attr->values[i].string.charset);
	    else
              toattr->values[i].string.charset =
	          toattr->values[0].string.charset;

	    toattr->values[i].string.text = strdup(attr->values[i].string.text);
          }
        }
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        toattr = ippAddCollections(to, attr->group_tag, attr->name,
	                           attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].collection = ippNew();
	  copy_attrs(toattr->values[i].collection, attr->values[i].collection,
	             NULL, IPP_TAG_ZERO, 0);
	}
        break;

    default :
        toattr = ippAddIntegers(to, attr->group_tag, attr->value_tag,
	                        attr->name, attr->num_values, NULL);

        for (i = 0; i < attr->num_values; i ++)
	{
	  toattr->values[i].unknown.length = attr->values[i].unknown.length;

	  if (toattr->values[i].unknown.length > 0)
	  {
	    if ((toattr->values[i].unknown.data = malloc(toattr->values[i].unknown.length)) == NULL)
	      toattr->values[i].unknown.length = 0;
	    else
	      memcpy(toattr->values[i].unknown.data,
		     attr->values[i].unknown.data,
		     toattr->values[i].unknown.length);
	  }
	}
        break; /* anti-compiler-warning-code */
  }

  return (toattr);
}


/*
 * 'copy_attrs()' - Copy attributes from one request to another.
 */

static void
copy_attrs(ipp_t           *to,		/* I - Destination request */
           ipp_t           *from,	/* I - Source request */
           ipp_attribute_t *req,	/* I - Requested attributes */
	   ipp_tag_t       group,	/* I - Group to copy */
	   int             quickcopy)	/* I - Do a quick copy? */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*fromattr;	/* Source attribute */


  LogMessage(L_DEBUG2, "copy_attrs(%p, %p, %p, %x)\n", to, from, req, group);

  if (to == NULL || from == NULL)
    return;

  if (req != NULL && strcmp(req->values[0].string.text, "all") == 0)
    req = NULL;				/* "all" means no filter... */

  for (fromattr = from->attrs; fromattr != NULL; fromattr = fromattr->next)
  {
   /*
    * Filter attributes as needed...
    */

    if (group != IPP_TAG_ZERO && fromattr->group_tag != group &&
        fromattr->group_tag != IPP_TAG_ZERO)
      continue;

    if (req != NULL && fromattr->name != NULL)
    {
      for (i = 0; i < req->num_values; i ++)
        if (strcmp(fromattr->name, req->values[i].string.text) == 0)
	  break;

      if (i == req->num_values)
        continue;
    }

    copy_attribute(to, fromattr, quickcopy);
  }
}


/*
 * 'copy_banner()' - Copy a banner file to the requests directory for the
 *                   specified job.
 */

static int			/* O - Size of banner file in kbytes */
copy_banner(client_t   *con,	/* I - Client connection */
            job_t      *job,	/* I - Job information */
            const char *name)	/* I - Name of banner */
{
  int		i;		/* Looping var */
  int		kbytes;		/* Size of banner file in kbytes */
  char		filename[1024];	/* Job filename */
  banner_t	*banner;	/* Pointer to banner */
  cups_file_t	*in;		/* Input file */
  cups_file_t	*out;		/* Output file */
  int		ch;		/* Character from file */
  char		attrname[255],	/* Name of attribute */
		*s;		/* Pointer into name */
  ipp_attribute_t *attr;	/* Attribute */


  LogMessage(L_DEBUG2, "copy_banner(%d, %d, %s)\n", con->http.fd, job->id,
             name);

 /*
  * Find the banner; return if not found or "none"...
  */

  LogMessage(L_DEBUG, "copy_banner(%p, %d, \"%s\")", con, job->id,
             name ? name : "(null)");

  if (name == NULL ||
      strcmp(name, "none") == 0 ||
      (banner = FindBanner(name)) == NULL)
    return (0);

 /*
  * Open the banner and job files...
  */

  if (add_file(con, job, banner->filetype, 0))
    return (0);

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  if ((out = cupsFileOpen(filename, "w")) == NULL)
  {
    LogMessage(L_ERROR, "copy_banner: Unable to create banner job file %s - %s",
               filename, strerror(errno));
    job->num_files --;
    return (0);
  }

  fchmod(cupsFileNumber(out), 0640);
  fchown(cupsFileNumber(out), User, Group);

  if (con->language)
  {
   /*
    * Try the localized banner file under the subdirectory...
    */

    snprintf(filename, sizeof(filename), "%s/banners/%s/%s", DataDir,
             con->language->language, name);

    if (access(filename, 0) && con->language->language[2])
    {
     /*
      * Wasn't able to find "ll_CC" locale file; try the non-national
      * localization banner directory.
      */

      attrname[0] = con->language->language[0];
      attrname[1] = con->language->language[1];
      attrname[2] = '\0';

      snprintf(filename, sizeof(filename), "%s/banners/%s/%s", DataDir,
               attrname, name);
    }

    if (access(filename, 0))
    {
     /*
      * Use the non-localized banner file.
      */

      snprintf(filename, sizeof(filename), "%s/banners/%s", DataDir, name);
    }
  }
  else
  {
   /*
    * Use the non-localized banner file.
    */

    snprintf(filename, sizeof(filename), "%s/banners/%s", DataDir, name);
  }

  if ((in = cupsFileOpen(filename, "r")) == NULL)
  {
    cupsFileClose(out);
    unlink(filename);
    LogMessage(L_ERROR, "copy_banner: Unable to open banner template file %s - %s",
               filename, strerror(errno));
    job->num_files --;
    return (0);
  }

 /*
  * Parse the file to the end...
  */

  while ((ch = cupsFileGetChar(in)) != EOF)
    if (ch == '{')
    {
     /*
      * Get an attribute name...
      */

      for (s = attrname; (ch = cupsFileGetChar(in)) != EOF;)
        if (!isalpha(ch) && ch != '-' && ch != '?')
          break;
	else if (s < (attrname + sizeof(attrname) - 1))
          *s++ = ch;
	else
	  break;

      *s = '\0';

      if (ch != '}')
      {
       /*
        * Ignore { followed by stuff that is not an attribute name...
	*/

        cupsFilePrintf(out, "{%s%c", attrname, ch);
	continue;
      }

     /*
      * See if it is defined...
      */

      if (attrname[0] == '?')
        s = attrname + 1;
      else
        s = attrname;

      if (strcmp(s, "printer-name") == 0)
      {
        cupsFilePuts(out, job->dest);
	continue;
      }
      else if ((attr = ippFindAttribute(job->attrs, s, IPP_TAG_ZERO)) == NULL)
      {
       /*
        * See if we have a leading question mark...
	*/

	if (attrname[0] != '?')
	{
	 /*
          * Nope, write to file as-is; probably a PostScript procedure...
	  */

	  cupsFilePrintf(out, "{%s}", attrname);
        }

        continue;
      }

     /*
      * Output value(s)...
      */

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  cupsFilePutChar(out, ',');

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      if (strncmp(s, "time-at-", 8) == 0)
	        cupsFilePuts(out, GetDateTime(attr->values[i].integer));
	      else
	        cupsFilePrintf(out, "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      cupsFilePrintf(out, "%d", attr->values[i].boolean);
	      break;

	  case IPP_TAG_NOVALUE :
	      cupsFilePuts(out, "novalue");
	      break;

	  case IPP_TAG_RANGE :
	      cupsFilePrintf(out, "%d-%d", attr->values[i].range.lower,
		      attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      cupsFilePrintf(out, "%dx%d%s", attr->values[i].resolution.xres,
		      attr->values[i].resolution.yres,
		      attr->values[i].resolution.units == IPP_RES_PER_INCH ?
			  "dpi" : "dpc");
	      break;

	  case IPP_TAG_URI :
          case IPP_TAG_STRING :
	  case IPP_TAG_TEXT :
	  case IPP_TAG_NAME :
	  case IPP_TAG_KEYWORD :
	  case IPP_TAG_CHARSET :
	  case IPP_TAG_LANGUAGE :
	      if (strcasecmp(banner->filetype->type, "postscript") == 0)
	      {
	       /*
	        * Need to quote strings for PS banners...
		*/

	        const char *p;

		for (p = attr->values[i].string.text; *p; p ++)
		{
		  if (*p == '(' || *p == ')' || *p == '\\')
		  {
		    cupsFilePutChar(out, '\\');
		    cupsFilePutChar(out, *p);
		  }
		  else if (*p < 32 || *p > 126)
		    cupsFilePrintf(out, "\\%03o", *p);
		  else
		    cupsFilePutChar(out, *p);
		}
	      }
	      else
		cupsFilePuts(out, attr->values[i].string.text);
	      break;

          default :
	      break; /* anti-compiler-warning-code */
	}
      }
    }
    else if (ch == '\\')	/* Quoted char */
    {
      ch = cupsFileGetChar(in);

      if (ch != '{')		/* Only do special handling for \{ */
        cupsFilePutChar(out, '\\');

      cupsFilePutChar(out, ch);
    }
    else
      cupsFilePutChar(out, ch);

  cupsFileClose(in);

  kbytes = (cupsFileTell(out) + 1023) / 1024;

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets", IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer += kbytes;

  cupsFileClose(out);

  return (kbytes);
}


/*
 * 'copy_file()' - Copy a PPD file or interface script...
 */

static int				/* O - 0 = success, -1 = error */
copy_file(const char *from,		/* I - Source file */
          const char *to)		/* I - Destination file */
{
  cups_file_t	*src,			/* Source file */
		*dst;			/* Destination file */
  int		bytes;			/* Bytes to read/write */
  char		buffer[2048];		/* Copy buffer */


  LogMessage(L_DEBUG2, "copy_file(\"%s\", \"%s\")\n", from, to);

 /*
  * Open the source and destination file for a copy...
  */

  if ((src = cupsFileOpen(from, "rb")) == NULL)
    return (-1);

  if ((dst = cupsFileOpen(to, "wb")) == NULL)
  {
    cupsFileClose(src);
    return (-1);
  }

 /*
  * Copy the source file to the destination...
  */

  while ((bytes = cupsFileRead(src, buffer, sizeof(buffer))) > 0)
    if (cupsFileWrite(dst, buffer, bytes) < bytes)
    {
      cupsFileClose(src);
      cupsFileClose(dst);
      return (-1);
    }

 /*
  * Close both files and return...
  */

  cupsFileClose(src);

  return (cupsFileClose(dst));
}


/*
 * 'copy_model()' - Copy a PPD model file, substituting default values
 *                  as needed...
 */

static int				/* O - 0 = success, -1 = error */
copy_model(const char *from,		/* I - Source file */
           const char *to)		/* I - Destination file */
{
  cups_file_t	*src,			/* Source file */
		*dst;			/* Destination file */
  char		buffer[2048];		/* Copy buffer */
  int		i;			/* Looping var */
  char		option[PPD_MAX_NAME],	/* Option name */
		choice[PPD_MAX_NAME];	/* Choice name */
  int		num_defaults;		/* Number of default options */
  ppd_default_t	*defaults;		/* Default options */
  char		cups_protocol[PPD_MAX_LINE];
					/* cupsProtocol attribute */
#ifdef HAVE_LIBPAPER
  char		*paper_result;		/* Paper size name from libpaper */
  char		system_paper[64];	/* Paper size name buffer */
#endif /* HAVE_LIBPAPER */


  LogMessage(L_DEBUG2, "copy_model(\"%s\", \"%s\")\n", from, to);

 /*
  * Open the destination (if possible) and set the default options...
  */

  num_defaults     = 0;
  defaults         = NULL;
  cups_protocol[0] = '\0';

  if ((dst = cupsFileOpen(to, "rb")) != NULL)
  {
   /*
    * Read all of the default lines from the old PPD...
    */

    while (cupsFileGets(dst, buffer, sizeof(buffer)) != NULL)
      if (!strncmp(buffer, "*Default", 8))
      {
       /*
	* Add the default option...
	*/

        if (!ppd_parse_line(buffer, option, sizeof(option),
	                    choice, sizeof(choice)))
          num_defaults = ppd_add_default(option, choice, num_defaults,
	                                 &defaults);
      }
      else if (!strncmp(buffer, "*cupsProtocol:", 14))
        strlcpy(cups_protocol, buffer, sizeof(cups_protocol));

    cupsFileClose(dst);
  }
#ifdef HAVE_LIBPAPER
  else if ((paper_result = systempapername()) != NULL)
  {
   /*
    * Set the default media sizes from the systemwide default...
    */

    strlcpy(system_paper, paper_result, sizeof(system_paper));
    system_paper[0] = toupper(system_paper[0]);

    num_defaults = ppd_add_default("PageSize", system_paper, 
				   num_defaults, &defaults);
    num_defaults = ppd_add_default("PageRegion", system_paper, 
				   num_defaults, &defaults);
    num_defaults = ppd_add_default("PaperDimension", system_paper, 
				   num_defaults, &defaults);
    num_defaults = ppd_add_default("ImageableArea", system_paper, 
				   num_defaults, &defaults);
  }
#endif /* HAVE_LIBPAPER */
  else
  {
   /*
    * Add the default media sizes...
    *
    * Note: These values are generally not valid for large-format devices
    *       like plotters, however it is probably safe to say that those
    *       users will configure the media size after initially adding
    *       the device anyways...
    */

    if (!DefaultLanguage ||
        !strcasecmp(DefaultLanguage, "C") ||
        !strcasecmp(DefaultLanguage, "POSIX") ||
	!strcasecmp(DefaultLanguage, "en") ||
	!strncasecmp(DefaultLanguage, "en_US", 5) ||
	!strncasecmp(DefaultLanguage, "en_CA", 5) ||
	!strncasecmp(DefaultLanguage, "fr_CA", 5))
    {
     /*
      * These are the only locales that will default to "letter" size...
      */

      num_defaults = ppd_add_default("PageSize", "Letter", num_defaults,
                                     &defaults);
      num_defaults = ppd_add_default("PageRegion", "Letter", num_defaults,
                                     &defaults);
      num_defaults = ppd_add_default("PaperDimension", "Letter", num_defaults,
                                     &defaults);
      num_defaults = ppd_add_default("ImageableArea", "Letter", num_defaults,
                                     &defaults);
    }
    else
    {
     /*
      * The rest default to "a4" size...
      */

      num_defaults = ppd_add_default("PageSize", "A4", num_defaults,
                                     &defaults);
      num_defaults = ppd_add_default("PageRegion", "A4", num_defaults,
                                     &defaults);
      num_defaults = ppd_add_default("PaperDimension", "A4", num_defaults,
                                     &defaults);
      num_defaults = ppd_add_default("ImageableArea", "A4", num_defaults,
                                     &defaults);
    }
  }

 /*
  * Open the source and destination file for a copy...
  */

  if ((src = cupsFileOpen(from, "rb")) == NULL)
    return (-1);

  if ((dst = cupsFileOpen(to, "wb")) == NULL)
  {
    cupsFileClose(src);
    return (-1);
  }

 /*
  * Copy the source file to the destination...
  */

  while (cupsFileGets(src, buffer, sizeof(buffer)) != NULL)
  {
    if (!strncmp(buffer, "*Default", 8))
    {
     /*
      * Check for an previous default option choice...
      */

      if (!ppd_parse_line(buffer, option, sizeof(option),
	                  choice, sizeof(choice)))
      {
        for (i = 0; i < num_defaults; i ++)
	  if (!strcmp(option, defaults[i].option))
	  {
	   /*
	    * Substitute the previous choice...
	    */

	    snprintf(buffer, sizeof(buffer), "*Default%s: %s", option,
	             defaults[i].choice);
	    break;
	  }
      }
    }

    cupsFilePrintf(dst, "%s\n", buffer);
  }

  if (cups_protocol[0])
    cupsFilePrintf(dst, "%s\n", cups_protocol);

 /*
  * Close both files and return...
  */

  cupsFileClose(src);

  return (cupsFileClose(dst));
}


/*
 * 'create_job()' - Print a file to a printer or class.
 */

static void
create_job(client_t        *con,	/* I - Client connection */
	   ipp_attribute_t *uri)	/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  int			priority;	/* Job priority */
  char			*title;		/* Job name/title */
  job_t			*job;		/* Current job */
  char			job_uri[HTTP_MAX_URI],
					/* Job URI */
			printer_uri[HTTP_MAX_URI],
					/* Printer URI */
			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*printer;	/* Printer data */
  int			kbytes;		/* Size of print file */
  int			i;		/* Looping var */
  int			lowerpagerange;	/* Page range bound */


  LogMessage(L_DEBUG2, "create_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "create_job: cancel request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "create_job: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the printer is accepting jobs...
  */

  if (dtype & CUPS_PRINTER_CLASS)
  {
    printer = FindClass(dest);

#ifdef AF_INET6
    if (con->http.hostaddr.addr.sa_family == AF_INET6)
    snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/classes/%s",
             ServerName, ntohs(con->http.hostaddr.ipv6.sin6_port), dest);
    else
#endif /* AF_INET6 */
    snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/classes/%s",
             ServerName, ntohs(con->http.hostaddr.ipv4.sin_port), dest);
  }
  else
  {
    printer = FindPrinter(dest);

#ifdef AF_INET6
    if (con->http.hostaddr.addr.sa_family == AF_INET6)
    snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/printers/%s",
             ServerName, ntohs(con->http.hostaddr.ipv6.sin6_port), dest);
    else
#endif /* AF_INET6 */
    snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/printers/%s",
             ServerName, ntohs(con->http.hostaddr.ipv4.sin_port), dest);
  }

  if (!printer->accepting)
  {
    LogMessage(L_INFO, "create_job: destination \'%s\' is not accepting jobs.",
               dest);
    send_ipp_error(con, IPP_NOT_ACCEPTING);
    return;
  }

 /*
  * Validate job template attributes; for now just copies and page-ranges...
  */

  if ((attr = ippFindAttribute(con->request, "copies", IPP_TAG_INTEGER)) != NULL)
  {
    if (attr->values[0].integer < 1 || attr->values[0].integer > MaxCopies)
    {
      LogMessage(L_INFO, "create_job: bad copies value %d.",
                 attr->values[0].integer);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
  }

  if ((attr = ippFindAttribute(con->request, "page-ranges", IPP_TAG_RANGE)) != NULL)
  {
    for (i = 0, lowerpagerange = 1; i < attr->num_values; i ++)
    {
      if (attr->values[i].range.lower < lowerpagerange || 
	  attr->values[i].range.lower > attr->values[i].range.upper)
      {
	LogMessage(L_ERROR, "create_job: bad page-ranges values %d-%d.",
	           attr->values[i].range.lower, attr->values[i].range.upper);
	send_ipp_error(con, IPP_BAD_REQUEST);
	return;
      }

      lowerpagerange = attr->values[i].range.upper + 1;
    }
  }

 /*
  * Make sure we aren't over our limit...
  */

  if (NumJobs >= MaxJobs && MaxJobs)
    CleanJobs();

  if (NumJobs >= MaxJobs && MaxJobs)
  {
    LogMessage(L_INFO, "create_job: too many jobs.");
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

  if (!check_quotas(con, printer))
  {
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * Create the job and set things up...
  */

  if ((attr = ippFindAttribute(con->request, "job-priority", IPP_TAG_INTEGER)) != NULL)
    priority = attr->values[0].integer;
  else
    ippAddInteger(con->request, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-priority",
                  priority = 50);

  if ((attr = ippFindAttribute(con->request, "job-name", IPP_TAG_NAME)) != NULL)
    title = attr->values[0].string.text;
  else
    ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
                 title = "Untitled");

  if ((job = AddJob(priority, printer->name)) == NULL)
  {
    LogMessage(L_ERROR, "create_job: unable to add job for destination \'%s\'!",
               dest);
    send_ipp_error(con, IPP_INTERNAL_ERROR);
    return;
  }

  job->dtype   = dtype;
  job->attrs   = con->request;
  con->request = NULL;

  attr = ippFindAttribute(job->attrs, "requesting-user-name", IPP_TAG_NAME);

  if (con->username[0])
    SetString(&job->username, con->username);
  else if (attr != NULL)
  {
    LogMessage(L_DEBUG, "create_job: requesting-user-name = \'%s\'",
               attr->values[0].string.text);

    SetString(&job->username, attr->values[0].string.text);
  }
  else
    SetString(&job->username, "anonymous");

  if (attr == NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name",
                 NULL, job->username);
  else
  {
    attr->group_tag = IPP_TAG_JOB;
    SetString(&attr->name, "job-originating-user-name");
  }

  if ((attr = ippFindAttribute(job->attrs, "job-originating-host-name",
                               IPP_TAG_ZERO)) != NULL)
  {
   /*
    * Request contains a job-originating-host-name attribute; validate it...
    */

    if (attr->value_tag != IPP_TAG_NAME ||
        attr->num_values != 1 ||
        strcmp(con->http.hostname, "localhost") != 0)
    {
     /*
      * Can't override the value if we aren't connected via localhost.
      * Also, we can only have 1 value and it must be a name value.
      */

      int i;	/* Looping var */

      switch (attr->value_tag)
      {
        case IPP_TAG_STRING :
	case IPP_TAG_TEXTLANG :
	case IPP_TAG_NAMELANG :
	case IPP_TAG_TEXT :
	case IPP_TAG_NAME :
	case IPP_TAG_KEYWORD :
	case IPP_TAG_URI :
	case IPP_TAG_URISCHEME :
	case IPP_TAG_CHARSET :
	case IPP_TAG_LANGUAGE :
	case IPP_TAG_MIMETYPE :
	   /*
	    * Free old strings...
	    */

	    for (i = 0; i < attr->num_values; i ++)
	    {
	      free(attr->values[i].string.text);
	      attr->values[i].string.text = NULL;
	      if (attr->values[i].string.charset)
	      {
		free(attr->values[i].string.charset);
		attr->values[i].string.charset = NULL;
	      }
            }

	default :
            break;
      }

     /*
      * Use the default connection hostname instead...
      */

      attr->value_tag             = IPP_TAG_NAME;
      attr->num_values            = 1;
      attr->values[0].string.text = strdup(con->http.hostname);
    }

    attr->group_tag = IPP_TAG_JOB;
  }
  else
  {
   /*
    * No job-originating-host-name attribute, so use the hostname from
    * the connection...
    */

    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, 
        	 "job-originating-host-name", NULL, con->http.hostname);
  }

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation",
                time(NULL));
  attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                       "time-at-processing", 0);
  attr->value_tag = IPP_TAG_NOVALUE;
  attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                       "time-at-completed", 0);
  attr->value_tag = IPP_TAG_NOVALUE;

 /*
  * Add remaining job attributes...
  */

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  job->state = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_ENUM,
                             "job-state", IPP_JOB_STOPPED);
  job->sheets = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                              "job-media-sheets-completed", 0);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL,
               printer_uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
               title);

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets", IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer = 0;
  else
    attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                         "job-k-octets", 0);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);
  if (attr == NULL)
    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                        "job-hold-until", NULL, "no-hold");
  if (attr != NULL && strcmp(attr->values[0].string.text, "no-hold") != 0 &&
      !(printer->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Hold job until specified time...
    */

    SetJobHoldUntil(job->id, attr->values[0].string.text);
  }
  else
    job->hold_until = time(NULL) + 60;

  job->state->values[0].integer = IPP_JOB_HELD;

  if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) ||
      Classification)
  {
   /*
    * Add job sheets options...
    */

    if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) == NULL)
    {
      LogMessage(L_DEBUG, "Adding default job-sheets values \"%s,%s\"...",
                 printer->job_sheets[0], printer->job_sheets[1]);

      attr = ippAddStrings(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-sheets",
                           2, NULL, NULL);
      attr->values[0].string.text = strdup(printer->job_sheets[0]);
      attr->values[1].string.text = strdup(printer->job_sheets[1]);
    }

    job->job_sheets = attr;

   /*
    * Enforce classification level if set...
    */

    if (Classification)
    {
      if (ClassifyOverride)
      {
        if (strcmp(attr->values[0].string.text, "none") == 0 &&
	    (attr->num_values == 1 ||
	     strcmp(attr->values[1].string.text, "none") == 0))
        {
	 /*
          * Force the leading banner to have the classification on it...
	  */

          SetString(&attr->values[0].string.text, Classification);
	}
	else if (attr->num_values == 2 &&
	         strcmp(attr->values[0].string.text, attr->values[1].string.text) != 0 &&
		 strcmp(attr->values[0].string.text, "none") != 0 &&
		 strcmp(attr->values[1].string.text, "none") != 0)
        {
	 /*
	  * Can't put two different security markings on the same document!
	  */

          SetString(&attr->values[1].string.text, attr->values[0].string.text);
	}
      }
      else if (strcmp(attr->values[0].string.text, Classification) != 0 &&
               (attr->num_values == 1 ||
	       strcmp(attr->values[1].string.text, Classification) != 0))
      {
       /*
        * Force the leading banner to have the classification on it...
	*/

        SetString(&attr->values[0].string.text, Classification);
      }
    }

   /*
    * See if we need to add the starting sheet...
    */

    if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
    {
      LogMessage(L_INFO, "Adding start banner page \"%s\" to job %d.",
                 attr->values[0].string.text, job->id);

      kbytes = copy_banner(con, job, attr->values[0].string.text);

      UpdateQuota(printer, job->username, 0, kbytes);
    }
  }
  else if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL)
    job->sheets = attr;

 /*
  * Save and log the job...
  */
   
  SaveJob(job->id);

  LogMessage(L_INFO, "Job %d created on \'%s\' by \'%s\'.", job->id,
             job->dest, job->username);

 /*
  * Fill in the response info...
  */

#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
    snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	     ntohs(con->http.hostaddr.ipv6.sin6_port), job->id);
  else
#endif /* AF_INET6 */
  snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	   ntohs(con->http.hostaddr.ipv4.sin_port), job->id);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, job_uri);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state",
                job->state->values[0].integer);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'delete_printer()' - Remove a printer or class from the system.
 */

static void
delete_printer(client_t        *con,	/* I - Client connection */
               ipp_attribute_t *uri)	/* I - URI of printer or class */
{
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*printer;	/* Printer/class */
  char			filename[1024];	/* Script/PPD filename */


  LogMessage(L_DEBUG2, "delete_printer(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "delete_printer: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

  DEBUG_printf(("delete_printer(%08x, %08x)\n", con, uri));

 /*
  * Do we have a valid URI?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "delete_printer: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Find the printer or class and delete it...
  */

  if (dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(dest);
  else
    printer = FindPrinter(dest);

 /*
  * Remove old jobs...
  */

  CancelJobs(dest, NULL, 1);

 /*
  * Remove any old PPD or script files...
  */

  snprintf(filename, sizeof(filename), "%s/interfaces/%s", ServerRoot, dest);
  unlink(filename);

  snprintf(filename, sizeof(filename), "%s/ppd/%s.ppd", ServerRoot, dest);
  unlink(filename);

  if (dtype & CUPS_PRINTER_CLASS)
  {
    LogMessage(L_INFO, "Class \'%s\' deleted by \'%s\'.", dest,
               con->username);

    DeletePrinter(printer, 0);
    SaveAllClasses();
  }
  else
  {
    LogMessage(L_INFO, "Printer \'%s\' deleted by \'%s\'.", dest,
               con->username);

    DeletePrinter(printer, 0);
    SaveAllPrinters();
  }

 /*
  * Return with no errors...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_default()' - Get the default destination.
 */

static void
get_default(client_t *con)		/* I - Client connection */
{
  LogMessage(L_DEBUG2, "get_default(%d)\n", con->http.fd);

  if (DefaultPrinter != NULL)
  {
    copy_attrs(con->response, DefaultPrinter->attrs,
               ippFindAttribute(con->request, "requested-attributes",
	                	IPP_TAG_KEYWORD), IPP_TAG_ZERO, 0);

    con->response->request.status.status_code = IPP_OK;
  }
  else
    con->response->request.status.status_code = IPP_NOT_FOUND;
}


/*
 * 'get_devices()' - Get the list of available devices on the local system.
 */

static void
get_devices(client_t *con)		/* I - Client connection */
{
  LogMessage(L_DEBUG2, "get_devices(%d)\n", con->http.fd);

 /*
  * Copy the device attributes to the response using the requested-attributes
  * attribute that may be provided by the client.
  */

  copy_attrs(con->response, Devices,
             ippFindAttribute(con->request, "requested-attributes",
	                      IPP_TAG_KEYWORD), IPP_TAG_ZERO, IPP_TAG_COPY);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_jobs()' - Get a list of jobs for the specified printer.
 */

static void
get_jobs(client_t        *con,		/* I - Client connection */
	 ipp_attribute_t *uri)		/* I - Printer URI */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*requested;	/* Requested attributes */
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  cups_ptype_t		dmask;		/* Destination type mask */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  int			completed;	/* Completed jobs? */
  int			limit;		/* Maximum number of jobs to return */
  int			count;		/* Number of jobs that match */
  job_t			*job;		/* Current job pointer */
  char			job_uri[HTTP_MAX_URI];
					/* Job URI... */


  LogMessage(L_DEBUG2, "get_jobs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if (strcmp(resource, "/") == 0 ||
      (strncmp(resource, "/jobs", 5) == 0 && strlen(resource) <= 6))
  {
    dest  = NULL;
    dtype = (cups_ptype_t)0;
    dmask = (cups_ptype_t)0;
  }
  else if (strncmp(resource, "/printers", 9) == 0 && strlen(resource) <= 10)
  {
    dest  = NULL;
    dtype = (cups_ptype_t)0;
    dmask = CUPS_PRINTER_CLASS;
  }
  else if (strncmp(resource, "/classes", 8) == 0 && strlen(resource) <= 9)
  {
    dest  = NULL;
    dtype = CUPS_PRINTER_CLASS;
    dmask = CUPS_PRINTER_CLASS;
  }
  else if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "get_jobs: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }
  else
    dmask = CUPS_PRINTER_CLASS;

 /*
  * See if the "which-jobs" attribute have been specified...
  */

  if ((attr = ippFindAttribute(con->request, "which-jobs", IPP_TAG_KEYWORD)) != NULL &&
      strcmp(attr->values[0].string.text, "completed") == 0)
    completed = 1;
  else
    completed = 0;

 /*
  * See if they want to limit the number of jobs reported...
  */

  if ((attr = ippFindAttribute(con->request, "limit", IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 1000000;

 /*
  * See if we only want to see jobs for a specific user...
  */

  if ((attr = ippFindAttribute(con->request, "my-jobs", IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
  {
    if (con->username[0])
      strlcpy(username, con->username, sizeof(username));
    else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
      strlcpy(username, attr->values[0].string.text, sizeof(username));
    else
      strcpy(username, "anonymous");
  }
  else
    username[0] = '\0';

  requested = ippFindAttribute(con->request, "requested-attributes",
	                       IPP_TAG_KEYWORD);

 /*
  * OK, build a list of jobs for this printer...
  */

  for (count = 0, job = Jobs; count < limit && job != NULL; job = job->next)
  {
   /*
    * Filter out jobs that don't match...
    */

    DEBUG_printf(("get_jobs: job->id = %d\n", job->id));

    if ((dest != NULL && strcmp(job->dest, dest) != 0) &&
        (job->printer == NULL || dest == NULL ||
	 strcmp(job->printer->name, dest) != 0))
      continue;
    if ((job->dtype & dmask) != dtype &&
        (job->printer == NULL || (job->printer->type & dmask) != dtype))
      continue;
    if (username[0] != '\0' && strcmp(username, job->username) != 0)
      continue;

    if (completed && job->state->values[0].integer <= IPP_JOB_STOPPED)
      continue;
    if (!completed && job->state->values[0].integer > IPP_JOB_STOPPED)
      continue;

    count ++;

    DEBUG_printf(("get_jobs: count = %d\n", count));

   /*
    * Send the requested attributes for each job...
    */

#ifdef AF_INET6
    if (con->http.hostaddr.addr.sa_family == AF_INET6)
      snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	       ntohs(con->http.hostaddr.ipv6.sin6_port), job->id);
    else
#endif /* AF_INET6 */
    snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	     ntohs(con->http.hostaddr.ipv4.sin_port), job->id);

    ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
                 "job-more-info", NULL, job_uri);

    ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
                 "job-uri", NULL, job_uri);

    ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                  "job-printer-up-time", time(NULL));

   /*
    * Copy the job attributes to the response using the requested-attributes
    * attribute that may be provided by the client.
    */

    copy_attrs(con->response, job->attrs, requested, IPP_TAG_JOB, 0);

    add_job_state_reasons(con, job);

    ippAddSeparator(con->response);
  }

  if (requested != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_job_attrs()' - Get job attributes.
 */

static void
get_job_attrs(client_t        *con,		/* I - Client connection */
	      ipp_attribute_t *uri)		/* I - Job URI */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*requested;	/* Requested attributes */
  int			jobid;		/* Job ID */
  job_t			*job;		/* Current job */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  char			job_uri[HTTP_MAX_URI];
					/* Job URI... */


  LogMessage(L_DEBUG2, "get_job_attrs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "get_job_attrs: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "get_job_attrs: bad job-uri attribute \'%s\'!\n",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "get_job_attrs: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Put out the standard attributes...
  */

#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
    snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d",
	     ServerName, ntohs(con->http.hostaddr.ipv6.sin6_port),
	     job->id);
  else
#endif /* AF_INET6 */
  snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d",
	   ServerName, ntohs(con->http.hostaddr.ipv4.sin_port),
	   job->id);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
               "job-more-info", NULL, job_uri);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
               "job-uri", NULL, job_uri);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "job-printer-up-time", time(NULL));

 /*
  * Copy the job attributes to the response using the requested-attributes
  * attribute that may be provided by the client.
  */

  requested = ippFindAttribute(con->request, "requested-attributes",
	                       IPP_TAG_KEYWORD);

  copy_attrs(con->response, job->attrs, requested, IPP_TAG_JOB, 0);

  add_job_state_reasons(con, job);

  if (requested != NULL)
    con->response->request.status.status_code = IPP_OK_SUBST;
  else
    con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_ppds()' - Get the list of PPD files on the local system.
 */

static void
get_ppds(client_t *con)			/* I - Client connection */
{
  LogMessage(L_DEBUG2, "get_ppds(%d)\n", con->http.fd);

 /*
  * Copy the PPD attributes to the response using the requested-attributes
  * attribute that may be provided by the client.
  */

  copy_attrs(con->response, PPDs,
             ippFindAttribute(con->request, "requested-attributes",
	                      IPP_TAG_KEYWORD), IPP_TAG_ZERO, IPP_TAG_COPY);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_printer_attrs()' - Get printer attributes.
 */

static void
get_printer_attrs(client_t        *con,	/* I - Client connection */
		  ipp_attribute_t *uri)	/* I - Printer URI */
{
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*printer;	/* Printer/class */
  time_t		curtime;	/* Current time */
  int			i;		/* Looping var */
  ipp_attribute_t	*requested,	/* requested-attributes */
			*history;	/* History collection */
  int			need_history;	/* Need to send history collection? */


  LogMessage(L_DEBUG2, "get_printer_attrs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "get_printer_attrs: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

  if (dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(dest);
  else
    printer = FindPrinter(dest);

  curtime = time(NULL);

 /*
  * Copy the printer attributes to the response using requested-attributes
  * and document-format attributes that may be provided by the client.
  */

  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                printer->state);

  add_printer_state_reasons(con, printer);

  ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "printer-state-message", NULL, printer->state_message);

  ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                printer->accepting);

  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "printer-up-time", curtime);
  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "printer-state-time", printer->state_time);
  ippAddDate(con->response, IPP_TAG_PRINTER, "printer-current-time",
             ippTimeToDate(curtime));

  add_queued_job_count(con, printer);

  requested = ippFindAttribute(con->request, "requested-attributes",
	                       IPP_TAG_KEYWORD);

  copy_attrs(con->response, printer->attrs, requested, IPP_TAG_ZERO, 0);
  copy_attrs(con->response, CommonData, requested, IPP_TAG_ZERO, IPP_TAG_COPY);

  need_history = 0;

  if (MaxPrinterHistory > 0 && printer->num_history > 0 && requested)
  {
    for (i = 0; i < requested->num_values; i ++)
      if (!strcmp(requested->values[i].string.text, "all") ||
          !strcmp(requested->values[i].string.text, "printer-state-history"))
      {
        need_history = 1;
        break;
      }
  }

  if (need_history)
  {
    history = ippAddCollections(con->response, IPP_TAG_PRINTER,
                                "printer-state-history",
                                printer->num_history, NULL);

    for (i = 0; i < printer->num_history; i ++)
      copy_attrs(history->values[i].collection = ippNew(), printer->history[i],
                 NULL, IPP_TAG_ZERO, 0);
  }

  con->response->request.status.status_code = requested ? IPP_OK_SUBST : IPP_OK;
}


/*
 * 'get_printers()' - Get a list of printers or classes.
 */

static void
get_printers(client_t *con,		/* I - Client connection */
             int      type)		/* I - 0 or CUPS_PRINTER_CLASS */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*requested,	/* requested-attributes */
			*history,	/* History collection */
			*attr;		/* Current attribute */
  int			need_history;	/* Need to send history collection? */
  int			limit;		/* Maximum number of printers to return */
  int			count;		/* Number of printers that match */
  printer_t		*printer;	/* Current printer pointer */
  time_t		curtime;	/* Current time */
  int			printer_type,	/* printer-type attribute */
			printer_mask;	/* printer-type-mask attribute */
  char			*location;	/* Location string */
  char			name[IPP_MAX_NAME],
					/* Printer name */
			*nameptr;	/* Pointer into name */
  printer_t		*iclass;	/* Implicit class */


  LogMessage(L_DEBUG2, "get_printers(%d, %x)\n", con->http.fd, type);

 /*
  * See if they want to limit the number of printers reported...
  */

  if ((attr = ippFindAttribute(con->request, "limit", IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 10000000;

 /*
  * Support filtering...
  */

  if ((attr = ippFindAttribute(con->request, "printer-type", IPP_TAG_ENUM)) != NULL)
    printer_type = attr->values[0].integer;
  else
    printer_type = 0;

  if ((attr = ippFindAttribute(con->request, "printer-type-mask", IPP_TAG_ENUM)) != NULL)
    printer_mask = attr->values[0].integer;
  else
    printer_mask = 0;

  if ((attr = ippFindAttribute(con->request, "printer-location", IPP_TAG_TEXT)) != NULL)
    location = attr->values[0].string.text;
  else
    location = NULL;

  requested = ippFindAttribute(con->request, "requested-attributes",
	                       IPP_TAG_KEYWORD);

  need_history = 0;

  if (MaxPrinterHistory > 0 && requested)
  {
    for (i = 0; i < requested->num_values; i ++)
      if (!strcmp(requested->values[i].string.text, "all") ||
          !strcmp(requested->values[i].string.text, "printer-state-history"))
      {
        need_history = 1;
        break;
      }
  }

 /*
  * OK, build a list of printers for this printer...
  */

  curtime = time(NULL);

  for (count = 0, printer = Printers;
       count < limit && printer != NULL;
       printer = printer->next)
    if ((printer->type & CUPS_PRINTER_CLASS) == type &&
        (printer->type & printer_mask) == printer_type &&
	(location == NULL || printer->location == NULL ||
	 strcasecmp(printer->location, location) == 0))
    {
     /*
      * If HideImplicitMembers is enabled, see if this printer or class
      * is a member of an implicit class...
      */

      if (ImplicitClasses && HideImplicitMembers &&
          (printer->type & CUPS_PRINTER_REMOTE))
      {
       /*
        * Make a copy of the printer name...
	*/

        strlcpy(name, printer->name, sizeof(name));

	if ((nameptr = strchr(name, '@')) != NULL)
	{
	 /*
	  * Strip trailing @server...
	  */

	  *nameptr = '\0';

         /*
	  * Find the core printer, if any...
	  */

          if ((iclass = FindPrinter(name)) != NULL &&
	      (iclass->type & CUPS_PRINTER_IMPLICIT))
	    continue;
	}
      }

     /*
      * Add the group separator as needed...
      */

      if (count > 0)
        ippAddSeparator(con->response);

      count ++;

     /*
      * Send the following attributes for each printer:
      *
      *    printer-state
      *    printer-state-message
      *    printer-is-accepting-jobs
      *    + all printer attributes
      */

      ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_ENUM,
                    "printer-state", printer->state);

      add_printer_state_reasons(con, printer);

      ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                   "printer-state-message", NULL, printer->state_message);

      ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                    printer->accepting);

      ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                    "printer-up-time", curtime);
      ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                    "printer-state-time", printer->state_time);
      ippAddDate(con->response, IPP_TAG_PRINTER, "printer-current-time",
        	 ippTimeToDate(curtime));

      add_queued_job_count(con, printer);

      copy_attrs(con->response, printer->attrs, requested, IPP_TAG_ZERO, 0);

      copy_attrs(con->response, CommonData, requested, IPP_TAG_ZERO,
                 IPP_TAG_COPY);

      if (need_history && printer->num_history > 0)
      {
	history = ippAddCollections(con->response, IPP_TAG_PRINTER,
                                    "printer-state-history",
                                    printer->num_history, NULL);

	for (i = 0; i < printer->num_history; i ++)
	  copy_attrs(history->values[i].collection = ippNew(),
	             printer->history[i], NULL, IPP_TAG_ZERO, 0);
      }
    }

  con->response->request.status.status_code = requested ? IPP_OK_SUBST : IPP_OK;
}


/*
 * 'hold_job()' - Hold a print job.
 */

static void
hold_job(client_t        *con,	/* I - Client connection */
         ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t	*attr,		/* Current job-hold-until */
			*newattr;	/* New job-hold-until */
  int			jobid;		/* Job ID */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  job_t			*job;		/* Job information */


  LogMessage(L_DEBUG2, "hold_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/jobs/", 5) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "hold_job: hold request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "hold_job: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "hold_job: bad job-uri attribute \'%s\'!",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "hold_job: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "hold_job: \"%s\" not authorized to hold job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

 /*
  * Hold the job and return...
  */

  HoldJob(jobid);

  if ((newattr = ippFindAttribute(con->request, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    newattr = ippFindAttribute(con->request, "job-hold-until", IPP_TAG_NAME);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

  if (attr != NULL)
  {
   /*
    * Free the old hold value and copy the new one over...
    */

    free(attr->values[0].string.text);

    if (newattr != NULL)
    {
      attr->value_tag = newattr->value_tag;
      attr->values[0].string.text = strdup(newattr->values[0].string.text);
    }
    else
    {
      attr->value_tag = IPP_TAG_KEYWORD;
      attr->values[0].string.text = strdup("indefinite");
    }

   /*
    * Hold job until specified time...
    */

    SetJobHoldUntil(job->id, attr->values[0].string.text);
  }

  LogMessage(L_INFO, "Job %d was held by \'%s\'.", jobid, username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'move_job()' - Move a job to a new destination.
 */

static void
move_job(client_t        *con,		/* I - Client connection */
	 ipp_attribute_t *uri)		/* I - Job URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			jobid;		/* Job ID */
  job_t			*job;		/* Current job */
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  LogMessage(L_DEBUG2, "move_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "move_job: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "move_job: bad job-uri attribute \'%s\'!\n",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "move_job: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the job has been completed...
  */

  if (job->state->values[0].integer > IPP_JOB_STOPPED)
  {
   /*
    * Return a "not-possible" error...
    */

    LogMessage(L_ERROR, "move_job: job #%d is finished and cannot be altered!", jobid);
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "move_job: \"%s\" not authorized to move job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

  if ((attr = ippFindAttribute(con->request, "job-printer-uri", IPP_TAG_URI)) == NULL)
  {
   /*
    * Need job-printer-uri...
    */

    LogMessage(L_ERROR, "move_job: job-printer-uri attribute missing!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }
    
 /*
  * Move the job to a different printer or class...
  */

  httpSeparate(attr->values[0].string.text, method, username, host, &port,
               resource);
  if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "move_job: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

  MoveJob(jobid, dest);

 /*
  * Start jobs if possible...
  */

  CheckJobs();

 /*
  * Return with "everything is OK" status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'ppd_add_default()' - Add a PPD default choice.
 */

static int				/* O  - Number of defaults */
ppd_add_default(const char    *option,	/* I  - Option name */
                const char    *choice,	/* I  - Choice name */
                int           num_defaults,
					/* I  - Number of defaults */
		ppd_default_t **defaults)
					/* IO - Defaults */
{
  int		i;			/* Looping var */
  ppd_default_t	*temp;			/* Temporary defaults array */


 /*
  * First check if the option already has a default value; the PPD spec
  * says that the first one is used...
  */

  for (i = 0, temp = *defaults; i < num_defaults; i ++)
    if (!strcmp(option, temp[i].option))
      return (num_defaults);

 /*
  * Now add the option...
  */

  if (num_defaults == 0)
    temp = malloc(sizeof(ppd_default_t));
  else
    temp = realloc(*defaults, (num_defaults + 1) * sizeof(ppd_default_t));

  if (!temp)
  {
    LogMessage(L_ERROR, "ppd_add_default: Unable to add default value for \"%s\" - %s",
               option, strerror(errno));
    return (num_defaults);
  }

  *defaults = temp;
  temp      += num_defaults;

  strlcpy(temp->option, option, sizeof(temp->option));
  strlcpy(temp->choice, choice, sizeof(temp->choice));

  return (num_defaults + 1);
}


/*
 * 'ppd_parse_line()' - Parse a PPD default line.
 */

static int				/* O - 0 on success, -1 on failure */
ppd_parse_line(const char *line,	/* I - Line */
               char       *option,	/* O - Option name */
	       int        olen,		/* I - Size of option name */
               char       *choice,	/* O - Choice name */
	       int        clen)		/* I - Size of choice name */
{
 /*
  * Verify this is a default option line...
  */

  if (strncmp(line, "*Default", 8))
    return (-1);

 /*
  * Read the option name...
  */

  for (line += 8, olen --; isalnum(*line); line ++)
    if (olen > 0)
    {
      *option++ = *line;
      olen --;
    }

  *option = '\0';

 /*
  * Skip everything else up to the colon (:)...
  */

  while (*line && *line != ':')
    line ++;

  if (!*line)
    return (-1);

  line ++;

 /*
  * Now grab the option choice, skipping leading whitespace...
  */

  while (isspace(*line))
    line ++;

  for (clen --; isalnum(*line); line ++)
    if (clen > 0)
    {
      *choice++ = *line;
      clen --;
    }

  *choice = '\0';

 /*
  * Return with no errors...
  */

  return (0);
}


/*
 * 'print_job()' - Print a file to a printer or class.
 */

static void
print_job(client_t        *con,		/* I - Client connection */
	  ipp_attribute_t *uri)		/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  const char		*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  int			priority;	/* Job priority */
  char			*title;		/* Job name/title */
  job_t			*job;		/* Current job */
  char			job_uri[HTTP_MAX_URI],
					/* Job URI */
			printer_uri[HTTP_MAX_URI],
					/* Printer URI */
			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI],
					/* Resource portion of URI */
			filename[1024];	/* Job filename */
  int			port;		/* Port portion of URI */
  mime_type_t		*filetype;	/* Type of file */
  char			super[MIME_MAX_SUPER],
					/* Supertype of file */
			type[MIME_MAX_TYPE],
					/* Subtype of file */
			mimetype[MIME_MAX_SUPER + MIME_MAX_TYPE + 2];
					/* Textual name of mime type */
  printer_t		*printer;	/* Printer data */
  struct stat		fileinfo;	/* File information */
  int			kbytes;		/* Size of file */
  int			i;		/* Looping var */
  int			lowerpagerange;	/* Page range bound */
  int			compression;	/* Document compression */


  LogMessage(L_DEBUG2, "print_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "print_job: cancel request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Validate job template attributes; for now just copies and page-ranges...
  */

  if ((attr = ippFindAttribute(con->request, "copies", IPP_TAG_INTEGER)) != NULL)
  {
    if (attr->values[0].integer < 1 || attr->values[0].integer > MaxCopies)
    {
      LogMessage(L_INFO, "print_job: bad copies value %d.",
                 attr->values[0].integer);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
  }

  if ((attr = ippFindAttribute(con->request, "page-ranges", IPP_TAG_RANGE)) != NULL)
  {
    for (i = 0, lowerpagerange = 1; i < attr->num_values; i ++)
    {
      if (attr->values[i].range.lower < lowerpagerange || 
	  attr->values[i].range.lower > attr->values[i].range.upper)
      {
	LogMessage(L_ERROR, "print_job: bad page-ranges values %d-%d.",
		   attr->values[i].range.lower, attr->values[i].range.upper);
	send_ipp_error(con, IPP_BAD_REQUEST);
	return;
      }

      lowerpagerange = attr->values[i].range.upper + 1;
    }
  }

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * only supports "none" and "gzip".
  */

  compression = CUPS_FILE_NONE;

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL)
  {
    if (strcmp(attr->values[0].string.text, "none")
#ifdef HAVE_LIBZ
        && strcmp(attr->values[0].string.text, "gzip")
#endif /* HAVE_LIBZ */
      )
    {
      LogMessage(L_ERROR, "print_job: Unsupported compression \"%s\"!",
        	 attr->values[0].string.text);
      send_ipp_error(con, IPP_ATTRIBUTES);
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
	           "compression", NULL, attr->values[0].string.text);
      return;
    }

#ifdef HAVE_LIBZ
    if (!strcmp(attr->values[0].string.text, "gzip"))
      compression = CUPS_FILE_GZIP;
#endif /* HAVE_LIBZ */
  }

 /*
  * Do we have a file to print?
  */

  if (!con->filename)
  {
    LogMessage(L_ERROR, "print_job: No file!?!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format", IPP_TAG_MIMETYPE)) != NULL)
  {
   /*
    * Grab format from client...
    */

    if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
    {
      LogMessage(L_ERROR, "print_job: could not scan type \'%s\'!",
	         format->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
  }
  else
  {
   /*
    * No document format attribute?  Auto-type it!
    */

    strcpy(super, "application");
    strcpy(type, "octet-stream");
  }

  if (strcmp(super, "application") == 0 &&
      strcmp(type, "octet-stream") == 0)
  {
   /*
    * Auto-type the file...
    */

    LogMessage(L_DEBUG, "print_job: auto-typing file...");

    filetype = mimeFileType(MimeDatabase, con->filename, &compression);

    if (filetype != NULL)
    {
     /*
      * Replace the document-format attribute value with the auto-typed one.
      */

      snprintf(mimetype, sizeof(mimetype), "%s/%s", filetype->super,
               filetype->type);

      if (format != NULL)
      {
	free(format->values[0].string.text);
	format->values[0].string.text = strdup(mimetype);
      }
      else
        ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
	             "document-format", NULL, mimetype);
    }
    else
      filetype = mimeType(MimeDatabase, super, type);
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype == NULL)
  {
    LogMessage(L_ERROR, "print_job: Unsupported format \'%s/%s\'!",
	       super, type);
    LogMessage(L_INFO, "Hint: Do you have the raw file printing rules enabled?");
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);

    if (format)
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);

    return;
  }

  LogMessage(L_DEBUG, "print_job: request file type is %s/%s.",
	     filetype->super, filetype->type);

 /*
  * Read any embedded job ticket info from PS files...
  */

  if (strcasecmp(filetype->super, "application") == 0 &&
      strcasecmp(filetype->type, "postscript") == 0)
    read_ps_job_ticket(con);

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "print_job: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the printer is accepting jobs...
  */

  if (dtype & CUPS_PRINTER_CLASS)
  {
    printer = FindClass(dest);
#ifdef AF_INET6
    if (con->http.hostaddr.addr.sa_family == AF_INET6)
      snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/classes/%s",
               ServerName, ntohs(con->http.hostaddr.ipv6.sin6_port), dest);
    else
#endif /* AF_INET6 */
    snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/classes/%s",
             ServerName, ntohs(con->http.hostaddr.ipv4.sin_port), dest);
  }
  else
  {
    printer = FindPrinter(dest);

#ifdef AF_INET6
    if (con->http.hostaddr.addr.sa_family == AF_INET6)
      snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/printers/%s",
               ServerName, ntohs(con->http.hostaddr.ipv6.sin6_port), dest);
    else
#endif /* AF_INET6 */
    snprintf(printer_uri, sizeof(printer_uri), "http://%s:%d/printers/%s",
             ServerName, ntohs(con->http.hostaddr.ipv4.sin_port), dest);
  }

  if (!printer->accepting)
  {
    LogMessage(L_INFO, "print_job: destination \'%s\' is not accepting jobs.",
               dest);
    send_ipp_error(con, IPP_NOT_ACCEPTING);
    return;
  }

 /*
  * Make sure we aren't over our limit...
  */

  if (NumJobs >= MaxJobs && MaxJobs)
    CleanJobs();

  if (NumJobs >= MaxJobs && MaxJobs)
  {
    LogMessage(L_INFO, "print_job: too many jobs - %d jobs, max jobs is %d.",
               NumJobs, MaxJobs);
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

  if (!check_quotas(con, printer))
  {
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * Create the job and set things up...
  */

  if ((attr = ippFindAttribute(con->request, "job-priority", IPP_TAG_INTEGER)) != NULL)
    priority = attr->values[0].integer;
  else
    ippAddInteger(con->request, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-priority",
                  priority = 50);

  if ((attr = ippFindAttribute(con->request, "job-name", IPP_TAG_NAME)) != NULL)
    title = attr->values[0].string.text;
  else
    ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
                 title = "Untitled");

  if ((job = AddJob(priority, printer->name)) == NULL)
  {
    LogMessage(L_ERROR, "print_job: unable to add job for destination \'%s\'!",
               dest);
    send_ipp_error(con, IPP_INTERNAL_ERROR);
    return;
  }

  job->dtype   = dtype;
  job->attrs   = con->request;
  con->request = NULL;

 /*
  * Copy the rest of the job info...
  */

  attr = ippFindAttribute(job->attrs, "requesting-user-name", IPP_TAG_NAME);

  if (con->username[0])
    SetString(&job->username, con->username);
  else if (attr != NULL)
  {
    LogMessage(L_DEBUG, "print_job: requesting-user-name = \'%s\'",
               attr->values[0].string.text);

    SetString(&job->username, attr->values[0].string.text);
  }
  else
    SetString(&job->username, "anonymous");

  if (attr == NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name",
                 NULL, job->username);
  else
  {
    attr->group_tag = IPP_TAG_JOB;
    SetString(&attr->name, "job-originating-user-name");
  }

 /*
  * Add remaining job attributes...
  */

  if ((attr = ippFindAttribute(job->attrs, "job-originating-host-name",
                               IPP_TAG_ZERO)) != NULL)
  {
   /*
    * Request contains a job-originating-host-name attribute; validate it...
    */

    if (attr->value_tag != IPP_TAG_NAME ||
        attr->num_values != 1 ||
        strcmp(con->http.hostname, "localhost") != 0)
    {
     /*
      * Can't override the value if we aren't connected via localhost.
      * Also, we can only have 1 value and it must be a name value.
      */

      int i;	/* Looping var */

      switch (attr->value_tag)
      {
        case IPP_TAG_STRING :
	case IPP_TAG_TEXTLANG :
	case IPP_TAG_NAMELANG :
	case IPP_TAG_TEXT :
	case IPP_TAG_NAME :
	case IPP_TAG_KEYWORD :
	case IPP_TAG_URI :
	case IPP_TAG_URISCHEME :
	case IPP_TAG_CHARSET :
	case IPP_TAG_LANGUAGE :
	case IPP_TAG_MIMETYPE :
	   /*
	    * Free old strings...
	    */

	    for (i = 0; i < attr->num_values; i ++)
	    {
	      free(attr->values[i].string.text);
	      attr->values[i].string.text = NULL;
	      if (attr->values[i].string.charset)
	      {
		free(attr->values[i].string.charset);
		attr->values[i].string.charset = NULL;
	      }
            }

	default :
            break;
      }

     /*
      * Use the default connection hostname instead...
      */

      attr->value_tag             = IPP_TAG_NAME;
      attr->num_values            = 1;
      attr->values[0].string.text = strdup(con->http.hostname);
    }

    attr->group_tag = IPP_TAG_JOB;
  }
  else
  {
   /*
    * No job-originating-host-name attribute, so use the hostname from
    * the connection...
    */

    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, 
        	 "job-originating-host-name", NULL, con->http.hostname);
  }

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  job->state = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_ENUM,
                             "job-state", IPP_JOB_PENDING);
  job->sheets = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                              "job-media-sheets-completed", 0);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL,
               printer_uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
               title);

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets", IPP_TAG_INTEGER)) == NULL)
    attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                         "job-k-octets", 0);

  if (stat(con->filename, &fileinfo))
    kbytes = 0;
  else
    kbytes = (fileinfo.st_size + 1023) / 1024;

  UpdateQuota(printer, job->username, 0, kbytes);
  attr->values[0].integer += kbytes;

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "time-at-creation",
                time(NULL));
  attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                       "time-at-processing", 0);
  attr->value_tag = IPP_TAG_NOVALUE;
  attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                       "time-at-completed", 0);
  attr->value_tag = IPP_TAG_NOVALUE;

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);
  if (attr == NULL)
    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                        "job-hold-until", NULL, "no-hold");

  if (attr != NULL && strcmp(attr->values[0].string.text, "no-hold") != 0 &&
      !(printer->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Hold job until specified time...
    */

    job->state->values[0].integer = IPP_JOB_HELD;
    SetJobHoldUntil(job->id, attr->values[0].string.text);
  }

  if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) ||
      Classification)
  {
   /*
    * Add job sheets options...
    */

    if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) == NULL)
    {
      LogMessage(L_DEBUG, "Adding default job-sheets values \"%s,%s\"...",
                 printer->job_sheets[0], printer->job_sheets[1]);

      attr = ippAddStrings(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-sheets",
                           2, NULL, NULL);
      attr->values[0].string.text = strdup(printer->job_sheets[0]);
      attr->values[1].string.text = strdup(printer->job_sheets[1]);
    }

    job->job_sheets = attr;

   /*
    * Enforce classification level if set...
    */

    if (Classification)
    {
      if (ClassifyOverride)
      {
        if (strcmp(attr->values[0].string.text, "none") == 0 &&
	    (attr->num_values == 1 ||
	     strcmp(attr->values[1].string.text, "none") == 0))
        {
	 /*
          * Force the leading banner to have the classification on it...
	  */

          SetString(&attr->values[0].string.text, Classification);
	}
	else if (attr->num_values == 2 &&
	         strcmp(attr->values[0].string.text, attr->values[1].string.text) != 0 &&
		 strcmp(attr->values[0].string.text, "none") != 0 &&
		 strcmp(attr->values[1].string.text, "none") != 0)
        {
	 /*
	  * Can't put two different security markings on the same document!
	  */

          SetString(&attr->values[1].string.text, attr->values[0].string.text);
	}
      }
      else if (strcmp(attr->values[0].string.text, Classification) != 0 &&
               (attr->num_values == 1 ||
	       strcmp(attr->values[1].string.text, Classification) != 0))
      {
       /*
        * Force the leading banner to have the classification on it...
	*/

        SetString(&attr->values[0].string.text, Classification);
      }
    }

   /*
    * Add the starting sheet...
    */

    if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
    {
      LogMessage(L_INFO, "Adding start banner page \"%s\" to job %d.",
        	 attr->values[0].string.text, job->id);

      kbytes = copy_banner(con, job, attr->values[0].string.text);

      UpdateQuota(printer, job->username, 0, kbytes);
    }
  }
  else if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL)
    job->sheets = attr;
   
 /*
  * Add the job file...
  */

  if (add_file(con, job, filetype, compression))
    return;

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  rename(con->filename, filename);
  ClearString(&con->filename);

 /*
  * See if we need to add the ending sheet...
  */

  if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) &&
      attr->num_values > 1)
  {
   /*
    * Yes...
    */

    LogMessage(L_INFO, "Adding end banner page \"%s\" to job %d.",
               attr->values[1].string.text, job->id);

    kbytes = copy_banner(con, job, attr->values[1].string.text);

    UpdateQuota(printer, job->username, 0, kbytes);
  }

 /*
  * Log and save the job...
  */

  LogMessage(L_INFO, "Job %d queued on \'%s\' by \'%s\'.", job->id,
             job->dest, job->username);
  LogMessage(L_DEBUG, "Job %d hold_until = %d", job->id, (int)job->hold_until);

  SaveJob(job->id);

 /*
  * Start the job if possible...
  */

  CheckJobs();

 /*
  * Fill in the response info...
  */

#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
    snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	     ntohs(con->http.hostaddr.ipv6.sin6_port), job->id);
  else
#endif /* AF_INET6 */
  snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	   ntohs(con->http.hostaddr.ipv4.sin_port), job->id);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL, job_uri);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state",
                job->state->values[0].integer);
  add_job_state_reasons(con, job);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'read_ps_job_ticket()' - Reads a job ticket embedded in a PS file.
 *
 * This function only gets called when printing a single PostScript
 * file using the Print-Job operation.  It doesn't work for Create-Job +
 * Send-File, since the job attributes need to be set at job creation
 * time for banners to work.  The embedded PS job ticket stuff is here
 * only to allow the Windows printer driver for CUPS to pass in JCL
 * options and IPP attributes which otherwise would be lost.
 *
 * The format of a PS job ticket is simple:
 *
 *     %cupsJobTicket: attr1=value1 attr2=value2 ... attrN=valueN
 *
 *     %cupsJobTicket: attr1=value1
 *     %cupsJobTicket: attr2=value2
 *     ...
 *     %cupsJobTicket: attrN=valueN
 *
 * Job ticket lines must appear immediately after the first line that
 * specifies PostScript format (%!PS-Adobe-3.0), and CUPS will stop
 * looking for job ticket info when it finds a line that does not begin
 * with "%cupsJobTicket:".
 *
 * The maximum length of a job ticket line, including the prefix, is
 * 255 characters to conform with the Adobe DSC.
 *
 * Read-only attributes are rejected with a notice to the error log in
 * case a malicious user tries anything.  Since the job ticket is read
 * prior to attribute validation in print_job(), job ticket attributes
 * will go through the same validation as IPP attributes...
 */

static void
read_ps_job_ticket(client_t *con)	/* I - Client connection */
{
  cups_file_t		*fp;		/* File to read from */
  char			line[256];	/* Line data */
  int			num_options;	/* Number of options */
  cups_option_t		*options;	/* Options */
  ipp_t			*ticket;	/* New attributes */
  ipp_attribute_t	*attr,		/* Current attribute */
			*attr2,		/* Job attribute */
			*prev2;		/* Previous job attribute */


 /*
  * First open the print file...
  */

  if ((fp = cupsFileOpen(con->filename, "rb")) == NULL)
  {
    LogMessage(L_ERROR, "read_ps_job_ticket: Unable to open PostScript print file - %s",
               strerror(errno));
    return;
  }

 /*
  * Skip the first line...
  */

  if (cupsFileGets(fp, line, sizeof(line)) == NULL)
  {
    LogMessage(L_ERROR, "read_ps_job_ticket: Unable to read from PostScript print file - %s",
               strerror(errno));
    cupsFileClose(fp);
    return;
  }

  if (strncmp(line, "%!PS-Adobe-", 11) != 0)
  {
   /*
    * Not a DSC-compliant file, so no job ticket info will be available...
    */

    cupsFileClose(fp);
    return;
  }

 /*
  * Read job ticket info from the file...
  */

  num_options = 0;
  options     = NULL;

  while (cupsFileGets(fp, line, sizeof(line)) != NULL)
  {
   /*
    * Stop at the first non-ticket line...
    */

    if (strncmp(line, "%cupsJobTicket:", 15) != 0)
      break;

   /*
    * Add the options to the option array...
    */

    num_options = cupsParseOptions(line + 15, num_options, &options);
  }

 /*
  * Done with the file; see if we have any options...
  */

  cupsFileClose(fp);

  if (num_options == 0)
    return;

 /*
  * OK, convert the options to an attribute list, and apply them to
  * the request...
  */

  ticket = ippNew();
  cupsEncodeOptions(ticket, num_options, options);

 /*
  * See what the user wants to change.
  */

  for (attr = ticket->attrs; attr != NULL; attr = attr->next)
  {
    if (attr->group_tag != IPP_TAG_JOB || !attr->name)
      continue;

    if (strcmp(attr->name, "job-originating-host-name") == 0 ||
        strcmp(attr->name, "job-originating-user-name") == 0 ||
	strcmp(attr->name, "job-media-sheets-completed") == 0 ||
	strcmp(attr->name, "job-k-octets") == 0 ||
	strcmp(attr->name, "job-id") == 0 ||
	strncmp(attr->name, "job-state", 9) == 0 ||
	strncmp(attr->name, "time-at-", 8) == 0)
      continue; /* Read-only attrs */

    if ((attr2 = ippFindAttribute(con->request, attr->name, IPP_TAG_ZERO)) != NULL)
    {
     /*
      * Some other value; first free the old value...
      */

      if (con->request->attrs == attr2)
      {
	con->request->attrs = attr2->next;
	prev2               = NULL;
      }
      else
      {
	for (prev2 = con->request->attrs; prev2 != NULL; prev2 = prev2->next)
	  if (prev2->next == attr2)
	  {
	    prev2->next = attr2->next;
	    break;
	  }
      }

      if (con->request->last == attr2)
        con->request->last = prev2;

      _ipp_free_attr(attr2);
    }

   /*
    * Add new option by copying it...
    */

    copy_attribute(con->request, attr, 0);
  }

 /*
  * Then free the attribute list and option array...
  */

  ippDelete(ticket);
  cupsFreeOptions(num_options, options);
}


/*
 * 'reject_jobs()' - Reject print jobs to a printer.
 */

static void
reject_jobs(client_t        *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Printer or class URI */
{
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  const char		*name;		/* Printer name */
  printer_t		*printer;	/* Printer data */
  ipp_attribute_t	*attr;		/* printer-state-message text */


  LogMessage(L_DEBUG2, "reject_jobs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "reject_jobs: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((name = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "reject_jobs: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Reject jobs sent to the printer...
  */

  if (dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  printer->accepting = 0;

  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) == NULL)
    strcpy(printer->state_message, "Rejecting Jobs");
  else
    strlcpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message));

  AddPrinterHistory(printer);

  if (dtype & CUPS_PRINTER_CLASS)
  {
    SaveAllClasses();

    LogMessage(L_INFO, "Class \'%s\' rejecting jobs (\'%s\').", name,
               con->username);
  }
  else
  {
    SaveAllPrinters();

    LogMessage(L_INFO, "Printer \'%s\' rejecting jobs (\'%s\').", name,
               con->username);
  }

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'release_job()' - Release a held print job.
 */

static void
release_job(client_t        *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			jobid;		/* Job ID */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  job_t			*job;		/* Job information */


  LogMessage(L_DEBUG2, "release_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/jobs/", 5) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "release_job: release request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "release_job: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "release_job: bad job-uri attribute \'%s\'!",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "release_job: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if job is "held"...
  */

  if (job->state->values[0].integer != IPP_JOB_HELD)
  {
   /*
    * Nope - return a "not possible" error...
    */

    LogMessage(L_ERROR, "release_job: job #%d is not held!", jobid);
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "release_job: \"%s\" not authorized to release job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

 /*
  * Reset the job-hold-until value to "no-hold"...
  */

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

  if (attr != NULL)
  {
    free(attr->values[0].string.text);
    attr->value_tag = IPP_TAG_KEYWORD;
    attr->values[0].string.text = strdup("no-hold");
  }

 /*
  * Release the job and return...
  */

  ReleaseJob(jobid);

  LogMessage(L_INFO, "Job %d was released by \'%s\'.", jobid, username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'restart_job()' - Restart an old print job.
 */

static void
restart_job(client_t        *con,	/* I - Client connection */
         ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			jobid;		/* Job ID */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  job_t			*job;		/* Job information */


  LogMessage(L_DEBUG2, "restart_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/jobs/", 5) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "restart_job: restart request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "restart_job: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "restart_job: bad job-uri attribute \'%s\'!",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "restart_job: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if job is in any of the "completed" states...
  */

  if (job->state->values[0].integer <= IPP_JOB_PROCESSING)
  {
   /*
    * Nope - return a "not possible" error...
    */

    LogMessage(L_ERROR, "restart_job: job #%d is not complete!", jobid);
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * See if we have retained the job files...
  */

  if (!JobFiles && job->state->values[0].integer > IPP_JOB_STOPPED)
  {
   /*
    * Nope - return a "not possible" error...
    */

    LogMessage(L_ERROR, "restart_job: job #%d cannot be restarted - no files!", jobid);
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "restart_job: \"%s\" not authorized to restart job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

 /*
  * Restart the job and return...
  */

  RestartJob(jobid);

  LogMessage(L_INFO, "Job %d was restarted by \'%s\'.", jobid, username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'send_document()' - Send a file to a printer or class.
 */

static void
send_document(client_t        *con,	/* I - Client connection */
	      ipp_attribute_t *uri)	/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  int			jobid;		/* Job ID number */
  job_t			*job;		/* Current job */
  char			job_uri[HTTP_MAX_URI],
					/* Job URI */
			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  mime_type_t		*filetype;	/* Type of file */
  char			super[MIME_MAX_SUPER],
					/* Supertype of file */
			type[MIME_MAX_TYPE],
					/* Subtype of file */
			mimetype[MIME_MAX_SUPER + MIME_MAX_TYPE + 2];
					/* Textual name of mime type */
  char			filename[1024];	/* Job filename */
  printer_t		*printer;	/* Current printer */
  struct stat		fileinfo;	/* File information */
  int			kbytes;		/* Size of file */
  int			compression;	/* Type of compression */


  LogMessage(L_DEBUG2, "send_document(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/jobs/", 6) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "send_document: print request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "send_document: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "send_document: bad job-uri attribute \'%s\'!",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "send_document: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "send_document: \"%s\" not authorized to send document for job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * only supports "none" and "gzip".
  */

  compression = CUPS_FILE_NONE;

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL)
  {
    if (strcmp(attr->values[0].string.text, "none")
#ifdef HAVE_LIBZ
        && strcmp(attr->values[0].string.text, "gzip")
#endif /* HAVE_LIBZ */
      )
    {
      LogMessage(L_ERROR, "print_job: Unsupported compression \"%s\"!",
        	 attr->values[0].string.text);
      send_ipp_error(con, IPP_ATTRIBUTES);
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
	           "compression", NULL, attr->values[0].string.text);
      return;
    }

#ifdef HAVE_LIBZ
    if (!strcmp(attr->values[0].string.text, "gzip"))
      compression = CUPS_FILE_GZIP;
#endif /* HAVE_LIBZ */
  }

 /*
  * Do we have a file to print?
  */

  if (!con->filename)
  {
    LogMessage(L_ERROR, "send_document: No file!?!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format", IPP_TAG_MIMETYPE)) != NULL)
  {
   /*
    * Grab format from client...
    */

    if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
    {
      LogMessage(L_ERROR, "send_document: could not scan type \'%s\'!",
	         format->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
  }
  else
  {
   /*
    * No document format attribute?  Auto-type it!
    */

    strcpy(super, "application");
    strcpy(type, "octet-stream");
  }

  if (strcmp(super, "application") == 0 &&
      strcmp(type, "octet-stream") == 0)
  {
   /*
    * Auto-type the file...
    */

    LogMessage(L_DEBUG, "send_document: auto-typing file...");

    filetype = mimeFileType(MimeDatabase, con->filename, &compression);

    if (filetype != NULL)
    {
     /*
      * Replace the document-format attribute value with the auto-typed one.
      */

      snprintf(mimetype, sizeof(mimetype), "%s/%s", filetype->super,
               filetype->type);

      if (format != NULL)
      {
	free(format->values[0].string.text);
	format->values[0].string.text = strdup(mimetype);
      }
      else
        ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
	             "document-format", NULL, mimetype);
    }
    else
      filetype = mimeType(MimeDatabase, super, type);
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype == NULL)
  {
    LogMessage(L_ERROR, "send_document: Unsupported format \'%s/%s\'!",
	       super, type);
    LogMessage(L_INFO, "Hint: Do you have the raw file printing rules enabled?");
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);

    if (format)
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);

    return;
  }

  LogMessage(L_DEBUG, "send_document: request file type is %s/%s.",
	     filetype->super, filetype->type);

 /*
  * Add the file to the job...
  */

  if (add_file(con, job, filetype, compression))
    return;

  if (job->dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(job->dest);
  else
    printer = FindPrinter(job->dest);

  if (stat(con->filename, &fileinfo))
    kbytes = 0;
  else
    kbytes = (fileinfo.st_size + 1023) / 1024;

  UpdateQuota(printer, job->username, 0, kbytes);

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets", IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer += kbytes;

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  rename(con->filename, filename);

  ClearString(&con->filename);

  LogMessage(L_INFO, "File of type %s/%s queued in job #%d by \'%s\'.",
             filetype->super, filetype->type, job->id, job->username);

 /*
  * Start the job if this is the last document...
  */

  if ((attr = ippFindAttribute(con->request, "last-document", IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
  {
   /*
    * See if we need to add the ending sheet...
    */

    if (printer != NULL &&
        !(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) &&
        (attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL &&
        attr->num_values > 1)
    {
     /*
      * Yes...
      */

      LogMessage(L_INFO, "Adding end banner page \"%s\" to job %d.",
        	 attr->values[1].string.text, job->id);

      kbytes = copy_banner(con, job, attr->values[1].string.text);

      UpdateQuota(printer, job->username, 0, kbytes);
    }

    if (job->state->values[0].integer == IPP_JOB_STOPPED)
      job->state->values[0].integer = IPP_JOB_PENDING;
    else if (job->state->values[0].integer == IPP_JOB_HELD)
    {
      if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
	attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

      if (attr == NULL || strcmp(attr->values[0].string.text, "no-hold") == 0)
	job->state->values[0].integer = IPP_JOB_PENDING;
    }

    SaveJob(job->id);
    CheckJobs();
  }
  else
  {
    if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
      attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

    if (attr == NULL || strcmp(attr->values[0].string.text, "no-hold") == 0)
    {
      job->state->values[0].integer = IPP_JOB_HELD;
      job->hold_until               = time(NULL) + 60;
      SaveJob(job->id);
    }
  }

 /*
  * Fill in the response info...
  */

#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
    snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	     ntohs(con->http.hostaddr.ipv6.sin6_port), job->id);
  else
#endif /* AF_INET6 */
  snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	   ntohs(con->http.hostaddr.ipv4.sin_port), job->id);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL,
               job_uri);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state",
                job->state->values[0].integer);
  add_job_state_reasons(con, job);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'send_ipp_error()' - Send an error status back to the IPP client.
 */

static void
send_ipp_error(client_t     *con,	/* I - Client connection */
               ipp_status_t status)	/* I - IPP status code */
{
  LogMessage(L_DEBUG2, "send_ipp_error(%d, %x)\n", con->http.fd, status);

  LogMessage(L_DEBUG, "Sending error: %s", ippErrorString(status));

  con->response->request.status.status_code = status;

  if (ippFindAttribute(con->response, "attributes-charset", IPP_TAG_ZERO) == NULL)
    ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                 "attributes-charset", NULL, DefaultCharset);

  if (ippFindAttribute(con->response, "attributes-natural-language",
                       IPP_TAG_ZERO) == NULL)
    ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                 "attributes-natural-language", NULL, DefaultLanguage);
}


/*
 * 'set_default()' - Set the default destination...
 */

static void
set_default(client_t        *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Printer URI */
{
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  const char		*name;		/* Printer name */


  LogMessage(L_DEBUG2, "set_default(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "set_default: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((name = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "set_default: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Set it as the default...
  */

  if (dtype & CUPS_PRINTER_CLASS)
    DefaultPrinter = FindClass(name);
  else
    DefaultPrinter = FindPrinter(name);

  SaveAllPrinters();
  SaveAllClasses();

  LogMessage(L_INFO, "Default destination set to \'%s\' by \'%s\'.", name,
             con->username);

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'set_job_attrs()' - Set job attributes.
 */

static void
set_job_attrs(client_t        *con,	/* I - Client connection */
	      ipp_attribute_t *uri)	/* I - Job URI */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*attr2,		/* Job attribute */
			*prev2;		/* Previous job attribute */
  int			jobid;		/* Job ID */
  job_t			*job;		/* Current job */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  LogMessage(L_DEBUG2, "set_job_attrs(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Start with "everything is OK" status...
  */

  con->response->request.status.status_code = IPP_OK;

 /*
  * See if we have a job URI or a printer URI...
  */

  if (strcmp(uri->name, "printer-uri") == 0)
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id", IPP_TAG_INTEGER)) == NULL)
    {
      LogMessage(L_ERROR, "set_job_attrs: got a printer-uri attribute but no job-id!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);
 
    if (strncmp(resource, "/jobs/", 6) != 0)
    {
     /*
      * Not a valid URI!
      */

      LogMessage(L_ERROR, "set_job_attrs: bad job-uri attribute \'%s\'!\n",
                 uri->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = FindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    LogMessage(L_ERROR, "set_job_attrs: job #%d doesn't exist!", jobid);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * See if the job has been completed...
  */

  if (job->state->values[0].integer > IPP_JOB_STOPPED)
  {
   /*
    * Return a "not-possible" error...
    */

    LogMessage(L_ERROR, "set_job_attrs: job #%d is finished and cannot be altered!", jobid);
    send_ipp_error(con, IPP_NOT_POSSIBLE);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(con, job->username, username, sizeof(username)))
  {
    LogMessage(L_ERROR, "set_job_attrs: \"%s\" not authorized to alter job id %d owned by \"%s\"!",
               username, jobid, job->username);
    send_ipp_error(con, IPP_FORBIDDEN);
    return;
  }

 /*
  * See what the user wants to change.
  */

  for (attr = con->request->attrs; attr != NULL; attr = attr->next)
  {
    if (attr->group_tag != IPP_TAG_JOB || !attr->name)
      continue;

    if (!strcmp(attr->name, "attributes-charset") ||
	!strcmp(attr->name, "attributes-natural-language") ||
	!strcmp(attr->name, "document-compression") ||
	!strcmp(attr->name, "document-format") ||
	!strcmp(attr->name, "job-detailed-status-messages") ||
	!strcmp(attr->name, "job-document-access-errors") ||
	!strcmp(attr->name, "job-id") ||
	!strcmp(attr->name, "job-k-octets") ||
        !strcmp(attr->name, "job-originating-host-name") ||
        !strcmp(attr->name, "job-originating-user-name") ||
	!strcmp(attr->name, "job-printer-up-time") ||
	!strcmp(attr->name, "job-printer-uri") ||
	!strcmp(attr->name, "job-sheets") ||
	!strcmp(attr->name, "job-state-message") ||
	!strcmp(attr->name, "job-state-reasons") ||
	!strcmp(attr->name, "job-uri") ||
	!strcmp(attr->name, "number-of-documents") ||
	!strcmp(attr->name, "number-of-intervening-jobs") ||
	!strcmp(attr->name, "output-device-assigned") ||
	!strncmp(attr->name, "date-time-at-", 13) ||
	!strncmp(attr->name, "job-impressions", 15) ||
	!strncmp(attr->name, "job-k-octets", 12) ||
	!strncmp(attr->name, "job-media-sheets", 16) ||
	!strncmp(attr->name, "time-at-", 8))
    {
     /*
      * Read-only attrs!
      */

      send_ipp_error(con, IPP_ATTRIBUTES_NOT_SETTABLE);

      if ((attr2 = copy_attribute(con->response, attr, 0)) != NULL)
        attr2->group_tag = IPP_TAG_UNSUPPORTED_GROUP;

      continue;
    }

    if (!strcmp(attr->name, "job-priority"))
    {
     /*
      * Change the job priority...
      */

      if (attr->value_tag != IPP_TAG_INTEGER)
      {
	send_ipp_error(con, IPP_REQUEST_VALUE);

	if ((attr2 = copy_attribute(con->response, attr, 0)) != NULL)
          attr2->group_tag = IPP_TAG_UNSUPPORTED_GROUP;
      }
      else if (job->state->values[0].integer >= IPP_JOB_PROCESSING)
      {
	send_ipp_error(con, IPP_NOT_POSSIBLE);
	return;
      }
      else if (con->response->request.status.status_code == IPP_OK)
        SetJobPriority(jobid, attr->values[0].integer);
    }
    else if (!strcmp(attr->name, "job-state"))
    {
     /*
      * Change the job state...
      */

      if (attr->value_tag != IPP_TAG_ENUM)
      {
	send_ipp_error(con, IPP_REQUEST_VALUE);

	if ((attr2 = copy_attribute(con->response, attr, 0)) != NULL)
          attr2->group_tag = IPP_TAG_UNSUPPORTED_GROUP;
      }
      else
      {
        switch (attr->values[0].integer)
	{
	  case IPP_JOB_PENDING :
	  case IPP_JOB_HELD :
	      if (job->state->values[0].integer > IPP_JOB_HELD)
	      {
		send_ipp_error(con, IPP_NOT_POSSIBLE);
		return;
	      }
              else if (con->response->request.status.status_code == IPP_OK)
		job->state->values[0].integer = attr->values[0].integer;
	      break;

	  case IPP_JOB_PROCESSING :
	  case IPP_JOB_STOPPED :
	      if (job->state->values[0].integer != attr->values[0].integer)
	      {
		send_ipp_error(con, IPP_NOT_POSSIBLE);
		return;
	      }
	      break;

	  case IPP_JOB_CANCELLED :
	  case IPP_JOB_ABORTED :
	  case IPP_JOB_COMPLETED :
	      if (job->state->values[0].integer > IPP_JOB_PROCESSING)
	      {
		send_ipp_error(con, IPP_NOT_POSSIBLE);
		return;
	      }
              else if (con->response->request.status.status_code == IPP_OK)
	      {
                CancelJob(job->id, 0);

		if (JobHistory)
		{
                  job->state->values[0].integer = attr->values[0].integer;
		  SaveJob(job->id);
		}
	      }
	      break;
	}
      }
    }
    else if (con->response->request.status.status_code != IPP_OK)
      continue;
    else if ((attr2 = ippFindAttribute(job->attrs, attr->name, IPP_TAG_ZERO)) != NULL)
    {
     /*
      * Some other value; first free the old value...
      */

      if (job->attrs->attrs == attr2)
      {
	job->attrs->attrs = attr2->next;
	prev2             = NULL;
      }
      else
      {
	for (prev2 = job->attrs->attrs; prev2 != NULL; prev2 = prev2->next)
	  if (prev2->next == attr2)
	  {
	    prev2->next = attr2->next;
	    break;
	  }
      }

      if (job->attrs->last == attr2)
        job->attrs->last = prev2;

      _ipp_free_attr(attr2);

     /*
      * Then copy the attribute...
      */

      copy_attribute(job->attrs, attr, 0);

     /*
      * See if the job-name or job-hold-until is being changed.
      */

      if (strcmp(attr->name, "job-hold-until") == 0)
      {
        SetJobHoldUntil(job->id, attr->values[0].string.text);

	if (strcmp(attr->values[0].string.text, "no-hold") == 0)
	  ReleaseJob(job->id);
	else
	  HoldJob(job->id);
      }
    }
    else if (attr->value_tag == IPP_TAG_DELETEATTR)
    {
     /*
      * Delete the attribute...
      */

      for (attr2 = job->attrs->attrs, prev2 = NULL;
           attr2 != NULL;
	   prev2 = attr2, attr2 = attr2->next)
        if (attr2->name && strcmp(attr2->name, attr->name) == 0)
	  break;

      if (attr2)
      {
        if (prev2)
	  prev2->next = attr2->next;
	else
	  job->attrs->attrs = attr2->next;

        if (attr2 == job->attrs->last)
	  job->attrs->last = prev2;

        _ipp_free_attr(attr2);
      }
    }
    else
    {
     /*
      * Add new option by copying it...
      */

      copy_attribute(job->attrs, attr, 0);
    }
  }

 /*
  * Save the job...
  */

  SaveJob(job->id);

 /*
  * Start jobs if possible...
  */

  CheckJobs();
}


/*
 * 'start_printer()' - Start a printer.
 */

static void
start_printer(client_t        *con,	/* I - Client connection */
              ipp_attribute_t *uri)	/* I - Printer URI */
{
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  const char		*name;		/* Printer name */
  printer_t		*printer;	/* Printer data */


  LogMessage(L_DEBUG2, "start_printer(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "start_printer: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((name = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "start_printer: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Start the printer...
  */

  if (dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  printer->state_message[0] = '\0';

  StartPrinter(printer, 1);

  if (dtype & CUPS_PRINTER_CLASS)
    LogMessage(L_INFO, "Class \'%s\' started by \'%s\'.", name,
               con->username);
    LogMessage(L_INFO, "Printer \'%s\' started by \'%s\'.", name,
               con->username);

  CheckJobs();

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'stop_printer()' - Stop a printer.
 */

static void
stop_printer(client_t        *con,	/* I - Client connection */
             ipp_attribute_t *uri)	/* I - Printer URI */
{
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  const char		*name;		/* Printer name */
  printer_t		*printer;	/* Printer data */
  ipp_attribute_t	*attr;		/* printer-state-message attribute */


  LogMessage(L_DEBUG2, "stop_printer(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Was this operation called from the correct URI?
  */

  if (strncmp(con->uri, "/admin/", 7) != 0)
  {
    LogMessage(L_ERROR, "stop_printer: admin request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((name = ValidateDest(host, resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "stop_printer: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Stop the printer...
  */

  if (dtype & CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) == NULL)
    strcpy(printer->state_message, "Paused");
  else
  {
    strlcpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message));
  }

  StopPrinter(printer, 1);

  if (dtype & CUPS_PRINTER_CLASS)
    LogMessage(L_INFO, "Class \'%s\' stopped by \'%s\'.", name,
               con->username);
  else
    LogMessage(L_INFO, "Printer \'%s\' stopped by \'%s\'.", name,
               con->username);

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'validate_job()' - Validate printer options and destination.
 */

static void
validate_job(client_t        *con,	/* I - Client connection */
	     ipp_attribute_t *uri)	/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  char			super[MIME_MAX_SUPER],
					/* Supertype of file */
			type[MIME_MAX_TYPE];
					/* Subtype of file */


  LogMessage(L_DEBUG2, "validate_job(%d, %s)\n", con->http.fd,
             uri->values[0].string.text);

 /*
  * Verify that the POST operation was done to a valid URI.
  */

  if (strncmp(con->uri, "/classes/", 9) != 0 &&
      strncmp(con->uri, "/printers/", 10) != 0)
  {
    LogMessage(L_ERROR, "validate_job: request on bad resource \'%s\'!",
               con->uri);
    send_ipp_error(con, IPP_NOT_AUTHORIZED);
    return;
  }

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL &&
      strcmp(attr->values[0].string.text, "none") == 0)
  {
    LogMessage(L_ERROR, "validate_job: Unsupported compression attribute %s!",
               attr->values[0].string.text);
    send_ipp_error(con, IPP_ATTRIBUTES);
    ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
	         "compression", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format", IPP_TAG_MIMETYPE)) != NULL)
  {
    if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
    {
      LogMessage(L_ERROR, "validate_job: could not scan type \'%s\'!\n",
		 format->values[0].string.text);
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }

    if ((strcmp(super, "application") != 0 ||
	 strcmp(type, "octet-stream") != 0) &&
	mimeType(MimeDatabase, super, type) == NULL)
    {
      LogMessage(L_ERROR, "validate_job: Unsupported format \'%s\'!\n",
		 format->values[0].string.text);
      LogMessage(L_INFO, "Hint: Do you have the raw file printing rules enabled?");
      send_ipp_error(con, IPP_DOCUMENT_FORMAT);
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);
      return;
    }
  }

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if (ValidateDest(host, resource, &dtype) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "validate_job: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'validate_user()' - Validate the user for the request.
 */

static int				/* O - 1 if permitted, 0 otherwise */
validate_user(client_t   *con,		/* I - Client connection */
              const char *owner,	/* I - Owner of job/resource */
              char       *username,	/* O - Authenticated username */
	      int        userlen)	/* I - Length of username */
{
  int			i, j;		/* Looping vars */
  ipp_attribute_t	*attr;		/* requesting-user-name attribute */
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */
  char			junk[33];	/* MD5 password (not used) */


  LogMessage(L_DEBUG2, "validate_user(%d, %s, %s, %d)\n", con->http.fd,
             owner, username, userlen);

 /*
  * Validate input...
  */

  if (con == NULL || owner == NULL || username == NULL || userlen <= 0)
    return (0);

 /*
  * Get the best authenticated username that is available.
  */

  if (con->username[0])
    strlcpy(username, con->username, userlen);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    strlcpy(username, attr->values[0].string.text, userlen);
  else
    strlcpy(username, "anonymous", userlen);

 /*
  * Check the username against the owner...
  */

  if (strcasecmp(username, owner) != 0 && strcasecmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    for (i = 0, j = 0, group = NULL; i < NumSystemGroups; i ++)
    {
      group = getgrnam(SystemGroups[i]);
      endgrent();

      if (group != NULL)
      {
	for (j = 0; group->gr_mem[j]; j ++)
          if (strcasecmp(username, group->gr_mem[j]) == 0)
	    break;

        if (group->gr_mem[j])
	  break;
      }
      else
	j = 0;
    }

    if (user == NULL || group == NULL ||
        (group->gr_mem[j] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...  Check for a user and group in the MD5 password
      * file...
      */

      for (i = 0; i < NumSystemGroups; i ++)
        if (GetMD5Passwd(username, SystemGroups[i], junk) != NULL)
	  return (1);

     /*
      * Nope, not an MD5 user, either.  Return 0 indicating no-go...
      */

      return (0);
    }
  }

  return (1);
}


/*
 * End of "$Id: ipp.c,v 1.127.2.70 2003/05/13 14:50:54 mike Exp $".
 */
