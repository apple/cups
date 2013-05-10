/*
 * "$Id$"
 *
 *   IPP utilities for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsDoFileRequest() - Do an IPP request with a file.
 *   cupsDoRequest()     - Do an IPP request.
 *   _cupsSetError()     - Set the last IPP status code and status-message.
 */

/*
 * Include necessary headers...
 */

#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * 'cupsDoFileRequest()' - Do an IPP request with a file.
 *
 * This function sends the IPP request to the specified server, retrying
 * and authenticating as necessary.  The request is freed with ippDelete()
 * after receiving a valid IPP response.
 */

ipp_t *					/* O - Response data */
cupsDoFileRequest(http_t     *http,	/* I - HTTP connection to server */
                  ipp_t      *request,	/* I - IPP request */
                  const char *resource,	/* I - HTTP resource for POST */
		  const char *filename)	/* I - File to send or NULL for none */
{
  ipp_t		*response;		/* IPP response data */
  size_t	length;			/* Content-Length value */
  http_status_t	status;			/* Status of HTTP request */
  int		got_status;		/* Did we get the status? */
  ipp_state_t	state;			/* State of IPP processing */
  FILE		*file;			/* File to send */
  struct stat	fileinfo;		/* File information */
  int		bytes;			/* Number of bytes read/written */
  char		buffer[32768];		/* Output buffer */
  http_status_t	expect;			/* Expect: header to use */


  DEBUG_printf(("cupsDoFileRequest(%p, %p, \'%s\', \'%s\')\n",
                http, request, resource ? resource : "(null)",
		filename ? filename : "(null)"));

  if (http == NULL || request == NULL || resource == NULL)
  {
    if (request != NULL)
      ippDelete(request);

    _cupsSetError(IPP_INTERNAL_ERROR, NULL);

    return (NULL);
  }

 /*
  * See if we have a file to send...
  */

  if (filename != NULL)
  {
    if (stat(filename, &fileinfo))
    {
     /*
      * Can't get file information!
      */

      _cupsSetError(errno == ENOENT ? IPP_NOT_FOUND : IPP_NOT_AUTHORIZED,
                     strerror(errno));

      ippDelete(request);

      return (NULL);
    }

#ifdef WIN32
    if (fileinfo.st_mode & _S_IFDIR)
#else
    if (S_ISDIR(fileinfo.st_mode))
#endif /* WIN32 */
    {
     /*
      * Can't send a directory...
      */

      ippDelete(request);

      _cupsSetError(IPP_NOT_POSSIBLE, strerror(EISDIR));

      return (NULL);
    }

    if ((file = fopen(filename, "rb")) == NULL)
    {
     /*
      * Can't open file!
      */

      _cupsSetError(errno == ENOENT ? IPP_NOT_FOUND : IPP_NOT_AUTHORIZED,
                     strerror(errno));

      ippDelete(request);

      return (NULL);
    }
  }
  else
    file = NULL;

 /*
  * Loop until we can send the request without authorization problems.
  */

  response = NULL;
  status   = HTTP_ERROR;
  expect   = HTTP_CONTINUE;

  while (response == NULL)
  {
    DEBUG_puts("cupsDoFileRequest: setup...");

   /*
    * Setup the HTTP variables needed...
    */

    length = ippLength(request);
    if (filename)
      length += fileinfo.st_size;

    httpClearFields(http);
    httpSetLength(http, length);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);
    httpSetExpect(http, expect);

    DEBUG_printf(("cupsDoFileRequest: authstring=\"%s\"\n", http->authstring));

   /*
    * Try the request...
    */

    DEBUG_puts("cupsDoFileRequest: post...");

    if (httpPost(http, resource))
    {
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }
      else
        continue;
    }

   /*
    * Send the IPP data...
    */

    DEBUG_puts("cupsDoFileRequest: ipp write...");

    request->state = IPP_IDLE;
    status         = HTTP_CONTINUE;
    got_status     = 0;

    while ((state = ippWrite(http, request)) != IPP_DATA)
      if (state == IPP_ERROR)
	break;
      else if (httpCheck(http))
      {
        got_status = 1;

	if ((status = httpUpdate(http)) != HTTP_CONTINUE)
	  break;
      }

    if (!got_status)
    {
     /*
      * Wait up to 1 second to get the 100-continue response...
      */

      if (httpWait(http, 1000))
        status = httpUpdate(http);
    }
    else if (httpCheck(http))
      status = httpUpdate(http);

    if (status == HTTP_CONTINUE && state == IPP_DATA && filename)
    {
      DEBUG_puts("cupsDoFileRequest: file write...");

     /*
      * Send the file...
      */

      rewind(file);

      while ((bytes = (int)fread(buffer, 1, sizeof(buffer), file)) > 0)
      {
	if (httpCheck(http))
	{
	  if ((status = httpUpdate(http)) != HTTP_CONTINUE)
	    break;
        }

  	if (httpWrite2(http, buffer, bytes) < bytes)
          break;
      }
    }

   /*
    * Get the server's return status...
    */

    DEBUG_puts("cupsDoFileRequest: update...");

    while (status == HTTP_CONTINUE)
      status = httpUpdate(http);

    DEBUG_printf(("cupsDoFileRequest: status = %d\n", status));

    if (status == HTTP_UNAUTHORIZED)
    {
      DEBUG_puts("cupsDoFileRequest: unauthorized...");

     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * See if we can do authentication...
      */

      if (cupsDoAuthentication(http, "POST", resource))
        break;

      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
	break;
      }

      continue;
    }
    else if (status == HTTP_ERROR)
    {
      DEBUG_printf(("cupsDoFileRequest: http->error=%d (%s)\n", http->error,
                    strerror(http->error)));

#ifdef WIN32
      if (http->error != WSAENETDOWN && http->error != WSAENETUNREACH &&
          http->error != ETIMEDOUT)
#else
      if (http->error != ENETDOWN && http->error != ENETUNREACH &&
          http->error != ETIMEDOUT)
#endif /* WIN32 */
        continue;
      else
        break;
    }
