/*
 * "$Id$"
 *
 *   IPP utilities for the Common UNIX Printing System (CUPS).
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsDoFileRequest()    - Do an IPP request with a file.
 *   cupsDoIORequest()      - Do an IPP request with file descriptors.
 *   cupsDoRequest()        - Do an IPP request.
 *   cupsGetResponse()      - Get a response to an IPP request.
 *   cupsReadResponseData() - Read additional data after the IPP response.
 *   cupsSendRequest()      - Send an IPP request.
 *   cupsWriteRequestData() - Write additional data after an IPP request.
 *   _cupsSetError()        - Set the last IPP status code and status-message.
 *   _cupsSetHTTPError()    - Set the last error using the HTTP status.
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
#ifndef O_BINARY
#  define O_BINARY 0
#endif /* O_BINARY */


/*
 * 'cupsDoFileRequest()' - Do an IPP request with a file.
 *
 * This function sends the IPP request to the specified server, retrying
 * and authenticating as necessary.  The request is freed with @link ippDelete@
 * after receiving a valid IPP response.
 */

ipp_t *					/* O - Response data */
cupsDoFileRequest(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                  ipp_t      *request,	/* I - IPP request */
                  const char *resource,	/* I - HTTP resource for POST */
		  const char *filename)	/* I - File to send or @code NULL@ for none */
{
  ipp_t		*response;		/* IPP response data */
  int		infile;			/* Input file */


  DEBUG_printf(("cupsDoFileRequest(http=%p, request=%p(%s), resource=\"%s\", "
                "filename=\"%s\")\n", http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource ? resource : "(null)",
		filename ? filename : "(null)"));

  if (filename)
  {
    if ((infile = open(filename, O_RDONLY | O_BINARY)) < 0)
    {
     /*
      * Can't get file information!
      */

      _cupsSetError(errno == ENOENT ? IPP_NOT_FOUND : IPP_NOT_AUTHORIZED,
                    strerror(errno));

      ippDelete(request);

      return (NULL);
    }
  }
  else
    infile = -1;

  response = cupsDoIORequest(http, request, resource, infile, -1);

  if (infile >= 0)
    close(infile);

  return (response);
}


/*
 * 'cupsDoIORequest()' - Do an IPP request with file descriptors.
 *
 * This function sends the IPP request to the specified server, retrying
 * and authenticating as necessary.  The request is freed with ippDelete()
 * after receiving a valid IPP response.
 *
 * If "infile" is a valid file descriptor, cupsDoIORequest() copies
 * all of the data from the file after the IPP request message.
 *
 * If "outfile" is a valid file descriptor, cupsDoIORequest() copies
 * all of the data after the IPP response message to the file.
 *
 * @since CUPS 1.3@
 */

