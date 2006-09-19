/*
 * "$Id$"
 *
 *   IPP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 *   This file contains Kerberos support code, copyright 2006 by
 *   Jelmer Vernooij.
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
 *   cupsdProcessIPPRequest()    - Process an incoming IPP request...
 *   accept_jobs()               - Accept print jobs to a printer.
 *   add_class()                 - Add a class to the system.
 *   add_file()                  - Add a file to a job.
 *   add_job()                   - Add a job to a print queue.
 *   add_job_state_reasons()     - Add the "job-state-reasons" attribute based
 *                                 upon the job and printer state...
 *   add_job_subscriptions()     - Add any subcriptions for a job.
 *   add_job_uuid()              - Add job-uuid attribute to a job.
 *   add_printer()               - Add a printer to the system.
 *   add_printer_state_reasons() - Add the "printer-state-reasons" attribute
 *                                 based upon the printer state...
 *   add_queued_job_count()      - Add the "queued-job-count" attribute for
 *   apply_printer_defaults()    - Apply printer default options to a job.
 *   authenticate_job()          - Set job authentication info.
 *   cancel_all_jobs()           - Cancel all print jobs.
 *   cancel_job()                - Cancel a print job.
 *   cancel_subscription()       - Cancel a subscription.
 *   check_quotas()              - Check quotas for a printer and user.
 *   copy_attribute()            - Copy a single attribute.
 *   copy_attrs()                - Copy attributes from one request to another.
 *   copy_banner()               - Copy a banner file to the requests directory
 *                                 for the specified job.
 *   copy_file()                 - Copy a PPD file or interface script...
 *   copy_model()                - Copy a PPD model file, substituting default
 *                                 values as needed...
 *   copy_job_attrs()            - Copy job attributes.
 *   copy_printer_attrs()        - Copy printer attributes.
 *   copy_subscription_attrs()   - Copy subscription attributes.
 *   create_job()                - Print a file to a printer or class.
 *   create_requested_array()    - Create an array for the requested-attributes.
 *   create_subscription()       - Create a notification subscription.
 *   delete_printer()            - Remove a printer or class from the system.
 *   get_default()               - Get the default destination.
 *   get_devices()               - Get the list of available devices on the
 *                                 local system.
 *   get_job_attrs()             - Get job attributes.
 *   get_jobs()                  - Get a list of jobs for the specified printer.
 *   get_notifications()         - Get events for a subscription.
 *   get_ppds()                  - Get the list of PPD files on the local
 *                                 system.
 *   get_printer_attrs()         - Get printer attributes.
 *   get_printers()              - Get a list of printers.
 *   get_subscription_attrs()    - Get subscription attributes.
 *   get_subscriptions()         - Get subscriptions.
 *   get_username()              - Get the username associated with a request.
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
 *   save_auth_info()            - Save authentication information for a job.
 *   save_krb5_creds()           - Save Kerberos credentials for a job.
 *   send_document()             - Send a file to a printer or class.
 *   send_http_error()           - Send a HTTP error back to the IPP client.
 *   send_ipp_status()           - Send a status back to the IPP client.
 *   set_default()               - Set the default destination...
 *   set_job_attrs()             - Set job attributes.
 *   set_printer_defaults()      - Set printer default options from a request.
 *   start_printer()             - Start a printer.
 *   stop_printer()              - Stop a printer.
 *   url_encode_attr()           - URL-encode a string attribute.
 *   user_allowed()              - See if a user is allowed to print to a queue.
 *   validate_job()              - Validate printer options and destination.
 *   validate_name()             - Make sure the printer name only contains
 *                                 valid chars.
 *   validate_user()             - Validate the user for the request.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"

#ifdef HAVE_KRB5_H
#  include <krb5.h>
#endif /* HAVE_KRB5_H */

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

static void	accept_jobs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	add_class(cupsd_client_t *con, ipp_attribute_t *uri);
static int	add_file(cupsd_client_t *con, cupsd_job_t *job,
		         mime_type_t *filetype, int compression);
static cupsd_job_t *add_job(cupsd_client_t *con, cupsd_printer_t *printer,
			    mime_type_t *filetype);
static void	add_job_state_reasons(cupsd_client_t *con, cupsd_job_t *job);
static void	add_job_subscriptions(cupsd_client_t *con, cupsd_job_t *job);
static void	add_job_uuid(cupsd_client_t *con, cupsd_job_t *job);
static void	add_printer(cupsd_client_t *con, ipp_attribute_t *uri);
static void	add_printer_state_reasons(cupsd_client_t *con,
		                          cupsd_printer_t *p);
static void	add_queued_job_count(cupsd_client_t *con, cupsd_printer_t *p);
static void	apply_printer_defaults(cupsd_printer_t *printer,
				       cupsd_job_t *job);
static void	authenticate_job(cupsd_client_t *con, ipp_attribute_t *uri);
static void	cancel_all_jobs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	cancel_job(cupsd_client_t *con, ipp_attribute_t *uri);
static void	cancel_subscription(cupsd_client_t *con, int id);
static int	check_quotas(cupsd_client_t *con, cupsd_printer_t *p);
static ipp_attribute_t	*copy_attribute(ipp_t *to, ipp_attribute_t *attr,
		                        int quickcopy);
static void	copy_attrs(ipp_t *to, ipp_t *from, cups_array_t *ra,
		           ipp_tag_t group, int quickcopy);
static int	copy_banner(cupsd_client_t *con, cupsd_job_t *job,
		            const char *name);
static int	copy_file(const char *from, const char *to);
static int	copy_model(cupsd_client_t *con, const char *from,
		           const char *to);
static void	copy_job_attrs(cupsd_client_t *con,
		               cupsd_job_t *job,
			       cups_array_t *ra);
static void	copy_printer_attrs(cupsd_client_t *con,
		                   cupsd_printer_t *printer,
				   cups_array_t *ra);
static void	copy_subscription_attrs(cupsd_client_t *con,
		                        cupsd_subscription_t *sub,
					cups_array_t *ra);
static void	create_job(cupsd_client_t *con, ipp_attribute_t *uri);
static cups_array_t *create_requested_array(ipp_t *request);
static void	create_subscription(cupsd_client_t *con, ipp_attribute_t *uri);
static void	delete_printer(cupsd_client_t *con, ipp_attribute_t *uri);
static void	get_default(cupsd_client_t *con);
static void	get_devices(cupsd_client_t *con);
static void	get_jobs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	get_job_attrs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	get_notifications(cupsd_client_t *con);
static void	get_ppds(cupsd_client_t *con);
static void	get_printers(cupsd_client_t *con, int type);
static void	get_printer_attrs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	get_subscription_attrs(cupsd_client_t *con, int sub_id);
static void	get_subscriptions(cupsd_client_t *con, ipp_attribute_t *uri);
static const char *get_username(cupsd_client_t *con);
static void	hold_job(cupsd_client_t *con, ipp_attribute_t *uri);
static void	move_job(cupsd_client_t *con, ipp_attribute_t *uri);
static int	ppd_add_default(const char *option, const char *choice,
		                int num_defaults, ppd_default_t **defaults);
static int	ppd_parse_line(const char *line, char *option, int olen,
		               char *choice, int clen);
static void	print_job(cupsd_client_t *con, ipp_attribute_t *uri);
static void	read_ps_job_ticket(cupsd_client_t *con);
static void	reject_jobs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	release_job(cupsd_client_t *con, ipp_attribute_t *uri);
static void	renew_subscription(cupsd_client_t *con, int sub_id);
static void	restart_job(cupsd_client_t *con, ipp_attribute_t *uri);
static void	save_auth_info(cupsd_client_t *con, cupsd_job_t *job);
#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5_H)
static void	save_krb5_creds(cupsd_client_t *con, cupsd_job_t *job);
#endif /* HAVE_GSSAPI && HAVE_KRB5_H */
static void	send_document(cupsd_client_t *con, ipp_attribute_t *uri);
static void	send_http_error(cupsd_client_t *con, http_status_t status);
static void	send_ipp_status(cupsd_client_t *con, ipp_status_t status,
		                const char *message, ...)
#    ifdef __GNUC__
__attribute__ ((__format__ (__printf__, 3, 4)))
#    endif /* __GNUC__ */
;
static void	set_default(cupsd_client_t *con, ipp_attribute_t *uri);
static void	set_job_attrs(cupsd_client_t *con, ipp_attribute_t *uri);
static void	set_printer_defaults(cupsd_client_t *con,
		                     cupsd_printer_t *printer);
static void	start_printer(cupsd_client_t *con, ipp_attribute_t *uri);
static void	stop_printer(cupsd_client_t *con, ipp_attribute_t *uri);
static void	url_encode_attr(ipp_attribute_t *attr, char *buffer,
		                int bufsize);
static int	user_allowed(cupsd_printer_t *p, const char *username);
static void	validate_job(cupsd_client_t *con, ipp_attribute_t *uri);
static int	validate_name(const char *name);
static int	validate_user(cupsd_job_t *job, cupsd_client_t *con,
		              const char *owner, char *username,
		              int userlen);


/*
 * 'cupsdProcessIPPRequest()' - Process an incoming IPP request...
 */

int					/* O - 1 on success, 0 on failure */
cupsdProcessIPPRequest(
    cupsd_client_t *con)		/* I - Client connection */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */
  ipp_attribute_t	*username;	/* requesting-user-name attr */
  int			sub_id;		/* Subscription ID */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdProcessIPPRequest(%p[%d]): operation_id = %04x",
                  con, con->http.fd, con->request->request.op.operation_id);

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

    cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                  "%04X %s Bad request version number %d.%d",
		  IPP_VERSION_NOT_SUPPORTED, con->http.hostname,
                  con->request->request.any.version[0],
	          con->request->request.any.version[1]);

    send_ipp_status(con, IPP_VERSION_NOT_SUPPORTED,
                    _("Bad request version number %d.%d!"),
		    con->request->request.any.version[0],
	            con->request->request.any.version[1]);
  }
  else if (!con->request->attrs)
  {
    cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                  "%04X %s No attributes in request",
		  IPP_BAD_REQUEST, con->http.hostname);

    send_ipp_status(con, IPP_BAD_REQUEST, _("No attributes in request!"));
  }
  else
  {
   /*
    * Make sure that the attributes are provided in the correct order and
    * don't repeat groups...
    */

    for (attr = con->request->attrs, group = attr->group_tag;
	 attr;
	 attr = attr->next)
      if (attr->group_tag < group && attr->group_tag != IPP_TAG_ZERO)
      {
       /*
	* Out of order; return an error...
	*/

	cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                      "%04X %s Attribute groups are out of order",
		      IPP_BAD_REQUEST, con->http.hostname);

	send_ipp_status(con, IPP_BAD_REQUEST,
	                _("Attribute groups are out of order (%x < %x)!"),
			attr->group_tag, group);
	break;
      }
      else
	group = attr->group_tag;

    if (!attr)
    {
     /*
      * Then make sure that the first three attributes are:
      *
      *     attributes-charset
      *     attributes-natural-language
      *     printer-uri/job-uri
      */

      attr = con->request->attrs;
      if (attr && !strcmp(attr->name, "attributes-charset") &&
	  (attr->value_tag & IPP_TAG_MASK) == IPP_TAG_CHARSET)
	charset = attr;
      else
	charset = NULL;

      if (attr)
        attr = attr->next;

      if (attr && !strcmp(attr->name, "attributes-natural-language") &&
	  (attr->value_tag & IPP_TAG_MASK) == IPP_TAG_LANGUAGE)
	language = attr;
      else
	language = NULL;

      if ((attr = ippFindAttribute(con->request, "printer-uri",
                                   IPP_TAG_URI)) != NULL)
	uri = attr;
      else if ((attr = ippFindAttribute(con->request, "job-uri",
                                        IPP_TAG_URI)) != NULL)
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

      if (!charset || !language ||
	  (!uri &&
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

        if (!charset)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Missing attributes-charset attribute!");

	  cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                	"%04X %s Missing attributes-charset attribute",
			IPP_BAD_REQUEST, con->http.hostname);
        }

        if (!language)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Missing attributes-natural-language attribute!");

	  cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                	"%04X %s Missing attributes-natural-language attribute",
			IPP_BAD_REQUEST, con->http.hostname);
        }

        if (!uri)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Missing printer-uri or job-uri attribute!");

	  cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                	"%04X %s Missing printer-uri or job-uri attribute",
			IPP_BAD_REQUEST, con->http.hostname);
        }

	cupsdLogMessage(CUPSD_LOG_DEBUG, "Request attributes follow...");

	for (attr = con->request->attrs; attr; attr = attr->next)
	  cupsdLogMessage(CUPSD_LOG_DEBUG,
	        	  "attr \"%s\": group_tag = %x, value_tag = %x",
	        	  attr->name ? attr->name : "(null)", attr->group_tag,
			  attr->value_tag);

	cupsdLogMessage(CUPSD_LOG_DEBUG, "End of attributes...");

	send_ipp_status(con, IPP_BAD_REQUEST,
	                _("Missing required attributes!"));
      }
      else
      {
       /*
	* OK, all the checks pass so far; make sure requesting-user-name is
	* not "root" from a remote host...
	*/

        if ((username = ippFindAttribute(con->request, "requesting-user-name",
	                                 IPP_TAG_NAME)) != NULL)
	{
	 /*
	  * Check for root user...
	  */

	  if (!strcmp(username->values[0].string.text, "root") &&
	      strcasecmp(con->http.hostname, "localhost") &&
	      strcmp(con->username, "root"))
	  {
	   /*
	    * Remote unauthenticated user masquerading as local root...
	    */

	    _cupsStrFree(username->values[0].string.text);
	    username->values[0].string.text = _cupsStrAlloc(RemoteRoot);
	  }
	}

        if ((attr = ippFindAttribute(con->request, "notify-subscription-id",
	                             IPP_TAG_INTEGER)) != NULL)
	  sub_id = attr->values[0].integer;
	else
	  sub_id = 0;

       /*
        * Then try processing the operation...
	*/

        if (uri)
	  cupsdLogMessage(CUPSD_LOG_DEBUG, "%s %s",
                	  ippOpString(con->request->request.op.operation_id),
			  uri->values[0].string.text);
        else
	  cupsdLogMessage(CUPSD_LOG_DEBUG, "%s",
                	  ippOpString(con->request->request.op.operation_id));

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

	  case CUPS_AUTHENTICATE_JOB :
              authenticate_job(con, uri);
              break;

          case IPP_CREATE_PRINTER_SUBSCRIPTION :
	  case IPP_CREATE_JOB_SUBSCRIPTION :
	      create_subscription(con, uri);
	      break;

          case IPP_GET_SUBSCRIPTION_ATTRIBUTES :
	      get_subscription_attrs(con, sub_id);
	      break;

	  case IPP_GET_SUBSCRIPTIONS :
	      get_subscriptions(con, uri);
	      break;

	  case IPP_RENEW_SUBSCRIPTION :
	      renew_subscription(con, sub_id);
	      break;

	  case IPP_CANCEL_SUBSCRIPTION :
	      cancel_subscription(con, sub_id);
	      break;

          case IPP_GET_NOTIFICATIONS :
	      get_notifications(con);
	      break;

	  default :
	      cupsdAddEvent(CUPSD_EVENT_SERVER_AUDIT, NULL, NULL,
                	    "%04X %s Operation %04X (%s) not supported",
			    IPP_OPERATION_NOT_SUPPORTED, con->http.hostname,
			    con->request->request.op.operation_id,
			    ippOpString(con->request->request.op.operation_id));

              send_ipp_status(con, IPP_OPERATION_NOT_SUPPORTED,
	                      _("%s not supported!"),
			      ippOpString(con->request->request.op.operation_id));
	      break;
	}
      }
    }
  }

  if (con->response)
  {
   /*
    * Sending data from the scheduler...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "cupsdProcessIPPRequest: %d status_code=%x (%s)",
                    con->http.fd, con->response->request.status.status_code,
	            ippErrorString(con->response->request.status.status_code));

    if (cupsdSendHeader(con, HTTP_OK, "application/ipp"))
    {
#ifdef CUPSD_USE_CHUNKING
     /*
      * Because older versions of CUPS (1.1.17 and older) and some IPP
      * clients do not implement chunking properly, we cannot use
      * chunking by default.  This may become the default in future
      * CUPS releases, or we might add a configuration directive for
      * it.
      */

      if (con->http.version == HTTP_1_1)
      {
	if (httpPrintf(HTTP(con), "Transfer-Encoding: chunked\r\n\r\n") < 0)
	  return (0);

	if (cupsdFlushHeader(con) < 0)
	  return (0);

	con->http.data_encoding = HTTP_ENCODE_CHUNKED;
      }
      else
#endif /* CUPSD_USE_CHUNKING */
      {
        size_t	length;			/* Length of response */


	length = ippLength(con->response);

	if (httpPrintf(HTTP(con), "Content-Length: " CUPS_LLFMT "\r\n\r\n",
        	       CUPS_LLCAST length) < 0)
	  return (0);

	if (cupsdFlushHeader(con) < 0)
	  return (0);

	con->http.data_encoding  = HTTP_ENCODE_LENGTH;
	con->http.data_remaining = length;
      }

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdProcessIPPRequest: Adding fd %d to OutputSet...",
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
  else
  {
   /*
    * Sending data from a subprocess like cups-deviced; tell the caller
    * everything is A-OK so far...
    */

    return (1);
  }
}


/*
 * 'accept_jobs()' - Accept print jobs to a printer.
 */

static void
accept_jobs(cupsd_client_t  *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Printer or class URI */
{
  http_status_t	status;			/* Policy status */
  cups_ptype_t	dtype;			/* Destination type (printer or class) */
  cupsd_printer_t *printer;		/* Printer data */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "accept_jobs(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Accept jobs sent to the printer...
  */

  printer->accepting        = 1;
  printer->state_message[0] = '\0';

  cupsdAddPrinterHistory(printer);

  if (dtype & CUPS_PRINTER_CLASS)
  {
    cupsdSaveAllClasses();

    cupsdLogMessage(CUPSD_LOG_INFO, "Class \"%s\" now accepting jobs (\"%s\").",
                    printer->name, get_username(con));
  }
  else
  {
    cupsdSaveAllPrinters();

    cupsdLogMessage(CUPSD_LOG_INFO, "Printer \"%s\" now accepting jobs (\"%s\").",
                    printer->name, get_username(con));
  }

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_class()' - Add a class to the system.
 */

static void
add_class(cupsd_client_t  *con,		/* I - Client connection */
          ipp_attribute_t *uri)		/* I - URI of class */
{
  http_status_t	status;			/* Policy status */
  int		i;			/* Looping var */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_printer_t *pclass,		/* Class */
		*member;		/* Member printer/class */
  cups_ptype_t	dtype;			/* Destination type */
  ipp_attribute_t *attr;		/* Printer attribute */
  int		modify;			/* Non-zero if we just modified */
  char		newname[IPP_MAX_NAME];	/* New class name */
  int		need_restart_job;	/* Need to restart job? */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "add_class(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Do we have a valid URI?
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                  sizeof(method), username, sizeof(username), host,
		  sizeof(host), &port, resource, sizeof(resource));


  if (strncmp(resource, "/classes/", 9) || strlen(resource) == 9)
  {
   /*
    * No, return an error...
    */

    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("The printer-uri must be of the form "
		      "\"ipp://HOSTNAME/classes/CLASSNAME\"."));
    return;
  }

 /*
  * Do we have a valid printer name?
  */

  if (!validate_name(resource + 9))
  {
   /*
    * No, return an error...
    */

    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("The printer-uri \"%s\" contains invalid characters."),
		    uri->values[0].string.text);
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * See if the class already exists; if not, create a new class...
  */

  if ((pclass = cupsdFindClass(resource + 9)) == NULL)
  {
   /*
    * Class doesn't exist; see if we have a printer of the same name...
    */

    if ((pclass = cupsdFindPrinter(resource + 9)) != NULL &&
        !(pclass->type & CUPS_PRINTER_REMOTE))
    {
     /*
      * Yes, return an error...
      */

      send_ipp_status(con, IPP_NOT_POSSIBLE,
                      _("A printer named \"%s\" already exists!"),
		      resource + 9);
      return;
    }

   /*
    * No, add the pclass...
    */

    pclass = cupsdAddClass(resource + 9);
    modify = 0;
  }
  else if (pclass->type & CUPS_PRINTER_IMPLICIT)
  {
   /*
    * Rename the implicit class to "AnyClass" or remove it...
    */

    if (ImplicitAnyClasses)
    {
      snprintf(newname, sizeof(newname), "Any%s", resource + 9);
      cupsdRenamePrinter(pclass, newname);
    }
    else
      cupsdDeletePrinter(pclass, 1);

   /*
    * Add the class as a new local class...
    */

    pclass = cupsdAddClass(resource + 9);
    modify = 0;
  }
  else if (pclass->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Rename the remote class to "Class"...
    */

    snprintf(newname, sizeof(newname), "%s@%s", resource + 9, pclass->hostname);
    cupsdRenamePrinter(pclass, newname);

   /*
    * Add the class as a new local class...
    */

    pclass = cupsdAddClass(resource + 9);
    modify = 0;
  }
  else
    modify = 1;

 /*
  * Look for attributes and copy them over as needed...
  */

  need_restart_job = 0;

  if ((attr = ippFindAttribute(con->request, "printer-location",
                               IPP_TAG_TEXT)) != NULL)
    cupsdSetString(&pclass->location, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "printer-info",
                               IPP_TAG_TEXT)) != NULL)
    cupsdSetString(&pclass->info, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "printer-is-accepting-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Setting %s printer-is-accepting-jobs to %d (was %d.)",
               pclass->name, attr->values[0].boolean, pclass->accepting);

    pclass->accepting = attr->values[0].boolean;
    cupsdAddPrinterHistory(pclass);
  }

  if ((attr = ippFindAttribute(con->request, "printer-is-shared",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    if (pclass->shared && !attr->values[0].boolean)
      cupsdSendBrowseDelete(pclass);

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Setting %s printer-is-shared to %d (was %d.)",
                    pclass->name, attr->values[0].boolean, pclass->shared);

    pclass->shared = attr->values[0].boolean;
  }

  if ((attr = ippFindAttribute(con->request, "printer-state",
                               IPP_TAG_ENUM)) != NULL)
  {
    if (attr->values[0].integer != IPP_PRINTER_IDLE &&
        attr->values[0].integer != IPP_PRINTER_STOPPED)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Attempt to set %s printer-state to bad value %d!"),
                      pclass->name, attr->values[0].integer);
      return;
    }

    cupsdLogMessage(CUPSD_LOG_INFO, "Setting %s printer-state to %d (was %d.)", pclass->name,
                    attr->values[0].integer, pclass->state);

    if (attr->values[0].integer == IPP_PRINTER_STOPPED)
      cupsdStopPrinter(pclass, 0);
    else
    {
      cupsdSetPrinterState(pclass, (ipp_pstate_t)(attr->values[0].integer), 0);
      need_restart_job = 1;
    }
  }
  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) != NULL)
  {
    strlcpy(pclass->state_message, attr->values[0].string.text,
            sizeof(pclass->state_message));
    cupsdAddPrinterHistory(pclass);
  }
  if ((attr = ippFindAttribute(con->request, "member-uris",
                               IPP_TAG_URI)) != NULL)
  {
   /*
    * Clear the printer array as needed...
    */

    need_restart_job = 1;

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

      if (!cupsdValidateDest(attr->values[i].string.text, &dtype, &member))
      {
       /*
	* Bad URI...
	*/

	send_ipp_status(con, IPP_NOT_FOUND,
                	_("The printer or class was not found."));
	return;
      }

     /*
      * Add it to the class...
      */

      cupsdAddPrinterToClass(pclass, member);
    }
  }

  set_printer_defaults(con, pclass);

 /*
  * Update the printer class attributes and return...
  */

  cupsdSetPrinterAttrs(pclass);
  cupsdSaveAllClasses();

  if (need_restart_job && pclass->job)
  {
    cupsd_job_t *job;

   /*
    * Stop the current job and then restart it below...
    */

    job = (cupsd_job_t *)pclass->job;

    cupsdStopJob(job, 1);

    job->state->values[0].integer = IPP_JOB_PENDING;
    job->state_value              = IPP_JOB_PENDING;
  }

  if (need_restart_job)
    cupsdCheckJobs();

  cupsdWritePrintcap();

  if (modify)
  {
    cupsdAddEvent(CUPSD_EVENT_PRINTER_MODIFIED, pclass, NULL,
                  "Class \"%s\" modified by \"%s\".", pclass->name,
        	  get_username(con));

    cupsdLogMessage(CUPSD_LOG_INFO, "Class \"%s\" modified by \"%s\".",
                    pclass->name, get_username(con));
  }
  else
  {
    cupsdAddPrinterHistory(pclass);

    cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, pclass, NULL,
                  "New class \"%s\" added by \"%s\".", pclass->name,
        	  get_username(con));

    cupsdLogMessage(CUPSD_LOG_INFO, "New class \"%s\" added by \"%s\".",
                    pclass->name, get_username(con));
  }

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_file()' - Add a file to a job.
 */