#ifdef HAVE_SSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush(http);

      /* Reconnect... */
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }

      /* Upgrade with encryption... */
      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);

      /* Try again, this time with encryption enabled... */
      continue;
    }
#endif /* HAVE_SSL */
    else if (status == HTTP_EXPECTATION_FAILED)
    {
     /*
      * Don't try using the Expect: header the next time around...
      */

      expect = (http_status_t)0;
    }
    else if (status != HTTP_OK)
    {
      DEBUG_printf(("cupsDoFileRequest: error %d...\n", status));

     /*
      * Flush any error message...
      */

      httpFlush(http);
      break;
    }
    else
    {
     /*
      * Read the response...
      */

      DEBUG_puts("cupsDoFileRequest: response...");

      response = ippNew();

      while ((state = ippRead(http, response)) != IPP_DATA)
	if (state == IPP_ERROR)
	{
	 /*
          * Delete the response...
	  */

          DEBUG_puts("IPP read error!");
	  ippDelete(response);
	  response = NULL;

          _cupsSetError(IPP_SERVICE_UNAVAILABLE, strerror(errno));

	  break;
	}
    }
  }

 /*
  * Close the file if needed...
  */

  if (filename != NULL)
    fclose(file);

 /*
  * Flush any remaining data...
  */

  httpFlush(http);

 /*
  * Delete the original request and return the response...
  */
  
  ippDelete(request);

  if (response)
  {
    ipp_attribute_t	*attr;		/* status-message attribute */


    attr = ippFindAttribute(response, "status-message", IPP_TAG_TEXT);

    _cupsSetError(response->request.status.status_code,
                   attr ? attr->values[0].string.text :
		       ippErrorString(response->request.status.status_code));
  }
  else if (status != HTTP_OK)
  {
    switch (status)
    {
      case HTTP_NOT_FOUND :
          _cupsSetError(IPP_NOT_FOUND, httpStatus(status));
	  break;

      case HTTP_UNAUTHORIZED :
          _cupsSetError(IPP_NOT_AUTHORIZED, httpStatus(status));
	  break;

      case HTTP_FORBIDDEN :
          _cupsSetError(IPP_FORBIDDEN, httpStatus(status));
	  break;

      case HTTP_BAD_REQUEST :
          _cupsSetError(IPP_BAD_REQUEST, httpStatus(status));
	  break;

      case HTTP_REQUEST_TOO_LARGE :
          _cupsSetError(IPP_REQUEST_VALUE, httpStatus(status));
	  break;

      case HTTP_NOT_IMPLEMENTED :
          _cupsSetError(IPP_OPERATION_NOT_SUPPORTED, httpStatus(status));
	  break;

      case HTTP_NOT_SUPPORTED :
          _cupsSetError(IPP_VERSION_NOT_SUPPORTED, httpStatus(status));
	  break;

      default :
	  DEBUG_printf(("HTTP error %d mapped to IPP_SERVICE_UNAVAILABLE!\n",
                	status));
	  _cupsSetError(IPP_SERVICE_UNAVAILABLE, httpStatus(status));
	  break;
    }
  }

  return (response);
}


/*
 * 'cupsDoRequest()' - Do an IPP request.
 *
 * This function sends the IPP request to the specified server, retrying
 * and authenticating as necessary.  The request is freed with ippDelete()
 * after receiving a valid IPP response.
 */

ipp_t *					/* O - Response data */
cupsDoRequest(http_t     *http,		/* I - HTTP connection to server */
              ipp_t      *request,	/* I - IPP request */
              const char *resource)	/* I - HTTP resource for POST */
{
  return (cupsDoFileRequest(http, request, resource, NULL));
}


/*
 * '_cupsSetError()' - Set the last IPP status code and status-message.
 */

void
_cupsSetError(ipp_status_t status,	/* I - IPP status code */
               const char   *message)	/* I - status-message value */
{
  _cups_globals_t	*cg;		/* Global data */


  cg             = _cupsGlobals();
  cg->last_error = status;

  if (cg->last_status_message)
  {
    free(cg->last_status_message);

    cg->last_status_message = NULL;
  }

  if (message)
    cg->last_status_message = strdup(message);
}


/*
 * End of "$Id$".
 */