ipp_t *					/* O - Response data */
cupsDoIORequest(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                ipp_t      *request,	/* I - IPP request */
                const char *resource,	/* I - HTTP resource for POST */
		int        infile,	/* I - File to read from or -1 for none */
		int        outfile)	/* I - File to write to or -1 for none */
{
  ipp_t		*response = NULL;	/* IPP response data */
  size_t	length = 0;		/* Content-Length value */
  http_status_t	status;			/* Status of HTTP request */
  struct stat	fileinfo;		/* File information */
  int		bytes;			/* Number of bytes read/written */
  char		buffer[32768];		/* Output buffer */


  DEBUG_printf(("cupsDoIORequest(http=%p, request=%p(%s), resource=\"%s\", "
                "infile=%d, outfile=%d)\n", http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource ? resource : "(null)", infile, outfile));

 /*
  * Range check input...
  */

  if (!request || !resource)
  {
    ippDelete(request);

    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL));

    return (NULL);
  }

 /*
  * Get the default connection as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (NULL);

 /*
  * See if we have a file to send...
  */

  if (infile >= 0)
  {
    if (fstat(infile, &fileinfo))
    {
     /*
      * Can't get file information!
      */

      _cupsSetError(errno == EBADF ? IPP_NOT_FOUND : IPP_NOT_AUTHORIZED,
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

#ifndef WIN32
    if (!S_ISREG(fileinfo.st_mode))
      length = 0;			/* Chunk when piping */
    else
#endif /* !WIN32 */
    length = ippLength(request) + fileinfo.st_size;
  }
  else
    length = ippLength(request);

 /*
  * Loop until we can send the request without authorization problems.
  */

  while (response == NULL)
  {
    DEBUG_puts("cupsDoFileRequest: setup...");

   /*
    * Send the request...
    */

    status = cupsSendRequest(http, request, resource, length);

    DEBUG_printf(("cupsDoFileRequest: status=%d\n", status));

    if (status == HTTP_CONTINUE && request->state == IPP_DATA && infile >= 0)
    {
      DEBUG_puts("cupsDoFileRequest: file write...");

     /*
      * Send the file with the request...
      */

#ifndef WIN32
      if (S_ISREG(fileinfo.st_mode))
#endif /* WIN32 */
      lseek(infile, 0, SEEK_SET);

      while ((bytes = (int)read(infile, buffer, sizeof(buffer))) > 0)
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
    * Get the server's response...
    */

    if (status == HTTP_CONTINUE || status == HTTP_OK)
    {
      response = cupsGetResponse(http, resource);
      status   = http->status;
    }

    if (status == HTTP_FORBIDDEN)
      break;

    if (response)
    {
      if (outfile >= 0)
      {
       /*
        * Write trailing data to file...
	*/

	while ((bytes = (int)httpRead2(http, buffer, sizeof(buffer))) > 0)
	  if (write(outfile, buffer, bytes) < bytes)
	    break;
      }
      else
      {
       /*
        * Flush any remaining data...
        */

        httpFlush(http);
      }
    }
  }

 /*
  * Delete the original request and return the response...
  */
  
  ippDelete(request);

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
cupsDoRequest(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
              ipp_t      *request,	/* I - IPP request */
              const char *resource)	/* I - HTTP resource for POST */
{
  DEBUG_printf(("cupsDoRequest(http=%p, request=%p(%s), resource=\"%s\")\n",
                http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource ? resource : "(null)"));

  return (cupsDoFileRequest(http, request, resource, NULL));
}


/*
 * 'cupsGetResponse()' - Get a response to an IPP request.
 *
 * Use this function to get the response for an IPP request sent using
 * cupsSendDocument() or cupsSendRequest(). For requests that return
 * additional data, use httpRead() after getting a successful response.
 *
 * @since CUPS 1.4@
 */

ipp_t *					/* O - Response or @code NULL@ on HTTP error */
cupsGetResponse(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                const char *resource)	/* I - HTTP resource for POST */
{
  http_status_t	status;			/* HTTP status */
  ipp_state_t	state;			/* IPP read state */
  ipp_t		*response = NULL;	/* IPP response */


  DEBUG_printf(("cupsGetReponse(http=%p, resource=\"%s\")\n", http,
                resource ? resource : "(null)"));

 /*
  * Connect to the default server as needed...
  */

  if (!http)
    http = _cupsConnect();

  if (!http || (http->state != HTTP_POST_RECV && http->state != HTTP_POST_SEND))
    return (NULL);

 /*
  * Check for an unfinished chunked request...
  */

  if (http->data_encoding == HTTP_ENCODE_CHUNKED)
  {
   /*
    * Send a 0-length chunk to finish off the request...
    */

    DEBUG_puts("cupsGetResponse: Finishing chunked POST...");

    if (httpWrite2(http, "", 0) < 0)
      return (NULL);
  }

 /*
  * Wait for a response from the server...
  */

  DEBUG_puts("cupsGetResponse: Update loop...");

  while ((status = httpUpdate(http)) == HTTP_CONTINUE)
    /* Do nothing but update */;

  DEBUG_printf(("cupsGetResponse: status=%d\n", status));

  if (status == HTTP_OK)
  {
   /*
    * Get the IPP response...
    */

    response = ippNew();

    while ((state = ippRead(http, response)) != IPP_DATA)
      if (state == IPP_ERROR)
	break;

    if (state == IPP_ERROR)
    {
     /*
      * Delete the response...
      */

      DEBUG_puts("cupsGetResponse: IPP read error!");

      ippDelete(response);
      response = NULL;

      _cupsSetError(IPP_SERVICE_UNAVAILABLE, strerror(errno));
    }
  }
  else if (status != HTTP_ERROR)
  {
   /*
    * Flush any error message...
    */

    httpFlush(http);

   /*
    * Then handle encryption and authentication...
    */

    if (status == HTTP_UNAUTHORIZED)
    {
     /*
      * See if we can do authentication...
      */

      int auth_result;

      DEBUG_puts("cupsGetResponse: Need authorization...");

      if ((auth_result =cupsDoAuthentication(http, "POST", resource)) == 0)
	httpReconnect(http);
      else if (auth_result < 0)
        http->status = status = HTTP_FORBIDDEN;
    }

#ifdef HAVE_SSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
     /*
      * Force a reconnect with encryption...
      */

      DEBUG_puts("cupsGetResponse: Need encryption...");

      if (!httpReconnect(http))
        httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
    }
#endif /* HAVE_SSL */
  }

  if (response)
  {
    ipp_attribute_t	*attr;		/* status-message attribute */


    attr = ippFindAttribute(response, "status-message", IPP_TAG_TEXT);

    DEBUG_printf(("cupsGetResponse: status-code=%s, status-message=\"%s\"\n",
                  ippErrorString(response->request.status.status_code),
                  attr ? attr->values[0].string.text : ""));

    _cupsSetError(response->request.status.status_code,
                   attr ? attr->values[0].string.text :
		       ippErrorString(response->request.status.status_code));
  }
  else if (status != HTTP_OK)
    _cupsSetHTTPError(status);

  return (response);
}


/*
 * 'cupsReadResponseData()' - Read additional data after the IPP response.
 *
 * This function is used after cupsGetResponse() to read the PPD or document
 * files for CUPS_GET_PPD and CUPS_GET_DOCUMENT requests, respectively.
 *
 * @since CUPS 1.4@
 */

ssize_t					/* O - Bytes read, 0 on EOF, -1 on error */
cupsReadResponseData(
    http_t *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    char   *buffer,			/* I - Buffer to use */
    size_t length)			/* I - Number of bytes to read */
{
 /*
  * Get the default connection as needed...
  */

  DEBUG_printf(("cupsReadResponseData(http=%p, buffer=%p, "
                "length=" CUPS_LLFMT ")\n", http, buffer, CUPS_LLCAST length));

  if (!http)
  {
    _cups_globals_t *cg = _cupsGlobals();
					/* Pointer to library globals */

    if ((http = cg->http) == NULL)
    {
      _cupsSetError(IPP_INTERNAL_ERROR, "No active connection");
      return (-1);
    }
  }

 /*
  * Then read from the HTTP connection...
  */

  return (httpRead2(http, buffer, length));
}


/*
 * 'cupsSendRequest()' - Send an IPP request.
 *
 * Use httpWrite() to write any additional data (document, PPD file, etc.)
 * for the request, cupsGetResponse() to get the IPP response, and httpRead()
 * to read any additional data following the response. Only one request can be 
 * sent/queued at a time.
 *
 * Unlike cupsDoFileRequest(), cupsDoIORequest(), and cupsDoRequest(), the
 * request is not freed.
 *
 * @since CUPS 1.4@
 */

http_status_t				/* O - Initial HTTP status */
cupsSendRequest(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                ipp_t      *request,	/* I - IPP request */
                const char *resource,	/* I - Resource path */
		size_t     length)	/* I - Length of data to follow or CUPS_LENGTH_VARIABLE */
{
  http_status_t	status;			/* Status of HTTP request */
  int		got_status;		/* Did we get the status? */
  ipp_state_t	state;			/* State of IPP processing */
  http_status_t	expect;			/* Expect: header to use */


  DEBUG_printf(("cupsSendRequest(http=%p, request=%p(%s), resource=\"%s\", "
                "length=" CUPS_LLFMT ")\n", http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource ? resource : "(null)", CUPS_LLCAST length));

 /*
  * Range check input...
  */

  if (!request || !resource)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL));

    return (HTTP_ERROR);
  }

 /*
  * Get the default connection as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (HTTP_SERVICE_UNAVAILABLE);

#ifdef HAVE_SSL
 /*
  * See if we have an auth-info attribute and are communicating over
  * a non-local link.  If so, encrypt the link so that we can pass
  * the authentication information securely...
  */

  if (ippFindAttribute(request, "auth-info", IPP_TAG_TEXT) &&
      !httpAddrLocalhost(http->hostaddr) && !http->tls &&
      httpEncryption(http, HTTP_ENCRYPT_REQUIRED))
    return (HTTP_ERROR);
