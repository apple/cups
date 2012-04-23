/*
 * "$Id: getputfile.c 7359 2008-02-29 19:01:35Z mike $"
 *
 *   Get/put file functions for CUPS.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2006 by Easy Software Products.
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
 *   cupsGetFd()   - Get a file from the server.
 *   cupsGetFile() - Get a file from the server.
 *   cupsPutFd()   - Put a file on the server.
 *   cupsPutFile() - Put a file on the server.
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


/*
 * 'cupsGetFd()' - Get a file from the server.
 *
 * This function returns @code HTTP_OK@ when the file is successfully retrieved.
 *
 * @since CUPS 1.1.20/OS X 10.4@
 */

http_status_t				/* O - HTTP status */
cupsGetFd(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
	  const char *resource,		/* I - Resource name */
	  int        fd)		/* I - File descriptor */
{
  int		bytes;			/* Number of bytes read */
  char		buffer[8192];		/* Buffer for file */
  http_status_t	status;			/* HTTP status from server */
  char		if_modified_since[HTTP_MAX_VALUE];
					/* If-Modified-Since header */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsGetFd(http=%p, resource=\"%s\", fd=%d)", http,
                resource, fd));

  if (!resource || fd < 0)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_ERROR);
  }

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (HTTP_SERVICE_UNAVAILABLE);

 /*
  * Then send GET requests to the HTTP server...
  */

  strlcpy(if_modified_since, httpGetField(http, HTTP_FIELD_IF_MODIFIED_SINCE),
          sizeof(if_modified_since));

  do
  {
    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);
    httpSetField(http, HTTP_FIELD_IF_MODIFIED_SINCE, if_modified_since);

    if (httpGet(http, resource))
    {
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
	break;
      }
      else
      {
        status = HTTP_UNAUTHORIZED;
        continue;
      }
    }

    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status == HTTP_UNAUTHORIZED)
    {
     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * See if we can do authentication...
      */

      if (cupsDoAuthentication(http, "GET", resource))
      {
        status = HTTP_AUTHORIZATION_CANCELED;
        break;
      }

      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }

      continue;
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
  }
  while (status == HTTP_UNAUTHORIZED || status == HTTP_UPGRADE_REQUIRED);

 /*
  * See if we actually got the file or an error...
  */

  if (status == HTTP_OK)
  {
   /*
    * Yes, copy the file...
    */

    while ((bytes = httpRead2(http, buffer, sizeof(buffer))) > 0)
      write(fd, buffer, bytes);
  }
  else
  {
    _cupsSetHTTPError(status);
    httpFlush(http);
  }

 /*
  * Return the request status...
  */

  DEBUG_printf(("1cupsGetFd: Returning %d...", status));

  return (status);
}


/*
 * 'cupsGetFile()' - Get a file from the server.
 *
 * This function returns @code HTTP_OK@ when the file is successfully retrieved.
 *
 * @since CUPS 1.1.20/OS X 10.4@
 */

http_status_t				/* O - HTTP status */
cupsGetFile(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
	    const char *resource,	/* I - Resource name */
	    const char *filename)	/* I - Filename */
{
  int		fd;			/* File descriptor */
  http_status_t	status;			/* Status */


 /*
  * Range check input...
  */

  if (!http || !resource || !filename)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_ERROR);
  }

 /*
  * Create the file...
  */

  if ((fd = open(filename, O_WRONLY | O_EXCL | O_TRUNC)) < 0)
  {
   /*
    * Couldn't open the file!
    */

    http->error = errno;

    return (HTTP_ERROR);
  }

 /*
  * Get the file...
  */

  status = cupsGetFd(http, resource, fd);

 /*
  * If the file couldn't be gotten, then remove the file...
  */

  close(fd);

  if (status != HTTP_OK)
    unlink(filename);

 /*
  * Return the HTTP status code...
  */

  return (status);
}


/*
 * 'cupsPutFd()' - Put a file on the server.
 *
 * This function returns @code HTTP_CREATED@ when the file is stored
 * successfully.
 *
 * @since CUPS 1.1.20/OS X 10.4@
 */