static int				/* O - 0 on success, -1 on error */
add_file(cupsd_client_t *con,		/* I - Connection to client */
         cupsd_job_t    *job,		/* I - Job to add to */
         mime_type_t    *filetype,	/* I - Type of file */
	 int            compression)	/* I - Compression */
{
  mime_type_t	**filetypes;		/* New filetypes array... */
  int		*compressions;		/* New compressions array... */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
        	  "add_file(con=%p[%d], job=%d, filetype=%s/%s, compression=%d)",
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

  if (!compressions || !filetypes)
  {
    cupsdCancelJob(job, 1, IPP_JOB_ABORTED);

    send_ipp_status(con, IPP_INTERNAL_ERROR,
                    _("Unable to allocate memory for file types!"));
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
 * 'add_job()' - Add a job to a print queue.
 */

static cupsd_job_t *			/* O - Job object */
add_job(cupsd_client_t  *con,		/* I - Client connection */
	cupsd_printer_t *printer,	/* I - Destination printer */
	mime_type_t     *filetype)	/* I - First print file type, if any */
{
  http_status_t	status;			/* Policy status */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*val;			/* Default option value */
  int		priority;		/* Job priority */
  char		*title;			/* Job name/title */
  cupsd_job_t	*job;			/* Current job */
  char		job_uri[HTTP_MAX_URI];	/* Job URI */
  int		kbytes;			/* Size of print file */
  int		i;			/* Looping var */
  int		lowerpagerange;		/* Page range bound */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "add_job(%p[%d], %p(%s), %p(%s/%s))",
                  con, con->http.fd, printer, printer->name,
		  filetype, filetype->super, filetype->type);

 /*
  * Check remote printing to non-shared printer...
  */

  if (!printer->shared &&
      strcasecmp(con->http.hostname, "localhost") &&
      strcasecmp(con->http.hostname, ServerName))
  {
    send_ipp_status(con, IPP_NOT_AUTHORIZED,
                    _("The printer or class is not shared!"));
    return (NULL);
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return (NULL);
  }
  else if ((printer->type & CUPS_PRINTER_AUTHENTICATED) && !con->username[0])
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return (NULL);
  }

 /*
  * See if the printer is accepting jobs...
  */

  if (!printer->accepting)
  {
    send_ipp_status(con, IPP_NOT_ACCEPTING,
                    _("Destination \"%s\" is not accepting jobs."),
                    printer->name);
    return (NULL);
  }

 /*
  * Validate job template attributes; for now just document-format,
  * copies, and page-ranges...
  */

  if (filetype && printer->filetypes &&
      !cupsArrayFind(printer->filetypes, filetype))
  {
    char	mimetype[MIME_MAX_SUPER + MIME_MAX_TYPE + 2];
					/* MIME media type string */


    snprintf(mimetype, sizeof(mimetype), "%s/%s", filetype->super,
             filetype->type);

    send_ipp_status(con, IPP_DOCUMENT_FORMAT,
                    _("Unsupported format \'%s\'!"), mimetype);

    ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                 "document-format", NULL, mimetype);

    return (NULL);
  }

  if ((attr = ippFindAttribute(con->request, "copies",
                               IPP_TAG_INTEGER)) != NULL)
  {
    if (attr->values[0].integer < 1 || attr->values[0].integer > MaxCopies)
    {
      send_ipp_status(con, IPP_ATTRIBUTES, _("Bad copies value %d."),
                      attr->values[0].integer);
      ippAddInteger(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER,
	            "copies", attr->values[0].integer);
      return (NULL);
    }
  }

  if ((attr = ippFindAttribute(con->request, "page-ranges",
                               IPP_TAG_RANGE)) != NULL)
  {
    for (i = 0, lowerpagerange = 1; i < attr->num_values; i ++)
    {
      if (attr->values[i].range.lower < lowerpagerange ||
	  attr->values[i].range.lower > attr->values[i].range.upper)
      {
	send_ipp_status(con, IPP_BAD_REQUEST,
	                _("Bad page-ranges values %d-%d."),
	                attr->values[i].range.lower,
			attr->values[i].range.upper);
	return (NULL);
      }

      lowerpagerange = attr->values[i].range.upper + 1;
    }
  }

 /*
  * Make sure we aren't over our limit...
  */

  if (MaxJobs && cupsArrayCount(Jobs) >= MaxJobs)
    cupsdCleanJobs();

  if (MaxJobs && cupsArrayCount(Jobs) >= MaxJobs)
  {
    send_ipp_status(con, IPP_NOT_POSSIBLE,
                    _("Too many active jobs."));
    return (NULL);
  }

  if (!check_quotas(con, printer))
  {
    send_ipp_status(con, IPP_NOT_POSSIBLE, _("Quota limit reached."));
    return (NULL);
  }

 /*
  * Create the job and set things up...
  */

  if ((attr = ippFindAttribute(con->request, "job-priority",
                               IPP_TAG_INTEGER)) != NULL)
    priority = attr->values[0].integer;
  else
  {
    if ((val = cupsGetOption("job-priority", printer->num_options,
                             printer->options)) != NULL)
      priority = atoi(val);
    else
      priority = 50;

    ippAddInteger(con->request, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-priority",
                  priority);
  }

  if ((attr = ippFindAttribute(con->request, "job-name",
                               IPP_TAG_NAME)) != NULL)
    title = attr->values[0].string.text;
  else
    ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
                 title = "Untitled");

  if ((job = cupsdAddJob(priority, printer->name)) == NULL)
  {
    send_ipp_status(con, IPP_INTERNAL_ERROR,
                    _("Unable to add job for destination \"%s\"!"),
		    printer->name);
    return (NULL);
  }

  job->dtype   = printer->type & (CUPS_PRINTER_CLASS | CUPS_PRINTER_IMPLICIT |
                                  CUPS_PRINTER_REMOTE);
  job->attrs   = con->request;
  con->request = NULL;

  add_job_uuid(con, job);
  apply_printer_defaults(printer, job);

  attr = ippFindAttribute(job->attrs, "requesting-user-name", IPP_TAG_NAME);

  if (con->username[0])
  {
    cupsdSetString(&job->username, con->username);

    if (attr)
      cupsdSetString(&attr->values[0].string.text, con->username);

    save_auth_info(con, job);
  }
  else if (attr)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
                    "add_job: requesting-user-name=\"%s\"",
                    attr->values[0].string.text);

    cupsdSetString(&job->username, attr->values[0].string.text);
  }
  else
    cupsdSetString(&job->username, "anonymous");

  if (!attr)
    ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME,
                 "job-originating-user-name", NULL, job->username);
  else
  {
    attr->group_tag = IPP_TAG_JOB;
    _cupsStrFree(attr->name);
    attr->name = _cupsStrAlloc("job-originating-user-name");
  }

  if ((attr = ippFindAttribute(job->attrs, "job-originating-host-name",
                               IPP_TAG_ZERO)) != NULL)
  {
   /*
    * Request contains a job-originating-host-name attribute; validate it...
    */

    if (attr->value_tag != IPP_TAG_NAME ||
        attr->num_values != 1 ||
        strcmp(con->http.hostname, "localhost"))
    {
     /*
      * Can't override the value if we aren't connected via localhost.
      * Also, we can only have 1 value and it must be a name value.
      */

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
	      _cupsStrFree(attr->values[i].string.text);
	      attr->values[i].string.text = NULL;
	      if (attr->values[i].string.charset)
	      {
		_cupsStrFree(attr->values[i].string.charset);
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
      attr->values[0].string.text = _cupsStrAlloc(con->http.hostname);
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
  job->state_value = (ipp_jstate_t)job->state->values[0].integer;
  job->sheets = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                              "job-media-sheets-completed", 0);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-printer-uri", NULL,
               printer->uri);
  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-name", NULL,
               title);

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets",
                               IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer = 0;
  else
    attr = ippAddInteger(job->attrs, IPP_TAG_JOB, IPP_TAG_INTEGER,
                         "job-k-octets", 0);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                               IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);
  if (!attr)
  {
    if ((val = cupsGetOption("job-hold-until", printer->num_options,
                             printer->options)) == NULL)
      val = "no-hold";

    attr = ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_KEYWORD,
                        "job-hold-until", NULL, val);
  }
  if (attr && strcmp(attr->values[0].string.text, "no-hold") &&
      !(printer->type & CUPS_PRINTER_REMOTE))
  {
   /*
    * Hold job until specified time...
    */

    cupsdSetJobHoldUntil(job, attr->values[0].string.text);

    job->state->values[0].integer = IPP_JOB_HELD;
    job->state_value              = IPP_JOB_HELD;
  }
  else if (job->attrs->request.op.operation_id == IPP_CREATE_JOB)
  {
    job->hold_until               = time(NULL) + 60;
    job->state->values[0].integer = IPP_JOB_HELD;
    job->state_value              = IPP_JOB_HELD;
  }
  else
  {
    job->state->values[0].integer = IPP_JOB_PENDING;
    job->state_value              = IPP_JOB_PENDING;
  }

  if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) ||
      Classification)
  {
   /*
    * Add job sheets options...
    */

    if ((attr = ippFindAttribute(job->attrs, "job-sheets",
                                 IPP_TAG_ZERO)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Adding default job-sheets values \"%s,%s\"...",
                      printer->job_sheets[0], printer->job_sheets[1]);

      attr = ippAddStrings(job->attrs, IPP_TAG_JOB, IPP_TAG_NAME, "job-sheets",
                           2, NULL, NULL);
      attr->values[0].string.text = _cupsStrAlloc(printer->job_sheets[0]);
      attr->values[1].string.text = _cupsStrAlloc(printer->job_sheets[1]);
    }

    job->job_sheets = attr;

   /*
    * Enforce classification level if set...
    */

    if (Classification)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Classification=\"%s\", ClassifyOverride=%d",
                      Classification ? Classification : "(null)",
		      ClassifyOverride);

      if (ClassifyOverride)
      {
        if (!strcmp(attr->values[0].string.text, "none") &&
	    (attr->num_values == 1 ||
	     !strcmp(attr->values[1].string.text, "none")))
        {
	 /*
          * Force the leading banner to have the classification on it...
	  */

          cupsdSetString(&attr->values[0].string.text, Classification);

	  cupsdLogMessage(CUPSD_LOG_NOTICE, "[Job %d] CLASSIFICATION FORCED "
	                		    "job-sheets=\"%s,none\", "
					    "job-originating-user-name=\"%s\"",
	        	 job->id, Classification, job->username);
	}
	else if (attr->num_values == 2 &&
	         strcmp(attr->values[0].string.text,
		        attr->values[1].string.text) &&
		 strcmp(attr->values[0].string.text, "none") &&
		 strcmp(attr->values[1].string.text, "none"))
        {
	 /*
	  * Can't put two different security markings on the same document!
	  */

          cupsdSetString(&attr->values[1].string.text, attr->values[0].string.text);

	  cupsdLogMessage(CUPSD_LOG_NOTICE, "[Job %d] CLASSIFICATION FORCED "
	                		    "job-sheets=\"%s,%s\", "
					    "job-originating-user-name=\"%s\"",
	        	 job->id, attr->values[0].string.text,
			 attr->values[1].string.text, job->username);
	}
	else if (strcmp(attr->values[0].string.text, Classification) &&
	         strcmp(attr->values[0].string.text, "none") &&
		 (attr->num_values == 1 ||
	          (strcmp(attr->values[1].string.text, Classification) &&
	           strcmp(attr->values[1].string.text, "none"))))
        {
	  if (attr->num_values == 1)
            cupsdLogMessage(CUPSD_LOG_NOTICE,
	                    "[Job %d] CLASSIFICATION OVERRIDDEN "
	                    "job-sheets=\"%s\", "
			    "job-originating-user-name=\"%s\"",
	               job->id, attr->values[0].string.text, job->username);
          else
            cupsdLogMessage(CUPSD_LOG_NOTICE,
	                    "[Job %d] CLASSIFICATION OVERRIDDEN "
	                    "job-sheets=\"%s,%s\",fffff "
			    "job-originating-user-name=\"%s\"",
	        	    job->id, attr->values[0].string.text,
			    attr->values[1].string.text, job->username);
        }
      }
      else if (strcmp(attr->values[0].string.text, Classification) &&
               (attr->num_values == 1 ||
	       strcmp(attr->values[1].string.text, Classification)))
      {
       /*
        * Force the banner to have the classification on it...
	*/

        if (attr->num_values > 1 &&
	    !strcmp(attr->values[0].string.text, attr->values[1].string.text))
	{
          cupsdSetString(&(attr->values[0].string.text), Classification);
          cupsdSetString(&(attr->values[1].string.text), Classification);
	}
        else
	{
          if (attr->num_values == 1 ||
	      strcmp(attr->values[0].string.text, "none"))
            cupsdSetString(&(attr->values[0].string.text), Classification);

          if (attr->num_values > 1 &&
	      strcmp(attr->values[1].string.text, "none"))
            cupsdSetString(&(attr->values[1].string.text), Classification);
        }

        if (attr->num_values > 1)
	  cupsdLogMessage(CUPSD_LOG_NOTICE,
	                  "[Job %d] CLASSIFICATION FORCED "
	                  "job-sheets=\"%s,%s\", "
			  "job-originating-user-name=\"%s\"",
	        	  job->id, attr->values[0].string.text,
			  attr->values[1].string.text, job->username);
        else
	  cupsdLogMessage(CUPSD_LOG_NOTICE,
	                  "[Job %d] CLASSIFICATION FORCED "
	                  "job-sheets=\"%s\", "
			  "job-originating-user-name=\"%s\"",
	        	 job->id, Classification, job->username);
      }
    }

   /*
    * See if we need to add the starting sheet...
    */

    if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)))
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Adding start banner page \"%s\" to job %d.",
                      attr->values[0].string.text, job->id);

      kbytes = copy_banner(con, job, attr->values[0].string.text);

      cupsdUpdateQuota(printer, job->username, 0, kbytes);
    }
  }
  else if ((attr = ippFindAttribute(job->attrs, "job-sheets",
                                    IPP_TAG_ZERO)) != NULL)
    job->sheets = attr;

 /*
  * Fill in the response info...
  */

  snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	   LocalPort, job->id);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL,
               job_uri);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", job->id);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state",
                job->state_value);
  add_job_state_reasons(con, job);

  con->response->request.status.status_code = IPP_OK;

 /*
  * Add any job subscriptions...
  */

  add_job_subscriptions(con, job);

 /*
  * Set all but the first two attributes to the job attributes group...
  */

  for (attr = job->attrs->attrs->next->next; attr; attr = attr->next)
    attr->group_tag = IPP_TAG_JOB;

 /*
  * Fire the "job created" event...
  */

  cupsdAddEvent(CUPSD_EVENT_JOB_CREATED, printer, job, "Job created.");

 /*
  * Return the new job...
  */

  return (job);
}


/*
 * 'add_job_state_reasons()' - Add the "job-state-reasons" attribute based
 *                             upon the job and printer state...
 */

static void
add_job_state_reasons(
    cupsd_client_t *con,		/* I - Client connection */
    cupsd_job_t    *job)		/* I - Job info */
{
  cupsd_printer_t	*dest;		/* Destination printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "add_job_state_reasons(%p[%d], %d)",
                  con, con->http.fd, job ? job->id : 0);

  switch (job ? job->state_value : IPP_JOB_CANCELED)
  {
    case IPP_JOB_PENDING :
	dest = cupsdFindDest(job->dest);

        if (dest && dest->state == IPP_PRINTER_STOPPED)
          ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	               "job-state-reasons", NULL, "printer-stopped");
        else
          ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_KEYWORD,
	               "job-state-reasons", NULL, "none");
        break;

    case IPP_JOB_HELD :
        if (ippFindAttribute(job->attrs, "job-hold-until",
	                     IPP_TAG_KEYWORD) != NULL ||
	    ippFindAttribute(job->attrs, "job-hold-until",
	                     IPP_TAG_NAME) != NULL)
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

    case IPP_JOB_CANCELED :
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
 * 'add_job_subscriptions()' - Add any subcriptions for a job.
 */

static void
add_job_subscriptions(
    cupsd_client_t *con,		/* I - Client connection */
    cupsd_job_t    *job)		/* I - Newly created job */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*prev,		/* Previous attribute */
			*next,		/* Next attribute */
			*attr;		/* Current attribute */
  cupsd_subscription_t	*sub;		/* Subscription object */
  const char		*recipient,	/* notify-recipient-uri */
			*pullmethod;	/* notify-pull-method */
  ipp_attribute_t	*user_data;	/* notify-user-data */
  int			interval;	/* notify-time-interval */
  unsigned		mask;		/* notify-events */


 /*
  * Find the first subscription group attribute; return if we have
  * none...
  */

  for (attr = job->attrs->attrs, prev = NULL;
       attr;
       prev = attr, attr = attr->next)
    if (attr->group_tag == IPP_TAG_SUBSCRIPTION)
      break;

  if (!attr)
    return;

 /*
  * Process the subscription attributes in the request...
  */

  while (attr)
  {
    recipient = NULL;
    pullmethod = NULL;
    user_data  = NULL;
    interval   = 0;
    mask       = CUPSD_EVENT_NONE;

    while (attr && attr->group_tag != IPP_TAG_ZERO)
    {
      if (!strcmp(attr->name, "notify-recipient") &&
          attr->value_tag == IPP_TAG_URI)
        recipient = attr->values[0].string.text;
      else if (!strcmp(attr->name, "notify-pull-method") &&
               attr->value_tag == IPP_TAG_KEYWORD)
        pullmethod = attr->values[0].string.text;
      else if (!strcmp(attr->name, "notify-charset") &&
               attr->value_tag == IPP_TAG_CHARSET &&
	       strcmp(attr->values[0].string.text, "us-ascii") &&
	       strcmp(attr->values[0].string.text, "utf-8"))
      {
        send_ipp_status(con, IPP_CHARSET,
	                _("Character set \"%s\" not supported!"),
			attr->values[0].string.text);
	return;
      }
      else if (!strcmp(attr->name, "notify-natural-language") &&
               (attr->value_tag != IPP_TAG_LANGUAGE ||
	        strcmp(attr->values[0].string.text, DefaultLanguage)))
      {
        send_ipp_status(con, IPP_CHARSET,
	                _("Language \"%s\" not supported!"),
			attr->values[0].string.text);
	return;
      }
      else if (!strcmp(attr->name, "notify-user-data") &&
               attr->value_tag == IPP_TAG_STRING)
      {
        if (attr->num_values > 1 || attr->values[0].unknown.length > 63)
	{
          send_ipp_status(con, IPP_REQUEST_VALUE,
	                  _("The notify-user-data value is too large "
			    "(%d > 63 octets)!"),
			  attr->values[0].unknown.length);
	  return;
	}

        user_data = attr;
      }
      else if (!strcmp(attr->name, "notify-events") &&
               attr->value_tag == IPP_TAG_KEYWORD)
      {
        for (i = 0; i < attr->num_values; i ++)
	  mask |= cupsdEventValue(attr->values[i].string.text);
      }
      else if (!strcmp(attr->name, "notify-lease-duration"))
      {
        send_ipp_status(con, IPP_BAD_REQUEST,
	                _("The notify-lease-duration attribute cannot be "
			  "used with job subscriptions."));
	return;
      }
      else if (!strcmp(attr->name, "notify-time-interval") &&
               attr->value_tag == IPP_TAG_INTEGER)
        interval = attr->values[0].integer;

      attr = attr->next;
    }

    if (!recipient && !pullmethod)
      break;

    if (mask == CUPSD_EVENT_NONE)
      mask = CUPSD_EVENT_JOB_COMPLETED;

    sub = cupsdAddSubscription(mask, cupsdFindDest(job->dest), job, recipient,
                               0);

    sub->interval = interval;

    cupsdSetString(&sub->owner, job->username);

    if (user_data)
    {
      sub->user_data_len = user_data->values[0].unknown.length;
      memcpy(sub->user_data, user_data->values[0].unknown.data,
             sub->user_data_len);
    }

    ippAddSeparator(con->response);
    ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                  "notify-subscription-id", sub->id);

    if (attr)
      attr = attr->next;
  }

  cupsdSaveAllSubscriptions();

 /*
  * Remove all of the subscription attributes from the job request...
  */

  for (attr = job->attrs->attrs, prev = NULL; attr; attr = next)
  {
    next = attr->next;

    if (attr->group_tag == IPP_TAG_SUBSCRIPTION ||
        attr->group_tag == IPP_TAG_ZERO)
    {
     /*
      * Free and remove this attribute...
      */

      _ippFreeAttr(attr);

      if (prev)
        prev->next = next;
      else
        job->attrs->attrs = next;
    }
    else
      prev = attr;
  }

  job->attrs->last    = prev;
  job->attrs->current = prev;
}


/*
 * 'add_job_uuid()' - Add job-uuid attribute to a job.
 *
 * See RFC 4122 for the definition of UUIDs and the format.
 */

static void
add_job_uuid(cupsd_client_t *con,	/* I - Client connection */
             cupsd_job_t    *job)	/* I - Job */
{
  char			uuid[1024];	/* job-uuid string */
  _cups_md5_state_t	md5state;	/* MD5 state */
  unsigned char		md5sum[16];	/* MD5 digest/sum */


 /*
  * First see if the job already has a job-uuid attribute; if so, return...
  */

  if (ippFindAttribute(job->attrs, "job-uuid", IPP_TAG_URI))
    return;

 /*
  * No job-uuid attribute, so build a version 3 UUID with the local job
  * ID at the end; see RFC 4122 for details.  Start with the MD5 sum of
  * the ServerName, server name and port that the client connected to,
  * and local job ID...
  */

  snprintf(uuid, sizeof(uuid), "%s:%s:%d:%d", ServerName, con->servername,
	   con->serverport, job->id);

  _cupsMD5Init(&md5state);
  _cupsMD5Append(&md5state, (unsigned char *)uuid, strlen(uuid));
  _cupsMD5Finish(&md5state, md5sum);

 /*
  * Format the UUID URI using the MD5 sum and job ID.
  */

  snprintf(uuid, sizeof(uuid),
           "urn:uuid:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
	   "%02x%02x%02x%02x%02x%02x",
	   md5sum[0], md5sum[1], md5sum[2], md5sum[3], md5sum[4], md5sum[5],
	   (md5sum[6] & 15) | 0x30, md5sum[7], (md5sum[8] & 0x3f) | 0x40,
	   md5sum[9], md5sum[10], md5sum[11], md5sum[12], md5sum[13],
	   md5sum[14], md5sum[15]);

  ippAddString(job->attrs, IPP_TAG_JOB, IPP_TAG_URI, "job-uuid", NULL, uuid);
}


