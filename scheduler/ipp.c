/*
 * "$Id: ipp.c,v 1.4 1999/03/01 22:26:17 mike Exp $"
 *
 *   IPP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static void	add_class(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language);
static void	add_printer(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language);
static void	cancel_job(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language, ipp_attribute_t *uri);
static void	delete_class(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language);
static void	delete_printer(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language);
static void	get_classes(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language);
static void	get_jobs(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language, ipp_attribute_t *uri);
static void	get_job_attrs(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language, ipp_attribute_t *uri);
static void	get_printers(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language);
static void	get_printer_attrs(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language, ipp_attribute_t *uri);
static void	print_job(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language, ipp_attribute_t *uri);
static void	send_ipp_error(client_t *con, ipp_status_t status);
static void	validate_job(client_t *con, ipp_attribute_t *charset, ipp_attribute_t *language, ipp_attribute_t *uri);


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

  con->response->request.status.version[0] = 1;
  con->response->request.status.version[1] = 0;
  con->response->request.status.request_id = con->request->request.op.request_id;

 /*
  * Then validate the request header and required attributes...
  */
  
  if (con->request->request.any.version[0] != 1)
  {
   /*
    * Return an error, since we only support IPP 1.x.
    */

    send_ipp_error(con, IPP_VERSION_NOT_SUPPORTED);

    return;
  }  

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

      DEBUG_puts("ProcessIPPRequest: attribute groups are out of order!");
      send_ipp_error(con, IPP_BAD_REQUEST);
      return;
    }
    else
      group = attr->group_tag;

 /*
  * Then make sure that the first three attributes are:
  *
  *     attributes-charset
  *     attributes-natural-language
  *     printer-uri
  */

  attr = con->request->attrs;
  if (attr != NULL && strcmp(attr->name, "attributes-charset") == 0 &&
      attr->value_tag == IPP_TAG_CHARSET)
    charset = attr;
  else
    charset = NULL;

  attr = attr->next;
  if (attr != NULL && strcmp(attr->name, "attributes-natural-language") == 0 &&
      attr->value_tag == IPP_TAG_LANGUAGE)
    language = attr;
  else
    language = NULL;

  attr = attr->next;
  if (attr != NULL && strcmp(attr->name, "printer-uri") == 0 &&
      attr->value_tag == IPP_TAG_URI)
    uri = attr;
  else if (attr != NULL && strcmp(attr->name, "job-uri") == 0 &&
           attr->value_tag == IPP_TAG_URI)
    uri = attr;
  else
    uri = NULL;

  if (charset == NULL || language == NULL ||
      (uri == NULL && con->request->request.op.operation_id < IPP_PRIVATE))
  {
   /*
    * Return an error, since attributes-charset,
    * attributes-natural-language, and printer-uri/job-uri are required
    * for all operations.
    */

    DEBUG_printf(("ProcessIPPRequest: missing attributes (%08x, %08x, %08x)!\n",
                  charset, language, uri));
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  attr = ippAddString(con->response, IPP_TAG_OPERATION, "attributes-charset",
                      charset->values[0].string);
  attr->value_tag = IPP_TAG_CHARSET;

  attr = ippAddString(con->response, IPP_TAG_OPERATION,
                      "attributes-natural-language", language->values[0].string);
  attr->value_tag = IPP_TAG_LANGUAGE;

 /*
  * OK, all the checks pass so far; try processing the operation...
  */

  switch (con->request->request.op.operation_id)
  {
    case IPP_PRINT_JOB :
        print_job(con, charset, language, uri);
        break;

    case IPP_VALIDATE_JOB :
        validate_job(con, charset, language, uri);
        break;

    case IPP_CANCEL_JOB :
        cancel_job(con, charset, language, uri);
        break;

    case IPP_GET_JOB_ATTRIBUTES :
        get_job_attrs(con, charset, language, uri);
        break;

    case IPP_GET_JOBS :
        get_jobs(con, charset, language, uri);
        break;

    case IPP_GET_PRINTER_ATTRIBUTES :
        get_printer_attrs(con, charset, language, uri);
        break;

    case CUPS_GET_PRINTERS :
        get_printers(con, charset, language);
        break;

    case CUPS_ADD_PRINTER :
        add_printer(con, charset, language);
        break;

    case CUPS_DELETE_PRINTER :
        delete_printer(con, charset, language);
        break;

    case CUPS_GET_CLASSES :
        get_classes(con, charset, language);
        break;

    case CUPS_ADD_CLASS :
        add_class(con, charset, language);
        break;

    case CUPS_DELETE_CLASS :
        delete_class(con, charset, language);
        break;

    default :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
	return;
  }

  FD_SET(con->http.fd, &OutputSet);
}


