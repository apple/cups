/*
 * "$Id: ipp.c,v 1.127.2.5 2002/01/09 17:04:15 mike Exp $"
 *
 *   IPP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
 *   create_job()                - Print a file to a printer or class.
 *   copy_banner()               - Copy a banner file to the requests directory
 *                                 for the specified job.
 *   copy_file()                 - Copy a PPD file or interface script...
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
 *   print_job()                 - Print a file to a printer or class.
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
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */


/*
 * Local functions...
 */

static void	accept_jobs(client_t *con, ipp_attribute_t *uri);
static void	add_class(client_t *con, ipp_attribute_t *uri);
static int	add_file(client_t *con, job_t *job, mime_type_t *filetype);
static void	add_job_state_reasons(client_t *con, job_t *job);
static void	add_printer(client_t *con, ipp_attribute_t *uri);
static void	add_printer_state_reasons(client_t *con, printer_t *p);
static void	add_queued_job_count(client_t *con, printer_t *p);
static void	cancel_all_jobs(client_t *con, ipp_attribute_t *uri);
static void	cancel_job(client_t *con, ipp_attribute_t *uri);
static int	check_quotas(client_t *con, printer_t *p);
static void	copy_attribute(ipp_t *to, ipp_attribute_t *attr,
		               int quickcopy);
static void	copy_attrs(ipp_t *to, ipp_t *from, ipp_attribute_t *req,
		           ipp_tag_t group);
static int	copy_banner(client_t *con, job_t *job, const char *name);
static int	copy_file(const char *from, const char *to);
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
static void	print_job(client_t *con, ipp_attribute_t *uri);
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