/*
 * 'add_printer()' - Add a printer to the system.
 */

static void
add_printer(cupsd_client_t  *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - URI of printer */
{
  http_status_t	status;			/* Policy status */
  int		i;			/* Looping var */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_printer_t *printer;		/* Printer/class */
  ipp_attribute_t *attr;		/* Printer attribute */
  cups_file_t	*fp;			/* Script/PPD file */
  char		line[1024];		/* Line from file... */
  char		srcfile[1024],		/* Source Script/PPD file */
		dstfile[1024];		/* Destination Script/PPD file */
  int		modify;			/* Non-zero if we are modifying */
  char		newname[IPP_MAX_NAME];	/* New printer name */
  int		need_restart_job;	/* Need to restart job? */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "add_printer(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Do we have a valid URI?
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                  sizeof(method), username, sizeof(username), host,
		  sizeof(host), &port, resource, sizeof(resource));

  if (strncmp(resource, "/printers/", 10) || strlen(resource) == 10)
  {
   /*
    * No, return an error...
    */

    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("The printer-uri must be of the form "
		      "\"ipp://HOSTNAME/printers/PRINTERNAME\"."));
    return;
  }

 /*
  * Do we have a valid printer name?
  */

  if (!validate_name(resource + 10))
  {
   /*
    * No, return an error...
    */

    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("The printer-uri \"%s\" contains invalid characters."),
		    uri->values[0].string.text);
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * See if the printer already exists; if not, create a new printer...
  */

  if ((printer = cupsdFindPrinter(resource + 10)) == NULL)
  {
   /*
    * Printer doesn't exist; see if we have a class of the same name...
    */

    if ((printer = cupsdFindClass(resource + 10)) != NULL &&
        !(printer->type & CUPS_PRINTER_REMOTE))
    {
     /*
      * Yes, return an error...
      */

      send_ipp_status(con, IPP_NOT_POSSIBLE,
                      _("A class named \"%s\" already exists!"),
        	      resource + 10);
      return;
    }

   /*
    * No, add the printer...
    */

    printer = cupsdAddPrinter(resource + 10);
    modify  = 0;
  }
  else if (printer->type & CUPS_PRINTER_IMPLICIT)
  {
   /*
    * Rename the implicit printer to "AnyPrinter" or delete it...
    */

    if (ImplicitAnyClasses)
    {
      snprintf(newname, sizeof(newname), "Any%s", resource + 10);
      cupsdRenamePrinter(printer, newname);
    }
    else
      cupsdDeletePrinter(printer, 1);

   /*
    * Add the printer as a new local printer...
    */

    printer = cupsdAddPrinter(resource + 10);
    modify  = 0;
  }
  else if (printer->type & CUPS_PRINTER_REMOTE)
  {
   /*
    * Rename the remote printer to "Printer@server"...
    */

    snprintf(newname, sizeof(newname), "%s@%s", resource + 10,
             printer->hostname);
    cupsdRenamePrinter(printer, newname);

   /*
    * Add the printer as a new local printer...
    */

    printer = cupsdAddPrinter(resource + 10);
    modify  = 0;
  }
  else
    modify = 1;

 /*
  * Look for attributes and copy them over as needed...
  */

  need_restart_job = 0;

  if ((attr = ippFindAttribute(con->request, "printer-location",
                               IPP_TAG_TEXT)) != NULL)
    cupsdSetString(&printer->location, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "printer-info",
                               IPP_TAG_TEXT)) != NULL)
    cupsdSetString(&printer->info, attr->values[0].string.text);

  if ((attr = ippFindAttribute(con->request, "device-uri",
                               IPP_TAG_URI)) != NULL)
  {
   /*
    * Do we have a valid device URI?
    */

    need_restart_job = 1;

    httpSeparateURI(HTTP_URI_CODING_ALL, attr->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (!strcmp(method, "file"))
    {
     /*
      * See if the administrator has enabled file devices...
      */

      if (!FileDevice && strcmp(resource, "/dev/null"))
      {
       /*
        * File devices are disabled and the URL is not file:/dev/null...
	*/

	send_ipp_status(con, IPP_NOT_POSSIBLE,
	                _("File device URIs have been disabled! "
	                  "To enable, see the FileDevice directive in "
			  "\"%s/cupsd.conf\"."),
			ServerRoot);
	return;
      }
    }
    else
    {
     /*
      * See if the backend exists and is executable...
      */

      snprintf(srcfile, sizeof(srcfile), "%s/backend/%s", ServerBin, method);
      if (access(srcfile, X_OK))
      {
       /*
        * Could not find device in list!
	*/

	send_ipp_status(con, IPP_NOT_POSSIBLE, _("Bad device-uri \"%s\"!"),
        	        attr->values[0].string.text);
	return;
      }
    }

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Setting %s device-uri to \"%s\" (was \"%s\".)",
        	    printer->name,
		    cupsdSanitizeURI(attr->values[0].string.text, line,
		                     sizeof(line)),
		    cupsdSanitizeURI(printer->device_uri, resource,
		                     sizeof(resource)));

    cupsdSetString(&printer->device_uri, attr->values[0].string.text);
  }

  if ((attr = ippFindAttribute(con->request, "port-monitor",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    ipp_attribute_t	*supported;	/* port-monitor-supported attribute */


    need_restart_job = 1;

    supported = ippFindAttribute(printer->attrs, "port-monitor-supported",
                                 IPP_TAG_KEYWORD);
    for (i = 0; i < supported->num_values; i ++)
      if (!strcmp(supported->values[i].string.text,
                  attr->values[0].string.text))
        break;

    if (i >= supported->num_values)
    {
      send_ipp_status(con, IPP_NOT_POSSIBLE, _("Bad port-monitor \"%s\"!"),
        	      attr->values[0].string.text);
      return;
    }

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Setting %s port-monitor to \"%s\" (was \"%s\".)",
                    printer->name, attr->values[0].string.text,
	            printer->port_monitor);

    if (strcmp(attr->values[0].string.text, "none"))
      cupsdSetString(&printer->port_monitor, attr->values[0].string.text);
    else
      cupsdClearString(&printer->port_monitor);
  }

  if ((attr = ippFindAttribute(con->request, "printer-is-accepting-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Setting %s printer-is-accepting-jobs to %d (was %d.)",
                    printer->name, attr->values[0].boolean, printer->accepting);

    printer->accepting = attr->values[0].boolean;
    cupsdAddPrinterHistory(printer);
  }

  if ((attr = ippFindAttribute(con->request, "printer-is-shared",
                               IPP_TAG_BOOLEAN)) != NULL)
  {
    if (printer->shared && !attr->values[0].boolean)
      cupsdSendBrowseDelete(printer);

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Setting %s printer-is-shared to %d (was %d.)",
                    printer->name, attr->values[0].boolean, printer->shared);

    printer->shared = attr->values[0].boolean;
  }

  if ((attr = ippFindAttribute(con->request, "printer-state",
                               IPP_TAG_ENUM)) != NULL)
  {
    if (attr->values[0].integer != IPP_PRINTER_IDLE &&
        attr->values[0].integer != IPP_PRINTER_STOPPED)
    {
      send_ipp_status(con, IPP_BAD_REQUEST, _("Bad printer-state value %d!"),
                      attr->values[0].integer);
      return;
    }

    cupsdLogMessage(CUPSD_LOG_INFO, "Setting %s printer-state to %d (was %d.)", printer->name,
               attr->values[0].integer, printer->state);

    if (attr->values[0].integer == IPP_PRINTER_STOPPED)
      cupsdStopPrinter(printer, 0);
    else
    {
      need_restart_job = 1;
      cupsdSetPrinterState(printer, (ipp_pstate_t)(attr->values[0].integer), 0);
    }
  }
  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) != NULL)
  {
    strlcpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message));
    cupsdAddPrinterHistory(printer);
  }

  set_printer_defaults(con, printer);

 /*
  * See if we have all required attributes...
  */

  if (!printer->device_uri)
    cupsdSetString(&printer->device_uri, "file:///dev/null");

 /*
  * See if we have an interface script or PPD file attached to the request...
  */

  if (con->filename)
  {
    need_restart_job = 1;

    strlcpy(srcfile, con->filename, sizeof(srcfile));

    if ((fp = cupsFileOpen(srcfile, "rb")))
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

      if (!strncmp(line, "*PPD-Adobe", 10))
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
          send_ipp_status(con, IPP_INTERNAL_ERROR,
	                  _("Unable to copy interface script - %s!"),
	                  strerror(errno));
	  return;
	}
	else
	{
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Copied interface script successfully!");
          chmod(dstfile, 0755);
	}
      }

      snprintf(dstfile, sizeof(dstfile), "%s/ppd/%s.ppd", ServerRoot,
               printer->name);

      if (!strncmp(line, "*PPD-Adobe", 10))
      {
       /*
	* The new file is a PPD file, so move the file over to the
	* ppd directory and make it readable by all...
	*/

	if (copy_file(srcfile, dstfile))
	{
          send_ipp_status(con, IPP_INTERNAL_ERROR,
	                  _("Unable to copy PPD file - %s!"),
	                  strerror(errno));
	  return;
	}
	else
	{
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Copied PPD file successfully!");
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
  else if ((attr = ippFindAttribute(con->request, "ppd-name",
                                    IPP_TAG_NAME)) != NULL)
  {
    need_restart_job = 1;

    if (!strcmp(attr->values[0].string.text, "raw"))
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

      snprintf(dstfile, sizeof(dstfile), "%s/interfaces/%s", ServerRoot,
               printer->name);
      unlink(dstfile);

      snprintf(dstfile, sizeof(dstfile), "%s/ppd/%s.ppd", ServerRoot,
               printer->name);

      if (copy_model(con, attr->values[0].string.text, dstfile))
      {
        send_ipp_status(con, IPP_INTERNAL_ERROR, _("Unable to copy PPD file!"));
	return;
      }
      else
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG,
	                "Copied PPD file successfully!");
        chmod(dstfile, 0644);
      }
    }
  }

 /*
  * Update the printer attributes and return...
  */

  cupsdSetPrinterAttrs(printer);
  cupsdSaveAllPrinters();

  if (need_restart_job && printer->job)
  {
    cupsd_job_t *job;

   /*
    * Stop the current job and then restart it below...
    */

    job = (cupsd_job_t *)printer->job;

    cupsdStopJob(job, 1);

    job->state->values[0].integer = IPP_JOB_PENDING;
    job->state_value              = IPP_JOB_PENDING;
  }

  if (need_restart_job)
    cupsdCheckJobs();

  cupsdWritePrintcap();

  if (modify)
  {
    cupsdAddEvent(CUPSD_EVENT_PRINTER_MODIFIED, printer, NULL,
                  "Printer \"%s\" modified by \"%s\".", printer->name,
        	  get_username(con));

    cupsdLogMessage(CUPSD_LOG_INFO, "Printer \"%s\" modified by \"%s\".",
                    printer->name, get_username(con));
  }
  else
  {
    cupsdAddPrinterHistory(printer);

    cupsdAddEvent(CUPSD_EVENT_PRINTER_ADDED, printer, NULL,
                  "New printer \"%s\" added by \"%s\".", printer->name,
        	  get_username(con));

    cupsdLogMessage(CUPSD_LOG_INFO, "New printer \"%s\" added by \"%s\".",
                    printer->name, get_username(con));
  }

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'add_printer_state_reasons()' - Add the "printer-state-reasons" attribute
 *                                 based upon the printer state...
 */

static void
add_printer_state_reasons(
    cupsd_client_t  *con,		/* I - Client connection */
    cupsd_printer_t *p)			/* I - Printer info */
{
  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "add_printer_state_reasons(%p[%d], %p[%s])",
                  con, con->http.fd, p, p->name);

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
add_queued_job_count(
    cupsd_client_t  *con,		/* I - Client connection */
    cupsd_printer_t *p)			/* I - Printer or class */
{
  int		count;			/* Number of jobs on destination */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "add_queued_job_count(%p[%d], %p[%s])",
                  con, con->http.fd, p, p->name);

  count = cupsdGetPrinterJobCount(p->name);

  ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                "queued-job-count", count);
}


/*
 * 'apply_printer_defaults()' - Apply printer default options to a job.
 */

static void
apply_printer_defaults(
    cupsd_printer_t *printer,		/* I - Printer */
    cupsd_job_t     *job)		/* I - Job */
{
  int		i,			/* Looping var */
		num_options;		/* Number of default options */
  cups_option_t	*options,		/* Default options */
		*option;		/* Current option */


 /*
  * Collect all of the default options and add the missing ones to the
  * job object...
  */

  for (i = printer->num_options, num_options = 0, option = printer->options;
       i > 0;
       i --, option ++)
    if (!ippFindAttribute(job->attrs, option->name, IPP_TAG_ZERO))
    {
      num_options = cupsAddOption(option->name, option->value, num_options,
                                  &options);
    }

 /*
  * Encode these options as attributes in the job object...
  */

  cupsEncodeOptions2(job->attrs, num_options, options, IPP_TAG_JOB);
  cupsFreeOptions(num_options, options);
}


/*
 * 'authenticate_job()' - Set job authentication info.
 */

static void
authenticate_job(cupsd_client_t  *con,	/* I - Client connection */
	         ipp_attribute_t *uri)	/* I - Job URI */
{
  ipp_attribute_t	*attr;		/* Job-id attribute */
  int			jobid;		/* Job ID */
  cupsd_job_t		*job;		/* Current job */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "authenticate_job(%p[%d], %s)",
                  con, con->http.fd, uri->values[0].string.text);

 /*
  * Start with "everything is OK" status...
  */

  con->response->request.status.status_code = IPP_OK;

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST, _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * See if the job has been completed...
  */

  if (job->state_value != IPP_JOB_HELD)
  {
   /*
    * Return a "not-possible" error...
    */

    send_ipp_status(con, IPP_NOT_POSSIBLE,
                    _("Job #%d is not held for authentication!"),
		    jobid);
    return;
  }

 /*
  * See if we have already authenticated...
  */

  if (!con->username[0])
  {
    send_ipp_status(con, IPP_NOT_AUTHORIZED,
                    _("No authentication information provided!"));
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * Save the authentication information for this job...
  */

  save_auth_info(con, job);

 /*
  * Reset the job-hold-until value to "no-hold"...
  */

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                               IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

  if (attr)
  {
    attr->value_tag = IPP_TAG_KEYWORD;
    cupsdSetString(&(attr->values[0].string.text), "no-hold");
  }

 /*
  * Release the job and return...
  */

  cupsdReleaseJob(job);

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d was authenticated by \"%s\".", jobid,
                  con->username);
}


/*
 * 'cancel_all_jobs()' - Cancel all print jobs.
 */

static void
cancel_all_jobs(cupsd_client_t  *con,	/* I - Client connection */
	        ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  http_status_t	status;			/* Policy status */
  cups_ptype_t	dtype;			/* Destination type */
  char		scheme[HTTP_MAX_URI],	/* Scheme portion of URI */
		userpass[HTTP_MAX_URI],	/* Username portion of URI */
		hostname[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  ipp_attribute_t *attr;		/* Attribute in request */
  const char	*username;		/* Username */
  int		purge;			/* Purge? */
  cupsd_printer_t *printer;		/* Printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cancel_all_jobs(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * See if we have a printer URI...
  */

  if (strcmp(uri->name, "printer-uri"))
  {
    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("The printer-uri attribute is required!"));
    return;
  }

 /*
  * Get the username (if any) for the jobs we want to cancel (only if
  * "my-jobs" is specified...
  */

  if ((attr = ippFindAttribute(con->request, "my-jobs",
                               IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
  {
    if ((attr = ippFindAttribute(con->request, "requesting-user-name",
                                 IPP_TAG_NAME)) != NULL)
      username = attr->values[0].string.text;
    else
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Missing requesting-user-name attribute!"));
      return;
    }
  }
  else
    username = NULL;

 /*
  * Look for the "purge-jobs" attribute...
  */

  if ((attr = ippFindAttribute(con->request, "purge-jobs",
                               IPP_TAG_BOOLEAN)) != NULL)
    purge = attr->values[0].boolean;
  else
    purge = 1;

 /*
  * And if the destination is valid...
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI?
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text,
                    scheme, sizeof(scheme), userpass, sizeof(userpass),
		    hostname, sizeof(hostname), &port,
		    resource, sizeof(resource));

    if ((!strncmp(resource, "/printers/", 10) && resource[10]) ||
        (!strncmp(resource, "/classes/", 9) && resource[9]))
    {
      send_ipp_status(con, IPP_NOT_FOUND,
                      _("The printer or class was not found."));
      return;
    }

   /*
    * Check policy...
    */

    if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
    {
      send_http_error(con, status);
      return;
    }

   /*
    * Cancel all jobs on all printers...
    */

    cupsdCancelJobs(NULL, username, purge);

    cupsdLogMessage(CUPSD_LOG_INFO, "All jobs were %s by \"%s\".",
                    purge ? "purged" : "canceled", get_username(con));
  }
  else
  {
   /*
    * Check policy...
    */

    if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con,
                                   NULL)) != HTTP_OK)
    {
      send_http_error(con, status);
      return;
    }

   /*
    * Cancel all of the jobs on the named printer...
    */

    cupsdCancelJobs(printer->name, username, purge);

    cupsdLogMessage(CUPSD_LOG_INFO, "All jobs on \"%s\" were %s by \"%s\".",
                    printer->name, purge ? "purged" : "canceled",
		    get_username(con));
  }

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'cancel_job()' - Cancel a print job.
 */