static void
add_class(client_t        *con,
          ipp_attribute_t *charset,
	  ipp_attribute_t *language)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
add_printer(client_t        *con,
            ipp_attribute_t *charset,
	    ipp_attribute_t *language)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
cancel_job(client_t        *con,
           ipp_attribute_t *charset,
	   ipp_attribute_t *language,
	   ipp_attribute_t *uri)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
delete_class(client_t        *con,
             ipp_attribute_t *charset,
	     ipp_attribute_t *language)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
delete_printer(client_t        *con,
               ipp_attribute_t *charset,
	       ipp_attribute_t *language)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
get_classes(client_t        *con,
            ipp_attribute_t *charset,
	    ipp_attribute_t *language)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
get_jobs(client_t        *con,
         ipp_attribute_t *charset,
	 ipp_attribute_t *language,
	 ipp_attribute_t *uri)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
get_job_attrs(client_t        *con,
              ipp_attribute_t *charset,
	      ipp_attribute_t *language,
	      ipp_attribute_t *uri)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
get_printers(client_t        *con,
             ipp_attribute_t *charset,
	     ipp_attribute_t *language)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
get_printer_attrs(client_t        *con,
                  ipp_attribute_t *charset,
		  ipp_attribute_t *language,
		  ipp_attribute_t *uri)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


