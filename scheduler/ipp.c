/*
 * "$Id: ipp.c,v 1.79 2000/06/28 16:13:13 mike Exp $"
 *
 *   IPP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *   move_job()                  - Move a job.
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
static void	copy_attrs(ipp_t *to, ipp_t *from, ipp_attribute_t *req,
		           ipp_tag_t group);
static void	copy_banner(client_t *con, job_t *job, const char *name);
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
	  (uri == NULL && con->request->request.op.operation_id < IPP_PRIVATE))
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
	* OK, all the checks pass so far; try processing the operation...
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
              accept_jobs(con, uri);
              break;

	  case CUPS_REJECT_JOBS :
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


  DEBUG_printf(("accept_jobs(%08x, %08x)\n", con, uri));

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

  if ((name = ValidateDest(resource, &dtype)) == NULL)
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

  if (strncmp(resource, "/classes/", 9) != 0)
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
    * Rename the implicit class to "AnyClass"...
    */

    snprintf(pclass->name, sizeof(pclass->name), "Any%s", resource + 10);
    SortPrinters();

   /*
    * Add the class as a new local class...
    */

    pclass = AddClass(resource + 10);
    modify = 0;
  }
  else if (pclass->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Rename the remote class to "Class@server"...
    */

    snprintf(pclass->name, sizeof(pclass->name), "%s@%s", resource + 10,
             pclass->hostname);
    SortPrinters();

   /*
    * Add the class as a new local class...
    */

    pclass = AddClass(resource + 10);
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

  if ((attr = ippFindAttribute(con->request, "printer-more-info", IPP_TAG_URI)) != NULL)
  {
    strncpy(pclass->more_info, attr->values[0].string.text, sizeof(pclass->more_info) - 1);
    pclass->more_info[sizeof(pclass->more_info) - 1] = '\0';
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
  if ((attr = ippFindAttribute(con->request, "job-sheets-default", IPP_TAG_ZERO)) != NULL)
  {
    strncpy(pclass->job_sheets[0], attr->values[0].string.text,
            sizeof(pclass->job_sheets[0]) - 1);
    if (attr->num_values > 1)
      strncpy(pclass->job_sheets[1], attr->values[1].string.text,
              sizeof(pclass->job_sheets[1]) - 1);
    else
      strcpy(pclass->job_sheets[1], "none");
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

      if ((dest = ValidateDest(resource, &dtype)) == NULL)
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

static int
add_file(client_t    *con,		/* I - Connection to client */
         job_t       *job,		/* I - Job to add to */
         mime_type_t *filetype)		/* I - Type of file */
{
  mime_type_t	**filetypes;		/* New filetypes array... */


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

  DEBUG_printf(("add_printer(%08x, %08x)\n", con, uri));

 /*
  * Do we have a valid URI?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if (strncmp(resource, "/printers/", 10) != 0)
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
    * Rename the implicit printer to "AnyPrinter"...
    */

    snprintf(printer->name, sizeof(printer->name), "Any%s", resource + 10);
    SortPrinters();

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

    snprintf(printer->name, sizeof(printer->name), "%s@%s", resource + 10,
             printer->hostname);
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

  if ((attr = ippFindAttribute(con->request, "printer-more-info", IPP_TAG_URI)) != NULL)
  {
    strncpy(printer->more_info, attr->values[0].string.text, sizeof(printer->more_info) - 1);
    printer->more_info[sizeof(printer->more_info) - 1] = '\0';
  }

  if ((attr = ippFindAttribute(con->request, "device-uri", IPP_TAG_URI)) != NULL)
  {
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
  if ((attr = ippFindAttribute(con->request, "job-sheets-default", IPP_TAG_ZERO)) != NULL)
  {
    strncpy(printer->job_sheets[0], attr->values[0].string.text,
            sizeof(printer->job_sheets[0]) - 1);
    if (attr->num_values > 1)
      strncpy(printer->job_sheets[1], attr->values[1].string.text,
              sizeof(printer->job_sheets[1]) - 1);
    else
      strcpy(printer->job_sheets[1], "none");
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
    strcpy(srcfile, con->filename);
  else if ((attr = ippFindAttribute(con->request, "ppd-name", IPP_TAG_NAME)) != NULL)
    snprintf(srcfile, sizeof(srcfile), "%s/model/%s", DataDir,
             attr->values[0].string.text);
  else
    srcfile[0] = '\0';

  LogMessage(L_DEBUG, "add_printer: srcfile = \"%s\"", srcfile);

#ifdef HAVE_LIBZ
  if (srcfile[0] && (fp = gzopen(srcfile, "rb")) != NULL)
#else
  if (srcfile[0] && (fp = fopen(srcfile, "rb")) != NULL)
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
        LogMessage(L_ERROR, "add_printer: Unable to copy interface script - %s!",
	           strerror(errno));
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
        LogMessage(L_ERROR, "add_printer: Unable to copy PPD file - %s!",
	           strerror(errno));
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
  job_t		*job;			/* Current job */
  cups_ptype_t	dtype;			/* Destination type */
  int		count;			/* Number of jobs on destination */


  dtype = p->type & CUPS_PRINTER_CLASS;

  for (count = 0, job = Jobs; job != NULL; job = job->next)
    if (strcmp(job->dest, p->name) == 0 && job->dtype == dtype &&
        job->state->values[0].integer < IPP_JOB_STOPPED)
      count ++;

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


  DEBUG_printf(("cancel_all_jobs(%08x, %08x)\n", con, uri));

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

  if ((dest = ValidateDest(resource, &dtype)) == NULL)
  {
   /*
    * Bad URI...
    */

    LogMessage(L_ERROR, "cancel_all_jobs: resource name \'%s\' no good!", resource);
    send_ipp_error(con, IPP_NOT_FOUND);
    return;
  }

 /*
  * Cancel all of the jobs and return...
  */

  CancelJobs(dest);
  LogMessage(L_INFO, "All jobs on \'%s\' were cancelled by \'%s\'.", dest,
             con->username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'cancel_job()' - Cancel a print job.
 */

static void
cancel_job(client_t        *con,	/* I - Client connection */
	   ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  int			i;		/* Looping var */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */


  DEBUG_printf(("cancel_job(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "cancel_job: \"%s\" not authorized to delete job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
  }

 /*
  * Cancel the job and return...
  */

  CancelJob(jobid, 0);
  CheckJobs();

  LogMessage(L_INFO, "Job %d was cancelled by \'%s\'.", jobid,
             con->username[0] ? con->username : "unknown");

  con->response->request.status.status_code = IPP_OK;
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
  ipp_attribute_t	*toattr,	/* Destination attribute */
			*fromattr;	/* Source attribute */


  DEBUG_printf(("copy_attrs(%08x, %08x)\n", to, from));

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

    DEBUG_printf(("copy_attrs: copying attribute \'%s\'...\n", fromattr->name));

    switch (fromattr->value_tag)
    {
      case IPP_TAG_ZERO :
          ippAddSeparator(to);
	  break;

      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          toattr = ippAddIntegers(to, fromattr->group_tag, fromattr->value_tag,
	                          fromattr->name, fromattr->num_values, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	    toattr->values[i].integer = fromattr->values[i].integer;
          break;

      case IPP_TAG_BOOLEAN :
          toattr = ippAddBooleans(to, fromattr->group_tag, fromattr->name,
	                          fromattr->num_values, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	    toattr->values[i].boolean = fromattr->values[i].boolean;
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
          toattr = ippAddStrings(to, fromattr->group_tag, fromattr->value_tag,
	                         fromattr->name, fromattr->num_values, NULL,
				 NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	    toattr->values[i].string.text = strdup(fromattr->values[i].string.text);
          break;

      case IPP_TAG_DATE :
          toattr = ippAddDate(to, fromattr->group_tag, fromattr->name,
	                      fromattr->values[0].date);
          break;

      case IPP_TAG_RESOLUTION :
          toattr = ippAddResolutions(to, fromattr->group_tag, fromattr->name,
	                             fromattr->num_values, IPP_RES_PER_INCH,
				     NULL, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	  {
	    toattr->values[i].resolution.xres  = fromattr->values[i].resolution.xres;
	    toattr->values[i].resolution.yres  = fromattr->values[i].resolution.yres;
	    toattr->values[i].resolution.units = fromattr->values[i].resolution.units;
	  }
          break;

      case IPP_TAG_RANGE :
          toattr = ippAddRanges(to, fromattr->group_tag, fromattr->name,
	                        fromattr->num_values, NULL, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	  {
	    toattr->values[i].range.lower = fromattr->values[i].range.lower;
	    toattr->values[i].range.upper = fromattr->values[i].range.upper;
	  }
          break;

      case IPP_TAG_TEXTLANG :
      case IPP_TAG_NAMELANG :
          toattr = ippAddStrings(to, fromattr->group_tag, fromattr->value_tag,
	                         fromattr->name, fromattr->num_values, NULL, NULL);

          for (i = 0; i < fromattr->num_values; i ++)
	  {
	    if (i == 0)
	      toattr->values[0].string.charset =
	          strdup(fromattr->values[0].string.charset);
	    else
	      toattr->values[i].string.charset =
	          toattr->values[0].string.charset;

	    toattr->values[i].string.text =
	        strdup(fromattr->values[i].string.text);
          }
          break;

       default :
          break; /* anti-compiler-warning-code */
    }
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


  DEBUG_printf(("create_job(%08x, %08x)\n", con, uri));

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

  if ((dest = ValidateDest(resource, &dtype)) == NULL)
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
    sprintf(printer_uri, "http://%s:%d/classes/%s", ServerName,
	    ntohs(con->http.hostaddr.sin_port), dest);
  }
  else
  {
    printer = FindPrinter(dest);

    sprintf(printer_uri, "http://%s:%d/printers/%s", ServerName,
	    ntohs(con->http.hostaddr.sin_port), dest);
  }

  if (!printer->accepting)
  {
    LogMessage(L_INFO, "create_job: destination \'%s\' is not accepting jobs.",
               dest);
    send_ipp_error(con, IPP_NOT_ACCEPTING);
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
    strcpy(job->username, con->username);
  else if (attr != NULL)
  {
    LogMessage(L_DEBUG, "create_job: requesting-user-name = \'%s\'",
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

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);
  if (attr == NULL)
    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                        "job-hold-until", NULL, "no-hold");
  if (attr != NULL && strcmp(attr->values[0].string.text, "no-hold") != 0)
  {
   /*
    * Hold job until specified time...
    */

    job->state->values[0].integer = IPP_JOB_HELD;
    SetJobHoldUntil(job->id, attr->values[0].string.text);
  }

  if (!(printer->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Add job sheets options...
    */

    if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) == NULL)
    {
      attr = ippAddStrings(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-sheets",
                           2, NULL, NULL);
      attr->values[0].string.text = strdup(printer->job_sheets[0]);
      attr->values[1].string.text = strdup(printer->job_sheets[1]);
    }

   /*
    * See if we need to add the starting sheet...
    */

    copy_banner(con, job, attr->values[0].string.text);
  }

 /*
  * Save and log the job...
  */
   
  SaveJob(job->id);

  LogMessage(L_INFO, "Job %d created on \'%s\' by \'%s\'.", job->id,
             job->dest, job->username);

 /*
  * Fill in the response info...
  */

  sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	  ntohs(con->http.hostaddr.sin_port), job->id);
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

static void
copy_banner(client_t   *con,	/* I - Client connection */
            job_t      *job,	/* I - Job information */
            const char *name)	/* I - Name of banner */
{
  int		i;		/* Looping var */
  char		filename[1024];	/* Job filename */
  banner_t	*banner;	/* Pointer to banner */
  FILE		*in;		/* Input file */
  FILE		*out;		/* Output file */
  int		ch;		/* Character from file */
  char		attrname[255],	/* Name of attribute */
		*s;		/* Pointer into name */
  ipp_attribute_t *attr;	/* Attribute */


 /*
  * Find the banner; return if not found or "none"...
  */

  LogMessage(L_DEBUG, "copy_banner(%p, %d, \"%s\")", con, job->id,
             name ? name : "(null)");

  if (name == NULL ||
      strcmp(name, "none") == 0 ||
      (banner = FindBanner(name)) == NULL)
    return;

 /*
  * Open the banner and job files...
  */

  if (add_file(con, job, banner->filetype))
    return;

  sprintf(filename, "%s/d%05d-%03d", RequestRoot, job->id, job->num_files);
  if ((out = fopen(filename, "w")) == NULL)
  {
    LogMessage(L_ERROR, "copy_banner: Unable to create banner job file %s - %s",
               filename, strerror(errno));
    job->num_files --;
    return;
  }

  if ((in = fopen(banner->filename, "r")) == NULL)
  {
    fclose(out);
    unlink(filename);
    LogMessage(L_ERROR, "copy_banner: Unable to open banner template file %s - %s",
               filename, strerror(errno));
    job->num_files --;
    return;
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
        if (ch == '}')
          break;
	else if (s < (attrname + sizeof(attrname) - 1))
          *s++ = ch;

      *s = '\0';

     /*
      * See if it is defined...
      */

      if (strcmp(attrname, "printer-name") == 0)
      {
        fputs(job->dest, out);
	continue;
      }
      else if ((attr = ippFindAttribute(job->attrs, attrname, IPP_TAG_ZERO)) == NULL)
        continue; /* Nope */

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
  fclose(out);
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

  if ((dest = ValidateDest(resource, &dtype)) == NULL)
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

  CancelJobs(dest);
  DeletePrinter(printer);

 /*
  * Remove any old PPD or script files...
  */

  sprintf(filename, "%s/interfaces/%s", ServerRoot, dest);
  unlink(filename);

  sprintf(filename, "%s/ppd/%s.ppd", ServerRoot, dest);
  unlink(filename);

  SaveAllPrinters();

  if (dtype == CUPS_PRINTER_CLASS)
    LogMessage(L_INFO, "Class \'%s\' deleted by \'%s\'.", dest,
               con->username);
  else
    LogMessage(L_INFO, "Printer \'%s\' deleted by \'%s\'.", dest,
               con->username);

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
  DEBUG_printf(("get_default(%08x)\n", con));

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
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Current attribute */
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
  char			filename[1024];	/* Job filename */
  struct stat		filestats;	/* Print file information */
  size_t		jobsize;	/* Total job sizes */


  DEBUG_printf(("get_jobs(%08x, %08x)\n", con, uri));

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
  else if ((dest = ValidateDest(resource, &dtype)) == NULL)
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
      strcpy(username, con->username);
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
    * Send the following attributes for each job:
    *
    *    job-id
    *    job-k-octets
    *    job-more-info
    *    job-originating-user-name
    *    job-printer-uri
    *    job-priority
    *    job-state
    *    job-uri
    *    job-name
    *    job-printer-up-time
    *
    * Note that we are supposed to look at the "requested-attributes"
    * attribute to determine what we send, however the IPP/1.0 spec also
    * doesn't state that the server must limit the attributes to those
    * requested.  In other words, the server can either implement the
    * filtering or not, and if not it needs to send all attributes that
    * it has...
    */

    sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	    ntohs(con->http.hostaddr.sin_port), job->id);

    for (i = 0, jobsize = 0; i < job->num_files; i ++)
    {
      sprintf(filename, "%s/d%05d-%03d", RequestRoot, job->id, i + 1);
      stat(filename, &filestats);
      jobsize += filestats.st_size;
    }

    ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                  "job-k-octets", (jobsize + 1023) / 1024);

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

    copy_attrs(con->response, job->attrs,
               ippFindAttribute(con->request, "requested-attributes",
	                	IPP_TAG_KEYWORD), IPP_TAG_JOB);

    add_job_state_reasons(con, job);

    ippAddSeparator(con->response);
  }

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
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
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Current attribute */
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
  char			filename[1024];	/* Job filename */
  struct stat		filestats;	/* Print file information */
  size_t		jobsize;	/* Total job sizes */


  DEBUG_printf(("get_job_attrs(%08x, %08x)\n", con, uri));

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

  sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	  ntohs(con->http.hostaddr.sin_port), job->id);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);

  for (i = 0, jobsize = 0; i < job->num_files; i ++)
  {
    sprintf(filename, "%s/d%05d-%03d", RequestRoot, job->id, i + 1);
    stat(filename, &filestats);
    jobsize += filestats.st_size;
  }

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                "job-k-octets", (jobsize + 1023) / 1024);

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

  copy_attrs(con->response, job->attrs,
             ippFindAttribute(con->request, "requested-attributes",
	                      IPP_TAG_KEYWORD), IPP_TAG_JOB);

  add_job_state_reasons(con, job);

  if (ippFindAttribute(con->request, "requested-attributes", IPP_TAG_KEYWORD) != NULL)
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


  DEBUG_printf(("get_printer_attrs(%08x, %08x)\n", con, uri));
  LogMessage(L_DEBUG, "get_printer_attrs(%08x, \"%s\")\n", con,
             uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string.text, method, username, host, &port, resource);

  if ((dest = ValidateDest(resource, &dtype)) == NULL)
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
 * 'get_printers()' - Get a list of printers.
 */

static void
get_printers(client_t *con,		/* I - Client connection */
             int      type)		/* I - 0 or CUPS_PRINTER_CLASS */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  int			limit;		/* Maximum number of printers to return */
  int			count;		/* Number of printers that match */
  printer_t		*printer;	/* Current printer pointer */
  time_t		curtime;	/* Current time */
  int			printer_type,	/* printer-type attribute */
			printer_mask;	/* printer-type-mask attribute */
  char			*location;	/* Location string */


  DEBUG_printf(("get_printers(%08x)\n", con));

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

      copy_attrs(con->response, printer->attrs,
        	 ippFindAttribute(con->request, "requested-attributes",
	                	  IPP_TAG_KEYWORD), IPP_TAG_ZERO);
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
  int			i;		/* Looping var */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */


  DEBUG_printf(("hold_job(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "hold_job: \"%s\" not authorized to hold job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
  }

 /*
  * Hold the job and return...
  */

  HoldJob(jobid);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

  if (attr != NULL && strcmp(attr->values[0].string.text, "no-hold") != 0)
  {
   /*
    * Hold job until specified time...
    */

    SetJobHoldUntil(job->id, attr->values[0].string.text);
  }

  LogMessage(L_INFO, "Job %d was held by \'%s\'.", jobid,
             con->username[0] ? con->username : "unknown");

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'move_job()' - Set job attributes.
 */

static void
move_job(client_t        *con,		/* I - Client connection */
	 ipp_attribute_t *uri)		/* I - Job URI */
{
  int			i;		/* Looping var */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */


  DEBUG_printf(("move_job(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "move_job: \"%s\" not authorized to delete job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
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
  if ((dest = ValidateDest(resource, &dtype)) == NULL)
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


  DEBUG_printf(("print_job(%08x, %08x)\n", con, uri));

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

      sprintf(mimetype, "%s/%s", filetype->super, filetype->type);

      if (format != NULL)
      {
	free(format->values[0].string.text);
	format->values[0].string.text = strdup(mimetype);
      }
      else
        ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
	             "document-format", NULL, mimetype);
    }
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype == NULL)
  {
    LogMessage(L_ERROR, "print_job: Unsupported format \'%s\'!",
	       format->values[0].string.text);
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);
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

  if ((dest = ValidateDest(resource, &dtype)) == NULL)
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
    sprintf(printer_uri, "http://%s:%d/classes/%s", ServerName,
	    ntohs(con->http.hostaddr.sin_port), dest);
  }
  else
  {
    printer = FindPrinter(dest);

    sprintf(printer_uri, "http://%s:%d/printers/%s", ServerName,
	    ntohs(con->http.hostaddr.sin_port), dest);
  }

  if (!printer->accepting)
  {
    LogMessage(L_INFO, "print_job: destination \'%s\' is not accepting jobs.",
               dest);
    send_ipp_error(con, IPP_NOT_ACCEPTING);
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
    strcpy(job->username, con->username);
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

  ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);
  job->state = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_ENUM,
                             "job-state", IPP_JOB_PENDING);
  job->sheets = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                              "job-media-sheets-completed", 0);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL,
               printer_uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
               title);

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

  if (attr != NULL && strcmp(attr->values[0].string.text, "no-hold") != 0)
  {
   /*
    * Hold job until specified time...
    */

    job->state->values[0].integer = IPP_JOB_HELD;
    SetJobHoldUntil(job->id, attr->values[0].string.text);
  }

  if (!(printer->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Add job sheets options...
    */

    if ((attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) == NULL)
    {
      attr = ippAddStrings(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-sheets",
                           2, NULL, NULL);
      attr->values[0].string.text = strdup(printer->job_sheets[0]);
      attr->values[1].string.text = strdup(printer->job_sheets[1]);
    }

   /*
    * Add the starting sheet...
    */

    copy_banner(con, job, attr->values[0].string.text);
  }
   
 /*
  * Add the job file...
  */

  if (add_file(con, job, filetype))
    return;

  sprintf(filename, "%s/d%05d-%03d", RequestRoot, job->id, job->num_files);
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

    copy_banner(con, job, attr->values[1].string.text);
  }

 /*
  * Log and save the job...
  */

  LogMessage(L_INFO, "Job %d queued on \'%s\' by \'%s\'.", job->id,
             job->dest, job->username);

  SaveJob(job->id);

 /*
  * Start the job if possible...
  */

  CheckJobs();

 /*
  * Fill in the response info...
  */

  sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	  ntohs(con->http.hostaddr.sin_port), job->id);
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


  DEBUG_printf(("reject_jobs(%08x, %08x)\n", con, uri));

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

  if ((name = ValidateDest(resource, &dtype)) == NULL)
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
  int			i;		/* Looping var */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */


  DEBUG_printf(("release_job(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "release_job: \"%s\" not authorized to release job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
  }

 /*
  * Release the job and return...
  */

  ReleaseJob(jobid);

  LogMessage(L_INFO, "Job %d was released by \'%s\'.", jobid,
             con->username[0] ? con->username : "unknown");

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'restart_job()' - Restart an old print job.
 */

static void
restart_job(client_t        *con,	/* I - Client connection */
         ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  int			i;		/* Looping var */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */


  DEBUG_printf(("restart_job(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "restart_job: \"%s\" not authorized to restart job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
  }

 /*
  * Restart the job and return...
  */

  RestartJob(jobid);

  LogMessage(L_INFO, "Job %d was restarted by \'%s\'.", jobid,
             con->username[0] ? con->username : "unknown");

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'send_document()' - Send a file to a printer or class.
 */

static void
send_document(client_t        *con,	/* I - Client connection */
	      ipp_attribute_t *uri)	/* I - Printer URI */
{
  int			i;		/* Looping var */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */
  char			filename[1024];	/* Job filename */
  printer_t		*printer;	/* Current printer */


  DEBUG_printf(("send_document(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "send_document: \"%s\" not authorized to send document for job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
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

      sprintf(mimetype, "%s/%s", filetype->super, filetype->type);

      if (format != NULL)
      {
	free(format->values[0].string.text);
	format->values[0].string.text = strdup(mimetype);
      }
      else
        ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
	             "document-format", NULL, mimetype);
    }
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype == NULL)
  {
    LogMessage(L_ERROR, "send_document: Unsupported format \'%s\'!",
	       format->values[0].string.text);
    send_ipp_error(con, IPP_DOCUMENT_FORMAT);
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

  sprintf(filename, "%s/d%05d-%03d", RequestRoot, job->id, job->num_files);
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

    if (job->dtype & CUPS_PRINTER_CLASS)
      printer = FindClass(job->dest);
    else
      printer = FindPrinter(job->dest);

    if (printer != NULL && !(printer->type & CUPS_PRINTER_REMOTE) &&
        (attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_ZERO)) != NULL &&
        attr->num_values > 1)
    {
     /*
      * Yes...
      */

      copy_banner(con, job, attr->values[1].string.text);
    }

    if (job->state->values[0].integer == IPP_JOB_STOPPED)
      job->state->values[0].integer = IPP_JOB_PENDING;

    SaveJob(job->id);
    CheckJobs();
  }
  else if (ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_ZERO) == NULL)
  {
    job->state->values[0].integer = IPP_JOB_HELD;
    job->hold_until               = time(NULL) + 60;
    SaveJob(job->id);
  }

 /*
  * Fill in the response info...
  */

  sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	  ntohs(con->http.hostaddr.sin_port), job->id);
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
  DEBUG_printf(("send_ipp_error(%08x, %04x)\n", con, status));

  LogMessage(L_DEBUG, "Sending IPP error code %x.", status);
  if (con->filename[0])
    unlink(con->filename);

  con->response->request.status.status_code = status;
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


  DEBUG_printf(("set_default(%08x, %08x)\n", con, uri));

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

  if ((name = ValidateDest(resource, &dtype)) == NULL)
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
  int			i;		/* Looping var */
  ipp_attribute_t	*attr,		/* Current attribute */
			*prev,		/* Previous attribute */
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
  struct passwd		*user;		/* User info */
  struct group		*group;		/* System group info */


  DEBUG_printf(("set_job_attrs(%08x, %08x)\n", con, uri));

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

  if (con->username[0])
    strcpy(username, con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name", IPP_TAG_NAME)) != NULL)
  {
    strncpy(username, attr->values[0].string.text, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
  }
  else
    strcpy(username, "anonymous");

  if (strcmp(username, job->username) != 0 && strcmp(username, "root") != 0)
  {
   /*
    * Not the owner or root; check to see if the user is a member of the
    * system group...
    */

    user = getpwnam(username);
    endpwent();

    group = getgrnam(SystemGroup);
    endgrent();

    if (group != NULL)
    {
      for (i = 0; group->gr_mem[i]; i ++)
        if (strcmp(username, group->gr_mem[i]) == 0)
	  break;
    }
    else
      i = 0;

    if (user == NULL || group == NULL ||
        (group->gr_mem[i] == NULL && group->gr_gid != user->pw_gid))
    {
     /*
      * Username not found, group not found, or user is not part of the
      * system group...
      */

      LogMessage(L_ERROR, "set_job_attrs: \"%s\" not authorized to delete job id %d owned by \"%s\"!",
        	 username, jobid, job->username);
      send_ipp_error(con, IPP_FORBIDDEN);
      return;
    }
  }

 /*
  * See what the user wants to change.
  */

  for (attr = con->request->attrs, prev = NULL;
       attr != NULL;
       prev = attr, attr = attr->next)
  {
    if (attr->group_tag != IPP_TAG_JOB || !attr->name)
      continue;

    if (strcmp(attr->name, "job-priority") == 0 &&
        attr->value_tag == IPP_TAG_INTEGER &&
	job->state->values[0].integer != IPP_JOB_PROCESSING)
    {
     /*
      * Change the job priority
      */

      SetJobPriority(jobid, attr->values[0].integer);
    }
    else if ((attr2 = ippFindAttribute(job->attrs, attr->name, attr->value_tag)) != NULL)
    {
     /*
      * Some other value...
      */

      for (prev2 = job->attrs->attrs; prev2 != NULL; prev2 = prev2->next)
	if (prev2->next == attr2)
	  break;

      if (prev)
        prev->next = attr->next;
      else
        con->request->attrs = attr->next;

      if (prev2)
	prev2->next = attr;
      else
	job->attrs->attrs = attr;

      attr->next = attr2->next;
      attr       = prev;

      _ipp_free_attr(attr2);
            
     /*
      * See if the job-name is being changed.
      */

      if (strcmp(attr->name, "job-name") == 0)
        strncpy(job->title, attr->values[0].string.text, sizeof(job->title) - 1);
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
      * Add new option by moving it from one request to another...
      */

      for (attr2 = job->attrs->attrs; attr2 != NULL; attr2 = attr2->next)
        if (!attr2->next)
	  break;

      if (attr2)
        attr2->next = attr;
      else
        job->attrs->attrs = attr;

      if (prev)
        prev->next = attr->next;
      else
        con->request->attrs = attr->next;

      attr->next = NULL;
      attr       = prev;
    }
  }

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


  DEBUG_printf(("start_printer(%08x, %08x)\n", con, uri));

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

  if ((name = ValidateDest(resource, &dtype)) == NULL)
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


  DEBUG_printf(("stop_printer(%08x, %08x)\n", con, uri));

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

  if ((name = ValidateDest(resource, &dtype)) == NULL)
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


  DEBUG_printf(("validate_job(%08x, %08x)\n", con, uri));

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

  if (ValidateDest(resource, &dtype) == NULL)
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
 * End of "$Id: ipp.c,v 1.79 2000/06/28 16:13:13 mike Exp $".
 */
