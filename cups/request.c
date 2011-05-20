/*
 * "$Id: request.c 7946 2008-09-16 23:27:54Z mike $"
 *
 *   IPP utilities for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
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
 *   cupsLastError()        - Return the last IPP status code.
 *   cupsLastErrorString()  - Return the last IPP status-message.
 *   _cupsNextDelay()       - Return the next retry delay value.
 *   cupsReadResponseData() - Read additional data after the IPP response.
 *   cupsSendRequest()      - Send an IPP request.
 *   cupsWriteRequestData() - Write additional data after an IPP request.
 *   _cupsConnect()         - Get the default server connection...
 *   _cupsSetError()        - Set the last IPP status code and status-message.
 *   _cupsSetHTTPError()    - Set the last error using the HTTP status.
 */

/*
 * Include necessary headers...
 */

#include "cups-private.h"
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
                "filename=\"%s\")", http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource, filename));

  if (filename)
  {
    if ((infile = open(filename, O_RDONLY | O_BINARY)) < 0)
    {
     /*
      * Can't get file information!
      */

      _cupsSetError(errno == ENOENT ? IPP_NOT_FOUND : IPP_NOT_AUTHORIZED,
                    NULL, 0);

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
 * @since CUPS 1.3/Mac OS X 10.5@
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
                "infile=%d, outfile=%d)", http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource, infile, outfile));

 /*
  * Range check input...
  */

  if (!request || !resource)
  {
    ippDelete(request);

    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL), 0);

    return (NULL);
  }

 /*
  * Get the default connection as needed...
  */

  if (!http)
    if ((http = _cupsConnect()) == NULL)
    {
      ippDelete(request);

      return (NULL);
    }

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
                    NULL, 0);

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

      _cupsSetError(IPP_NOT_POSSIBLE, strerror(EISDIR), 0);

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

  DEBUG_printf(("2cupsDoIORequest: Request length=%ld, total length=%ld",
                (long)ippLength(request), (long)length));

 /*
  * Clear any "Local" authentication data since it is probably stale...
  */

  if (http->authstring && !strncmp(http->authstring, "Local ", 6))
    httpSetAuthString(http, NULL, NULL);

 /*
  * Loop until we can send the request without authorization problems.
  */

  while (response == NULL)
  {
    DEBUG_puts("2cupsDoIORequest: setup...");

   /*
    * Send the request...
    */

    status = cupsSendRequest(http, request, resource, length);

    DEBUG_printf(("2cupsDoIORequest: status=%d", status));

    if (status == HTTP_CONTINUE && request->state == IPP_DATA && infile >= 0)
    {
      DEBUG_puts("2cupsDoIORequest: file write...");

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
	  _httpUpdate(http, &status);

	  if (status >= HTTP_MULTIPLE_CHOICES)
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
    else
      httpFlush(http);

    DEBUG_printf(("2cupsDoIORequest: status=%d", status));

    if (status == HTTP_ERROR ||
        (status >= HTTP_BAD_REQUEST && status != HTTP_UNAUTHORIZED &&
	 status != HTTP_UPGRADE_REQUIRED))
    {
      _cupsSetHTTPError(status);
      break;
    }

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
  DEBUG_printf(("cupsDoRequest(http=%p, request=%p(%s), resource=\"%s\")",
                http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource));

  return (cupsDoIORequest(http, request, resource, -1, -1));
}


/*
 * 'cupsGetResponse()' - Get a response to an IPP request.
 *
 * Use this function to get the response for an IPP request sent using
 * cupsSendDocument() or cupsSendRequest(). For requests that return
 * additional data, use httpRead() after getting a successful response.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
 */

ipp_t *					/* O - Response or @code NULL@ on HTTP error */
cupsGetResponse(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                const char *resource)	/* I - HTTP resource for POST */
{
  http_status_t	status;			/* HTTP status */
  ipp_state_t	state;			/* IPP read state */
  ipp_t		*response = NULL;	/* IPP response */


  DEBUG_printf(("cupsGetResponse(http=%p, resource=\"%s\")", http, resource));

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

    DEBUG_puts("2cupsGetResponse: Finishing chunked POST...");

    if (httpWrite2(http, "", 0) < 0)
      return (NULL);
  }

 /*
  * Wait for a response from the server...
  */

  DEBUG_printf(("2cupsGetResponse: Update loop, http->status=%d...",
                http->status));

  do
  {
    status = httpUpdate(http);
  }
  while (status != HTTP_ERROR && http->state == HTTP_POST_RECV);

  DEBUG_printf(("2cupsGetResponse: status=%d", status));

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
      * Flush remaining data and delete the response...
      */

      DEBUG_puts("1cupsGetResponse: IPP read error!");

      httpFlush(http);

      ippDelete(response);
      response = NULL;

      _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
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

      DEBUG_puts("2cupsGetResponse: Need authorization...");

      if (!cupsDoAuthentication(http, "POST", resource))
        httpReconnect(http);
      else
        status = HTTP_AUTHORIZATION_CANCELED;
    }

