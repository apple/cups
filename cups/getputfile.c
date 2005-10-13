/*
 * "$Id$"
 *
 *   Get/put file functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *   cupsGetFd()   - Get a file from the server.
 *   cupsGetFile() - Get a file from the server.
 *   cupsPutFd()   - Put a file on the server.
 *   cupsPutFile() - Put a file on the server.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "ipp.h"
#include "language.h"
#include "string.h"
#include "debug.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
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
 * @since CUPS 1.1.20@
 */

http_status_t				/* O - Status */
cupsGetFd(http_t     *http,		/* I - HTTP connection to server */
	  const char *resource,		/* I - Resource name */
	  int        fd)		/* I - File descriptor */
{
  int		bytes;			/* Number of bytes read */
  char		buffer[8192];		/* Buffer for file */
  http_status_t	status;			/* HTTP status from server */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsGetFd(http=%p, resource=\"%s\", fd=%d)\n", http,
                resource, fd));

  if (!http || !resource || fd < 0)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_ERROR);
  }

 /*
  * Then send GET requests to the HTTP server...
  */

  do
  {
    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);

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
        break;

      httpReconnect(http);

      continue;
    }
#ifdef HAVE_SSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush(http);

      /* Reconnect... */
      httpReconnect(http);

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

    while ((bytes = httpRead(http, buffer, sizeof(buffer))) > 0)
      write(fd, buffer, bytes);
  }
  else
    httpFlush(http);

 /*
  * Return the request status...
  */

  return (status);
}


/*
 * 'cupsGetFile()' - Get a file from the server.
 *
 * @since CUPS 1.1.20@
 */

http_status_t				/* O - Status */
cupsGetFile(http_t     *http,		/* I - HTTP connection to server */
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
 * @since CUPS 1.1.20@
 */

http_status_t				/* O - Status */
cupsPutFd(http_t     *http,		/* I - HTTP connection to server */
          const char *resource,		/* I - Resource name */
	  int        fd)		/* I - File descriptor */
{
  int		bytes;			/* Number of bytes read */
  char		buffer[8192];		/* Buffer for file */
  http_status_t	status;			/* HTTP status from server */


 /*
  * Range check input...
  */

  DEBUG_printf(("cupsPutFd(http=%p, resource=\"%s\", fd=%d)\n", http,
                resource, fd));

  if (!http || !resource || fd < 0)
  {
    if (http)
      http->error = EINVAL;

    return (HTTP_ERROR);
  }

 /*
  * Then send PUT requests to the HTTP server...
  */

  do
  {
    DEBUG_printf(("cupsPutFd: starting attempt, authstring=\"%s\"...\n",
                  http->authstring));

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, http->authstring);
    httpSetField(http, HTTP_FIELD_TRANSFER_ENCODING, "chunked");

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
    * Copy the file...
    */

    lseek(fd, 0, SEEK_SET);

    status = HTTP_CONTINUE;

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
      if (httpCheck(http))
      {
        if ((status = httpUpdate(http)) != HTTP_CONTINUE)
          break;
      }
      else
        httpWrite(http, buffer, bytes);

    if (status == HTTP_CONTINUE)
    {
      httpWrite(http, buffer, 0);

      while ((status = httpUpdate(http)) == HTTP_CONTINUE);
    }

    DEBUG_printf(("cupsPutFd: status=%d\n", status));

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
        break;

      httpReconnect(http);

      continue;
    }
#ifdef HAVE_SSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush(http);

      /* Reconnect... */
      httpReconnect(http);

      /* Upgrade with encryption... */
      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);

      /* Try again, this time with encryption enabled... */
      continue;
    }
#endif /* HAVE_SSL */
  }
  while (status == HTTP_UNAUTHORIZED || status == HTTP_UPGRADE_REQUIRED);

 /*
  * See if we actually put the file or an error...
  */

  if (status != HTTP_CREATED)
    httpFlush(http);

  return (status);
}


/*
 * 'cupsPutFile()' - Put a file on the server.
 *
 * @since CUPS 1.1.20@
 */

http_status_t				/* O - Status */
cupsPutFile(http_t     *http,		/* I - HTTP connection to server */
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
 * End of "$Id$".
 */