#endif /* HAVE_SSL */

 /*
  * Loop until we can send the request without authorization problems.
  */

  status = HTTP_ERROR;
  expect = HTTP_CONTINUE;

  for (;;)
  {
    DEBUG_puts("cupsSendRequest: Setup...");

   /*
    * Setup the HTTP variables needed...
    */

    httpClearFields(http);
    httpSetLength(http, length);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);
    httpSetExpect(http, expect);

    DEBUG_printf(("cupsSendRequest: authstring=\"%s\"\n", http->authstring));

   /*
    * Try the request...
    */

    DEBUG_puts("cupsSendRequest: Sending HTTP POST...");

    if (httpPost(http, resource))
    {
      if (httpReconnect(http))
        return (HTTP_ERROR);
      else
        continue;
    }

   /*
    * Send the IPP data...
    */

    DEBUG_puts("cupsSendRequest: Writing IPP request...");

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

   /*
    * Wait up to 1 second to get the 100-continue response as needed...
    */

    if (!got_status && expect == HTTP_CONTINUE)
    {
      DEBUG_puts("cupsSendRequest: Waiting for 100-continue...");

      if (httpWait(http, 1000))
        status = httpUpdate(http);
      else
        status = HTTP_EXPECTATION_FAILED;
    }
    else if (httpCheck(http))
      status = httpUpdate(http);

    DEBUG_printf(("cupsSendRequest: status=%d\n", status));

   /*
    * Process the current HTTP status...
    */

    switch (status)
    {
      case HTTP_ERROR :
      case HTTP_CONTINUE :
      case HTTP_OK :
          return (status);

      case HTTP_UNAUTHORIZED :
          if (!cupsDoAuthentication(http, "POST", resource))
	    if (httpReconnect(http))
	      return (HTTP_ERROR);

          return (status);

#ifdef HAVE_SSL
      case HTTP_UPGRADE_REQUIRED :
	 /*
	  * Flush any error message, reconnect, and then upgrade with
	  * encryption...
	  */

	  if (httpReconnect(http))
	    return (HTTP_ERROR);

	  httpEncryption(http, HTTP_ENCRYPT_REQUIRED);

          return (status);
#endif /* HAVE_SSL */

      case HTTP_EXPECTATION_FAILED :
	 /*
	  * Don't try using the Expect: header the next time around...
	  */

	  expect = (http_status_t)0;
	  break;

      default :
         /*
	  * Some other error...
	  */

	  return (status);
    }
  }
}