#ifdef HAVE_SSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
     /*
      * Force a reconnect with encryption...
      */

      DEBUG_puts("2cupsGetResponse: Need encryption...");

      if (!httpReconnect(http))
        httpEncryption(http, HTTP_ENCRYPT_REQUIRED);
    }
#endif /* HAVE_SSL */
  }

  if (response)
  {
    ipp_attribute_t	*attr;		/* status-message attribute */


    attr = ippFindAttribute(response, "status-message", IPP_TAG_TEXT);

    DEBUG_printf(("1cupsGetResponse: status-code=%s, status-message=\"%s\"",
                  ippErrorString(response->request.status.status_code),
                  attr ? attr->values[0].string.text : ""));

    _cupsSetError(response->request.status.status_code,
                  attr ? attr->values[0].string.text :
		      ippErrorString(response->request.status.status_code), 0);
  }
  else if (status != HTTP_OK)
    _cupsSetHTTPError(status);

  return (response);
}


/*
 * 'cupsLastError()' - Return the last IPP status code.
 */

ipp_status_t				/* O - IPP status code from last request */
cupsLastError(void)
{
  return (_cupsGlobals()->last_error);
}


/*
 * 'cupsLastErrorString()' - Return the last IPP status-message.
 *
 * @since CUPS 1.2/Mac OS X 10.5@
 */

const char *				/* O - status-message text from last request */
cupsLastErrorString(void)
{
  return (_cupsGlobals()->last_status_message);
}


/*
 * '_cupsNextDelay()' - Return the next retry delay value.
 *
 * This function currently returns the Fibonacci sequence 1 1 2 3 5 8.
 *
 * Pass 0 for the current delay value to initialize the sequence.
 */

int					/* O  - Next delay value */
_cupsNextDelay(int current,		/* I  - Current delay value or 0 */
               int *previous)		/* IO - Previous delay value */
{
  int	next;				/* Next delay value */


  if (current > 0)
  {
    next      = (current + *previous) % 12;
    *previous = next < current ? 0 : current;
  }
  else
  {
    next      = 1;
    *previous = 0;
  }

  return (next);
}