static void
cancel_job(cupsd_client_t  *con,	/* I - Client connection */
	   ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t *attr;		/* Current attribute */
  int		jobid;			/* Job ID */
  char		scheme[HTTP_MAX_URI],	/* Scheme portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_job_t	*job;			/* Job information */
  cups_ptype_t	dtype;			/* Destination type (printer or class) */
  cupsd_printer_t *printer;		/* Printer data */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cancel_job(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    if ((jobid = attr->values[0].integer) == 0)
    {
     /*
      * Find the current job on the specified printer...
      */

      if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
      {
       /*
	* Bad URI...
	*/

	send_ipp_status(con, IPP_NOT_FOUND,
                	_("The printer or class was not found."));
	return;
      }

     /*
      * See if the printer is currently printing a job...
      */

      if (printer->job)
        jobid = ((cupsd_job_t *)printer->job)->id;
      else
      {
       /*
        * No, see if there are any pending jobs...
	*/

        for (job = (cupsd_job_t *)cupsArrayFirst(ActiveJobs);
	     job;
	     job = (cupsd_job_t *)cupsArrayNext(ActiveJobs))
	  if (job->state_value <= IPP_JOB_PROCESSING &&
	      !strcasecmp(job->dest, printer->name))
	    break;

	if (job)
	  jobid = job->id;
	else
	{
	  send_ipp_status(con, IPP_NOT_POSSIBLE, _("No active jobs on %s!"),
	                  printer->name);
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

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, scheme,
                    sizeof(scheme), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * See if the job is already completed, canceled, or aborted; if so,
  * we can't cancel...
  */

  if (job->state_value >= IPP_JOB_CANCELED)
  {
    switch (job->state_value)
    {
      case IPP_JOB_CANCELED :
	  send_ipp_status(con, IPP_NOT_POSSIBLE,
                	  _("Job #%d is already canceled - can\'t cancel."),
			  jobid);
          break;

      case IPP_JOB_ABORTED :
	  send_ipp_status(con, IPP_NOT_POSSIBLE,
                	  _("Job #%d is already aborted - can\'t cancel."),
			  jobid);
          break;

      default :
	  send_ipp_status(con, IPP_NOT_POSSIBLE,
                	  _("Job #%d is already completed - can\'t cancel."),
			  jobid);
          break;
    }

    return;
  }

 /*
  * Cancel the job and return...
  */

  cupsdCancelJob(job, 0, IPP_JOB_CANCELED);
  cupsdCheckJobs();

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d was canceled by \"%s\".", jobid,
                  username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'cancel_subscription()' - Cancel a subscription.
 */

static void
cancel_subscription(
    cupsd_client_t *con,		/* I - Client connection */
    int            sub_id)		/* I - Subscription ID */
{
  http_status_t		status;		/* Policy status */
  cupsd_subscription_t	*sub;		/* Subscription */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cancel_subscription(con=%p[%d], sub_id=%d)",
                  con, con->http.fd, sub_id);

 /*
  * Is the subscription ID valid?
  */

  if ((sub = cupsdFindSubscription(sub_id)) == NULL)
  {
   /*
    * Bad subscription ID...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("notify-subscription-id %d no good!"), sub_id);
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(sub->dest ? sub->dest->op_policy_ptr :
                                             DefaultPolicyPtr,
                                 con, sub->owner)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Cancel the subscription...
  */

  cupsdDeleteSubscription(sub, 1);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'check_quotas()' - Check quotas for a printer and user.
 */

static int				/* O - 1 if OK, 0 if not */
check_quotas(cupsd_client_t  *con,	/* I - Client connection */
             cupsd_printer_t *p)	/* I - Printer or class */
{
  int		i;			/* Looping var */
  char		username[33];		/* Username */
  cupsd_quota_t	*q;			/* Quota data */
  struct passwd	*pw;			/* User password data */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "check_quotas(%p[%d], %p[%s])",
                  con, con->http.fd, p, p->name);

 /*
  * Check input...
  */

  if (!con || !p)
    return (0);

 /*
  * Figure out who is printing...
  */

  strlcpy(username, get_username(con), sizeof(username));

 /*
  * Check global active job limits for printers and users...
  */

  if (MaxJobsPerPrinter)
  {
   /*
    * Check if there are too many pending jobs on this printer...
    */

    if (cupsdGetPrinterJobCount(p->name) >= MaxJobsPerPrinter)
    {
      cupsdLogMessage(CUPSD_LOG_INFO, "Too many jobs for printer \"%s\"...",
                      p->name);
      return (0);
    }
  }

  if (MaxJobsPerUser)
  {
   /*
    * Check if there are too many pending jobs for this user...
    */

    if (cupsdGetUserJobCount(username) >= MaxJobsPerUser)
    {
      cupsdLogMessage(CUPSD_LOG_INFO, "Too many jobs for user \"%s\"...",
                      username);
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

        if (cupsdCheckGroup(username, pw, p->users[i] + 1))
	  break;
      }
      else if (!strcasecmp(username, p->users[i]))
	break;

    if ((i < p->num_users) == p->deny_users)
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Denying user \"%s\" access to printer \"%s\"...",
        	      username, p->name);
      return (0);
    }
  }

 /*
  * Check quotas...
  */

  if (p->k_limit || p->page_limit)
  {
    if ((q = cupsdUpdateQuota(p, username, 0, 0)) == NULL)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate quota data for user \"%s\"!",
                      username);
      return (0);
    }

    if ((q->k_count >= p->k_limit && p->k_limit) ||
        (q->page_count >= p->page_limit && p->page_limit))
    {
      cupsdLogMessage(CUPSD_LOG_INFO, "User \"%s\" is over the quota limit...",
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
copy_attribute(
    ipp_t           *to,		/* O - Destination request/response */
    ipp_attribute_t *attr,		/* I - Attribute to copy */
    int             quickcopy)		/* I - Do a quick copy? */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*toattr;	/* Destination attribute */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "copy_attribute(%p, %p[%s,%x,%x])", to, attr,
        	  attr->name ? attr->name : "(null)", attr->group_tag,
		  attr->value_tag);

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
	    toattr->values[i].string.text = _cupsStrAlloc(attr->values[i].string.text);
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
	          _cupsStrAlloc(attr->values[i].string.charset);
	    else
              toattr->values[i].string.charset =
	          toattr->values[0].string.charset;

	    toattr->values[i].string.text = _cupsStrAlloc(attr->values[i].string.text);
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
	    if ((toattr->values[i].unknown.data =
	             malloc(toattr->values[i].unknown.length)) == NULL)
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
copy_attrs(ipp_t        *to,		/* I - Destination request */
           ipp_t        *from,		/* I - Source request */
           cups_array_t *ra,		/* I - Requested attributes */
	   ipp_tag_t    group,		/* I - Group to copy */
	   int          quickcopy)	/* I - Do a quick copy? */
{
  ipp_attribute_t	*fromattr;	/* Source attribute */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "copy_attrs(to=%p, from=%p, ra=%p, group=%x, quickcopy=%d)",
		  to, from, ra, group, quickcopy);

  if (!to || !from)
    return;

  for (fromattr = from->attrs; fromattr; fromattr = fromattr->next)
  {
   /*
    * Filter attributes as needed...
    */

    if (group != IPP_TAG_ZERO && fromattr->group_tag != group &&
        fromattr->group_tag != IPP_TAG_ZERO && !fromattr->name)
      continue;

    if (!ra || cupsArrayFind(ra, fromattr->name))
      copy_attribute(to, fromattr, quickcopy);
  }
}


/*
 * 'copy_banner()' - Copy a banner file to the requests directory for the
 *                   specified job.
 */

static int				/* O - Size of banner file in kbytes */
copy_banner(cupsd_client_t *con,	/* I - Client connection */
            cupsd_job_t    *job,	/* I - Job information */
            const char     *name)	/* I - Name of banner */
{
  int		i;			/* Looping var */
  int		kbytes;			/* Size of banner file in kbytes */
  char		filename[1024];		/* Job filename */
  cupsd_banner_t *banner;		/* Pointer to banner */
  cups_file_t	*in;			/* Input file */
  cups_file_t	*out;			/* Output file */
  int		ch;			/* Character from file */
  char		attrname[255],		/* Name of attribute */
		*s;			/* Pointer into name */
  ipp_attribute_t *attr;		/* Attribute */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "copy_banner(%p[%d], %p[%d], %s)",
                  con, con->http.fd, job, job->id, name ? name : "(null)");

 /*
  * Find the banner; return if not found or "none"...
  */

  if (!name || !strcmp(name, "none") ||
      (banner = cupsdFindBanner(name)) == NULL)
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
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "copy_banner: Unable to create banner job file %s - %s",
                    filename, strerror(errno));
    job->num_files --;
    return (0);
  }

  fchmod(cupsFileNumber(out), 0640);
  fchown(cupsFileNumber(out), RunUser, Group);

 /*
  * Try the localized banner file under the subdirectory...
  */

  strlcpy(attrname, job->attrs->attrs->next->values[0].string.text,
          sizeof(attrname));
  if (strlen(attrname) > 2 && attrname[2] == '-')
  {
   /*
    * Convert ll-cc to ll_CC...
    */

    attrname[2] = '_';
    attrname[3] = toupper(attrname[3] & 255);
    attrname[4] = toupper(attrname[4] & 255);
  }

  snprintf(filename, sizeof(filename), "%s/banners/%s/%s", DataDir,
           attrname, name);

  if (access(filename, 0) && strlen(attrname) > 2)
  {
   /*
    * Wasn't able to find "ll_CC" locale file; try the non-national
    * localization banner directory.
    */

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

  if ((in = cupsFileOpen(filename, "r")) == NULL)
  {
    cupsFileClose(out);
    unlink(filename);
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "copy_banner: Unable to open banner template file %s - %s",
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
        if (!isalpha(ch & 255) && ch != '-' && ch != '?')
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

      if (!strcmp(s, "printer-name"))
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
	      if (!strncmp(s, "time-at-", 8))
	        cupsFilePuts(out, cupsdGetDateTime(attr->values[i].integer));
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
	      if (!strcasecmp(banner->filetype->type, "postscript"))
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
		    cupsFilePrintf(out, "\\%03o", *p & 255);
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

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets",
                               IPP_TAG_INTEGER)) != NULL)
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


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "copy_file(\"%s\", \"%s\")", from, to);

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
copy_model(cupsd_client_t *con,		/* I - Client connection */
           const char     *from,	/* I - Source file */
           const char     *to)		/* I - Destination file */
{
  fd_set	*input;			/* select() input set */
  struct timeval timeout;		/* select() timeout */
  int		maxfd;			/* Maximum file descriptor for select() */
  char		tempfile[1024];		/* Temporary PPD file */
  int		tempfd;			/* Temporary PPD file descriptor */
  int		temppid;		/* Process ID of cups-driverd */
  int		temppipe[2];		/* Temporary pipes */
  char		*argv[4],		/* Command-line arguments */
		*envp[MAX_ENV];		/* Environment */
  cups_file_t	*src,			/* Source file */
		*dst;			/* Destination file */
  int		bytes,			/* Bytes from pipe */
		total;			/* Total bytes from pipe */
  char		buffer[2048],		/* Copy buffer */
		*ptr;			/* Pointer into buffer */
  int		i;			/* Looping var */
  char		option[PPD_MAX_NAME],	/* Option name */
		choice[PPD_MAX_NAME];	/* Choice name */
  int		num_defaults;		/* Number of default options */
  ppd_default_t	*defaults;		/* Default options */
  char		cups_protocol[PPD_MAX_LINE];
					/* cupsProtocol attribute */
  int		have_letter,		/* Have Letter size */
		have_a4;		/* Have A4 size */
#ifdef HAVE_LIBPAPER
  char		*paper_result;		/* Paper size name from libpaper */
  char		system_paper[64];	/* Paper size name buffer */
#endif /* HAVE_LIBPAPER */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
        	  "copy_model(con=%p, from=\"%s\", to=\"%s\")",
        	  con, from, to);

 /*
  * Run cups-driverd to get the PPD file...
  */

  argv[0] = "cups-driverd";
  argv[1] = "cat";
  argv[2] = (char *)from;
  argv[3] = NULL;

  cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

  snprintf(buffer, sizeof(buffer), "%s/daemon/cups-driverd", ServerBin);
  snprintf(tempfile, sizeof(tempfile), "%s/%d.ppd", TempDir, con->http.fd);
  tempfd = open(tempfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (tempfd < 0)
    return (-1);

  cupsdOpenPipe(temppipe);

  if ((input = calloc(1, SetSize)) == NULL)
  {
    close(tempfd);
    unlink(tempfile);

    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "copy_model: Unable to allocate %d bytes for select()...",
                    SetSize);
    return (-1);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "copy_model: Running \"cups-driverd cat %s\"...", from);

  if (!cupsdStartProcess(buffer, argv, envp, -1, temppipe[1], CGIPipes[1],
                         -1, 0, &temppid))
  {
    free(input);
    close(tempfd);
    unlink(tempfile);
    return (-1);
  }

  close(temppipe[1]);

 /*
  * Wait up to 30 seconds for the PPD file to be copied...
  */

  total = 0;

  if (temppipe[0] > CGIPipes[0])
    maxfd = temppipe[0] + 1;
  else
    maxfd = CGIPipes[0] + 1;

  for (;;)
  {
   /*
    * See if we have data ready...
    */

    bytes = 0;

    FD_SET(temppipe[0], input);
    FD_SET(CGIPipes[0], input);

    timeout.tv_sec  = 30;
    timeout.tv_usec = 0;

    if ((i = select(maxfd, input, NULL, NULL, &timeout)) < 0)
    {
      if (errno == EINTR)
        continue;
      else
        break;
    }
    else if (i == 0)
    {
     /*
      * We have timed out...
      */

      break;
    }

    if (FD_ISSET(temppipe[0], input))
    {
     /*
      * Read the PPD file from the pipe, and write it to the PPD file.
      */

      if ((bytes = read(temppipe[0], buffer, sizeof(buffer))) > 0)
      {
	if (write(tempfd, buffer, bytes) < bytes)
          break;

	total += bytes;
      }
      else
	break;
    }

    if (FD_ISSET(CGIPipes[0], input))
      cupsdUpdateCGI();
  }

  close(temppipe[0]);
  close(tempfd);

  free(input);

  if (!total)
  {
   /*
    * No data from cups-deviced...
    */

    cupsdLogMessage(CUPSD_LOG_ERROR, "copy_model: empty PPD file!");
    unlink(tempfile);
    return (-1);
  }

 /*
  * Read the source file and see what page sizes are supported...
  */

  if ((src = cupsFileOpen(tempfile, "rb")) == NULL)
  {
    unlink(tempfile);
    return (-1);
  }

  have_letter = 0;
  have_a4     = 0;

  while (cupsFileGets(src, buffer, sizeof(buffer)))
    if (!strncmp(buffer, "*PageSize ", 10))
    {
     /*
      * Strip UI text and command data from the end of the line...
      */

      if ((ptr = strchr(buffer + 10, '/')) != NULL)
        *ptr = '\0';
      if ((ptr = strchr(buffer + 10, ':')) != NULL)
        *ptr = '\0';

      for (ptr = buffer + 10; isspace(*ptr); ptr ++);

     /*
      * Look for Letter and A4 page sizes...
      */

      if (!strcmp(ptr, "Letter"))
	have_letter = 1;

      if (!strcmp(ptr, "A4"))
	have_a4 = 1;
    }

  cupsFileRewind(src);

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

    while (cupsFileGets(dst, buffer, sizeof(buffer)))
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
    system_paper[0] = toupper(system_paper[0] & 255);

    if ((!strcmp(system_paper, "Letter") && have_letter) ||
        (!strcmp(system_paper, "A4") && have_a4))
    {
      num_defaults = ppd_add_default("PageSize", system_paper,
				     num_defaults, &defaults);
      num_defaults = ppd_add_default("PageRegion", system_paper,
				     num_defaults, &defaults);
      num_defaults = ppd_add_default("PaperDimension", system_paper,
				     num_defaults, &defaults);
      num_defaults = ppd_add_default("ImageableArea", system_paper,
				     num_defaults, &defaults);
    }
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

      if (have_letter)
      {
	num_defaults = ppd_add_default("PageSize", "Letter", num_defaults,
                                       &defaults);
	num_defaults = ppd_add_default("PageRegion", "Letter", num_defaults,
                                       &defaults);
	num_defaults = ppd_add_default("PaperDimension", "Letter", num_defaults,
                                       &defaults);
	num_defaults = ppd_add_default("ImageableArea", "Letter", num_defaults,
                                       &defaults);
      }
    }
    else if (have_a4)
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
  * Open the destination file for a copy...
  */

  if ((dst = cupsFileOpen(to, "wb")) == NULL)
  {
    if (num_defaults > 0)
      free(defaults);

    cupsFileClose(src);
    unlink(tempfile);
    return (-1);
  }

 /*
  * Copy the source file to the destination...
  */

  while (cupsFileGets(src, buffer, sizeof(buffer)))
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

  if (num_defaults > 0)
    free(defaults);

 /*
  * Close both files and return...
  */

  cupsFileClose(src);

  unlink(tempfile);

  return (cupsFileClose(dst));
}


/*
 * 'copy_job_attrs()' - Copy job attributes.
 */

static void
copy_job_attrs(cupsd_client_t *con,	/* I - Client connection */
	       cupsd_job_t    *job,	/* I - Job */
	       cups_array_t   *ra)	/* I - Requested attributes array */
{
  char	job_uri[HTTP_MAX_URI];		/* Job URI */


 /*
  * Send the requested attributes for each job...
  */

  httpAssembleURIf(HTTP_URI_CODING_ALL, job_uri, sizeof(job_uri), "ipp", NULL,
                   con->servername, con->serverport, "/jobs/%d",
        	   job->id);

  if (!ra || cupsArrayFind(ra, "job-more-info"))
    ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
        	 "job-more-info", NULL, job_uri);

  if (job->state_value > IPP_JOB_PROCESSING &&
      (!ra || cupsArrayFind(ra, "job-preserved")))
    ippAddBoolean(con->response, IPP_TAG_JOB, "job-preserved",
                  job->num_files > 0);

  if (!ra || cupsArrayFind(ra, "job-printer-up-time"))
    ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER,
                  "job-printer-up-time", time(NULL));

  if (!ra || cupsArrayFind(ra, "job-state-reasons"))
    add_job_state_reasons(con, job);

  if (!ra || cupsArrayFind(ra, "job-uri"))
    ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI,
        	 "job-uri", NULL, job_uri);

  copy_attrs(con->response, job->attrs, ra, IPP_TAG_JOB, 0);
}


/*
 * 'copy_printer_attrs()' - Copy printer attributes.
 */

static void
copy_printer_attrs(
    cupsd_client_t  *con,		/* I - Client connection */
    cupsd_printer_t *printer,		/* I - Printer */
    cups_array_t    *ra)		/* I - Requested attributes array */
{
  char			printer_uri[HTTP_MAX_URI];
					/* Printer URI */
  time_t		curtime;	/* Current time */
  int			i;		/* Looping var */
  ipp_attribute_t	*history;	/* History collection */


 /*
  * Copy the printer attributes to the response using requested-attributes
  * and document-format attributes that may be provided by the client.
  */

  curtime = time(NULL);

#ifdef __APPLE__
  if ((!ra || cupsArrayFind(ra, "com.apple.print.recoverable-message")) &&
      printer->recoverable)
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                 "com.apple.print.recoverable-message", NULL,
		 printer->recoverable);
#endif /* __APPLE__ */

  if (!ra || cupsArrayFind(ra, "printer-current-time"))
    ippAddDate(con->response, IPP_TAG_PRINTER, "printer-current-time",
               ippTimeToDate(curtime));

  if (!ra || cupsArrayFind(ra, "printer-error-policy"))
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_NAME,
        	 "printer-error-policy", NULL, printer->error_policy);

  if (!ra || cupsArrayFind(ra, "printer-is-accepting-jobs"))
    ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-accepting-jobs",
                  printer->accepting);

  if (!ra || cupsArrayFind(ra, "printer-is-shared"))
    ippAddBoolean(con->response, IPP_TAG_PRINTER, "printer-is-shared",
                  printer->shared);

  if (!ra || cupsArrayFind(ra, "printer-op-policy"))
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_NAME,
        	 "printer-op-policy", NULL, printer->op_policy);

  if (!ra || cupsArrayFind(ra, "printer-state"))
    ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-state",
                  printer->state);

  if (!ra || cupsArrayFind(ra, "printer-state-change-time"))
    ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "printer-state-change-time", printer->state_time);

  if (MaxPrinterHistory > 0 && printer->num_history > 0 &&
      cupsArrayFind(ra, "printer-state-history"))
  {
   /*
    * Printer history is only sent if specifically requested, so that
    * older CUPS/IPP clients won't barf on the collection attributes.
    */

    history = ippAddCollections(con->response, IPP_TAG_PRINTER,
                                "printer-state-history",
                                printer->num_history, NULL);

    for (i = 0; i < printer->num_history; i ++)
      copy_attrs(history->values[i].collection = ippNew(), printer->history[i],
                 NULL, IPP_TAG_ZERO, 0);
  }

  if (!ra || cupsArrayFind(ra, "printer-state-message"))
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_TEXT,
        	 "printer-state-message", NULL, printer->state_message);

  if (!ra || cupsArrayFind(ra, "printer-state-reasons"))
    add_printer_state_reasons(con, printer);

  if (!ra || cupsArrayFind(ra, "printer-type"))
  {
    int type;				/* printer-type value */


   /*
    * Add the CUPS-specific printer-type attribute...
    */

    type = printer->type;

    if (printer == DefaultPrinter)
      type |= CUPS_PRINTER_DEFAULT;

    if (!printer->accepting)
      type |= CUPS_PRINTER_REJECTING;

    if (!printer->shared)
      type |= CUPS_PRINTER_NOT_SHARED;

    ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_ENUM, "printer-type",
		  type);
  }

  if (!ra || cupsArrayFind(ra, "printer-up-time"))
    ippAddInteger(con->response, IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  "printer-up-time", curtime);

  if ((!ra || cupsArrayFind(ra, "printer-uri-supported")) &&
      !ippFindAttribute(printer->attrs, "printer-uri-supported",
                        IPP_TAG_URI))
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, printer_uri, sizeof(printer_uri),
                     "ipp", NULL, con->servername, con->serverport,
		     (printer->type & CUPS_PRINTER_CLASS) ?
		         "/classes/%s" : "/printers/%s", printer->name);
    ippAddString(con->response, IPP_TAG_PRINTER, IPP_TAG_URI,
        	 "printer-uri-supported", NULL, printer_uri);
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "printer-uri-supported=\"%s\"",
                    printer_uri);
  }

  if (!ra || cupsArrayFind(ra, "queued-job-count"))
    add_queued_job_count(con, printer);

  copy_attrs(con->response, printer->attrs, ra, IPP_TAG_ZERO, 0);
  copy_attrs(con->response, CommonData, ra, IPP_TAG_ZERO, IPP_TAG_COPY);
}


/*
 * 'copy_subscription_attrs()' - Copy subscription attributes.
 */

static void
copy_subscription_attrs(
    cupsd_client_t       *con,		/* I - Client connection */
    cupsd_subscription_t *sub,		/* I - Subscription */
    cups_array_t         *ra)		/* I - Requested attributes array */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  char			printer_uri[HTTP_MAX_URI];
					/* Printer URI */
  int			count;		/* Number of events */
  unsigned		mask;		/* Current event mask */
  const char		*name;		/* Current event name */


 /*
  * Copy the subscription attributes to the response using the
  * requested-attributes attribute that may be provided by the client.
  */

  if (!ra || cupsArrayFind(ra, "notify-events"))
  {
    if ((name = cupsdEventName((cupsd_eventmask_t)sub->mask)) != NULL)
    {
     /*
      * Simple event list...
      */

      ippAddString(con->response, IPP_TAG_SUBSCRIPTION,
                   (ipp_tag_t)(IPP_TAG_KEYWORD | IPP_TAG_COPY),
                   "notify-events", NULL, name);
    }
    else
    {
     /*
      * Complex event list...
      */

      for (mask = 1, count = 0; mask < CUPSD_EVENT_ALL; mask <<= 1)
	if (sub->mask & mask)
          count ++;

      attr = ippAddStrings(con->response, IPP_TAG_SUBSCRIPTION,
                           (ipp_tag_t)(IPP_TAG_KEYWORD | IPP_TAG_COPY),
                           "notify-events", count, NULL, NULL);

      for (mask = 1, count = 0; mask < CUPSD_EVENT_ALL; mask <<= 1)
	if (sub->mask & mask)
	{
          attr->values[count].string.text =
	      (char *)cupsdEventName((cupsd_eventmask_t)mask);

          count ++;
	}
    }
  }

  if (sub->job && (!ra || cupsArrayFind(ra, "notify-job-id")))
    ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                  "notify-job-id", sub->job->id);

  if (!sub->job && (!ra || cupsArrayFind(ra, "notify-lease-duration")))
    ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                  "notify-lease-duration", sub->lease);

  if (sub->dest && (!ra || cupsArrayFind(ra, "notify-printer-uri")))
  {
    httpAssembleURIf(HTTP_URI_CODING_ALL, printer_uri, sizeof(printer_uri),
                     "ipp", NULL, con->servername, con->serverport,
		     "/printers/%s", sub->dest->name);
    ippAddString(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI,
        	 "notify-printer-uri", NULL, printer_uri);
  }

  if (sub->recipient && (!ra || cupsArrayFind(ra, "notify-recipient-uri")))
    ippAddString(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI,
        	 "notify-recipient-uri", NULL, sub->recipient);
  else if (!ra || cupsArrayFind(ra, "notify-pull-method"))
    ippAddString(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD,
                 "notify-pull-method", NULL, "ippget");

  if (!ra || cupsArrayFind(ra, "notify-subscriber-user-name"))
    ippAddString(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_NAME,
        	 "notify-subscriber-user-name", NULL, sub->owner);

  if (!ra || cupsArrayFind(ra, "notify-subscription-id"))
    ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                  "notify-subscription-id", sub->id);

  if (!ra || cupsArrayFind(ra, "notify-time-interval"))
    ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                  "notify-time-interval", sub->interval);

  if (sub->user_data_len > 0 && (!ra || cupsArrayFind(ra, "notify-user-data")))
    ippAddOctetString(con->response, IPP_TAG_SUBSCRIPTION, "notify-user-data",
                      sub->user_data, sub->user_data_len);
}


/*
 * 'create_job()' - Print a file to a printer or class.
 */

static void
create_job(cupsd_client_t  *con,	/* I - Client connection */
	   ipp_attribute_t *uri)	/* I - Printer URI */
{
  cupsd_printer_t	*printer;	/* Printer */
  cupsd_job_t		*job;		/* New job */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "create_job(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, NULL, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Create the job object...
  */

  if ((job = add_job(con, printer, NULL)) == NULL)
    return;

 /*
  * Save and log the job...
  */

  cupsdSaveJob(job);

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d created on \"%s\" by \"%s\".",
                  job->id, job->dest, job->username);
}


/*
 * 'create_requested_array()' - Create an array for the requested-attributes.
 */