/*
 * 'cupsWriteRequestData()' - Write additional data after an IPP request.
 *
 * This function is used after @link cupsSendRequest@ to provide a PPD and
 * after @link cupsStartDocument@ to provide a document file.
 *
 * @since CUPS 1.4@
 */

http_status_t				/* O - @code HTTP_CONTINUE@ if OK or HTTP status on error */
cupsWriteRequestData(
    http_t     *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char *buffer,			/* I - Bytes to write */
    size_t     length)			/* I - Number of bytes to write */
{
 /*
  * Get the default connection as needed...
  */

  DEBUG_printf(("cupsWriteRequestData(http=%p, buffer=%p, "
                "length=" CUPS_LLFMT ")\n", http, buffer, CUPS_LLCAST length));

  if (!http)
  {
    _cups_globals_t *cg = _cupsGlobals();
					/* Pointer to library globals */

    if ((http = cg->http) == NULL)
    {
      _cupsSetError(IPP_INTERNAL_ERROR, "No active connection");
      return (HTTP_ERROR);
    }
  }

 /*
  * Then write to the HTTP connection...
  */

  if (httpWrite2(http, buffer, length) < 0)
    return (HTTP_ERROR);

 /*
  * Finally, check if we have any pending data from the server...
  */

  if (httpCheck(http))
    return (httpUpdate(http));
  else
    return (HTTP_CONTINUE);
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

  DEBUG_printf(("_cupsSetError: last_error=%s, last_status_message=\"%s\"\n",
                ippErrorString(cg->last_error), message ? message : ""));
}


/*
 * '_cupsSetHTTPError()' - Set the last error using the HTTP status.
 */

void
_cupsSetHTTPError(http_status_t status)	/* I - HTTP status code */
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


/*
 * End of "$Id$".
 */