void
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

  SendHeader(con, HTTP_OK, "application/ipp");

  con->http.data_encoding = HTTP_ENCODE_LENGTH;
  con->http.data_remaining = ippLength(con->response);

  httpPrintf(HTTP(con), "Content-Length: %d\r\n\r\n",
             con->http.data_remaining);

  FD_SET(con->http.fd, &OutputSet);
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

  if (dtype == CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  printer->accepting        = 1;
  printer->state_message[0] = '\0';

  if (dtype == CUPS_PRINTER_CLASS)
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
      DeletePrinter(pclass);

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
  {
    strncpy(pclass->location, attr->values[0].string.text, sizeof(pclass->location) - 1);
    pclass->location[sizeof(pclass->location) - 1] = '\0';
  }

  if ((attr = ippFindAttribute(con->request, "printer-info", IPP_TAG_TEXT)) != NULL)
  {
    strncpy(pclass->info, attr->values[0].string.text, sizeof(pclass->info) - 1);
    pclass->info[sizeof(pclass->info) - 1] = '\0';
  }

  if ((attr = ippFindAttribute(con->request, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    LogMessage(L_INFO, "Setting %s printer-is-accepting-jobs to %d (was %d.)",
               pclass->name, attr->values[0].boolean, pclass->accepting);

    pclass->accepting = attr->values[0].boolean;
  }
  if ((attr = ippFindAttribute(con->request, "printer-state", IPP_TAG_ENUM)) != NULL)
  {
    LogMessage(L_INFO, "Setting %s printer-state to %d (was %d.)", pclass->name,
               attr->values[0].integer, pclass->state);

    if (pclass->state == IPP_PRINTER_STOPPED &&
        attr->values[0].integer != IPP_PRINTER_STOPPED)
      pclass->state = IPP_PRINTER_IDLE;
    else if (pclass->state != IPP_PRINTER_STOPPED &&
             attr->values[0].integer == IPP_PRINTER_STOPPED)
    {
      if (pclass->state == IPP_PRINTER_PROCESSING)
        StopJob(((job_t *)pclass->job)->id);

      pclass->state = IPP_PRINTER_STOPPED;
    }

    pclass->browse_time = 0;
  }
  if ((attr = ippFindAttribute(con->request, "printer-state-message", IPP_TAG_TEXT)) != NULL)
  {
    strncpy(pclass->state_message, attr->values[0].string.text,
            sizeof(pclass->state_message) - 1);
    pclass->state_message[sizeof(pclass->state_message) - 1] = '\0';
  }
  if ((attr = ippFindAttribute(con->request, "job-sheets-default", IPP_TAG_ZERO)) != NULL &&
      !Classification[0])
  {
    strncpy(pclass->job_sheets[0], attr->values[0].string.text,
            sizeof(pclass->job_sheets[0]) - 1);
    if (attr->num_values > 1)
      strncpy(pclass->job_sheets[1], attr->values[1].string.text,
              sizeof(pclass->job_sheets[1]) - 1);
    else
      strcpy(pclass->job_sheets[1], "none");
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
    FreeQuotas(pclass);
    pclass->quota_period = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-k-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    FreeQuotas(pclass);
    pclass->k_limit = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-page-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
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

      if (dtype == CUPS_PRINTER_CLASS)
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

  if (modify)
    LogMessage(L_INFO, "Class \'%s\' modified by \'%s\'.", pclass->name,
               con->username);
  else
    LogMessage(L_INFO, "New class \'%s\' added by \'%s\'.", pclass->name,
               con->username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_file()' - Add a file to a job.
 */

static int				/* O - 0 on success, -1 on error */
add_file(client_t    *con,		/* I - Connection to client */
         job_t       *job,		/* I - Job to add to */
         mime_type_t *filetype)		/* I - Type of file */
{
  mime_type_t	**filetypes;		/* New filetypes array... */


  LogMessage(L_DEBUG2, "add_file(%d, %d, %s/%s)\n", con->http.fd,
             job->id, filetype->super, filetype->type);

 /*
  * Add the file to the job...
  */

  if (job->num_files == 0)
    filetypes = (mime_type_t **)malloc(sizeof(mime_type_t *));
  else
    filetypes = (mime_type_t **)realloc(job->filetypes,
                                       (job->num_files + 1) *
				       sizeof(mime_type_t));

  if (filetypes == NULL)
  {
    CancelJob(job->id, 1);
    LogMessage(L_ERROR, "add_file: unable to allocate memory for file types!");
    send_ipp_error(con, IPP_INTERNAL_ERROR);
    return (-1);
  }

  job->filetypes                 = filetypes;
  job->filetypes[job->num_files] = filetype;

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
#ifdef HAVE_LIBZ
  gzFile		fp;		/* Script/PPD file */
#else
  FILE			*fp;		/* Script/PPD file */
#endif /* HAVE_LIBZ */
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
      DeletePrinter(printer);

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
  {
    strncpy(printer->location, attr->values[0].string.text, sizeof(printer->location) - 1);
    printer->location[sizeof(printer->location) - 1] = '\0';
  }

  if ((attr = ippFindAttribute(con->request, "printer-info", IPP_TAG_TEXT)) != NULL)
  {
    strncpy(printer->info, attr->values[0].string.text, sizeof(printer->info) - 1);
    printer->info[sizeof(printer->info) - 1] = '\0';
  }

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

    if (strcmp(method, "file") != 0)
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

    strncpy(printer->device_uri, attr->values[0].string.text,
            sizeof(printer->device_uri) - 1);
    printer->device_uri[sizeof(printer->device_uri) - 1] = '\0';
  }

  if ((attr = ippFindAttribute(con->request, "printer-is-accepting-jobs", IPP_TAG_BOOLEAN)) != NULL)
  {
    LogMessage(L_INFO, "Setting %s printer-is-accepting-jobs to %d (was %d.)",
               printer->name, attr->values[0].boolean, printer->accepting);

    printer->accepting = attr->values[0].boolean;
  }
  if ((attr = ippFindAttribute(con->request, "printer-state", IPP_TAG_ENUM)) != NULL)
  {
    LogMessage(L_INFO, "Setting %s printer-state to %d (was %d.)", printer->name,
               attr->values[0].integer, printer->state);

    if (printer->state == IPP_PRINTER_STOPPED &&
        attr->values[0].integer != IPP_PRINTER_STOPPED)
      printer->state = IPP_PRINTER_IDLE;
    else if (printer->state != IPP_PRINTER_STOPPED &&
             attr->values[0].integer == IPP_PRINTER_STOPPED)
    {
      if (printer->state == IPP_PRINTER_PROCESSING)
        StopJob(((job_t *)printer->job)->id);

      printer->state = IPP_PRINTER_STOPPED;
    }

    printer->browse_time = 0;
  }
  if ((attr = ippFindAttribute(con->request, "printer-state-message", IPP_TAG_TEXT)) != NULL)
  {
    strncpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message) - 1);
    printer->state_message[sizeof(printer->state_message) - 1] = '\0';
  }
  if ((attr = ippFindAttribute(con->request, "job-sheets-default", IPP_TAG_ZERO)) != NULL &&
      !Classification[0])
  {
    strncpy(printer->job_sheets[0], attr->values[0].string.text,
            sizeof(printer->job_sheets[0]) - 1);
    if (attr->num_values > 1)
      strncpy(printer->job_sheets[1], attr->values[1].string.text,
              sizeof(printer->job_sheets[1]) - 1);
    else
      strcpy(printer->job_sheets[1], "none");
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
    FreeQuotas(printer);
    printer->quota_period = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-k-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    FreeQuotas(printer);
    printer->k_limit = attr->values[0].integer;
  }
  if ((attr = ippFindAttribute(con->request, "job-page-limit",
                               IPP_TAG_INTEGER)) != NULL)
  {
    FreeQuotas(printer);
    printer->page_limit = attr->values[0].integer;
  }

 /*
  * See if we have all required attributes...
  */

  if (printer->device_uri[0] == '\0')
    strcpy(printer->device_uri, "file:/dev/null");

 /*
  * See if we have an interface script or PPD file attached to the request...
  */

  if (con->filename[0])
  {
    strncpy(srcfile, con->filename, sizeof(srcfile) - 1);
    srcfile[sizeof(srcfile) - 1] = '\0';
  }
  else if ((attr = ippFindAttribute(con->request, "ppd-name", IPP_TAG_NAME)) != NULL)
  {
    if (strcmp(attr->values[0].string.text, "raw") == 0)
      strcpy(srcfile, "raw");
    else
      snprintf(srcfile, sizeof(srcfile), "%s/model/%s", DataDir,
               attr->values[0].string.text);
  }
  else
    srcfile[0] = '\0';

  LogMessage(L_DEBUG, "add_printer: srcfile = \"%s\"", srcfile);

  if (strcmp(srcfile, "raw") == 0)
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
#ifdef HAVE_LIBZ
  else if (srcfile[0] && (fp = gzopen(srcfile, "rb")) != NULL)
#else
  else if (srcfile[0] && (fp = fopen(srcfile, "rb")) != NULL)
#endif /* HAVE_LIBZ */
  {
   /*
    * Yes; get the first line from it...
    */

    line[0] = '\0';
#ifdef HAVE_LIBZ
    gzgets(fp, line, sizeof(line));
    gzclose(fp);
#else
    fgets(line, sizeof(line), fp);
    fclose(fp);
#endif /* HAVE_LIBZ */

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
   /*
    * Stop the current job and then restart it below...
    */

    StopJob(((job_t *)printer->job)->id);
  }

  CheckJobs();

  if (modify)
    LogMessage(L_INFO, "Printer \'%s\' modified by \'%s\'.", printer->name,
               con->username);
  else
    LogMessage(L_INFO, "New printer \'%s\' added by \'%s\'.", printer->name,
               con->username);

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

  switch (p->state)
  {
    case IPP_PRINTER_STOPPED :
        ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	             "printer-state-reasons", NULL, "paused");
        break;

    default :
        ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
	             "printer-state-reasons", NULL, "none");
        break;
  }
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
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  printer_t		*printer;	/* Current printer */


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
  * And if the destination is valid...
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port,
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

    for (printer = Printers; printer; printer = printer->next)
    {
      CancelJobs(printer->name);
      LogMessage(L_INFO, "All jobs on \'%s\' were cancelled by \'%s\'.",
                 printer->name, con->username);
    }
  }
  else
  {
   /*
    * Cancel all of the jobs on the named printer...
    */

    CancelJobs(dest);
    LogMessage(L_INFO, "All jobs on \'%s\' were cancelled by \'%s\'.", dest,
               con->username);
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
  int		i;		/* Looping var */
  ipp_attribute_t *attr;	/* Current attribute */
  char		username[33];	/* Username */
  quota_t	*q;		/* Quota data */


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
  {
    strncpy(username, con->username, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else if (attr != NULL)
  {
    LogMessage(L_DEBUG, "check_quotas: requesting-user-name = \'%s\'",
               attr->values[0].string.text);

    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
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
    for (i = 0; i < p->num_users; i ++)
      if (strcasecmp(username, p->users[i]) == 0)
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

static void
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
        ippAddSeparator(to);
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
}


/*
 * 'copy_attrs()' - Copy attributes from one request to another.
 */

static void
copy_attrs(ipp_t           *to,		/* I - Destination request */
           ipp_t           *from,	/* I - Source request */
           ipp_attribute_t *req,	/* I - Requested attributes */
	   ipp_tag_t       group)	/* I - Group to copy */
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

    copy_attribute(to, fromattr, IPP_TAG_COPY);
  }
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

  if (dtype == CUPS_PRINTER_CLASS)
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

  strncpy(job->title, title, sizeof(job->title) - 1);

  attr = ippFindAttribute(job->attrs, "requesting-user-name", IPP_TAG_NAME);

  if (con->username[0])
  {
    strncpy(job->username, con->username, sizeof(job->username) - 1);
    job->username[sizeof(job->username) - 1] = '\0';
  }
  else if (attr != NULL)
  {
    LogMessage(L_DEBUG, "create_job: requesting-user-name = \'%s\'",
               attr->values[0].string.text);

    strncpy(job->username, attr->values[0].string.text,
            sizeof(job->username) - 1);
    job->username[sizeof(job->username) - 1] = '\0';
  }
  else
    strcpy(job->username, "anonymous");

  if (attr == NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name",
                 NULL, job->username);
  else
  {
    attr->group_tag = IPP_TAG_JOB;
    free(attr->name);
    attr->name = strdup("job-originating-user-name");
  }

  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, 
               "job-originating-host-name", NULL, con->http.hostname);
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

  if (!(printer->type & CUPS_PRINTER_REMOTE) || Classification[0])
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

    if (Classification[0])
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

          free(attr->values[0].string.text);
	  attr->values[0].string.text = strdup(Classification);
	}
	else if (attr->num_values == 2 &&
	         strcmp(attr->values[0].string.text, attr->values[1].string.text) != 0 &&
		 strcmp(attr->values[0].string.text, "none") != 0 &&
		 strcmp(attr->values[1].string.text, "none") != 0)
        {
	 /*
	  * Can't put two different security markings on the same document!
	  */

          free(attr->values[1].string.text);
	  attr->values[1].string.text = strdup(attr->values[0].string.text);
	}
      }
      else if (strcmp(attr->values[0].string.text, Classification) != 0 &&
               (attr->num_values == 1 ||
	       strcmp(attr->values[1].string.text, Classification) != 0))
      {
       /*
        * Force the leading banner to have the classification on it...
	*/

        free(attr->values[0].string.text);
	attr->values[0].string.text = strdup(Classification);
      }
    }

   /*
    * See if we need to add the starting sheet...
    */

    if (!(printer->type & CUPS_PRINTER_REMOTE))
    {
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
  FILE		*in;		/* Input file */
  FILE		*out;		/* Output file */
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

  if (add_file(con, job, banner->filetype))
    return (0);

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  if ((out = fopen(filename, "w")) == NULL)
  {
    LogMessage(L_ERROR, "copy_banner: Unable to create banner job file %s - %s",
               filename, strerror(errno));
    job->num_files --;
    return (0);
  }

  fchmod(fileno(out), 0640);
  fchown(fileno(out), User, Group);

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

  if ((in = fopen(filename, "r")) == NULL)
  {
    fclose(out);
    unlink(filename);
    LogMessage(L_ERROR, "copy_banner: Unable to open banner template file %s - %s",
               filename, strerror(errno));
    job->num_files --;
    return (0);
  }

 /*
  * Parse the file to the end...
  */

  while ((ch = getc(in)) != EOF)
    if (ch == '{')
    {
     /*
      * Get an attribute name...
      */

      for (s = attrname; (ch = getc(in)) != EOF;)
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

        putc('{', out);
	fputs(attrname, out);
	putc(ch, out);
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
        fputs(job->dest, out);
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

	  putc('{', out);
	  fputs(attrname, out);
	  putc('}', out);
        }

        continue;
      }

     /*
      * Output value(s)...
      */

      for (i = 0; i < attr->num_values; i ++)
      {
	if (i)
	  putc(',', out);

	switch (attr->value_tag)
	{
	  case IPP_TAG_INTEGER :
	  case IPP_TAG_ENUM :
	      if (strncmp(attrname, "time-at-", 8) == 0)
	        fputs(GetDateTime(attr->values[i].integer), out);
	      else
	        fprintf(out, "%d", attr->values[i].integer);
	      break;

	  case IPP_TAG_BOOLEAN :
	      fprintf(out, "%d", attr->values[i].boolean);
	      break;

	  case IPP_TAG_NOVALUE :
	      fputs("novalue", out);
	      break;

	  case IPP_TAG_RANGE :
	      fprintf(out, "%d-%d", attr->values[i].range.lower,
		      attr->values[i].range.upper);
	      break;

	  case IPP_TAG_RESOLUTION :
	      fprintf(out, "%dx%d%s", attr->values[i].resolution.xres,
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
		    putc('\\', out);
		    putc(*p, out);
		  }
		  else if (*p < 32 || *p > 126)
		    fprintf(out, "\\%03o", *p);
		  else
		    putc(*p, out);
		}
	      }
	      else
		fputs(attr->values[i].string.text, out);
	      break;

          default :
	      break; /* anti-compiler-warning-code */
	}
      }
    }
    else if (ch == '\\')	/* Quoted char */
      putc(getc(in), out);
    else
      putc(ch, out);

  fclose(in);

  kbytes = (ftell(out) + 1023) / 1024;

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets", IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer += kbytes;

  fclose(out);

  return (kbytes);
}