static cups_array_t *			/* O - Array of attributes or NULL */
create_requested_array(ipp_t *request)	/* I - IPP request */
{
  int			i;		/* Looping var */
  ipp_attribute_t	*requested;	/* requested-attributes attribute */
  cups_array_t		*ra;		/* Requested attributes array */
  char			*value;		/* Current value */


 /*
  * Get the requested-attributes attribute, and return NULL if we don't
  * have one...
  */

  if ((requested = ippFindAttribute(request, "requested-attributes",
                                    IPP_TAG_KEYWORD)) == NULL)
    return (NULL);

 /*
  * If the attribute contains a single "all" keyword, return NULL...
  */

  if (requested->num_values == 1 &&
      !strcmp(requested->values[0].string.text, "all"))
    return (NULL);

 /*
  * Create an array using "strcmp" as the comparison function...
  */

  ra = cupsArrayNew((cups_array_func_t)strcmp, NULL);

  for (i = 0; i < requested->num_values; i ++)
  {
    value = requested->values[i].string.text;

    if (!strcmp(value, "job-template"))
    {
      cupsArrayAdd(ra, "copies");
      cupsArrayAdd(ra, "copies-default");
      cupsArrayAdd(ra, "copies-supported");
      cupsArrayAdd(ra, "finishings");
      cupsArrayAdd(ra, "finishings-default");
      cupsArrayAdd(ra, "finishings-supported");
      cupsArrayAdd(ra, "job-hold-until");
      cupsArrayAdd(ra, "job-hold-until-default");
      cupsArrayAdd(ra, "job-hold-until-supported");
      cupsArrayAdd(ra, "job-priority");
      cupsArrayAdd(ra, "job-priority-default");
      cupsArrayAdd(ra, "job-priority-supported");
      cupsArrayAdd(ra, "job-sheets");
      cupsArrayAdd(ra, "job-sheets-default");
      cupsArrayAdd(ra, "job-sheets-supported");
      cupsArrayAdd(ra, "media");
      cupsArrayAdd(ra, "media-default");
      cupsArrayAdd(ra, "media-supported");
      cupsArrayAdd(ra, "multiple-document-handling");
      cupsArrayAdd(ra, "multiple-document-handling-default");
      cupsArrayAdd(ra, "multiple-document-handling-supported");
      cupsArrayAdd(ra, "number-up");
      cupsArrayAdd(ra, "number-up-default");
      cupsArrayAdd(ra, "number-up-supported");
      cupsArrayAdd(ra, "orientation-requested");
      cupsArrayAdd(ra, "orientation-requested-default");
      cupsArrayAdd(ra, "orientation-requested-supported");
      cupsArrayAdd(ra, "page-ranges");
      cupsArrayAdd(ra, "page-ranges-supported");
      cupsArrayAdd(ra, "printer-resolution");
      cupsArrayAdd(ra, "printer-resolution-default");
      cupsArrayAdd(ra, "printer-resolution-supported");
      cupsArrayAdd(ra, "print-quality");
      cupsArrayAdd(ra, "print-quality-default");
      cupsArrayAdd(ra, "print-quality-supported");
      cupsArrayAdd(ra, "sides");
      cupsArrayAdd(ra, "sides-default");
      cupsArrayAdd(ra, "sides-supported");
    }
    else if (!strcmp(value, "job-description"))
    {
      cupsArrayAdd(ra, "date-time-at-completed");
      cupsArrayAdd(ra, "date-time-at-creation");
      cupsArrayAdd(ra, "date-time-at-processing");
      cupsArrayAdd(ra, "job-detailed-status-message");
      cupsArrayAdd(ra, "job-document-access-errors");
      cupsArrayAdd(ra, "job-id");
      cupsArrayAdd(ra, "job-impressions");
      cupsArrayAdd(ra, "job-impressions-completed");
      cupsArrayAdd(ra, "job-k-octets");
      cupsArrayAdd(ra, "job-k-octets-processed");
      cupsArrayAdd(ra, "job-media-sheets");
      cupsArrayAdd(ra, "job-media-sheets-completed");
      cupsArrayAdd(ra, "job-message-from-operator");
      cupsArrayAdd(ra, "job-more-info");
      cupsArrayAdd(ra, "job-name");
      cupsArrayAdd(ra, "job-originating-user-name");
      cupsArrayAdd(ra, "job-printer-up-time");
      cupsArrayAdd(ra, "job-printer-uri");
      cupsArrayAdd(ra, "job-state");
      cupsArrayAdd(ra, "job-state-message");
      cupsArrayAdd(ra, "job-state-reasons");
      cupsArrayAdd(ra, "job-uri");
      cupsArrayAdd(ra, "number-of-documents");
      cupsArrayAdd(ra, "number-of-intervening-jobs");
      cupsArrayAdd(ra, "output-device-assigned");
      cupsArrayAdd(ra, "time-at-completed");
      cupsArrayAdd(ra, "time-at-creation");
      cupsArrayAdd(ra, "time-at-processing");
    }
    else if (!strcmp(value, "printer-description"))
    {
      cupsArrayAdd(ra, "charset-configured");
      cupsArrayAdd(ra, "charset-supported");
      cupsArrayAdd(ra, "color-supported");
      cupsArrayAdd(ra, "compression-supported");
      cupsArrayAdd(ra, "document-format-default");
      cupsArrayAdd(ra, "document-format-supported");
      cupsArrayAdd(ra, "generated-natural-language-supported");
      cupsArrayAdd(ra, "ipp-versions-supported");
      cupsArrayAdd(ra, "job-impressions-supported");
      cupsArrayAdd(ra, "job-k-octets-supported");
      cupsArrayAdd(ra, "job-media-sheets-supported");
      cupsArrayAdd(ra, "multiple-document-jobs-supported");
      cupsArrayAdd(ra, "multiple-operation-time-out");
      cupsArrayAdd(ra, "natural-language-configured");
      cupsArrayAdd(ra, "notify-attributes-supported");
      cupsArrayAdd(ra, "notify-lease-duration-default");
      cupsArrayAdd(ra, "notify-lease-duration-supported");
      cupsArrayAdd(ra, "notify-max-events-supported");
      cupsArrayAdd(ra, "notify-events-default");
      cupsArrayAdd(ra, "notify-events-supported");
      cupsArrayAdd(ra, "notify-pull-method-supported");
      cupsArrayAdd(ra, "notify-schemes-supported");
      cupsArrayAdd(ra, "operations-supported");
      cupsArrayAdd(ra, "pages-per-minute");
      cupsArrayAdd(ra, "pages-per-minute-color");
      cupsArrayAdd(ra, "pdl-override-supported");
      cupsArrayAdd(ra, "printer-current-time");
      cupsArrayAdd(ra, "printer-driver-installer");
      cupsArrayAdd(ra, "printer-info");
      cupsArrayAdd(ra, "printer-is-accepting-jobs");
      cupsArrayAdd(ra, "printer-location");
      cupsArrayAdd(ra, "printer-make-and-model");
      cupsArrayAdd(ra, "printer-message-from-operator");
      cupsArrayAdd(ra, "printer-more-info");
      cupsArrayAdd(ra, "printer-more-info-manufacturer");
      cupsArrayAdd(ra, "printer-name");
      cupsArrayAdd(ra, "printer-state");
      cupsArrayAdd(ra, "printer-state-message");
      cupsArrayAdd(ra, "printer-state-reasons");
      cupsArrayAdd(ra, "printer-up-time");
      cupsArrayAdd(ra, "printer-uri-supported");
      cupsArrayAdd(ra, "queued-job-count");
      cupsArrayAdd(ra, "reference-uri-schemes-supported");
      cupsArrayAdd(ra, "uri-authentication-supported");
      cupsArrayAdd(ra, "uri-security-supported");
    }
    else if (!strcmp(value, "subscription-template"))
    {
      cupsArrayAdd(ra, "notify-attributes");
      cupsArrayAdd(ra, "notify-charset");
      cupsArrayAdd(ra, "notify-events");
      cupsArrayAdd(ra, "notify-lease-duration");
      cupsArrayAdd(ra, "notify-natural-language");
      cupsArrayAdd(ra, "notify-pull-method");
      cupsArrayAdd(ra, "notify-recipient-uri");
      cupsArrayAdd(ra, "notify-time-interval");
      cupsArrayAdd(ra, "notify-user-data");
    }
    else
      cupsArrayAdd(ra, value);
  }

  return (ra);
}


/*
 * 'create_subscription()' - Create a notification subscription.
 */

static void
create_subscription(
    cupsd_client_t  *con,		/* I - Client connection */
    ipp_attribute_t *uri)		/* I - Printer URI */
{
  http_status_t	status;			/* Policy status */
  int			i;		/* Looping var */
  ipp_attribute_t	*attr;		/* Current attribute */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			scheme[HTTP_MAX_URI],
					/* Scheme portion of URI */
			userpass[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  cupsd_printer_t	*printer;	/* Printer/class */
  cupsd_job_t		*job;		/* Job */
  int			jobid;		/* Job ID */
  cupsd_subscription_t	*sub;		/* Subscription object */
  const char		*username,	/* requesting-user-name or authenticated username */
			*recipient,	/* notify-recipient-uri */
			*pullmethod;	/* notify-pull-method */
  ipp_attribute_t	*user_data;	/* notify-user-data */
  int			interval,	/* notify-time-interval */
			lease;		/* notify-lease-duration */
  unsigned		mask;		/* notify-events */


#ifdef DEBUG
  for (attr = con->request->attrs; attr; attr = attr->next)
  {
    if (attr->group_tag != IPP_TAG_ZERO)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "g%04x v%04x %s", attr->group_tag,
                      attr->value_tag, attr->name);
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG, "----SEP----");
  }
#endif /* DEBUG */

 /*
  * Is the destination valid?
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdCreateSubscription(con=%p(%d), uri=\"%s\")",
                  con, con->http.fd, uri->values[0].string.text);

  httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, scheme,
                  sizeof(scheme), userpass, sizeof(userpass), host,
		  sizeof(host), &port, resource, sizeof(resource));

  if (!strcmp(resource, "/"))
  {
    dtype   = (cups_ptype_t)0;
    printer = NULL;
  }
  else if (!strncmp(resource, "/printers", 9) && strlen(resource) <= 10)
  {
    dtype   = (cups_ptype_t)0;
    printer = NULL;
  }
  else if (!strncmp(resource, "/classes", 8) && strlen(resource) <= 9)
  {
    dtype   = CUPS_PRINTER_CLASS;
    printer = NULL;
  }
  else if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if (printer)
  {
    if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con,
                                   NULL)) != HTTP_OK)
    {
      send_http_error(con, status);
      return;
    }
  }
  else if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Get the user that is requesting the subscription...
  */

  username = get_username(con);

 /*
  * Find the first subscription group attribute; return if we have
  * none...
  */

  for (attr = con->request->attrs; attr; attr = attr->next)
    if (attr->group_tag == IPP_TAG_SUBSCRIPTION)
      break;

  if (!attr)
  {
    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("No subscription attributes in request!"));
    return;
  }

 /*
  * Process the subscription attributes in the request...
  */

  con->response->request.status.status_code = IPP_BAD_REQUEST;

  while (attr)
  {
    recipient = NULL;
    pullmethod = NULL;
    user_data  = NULL;
    interval   = 0;
    lease      = DefaultLeaseDuration;
    jobid      = 0;
    mask       = CUPSD_EVENT_NONE;

    while (attr && attr->group_tag != IPP_TAG_ZERO)
    {
      if (!strcmp(attr->name, "notify-recipient") &&
          attr->value_tag == IPP_TAG_URI)
      {
       /*
        * Validate the recipient scheme against the ServerBin/notifier
	* directory...
	*/

	char	notifier[1024];		/* Notifier filename */


        recipient = attr->values[0].string.text;

	if (httpSeparateURI(HTTP_URI_CODING_ALL, recipient,
	                    scheme, sizeof(scheme), userpass, sizeof(userpass),
			    host, sizeof(host), &port,
			    resource, sizeof(resource)) < HTTP_URI_OK)
        {
          send_ipp_status(con, IPP_NOT_POSSIBLE,
	                  _("Bad notify-recipient URI \"%s\"!"), recipient);
	  ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM,
	                "notify-status-code", IPP_URI_SCHEME);
	  return;
	}

        snprintf(notifier, sizeof(notifier), "%s/notifier/%s", ServerBin,
	         scheme);
        if (access(notifier, X_OK))
	{
          send_ipp_status(con, IPP_NOT_POSSIBLE,
	                  _("notify-recipient URI \"%s\" uses unknown scheme!"),
			  recipient);
	  ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM,
	                "notify-status-code", IPP_URI_SCHEME);
	  return;
	}
      }
      else if (!strcmp(attr->name, "notify-pull-method") &&
               attr->value_tag == IPP_TAG_KEYWORD)
      {
        pullmethod = attr->values[0].string.text;

        if (strcmp(pullmethod, "ippget"))
	{
          send_ipp_status(con, IPP_NOT_POSSIBLE,
	                  _("Bad notify-pull-method \"%s\"!"), pullmethod);
	  ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM,
	                "notify-status-code", IPP_ATTRIBUTES);
	  return;
	}
      }
      else if (!strcmp(attr->name, "notify-charset") &&
               attr->value_tag == IPP_TAG_CHARSET &&
	       strcmp(attr->values[0].string.text, "us-ascii") &&
	       strcmp(attr->values[0].string.text, "utf-8"))
      {
        send_ipp_status(con, IPP_CHARSET,
	                _("Character set \"%s\" not supported!"),
			attr->values[0].string.text);
	return;
      }
      else if (!strcmp(attr->name, "notify-natural-language") &&
               (attr->value_tag != IPP_TAG_LANGUAGE ||
	        strcmp(attr->values[0].string.text, DefaultLanguage)))
      {
        send_ipp_status(con, IPP_CHARSET,
	                _("Language \"%s\" not supported!"),
			attr->values[0].string.text);
	return;
      }
      else if (!strcmp(attr->name, "notify-user-data") &&
               attr->value_tag == IPP_TAG_STRING)
      {
        if (attr->num_values > 1 || attr->values[0].unknown.length > 63)
	{
          send_ipp_status(con, IPP_REQUEST_VALUE,
	                  _("The notify-user-data value is too large "
			    "(%d > 63 octets)!"),
			  attr->values[0].unknown.length);
	  return;
	}

        user_data = attr;
      }
      else if (!strcmp(attr->name, "notify-events") &&
               attr->value_tag == IPP_TAG_KEYWORD)
      {
        for (i = 0; i < attr->num_values; i ++)
	  mask |= cupsdEventValue(attr->values[i].string.text);
      }
      else if (!strcmp(attr->name, "notify-lease-duration") &&
               attr->value_tag == IPP_TAG_INTEGER)
        lease = attr->values[0].integer;
      else if (!strcmp(attr->name, "notify-time-interval") &&
               attr->value_tag == IPP_TAG_INTEGER)
        interval = attr->values[0].integer;
      else if (!strcmp(attr->name, "notify-job-id") &&
               attr->value_tag == IPP_TAG_INTEGER)
        jobid = attr->values[0].integer;

      attr = attr->next;
    }

    if (recipient)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "recipient=\"%s\"", recipient);
    if (pullmethod)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "pullmethod=\"%s\"", pullmethod);
    cupsdLogMessage(CUPSD_LOG_DEBUG, "notify-lease-duration=%d", lease);
    cupsdLogMessage(CUPSD_LOG_DEBUG, "notify-time-interval=%d", interval);

    if (!recipient && !pullmethod)
      break;

    if (mask == CUPSD_EVENT_NONE)
    {
      if (jobid)
        mask = CUPSD_EVENT_JOB_COMPLETED;
      else if (printer)
        mask = CUPSD_EVENT_PRINTER_STATE_CHANGED;
      else
      {
        send_ipp_status(con, IPP_BAD_REQUEST,
	                _("notify-events not specified!"));
	return;
      }
    }

    if (MaxLeaseDuration && (lease == 0 || lease > MaxLeaseDuration))
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "create_subscription: Limiting notify-lease-duration to "
		      "%d seconds.",
		      MaxLeaseDuration);
      lease = MaxLeaseDuration;
    }

    if (jobid)
    {
      if ((job = cupsdFindJob(jobid)) == NULL)
      {
	send_ipp_status(con, IPP_NOT_FOUND, _("Job %d not found!"), jobid);
	return;
      }
    }
    else
      job = NULL;

    sub = cupsdAddSubscription(mask, printer, job, recipient, 0);

    if (job)
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Added subscription %d for job %d",
		      sub->id, job->id);
    else if (printer)
      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Added subscription %d for printer \"%s\"",
		      sub->id, printer->name);
    else
      cupsdLogMessage(CUPSD_LOG_DEBUG, "Added subscription %d for server",
		      sub->id);

    sub->interval = interval;
    sub->lease    = lease;
    sub->expire   = lease ? time(NULL) + lease : 0;

    cupsdSetString(&sub->owner, username);

    if (user_data)
    {
      sub->user_data_len = user_data->values[0].unknown.length;
      memcpy(sub->user_data, user_data->values[0].unknown.data,
             sub->user_data_len);
    }

    ippAddSeparator(con->response);
    ippAddInteger(con->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                  "notify-subscription-id", sub->id);

    con->response->request.status.status_code = IPP_OK;

    if (attr)
      attr = attr->next;
  }

  cupsdSaveAllSubscriptions();

}


/*
 * 'delete_printer()' - Remove a printer or class from the system.
 */

static void
delete_printer(cupsd_client_t  *con,	/* I - Client connection */
               ipp_attribute_t *uri)	/* I - URI of printer or class */
{
  http_status_t	status;			/* Policy status */
  cups_ptype_t	dtype;			/* Destination type (printer or class) */
  cupsd_printer_t *printer;		/* Printer/class */
  char		filename[1024];		/* Script/PPD filename */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "delete_printer(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Do we have a valid URI?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Remove old jobs...
  */

  cupsdCancelJobs(printer->name, NULL, 1);

 /*
  * Remove old subscriptions and send a "deleted printer" event...
  */

  cupsdAddEvent(CUPSD_EVENT_PRINTER_DELETED, printer, NULL,
                "%s \"%s\" deleted by \"%s\".",
		(dtype & CUPS_PRINTER_CLASS) ? "Class" : "Printer",
		printer->name, get_username(con));

  cupsdExpireSubscriptions(printer, NULL);

 /*
  * Remove any old PPD or script files...
  */

  snprintf(filename, sizeof(filename), "%s/interfaces/%s", ServerRoot,
           printer->name);
  unlink(filename);

  snprintf(filename, sizeof(filename), "%s/ppd/%s.ppd", ServerRoot,
           printer->name);
  unlink(filename);

  if (dtype & CUPS_PRINTER_CLASS)
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Class \"%s\" deleted by \"%s\".",
                    printer->name, get_username(con));

    cupsdDeletePrinter(printer, 0);
    cupsdSaveAllClasses();
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_INFO, "Printer \"%s\" deleted by \"%s\".",
                    printer->name, get_username(con));

    cupsdDeletePrinter(printer, 0);
    cupsdSaveAllPrinters();
  }

  cupsdWritePrintcap();

 /*
  * Return with no errors...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_default()' - Get the default destination.
 */

static void
get_default(cupsd_client_t *con)	/* I - Client connection */
{
  http_status_t	status;			/* Policy status */
  cups_array_t	*ra;			/* Requested attributes array */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_default(%p[%d])", con, con->http.fd);

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

  if (DefaultPrinter)
  {
    ra = create_requested_array(con->request);

    copy_printer_attrs(con, DefaultPrinter, ra);

    cupsArrayDelete(ra);

    con->response->request.status.status_code = IPP_OK;
  }
  else
    send_ipp_status(con, IPP_NOT_FOUND, _("No default printer"));
}


/*
 * 'get_devices()' - Get the list of available devices on the local system.
 */

static void
get_devices(cupsd_client_t *con)	/* I - Client connection */
{
  http_status_t		status;		/* Policy status */
  ipp_attribute_t	*limit,		/* Limit attribute */
			*requested;	/* requested-attributes attribute */
  char			command[1024],	/* cups-deviced command */
			options[1024],	/* Options to pass to command */
			requested_str[256];
					/* String for requested attributes */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_devices(%p[%d])", con, con->http.fd);

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Run cups-deviced command with the given options...
  */

  limit = ippFindAttribute(con->request, "limit", IPP_TAG_INTEGER);
  requested = ippFindAttribute(con->request, "requested-attributes",
                               IPP_TAG_KEYWORD);

  if (requested)
    url_encode_attr(requested, requested_str, sizeof(requested_str));
  else
    strlcpy(requested_str, "requested-attributes=all", sizeof(requested_str));

  snprintf(command, sizeof(command), "%s/daemon/cups-deviced", ServerBin);
  snprintf(options, sizeof(options),
           "%d+%d+%d+%s",
           con->request->request.op.request_id,
           limit ? limit->values[0].integer : 0, (int)User,
	   requested_str);

  if (cupsdSendCommand(con, command, options, 1))
  {
   /*
    * Command started successfully, don't send an IPP response here...
    */

    ippDelete(con->response);
    con->response = NULL;
  }
  else
  {
   /*
    * Command failed, return "internal error" so the user knows something
    * went wrong...
    */

    send_ipp_status(con, IPP_INTERNAL_ERROR,
                    _("cups-deviced failed to execute."));
  }
}


/*
 * 'get_job_attrs()' - Get job attributes.
 */