static void
print_job(client_t        *con,
          ipp_attribute_t *charset,
	  ipp_attribute_t *language,
	  ipp_attribute_t *uri)
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*format;	/* Document-format attribute */
  char			*dest;		/* Destination */
  cups_ptype_t		dtype;		/* Destination type (printer or class) */
  int			priority;	/* Job priority */
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
			type[MIME_MAX_TYPE];
					/* Subtype of file */


  DEBUG_printf(("print_job(%08x, %08x, %08x, %08x)\n", con, charset,
                language, uri));

 /*
  * OK, see if the client is sending the document compressed - CUPS
  * doesn't support compression yet...
  */

  if ((attr = ippFindAttribute(con->request, "compression")) != NULL)
  {
    DEBUG_puts("print_job: Unsupported compression attribute!");
    send_ipp_error(con, IPP_ATTRIBUTES);
    attr            = ippAddString(con->response, IPP_TAG_UNSUPPORTED,
	                           "compression", attr->values[0].string);
    attr->value_tag = IPP_TAG_KEYWORD;

    return;
  }

 /*
  * Do we have a file to print?
  */

  if (con->filename[0] == '\0')
  {
    DEBUG_puts("print_job: No filename!?!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * Is it a format we support?
  */

  if ((format = ippFindAttribute(con->request, "document-format")) == NULL ||
      format->value_tag != IPP_TAG_MIMETYPE)
  {
    DEBUG_puts("print_job: missing document-format attribute!");
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  if (sscanf(format->values[0].string, "%15[^/]/%31[^;]", super, type) != 2)
  {
    DEBUG_printf(("print_job: could not scan type \'%s\'!\n",
	          format->values[0].string));
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

  if (strcmp(super, "application") == 0 &&
      strcmp(type, "octet-stream") == 0)
  {
    DEBUG_puts("print_job: auto-typing request using magic rules.");
    filetype = mimeFileType(MimeDatabase, con->filename);
  }
  else
    filetype = mimeType(MimeDatabase, super, type);

  if (filetype == NULL)
  {
    DEBUG_printf(("print_job: Unsupported format \'%s\'!\n",
	          format->values[0].string));
    send_ipp_error(con, IPP_ATTRIBUTES);
    attr            = ippAddString(con->response, IPP_TAG_UNSUPPORTED,
	                           "document-format",
				   format->values[0].string);
    attr->value_tag = IPP_TAG_MIMETYPE;

    return;
  }

  DEBUG_printf(("print_job: request file type is %s/%s.\n",
	        filetype->super, filetype->type));

 /*
  * Is the destination valid?
  */

  httpSeparate(uri->values[0].string, method, username, host, &port, resource);

  if (strncmp(resource, "/classes/", 9) == 0)
  {
   /*
    * Print to a class...
    */

    dest  = resource + 9;
    dtype = CUPS_PRINTER_CLASS;

    if (FindClass(dest) == NULL)
    {
      send_ipp_error(con, IPP_NOT_FOUND);
      return;
    }
  }
  else if (strncmp(resource, "/printers/", 10) == 0)
  {
   /*
    * Print to a specific printer...
    */

    dest  = resource + 10;
    dtype = (cups_ptype_t)0;

    if (FindPrinter(dest) == NULL && FindClass(dest) == NULL)
    {
      send_ipp_error(con, IPP_NOT_FOUND);
      return;
    }
  }
  else
  {
   /*
    * Bad URI...
    */

    DEBUG_printf(("print_job: resource name \'%s\' no good!\n",
	          resource));
    send_ipp_error(con, IPP_BAD_REQUEST);
    return;
  }

 /*
  * Create the job and set things up...
  */

  if ((attr = ippFindAttribute(con->request, "job-priority")) != NULL &&
      attr->value_tag == IPP_TAG_INTEGER)
    priority = attr->values[0].integer;
  else
    priority = 50;

  if ((job = AddJob(priority, dest)) == NULL)
  {
    send_ipp_error(con, IPP_INTERNAL_ERROR);
    return;
  }

  job->dtype    = dtype;
  job->state    = IPP_JOB_PENDING;
  job->filetype = filetype;
  job->attrs    = con->request;
  con->request  = NULL;

  strcpy(job->filename, con->filename);
  con->filename[0] = '\0';

  strcpy(job->username, con->username);
  if ((attr = ippFindAttribute(con->request, "requesting-user-name")) != NULL &&
      attr->value_tag == IPP_TAG_NAME)
  {
    strncpy(job->username, attr->values[0].string, sizeof(job->username) - 1);
    job->username[sizeof(job->username) - 1] = '\0';
  }

  if (job->username[0] == '\0')
    strcpy(job->username, "guest");

 /*
  * Start the job if possible...
  */

  CheckJobs();

 /*
  * Fill in the response info...
  */

  sprintf(job_uri, "http://%s:%d/jobs/%d", ServerName,
	  ntohs(con->http.hostaddr.sin_port), job->id);
  attr = ippAddString(con->response, IPP_TAG_JOB, "job-uri", job_uri);
  attr->value_tag = IPP_TAG_URI;

  attr = ippAddInteger(con->response, IPP_TAG_JOB, "job-id", job->id);
  attr->value_tag = IPP_TAG_INTEGER;

  attr = ippAddInteger(con->response, IPP_TAG_JOB, "job-state", job->state);
  attr->value_tag = IPP_TAG_ENUM;

  con->response->request.status.status_code = IPP_OK;
}


/*
 * 'send_ipp_error()' - Send an error status back to the IPP client.
 */

static void
send_ipp_error(client_t     *con,	/* I - Client connection */
               ipp_status_t status)	/* I - IPP status code */
{
  ipp_attribute_t	*attr;		/* Current attribute */


  DEBUG_printf(("send_ipp_error(%08x, %04x)\n", con, status));

  if (con->filename[0])
    unlink(con->filename);

  con->response->request.status.status_code = status;

  attr = ippAddString(con->response, IPP_TAG_OPERATION, "attributes-charset",
                      DefaultCharset);
  attr->value_tag = IPP_TAG_CHARSET;

  attr = ippAddString(con->response, IPP_TAG_OPERATION,
                      "attributes-natural-language", DefaultLanguage);
  attr->value_tag = IPP_TAG_LANGUAGE;

  FD_SET(con->http.fd, &OutputSet);
}


static void
validate_job(client_t        *con,
             ipp_attribute_t *charset,
	     ipp_attribute_t *language,
	     ipp_attribute_t *uri)
{
  send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
}


/*
 * End of "$Id: ipp.c,v 1.4 1999/03/01 22:26:17 mike Exp $".
 */