/*
 * 'copy_file()' - Copy a PPD file or interface script...
 */

static int				/* O - 0 = success, -1 = error */
copy_file(const char *from,		/* I - Source file */
          const char *to)		/* I - Destination file */
{
#ifdef HAVE_LIBZ
  gzFile	src;			/* Source file */
#else
  int		src;			/* Source file */
#endif /* HAVE_LIBZ */
  int		dst,			/* Destination file */
		bytes;			/* Bytes to read/write */
  char		buffer[8192];		/* Copy buffer */


  LogMessage(L_DEBUG2, "copy_file(%s, %s)\n", from, to);

#ifdef HAVE_LIBZ
  if ((src = gzopen(from, "rb")) == NULL)
    return (-1);
#else
  if ((src = open(from, O_RDONLY)) < 0)
    return (-1);
#endif /* HAVE_LIBZ */

  if ((dst = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
  {
#ifdef HAVE_LIBZ
    gzclose(src);
#else
    close(src);
#endif /* HAVE_LIBZ */
    return (-1);
  }

#ifdef HAVE_LIBZ
  while ((bytes = gzread(src, buffer, sizeof(buffer))) > 0)
#else
  while ((bytes = read(src, buffer, sizeof(buffer))) > 0)
#endif /* HAVE_LIBZ */
    if (write(dst, buffer, bytes) < bytes)
    {
#ifdef HAVE_LIBZ
      gzclose(src);
#else
      close(src);
#endif /* HAVE_LIBZ */
      close(dst);
      return (-1);
    }

#ifdef HAVE_LIBZ
  gzclose(src);
#else
  close(src);
#endif /* HAVE_LIBZ */
  close(dst);

  return (0);
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

  if (dtype == CUPS_PRINTER_CLASS)
    printer = FindClass(dest);
  else
    printer = FindPrinter(dest);

 /*
  * Remove old jobs...
  */

  CancelJobs(dest);

 /*
  * Remove any old PPD or script files...
  */

  snprintf(filename, sizeof(filename), "%s/interfaces/%s", ServerRoot, dest);
  unlink(filename);

  snprintf(filename, sizeof(filename), "%s/ppd/%s.ppd", ServerRoot, dest);
  unlink(filename);

  if (dtype == CUPS_PRINTER_CLASS)
  {
    LogMessage(L_INFO, "Class \'%s\' deleted by \'%s\'.", dest,
               con->username);

    DeletePrinter(printer);
    SaveAllClasses();
  }
  else
  {
    LogMessage(L_INFO, "Printer \'%s\' deleted by \'%s\'.", dest,
               con->username);

    DeletePrinter(printer);
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
	                	IPP_TAG_KEYWORD), IPP_TAG_ZERO);

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
	                      IPP_TAG_KEYWORD), IPP_TAG_ZERO);

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

  if ((strncmp(resource, "/jobs", 5) == 0 && strlen(resource) <= 6) ||
      (strncmp(resource, "/printers", 9) == 0 && strlen(resource) <= 10))
  {
    dest  = NULL;
    dtype = (cups_ptype_t)0;
  }
  else if (strncmp(resource, "/classes", 8) == 0 && strlen(resource) <= 9)
  {
    dest  = NULL;
    dtype = CUPS_PRINTER_CLASS;
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

 /*
  * See if the "which-jobs" attribute have been specified; if so, return
  * right away if they specify "completed" - we don't keep old job records...
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
    {
      strncpy(username, con->username, sizeof(username) - 1);
      username[sizeof(username) - 1] = '\0';
    }
    else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    {
      strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
      username[sizeof(username) - 1] = '\0';
    }
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

    if ((dest != NULL && strcmp(job->dest, dest) != 0))
      continue;
    if (job->dtype != dtype &&
        (username[0] == '\0' || strncmp(resource, "/jobs", 5) != 0))
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

    copy_attrs(con->response, job->attrs, requested, IPP_TAG_JOB);

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

  copy_attrs(con->response, job->attrs, requested, IPP_TAG_JOB);

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
	                      IPP_TAG_KEYWORD), IPP_TAG_ZERO);

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

  if (dtype == CUPS_PRINTER_CLASS)
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

  if (printer->state_message[0])
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "printer-state-message", NULL, printer->state_message);

  ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                printer->accepting);

  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "printer-up-time", curtime);
  ippAddDate(con->response, IPP_TAG_PRINTER, "printer-current-time",
             ippTimeToDate(curtime));

  add_queued_job_count(con, printer);

  copy_attrs(con->response, printer->attrs,
             ippFindAttribute(con->request, "requested-attributes",
	                      IPP_TAG_KEYWORD), IPP_TAG_ZERO);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_printers()' - Get a list of printers or classes.
 */

static void
get_printers(client_t *con,		/* I - Client connection */
             int      type)		/* I - 0 or CUPS_PRINTER_CLASS */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*requested;	/* Requested attributes */
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

  if ((attr = ippFindAttribute(con->request, "location", IPP_TAG_TEXT)) != NULL)
    location = attr->values[0].string.text;
  else
    location = NULL;

  requested = ippFindAttribute(con->request, "requested-attributes",
	                       IPP_TAG_KEYWORD);

 /*
  * OK, build a list of printers for this printer...
  */

  curtime = time(NULL);

  for (count = 0, printer = Printers;
       count < limit && printer != NULL;
       printer = printer->next)
    if ((printer->type & CUPS_PRINTER_CLASS) == type &&
        (printer->type & printer_mask) == printer_type &&
	(location == NULL || strcasecmp(printer->location, location) == 0))
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
	*
	* Note: name and printer->name are both IPP_MAX_NAME characters
	*       in size, so strcpy() is safe...
	*/

        strcpy(name, printer->name);

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

      if (printer->state_message[0])
	ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                     "printer-state-message", NULL, printer->state_message);

      ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                    printer->accepting);

      ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                    "printer-up-time", curtime);
      ippAddDate(con->response, IPP_TAG_PRINTER, "printer-current-time",
        	 ippTimeToDate(curtime));

      add_queued_job_count(con, printer);

      copy_attrs(con->response, printer->attrs, requested, IPP_TAG_ZERO);
    }

  con->response->request.status.status_code = IPP_OK;
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
  * OK, see if the client is sending the document compressed - CUPS
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL &&
      strcmp(attr->values[0].string.text, "none") == 0)
  {
    LogMessage(L_ERROR, "print_job: Unsupported compression attribute %s!",
               attr->values[0].string.text);
    send_ipp_error(con, IPP_ATTRIBUTES);
    ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
	         "compression", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (con->filename[0] == '\0')
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

    filetype = mimeFileType(MimeDatabase, con->filename);

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
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);

    if (format)
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);

    return;
  }

  LogMessage(L_DEBUG, "print_job: request file type is %s/%s.",
	     filetype->super, filetype->type);

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

  if (dtype == CUPS_PRINTER_CLASS)
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
    LogMessage(L_INFO, "print_job: too many jobs.");
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

  strncpy(job->title, title, sizeof(job->title) - 1);

  attr = ippFindAttribute(job->attrs, "requesting-user-name", IPP_TAG_NAME);

  if (con->username[0])
  {
    strncpy(job->username, con->username, sizeof(job->username) - 1);
    job->username[sizeof(job->username) - 1] = '\0';
  }
  if (attr != NULL)
  {
    LogMessage(L_DEBUG, "print_job: requesting-user-name = \'%s\'",
               attr->values[0].string.text);

    strncpy(job->username, attr->values[0].string.text, sizeof(job->username) - 1);
    job->username[sizeof(job->username) - 1] = '\0';
  }
  else
    strcpy(job->username, "anonymous");

  if (attr == NULL)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-originating-user-name",
                 NULL, job->username);
  else
  {
    attr->group_tag = IPP_TAG_JOB;
    free(attr->name);
    attr->name = strdup("job-originating-user-name");
  }

 /*
  * Add remaining job attributes...
  */

  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, 
               "job-originating-host-name", NULL, con->http.hostname);
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

  if (!(printer->type & CUPS_PRINTER_REMOTE) || Classification[0])
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

    if (Classification[0])
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

          free(attr->values[0].string.text);
	  attr->values[0].string.text = strdup(Classification);
	}
	else if (attr->num_values == 2 &&
	         strcmp(attr->values[0].string.text, attr->values[1].string.text) != 0 &&
		 strcmp(attr->values[0].string.text, "none") != 0 &&
		 strcmp(attr->values[1].string.text, "none") != 0)
        {
	 /*
	  * Can't put two different security markings on the same document!
	  */

          free(attr->values[1].string.text);
	  attr->values[1].string.text = strdup(attr->values[0].string.text);
	}
      }
      else if (strcmp(attr->values[0].string.text, Classification) != 0 &&
               (attr->num_values == 1 ||
	       strcmp(attr->values[1].string.text, Classification) != 0))
      {
       /*
        * Force the leading banner to have the classification on it...
	*/

        free(attr->values[0].string.text);
	attr->values[0].string.text = strdup(Classification);
      }
    }

   /*
    * Add the starting sheet...
    */

    if (!(printer->type & CUPS_PRINTER_REMOTE))
    {
      kbytes = copy_banner(con, job, attr->values[0].string.text);

      UpdateQuota(printer, job->username, 0, kbytes);
    }
  }
  else if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL)
    job->sheets = attr;
   
 /*
  * Add the job file...
  */

  if (add_file(con, job, filetype))
    return;

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  rename(con->filename, filename);
  con->filename[0] = '\0';

 /*
  * See if we need to add the ending sheet...
  */

  if (!(printer->type & CUPS_PRINTER_REMOTE) && attr->num_values > 1)
  {
   /*
    * Yes...
    */

    kbytes = copy_banner(con, job, attr->values[1].string.text);
    UpdateQuota(printer, job->username, 0, kbytes);
  }

 /*
  * Log and save the job...
  */

  LogMessage(L_INFO, "Job %d queued on \'%s\' by \'%s\'.", job->id,
             job->dest, job->username);
  LogMessage(L_DEBUG, "Job %d hold_until = %d", job->id, job->hold_until);

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

  if (dtype == CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  printer->accepting = 0;

  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) == NULL)
    strcpy(printer->state_message, "Rejecting Jobs");
  else
  {
    strncpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message) - 1);
    printer->state_message[sizeof(printer->state_message) - 1] = '\0';
  }

  if (dtype == CUPS_PRINTER_CLASS)
    SaveAllClasses();
  else
    SaveAllPrinters();

  if (dtype == CUPS_PRINTER_CLASS)
    LogMessage(L_INFO, "Class \'%s\' rejecting jobs (\'%s\').", name,
               con->username);
  else
    LogMessage(L_INFO, "Printer \'%s\' rejecting jobs (\'%s\').", name,
               con->username);

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
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression", IPP_TAG_KEYWORD)) != NULL &&
      strcmp(attr->values[0].string.text, "none") == 0)
  {
    LogMessage(L_ERROR, "send_document: Unsupported compression attribute %s!",
               attr->values[0].string.text);
    send_ipp_error(con, IPP_ATTRIBUTES);
    ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
	         "compression", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * Do we have a file to print?
  */

  if (con->filename[0] == '\0')
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

    filetype = mimeFileType(MimeDatabase, con->filename);

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

  if (add_file(con, job, filetype))
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

  con->filename[0] = '\0';

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

    if (printer != NULL && !(printer->type & CUPS_PRINTER_REMOTE) &&
        (attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL &&
        attr->num_values > 1)
    {
     /*
      * Yes...
      */

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

  if (dtype == CUPS_PRINTER_CLASS)
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

    if (strcmp(attr->name, "job-originating-host-name") == 0 ||
        strcmp(attr->name, "job-originating-user-name") == 0 ||
	strcmp(attr->name, "job-media-sheets-completed") == 0 ||
	strcmp(attr->name, "job-k-octets") == 0 ||
	strcmp(attr->name, "job-id") == 0 ||
	strcmp(attr->name, "job-sheets") == 0 ||
	strncmp(attr->name, "time-at-", 8) == 0)
      continue; /* Read-only attrs */

    if (strcmp(attr->name, "job-priority") == 0 &&
        attr->value_tag == IPP_TAG_INTEGER &&
	job->state->values[0].integer != IPP_JOB_PROCESSING)
    {
     /*
      * Change the job priority
      */

      SetJobPriority(jobid, attr->values[0].integer);
    }
    else if ((attr2 = ippFindAttribute(job->attrs, attr->name, IPP_TAG_ZERO)) != NULL)
    {
     /*
      * Some other value; first free the old value...
      */

      for (prev2 = job->attrs->attrs; prev2 != NULL; prev2 = prev2->next)
	if (prev2->next == attr2)
	  break;

      if (prev2)
	prev2->next = attr2->next;
      else
	job->attrs->attrs = attr2->next;

      _ipp_free_attr(attr2);

     /*
      * Then copy the attribute...
      */

      copy_attribute(job->attrs, attr, 0);

     /*
      * See if the job-name or job-hold-until is being changed.
      */

      if (strcmp(attr->name, "job-name") == 0)
        strncpy(job->title, attr->values[0].string.text, sizeof(job->title) - 1);
      else if (strcmp(attr->name, "job-hold-until") == 0)
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

 /*
  * Return with "everything is OK" status...
  */

  con->response->request.status.status_code = IPP_OK;
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

  if (dtype == CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  StartPrinter(printer);

  if (dtype == CUPS_PRINTER_CLASS)
    SaveAllClasses();
  else
    SaveAllPrinters();

  if (dtype == CUPS_PRINTER_CLASS)
    LogMessage(L_INFO, "Class \'%s\' started by \'%s\'.", name,
               con->username);
  else
    LogMessage(L_INFO, "Printer \'%s\' started by \'%s\'.", name,
               con->username);

  printer->state_message[0] = '\0';

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

  if (dtype == CUPS_PRINTER_CLASS)
    printer = FindClass(name);
  else
    printer = FindPrinter(name);

  StopPrinter(printer);

  if (dtype == CUPS_PRINTER_CLASS)
    SaveAllClasses();
  else
    SaveAllPrinters();

  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) == NULL)
    strcpy(printer->state_message, "Paused");
  else
  {
    strncpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message) - 1);
    printer->state_message[sizeof(printer->state_message) - 1] = '\0';
  }

  if (dtype == CUPS_PRINTER_CLASS)
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
    strncpy(username, con->username, userlen - 1);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
    strncpy(username, attr->values[0].string.text, userlen - 1);
  else
    strncpy(username, "anonymous", userlen - 1);

  username[userlen - 1] = '\0';

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
      * system group...
      */

      return (0);
    }
  }

  return (1);
}


/*
 * End of "$Id: ipp.c,v 1.127.2.5 2002/01/09 17:04:15 mike Exp $".
 */