static void
get_job_attrs(cupsd_client_t  *con,	/* I - Client connection */
	      ipp_attribute_t *uri)	/* I - Job URI */
{
  http_status_t	status;			/* Policy status */
  ipp_attribute_t *attr;		/* Current attribute */
  int		jobid;			/* Job ID */
  cupsd_job_t	*job;			/* Current job */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cups_array_t	*ra;			/* Requested attributes array */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_job_attrs(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Copy attributes...
  */

  cupsdLoadJob(job);

  ra = create_requested_array(con->request);
  copy_job_attrs(con, job, ra);
  cupsArrayDelete(ra);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_jobs()' - Get a list of jobs for the specified printer.
 */

static void
get_jobs(cupsd_client_t  *con,		/* I - Client connection */
	 ipp_attribute_t *uri)		/* I - Printer URI */
{
  http_status_t	status;			/* Policy status */
  ipp_attribute_t *attr;		/* Current attribute */
  const char	*dest;			/* Destination */
  cups_ptype_t	dtype;			/* Destination type (printer or class) */
  cups_ptype_t	dmask;			/* Destination type mask */
  char		scheme[HTTP_MAX_URI],	/* Scheme portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  int		completed;		/* Completed jobs? */
  int		first_job_id;		/* First job ID */
  int		limit;			/* Maximum number of jobs to return */
  int		count;			/* Number of jobs that match */
  cupsd_job_t	*job;			/* Current job pointer */
  cupsd_printer_t *printer;		/* Printer */
  cups_array_t	*list;			/* Which job list... */
  cups_array_t	*ra;			/* Requested attributes array */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_jobs(%p[%d], %s)", con, con->http.fd,
                  uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, scheme,
                  sizeof(scheme), username, sizeof(username), host,
		  sizeof(host), &port, resource, sizeof(resource));

  if (!strcmp(resource, "/") ||
      (!strncmp(resource, "/jobs", 5) && strlen(resource) <= 6))
  {
    dest    = NULL;
    dtype   = (cups_ptype_t)0;
    dmask   = (cups_ptype_t)0;
    printer = NULL;
  }
  else if (!strncmp(resource, "/printers", 9) && strlen(resource) <= 10)
  {
    dest    = NULL;
    dtype   = (cups_ptype_t)0;
    dmask   = CUPS_PRINTER_CLASS;
    printer = NULL;
  }
  else if (!strncmp(resource, "/classes", 8) && strlen(resource) <= 9)
  {
    dest    = NULL;
    dtype   = CUPS_PRINTER_CLASS;
    dmask   = CUPS_PRINTER_CLASS;
    printer = NULL;
  }
  else if ((dest = cupsdValidateDest(uri->values[0].string.text, &dtype,
                                     &printer)) == NULL)
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }
  else
    dmask = CUPS_PRINTER_CLASS;

 /*
  * Check policy...
  */

  if (printer)
  {
    if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con,
                                   NULL)) != HTTP_OK)
    {
      send_http_error(con, status);
      return;
    }
  }
  else if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * See if the "which-jobs" attribute have been specified...
  */

  if ((attr = ippFindAttribute(con->request, "which-jobs",
                               IPP_TAG_KEYWORD)) != NULL &&
      !strcmp(attr->values[0].string.text, "completed"))
  {
    completed = 1;
    list      = Jobs;
  }
  else if (attr && !strcmp(attr->values[0].string.text, "all"))
  {
    completed = 0;
    list      = Jobs;
  }
  else
  {
    completed = 0;
    list      = ActiveJobs;
  }

 /*
  * See if they want to limit the number of jobs reported...
  */

  if ((attr = ippFindAttribute(con->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 1000000;

  if ((attr = ippFindAttribute(con->request, "first-job-id",
                               IPP_TAG_INTEGER)) != NULL)
    first_job_id = attr->values[0].integer;
  else
    first_job_id = 1;

 /*
  * See if we only want to see jobs for a specific user...
  */

  if ((attr = ippFindAttribute(con->request, "my-jobs",
                               IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
    strlcpy(username, get_username(con), sizeof(username));
  else
    username[0] = '\0';

  ra = create_requested_array(con->request);

 /*
  * OK, build a list of jobs for this printer...
  */

  for (count = 0, job = (cupsd_job_t *)cupsArrayFirst(list);
       count < limit && job;
       job = (cupsd_job_t *)cupsArrayNext(list))
  {
   /*
    * Filter out jobs that don't match...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_jobs: job->id = %d", job->id);

    if ((dest && strcmp(job->dest, dest)) &&
        (!job->printer || !dest || strcmp(job->printer->name, dest)))
      continue;
    if ((job->dtype & dmask) != dtype &&
        (!job->printer || (job->printer->type & dmask) != dtype))
      continue;
    if (username[0] && strcasecmp(username, job->username))
      continue;

    if (completed && job->state_value <= IPP_JOB_STOPPED)
      continue;

    if (job->id < first_job_id)
      continue;

    cupsdLoadJob(job);

    if (!job->attrs)
      continue;

    if (count > 0)
      ippAddSeparator(con->response);

    count ++;

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_jobs: count = %d", count);

    copy_job_attrs(con, job, ra);
  }

  cupsArrayDelete(ra);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_notifications()' - Get events for a subscription.
 */

static void
get_notifications(cupsd_client_t *con)	/* I - Client connection */
{
  int			i, j;		/* Looping vars */
  http_status_t		status;		/* Policy status */
  cupsd_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*ids,		/* notify-subscription-ids */
			*sequences;	/* notify-sequence-numbers */
  int			min_seq;	/* Minimum sequence number */
  int			interval;	/* Poll interval */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_subscription_attrs(con=%p[%d])",
                  con, con->http.fd);

 /*
  * Get subscription attributes...
  */

  ids       = ippFindAttribute(con->request, "notify-subscription-ids",
                               IPP_TAG_INTEGER);
  sequences = ippFindAttribute(con->request, "notify-sequence-numbers",
                               IPP_TAG_INTEGER);

  if (!ids)
  {
    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("Missing notify-subscription-ids attribute!"));
    return;
  }

 /*
  * Are the subscription IDs valid?
  */

  for (i = 0, interval = 60; i < ids->num_values; i ++)
  {
    if ((sub = cupsdFindSubscription(ids->values[i].integer)) == NULL)
    {
     /*
      * Bad subscription ID...
      */

      send_ipp_status(con, IPP_NOT_FOUND,
                      _("notify-subscription-id %d no good!"),
		      ids->values[i].integer);
      return;
    }

   /*
    * Check policy...
    */

    if ((status = cupsdCheckPolicy(sub->dest ? sub->dest->op_policy_ptr :
                                               DefaultPolicyPtr,
                                   con, sub->owner)) != HTTP_OK)
    {
      send_http_error(con, status);
      return;
    }

   /*
    * Check the subscription type and update the interval accordingly.
    */

    if (sub->job && sub->job->state_value == IPP_JOB_PROCESSING &&
        interval > 10)
      interval = 10;
    else if (sub->job && sub->job->state_value >= IPP_JOB_STOPPED)
      interval = 0;
    else if (sub->dest && sub->dest->state == IPP_PRINTER_PROCESSING &&
             interval > 30)
      interval = 30;
  }

 /*
  * Tell the client to poll again in N seconds...
  */

  if (interval > 0)
    ippAddInteger(con->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                  "notify-get-interval", interval);

  ippAddInteger(con->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                "printer-up-time", time(NULL));

 /*
  * Copy the subscription event attributes to the response.
  */

  con->response->request.status.status_code =
      interval ? IPP_OK : IPP_OK_EVENTS_COMPLETE;

  for (i = 0; i < ids->num_values; i ++)
  {
   /*
    * Get the subscription and sequence number...
    */

    sub = cupsdFindSubscription(ids->values[i].integer);

    if (sequences && i < sequences->num_values)
      min_seq = sequences->values[i].integer;
    else
      min_seq = 1;

   /*
    * If we don't have any new events, nothing to do here...
    */

    if (min_seq > (sub->first_event_id + sub->num_events))
      continue;

   /*
    * Otherwise copy all of the new events...
    */

    if (sub->first_event_id > min_seq)
      j = 0;
    else
      j = min_seq - sub->first_event_id;

    for (; j < sub->num_events; j ++)
    {
      ippAddSeparator(con->response);

      copy_attrs(con->response, sub->events[j]->attrs, NULL,
        	 IPP_TAG_EVENT_NOTIFICATION, 0);
    }
  }
}


/*
 * 'get_ppds()' - Get the list of PPD files on the local system.
 */

static void
get_ppds(cupsd_client_t *con)		/* I - Client connection */
{
  http_status_t		status;		/* Policy status */
  ipp_attribute_t	*limit,		/* Limit attribute */
			*make,		/* ppd-make attribute */
			*requested;	/* requested-attributes attribute */
  char			command[1024],	/* cups-deviced command */
			options[1024],	/* Options to pass to command */
			requested_str[256],
					/* String for requested attributes */
			make_str[256];	/* Escaped ppd-make string */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_ppds(%p[%d])", con, con->http.fd);

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Run cups-driverd command with the given options...
  */

  limit     = ippFindAttribute(con->request, "limit", IPP_TAG_INTEGER);
  make      = ippFindAttribute(con->request, "ppd-make", IPP_TAG_TEXT);
  requested = ippFindAttribute(con->request, "requested-attributes",
                               IPP_TAG_KEYWORD);

  if (requested)
    url_encode_attr(requested, requested_str, sizeof(requested_str));
  else
    strlcpy(requested_str, "requested-attributes=all", sizeof(requested_str));

  if (make)
    url_encode_attr(make, make_str, sizeof(make_str));
  else
    make_str[0] = '\0';

  snprintf(command, sizeof(command), "%s/daemon/cups-driverd", ServerBin);
  snprintf(options, sizeof(options), "list+%d+%d+%s%s%s",
           con->request->request.op.request_id,
           limit ? limit->values[0].integer : 0,
	   requested_str, make ? "%20" : "", make_str);

  if (cupsdSendCommand(con, command, options, 0))
  {
   /*
    * Command started successfully, don't send an IPP response here...
    */

    ippDelete(con->response);
    con->response = NULL;
  }
  else
  {
   /*
    * Command failed, return "internal error" so the user knows something
    * went wrong...
    */

    send_ipp_status(con, IPP_INTERNAL_ERROR,
                    _("cups-driverd failed to execute."));
  }
}


/*
 * 'get_printer_attrs()' - Get printer attributes.
 */

static void
get_printer_attrs(cupsd_client_t  *con,	/* I - Client connection */
		  ipp_attribute_t *uri)	/* I - Printer URI */
{
  http_status_t		status;		/* Policy status */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  cupsd_printer_t	*printer;	/* Printer/class */
  cups_array_t		*ra;		/* Requested attributes array */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_printer_attrs(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Send the attributes...
  */

  ra = create_requested_array(con->request);

  copy_printer_attrs(con, printer, ra);

  cupsArrayDelete(ra);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_printers()' - Get a list of printers or classes.
 */

static void
get_printers(cupsd_client_t *con,	/* I - Client connection */
             int            type)	/* I - 0 or CUPS_PRINTER_CLASS */
{
  http_status_t	status;			/* Policy status */
  ipp_attribute_t *attr;		/* Current attribute */
  int		limit;			/* Maximum number of printers to return */
  int		count;			/* Number of printers that match */
  cupsd_printer_t *printer;		/* Current printer pointer */
  int		printer_type,		/* printer-type attribute */
		printer_mask;		/* printer-type-mask attribute */
  char		*location;		/* Location string */
  const char	*username;		/* Current user */
  char		*first_printer_name;	/* first-printer-name attribute */
  cups_array_t	*ra;			/* Requested attributes array */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_printers(%p[%d], %x)", con,
                  con->http.fd, type);

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Check for printers...
  */

  if (!Printers || !cupsArrayCount(Printers))
  {
    send_ipp_status(con, IPP_NOT_FOUND, _("No destinations added."));
    return;
  }

 /*
  * See if they want to limit the number of printers reported...
  */

  if ((attr = ippFindAttribute(con->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 10000000;

  if ((attr = ippFindAttribute(con->request, "first-printer-name",
                               IPP_TAG_NAME)) != NULL)
    first_printer_name = attr->values[0].string.text;
  else
    first_printer_name = NULL;

 /*
  * Support filtering...
  */

  if ((attr = ippFindAttribute(con->request, "printer-type",
                               IPP_TAG_ENUM)) != NULL)
    printer_type = attr->values[0].integer;
  else
    printer_type = 0;

  if ((attr = ippFindAttribute(con->request, "printer-type-mask",
                               IPP_TAG_ENUM)) != NULL)
    printer_mask = attr->values[0].integer;
  else
    printer_mask = 0;

  if ((attr = ippFindAttribute(con->request, "printer-location",
                               IPP_TAG_TEXT)) != NULL)
    location = attr->values[0].string.text;
  else
    location = NULL;

  if (con->username[0])
    username = con->username;
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name",
                                    IPP_TAG_NAME)) != NULL)
    username = attr->values[0].string.text;
  else
    username = NULL;

  ra = create_requested_array(con->request);

 /*
  * OK, build a list of printers for this printer...
  */

  if (first_printer_name)
  {
    if ((printer = cupsdFindDest(first_printer_name)) == NULL)
      printer = (cupsd_printer_t *)cupsArrayFirst(Printers);
  }
  else
    printer = (cupsd_printer_t *)cupsArrayFirst(Printers);

  for (count = 0;
       count < limit && printer;
       printer = (cupsd_printer_t *)cupsArrayNext(Printers))
  {
    if ((!type || (printer->type & CUPS_PRINTER_CLASS) == type) &&
        (printer->type & printer_mask) == printer_type &&
	(!location || !printer->location ||
	 !strcasecmp(printer->location, location)))
    {
     /*
      * If HideImplicitMembers is enabled, see if this printer or class
      * is a member of an implicit class...
      */

      if (ImplicitClasses && HideImplicitMembers &&
          printer->in_implicit_class)
        continue;

     /*
      * If a username is specified, see if it is allowed or denied
      * access...
      */

      if (printer->num_users && username && !user_allowed(printer, username))
        continue;

     /*
      * Add the group separator as needed...
      */

      if (count > 0)
        ippAddSeparator(con->response);

      count ++;

     /*
      * Send the attributes...
      */

      copy_printer_attrs(con, printer, ra);
    }
  }

  cupsArrayDelete(ra);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_subscription_attrs()' - Get subscription attributes.
 */

static void
get_subscription_attrs(
    cupsd_client_t *con,		/* I - Client connection */
    int            sub_id)		/* I - Subscription ID */
{
  http_status_t		status;		/* Policy status */
  cupsd_subscription_t	*sub;		/* Subscription */
  cups_array_t		*ra;		/* Requested attributes array */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "get_subscription_attrs(con=%p[%d], sub_id=%d)",
                  con, con->http.fd, sub_id);

 /*
  * Is the subscription ID valid?
  */

  if ((sub = cupsdFindSubscription(sub_id)) == NULL)
  {
   /*
    * Bad subscription ID...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("notify-subscription-id %d no good!"), sub_id);
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(sub->dest ? sub->dest->op_policy_ptr :
                                             DefaultPolicyPtr,
                                 con, sub->owner)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Copy the subscription attributes to the response using the
  * requested-attributes attribute that may be provided by the client.
  */

  ra = create_requested_array(con->request);

  copy_subscription_attrs(con, sub, ra);

  cupsArrayDelete(ra);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'get_subscriptions()' - Get subscriptions.
 */

static void
get_subscriptions(cupsd_client_t  *con,	/* I - Client connection */
                  ipp_attribute_t *uri)	/* I - Printer/job URI */
{
  http_status_t		status;		/* Policy status */
  int			count;		/* Number of subscriptions */
  int			limit;		/* Limit */
  cupsd_subscription_t	*sub;		/* Subscription */
  cups_array_t		*ra;		/* Requested attributes array */
  ipp_attribute_t	*attr;		/* Attribute */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			scheme[HTTP_MAX_URI],
					/* Scheme portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  cupsd_job_t		*job;		/* Job pointer */
  cupsd_printer_t	*printer;	/* Printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "get_subscriptions(con=%p[%d], uri=%s)",
                  con, con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, scheme,
                  sizeof(scheme), username, sizeof(username), host,
		  sizeof(host), &port, resource, sizeof(resource));

  if (!strcmp(resource, "/") ||
      (!strncmp(resource, "/jobs", 5) && strlen(resource) <= 6) ||
      (!strncmp(resource, "/printers", 9) && strlen(resource) <= 10) ||
      (!strncmp(resource, "/classes", 8) && strlen(resource) <= 9))
  {
    printer = NULL;
    job     = NULL;
  }
  else if (!strncmp(resource, "/jobs/", 6) && resource[6])
  {
    printer = NULL;
    job     = cupsdFindJob(atoi(resource + 6));

    if (!job)
    {
      send_ipp_status(con, IPP_NOT_FOUND, _("Job #%s does not exist!"),
                      resource + 6);
      return;
    }
  }
  else if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }
  else if ((attr = ippFindAttribute(con->request, "notify-job-id",
                                    IPP_TAG_INTEGER)) != NULL)
  {
    job = cupsdFindJob(attr->values[0].integer);

    if (!job)
    {
      send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"),
                      attr->values[0].integer);
      return;
    }
  }
  else
    job = NULL;

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer ? printer->op_policy_ptr :
                                           DefaultPolicyPtr,
                                 con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Copy the subscription attributes to the response using the
  * requested-attributes attribute that may be provided by the client.
  */

  ra = create_requested_array(con->request);

  if ((attr = ippFindAttribute(con->request, "limit",
                               IPP_TAG_INTEGER)) != NULL)
    limit = attr->values[0].integer;
  else
    limit = 0;

 /*
  * See if we only want to see subscriptions for a specific user...
  */

  if ((attr = ippFindAttribute(con->request, "my-subscriptions",
                               IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
    strlcpy(username, get_username(con), sizeof(username));
  else
    username[0] = '\0';

  for (sub = (cupsd_subscription_t *)cupsArrayFirst(Subscriptions), count = 0;
       sub;
       sub = (cupsd_subscription_t *)cupsArrayNext(Subscriptions))
    if ((!printer || sub->dest == printer) && (!job || sub->job == job) &&
        (!username[0] || !strcasecmp(username, sub->owner)))
    {
      ippAddSeparator(con->response);
      copy_subscription_attrs(con, sub, ra);

      count ++;
      if (limit && count >= limit)
        break;
    }

  cupsArrayDelete(ra);

  if (count)
    con->response->request.status.status_code = IPP_OK;
  else
    send_ipp_status(con, IPP_NOT_FOUND, _("No subscriptions found."));
}


/*
 * 'get_username()' - Get the username associated with a request.
 */

static const char *			/* O - Username */
get_username(cupsd_client_t *con)	/* I - Connection */
{
  ipp_attribute_t	*attr;		/* Attribute */


  if (con->username[0])
    return (con->username);
  else if ((attr = ippFindAttribute(con->request, "requesting-user-name",
                                    IPP_TAG_NAME)) != NULL)
    return (attr->values[0].string.text);
  else
    return ("anonymous");
}


/*
 * 'hold_job()' - Hold a print job.
 */

static void
hold_job(cupsd_client_t  *con,		/* I - Client connection */
         ipp_attribute_t *uri)		/* I - Job or Printer URI */
{
  ipp_attribute_t *attr,		/* Current job-hold-until */
		*newattr;		/* New job-hold-until */
  int		jobid;			/* Job ID */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_job_t	*job;			/* Job information */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "hold_job(%p[%d], %s)", con, con->http.fd,
                  uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * Hold the job and return...
  */

  cupsdHoldJob(job);

  cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job,
                "Job held by user.");

  if ((newattr = ippFindAttribute(con->request, "job-hold-until",
                                  IPP_TAG_KEYWORD)) == NULL)
    newattr = ippFindAttribute(con->request, "job-hold-until", IPP_TAG_NAME);

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                               IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

  if (attr)
  {
   /*
    * Free the old hold value and copy the new one over...
    */

    _cupsStrFree(attr->values[0].string.text);

    if (newattr)
    {
      attr->value_tag = newattr->value_tag;
      attr->values[0].string.text =
          _cupsStrAlloc(newattr->values[0].string.text);
    }
    else
    {
      attr->value_tag = IPP_TAG_KEYWORD;
      attr->values[0].string.text = _cupsStrAlloc("indefinite");
    }

   /*
    * Hold job until specified time...
    */

    cupsdSetJobHoldUntil(job, attr->values[0].string.text);

    cupsdAddEvent(CUPSD_EVENT_JOB_CONFIG_CHANGED, job->printer, job,
                  "Job job-hold-until value changed by user.");
  }

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d was held by \"%s\".", jobid,
                  username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'move_job()' - Move a job to a new destination.
 */

static void
move_job(cupsd_client_t  *con,		/* I - Client connection */
	 ipp_attribute_t *uri)		/* I - Job URI */
{
  http_status_t	status;			/* Policy status */
  ipp_attribute_t *attr;		/* Current attribute */
  int		jobid;			/* Job ID */
  cupsd_job_t	*job;			/* Current job */
  const char	*src;			/* Source printer/class */
  cups_ptype_t	stype,			/* Source type (printer or class) */
		dtype;			/* Destination type (printer or class) */
  char		scheme[HTTP_MAX_URI],	/* Scheme portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_printer_t *sprinter,		/* Source printer */
		*dprinter;		/* Destination printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "move_job(%p[%d], %s)", con, con->http.fd,
                  uri->values[0].string.text);

 /*
  * Get the new printer or class...
  */

  if ((attr = ippFindAttribute(con->request, "job-printer-uri",
                               IPP_TAG_URI)) == NULL)
  {
   /*
    * Need job-printer-uri...
    */

    send_ipp_status(con, IPP_BAD_REQUEST,
                    _("job-printer-uri attribute missing!"));
    return;
  }

  if (!cupsdValidateDest(attr->values[0].string.text, &dtype, &dprinter))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(dprinter->op_policy_ptr, con,
                                 NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * See if we have a job URI or a printer URI...
  */

  httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, scheme,
                  sizeof(scheme), username, sizeof(username), host,
		  sizeof(host), &port, resource, sizeof(resource));

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
     /*
      * Move all jobs...
      */

      if ((src = cupsdValidateDest(uri->values[0].string.text, &stype,
                                   &sprinter)) == NULL)
      {
       /*
	* Bad URI...
	*/

	send_ipp_status(con, IPP_NOT_FOUND,
                	_("The printer or class was not found."));
	return;
      }

      job = NULL;
    }
    else
    {
     /*
      * Otherwise, just move a single job...
      */

      if ((job = cupsdFindJob(attr->values[0].integer)) == NULL)
      {
       /*
	* Nope - return a "not found" error...
	*/

	send_ipp_status(con, IPP_NOT_FOUND,
                	_("Job #%d does not exist!"), attr->values[0].integer);
	return;
      }
      else
      {
       /*
        * Job found, initialize source pointers...
	*/

	src      = NULL;
	sprinter = NULL;
      }
    }
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

   /*
    * See if the job exists...
    */

    jobid = atoi(resource + 6);

    if ((job = cupsdFindJob(jobid)) == NULL)
    {
     /*
      * Nope - return a "not found" error...
      */

      send_ipp_status(con, IPP_NOT_FOUND,
                      _("Job #%d does not exist!"), jobid);
      return;
    }
    else
    {
     /*
      * Job found, initialize source pointers...
      */

      src      = NULL;
      sprinter = NULL;
    }
  }

 /*
  * Now move the job or jobs...
  */

  if (job)
  {
   /*
    * See if the job has been completed...
    */

    if (job->state_value > IPP_JOB_STOPPED)
    {
     /*
      * Return a "not-possible" error...
      */

      send_ipp_status(con, IPP_NOT_POSSIBLE,
                      _("Job #%d is finished and cannot be altered!"),
		      job->id);
      return;
    }

   /*
    * See if the job is owned by the requesting user...
    */

    if (!validate_user(job, con, job->username, username, sizeof(username)))
    {
      send_http_error(con, HTTP_UNAUTHORIZED);
      return;
    }

   /*
    * Move the job to a different printer or class...
    */

    cupsdMoveJob(job, dprinter);
  }
  else
  {
   /*
    * Got the source printer, now look through the jobs...
    */

    for (job = (cupsd_job_t *)cupsArrayFirst(Jobs);
         job;
	 job = (cupsd_job_t *)cupsArrayNext(Jobs))
    {
     /*
      * See if the job is pointing at the source printer or has not been
      * completed...
      */

      if (strcasecmp(job->dest, src) ||
          job->state_value > IPP_JOB_STOPPED)
	continue;

     /*
      * See if the job can be moved by the requesting user...
      */

      if (!validate_user(job, con, job->username, username, sizeof(username)))
        continue;

     /*
      * Move the job to a different printer or class...
      */

      cupsdMoveJob(job, dprinter);
    }
  }

 /*
  * Start jobs if possible...
  */

  cupsdCheckJobs();

 /*
  * Return with "everything is OK" status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'ppd_add_default()' - Add a PPD default choice.
 */

static int				/* O  - Number of defaults */
ppd_add_default(
    const char    *option,		/* I  - Option name */
    const char    *choice,		/* I  - Choice name */
    int           num_defaults,		/* I  - Number of defaults */
    ppd_default_t **defaults)		/* IO - Defaults */
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
    cupsdLogMessage(CUPSD_LOG_ERROR, "ppd_add_default: Unable to add default value for \"%s\" - %s",
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

  for (line += 8, olen --; isalnum(*line & 255); line ++)
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

  while (isspace(*line & 255))
    line ++;

  for (clen --; isalnum(*line & 255); line ++)
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
print_job(cupsd_client_t  *con,		/* I - Client connection */
	  ipp_attribute_t *uri)		/* I - Printer URI */
{
  ipp_attribute_t *attr;		/* Current attribute */
  ipp_attribute_t *format;		/* Document-format attribute */
  const char	*default_format;	/* document-format-default value */
  cupsd_job_t	*job;			/* New job */
  char		filename[1024];		/* Job filename */
  mime_type_t	*filetype;		/* Type of file */
  char		super[MIME_MAX_SUPER],	/* Supertype of file */
		type[MIME_MAX_TYPE],	/* Subtype of file */
		mimetype[MIME_MAX_SUPER + MIME_MAX_TYPE + 2];
					/* Textual name of mime type */
  cupsd_printer_t *printer;		/* Printer data */
  struct stat	fileinfo;		/* File information */
  int		kbytes;			/* Size of file */
  int		compression;		/* Document compression */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "print_job(%p[%d], %s)", con, con->http.fd,
                  uri->values[0].string.text);

 /*
  * Validate print file attributes, for now just document-format and
  * compression (CUPS only supports "none" and "gzip")...
  */

  compression = CUPS_FILE_NONE;

  if ((attr = ippFindAttribute(con->request, "compression",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    if (strcmp(attr->values[0].string.text, "none")
#ifdef HAVE_LIBZ
        && strcmp(attr->values[0].string.text, "gzip")
#endif /* HAVE_LIBZ */
      )
    {
      send_ipp_status(con, IPP_ATTRIBUTES,
                      _("Unsupported compression \"%s\"!"),
        	      attr->values[0].string.text);
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
    send_ipp_status(con, IPP_BAD_REQUEST, _("No file!?!"));
    return;
  }

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, NULL, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format",
                                 IPP_TAG_MIMETYPE)) != NULL)
  {
   /*
    * Grab format from client...
    */

    if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super,
               type) != 2)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Could not scan type \"%s\"!"),
		      format->values[0].string.text);
      return;
    }
  }
  else if ((default_format = cupsGetOption("document-format",
                                           printer->num_options,
					   printer->options)) != NULL)
  {
   /*
    * Use default document format...
    */

    if (sscanf(default_format, "%15[^/]/%31[^;]", super, type) != 2)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Could not scan type \"%s\"!"),
		      default_format);
      return;
    }
  }
  else
  {
   /*
    * Auto-type it!
    */

    strcpy(super, "application");
    strcpy(type, "octet-stream");
  }

  if (!strcmp(super, "application") && !strcmp(type, "octet-stream"))
  {
   /*
    * Auto-type the file...
    */

    ipp_attribute_t	*doc_name;	/* document-name attribute */


    cupsdLogMessage(CUPSD_LOG_DEBUG, "print_job: auto-typing file...");

    doc_name = ippFindAttribute(con->request, "document-name", IPP_TAG_NAME);
    filetype = mimeFileType(MimeDatabase, con->filename,
                            doc_name ? doc_name->values[0].string.text : NULL,
			    &compression);

    if (!filetype)
      filetype = mimeType(MimeDatabase, super, type);
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype &&
      (!format ||
       (!strcmp(super, "application") && !strcmp(type, "octet-stream"))))
  {
   /*
    * Replace the document-format attribute value with the auto-typed or
    * default one.
    */

    snprintf(mimetype, sizeof(mimetype), "%s/%s", filetype->super,
             filetype->type);

    if (format)
    {
      _cupsStrFree(format->values[0].string.text);

      format->values[0].string.text = _cupsStrAlloc(mimetype);
    }
    else
      ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
	           "document-format", NULL, mimetype);
  }
  else if (!filetype)
  {
    send_ipp_status(con, IPP_DOCUMENT_FORMAT,
                    _("Unsupported format \'%s/%s\'!"), super, type);
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Hint: Do you have the raw file printing rules enabled?");

    if (format)
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);

    return;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "print_job: request file type is %s/%s.",
	          filetype->super, filetype->type);

 /*
  * Read any embedded job ticket info from PS files...
  */

  if (!strcasecmp(filetype->super, "application") &&
      !strcasecmp(filetype->type, "postscript"))
    read_ps_job_ticket(con);

 /*
  * Create the job object...
  */

  if ((job = add_job(con, printer, filetype)) == NULL)
    return;

 /*
  * Update quota data...
  */

  if (stat(con->filename, &fileinfo))
    kbytes = 0;
  else
    kbytes = (fileinfo.st_size + 1023) / 1024;

  cupsdUpdateQuota(printer, job->username, 0, kbytes);

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets",
                               IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer += kbytes;

 /*
  * Add the job file...
  */

  if (add_file(con, job, filetype, compression))
    return;

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  rename(con->filename, filename);
  cupsdClearString(&con->filename);

 /*
  * See if we need to add the ending sheet...
  */

  attr = ippFindAttribute(job->attrs, "job-sheets", IPP_TAG_NAME);

  if (!(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) &&
      attr && attr->num_values > 1)
  {
   /*
    * Yes...
    */

    cupsdLogMessage(CUPSD_LOG_INFO, "Adding end banner page \"%s\" to job %d.",
                    attr->values[1].string.text, job->id);

    kbytes = copy_banner(con, job, attr->values[1].string.text);

    cupsdUpdateQuota(printer, job->username, 0, kbytes);
  }

 /*
  * Log and save the job...
  */

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d queued on \"%s\" by \"%s\".", job->id,
                  job->dest, job->username);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Job %d hold_until = %d", job->id,
                  (int)job->hold_until);

  cupsdSaveJob(job);

 /*
  * Start the job if possible...
  */

  cupsdCheckJobs();
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
read_ps_job_ticket(cupsd_client_t *con)	/* I - Client connection */
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
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "read_ps_job_ticket: Unable to open PostScript print file "
		    "- %s",
                    strerror(errno));
    return;
  }

 /*
  * Skip the first line...
  */

  if (cupsFileGets(fp, line, sizeof(line)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "read_ps_job_ticket: Unable to read from PostScript print "
		    "file - %s",
                    strerror(errno));
    cupsFileClose(fp);
    return;
  }

  if (strncmp(line, "%!PS-Adobe-", 11))
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

  while (cupsFileGets(fp, line, sizeof(line)))
  {
   /*
    * Stop at the first non-ticket line...
    */

    if (strncmp(line, "%cupsJobTicket:", 15))
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

  for (attr = ticket->attrs; attr; attr = attr->next)
  {
    if (attr->group_tag != IPP_TAG_JOB || !attr->name)
      continue;

    if (!strcmp(attr->name, "job-originating-host-name") ||
        !strcmp(attr->name, "job-originating-user-name") ||
	!strcmp(attr->name, "job-media-sheets-completed") ||
	!strcmp(attr->name, "job-k-octets") ||
	!strcmp(attr->name, "job-id") ||
	!strncmp(attr->name, "job-state", 9) ||
	!strncmp(attr->name, "time-at-", 8))
      continue; /* Read-only attrs */

    if ((attr2 = ippFindAttribute(con->request, attr->name,
                                  IPP_TAG_ZERO)) != NULL)
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
	for (prev2 = con->request->attrs; prev2; prev2 = prev2->next)
	  if (prev2->next == attr2)
	  {
	    prev2->next = attr2->next;
	    break;
	  }
      }

      if (con->request->last == attr2)
        con->request->last = prev2;

      _ippFreeAttr(attr2);
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
reject_jobs(cupsd_client_t  *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Printer or class URI */
{
  http_status_t	status;			/* Policy status */
  cups_ptype_t	dtype;			/* Destination type (printer or class) */
  cupsd_printer_t *printer;		/* Printer data */
  ipp_attribute_t *attr;		/* printer-state-message text */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "reject_jobs(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Reject jobs sent to the printer...
  */

  printer->accepting = 0;

  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) == NULL)
    strcpy(printer->state_message, "Rejecting Jobs");
  else
    strlcpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message));

  cupsdAddPrinterHistory(printer);

  if (dtype & CUPS_PRINTER_CLASS)
  {
    cupsdSaveAllClasses();

    cupsdLogMessage(CUPSD_LOG_INFO, "Class \"%s\" rejecting jobs (\"%s\").",
                    printer->name, get_username(con));
  }
  else
  {
    cupsdSaveAllPrinters();

    cupsdLogMessage(CUPSD_LOG_INFO, "Printer \"%s\" rejecting jobs (\"%s\").",
                    printer->name, get_username(con));
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
release_job(cupsd_client_t  *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t *attr;		/* Current attribute */
  int		jobid;			/* Job ID */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_job_t	*job;			/* Job information */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "release_job(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * See if job is "held"...
  */

  if (job->state_value != IPP_JOB_HELD)
  {
   /*
    * Nope - return a "not possible" error...
    */

    send_ipp_status(con, IPP_NOT_POSSIBLE, _("Job #%d is not held!"), jobid);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * Reset the job-hold-until value to "no-hold"...
  */

  if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                               IPP_TAG_KEYWORD)) == NULL)
    attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

  if (attr)
  {
    _cupsStrFree(attr->values[0].string.text);

    attr->value_tag = IPP_TAG_KEYWORD;
    attr->values[0].string.text = _cupsStrAlloc("no-hold");

    cupsdAddEvent(CUPSD_EVENT_JOB_CONFIG_CHANGED, job->printer, job,
                  "Job job-hold-until value changed by user.");
  }

 /*
  * Release the job and return...
  */

  cupsdReleaseJob(job);

  cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job,
                "Job released by user.");

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d was released by \"%s\".", jobid,
                  username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'renew_subscription()' - Renew an existing subscription...
 */