http_status_t				/* O - HTTP status */
cupsPutFd(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
          const char *resource,		/* I - Resource name */
	  int        fd)		/* I - File descriptor */
{
  int		bytes,			/* Number of bytes read */
		retries;		/* Number of retries */
  char		buffer[8192];		/* Buffer for file */
  http_status_t	status;			/* HTTP status from server */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsPutFd(http=%p, resource=\"%s\", fd=%d)", http,
                resource, fd));

  if (!resource || fd < 0)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_ERROR);
  }

  if (!http)
    if ((http = _cupsConnect()) == NULL)
      return (HTTP_SERVICE_UNAVAILABLE);

 /*
  * Then send PUT requests to the HTTP server...
  */

  retries = 0;

  do
  {
    DEBUG_printf(("2cupsPutFd: starting attempt, authstring=\"%s\"...",
                  http->authstring));

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);
    httpSetField(http, HTTP_FIELD_TRANSFER_ENCODING, "chunked");
    httpSetExpect(http, HTTP_CONTINUE);

    if (httpPut(http, resource))
    {
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
	break;
      }
      else
      {
        status = HTTP_UNAUTHORIZED;
        continue;
      }
    }

   /*
    * Wait up to 1 second for a 100-continue response...
    */

    if (httpWait(http, 1000))
      status = httpUpdate(http);
    else
      status = HTTP_CONTINUE;

    if (status == HTTP_CONTINUE)
    {
     /*
      * Copy the file...
      */

      lseek(fd, 0, SEEK_SET);

      while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
	if (httpCheck(http))
	{
          if ((status = httpUpdate(http)) != HTTP_CONTINUE)
            break;
	}
	else
          httpWrite2(http, buffer, bytes);
    }

    if (status == HTTP_CONTINUE)
    {
      httpWrite2(http, buffer, 0);

      while ((status = httpUpdate(http)) == HTTP_CONTINUE);
    }

    if (status == HTTP_ERROR && !retries)
    {
      DEBUG_printf(("2cupsPutFd: retry on status %d", status));

      retries ++;

      /* Flush any error message... */
      httpFlush(http);

      /* Reconnect... */
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }

      /* Try again... */
      continue;
    }

    DEBUG_printf(("2cupsPutFd: status=%d", status));

    if (status == HTTP_UNAUTHORIZED)
    {
     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * See if we can do authentication...
      */

      if (cupsDoAuthentication(http, "PUT", resource))
      {
        status = HTTP_AUTHORIZATION_CANCELED;
        break;
      }

      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }

      continue;
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
  }
  while (status == HTTP_UNAUTHORIZED || status == HTTP_UPGRADE_REQUIRED ||
         (status == HTTP_ERROR && retries < 2));

 /*
  * See if we actually put the file or an error...
  */

  if (status != HTTP_CREATED)
  {
    _cupsSetHTTPError(status);
    httpFlush(http);
  }

  DEBUG_printf(("1cupsPutFd: Returning %d...", status));

  return (status);
}


/*
 * 'cupsPutFile()' - Put a file on the server.
 *
 * This function returns @code HTTP_CREATED@ when the file is stored
 * successfully.
 *
 * @since CUPS 1.1.20/OS X 10.4@
 */

http_status_t				/* O - HTTP status */
cupsPutFile(http_t     *http,		/* I - Connection to server or @code CUPS_HTTP_DEFAULT@ */
            const char *resource,	/* I - Resource name */
	    const char *filename)	/* I - Filename */
{
  int		fd;			/* File descriptor */
  http_status_t	status;			/* Status */


 /*
  * Range check input...
  */

  if (!http || !resource || !filename)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_ERROR);
  }

 /*
  * Open the local file...
  */

  if ((fd = open(filename, O_RDONLY)) < 0)
  {
   /*
    * Couldn't open the file!
    */

    http->error = errno;

    return (HTTP_ERROR);
  }

 /*
  * Put the file...
  */

  status = cupsPutFd(http, resource, fd);

  close(fd);

  return (status);
}


/*
 * End of "$Id: getputfile.c 7359 2008-02-29 19:01:35Z mike $".
 */