/*
 * 'cupsReadResponseData()' - Read additional data after the IPP response.
 *
 * This function is used after cupsGetResponse() to read the PPD or document
 * files for CUPS_GET_PPD and CUPS_GET_DOCUMENT requests, respectively.
 *
 * @since CUPS 1.4/Mac OS X 10.6@
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
                "length=" CUPS_LLFMT ")", http, buffer, CUPS_LLCAST length));

  if (!http)
  {
    _cups_globals_t *cg = _cupsGlobals();
					/* Pointer to library globals */

    if ((http = cg->http) == NULL)
    {
      _cupsSetError(IPP_INTERNAL_ERROR, _("No active connection"), 1);
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
 * @since CUPS 1.4/Mac OS X 10.6@
 */

http_status_t				/* O - Initial HTTP status */
cupsSendRequest(http_t     *http,	/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
                ipp_t      *request,	/* I - IPP request */
                const char *resource,	/* I - Resource path */
		size_t     length)	/* I - Length of data to follow or @code CUPS_LENGTH_VARIABLE@ */
{
  http_status_t	status;			/* Status of HTTP request */
  int		got_status;		/* Did we get the status? */
  ipp_state_t	state;			/* State of IPP processing */
  http_status_t	expect;			/* Expect: header to use */


  DEBUG_printf(("cupsSendRequest(http=%p, request=%p(%s), resource=\"%s\", "
                "length=" CUPS_LLFMT ")", http, request,
		request ? ippOpString(request->request.op.operation_id) : "?",
		resource, CUPS_LLCAST length));

 /*
  * Range check input...
  */

  if (!request || !resource)
  {
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(EINVAL), 0);

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
  {
    _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
    return (HTTP_SERVICE_UNAVAILABLE);
  }
#endif /* HAVE_SSL */

 /*
  * Reconnect if the last response had a "Connection: close"...
  */

  if (!_cups_strcasecmp(http->fields[HTTP_FIELD_CONNECTION], "close"))
    if (httpReconnect(http))
    {
      _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
      return (HTTP_SERVICE_UNAVAILABLE);
    }

 /*
  * Loop until we can send the request without authorization problems.
  */

  expect = HTTP_CONTINUE;

  for (;;)
  {
    DEBUG_puts("2cupsSendRequest: Setup...");

   /*
    * Setup the HTTP variables needed...
    */

    httpClearFields(http);
    httpSetExpect(http, expect);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    httpSetLength(http, length);

#ifdef HAVE_GSSAPI
    if (http->authstring && !strncmp(http->authstring, "Negotiate", 9))
    {
     /*
      * Do not use cached Kerberos credentials since they will look like a
      * "replay" attack...
      */

      _cupsSetNegotiateAuthString(http, "POST", resource);
    }
#endif /* HAVE_GSSAPI */

    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);

    DEBUG_printf(("2cupsSendRequest: authstring=\"%s\"", http->authstring));

   /*
    * Try the request...
    */

    DEBUG_puts("2cupsSendRequest: Sending HTTP POST...");

    if (httpPost(http, resource))
    {
      if (httpReconnect(http))
      {
        _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
        return (HTTP_SERVICE_UNAVAILABLE);
      }
      else
        continue;
    }

   /*
    * Send the IPP data...
    */

    DEBUG_puts("2cupsSendRequest: Writing IPP request...");

    request->state = IPP_IDLE;
    status         = HTTP_CONTINUE;
    got_status     = 0;

    while ((state = ippWrite(http, request)) != IPP_DATA)
      if (state == IPP_ERROR)
	break;
      else if (httpCheck(http))
      {
        got_status = 1;

        _httpUpdate(http, &status);
	if (status >= HTTP_MULTIPLE_CHOICES)
	  break;
      }

    if (state == IPP_ERROR)
    {
      http->status = HTTP_ERROR;
      http->state  = HTTP_WAITING;

      return (HTTP_ERROR);
    }

   /*
    * Wait up to 1 second to get the 100-continue response as needed...
    */

    if (!got_status)
    {
      if (expect == HTTP_CONTINUE)
      {
	DEBUG_puts("2cupsSendRequest: Waiting for 100-continue...");

	if (httpWait(http, 1000))
	  _httpUpdate(http, &status);
      }
      else if (httpCheck(http))
	_httpUpdate(http, &status);
    }

    DEBUG_printf(("2cupsSendRequest: status=%d", status));

   /*
    * Process the current HTTP status...
    */

    if (status >= HTTP_MULTIPLE_CHOICES)
      httpFlush(http);

    switch (status)
    {
      case HTTP_ERROR :
      case HTTP_CONTINUE :
      case HTTP_OK :
          return (status);

      case HTTP_UNAUTHORIZED :
          if (cupsDoAuthentication(http, "POST", resource))
	    return (HTTP_AUTHORIZATION_CANCELED);

	  if (httpReconnect(http))
	  {
	    _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
	    return (HTTP_SERVICE_UNAVAILABLE);
	  }
	  break;

#ifdef HAVE_SSL
      case HTTP_UPGRADE_REQUIRED :
	 /*
	  * Flush any error message, reconnect, and then upgrade with
	  * encryption...
	  */

	  if (httpReconnect(http))
	  {
	    _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
	    return (HTTP_SERVICE_UNAVAILABLE);
	  }

	  if (httpEncryption(http, HTTP_ENCRYPT_REQUIRED))
	  {
	    _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
	    return (HTTP_SERVICE_UNAVAILABLE);
	  }
	  break;
#endif /* HAVE_SSL */

      case HTTP_EXPECTATION_FAILED :
	 /*
	  * Don't try using the Expect: header the next time around...
	  */

	  expect = (http_status_t)0;

	  if (httpReconnect(http))
	  {
	    _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
	    return (HTTP_SERVICE_UNAVAILABLE);
	  }
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
 * @since CUPS 1.4/Mac OS X 10.6@
 */

http_status_t				/* O - @code HTTP_CONTINUE@ if OK or HTTP status on error */
cupsWriteRequestData(
    http_t     *http,			/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
    const char *buffer,			/* I - Bytes to write */
    size_t     length)			/* I - Number of bytes to write */
{
  int	wused;				/* Previous bytes in buffer */


 /*
  * Get the default connection as needed...
  */

  DEBUG_printf(("cupsWriteRequestData(http=%p, buffer=%p, "
                "length=" CUPS_LLFMT ")", http, buffer, CUPS_LLCAST length));

  if (!http)
  {
    _cups_globals_t *cg = _cupsGlobals();
					/* Pointer to library globals */

    if ((http = cg->http) == NULL)
    {
      _cupsSetError(IPP_INTERNAL_ERROR, _("No active connection"), 1);
      DEBUG_puts("1cupsWriteRequestData: Returning HTTP_ERROR.");
      return (HTTP_ERROR);
    }
  }

 /*
  * Then write to the HTTP connection...
  */

  wused = http->wused;

  if (httpWrite2(http, buffer, length) < 0)
  {
    DEBUG_puts("1cupsWriteRequestData: Returning HTTP_ERROR.");
    _cupsSetError(IPP_INTERNAL_ERROR, strerror(http->error), 0);
    return (HTTP_ERROR);
  }

 /*
  * Finally, check if we have any pending data from the server...
  */

  if (length >= HTTP_MAX_BUFFER ||
      http->wused < wused ||
      (wused > 0 && http->wused == length))
  {
   /*
    * We've written something to the server, so check for response data...
    */

    if (_httpWait(http, 0, 1))
    {
      http_status_t	status;		/* Status from _httpUpdate */

      _httpUpdate(http, &status);
      if (status >= HTTP_MULTIPLE_CHOICES)
      {
        _cupsSetHTTPError(status);
        httpFlush(http);
      }

      DEBUG_printf(("1cupsWriteRequestData: Returning %d.\n", status));
      return (status);
    }
  }

  DEBUG_puts("1cupsWriteRequestData: Returning HTTP_CONTINUE.");
  return (HTTP_CONTINUE);
}


/*
 * '_cupsConnect()' - Get the default server connection...
 */

http_t *				/* O - HTTP connection */
_cupsConnect(void)
{
  _cups_globals_t *cg = _cupsGlobals();	/* Pointer to library globals */


 /*
  * See if we are connected to the same server...
  */

  if (cg->http)
  {
   /*
    * Compare the connection hostname, port, and encryption settings to
    * the cached defaults; these were initialized the first time we
    * connected...
    */

    if (strcmp(cg->http->hostname, cg->server) ||
        cg->ipp_port != _httpAddrPort(cg->http->hostaddr) ||
        (cg->http->encryption != cg->encryption &&
	 cg->http->encryption == HTTP_ENCRYPT_NEVER))
    {
     /*
      * Need to close the current connection because something has changed...
      */

      httpClose(cg->http);
      cg->http = NULL;
    }
  }

 /*
  * (Re)connect as needed...
  */

  if (!cg->http)
  {
    if ((cg->http = httpConnectEncrypt(cupsServer(), ippPort(),
                                       cupsEncryption())) == NULL)
    {
      if (errno)
        _cupsSetError(IPP_SERVICE_UNAVAILABLE, NULL, 0);
      else
        _cupsSetError(IPP_SERVICE_UNAVAILABLE,
	              _("Unable to connect to host."), 1);
    }
  }

 /*
  * Return the cached connection...
  */

  return (cg->http);
}


/*
 * '_cupsSetError()' - Set the last IPP status code and status-message.
 */

void
_cupsSetError(ipp_status_t status,	/* I - IPP status code */
              const char   *message,	/* I - status-message value */
	      int          localize)	/* I - Localize the message? */
{
  _cups_globals_t	*cg;		/* Global data */


  if (!message && errno)
  {
    message  = strerror(errno);
    localize = 0;
  }

  cg             = _cupsGlobals();
  cg->last_error = status;

  if (cg->last_status_message)
  {
    _cupsStrFree(cg->last_status_message);

    cg->last_status_message = NULL;
  }

  if (message)
  {
    if (localize)
    {
     /*
      * Get the message catalog...
      */

      if (!cg->lang_default)
	cg->lang_default = cupsLangDefault();

      cg->last_status_message = _cupsStrAlloc(_cupsLangString(cg->lang_default,
                                                              message));
    }
    else
      cg->last_status_message = _cupsStrAlloc(message);
  }

  DEBUG_printf(("4_cupsSetError: last_error=%s, last_status_message=\"%s\"",
                ippErrorString(cg->last_error), cg->last_status_message));
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
	_cupsSetError(IPP_NOT_FOUND, httpStatus(status), 0);
	break;

    case HTTP_UNAUTHORIZED :
	_cupsSetError(IPP_NOT_AUTHENTICATED, httpStatus(status), 0);
	break;

    case HTTP_AUTHORIZATION_CANCELED :
	_cupsSetError(IPP_AUTHENTICATION_CANCELED, httpStatus(status), 0);
	break;

    case HTTP_FORBIDDEN :
	_cupsSetError(IPP_FORBIDDEN, httpStatus(status), 0);
	break;

    case HTTP_BAD_REQUEST :
	_cupsSetError(IPP_BAD_REQUEST, httpStatus(status), 0);
	break;

    case HTTP_REQUEST_TOO_LARGE :
	_cupsSetError(IPP_REQUEST_VALUE, httpStatus(status), 0);
	break;

    case HTTP_NOT_IMPLEMENTED :
	_cupsSetError(IPP_OPERATION_NOT_SUPPORTED, httpStatus(status), 0);
	break;

    case HTTP_NOT_SUPPORTED :
	_cupsSetError(IPP_VERSION_NOT_SUPPORTED, httpStatus(status), 0);
	break;

    case HTTP_UPGRADE_REQUIRED :
	_cupsSetError(IPP_UPGRADE_REQUIRED, httpStatus(status), 0);
        break;

    case HTTP_PKI_ERROR :
	_cupsSetError(IPP_PKI_ERROR, httpStatus(status), 0);
        break;

    case HTTP_ERROR :
	_cupsSetError(IPP_INTERNAL_ERROR, httpStatus(status), 0);
        break;

    default :
	DEBUG_printf(("4_cupsSetHTTPError: HTTP error %d mapped to "
	              "IPP_SERVICE_UNAVAILABLE!", status));
	_cupsSetError(IPP_SERVICE_UNAVAILABLE, httpStatus(status), 0);
	break;
  }
}


/*
 * End of "$Id: request.c 7946 2008-09-16 23:27:54Z mike $".
 */