static void
renew_subscription(
    cupsd_client_t *con,		/* I - Client connection */
    int            sub_id)		/* I - Subscription ID */
{
  http_status_t		status;		/* Policy status */
  cupsd_subscription_t	*sub;		/* Subscription */
  ipp_attribute_t	*lease;		/* notify-lease-duration */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "renew_subscription(con=%p[%d], sub_id=%d)",
                  con, con->http.fd, sub_id);

 /*
  * Is the subscription ID valid?
  */

  if ((sub = cupsdFindSubscription(sub_id)) == NULL)
  {
   /*
    * Bad subscription ID...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("notify-subscription-id %d no good!"), sub_id);
    return;
  }

  if (sub->job)
  {
   /*
    * Job subscriptions cannot be renewed...
    */

    send_ipp_status(con, IPP_NOT_POSSIBLE,
                    _("Job subscriptions cannot be renewed!"));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(sub->dest ? sub->dest->op_policy_ptr :
                                             DefaultPolicyPtr,
                                 con, sub->owner)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Renew the subscription...
  */

  lease = ippFindAttribute(con->request, "notify-lease-duration",
                           IPP_TAG_INTEGER);

  sub->lease = lease ? lease->values[0].integer : DefaultLeaseDuration;

  if (MaxLeaseDuration && (sub->lease == 0 || sub->lease > MaxLeaseDuration))
  {
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "renew_subscription: Limiting notify-lease-duration to "
		    "%d seconds.",
		    MaxLeaseDuration);
    sub->lease = MaxLeaseDuration;
  }

  sub->expire = sub->lease ? time(NULL) + sub->lease : 0;

  cupsdSaveAllSubscriptions();

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'restart_job()' - Restart an old print job.
 */

static void
restart_job(cupsd_client_t  *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Job or Printer URI */
{
  ipp_attribute_t *attr;		/* Current attribute */
  int		jobid;			/* Job ID */
  char		method[HTTP_MAX_URI],	/* Method portion of URI */
		username[HTTP_MAX_URI],	/* Username portion of URI */
		host[HTTP_MAX_URI],	/* Host portion of URI */
		resource[HTTP_MAX_URI];	/* Resource portion of URI */
  int		port;			/* Port portion of URI */
  cupsd_job_t	*job;			/* Job information */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "restart_job(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * See if job is in any of the "completed" states...
  */

  if (job->state_value <= IPP_JOB_PROCESSING)
  {
   /*
    * Nope - return a "not possible" error...
    */

    send_ipp_status(con, IPP_NOT_POSSIBLE, _("Job #%d is not complete!"),
                    jobid);
    return;
  }

 /*
  * See if we have retained the job files...
  */

  cupsdLoadJob(job);

  if (!job->attrs || job->num_files == 0)
  {
   /*
    * Nope - return a "not possible" error...
    */

    send_ipp_status(con, IPP_NOT_POSSIBLE,
                    _("Job #%d cannot be restarted - no files!"), jobid);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * Restart the job and return...
  */

  cupsdRestartJob(job);

  cupsdLogMessage(CUPSD_LOG_INFO, "Job %d was restarted by \"%s\".", jobid,
                  username);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'save_auth_info()' - Save authentication information for a job.
 */

static void
save_auth_info(cupsd_client_t *con,	/* I - Client connection */
               cupsd_job_t    *job)	/* I - Job */
{
  int		i;			/* Looping var */
  char		filename[1024];		/* Job authentication filename */
  cups_file_t	*fp;			/* Job authentication file */
  char		line[2048];		/* Line for file */


 /*
  * This function saves the in-memory authentication information for
  * a job so that it can be used to authenticate with a remote host.
  * The information is stored in a file that is readable only by the
  * root user.  The username and password are Base-64 encoded, each
  * on a separate line, followed by random number (up to 1024) of
  * newlines to limit the amount of information that is exposed.
  *
  * Because of the potential for exposing of authentication information,
  * this functionality is only enabled when running cupsd as root.
  *
  * This caching only works for the Basic and BasicDigest authentication
  * types.  Digest authentication cannot be cached this way, and in
  * the future Kerberos authentication may make all of this obsolete.
  *
  * Authentication information is saved whenever an authenticated
  * Print-Job, Create-Job, or CUPS-Authenticate-Job operation is
  * performed.
  *
  * This information is deleted after a job is completed or canceled,
  * so reprints may require subsequent re-authentication.
  */

  if (RunUser)
    return;

 /*
  * Create the authentication file and change permissions...
  */

  snprintf(filename, sizeof(filename), "%s/a%05d", RequestRoot, job->id);
  if ((fp = cupsFileOpen(filename, "w")) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to save authentication info to \"%s\" - %s",
                    filename, strerror(errno));
    return;
  }

  fchown(cupsFileNumber(fp), 0, 0);
  fchmod(cupsFileNumber(fp), 0400);

 /*
  * Write the authenticated username...
  */

  httpEncode64_2(line, sizeof(line), con->username, strlen(con->username));
  cupsFilePrintf(fp, "%s\n", line);

 /*
  * Write the authenticated password...
  */

  httpEncode64_2(line, sizeof(line), con->password, strlen(con->password));
  cupsFilePrintf(fp, "%s\n", line);

 /*
  * Write a random number of newlines to the end of the file...
  */

  for (i = (rand() % 1024); i >= 0; i --)
    cupsFilePutChar(fp, '\n');

 /*
  * Close the file and return...
  */

  cupsFileClose(fp);

#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5_H)
  save_krb5_creds(con, job);
#endif /* HAVE_GSSAPI && HAVE_KRB5_H */
}


#if defined(HAVE_GSSAPI) && defined(HAVE_KRB5_H)
/*
 * 'save_krb5_creds()' - Save Kerberos credentials for the job.
 */

static void
save_krb5_creds(cupsd_client_t *con,	/* I - Client connection */
                cupsd_job_t    *job)	/* I - Job */
{
  krb5_context	krb_context;		/* Kerberos context */
  krb5_ccache	ccache;			/* Credentials cache */
  OM_uint32	major_status,		/* Major status code */
		minor_status;		/* Minor status code */


 /*
  * Setup a cached context for the job filters to use...
  */

  if (krb5_init_context(&krb_context))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to initialize Kerberos context");
    return;
  }

#  ifdef HAVE_HEIMDAL
  if (krb5_cc_gen_new(krb_context, &krb5_fcc_ops, &ccache))
#  else
  if (krb5_cc_gen_new(krb_context, &ccache))
#  endif /* HAVE_HEIMDAL */
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create new credentials");
    return;
  }

  major_status = gss_krb5_copy_ccache(&minor_status, con->gss_delegated_cred,
				      ccache);

  if (GSS_ERROR(major_status))
  {
    cupsdLogGSSMessage(CUPSD_LOG_ERROR, major_status, minor_status,
                       "Unable to import client credentials cache");
    krb5_cc_destroy(krb_context, ccache);
    return;
  }

  cupsdSetStringf(&(job->ccname), "KRB5CCNAME=FILE:%s",
                  krb5_cc_get_name(krb_context, ccache));
  krb5_cc_close(krb_context, ccache);
}
#endif /* HAVE_GSSAPI && HAVE_KRB5_H */


/*
 * 'send_document()' - Send a file to a printer or class.
 */

static void
send_document(cupsd_client_t  *con,	/* I - Client connection */
	      ipp_attribute_t *uri)	/* I - Printer URI */
{
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  const char		*default_format;/* document-format-default value */
  int			jobid;		/* Job ID number */
  cupsd_job_t		*job;		/* Current job */
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
  cupsd_printer_t	*printer;	/* Current printer */
  struct stat		fileinfo;	/* File information */
  int			kbytes;		/* Size of file */
  int			compression;	/* Type of compression */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "send_document(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

  printer = cupsdFindDest(job->dest);

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * only supports "none" and "gzip".
  */

  compression = CUPS_FILE_NONE;

  if ((attr = ippFindAttribute(con->request, "compression",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    if (strcmp(attr->values[0].string.text, "none")
#ifdef HAVE_LIBZ
        && strcmp(attr->values[0].string.text, "gzip")
#endif /* HAVE_LIBZ */
      )
    {
      send_ipp_status(con, IPP_ATTRIBUTES, _("Unsupported compression \"%s\"!"),
        	      attr->values[0].string.text);
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
    send_ipp_status(con, IPP_BAD_REQUEST, _("No file!?!"));
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format",
                                 IPP_TAG_MIMETYPE)) != NULL)
  {
   /*
    * Grab format from client...
    */

    if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
    {
      send_ipp_status(con, IPP_BAD_REQUEST, _("Bad document-format \"%s\"!"),
	              format->values[0].string.text);
      return;
    }
  }
  else if ((default_format = cupsGetOption("document-format",
                                           printer->num_options,
					   printer->options)) != NULL)
  {
   /*
    * Use default document format...
    */

    if (sscanf(default_format, "%15[^/]/%31[^;]", super, type) != 2)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Could not scan type \"%s\"!"),
		      default_format);
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

  if (!strcmp(super, "application") && !strcmp(type, "octet-stream"))
  {
   /*
    * Auto-type the file...
    */

    ipp_attribute_t	*doc_name;	/* document-name attribute */


    cupsdLogMessage(CUPSD_LOG_DEBUG, "send_document: auto-typing file...");

    doc_name = ippFindAttribute(con->request, "document-name", IPP_TAG_NAME);
    filetype = mimeFileType(MimeDatabase, con->filename,
                            doc_name ? doc_name->values[0].string.text : NULL,
			    &compression);

    if (!filetype)
      filetype = mimeType(MimeDatabase, super, type);
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype &&
      (!format ||
       (!strcmp(super, "application") && !strcmp(type, "octet-stream"))))
  {
   /*
    * Replace the document-format attribute value with the auto-typed or
    * default one.
    */

    snprintf(mimetype, sizeof(mimetype), "%s/%s", filetype->super,
             filetype->type);

    if (format)
    {
      _cupsStrFree(format->values[0].string.text);

      format->values[0].string.text = _cupsStrAlloc(mimetype);
    }
    else
      ippAddString(con->request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
	           "document-format", NULL, mimetype);
  }
  else if (!filetype)
  {
    send_ipp_status(con, IPP_DOCUMENT_FORMAT,
                    _("Unsupported format \'%s/%s\'!"), super, type);
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Hint: Do you have the raw file printing rules enabled?");

    if (format)
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);

    return;
  }

  if (printer->filetypes && !cupsArrayFind(printer->filetypes, filetype))
  {
    snprintf(mimetype, sizeof(mimetype), "%s/%s", filetype->super,
             filetype->type);

    send_ipp_status(con, IPP_DOCUMENT_FORMAT,
                    _("Unsupported format \'%s\'!"), mimetype);

    ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                 "document-format", NULL, mimetype);

    return;
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "send_document: request file type is %s/%s.",
	          filetype->super, filetype->type);

 /*
  * Add the file to the job...
  */

  cupsdLoadJob(job);

  if (add_file(con, job, filetype, compression))
    return;

  if (stat(con->filename, &fileinfo))
    kbytes = 0;
  else
    kbytes = (fileinfo.st_size + 1023) / 1024;

  cupsdUpdateQuota(printer, job->username, 0, kbytes);

  if ((attr = ippFindAttribute(job->attrs, "job-k-octets",
                               IPP_TAG_INTEGER)) != NULL)
    attr->values[0].integer += kbytes;

  snprintf(filename, sizeof(filename), "%s/d%05d-%03d", RequestRoot, job->id,
           job->num_files);
  rename(con->filename, filename);

  cupsdClearString(&con->filename);

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "File of type %s/%s queued in job #%d by \"%s\".",
                  filetype->super, filetype->type, job->id, job->username);

 /*
  * Start the job if this is the last document...
  */

  if ((attr = ippFindAttribute(con->request, "last-document",
                               IPP_TAG_BOOLEAN)) != NULL &&
      attr->values[0].boolean)
  {
   /*
    * See if we need to add the ending sheet...
    */

    if (printer &&
        !(printer->type & (CUPS_PRINTER_REMOTE | CUPS_PRINTER_IMPLICIT)) &&
        (attr = ippFindAttribute(job->attrs, "job-sheets",
	                         IPP_TAG_ZERO)) != NULL &&
        attr->num_values > 1)
    {
     /*
      * Yes...
      */

      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Adding end banner page \"%s\" to job %d.",
        	      attr->values[1].string.text, job->id);

      kbytes = copy_banner(con, job, attr->values[1].string.text);

      cupsdUpdateQuota(printer, job->username, 0, kbytes);
    }

    if (job->state_value == IPP_JOB_STOPPED)
    {
      job->state->values[0].integer = IPP_JOB_PENDING;
      job->state_value              = IPP_JOB_PENDING;
    }
    else if (job->state_value == IPP_JOB_HELD)
    {
      if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                                   IPP_TAG_KEYWORD)) == NULL)
	attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

      if (!attr || !strcmp(attr->values[0].string.text, "no-hold"))
      {
	job->state->values[0].integer = IPP_JOB_PENDING;
	job->state_value              = IPP_JOB_PENDING;
      }
    }

    cupsdSaveJob(job);

   /*
    * Start the job if possible...  Since cupsdCheckJobs() can cancel a
    * job if it doesn't print, we need to re-find the job afterwards...
    */

    jobid = job->id;

    cupsdCheckJobs();

    job = cupsdFindJob(jobid);
  }
  else
  {
    if ((attr = ippFindAttribute(job->attrs, "job-hold-until",
                                 IPP_TAG_KEYWORD)) == NULL)
      attr = ippFindAttribute(job->attrs, "job-hold-until", IPP_TAG_NAME);

    if (!attr || !strcmp(attr->values[0].string.text, "no-hold"))
    {
      job->state->values[0].integer = IPP_JOB_HELD;
      job->state_value              = IPP_JOB_HELD;
      job->hold_until               = time(NULL) + 60;
      cupsdSaveJob(job);
    }
  }

 /*
  * Fill in the response info...
  */

  snprintf(job_uri, sizeof(job_uri), "http://%s:%d/jobs/%d", ServerName,
	   LocalPort, jobid);

  ippAddString(con->response, IPP_TAG_JOB, IPP_TAG_URI, "job-uri", NULL,
               job_uri);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_INTEGER, "job-id", jobid);

  ippAddInteger(con->response, IPP_TAG_JOB, IPP_TAG_ENUM, "job-state",
                job ? job->state_value : IPP_JOB_CANCELED);
  add_job_state_reasons(con, job);

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'send_http_error()' - Send a HTTP error back to the IPP client.
 */

static void
send_http_error(cupsd_client_t *con,	/* I - Client connection */
                http_status_t  status)	/* I - HTTP status code */
{
  cupsdLogMessage(CUPSD_LOG_ERROR, "%s: %s",
                  ippOpString(con->request->request.op.operation_id),
		  httpStatus(status));

  cupsdSendError(con, status);

  ippDelete(con->response);
  con->response = NULL;

  return;
}


/*
 * 'send_ipp_status()' - Send a status back to the IPP client.
 */

static void
send_ipp_status(cupsd_client_t *con,	/* I - Client connection */
               ipp_status_t   status,	/* I - IPP status code */
	       const char     *message,	/* I - Status message */
	       ...)			/* I - Additional args as needed */
{
  va_list	ap;			/* Pointer to additional args */
  char		formatted[1024];	/* Formatted errror message */


  if (message)
  {
    va_start(ap, message);
    vsnprintf(formatted, sizeof(formatted),
              _cupsLangString(con->language, message), ap);
    va_end(ap);

    cupsdLogMessage(CUPSD_LOG_DEBUG, "%s %s: %s",
		    ippOpString(con->request->request.op.operation_id),
		    ippErrorString(status), formatted);
  }
  else
    cupsdLogMessage(CUPSD_LOG_DEBUG, "%s %s",
		    ippOpString(con->request->request.op.operation_id),
		    ippErrorString(status));

  con->response->request.status.status_code = status;

  if (ippFindAttribute(con->response, "attributes-charset",
                       IPP_TAG_ZERO) == NULL)
    ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                 "attributes-charset", NULL, DefaultCharset);

  if (ippFindAttribute(con->response, "attributes-natural-language",
                       IPP_TAG_ZERO) == NULL)
    ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                 "attributes-natural-language", NULL, DefaultLanguage);

  if (message)
    ippAddString(con->response, IPP_TAG_OPERATION, IPP_TAG_TEXT,
        	 "status-message", NULL, formatted);
}


/*
 * 'set_default()' - Set the default destination...
 */

static void
set_default(cupsd_client_t  *con,	/* I - Client connection */
            ipp_attribute_t *uri)	/* I - Printer URI */
{
  http_status_t		status;		/* Policy status */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  cupsd_printer_t	*printer;	/* Printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "set_default(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(DefaultPolicyPtr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Set it as the default...
  */

  DefaultPrinter = printer;

  cupsdSaveAllPrinters();
  cupsdSaveAllClasses();

  cupsdWritePrintcap();

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "Default destination set to \"%s\" by \"%s\".",
		  printer->name, get_username(con));

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'set_job_attrs()' - Set job attributes.
 */

static void
set_job_attrs(cupsd_client_t  *con,	/* I - Client connection */
	      ipp_attribute_t *uri)	/* I - Job URI */
{
  ipp_attribute_t	*attr,		/* Current attribute */
			*attr2;		/* Job attribute */
  int			jobid;		/* Job ID */
  cupsd_job_t		*job;		/* Current job */
  char			method[HTTP_MAX_URI],
					/* Method portion of URI */
			username[HTTP_MAX_URI],
					/* Username portion of URI */
			host[HTTP_MAX_URI],
					/* Host portion of URI */
			resource[HTTP_MAX_URI];
					/* Resource portion of URI */
  int			port;		/* Port portion of URI */
  int			event;		/* Events? */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "set_job_attrs(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Start with "everything is OK" status...
  */

  con->response->request.status.status_code = IPP_OK;

 /*
  * See if we have a job URI or a printer URI...
  */

  if (!strcmp(uri->name, "printer-uri"))
  {
   /*
    * Got a printer URI; see if we also have a job-id attribute...
    */

    if ((attr = ippFindAttribute(con->request, "job-id",
                                 IPP_TAG_INTEGER)) == NULL)
    {
      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Got a printer-uri attribute but no job-id!"));
      return;
    }

    jobid = attr->values[0].integer;
  }
  else
  {
   /*
    * Got a job URI; parse it to get the job ID...
    */

    httpSeparateURI(HTTP_URI_CODING_ALL, uri->values[0].string.text, method,
                    sizeof(method), username, sizeof(username), host,
		    sizeof(host), &port, resource, sizeof(resource));

    if (strncmp(resource, "/jobs/", 6))
    {
     /*
      * Not a valid URI!
      */

      send_ipp_status(con, IPP_BAD_REQUEST,
                      _("Bad job-uri attribute \"%s\"!"),
                      uri->values[0].string.text);
      return;
    }

    jobid = atoi(resource + 6);
  }

 /*
  * See if the job exists...
  */

  if ((job = cupsdFindJob(jobid)) == NULL)
  {
   /*
    * Nope - return a "not found" error...
    */

    send_ipp_status(con, IPP_NOT_FOUND, _("Job #%d does not exist!"), jobid);
    return;
  }

 /*
  * See if the job has been completed...
  */

  if (job->state_value > IPP_JOB_STOPPED)
  {
   /*
    * Return a "not-possible" error...
    */

    send_ipp_status(con, IPP_NOT_POSSIBLE,
                    _("Job #%d is finished and cannot be altered!"), jobid);
    return;
  }

 /*
  * See if the job is owned by the requesting user...
  */

  if (!validate_user(job, con, job->username, username, sizeof(username)))
  {
    send_http_error(con, HTTP_UNAUTHORIZED);
    return;
  }

 /*
  * See what the user wants to change.
  */

  cupsdLoadJob(job);

  event = 0;

  for (attr = con->request->attrs; attr; attr = attr->next)
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

      send_ipp_status(con, IPP_ATTRIBUTES_NOT_SETTABLE,
                      _("%s cannot be changed."), attr->name);

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
	send_ipp_status(con, IPP_REQUEST_VALUE, _("Bad job-priority value!"));

	if ((attr2 = copy_attribute(con->response, attr, 0)) != NULL)
          attr2->group_tag = IPP_TAG_UNSUPPORTED_GROUP;
      }
      else if (job->state_value >= IPP_JOB_PROCESSING)
      {
	send_ipp_status(con, IPP_NOT_POSSIBLE,
	                _("Job is completed and cannot be changed."));
	return;
      }
      else if (con->response->request.status.status_code == IPP_OK)
      {
        cupsdSetJobPriority(job, attr->values[0].integer);
        event |= CUPSD_EVENT_JOB_CONFIG_CHANGED;
      }
    }
    else if (!strcmp(attr->name, "job-state"))
    {
     /*
      * Change the job state...
      */

      if (attr->value_tag != IPP_TAG_ENUM)
      {
	send_ipp_status(con, IPP_REQUEST_VALUE, _("Bad job-state value!"));

	if ((attr2 = copy_attribute(con->response, attr, 0)) != NULL)
          attr2->group_tag = IPP_TAG_UNSUPPORTED_GROUP;
      }
      else
      {
        switch (attr->values[0].integer)
	{
	  case IPP_JOB_PENDING :
	  case IPP_JOB_HELD :
	      if (job->state_value > IPP_JOB_HELD)
	      {
		send_ipp_status(con, IPP_NOT_POSSIBLE,
		                _("Job state cannot be changed."));
		return;
	      }
              else if (con->response->request.status.status_code == IPP_OK)
	      {
		job->state->values[0].integer = attr->values[0].integer;
		job->state_value              = (ipp_jstate_t)attr->values[0].integer;

                event |= CUPSD_EVENT_JOB_STATE;
	      }
	      break;

	  case IPP_JOB_PROCESSING :
	  case IPP_JOB_STOPPED :
	      if (job->state_value != attr->values[0].integer)
	      {
		send_ipp_status(con, IPP_NOT_POSSIBLE,
		                _("Job state cannot be changed."));
		return;
	      }
	      break;

	  case IPP_JOB_CANCELED :
	  case IPP_JOB_ABORTED :
	  case IPP_JOB_COMPLETED :
	      if (job->state_value > IPP_JOB_PROCESSING)
	      {
		send_ipp_status(con, IPP_NOT_POSSIBLE,
		                _("Job state cannot be changed."));
		return;
	      }
              else if (con->response->request.status.status_code == IPP_OK)
                cupsdCancelJob(job, 0, (ipp_jstate_t)attr->values[0].integer);
	      break;
	}
      }
    }
    else if (con->response->request.status.status_code != IPP_OK)
      continue;
    else if ((attr2 = ippFindAttribute(job->attrs, attr->name,
                                       IPP_TAG_ZERO)) != NULL)
    {
     /*
      * Some other value; first free the old value...
      */

      if (job->attrs->prev)
        job->attrs->prev->next = attr2->next;
      else
        job->attrs->attrs = attr2->next;

      if (job->attrs->last == attr2)
        job->attrs->last = job->attrs->prev;

      _ippFreeAttr(attr2);

     /*
      * Then copy the attribute...
      */

      copy_attribute(job->attrs, attr, 0);

     /*
      * See if the job-name or job-hold-until is being changed.
      */

      if (!strcmp(attr->name, "job-hold-until"))
      {
        cupsdSetJobHoldUntil(job, attr->values[0].string.text);

	if (!strcmp(attr->values[0].string.text, "no-hold"))
	  cupsdReleaseJob(job);
	else
	  cupsdHoldJob(job);

        event |= CUPSD_EVENT_JOB_CONFIG_CHANGED | CUPSD_EVENT_JOB_STATE;
      }
    }
    else if (attr->value_tag == IPP_TAG_DELETEATTR)
    {
     /*
      * Delete the attribute...
      */

      if ((attr2 = ippFindAttribute(job->attrs, attr->name,
                                    IPP_TAG_ZERO)) != NULL)
      {
        if (job->attrs->prev)
	  job->attrs->prev->next = attr2->next;
	else
	  job->attrs->attrs = attr2->next;

        if (attr2 == job->attrs->last)
	  job->attrs->last = job->attrs->prev;

        _ippFreeAttr(attr2);

        event |= CUPSD_EVENT_JOB_CONFIG_CHANGED;
      }
    }
    else
    {
     /*
      * Add new option by copying it...
      */

      copy_attribute(job->attrs, attr, 0);

      event |= CUPSD_EVENT_JOB_CONFIG_CHANGED;
    }
  }

 /*
  * Save the job...
  */

  cupsdSaveJob(job);

 /*
  * Send events as needed...
  */

  if (event & CUPSD_EVENT_JOB_STATE)
    cupsdAddEvent(CUPSD_EVENT_JOB_STATE, job->printer, job,
                  job->state_value == IPP_JOB_HELD ?
		      "Job held by user." : "Job restarted by user.");

  if (event & CUPSD_EVENT_JOB_CONFIG_CHANGED)
    cupsdAddEvent(CUPSD_EVENT_JOB_CONFIG_CHANGED, job->printer, job,
                  "Job options changed by user.");

 /*
  * Start jobs if possible...
  */

  cupsdCheckJobs();
}


/*
 * 'set_printer_defaults()' - Set printer default options from a request.
 */

static void
set_printer_defaults(
    cupsd_client_t  *con,		/* I - Client connection */
    cupsd_printer_t *printer)		/* I - Printer */
{
  int			i;		/* Looping var */
  ipp_attribute_t 	*attr;		/* Current attribute */
  int			namelen;	/* Length of attribute name */
  char			name[256],	/* New attribute name */
			value[256];	/* String version of integer attrs */


  for (attr = con->request->attrs; attr; attr = attr->next)
  {
   /*
    * Skip non-printer attributes...
    */

    if (attr->group_tag != IPP_TAG_PRINTER || !attr->name)
      continue;

    cupsdLogMessage(CUPSD_LOG_DEBUG2, "set_printer_defaults: %s", attr->name);

    if (!strcmp(attr->name, "job-sheets-default"))
    {
     /*
      * Only allow keywords and names...
      */

      if (attr->value_tag != IPP_TAG_NAME && attr->value_tag != IPP_TAG_KEYWORD)
        continue;

     /*
      * Only allow job-sheets-default to be set when running without a
      * system high classification level...
      */

      if (Classification)
        continue;

      cupsdSetString(&printer->job_sheets[0], attr->values[0].string.text);

      if (attr->num_values > 1)
	cupsdSetString(&printer->job_sheets[1], attr->values[1].string.text);
      else
	cupsdSetString(&printer->job_sheets[1], "none");
    }
    else if (!strcmp(attr->name, "requesting-user-name-allowed"))
    {
      cupsdFreePrinterUsers(printer);

      printer->deny_users = 0;

      if (attr->value_tag == IPP_TAG_NAME &&
          (attr->num_values > 1 ||
	   strcmp(attr->values[0].string.text, "all")))
      {
	for (i = 0; i < attr->num_values; i ++)
	  cupsdAddPrinterUser(printer, attr->values[i].string.text);
      }
    }
    else if (!strcmp(attr->name, "requesting-user-name-denied"))
    {
      cupsdFreePrinterUsers(printer);

      printer->deny_users = 1;

      if (attr->value_tag == IPP_TAG_NAME &&
          (attr->num_values > 1 ||
	   strcmp(attr->values[0].string.text, "none")))
      {
	for (i = 0; i < attr->num_values; i ++)
	  cupsdAddPrinterUser(printer, attr->values[i].string.text);
      }
    }
    else if (!strcmp(attr->name, "job-quota-period"))
    {
      if (attr->value_tag != IPP_TAG_INTEGER)
        continue;

      cupsdLogMessage(CUPSD_LOG_DEBUG, "Setting job-quota-period to %d...",
        	      attr->values[0].integer);
      cupsdFreeQuotas(printer);

      printer->quota_period = attr->values[0].integer;
    }
    else if (!strcmp(attr->name, "job-k-limit"))
    {
      if (attr->value_tag != IPP_TAG_INTEGER)
        continue;

      cupsdLogMessage(CUPSD_LOG_DEBUG, "Setting job-k-limit to %d...",
        	      attr->values[0].integer);
      cupsdFreeQuotas(printer);

      printer->k_limit = attr->values[0].integer;
    }
    else if (!strcmp(attr->name, "job-page-limit"))
    {
      if (attr->value_tag != IPP_TAG_INTEGER)
        continue;

      cupsdLogMessage(CUPSD_LOG_DEBUG, "Setting job-page-limit to %d...",
        	      attr->values[0].integer);
      cupsdFreeQuotas(printer);

      printer->page_limit = attr->values[0].integer;
    }
    else if (!strcmp(attr->name, "printer-op-policy"))
    {
      cupsd_policy_t *p;		/* Policy */


      if (attr->value_tag != IPP_TAG_NAME)
        continue;

      if ((p = cupsdFindPolicy(attr->values[0].string.text)) != NULL)
      {
	cupsdLogMessage(CUPSD_LOG_DEBUG,
                	"Setting printer-op-policy to \"%s\"...",
                	attr->values[0].string.text);
	cupsdSetString(&printer->op_policy, attr->values[0].string.text);
	printer->op_policy_ptr = p;
      }
      else
      {
	send_ipp_status(con, IPP_NOT_POSSIBLE,
                	_("Unknown printer-op-policy \"%s\"."),
                	attr->values[0].string.text);
	return;
      }
    }
    else if (!strcmp(attr->name, "printer-error-policy"))
    {
      if (attr->value_tag != IPP_TAG_NAME && attr->value_tag != IPP_TAG_KEYWORD)
        continue;

      if (strcmp(attr->values[0].string.text, "abort-job") &&
          strcmp(attr->values[0].string.text, "retry-job") &&
          strcmp(attr->values[0].string.text, "stop-printer"))
      {
	send_ipp_status(con, IPP_NOT_POSSIBLE,
                	_("Unknown printer-error-policy \"%s\"."),
                	attr->values[0].string.text);
	return;
      }

      cupsdLogMessage(CUPSD_LOG_DEBUG,
                      "Setting printer-error-policy to \"%s\"...",
                      attr->values[0].string.text);
      cupsdSetString(&printer->error_policy, attr->values[0].string.text);
    }
    else if (!strcmp(attr->name, "notify-lease-duration-default") ||
             !strcmp(attr->name, "notify-events-default"))
      continue;

   /*
    * Skip any other non-default attributes...
    */

    namelen = strlen(attr->name);
    if (namelen < 9 || strcmp(attr->name + namelen - 8, "-default") ||
        namelen > (sizeof(name) - 1) || attr->num_values != 1)
      continue;

   /*
    * OK, anything else must be a user-defined default...
    */

    strlcpy(name, attr->name, sizeof(name));
    name[namelen - 8] = '\0';		/* Strip "-default" */

    switch (attr->value_tag)
    {
      case IPP_TAG_DELETEATTR :
          printer->num_options = cupsRemoveOption(name,
						  printer->num_options,
						  &(printer->options));
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Deleting %s", attr->name);
          break;

      case IPP_TAG_NAME :
      case IPP_TAG_KEYWORD :
      case IPP_TAG_URI :
          printer->num_options = cupsAddOption(name,
	                                       attr->values[0].string.text,
					       printer->num_options,
					       &(printer->options));
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Setting %s to \"%s\"...", attr->name,
			  attr->values[0].string.text);
          break;

      case IPP_TAG_BOOLEAN :
          printer->num_options = cupsAddOption(name,
	                                       attr->values[0].boolean ?
					           "true" : "false",
					       printer->num_options,
					       &(printer->options));
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Setting %s to %s...", attr->name,
			  attr->values[0].boolean ? "true" : "false");
          break;

      case IPP_TAG_INTEGER :
      case IPP_TAG_ENUM :
          sprintf(value, "%d", attr->values[0].integer);
          printer->num_options = cupsAddOption(name, value,
					       printer->num_options,
					       &(printer->options));
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Setting %s to %s...", attr->name, value);
          break;

      case IPP_TAG_RANGE :
          sprintf(value, "%d-%d", attr->values[0].range.lower,
	          attr->values[0].range.upper);
          printer->num_options = cupsAddOption(name, value,
					       printer->num_options,
					       &(printer->options));
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Setting %s to %s...", attr->name, value);
          break;

      case IPP_TAG_RESOLUTION :
          sprintf(value, "%dx%d%s", attr->values[0].resolution.xres,
	          attr->values[0].resolution.yres,
		  attr->values[0].resolution.units == IPP_RES_PER_INCH ?
		      "dpi" : "dpc");
          printer->num_options = cupsAddOption(name, value,
					       printer->num_options,
					       &(printer->options));
          cupsdLogMessage(CUPSD_LOG_DEBUG,
	                  "Setting %s to %s...", attr->name, value);
          break;

      default :
          /* Do nothing for other values */
	  break;
    }
  }
}


/*
 * 'start_printer()' - Start a printer.
 */

static void
start_printer(cupsd_client_t  *con,	/* I - Client connection */
              ipp_attribute_t *uri)	/* I - Printer URI */
{
  http_status_t		status;		/* Policy status */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  cupsd_printer_t	*printer;	/* Printer data */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "start_printer(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Start the printer...
  */

  printer->state_message[0] = '\0';

  cupsdStartPrinter(printer, 1);

  if (dtype & CUPS_PRINTER_CLASS)
    cupsdLogMessage(CUPSD_LOG_INFO, "Class \"%s\" started by \"%s\".",
                    printer->name, get_username(con));
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Printer \"%s\" started by \"%s\".",
                    printer->name, get_username(con));

  cupsdCheckJobs();

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'stop_printer()' - Stop a printer.
 */

static void
stop_printer(cupsd_client_t  *con,	/* I - Client connection */
             ipp_attribute_t *uri)	/* I - Printer URI */
{
  http_status_t		status;		/* Policy status */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  cupsd_printer_t	*printer;	/* Printer data */
  ipp_attribute_t	*attr;		/* printer-state-message attribute */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "stop_printer(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Stop the printer...
  */

  if ((attr = ippFindAttribute(con->request, "printer-state-message",
                               IPP_TAG_TEXT)) == NULL)
    strcpy(printer->state_message, "Paused");
  else
  {
    strlcpy(printer->state_message, attr->values[0].string.text,
            sizeof(printer->state_message));
  }

  cupsdStopPrinter(printer, 1);

  if (dtype & CUPS_PRINTER_CLASS)
    cupsdLogMessage(CUPSD_LOG_INFO, "Class \"%s\" stopped by \"%s\".",
                    printer->name, get_username(con));
  else
    cupsdLogMessage(CUPSD_LOG_INFO, "Printer \"%s\" stopped by \"%s\".",
                    printer->name, get_username(con));

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'url_encode_attr()' - URL-encode a string attribute.
 */

static void
url_encode_attr(ipp_attribute_t *attr,	/* I - Attribute */
                char            *buffer,/* I - String buffer */
		int             bufsize)/* I - Size of buffer */
{
  int	i;				/* Looping var */
  char	*bufptr,			/* Pointer into buffer */
	*bufend,			/* End of buffer */
	*valptr;			/* Pointer into value */


  strlcpy(buffer, attr->name, bufsize);
  bufptr = buffer + strlen(buffer);
  bufend = buffer + bufsize - 1;

  for (i = 0; i < attr->num_values; i ++)
  {
    if (bufptr >= bufend)
      break;

    if (i)
      *bufptr++ = ',';
    else
      *bufptr++ = '=';

    if (bufptr >= bufend)
      break;

    *bufptr++ = '\'';

    for (valptr = attr->values[i].string.text;
         *valptr && bufptr < bufend;
	 valptr ++)
      if (*valptr == ' ')
      {
        if (bufptr >= (bufend - 2))
	  break;

        *bufptr++ = '%';
	*bufptr++ = '2';
	*bufptr++ = '0';
      }
      else if (*valptr == '\'' || *valptr == '\\')
      {
        *bufptr++ = '\\';
        *bufptr++ = *valptr;
      }
      else
        *bufptr++ = *valptr;

    if (bufptr >= bufend)
      break;

    *bufptr++ = '\'';
  }

  *bufptr = '\0';
}


/*
 * 'user_allowed()' - See if a user is allowed to print to a queue.
 */

static int				/* O - 0 if not allowed, 1 if allowed */
user_allowed(cupsd_printer_t *p,	/* I - Printer or class */
             const char      *username)	/* I - Username */
{
  int		i;			/* Looping var */
  struct passwd	*pw;			/* User password data */


  if (p->num_users == 0)
    return (1);

  if (!strcmp(username, "root"))
    return (1);

  pw = getpwnam(username);
  endpwent();

  for (i = 0; i < p->num_users; i ++)
  {
    if (p->users[i][0] == '@')
    {
     /*
      * Check group membership...
      */

      if (cupsdCheckGroup(username, pw, p->users[i] + 1))
        break;
    }
    else if (!strcasecmp(username, p->users[i]))
      break;
  }

  return ((i < p->num_users) != p->deny_users);
}


/*
 * 'validate_job()' - Validate printer options and destination.
 */

static void
validate_job(cupsd_client_t  *con,	/* I - Client connection */
	     ipp_attribute_t *uri)	/* I - Printer URI */
{
  http_status_t		status;		/* Policy status */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  char			super[MIME_MAX_SUPER],
					/* Supertype of file */
			type[MIME_MAX_TYPE];
					/* Subtype of file */
  cupsd_printer_t	*printer;	/* Printer */


  cupsdLogMessage(CUPSD_LOG_DEBUG2, "validate_job(%p[%d], %s)", con,
                  con->http.fd, uri->values[0].string.text);

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression",
                               IPP_TAG_KEYWORD)) != NULL &&
      !strcmp(attr->values[0].string.text, "none"))
  {
    send_ipp_status(con, IPP_ATTRIBUTES,
                    _("Unsupported compression attribute %s!"),
                    attr->values[0].string.text);
    ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_KEYWORD,
	         "compression", NULL, attr->values[0].string.text);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format",
                                 IPP_TAG_MIMETYPE)) != NULL)
  {
    if (sscanf(format->values[0].string.text, "%15[^/]/%31[^;]", super, type) != 2)
    {
      send_ipp_status(con, IPP_BAD_REQUEST, _("Bad document-format \"%s\"!"),
		      format->values[0].string.text);
      return;
    }

    if ((strcmp(super, "application") || strcmp(type, "octet-stream")) &&
	!mimeType(MimeDatabase, super, type))
    {
      cupsdLogMessage(CUPSD_LOG_INFO,
                      "Hint: Do you have the raw file printing rules enabled?");
      send_ipp_status(con, IPP_DOCUMENT_FORMAT,
                      _("Unsupported format \"%s\"!"),
		      format->values[0].string.text);
      ippAddString(con->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_MIMETYPE,
                   "document-format", NULL, format->values[0].string.text);
      return;
    }
  }

 /*
  * Is the destination valid?
  */

  if (!cupsdValidateDest(uri->values[0].string.text, &dtype, &printer))
  {
   /*
    * Bad URI...
    */

    send_ipp_status(con, IPP_NOT_FOUND,
                    _("The printer or class was not found."));
    return;
  }

 /*
  * Check policy...
  */

  if ((status = cupsdCheckPolicy(printer->op_policy_ptr, con, NULL)) != HTTP_OK)
  {
    send_http_error(con, status);
    return;
  }

 /*
  * Everything was ok, so return OK status...
  */

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'validate_name()' - Make sure the printer name only contains valid chars.
 */

static int			/* O - 0 if name is no good, 1 if name is good */
validate_name(const char *name)	/* I - Name to check */
{
  const char	*ptr;		/* Pointer into name */


 /*
  * Scan the whole name...
  */

  for (ptr = name; *ptr; ptr ++)
    if ((*ptr >= 0 && *ptr <= ' ') || *ptr == 127 || *ptr == '/' || *ptr == '#')
      return (0);

 /*
  * All the characters are good; validate the length, too...
  */

  return ((ptr - name) < 128);
}


/*
 * 'validate_user()' - Validate the user for the request.
 */

static int				/* O - 1 if permitted, 0 otherwise */
validate_user(cupsd_job_t    *job,	/* I - Job */
              cupsd_client_t *con,	/* I - Client connection */
              const char     *owner,	/* I - Owner of job/resource */
              char           *username,	/* O - Authenticated username */
	      int            userlen)	/* I - Length of username */
{
  cupsd_printer_t	*printer;	/* Printer for job */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "validate_user(job=%d, con=%d, owner=\"%s\", username=%p, "
		  "userlen=%d)",
        	  job ? job->id : 0, con->http.fd, owner ? owner : "(null)",
		  username, userlen);

 /*
  * Validate input...
  */

  if (!con || !owner || !username || userlen <= 0)
    return (0);

 /*
  * Get the best authenticated username that is available.
  */

  strlcpy(username, get_username(con), userlen);

 /*
  * Check the username against the owner...
  */

  printer = cupsdFindDest(job->dest);

  return (cupsdCheckPolicy(printer ? printer->op_policy_ptr : DefaultPolicyPtr,
                           con, owner) == HTTP_OK);
}


/*
 * End of "$Id$".
 */
