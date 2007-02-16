/*
 * "$Id$"
 *
 *   HTTP routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   _httpBIOMethods()    - Get the OpenSSL BIO methods for HTTP connections.
 *   httpBlocking()       - Set blocking/non-blocking behavior on a connection.
 *   httpCheck()          - Check to see if there is a pending response from
 *                          the server.
 *   httpClearCookie()    - Clear the cookie value(s).
 *   httpClearFields()    - Clear HTTP request fields.
 *   httpClose()          - Close an HTTP connection...
 *   httpConnect()        - Connect to a HTTP server.
 *   httpConnectEncrypt() - Connect to a HTTP server using encryption.
 *   httpDelete()         - Send a DELETE request to the server.
 *   httpEncryption()     - Set the required encryption on the link.
 *   httpError()          - Get the last error on a connection.
 *   httpFlush()          - Flush data from a HTTP connection.
 *   httpFlushWrite()     - Flush data in write buffer.
 *   httpGet()            - Send a GET request to the server.
 *   httpGetBlocking()    - Get the blocking/non-block state of a connection.
 *   httpGetCookie()      - Get any cookie data from the response.
 *   httpGetFd()          - Get the file descriptor associated with a
 *                          connection.
 *   httpGetField()       - Get a field value from a request/response.
 *   httpGetLength()      - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   httpGetLength2()     - Get the amount of data remaining from the
 *                          content-length or transfer-encoding fields.
 *   httpGetStatus()      - Get the status of the last HTTP request.
 *   httpGetSubField()    - Get a sub-field value.
 *   httpGets()           - Get a line of text from a HTTP connection.
 *   httpHead()           - Send a HEAD request to the server.
 *   httpInitialize()     - Initialize the HTTP interface library and set the
 *                          default HTTP proxy (if any).
 *   httpOptions()        - Send an OPTIONS request to the server.
 *   httpPost()           - Send a POST request to the server.
 *   httpPrintf()         - Print a formatted string to a HTTP connection.
 *   httpPut()            - Send a PUT request to the server.
 *   httpRead()           - Read data from a HTTP connection.
 *   httpRead2()          - Read data from a HTTP connection.
 *   _httpReadCDSA()      - Read function for the CDSA library.
 *   _httpReadGNUTLS()    - Read function for the GNU TLS library.
 *   httpReconnect()      - Reconnect to a HTTP server...
 *   httpSetCookie()      - Set the cookie value(s)...
 *   httpSetExpect()      - Set the Expect: header in a request.
 *   httpSetField()       - Set the value of an HTTP header.
 *   httpSetLength()      - Set the content-length and transfer-encoding.
 *   httpTrace()          - Send an TRACE request to the server.
 *   httpUpdate()         - Update the current HTTP state for incoming data.
 *   httpWait()           - Wait for data available on a connection.
 *   httpWrite()          - Write data to a HTTP connection.
 *   httpWrite2()         - Write data to a HTTP connection.
 *   _httpWriteCDSA()     - Write function for the CDSA library.
 *   _httpWriteGNUTLS()   - Write function for the GNU TLS library.
 *   http_bio_ctrl()      - Control the HTTP connection.
 *   http_bio_free()      - Free OpenSSL data.
 *   http_bio_new()       - Initialize an OpenSSL BIO structure.
 *   http_bio_puts()      - Send a string for OpenSSL.
 *   http_bio_read()      - Read data for OpenSSL.
 *   http_bio_write()     - Write data for OpenSSL.
 *   http_field()         - Return the field index for a field name.
 *   http_read_ssl()      - Read from a SSL/TLS connection.
 *   http_send()          - Send a request with all fields and the trailing
 *                          blank line.
 *   http_setup_ssl()     - Set up SSL/TLS on a connection.
 *   http_shutdown_ssl()  - Shut down SSL/TLS on a connection.
 *   http_upgrade()       - Force upgrade to TLS encryption.
 *   http_wait()          - Wait for data available on a connection.
 *   http_write()         - Write data to a connection.
 *   http_write_ssl()     - Write to a SSL/TLS connection.
 */

/*
 * Include necessary headers...
 */

#include "http-private.h"
#include "globals.h"
#include "debug.h"
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#ifndef WIN32
#  include <signal.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif /* !WIN32 */
#ifdef HAVE_POLL
#  include <sys/poll.h>
#endif /* HAVE_POLL */


/*
 * Some operating systems have done away with the Fxxxx constants for
 * the fcntl() call; this works around that "feature"...
 */

#ifndef FNONBLK
#  define FNONBLK O_NONBLOCK
#endif /* !FNONBLK */


/*
 * Local functions...
 */

static http_field_t	http_field(const char *name);
static int		http_send(http_t *http, http_state_t request,
			          const char *uri);
static int		http_wait(http_t *http, int msec, int usessl);
static int		http_write(http_t *http, const char *buffer,
			           int length);
static int		http_write_chunk(http_t *http, const char *buffer,
			                 int length);
#ifdef HAVE_SSL
static int		http_read_ssl(http_t *http, char *buf, int len);
static int		http_setup_ssl(http_t *http);
static void		http_shutdown_ssl(http_t *http);
static int		http_upgrade(http_t *http);
static int		http_write_ssl(http_t *http, const char *buf, int len);
#endif /* HAVE_SSL */


/*
 * Local globals...
 */

static const char * const http_fields[] =
			{
			  "Accept-Language",
			  "Accept-Ranges",
			  "Authorization",
			  "Connection",
			  "Content-Encoding",
			  "Content-Language",
			  "Content-Length",
			  "Content-Location",
			  "Content-MD5",
			  "Content-Range",
			  "Content-Type",
			  "Content-Version",
			  "Date",
			  "Host",
			  "If-Modified-Since",
			  "If-Unmodified-since",
			  "Keep-Alive",
			  "Last-Modified",
			  "Link",
			  "Location",
			  "Range",
			  "Referer",
			  "Retry-After",
			  "Transfer-Encoding",
			  "Upgrade",
			  "User-Agent",
			  "WWW-Authenticate"
			};


#if defined(HAVE_SSL) && defined(HAVE_LIBSSL)
/*
 * BIO methods for OpenSSL...
 */

static int		http_bio_write(BIO *h, const char *buf, int num);
static int		http_bio_read(BIO *h, char *buf, int size);
static int		http_bio_puts(BIO *h, const char *str);
static long		http_bio_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int		http_bio_new(BIO *h);
static int		http_bio_free(BIO *data);

static BIO_METHOD	http_bio_methods =
			{
			  BIO_TYPE_SOCKET,
			  "http",
			  http_bio_write,
			  http_bio_read,
			  http_bio_puts,
			  NULL, /* http_bio_gets, */
			  http_bio_ctrl,
			  http_bio_new,
			  http_bio_free,
			  NULL,
			};


/*
 * '_httpBIOMethods()' - Get the OpenSSL BIO methods for HTTP connections.
 */

BIO_METHOD *				/* O - BIO methods for OpenSSL */
_httpBIOMethods(void)
{
  return (&http_bio_methods);
}
#endif /* HAVE_SSL && HAVE_LIBSSL */


/*
 * 'httpBlocking()' - Set blocking/non-blocking behavior on a connection.
 */

void
httpBlocking(http_t *http,		/* I - HTTP connection */
             int    b)			/* I - 1 = blocking, 0 = non-blocking */
{
  if (http)
    http->blocking = b;
}


/*
 * 'httpCheck()' - Check to see if there is a pending response from the server.
 */

int					/* O - 0 = no data, 1 = data available */
httpCheck(http_t *http)			/* I - HTTP connection */
{
  return (httpWait(http, 0));
}


/*
 * 'httpClearCookie()' - Clear the cookie value(s).
 *
 * @since CUPS 1.1.19@
 */

void
httpClearCookie(http_t *http)		/* I - HTTP connection */
{
  if (!http)
    return;

  if (http->cookie)
  {
    free(http->cookie);
    http->cookie = NULL;
  }
}


/*
 * 'httpClearFields()' - Clear HTTP request fields.
 */

void
httpClearFields(http_t *http)		/* I - HTTP connection */
{
  if (http)
  {
    memset(http->fields, 0, sizeof(http->fields));
    if (http->hostname[0] == '/')
      httpSetField(http, HTTP_FIELD_HOST, "localhost");
    else
      httpSetField(http, HTTP_FIELD_HOST, http->hostname);

    if (http->field_authorization)
    {
      free(http->field_authorization);
      http->field_authorization = NULL;
    }

    http->expect = (http_status_t)0;
  }
}


/*
 * 'httpClose()' - Close an HTTP connection...
 */

void
httpClose(http_t *http)			/* I - HTTP connection */
{
#ifdef HAVE_GSSAPI
  OM_uint32	minor_status,		/* Minor status code */
		major_status;		/* Major status code */
#endif /* HAVE_GSSAPI */


  DEBUG_printf(("httpClose(http=%p)\n", http));

  if (!http)
    return;

  httpAddrFreeList(http->addrlist);

  if (http->cookie)
    free(http->cookie);

#ifdef HAVE_SSL
  if (http->tls)
    http_shutdown_ssl(http);
#endif /* HAVE_SSL */

#ifdef WIN32
  closesocket(http->fd);
#else
  close(http->fd);
#endif /* WIN32 */

#ifdef HAVE_GSSAPI
  if (http->gssctx != GSS_C_NO_CONTEXT)
    major_status = gss_delete_sec_context(&minor_status, &http->gssctx,
                                          GSS_C_NO_BUFFER);

  if (http->gssname != GSS_C_NO_NAME)
    major_status = gss_release_name(&minor_status, &http->gssname);
#endif /* HAVE_GSSAPI */

  httpClearFields(http);

  if (http->authstring && http->authstring != http->_authstring)
    free(http->authstring);

  free(http);
}


/*
 * 'httpConnect()' - Connect to a HTTP server.
 */

http_t *				/* O - New HTTP connection */
httpConnect(const char *host,		/* I - Host to connect to */
            int        port)		/* I - Port number */
{
  http_encryption_t	encryption;	/* Type of encryption to use */


 /*
  * Set the default encryption status...
  */

  if (port == 443)
    encryption = HTTP_ENCRYPT_ALWAYS;
  else
    encryption = HTTP_ENCRYPT_IF_REQUESTED;

  return (httpConnectEncrypt(host, port, encryption));
}


/*
 * 'httpConnectEncrypt()' - Connect to a HTTP server using encryption.
 */

http_t *				/* O - New HTTP connection */
httpConnectEncrypt(
    const char        *host,		/* I - Host to connect to */
    int               port,		/* I - Port number */
    http_encryption_t encryption)	/* I - Type of encryption to use */
{
  http_t		*http;		/* New HTTP connection */
  http_addrlist_t	*addrlist;	/* Host address data */
  char			service[255];	/* Service name */


  DEBUG_printf(("httpConnectEncrypt(host=\"%s\", port=%d, encryption=%d)\n",
                host ? host : "(null)", port, encryption));

  if (!host)
    return (NULL);

  httpInitialize();

 /*
  * Lookup the host...
  */

  sprintf(service, "%d", port);

  if ((addrlist = httpAddrGetList(host, AF_UNSPEC, service)) == NULL)
    return (NULL);

 /*
  * Allocate memory for the structure...
  */

  http = calloc(sizeof(http_t), 1);
  if (http == NULL)
    return (NULL);

  http->version  = HTTP_1_1;
  http->blocking = 1;
  http->activity = time(NULL);
  http->fd       = -1;

#ifdef HAVE_GSSAPI
  http->gssctx  = GSS_C_NO_CONTEXT;
  http->gssname = GSS_C_NO_NAME;
#endif /* HAVE_GSSAPI */

 /*
  * Set the encryption status...
  */

  if (port == 443)			/* Always use encryption for https */
    http->encryption = HTTP_ENCRYPT_ALWAYS;
  else
    http->encryption = encryption;

 /*
  * Loop through the addresses we have until one of them connects...
  */

  strlcpy(http->hostname, host, sizeof(http->hostname));

 /*
  * Connect to the remote system...
  */

  http->addrlist = addrlist;

  if (!httpReconnect(http))
    return (http);

 /*
  * Could not connect to any known address - bail out!
  */

  httpAddrFreeList(addrlist);

  free(http);

  return (NULL);
}


/*
 * 'httpDelete()' - Send a DELETE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpDelete(http_t     *http,		/* I - HTTP connection */
           const char *uri)		/* I - URI to delete */
{
  return (http_send(http, HTTP_DELETE, uri));
}


/*
 * 'httpEncryption()' - Set the required encryption on the link.
 */

int					/* O - -1 on error, 0 on success */
httpEncryption(http_t            *http,	/* I - HTTP connection */
               http_encryption_t e)	/* I - New encryption preference */
{
  DEBUG_printf(("httpEncryption(http=%p, e=%d)\n", http, e));

#ifdef HAVE_SSL
  if (!http)
    return (0);

  http->encryption = e;

  if ((http->encryption == HTTP_ENCRYPT_ALWAYS && !http->tls) ||
      (http->encryption == HTTP_ENCRYPT_NEVER && http->tls))
    return (httpReconnect(http));
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
    return (http_upgrade(http));
  else
    return (0);
#else
  if (e == HTTP_ENCRYPT_ALWAYS || e == HTTP_ENCRYPT_REQUIRED)
    return (-1);
  else
    return (0);
#endif /* HAVE_SSL */
}


/*
 * 'httpError()' - Get the last error on a connection.
 */

int					/* O - Error code (errno) value */
httpError(http_t *http)			/* I - HTTP connection */
{
  if (http)
    return (http->error);
  else
    return (EINVAL);
}


/*
 * 'httpFlush()' - Flush data from a HTTP connection.
 */

void
httpFlush(http_t *http)			/* I - HTTP connection */
{
  char	buffer[8192];			/* Junk buffer */
  int	blocking;			/* To block or not to block */


  DEBUG_printf(("httpFlush(http=%p), state=%d\n", http, http->state));

 /*
  * Temporarily set non-blocking mode so we don't get stuck in httpRead()...
  */

  blocking = http->blocking;
  http->blocking = 0;

 /*
  * Read any data we can...
  */

  while (httpRead2(http, buffer, sizeof(buffer)) > 0);

 /*
  * Restore blocking and reset the connection if we didn't get all of
  * the remaining data...
  */

  http->blocking = blocking;

  if (http->state != HTTP_WAITING && http->fd >= 0)
  {
   /*
    * Didn't get the data back, so close the current connection.
    */

    http->state = HTTP_WAITING;

#ifdef HAVE_SSL
    if (http->tls)
      http_shutdown_ssl(http);
#endif /* HAVE_SSL */

#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

    http->fd = -1;
  }
}


/*
 * 'httpFlushWrite()' - Flush data in write buffer.
 *
 * @since CUPS 1.2@
 */

int					/* O - Bytes written or -1 on error */
httpFlushWrite(http_t *http)		/* I - HTTP connection */
{
  int	bytes;				/* Bytes written */


  DEBUG_printf(("httpFlushWrite(http=%p)\n", http));

  if (!http || !http->wused)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    bytes = http_write_chunk(http, http->wbuffer, http->wused);
  else
    bytes = http_write(http, http->wbuffer, http->wused);

  http->wused = 0;

  return (bytes);
}


/*
 * 'httpGet()' - Send a GET request to the server.
 */

int					/* O - Status of call (0 = success) */
httpGet(http_t     *http,		/* I - HTTP connection */
        const char *uri)		/* I - URI to get */
{
  return (http_send(http, HTTP_GET, uri));
}


/*
 * 'httpGetBlocking()' - Get the blocking/non-block state of a connection.
 *
 * @since CUPS 1.2@
 */

int					/* O - 1 if blocking, 0 if non-blocking */
httpGetBlocking(http_t *http)		/* I - HTTP connection */
{
  return (http ? http->blocking : 0);
}


/*
 * 'httpGetCookie()' - Get any cookie data from the response.
 *
 * @since CUPS 1.1.19@
 */

const char *				/* O - Cookie data or NULL */
httpGetCookie(http_t *http)		/* I - HTTP connecion */
{
  return (http ? http->cookie : NULL);
}


/*
 * 'httpGetFd()' - Get the file descriptor associated with a connection.
 *
 * @since CUPS 1.2@
 */

int					/* O - File descriptor or -1 if none */
httpGetFd(http_t *http)			/* I - HTTP connection */
{
  return (http ? http->fd : -1);
}


/*
 * 'httpGetField()' - Get a field value from a request/response.
 */

const char *				/* O - Field value */
httpGetField(http_t       *http,	/* I - HTTP connection */
             http_field_t field)	/* I - Field to get */
{
  if (!http || field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX)
    return (NULL);
  else if (field == HTTP_FIELD_AUTHORIZATION && 
	   http->field_authorization)
  {
   /*
    * Special case for WWW-Authenticate: as its contents can be
    * longer than HTTP_MAX_VALUE...
    */

    return (http->field_authorization);
  }
  else
    return (http->fields[field]);
}


/*
 * 'httpGetLength()' - Get the amount of data remaining from the
 *                     content-length or transfer-encoding fields.
 *
 * This function is deprecated and will not return lengths larger than
 * 2^31 - 1; use httpGetLength2() instead.
 *
 * @deprecated@
 */

int					/* O - Content length */
httpGetLength(http_t *http)		/* I - HTTP connection */
{
 /*
  * Get the read content length and return the 32-bit value.
  */

  if (http)
  {
    httpGetLength2(http);

    return (http->_data_remaining);
  }
  else
    return (-1);
}


/*
 * 'httpGetLength2()' - Get the amount of data remaining from the
 *                      content-length or transfer-encoding fields.
 *
 * This function returns the complete content length, even for
 * content larger than 2^31 - 1.
 *
 * @since CUPS 1.2@
 */

off_t					/* O - Content length */
httpGetLength2(http_t *http)		/* I - HTTP connection */
{
  DEBUG_printf(("httpGetLength2(http=%p), state=%d\n", http, http->state));

  if (!http)
    return (-1);

  if (!strcasecmp(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked"))
  {
    DEBUG_puts("httpGetLength2: chunked request!");

    http->data_encoding  = HTTP_ENCODE_CHUNKED;
    http->data_remaining = 0;
  }
  else
  {
    http->data_encoding = HTTP_ENCODE_LENGTH;

   /*
    * The following is a hack for HTTP servers that don't send a
    * content-length or transfer-encoding field...
    *
    * If there is no content-length then the connection must close
    * after the transfer is complete...
    */

    if (!http->fields[HTTP_FIELD_CONTENT_LENGTH][0])
    {
     /*
      * Default content length is 0 for errors and 2^31-1 for other
      * successful requests...
      */

      if (http->status >= HTTP_MULTIPLE_CHOICES)
        http->data_remaining = 0;
      else
        http->data_remaining = 2147483647;
    }
    else
      http->data_remaining = strtoll(http->fields[HTTP_FIELD_CONTENT_LENGTH],
                                     NULL, 10);

    DEBUG_printf(("httpGetLength2: content_length=" CUPS_LLFMT "\n",
                  CUPS_LLCAST http->data_remaining));
  }

  if (http->data_remaining <= INT_MAX)
    http->_data_remaining = (int)http->data_remaining;
  else
    http->_data_remaining = INT_MAX;

  return (http->data_remaining);
}


/*
 * 'httpGetStatus()' - Get the status of the last HTTP request.
 *
 * @since CUPS 1.2@
 */

http_status_t				/* O - HTTP status */
httpGetStatus(http_t *http)		/* I - HTTP connection */
{
  return (http ? http->status : HTTP_ERROR);
}


/*
 * 'httpGetSubField()' - Get a sub-field value.
 *
 * @deprecated@
 */

char *					/* O - Value or NULL */
httpGetSubField(http_t       *http,	/* I - HTTP connection */
                http_field_t field,	/* I - Field index */
                const char   *name,	/* I - Name of sub-field */
		char         *value)	/* O - Value string */
{
  return (httpGetSubField2(http, field, name, value, HTTP_MAX_VALUE));
}


/*
 * 'httpGetSubField2()' - Get a sub-field value.
 *
 * @since CUPS 1.2@
 */

char *					/* O - Value or NULL */
httpGetSubField2(http_t       *http,	/* I - HTTP connection */
                 http_field_t field,	/* I - Field index */
                 const char   *name,	/* I - Name of sub-field */
		 char         *value,	/* O - Value string */
		 int          valuelen)	/* I - Size of value buffer */
{
  const char	*fptr;			/* Pointer into field */
  char		temp[HTTP_MAX_VALUE],	/* Temporary buffer for name */
		*ptr,			/* Pointer into string buffer */
		*end;			/* End of value buffer */

  DEBUG_printf(("httpGetSubField2(http=%p, field=%d, name=\"%s\", value=%p, valuelen=%d)\n",
                http, field, name, value, valuelen));

  if (!http || !name || !value || valuelen < 2 ||
      field <= HTTP_FIELD_UNKNOWN || field >= HTTP_FIELD_MAX)
    return (NULL);

  end = value + valuelen - 1;

  for (fptr = http->fields[field]; *fptr;)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*fptr & 255))
      fptr ++;

    if (*fptr == ',')
    {
      fptr ++;
      continue;
    }

   /*
    * Get the sub-field name...
    */

    for (ptr = temp;
         *fptr && *fptr != '=' && !isspace(*fptr & 255) &&
	     ptr < (temp + sizeof(temp) - 1);
         *ptr++ = *fptr++);

    *ptr = '\0';

    DEBUG_printf(("httpGetSubField: name=\"%s\"\n", temp));

   /*
    * Skip trailing chars up to the '='...
    */

    while (isspace(*fptr & 255))
      fptr ++;

    if (!*fptr)
      break;

    if (*fptr != '=')
      continue;

   /*
    * Skip = and leading whitespace...
    */

    fptr ++;

    while (isspace(*fptr & 255))
      fptr ++;

    if (*fptr == '\"')
    {
     /*
      * Read quoted string...
      */

      for (ptr = value, fptr ++;
           *fptr && *fptr != '\"' && ptr < end;
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && *fptr != '\"')
        fptr ++;

      if (*fptr)
        fptr ++;
    }
    else
    {
     /*
      * Read unquoted string...
      */

      for (ptr = value;
           *fptr && !isspace(*fptr & 255) && *fptr != ',' && ptr < end;
	   *ptr++ = *fptr++);

      *ptr = '\0';

      while (*fptr && !isspace(*fptr & 255) && *fptr != ',')
        fptr ++;
    }

    DEBUG_printf(("httpGetSubField: value=\"%s\"\n", value));

   /*
    * See if this is the one...
    */

    if (!strcmp(name, temp))
      return (value);
  }

  value[0] = '\0';

  return (NULL);
}


/*
 * 'httpGets()' - Get a line of text from a HTTP connection.
 */

char *					/* O - Line or NULL */
httpGets(char   *line,			/* I - Line to read into */
         int    length,			/* I - Max length of buffer */
	 http_t *http)			/* I - HTTP connection */
{
  char	*lineptr,			/* Pointer into line */
	*lineend,			/* End of line */
	*bufptr,			/* Pointer into input buffer */
	*bufend;			/* Pointer to end of buffer */
  int	bytes,				/* Number of bytes read */
	eol;				/* End-of-line? */


  DEBUG_printf(("httpGets(line=%p, length=%d, http=%p)\n", line, length, http));

  if (http == NULL || line == NULL)
    return (NULL);

 /*
  * Read a line from the buffer...
  */
    
  lineptr = line;
  lineend = line + length - 1;
  eol     = 0;

  while (lineptr < lineend)
  {
   /*
    * Pre-load the buffer as needed...
    */

#ifdef WIN32
    WSASetLastError(0);
#else
    errno = 0;
#endif /* WIN32 */

    while (http->used == 0)
    {
     /*
      * No newline; see if there is more data to be read...
      */

      if (!http->blocking && !http_wait(http, 10000, 1))
      {
        DEBUG_puts("httpGets: Timed out!");
#ifdef WIN32
        http->error = WSAETIMEDOUT;
#else
        http->error = ETIMEDOUT;
#endif /* WIN32 */
        return (NULL);
      }

#ifdef HAVE_SSL
      if (http->tls)
	bytes = http_read_ssl(http, http->buffer + http->used,
	                      HTTP_MAX_BUFFER - http->used);
      else
#endif /* HAVE_SSL */
        bytes = recv(http->fd, http->buffer + http->used,
	             HTTP_MAX_BUFFER - http->used, 0);

      DEBUG_printf(("httpGets: read %d bytes...\n", bytes));

      if (bytes < 0)
      {
       /*
	* Nope, can't get a line this time...
	*/

#ifdef WIN32
        if (WSAGetLastError() != http->error)
	{
	  http->error = WSAGetLastError();
	  continue;
	}

        DEBUG_printf(("httpGets: recv() error %d!\n", WSAGetLastError()));
#else
        DEBUG_printf(("httpGets: recv() error %d!\n", errno));

        if (errno == EINTR)
	  continue;
	else if (errno != http->error)
	{
	  http->error = errno;
	  continue;
	}
#endif /* WIN32 */

        return (NULL);
      }
      else if (bytes == 0)
      {
	http->error = EPIPE;

        return (NULL);
      }

     /*
      * Yup, update the amount used...
      */

      http->used += bytes;
    }

   /*
    * Now copy as much of the current line as possible...
    */

    for (bufptr = http->buffer, bufend = http->buffer + http->used;
         lineptr < lineend && bufptr < bufend;)
    {
      if (*bufptr == 0x0a)
      {
        eol = 1;
	bufptr ++;
	break;
      }
      else if (*bufptr == 0x0d)
	bufptr ++;
      else
	*lineptr++ = *bufptr++;
    }

    http->used -= (int)(bufptr - http->buffer);
    if (http->used > 0)
      memmove(http->buffer, bufptr, http->used);

    if (eol)
    {
     /*
      * End of line...
      */

      http->activity = time(NULL);

      *lineptr = '\0';
      
      DEBUG_printf(("httpGets: Returning \"%s\"\n", line));

      return (line);
    }
  }

  DEBUG_puts("httpGets: No new line available!");

  return (NULL);
}


/*
 * 'httpHead()' - Send a HEAD request to the server.
 */

int					/* O - Status of call (0 = success) */
httpHead(http_t     *http,		/* I - HTTP connection */
         const char *uri)		/* I - URI for head */
{
  return (http_send(http, HTTP_HEAD, uri));
}


/*
 * 'httpInitialize()' - Initialize the HTTP interface library and set the
 *                      default HTTP proxy (if any).
 */

void
httpInitialize(void)
{
#ifdef HAVE_LIBSSL
#  ifndef WIN32
  struct timeval	curtime;	/* Current time in microseconds */
#  endif /* !WIN32 */
  int			i;		/* Looping var */
  unsigned char		data[1024];	/* Seed data */
#endif /* HAVE_LIBSSL */

#ifdef WIN32
  WSADATA	winsockdata;		/* WinSock data */
  static int	initialized = 0;	/* Has WinSock been initialized? */


  if (!initialized)
    WSAStartup(MAKEWORD(1,1), &winsockdata);
#elif !defined(SO_NOSIGPIPE)
 /*
  * Ignore SIGPIPE signals...
  */

#  ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);
#  elif defined(HAVE_SIGACTION)
  struct sigaction	action;		/* POSIX sigaction data */


  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#  else
  signal(SIGPIPE, SIG_IGN);
#  endif /* !SO_NOSIGPIPE */
#endif /* WIN32 */

#ifdef HAVE_GNUTLS
  gnutls_global_init();
#endif /* HAVE_GNUTLS */

#ifdef HAVE_LIBSSL
  SSL_load_error_strings();
  SSL_library_init();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

#ifdef WIN32
#else
  gettimeofday(&curtime, NULL);
  srand(curtime.tv_sec + curtime.tv_usec);
#endif /* WIN32 */

  for (i = 0; i < sizeof(data); i ++)
    data[i] = rand(); /* Yes, this is a poor source of random data... */

  RAND_seed(&data, sizeof(data));
#endif /* HAVE_LIBSSL */
}


/*
 * 'httpOptions()' - Send an OPTIONS request to the server.
 */

int					/* O - Status of call (0 = success) */
httpOptions(http_t     *http,		/* I - HTTP connection */
            const char *uri)		/* I - URI for options */
{
  return (http_send(http, HTTP_OPTIONS, uri));
}


/*
 * 'httpPost()' - Send a POST request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPost(http_t     *http,		/* I - HTTP connection */
         const char *uri)		/* I - URI for post */
{
  return (http_send(http, HTTP_POST, uri));
}


/*
 * 'httpPrintf()' - Print a formatted string to a HTTP connection.
 *
 * @private@
 */

int					/* O - Number of bytes written */
httpPrintf(http_t     *http,		/* I - HTTP connection */
           const char *format,		/* I - printf-style format string */
	   ...)				/* I - Additional args as needed */
{
  int		bytes;			/* Number of bytes to write */
  char		buf[16384];		/* Buffer for formatted string */
  va_list	ap;			/* Variable argument pointer */


  DEBUG_printf(("httpPrintf(http=%p, format=\"%s\", ...)\n", http, format));

  va_start(ap, format);
  bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  DEBUG_printf(("httpPrintf: %s", buf));

  if (http->data_encoding == HTTP_ENCODE_FIELDS)
    return (httpWrite2(http, buf, bytes));
  else
  {
    if (http->wused)
    {
      DEBUG_puts("    flushing existing data...");

      if (httpFlushWrite(http) < 0)
	return (-1);
    }

    return (http_write(http, buf, bytes));
  }
}


/*
 * 'httpPut()' - Send a PUT request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPut(http_t     *http,		/* I - HTTP connection */
        const char *uri)		/* I - URI to put */
{
  return (http_send(http, HTTP_PUT, uri));
}


/*
 * 'httpRead()' - Read data from a HTTP connection.
 *
 * This function is deprecated. Use the httpRead2() function which can
 * read more than 2GB of data.
 *
 * @deprecated@
 */

int					/* O - Number of bytes read */
httpRead(http_t *http,			/* I - HTTP connection */
         char   *buffer,		/* I - Buffer for data */
	 int    length)			/* I - Maximum number of bytes */
{
  return ((int)httpRead2(http, buffer, length));
}


/*
 * 'httpRead2()' - Read data from a HTTP connection.
 *
 * @since CUPS 1.2@
 */

ssize_t					/* O - Number of bytes read */
httpRead2(http_t *http,			/* I - HTTP connection */
          char   *buffer,		/* I - Buffer for data */
	  size_t length)		/* I - Maximum number of bytes */
{
  ssize_t	bytes;			/* Bytes read */
  char		len[32];		/* Length string */


  DEBUG_printf(("httpRead(http=%p, buffer=%p, length=%d)\n",
                http, buffer, length));

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (length <= 0)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED &&
      http->data_remaining <= 0)
  {
    DEBUG_puts("httpRead2: Getting chunk length...");

    if (httpGets(len, sizeof(len), http) == NULL)
    {
      DEBUG_puts("httpRead2: Could not get length!");
      return (0);
    }

    http->data_remaining = strtoll(len, NULL, 16);
    if (http->data_remaining < 0)
    {
      DEBUG_puts("httpRead2: Negative chunk length!");
      return (0);
    }
  }

  DEBUG_printf(("httpRead2: data_remaining=" CUPS_LLFMT "\n",
                CUPS_LLCAST http->data_remaining));

  if (http->data_remaining <= 0)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are reading POST
    * data, go idle...
    */

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;

   /*
    * Prevent future reads for this request...
    */

    http->data_encoding = HTTP_ENCODE_LENGTH;

    return (0);
  }
  else if (length > (size_t)http->data_remaining)
    length = (size_t)http->data_remaining;

  if (http->used == 0 && length <= 256)
  {
   /*
    * Buffer small reads for better performance...
    */

    if (!http->blocking && !httpWait(http, 10000))
      return (0);

    if (http->data_remaining > sizeof(http->buffer))
      bytes = sizeof(http->buffer);
    else
      bytes = http->data_remaining;

#ifdef HAVE_SSL
    if (http->tls)
      bytes = http_read_ssl(http, http->buffer, bytes);
    else
#endif /* HAVE_SSL */
    {
      DEBUG_printf(("httpRead2: reading %d bytes from socket into buffer...\n",
                    bytes));

      bytes = recv(http->fd, http->buffer, bytes, 0);

      DEBUG_printf(("httpRead2: read %d bytes from socket into buffer...\n",
                    bytes));
    }

    if (bytes > 0)
      http->used = bytes;
    else if (bytes < 0)
    {
#ifdef WIN32
      http->error = WSAGetLastError();
      return (-1);
#else
      if (errno != EINTR)
      {
        http->error = errno;
        return (-1);
      }
#endif /* WIN32 */
    }
    else
    {
      http->error = EPIPE;
      return (0);
    }
  }

  if (http->used > 0)
  {
    if (length > (size_t)http->used)
      length = (size_t)http->used;

    bytes = (ssize_t)length;

    DEBUG_printf(("httpRead2: grabbing %d bytes from input buffer...\n", bytes));

    memcpy(buffer, http->buffer, length);
    http->used -= (int)length;

    if (http->used > 0)
      memmove(http->buffer, http->buffer + length, http->used);
  }
#ifdef HAVE_SSL
  else if (http->tls)
  {
    if (!http->blocking && !httpWait(http, 10000))
      return (0);

    bytes = (ssize_t)http_read_ssl(http, buffer, (int)length);
  }
#endif /* HAVE_SSL */
  else
  {
    if (!http->blocking && !httpWait(http, 10000))
      return (0);

    DEBUG_printf(("httpRead2: reading %d bytes from socket...\n", length));

#ifdef WIN32
    bytes = (ssize_t)recv(http->fd, buffer, (int)length, 0);
#else
    while ((bytes = recv(http->fd, buffer, length, 0)) < 0)
      if (errno != EINTR)
        break;
#endif /* WIN32 */

    DEBUG_printf(("httpRead2: read %d bytes from socket...\n", bytes));
  }

  if (bytes > 0)
  {
    http->data_remaining -= bytes;

    if (http->data_remaining <= INT_MAX)
      http->_data_remaining = (int)http->data_remaining;
    else
      http->_data_remaining = INT_MAX;
  }
  else if (bytes < 0)
  {
#ifdef WIN32
    http->error = WSAGetLastError();
#else
    if (errno == EINTR)
      bytes = 0;
    else
      http->error = errno;
#endif /* WIN32 */
  }
  else
  {
    http->error = EPIPE;
    return (0);
  }

  if (http->data_remaining == 0)
  {
    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
      httpGets(len, sizeof(len), http);

    if (http->data_encoding != HTTP_ENCODE_CHUNKED)
    {
      if (http->state == HTTP_POST_RECV)
	http->state ++;
      else
	http->state = HTTP_WAITING;
    }
  }

#ifdef DEBUG
  {
    int i, j, ch;
    printf("httpRead2: Read %d bytes:\n", bytes);
    for (i = 0; i < bytes; i += 16)
    {
      printf("   ");

      for (j = 0; j < 16 && (i + j) < bytes; j ++)
        printf(" %02X", buffer[i + j] & 255);

      while (j < 16)
      {
        printf("   ");
	j ++;
      }

      printf("    ");
      for (j = 0; j < 16 && (i + j) < bytes; j ++)
      {
        ch = buffer[i + j] & 255;

	if (ch < ' ' || ch >= 127)
	  ch = '.';

        putchar(ch);
      }
      putchar('\n');
    }
  }
#endif /* DEBUG */

  return (bytes);
}


#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
/*
 * '_httpReadCDSA()' - Read function for the CDSA library.
 */

OSStatus				/* O  - -1 on error, 0 on success */
_httpReadCDSA(
    SSLConnectionRef connection,	/* I  - SSL/TLS connection */
    void             *data,		/* I  - Data buffer */
    size_t           *dataLength)	/* IO - Number of bytes */
{
  OSStatus	result;			/* Return value */
  ssize_t	bytes;			/* Number of bytes read */
  http_t	*http;			/* HTTP connection */


  http = (http_t *)connection;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    if (!http_wait(http, 10000, 0))
    {
      http->error = ETIMEDOUT;
      return (-1);
    }
  }

  do
  {
    bytes = recv(http->fd, data, *dataLength, 0);
  }
  while (bytes == -1 && errno == EINTR);

  if (bytes == *dataLength)
  {
    result = 0;
  }
  else if (bytes > 0)
  {
    *dataLength = bytes;
    result = errSSLWouldBlock;
  }
  else
  {
    *dataLength = 0;

    if (bytes == 0)
      result = errSSLClosedGraceful;
    else if (errno == EAGAIN)
      result = errSSLWouldBlock;
    else
      result = errSSLClosedAbort;
  }

  return (result);
}
#endif /* HAVE_SSL && HAVE_CDSASSL */


#if defined(HAVE_SSL) && defined(HAVE_GNUTLS)
/*
 * '_httpReadGNUTLS()' - Read function for the GNU TLS library.
 */

ssize_t					/* O - Number of bytes read or -1 on error */
_httpReadGNUTLS(
    gnutls_transport_ptr ptr,		/* I - HTTP connection */
    void                 *data,		/* I - Buffer */
    size_t               length)	/* I - Number of bytes to read */
{
  http_t	*http;			/* HTTP connection */


  http = (http_t *)ptr;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    if (!http_wait(http, 10000, 0))
    {
      http->error = ETIMEDOUT;
      return (-1);
    }
  }

  return (recv(http->fd, data, length, 0));
}
#endif /* HAVE_SSL && HAVE_GNUTLS */


/*
 * 'httpReconnect()' - Reconnect to a HTTP server.
 */

int					/* O - 0 on success, non-zero on failure */
httpReconnect(http_t *http)		/* I - HTTP connection */
{
  http_addrlist_t	*addr;		/* Connected address */


  DEBUG_printf(("httpReconnect(http=%p)\n", http));

  if (!http)
    return (-1);

#ifdef HAVE_SSL
  if (http->tls)
    http_shutdown_ssl(http);
#endif /* HAVE_SSL */

 /*
  * Close any previously open socket...
  */

  if (http->fd >= 0)
  {
#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

    http->fd = -1;
  }

 /*
  * Connect to the server...
  */

  if ((addr = httpAddrConnect(http->addrlist, &(http->fd))) == NULL)
  {
   /*
    * Unable to connect...
    */

#ifdef WIN32
    http->error  = WSAGetLastError();
#else
    http->error  = errno;
#endif /* WIN32 */
    http->status = HTTP_ERROR;

    return (-1);
  }

  http->hostaddr = &(addr->addr);
  http->error    = 0;
  http->status   = HTTP_CONTINUE;

#ifdef HAVE_SSL
  if (http->encryption == HTTP_ENCRYPT_ALWAYS)
  {
   /*
    * Always do encryption via SSL.
    */

    if (http_setup_ssl(http) != 0)
    {
#  ifdef WIN32
      closesocket(http->fd);
#  else
      close(http->fd);
#  endif /* WIN32 */

      return (-1);
    }
  }
  else if (http->encryption == HTTP_ENCRYPT_REQUIRED)
    return (http_upgrade(http));
#endif /* HAVE_SSL */

  return (0);
}


/*
 * 'httpSetCookie()' - Set the cookie value(s)...
 *
 * @since CUPS 1.1.19@
 */

void
httpSetCookie(http_t     *http,		/* I - Connection */
              const char *cookie)	/* I - Cookie string */
{
  if (!http)
    return;

  if (http->cookie)
    free(http->cookie);

  if (cookie)
    http->cookie = strdup(cookie);
  else
    http->cookie = NULL;
}


/*
 * 'httpSetExpect()' - Set the Expect: header in a request.
 *
 * Currently only HTTP_CONTINUE is supported for the "expect" argument.
 *
 * @since CUPS 1.2@
 */

void
httpSetExpect(http_t        *http,	/* I - HTTP connection */
              http_status_t expect)	/* I - HTTP status to expect (HTTP_CONTINUE) */
{
  if (http)
    http->expect = expect;
}


/*
 * 'httpSetField()' - Set the value of an HTTP header.
 */

void
httpSetField(http_t       *http,	/* I - HTTP connection */
             http_field_t field,	/* I - Field index */
	     const char   *value)	/* I - Value */
{
  if (http == NULL ||
      field < HTTP_FIELD_ACCEPT_LANGUAGE ||
      field > HTTP_FIELD_WWW_AUTHENTICATE ||
      value == NULL)
    return;

  strlcpy(http->fields[field], value, HTTP_MAX_VALUE);

 /*
  * Special case for Authorization: as its contents can be
  * longer than HTTP_MAX_VALUE
  */

  if (field == HTTP_FIELD_AUTHORIZATION)
  {
    if (http->field_authorization)
      free(http->field_authorization);

    http->field_authorization = strdup(value);
  }
}


/*
 * 'httpSetLength()' - Set the content-length and content-encoding.
 *
 * @since CUPS 1.2@
 */

void
httpSetLength(http_t *http,		/* I - HTTP connection */
              size_t length)		/* I - Length (0 for chunked) */
{
  if (!http)
    return;

  if (!length)
  {
    strcpy(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked");
    http->fields[HTTP_FIELD_CONTENT_LENGTH][0] = '\0';
  }
  else
  {
    http->fields[HTTP_FIELD_TRANSFER_ENCODING][0] = '\0';
    snprintf(http->fields[HTTP_FIELD_CONTENT_LENGTH], HTTP_MAX_VALUE,
             CUPS_LLFMT, CUPS_LLCAST length);
  }
}


/*
 * 'httpTrace()' - Send an TRACE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpTrace(http_t     *http,		/* I - HTTP connection */
          const char *uri)		/* I - URI for trace */
{
  return (http_send(http, HTTP_TRACE, uri));
}


/*
 * 'httpUpdate()' - Update the current HTTP state for incoming data.
 */

http_status_t				/* O - HTTP status */
httpUpdate(http_t *http)		/* I - HTTP connection */
{
  char		line[32768],		/* Line from connection... */
		*value;			/* Pointer to value on line */
  http_field_t	field;			/* Field index */
  int		major, minor,		/* HTTP version numbers */
		status;			/* Request status */


  DEBUG_printf(("httpUpdate(http=%p), state=%d\n", http, http->state));

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    DEBUG_puts("    flushing buffer...");

    if (httpFlushWrite(http) < 0)
      return (HTTP_ERROR);
  }

 /*
  * If we haven't issued any commands, then there is nothing to "update"...
  */

  if (http->state == HTTP_WAITING)
    return (HTTP_CONTINUE);

 /*
  * Grab all of the lines we can from the connection...
  */

  while (httpGets(line, sizeof(line), http) != NULL)
  {
    DEBUG_printf(("httpUpdate: Got \"%s\"\n", line));

    if (line[0] == '\0')
    {
     /*
      * Blank line means the start of the data section (if any).  Return
      * the result code, too...
      *
      * If we get status 100 (HTTP_CONTINUE), then we *don't* change states.
      * Instead, we just return HTTP_CONTINUE to the caller and keep on
      * tryin'...
      */

      if (http->status == HTTP_CONTINUE)
        return (http->status);

      if (http->status < HTTP_BAD_REQUEST)
        http->digest_tries = 0;

#ifdef HAVE_SSL
      if (http->status == HTTP_SWITCHING_PROTOCOLS && !http->tls)
      {
	if (http_setup_ssl(http) != 0)
	{
#  ifdef WIN32
	  closesocket(http->fd);
#  else
	  close(http->fd);
#  endif /* WIN32 */

	  return (HTTP_ERROR);
	}

        return (HTTP_CONTINUE);
      }
#endif /* HAVE_SSL */

      httpGetLength2(http);

      switch (http->state)
      {
        case HTTP_GET :
	case HTTP_POST :
	case HTTP_POST_RECV :
	case HTTP_PUT :
	    http->state ++;
	case HTTP_POST_SEND :
	case HTTP_HEAD :
	    break;

	default :
	    http->state = HTTP_WAITING;
	    break;
      }

      return (http->status);
    }
    else if (strncmp(line, "HTTP/", 5) == 0)
    {
     /*
      * Got the beginning of a response...
      */

      if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, &status) != 3)
        return (HTTP_ERROR);

      http->version = (http_version_t)(major * 100 + minor);
      http->status  = (http_status_t)status;
    }
    else if ((value = strchr(line, ':')) != NULL)
    {
     /*
      * Got a value...
      */

      *value++ = '\0';
      while (isspace(*value & 255))
        value ++;

     /*
      * Be tolerants of servers that send unknown attribute fields...
      */

      if (!strcasecmp(line, "expect"))
      {
       /*
        * "Expect: 100-continue" or similar...
	*/

        http->expect = (http_status_t)atoi(value);
      }
      else if (!strcasecmp(line, "cookie"))
      {
       /*
        * "Cookie: name=value[; name=value ...]" - replaces previous cookies...
	*/

        httpSetCookie(http, value);
      }
      else if ((field = http_field(line)) == HTTP_FIELD_UNKNOWN)
      {
        DEBUG_printf(("httpUpdate: unknown field %s seen!\n", line));
        continue;
      }
      else
        httpSetField(http, field, value);
    }
    else
    {
      http->status = HTTP_ERROR;
      return (HTTP_ERROR);
    }
  }

 /*
  * See if there was an error...
  */

  if (http->error == EPIPE && http->status > HTTP_CONTINUE)
    return (http->status);

  if (http->error)
  {
    DEBUG_printf(("httpUpdate: socket error %d - %s\n", http->error,
                  strerror(http->error)));
    http->status = HTTP_ERROR;
    return (HTTP_ERROR);
  }

 /*
  * If we haven't already returned, then there is nothing new...
  */

  return (HTTP_CONTINUE);
}


/*
 * 'httpWait()' - Wait for data available on a connection.
 *
 * @since CUPS 1.1.19@
 */

int					/* O - 1 if data is available, 0 otherwise */
httpWait(http_t *http,			/* I - HTTP connection */
         int    msec)			/* I - Milliseconds to wait */
{
 /*
  * First see if there is data in the buffer...
  */

  if (http == NULL)
    return (0);

  if (http->used)
    return (1);

 /*
  * Flush pending data, if any...
  */

  if (http->wused)
  {
    if (httpFlushWrite(http) < 0)
      return (0);
  }

 /*
  * If not, check the SSL/TLS buffers and do a select() on the connection...
  */

  return (http_wait(http, msec, 1));
}


/*
 * 'httpWrite()' - Write data to a HTTP connection.
 *
 * This function is deprecated. Use the httpWrite2() function which can
 * write more than 2GB of data.
 *
 * @deprecated@
 */
 
int					/* O - Number of bytes written */
httpWrite(http_t     *http,		/* I - HTTP connection */
          const char *buffer,		/* I - Buffer for data */
	  int        length)		/* I - Number of bytes to write */
{
  return ((int)httpWrite2(http, buffer, length));
}


/*
 * 'httpWrite2()' - Write data to a HTTP connection.
 *
 * @since CUPS 1.2@
 */
 
ssize_t					/* O - Number of bytes written */
httpWrite2(http_t     *http,		/* I - HTTP connection */
           const char *buffer,		/* I - Buffer for data */
	   size_t     length)		/* I - Number of bytes to write */
{
  ssize_t	bytes;			/* Bytes written */


  DEBUG_printf(("httpWrite(http=%p, buffer=%p, length=%d)\n", http,
                buffer, length));

 /*
  * Range check input...
  */

  if (http == NULL || buffer == NULL)
    return (-1);

 /*
  * Mark activity on the connection...
  */

  http->activity = time(NULL);

 /*
  * Buffer small writes for better performance...
  */

  if (length > 0)
  {
    if (http->wused && (length + http->wused) > sizeof(http->wbuffer))
    {
      DEBUG_printf(("    flushing buffer (wused=%d, length=%d)\n",
                    http->wused, length));

      httpFlushWrite(http);
    }

    if ((length + http->wused) <= sizeof(http->wbuffer))
    {
     /*
      * Write to buffer...
      */

      DEBUG_printf(("    copying %d bytes to wbuffer...\n", length));

      memcpy(http->wbuffer + http->wused, buffer, length);
      http->wused += (int)length;
      bytes = (ssize_t)length;
    }
    else
    {
     /*
      * Otherwise write the data directly...
      */

      DEBUG_printf(("    writing %d bytes to socket...\n", length));

      if (http->data_encoding == HTTP_ENCODE_CHUNKED)
	bytes = (ssize_t)http_write_chunk(http, buffer, (int)length);
      else
	bytes = (ssize_t)http_write(http, buffer, (int)length);

      DEBUG_printf(("    wrote %d bytes...\n", bytes));
    }

    if (http->data_encoding == HTTP_ENCODE_LENGTH)
      http->data_remaining -= bytes;
  }
  else
    bytes = 0;

 /*
  * Handle end-of-request processing...
  */

  if ((http->data_encoding == HTTP_ENCODE_CHUNKED && length == 0) ||
      (http->data_encoding == HTTP_ENCODE_LENGTH && http->data_remaining == 0))
  {
   /*
    * Finished with the transfer; unless we are sending POST or PUT
    * data, go idle...
    */

    DEBUG_puts("httpWrite: changing states...");

    if (http->wused)
      httpFlushWrite(http);

    if (http->data_encoding == HTTP_ENCODE_CHUNKED)
    {
     /*
      * Send a 0-length chunk at the end of the request...
      */

      http_write(http, "0\r\n\r\n", 5);

     /*
      * Reset the data state...
      */

      http->data_encoding  = HTTP_ENCODE_LENGTH;
      http->data_remaining = 0;
    }

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else if (http->state == HTTP_PUT_RECV)
      http->state = HTTP_STATUS;
    else
      http->state = HTTP_WAITING;
  }

  return (bytes);
}


#if defined(HAVE_SSL) && defined(HAVE_CDSASSL)
/*
 * '_httpWriteCDSA()' - Write function for the CDSA library.
 */

OSStatus				/* O  - -1 on error, 0 on success */
_httpWriteCDSA(
    SSLConnectionRef connection,	/* I  - SSL/TLS connection */
    const void       *data,		/* I  - Data buffer */
    size_t           *dataLength)	/* IO - Number of bytes */
{
  OSStatus	result;			/* Return value */
  ssize_t	bytes;			/* Number of bytes read */
  http_t	*http;			/* HTTP connection */


  http = (http_t *)connection;

  do
  {
    bytes = write(http->fd, data, *dataLength);
  }
  while (bytes == -1 && errno == EINTR);

  if (bytes == *dataLength)
  {
    result = 0;
  }
  else if (bytes >= 0)
  {
    *dataLength = bytes;
    result = errSSLWouldBlock;
  }
  else
  {
    *dataLength = 0;
  
    if (errno == EAGAIN)
      result = errSSLWouldBlock;
    else
      result = errSSLClosedAbort;
  }

  return (result);
}
#endif /* HAVE_SSL && HAVE_CDSASSL */


#if defined(HAVE_SSL) && defined(HAVE_GNUTLS)
/*
 * '_httpWriteGNUTLS()' - Write function for the GNU TLS library.
 */

ssize_t					/* O - Number of bytes written or -1 on error */
_httpWriteGNUTLS(
    gnutls_transport_ptr ptr,		/* I - HTTP connection */
    const void           *data,		/* I - Data buffer */
    size_t               length)	/* I - Number of bytes to write */
{
  return (send(((http_t *)ptr)->fd, data, length, 0));
}
#endif /* HAVE_SSL && HAVE_GNUTLS */


#if defined(HAVE_SSL) && defined(HAVE_LIBSSL)
/*
 * 'http_bio_ctrl()' - Control the HTTP connection.
 */

static long				/* O - Result/data */
http_bio_ctrl(BIO  *h,			/* I - BIO data */
              int  cmd,			/* I - Control command */
	      long arg1,		/* I - First argument */
	      void *arg2)		/* I - Second argument */
{
  switch (cmd)
  {
    default :
        return (0);

    case BIO_CTRL_RESET :
        h->ptr = NULL;
	return (0);

    case BIO_C_SET_FILE_PTR :
        h->ptr  = arg2;
	h->init = 1;
	return (1);

    case BIO_C_GET_FILE_PTR :
        if (arg2)
	{
	  *((void **)arg2) = h->ptr;
	  return (1);
	}
	else
	  return (0);
        
    case BIO_CTRL_DUP :
    case BIO_CTRL_FLUSH :
        return (1);
  }
}


/*
 * 'http_bio_free()' - Free OpenSSL data.
 */

static int				/* O - 1 on success, 0 on failure */
http_bio_free(BIO *h)			/* I - BIO data */
{
  if (!h)
    return (0);

  if (h->shutdown)
  {
    h->init  = 0;
    h->flags = 0;
  }

  return (1);
}


/*
 * 'http_bio_new()' - Initialize an OpenSSL BIO structure.
 */

static int				/* O - 1 on success, 0 on failure */
http_bio_new(BIO *h)			/* I - BIO data */
{
  if (!h)
    return (0);

  h->init  = 0;
  h->num   = 0;
  h->ptr   = NULL;
  h->flags = 0;

  return (1);
}


/*
 * 'http_bio_puts()' - Send a string for OpenSSL.
 */

static int				/* O - Bytes written */
http_bio_puts(BIO        *h,		/* I - BIO data */
              const char *str)		/* I - String to write */
{
#ifdef WIN32
  return (send(((http_t *)h->ptr)->fd, str, (int)strlen(str), 0));
#else
  return (send(((http_t *)h->ptr)->fd, str, strlen(str), 0));
#endif /* WIN32 */
}


/*
 * 'http_bio_read()' - Read data for OpenSSL.
 */

static int				/* O - Bytes read */
http_bio_read(BIO  *h,			/* I - BIO data */
              char *buf,		/* I - Buffer */
	      int  size)		/* I - Number of bytes to read */
{
  http_t	*http;			/* HTTP connection */


  http = (http_t *)h->ptr;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    if (!http_wait(http, 10000, 0))
    {
#ifdef WIN32
      http->error = WSAETIMEDOUT;
#else
      http->error = ETIMEDOUT;
#endif /* WIN32 */

      return (-1);
    }
  }

  return (recv(http->fd, buf, size, 0));
}


/*
 * 'http_bio_write()' - Write data for OpenSSL.
 */

static int				/* O - Bytes written */
http_bio_write(BIO        *h,		/* I - BIO data */
               const char *buf,		/* I - Buffer to write */
	       int        num)		/* I - Number of bytes to write */
{
  return (send(((http_t *)h->ptr)->fd, buf, num, 0));
}
#endif /* HAVE_SSL && HAVE_LIBSSL */


/*
 * 'http_field()' - Return the field index for a field name.
 */

static http_field_t		/* O - Field index */
http_field(const char *name)	/* I - String name */
{
  int	i;			/* Looping var */


  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (strcasecmp(name, http_fields[i]) == 0)
      return ((http_field_t)i);

  return (HTTP_FIELD_UNKNOWN);
}


#ifdef HAVE_SSL
/*
 * 'http_read_ssl()' - Read from a SSL/TLS connection.
 */

static int				/* O - Bytes read */
http_read_ssl(http_t *http,		/* I - HTTP connection */
	      char   *buf,		/* I - Buffer to store data */
	      int    len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_read((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  return (gnutls_record_recv(((http_tls_t *)(http->tls))->session, buf, len));

#  elif defined(HAVE_CDSASSL)
  int		result;			/* Return value */
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLRead(((http_tls_t *)http->tls)->session, buf, len, &processed);

  switch (error)
  {
    case 0 :
	result = (int)processed;
	break;
    case errSSLClosedGraceful :
	result = 0;
	break;
    case errSSLWouldBlock :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno = EINTR;
	}
	break;
    default :
	errno = EPIPE;
	result = -1;
	break;
  }

  return (result);
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


/*
 * 'http_send()' - Send a request with all fields and the trailing blank line.
 */

static int			/* O - 0 on success, non-zero on error */
http_send(http_t       *http,	/* I - HTTP connection */
          http_state_t request,	/* I - Request code */
	  const char   *uri)	/* I - URI */
{
  int		i;		/* Looping var */
  char		*ptr,		/* Pointer in buffer */
		buf[1024];	/* Encoded URI buffer */
  static const char * const codes[] =
		{		/* Request code strings */
		  NULL,
		  "OPTIONS",
		  "GET",
		  NULL,
		  "HEAD",
		  "POST",
		  NULL,
		  NULL,
		  "PUT",
		  NULL,
		  "DELETE",
		  "TRACE",
		  "CLOSE"
		};
  static const char hex[] = "0123456789ABCDEF";
				/* Hex digits */


  DEBUG_printf(("http_send(http=%p, request=HTTP_%s, uri=\"%s\")\n",
                http, codes[request], uri));

  if (http == NULL || uri == NULL)
    return (-1);

 /*
  * Set the User-Agent field if it isn't already...
  */

  if (!http->fields[HTTP_FIELD_USER_AGENT][0])
    httpSetField(http, HTTP_FIELD_USER_AGENT, CUPS_MINIMAL);

 /*
  * Encode the URI as needed...
  */

  for (ptr = buf; *uri != '\0' && ptr < (buf + sizeof(buf) - 1); uri ++)
    if (*uri <= ' ' || *uri >= 127)
    {
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = '%';
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = hex[(*uri >> 4) & 15];
      if (ptr < (buf + sizeof(buf) - 1))
        *ptr ++ = hex[*uri & 15];
    }
    else
      *ptr ++ = *uri;

  *ptr = '\0';

 /*
  * See if we had an error the last time around; if so, reconnect...
  */

  if (http->status == HTTP_ERROR || http->status >= HTTP_BAD_REQUEST)
    if (httpReconnect(http))
      return (-1);

 /*
  * Flush any written data that is pending...
  */

  if (http->wused)
    httpFlushWrite(http);

 /*
  * Send the request header...
  */

  http->state         = request;
  http->data_encoding = HTTP_ENCODE_FIELDS;

  if (request == HTTP_POST || request == HTTP_PUT)
    http->state ++;

  http->status = HTTP_CONTINUE;

#ifdef HAVE_SSL
  if (http->encryption == HTTP_ENCRYPT_REQUIRED && !http->tls)
  {
    httpSetField(http, HTTP_FIELD_CONNECTION, "Upgrade");
    httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.0,SSL/2.0,SSL/3.0");
  }
#endif /* HAVE_SSL */

  if (httpPrintf(http, "%s %s HTTP/1.1\r\n", codes[request], buf) < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (http->fields[i][0] != '\0')
    {
      DEBUG_printf(("%s: %s\n", http_fields[i], httpGetField(http, i)));

      if (httpPrintf(http, "%s: %s\r\n", http_fields[i], 
		     httpGetField(http, i)) < 1)
      {
	http->status = HTTP_ERROR;
	return (-1);
      }
    }

  if (http->cookie)
    if (httpPrintf(http, "Cookie: $Version=0; %s\r\n", http->cookie) < 1)
    {
      http->status = HTTP_ERROR;
      return (-1);
    }

  if (http->expect == HTTP_CONTINUE &&
      (http->state == HTTP_POST_RECV || http->state == HTTP_PUT_RECV))
    if (httpPrintf(http, "Expect: 100-continue\r\n") < 1)
    {
      http->status = HTTP_ERROR;
      return (-1);
    }

  if (httpPrintf(http, "\r\n") < 1)
  {
    http->status = HTTP_ERROR;
    return (-1);
  }

  httpFlushWrite(http);
  httpGetLength2(http);
  httpClearFields(http);

 /*
  * The Kerberos authentication string can only be used once...
  */

  if (http->authstring && !strncmp(http->authstring, "Negotiate", 9))
  {
    http->_authstring[0] = '\0';

    if (http->authstring != http->_authstring)
      free(http->authstring);
  
    http->authstring = http->_authstring;
  }

  return (0);
}


#ifdef HAVE_SSL
/*
 * 'http_setup_ssl()' - Set up SSL/TLS support on a connection.
 */

static int				/* O - Status of connection */
http_setup_ssl(http_t *http)		/* I - HTTP connection */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */
  BIO		*bio;			/* BIO data */
#  elif defined(HAVE_GNUTLS)
  http_tls_t	*conn;			/* TLS session object */
  gnutls_certificate_client_credentials *credentials;
					/* TLS credentials */
#  elif defined(HAVE_CDSASSL)
  OSStatus	error;			/* Error code */
  http_tls_t	*conn;			/* CDSA connection information */
#  endif /* HAVE_LIBSSL */


  DEBUG_printf(("http_setup_ssl(http=%p)\n", http));

#  ifdef HAVE_LIBSSL
  context = SSL_CTX_new(SSLv23_client_method());

  SSL_CTX_set_options(context, SSL_OP_NO_SSLv2); /* Only use SSLv3 or TLS */

  bio = BIO_new(_httpBIOMethods());
  BIO_ctrl(bio, BIO_C_SET_FILE_PTR, 0, (char *)http);

  conn = SSL_new(context);
  SSL_set_bio(conn, bio, bio);

  if (SSL_connect(conn) != 1)
  {
#    ifdef DEBUG
    unsigned long	error;	/* Error code */

    while ((error = ERR_get_error()) != 0)
      printf("http_setup_ssl: %s\n", ERR_error_string(error, NULL));
#    endif /* DEBUG */

    SSL_CTX_free(context);
    SSL_free(conn);

#    ifdef WIN32
    http->error  = WSAGetLastError();
#    else
    http->error  = errno;
#    endif /* WIN32 */
    http->status = HTTP_ERROR;

    return (HTTP_ERROR);
  }

#  elif defined(HAVE_GNUTLS)
  if ((conn = (http_tls_t *)malloc(sizeof(http_tls_t))) == NULL)
  {
    http->error  = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  credentials = (gnutls_certificate_client_credentials *)
                    malloc(sizeof(gnutls_certificate_client_credentials));
  if (credentials == NULL)
  {
    free(conn);

    http->error = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  gnutls_certificate_allocate_credentials(credentials);

  gnutls_init(&(conn->session), GNUTLS_CLIENT);
  gnutls_set_default_priority(conn->session);
  gnutls_credentials_set(conn->session, GNUTLS_CRD_CERTIFICATE, *credentials);
  gnutls_transport_set_ptr(conn->session, (gnutls_transport_ptr)http);
  gnutls_transport_set_pull_function(conn->session, _httpReadGNUTLS);
  gnutls_transport_set_push_function(conn->session, _httpWriteGNUTLS);

  if ((gnutls_handshake(conn->session)) != GNUTLS_E_SUCCESS)
  {
    http->error  = errno;
    http->status = HTTP_ERROR;

    return (-1);
  }

  conn->credentials = credentials;

#  elif defined(HAVE_CDSASSL)
  conn = (http_tls_t *)calloc(1, sizeof(http_tls_t));

  if (conn == NULL)
    return (-1);

  if ((error = SSLNewContext(false, &conn->session)))
  {
    http->error  = error;
    http->status = HTTP_ERROR;

    free(conn);
    return (-1);
  }

 /*
  * Use a union to resolve warnings about int/pointer size mismatches...
  */

  error = SSLSetConnection(conn->session, http);

  if (!error)
    error = SSLSetIOFuncs(conn->session, _httpReadCDSA, _httpWriteCDSA);

  if (!error)
    error = SSLSetAllowsExpiredCerts(conn->session, true);

  if (!error)
    error = SSLSetAllowsAnyRoot(conn->session, true);

  if (!error)
    error = SSLSetProtocolVersionEnabled(conn->session, kSSLProtocol2, false);

  if (!error)
  {
    while ((error = SSLHandshake(conn->session)) == errSSLWouldBlock)
      usleep(1000);
  }

  if (error)
  {
    http->error  = error;
    http->status = HTTP_ERROR;

    SSLDisposeContext(conn->session);

    free(conn);

    return (-1);
  }
#  endif /* HAVE_CDSASSL */

  http->tls = conn;
  return (0);
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_shutdown_ssl()' - Shut down SSL/TLS on a connection.
 */

static void
http_shutdown_ssl(http_t *http)		/* I - HTTP connection */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */


  conn    = (SSL *)(http->tls);
  context = SSL_get_SSL_CTX(conn);

  SSL_shutdown(conn);
  SSL_CTX_free(context);
  SSL_free(conn);

#  elif defined(HAVE_GNUTLS)
  http_tls_t      *conn;		/* Encryption session */
  gnutls_certificate_client_credentials *credentials;
					/* TLS credentials */


  conn = (http_tls_t *)(http->tls);
  credentials = (gnutls_certificate_client_credentials *)(conn->credentials);

  gnutls_bye(conn->session, GNUTLS_SHUT_RDWR);
  gnutls_deinit(conn->session);
  gnutls_certificate_free_credentials(*credentials);
  free(credentials);
  free(conn);

#  elif defined(HAVE_CDSASSL)
  http_tls_t      *conn;		/* CDSA connection information */


  conn = (http_tls_t *)(http->tls);

  while (SSLClose(conn->session) == errSSLWouldBlock)
    usleep(1000);

  SSLDisposeContext(conn->session);

  if (conn->certsArray)
    CFRelease(conn->certsArray);

  free(conn);
#  endif /* HAVE_LIBSSL */

  http->tls = NULL;
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_upgrade()' - Force upgrade to TLS encryption.
 */

static int				/* O - Status of connection */
http_upgrade(http_t *http)		/* I - HTTP connection */
{
  int		ret;			/* Return value */
  http_t	myhttp;			/* Local copy of HTTP data */


  DEBUG_printf(("http_upgrade(%p)\n", http));

 /*
  * Copy the HTTP data to a local variable so we can do the OPTIONS
  * request without interfering with the existing request data...
  */

  memcpy(&myhttp, http, sizeof(myhttp));

 /*
  * Send an OPTIONS request to the server, requiring SSL or TLS
  * encryption on the link...
  */

  http->field_authorization = NULL;	/* Don't free the auth string */

  httpClearFields(http);
  httpSetField(http, HTTP_FIELD_CONNECTION, "upgrade");
  httpSetField(http, HTTP_FIELD_UPGRADE, "TLS/1.0, SSL/2.0, SSL/3.0");

  if ((ret = httpOptions(http, "*")) == 0)
  {
   /*
    * Wait for the secure connection...
    */

    while (httpUpdate(http) == HTTP_CONTINUE);
  }

  httpFlush(http);

 /*
  * Restore the HTTP request data...
  */

  memcpy(http->fields, myhttp.fields, sizeof(http->fields));
  http->data_encoding       = myhttp.data_encoding;
  http->data_remaining      = myhttp.data_remaining;
  http->_data_remaining     = myhttp._data_remaining;
  http->expect              = myhttp.expect;
  http->field_authorization = myhttp.field_authorization;

 /*
  * See if we actually went secure...
  */

  if (!http->tls)
  {
   /*
    * Server does not support HTTP upgrade...
    */

    DEBUG_puts("Server does not support HTTP upgrade!");

#  ifdef WIN32
    closesocket(http->fd);
#  else
    close(http->fd);
#  endif

    http->fd = -1;

    return (-1);
  }
  else
    return (ret);
}
#endif /* HAVE_SSL */


/*
 * 'http_wait()' - Wait for data available on a connection.
 */

static int				/* O - 1 if data is available, 0 otherwise */
http_wait(http_t *http,			/* I - HTTP connection */
          int    msec,			/* I - Milliseconds to wait */
	  int    usessl)		/* I - Use SSL context? */
{
#ifdef HAVE_POLL
  struct pollfd		pfd;		/* Polled file descriptor */
#else
  fd_set		input_set;	/* select() input set */
  struct timeval	timeout;	/* Timeout */
#endif /* HAVE_POLL */
  int			nfds;		/* Result from select()/poll() */


  DEBUG_printf(("http_wait(http=%p, msec=%d)\n", http, msec));

  if (http->fd < 0)
    return (0);

 /*
  * Check the SSL/TLS buffers for data first...
  */

#ifdef HAVE_SSL
  if (http->tls && usessl)
  {
#  ifdef HAVE_LIBSSL
    if (SSL_pending((SSL *)(http->tls)))
      return (1);
#  elif defined(HAVE_GNUTLS)
    if (gnutls_record_check_pending(((http_tls_t *)(http->tls))->session))
      return (1);
#  elif defined(HAVE_CDSASSL)
    size_t bytes;			/* Bytes that are available */

    if (!SSLGetBufferedReadSize(((http_tls_t *)http->tls)->session, &bytes) && bytes > 0)
      return (1);
#  endif /* HAVE_LIBSSL */
  }
#endif /* HAVE_SSL */

 /*
  * Then try doing a select() or poll() to poll the socket...
  */

#ifdef HAVE_POLL
  pfd.fd     = http->fd;
  pfd.events = POLLIN;

  while ((nfds = poll(&pfd, 1, msec)) < 0 && errno == EINTR);

#else
  do
  {
    FD_ZERO(&input_set);
    FD_SET(http->fd, &input_set);

    DEBUG_printf(("http_wait: msec=%d, http->fd=%d\n", msec, http->fd));

    if (msec >= 0)
    {
      timeout.tv_sec  = msec / 1000;
      timeout.tv_usec = (msec % 1000) * 1000;

      nfds = select(http->fd + 1, &input_set, NULL, NULL, &timeout);
    }
    else
      nfds = select(http->fd + 1, &input_set, NULL, NULL, NULL);

    DEBUG_printf(("http_wait: select() returned %d...\n", nfds));
  }
#  ifdef WIN32
  while (nfds < 0 && WSAGetLastError() == WSAEINTR);
#  else
  while (nfds < 0 && errno == EINTR);
#  endif /* WIN32 */
#endif /* HAVE_POLL */

  DEBUG_printf(("http_wait: returning with nfds=%d...\n", nfds));

  return (nfds > 0);
}


/*
 * 'http_write()' - Write a buffer to a HTTP connection.
 */
 
static int				/* O - Number of bytes written */
http_write(http_t     *http,		/* I - HTTP connection */
          const char *buffer,		/* I - Buffer for data */
	  int        length)		/* I - Number of bytes to write */
{
  int	tbytes,				/* Total bytes sent */
	bytes;				/* Bytes sent */


  tbytes = 0;

  while (length > 0)
  {
#ifdef HAVE_SSL
    if (http->tls)
      bytes = http_write_ssl(http, buffer, length);
    else
#endif /* HAVE_SSL */
    bytes = send(http->fd, buffer, length, 0);

    if (bytes < 0)
    {
#ifdef WIN32
      if (WSAGetLastError() != http->error)
      {
        http->error = WSAGetLastError();
	continue;
      }
#else
      if (errno == EINTR)
        continue;
      else if (errno != http->error && errno != ECONNRESET)
      {
        http->error = errno;
	continue;
      }
#endif /* WIN32 */

      DEBUG_puts("http_write: error writing data...\n");

      return (-1);
    }

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
  }

#ifdef DEBUG
  {
    int i, j, ch;
    printf("http_write: wrote %d bytes: \n", tbytes);
    for (i = 0, buffer -= tbytes; i < tbytes; i += 16)
    {
      printf("   ");

      for (j = 0; j < 16 && (i + j) < tbytes; j ++)
        printf(" %02X", buffer[i + j] & 255);

      while (j < 16)
      {
        printf("   ");
	j ++;
      }

      printf("    ");
      for (j = 0; j < 16 && (i + j) < tbytes; j ++)
      {
        ch = buffer[i + j] & 255;

	if (ch < ' ' || ch == 127)
	  ch = '.';

        putchar(ch);
      }
      putchar('\n');
    }
  }
#endif /* DEBUG */

  return (tbytes);
}


/*
 * 'http_write_chunk()' - Write a chunked buffer.
 */

static int				/* O - Number bytes written */
http_write_chunk(http_t     *http,	/* I - HTTP connection */
                 const char *buffer,	/* I - Buffer to write */
		 int        length)	/* I - Length of buffer */
{
  char	header[255];			/* Chunk header */
  int	bytes;				/* Bytes written */

  DEBUG_printf(("http_write_chunk(http=%p, buffer=%p, length=%d)\n",
                http, buffer, length));

 /*
  * Write the chunk header, data, and trailer.
  */

  sprintf(header, "%x\r\n", length);
  if (http_write(http, header, (int)strlen(header)) < 0)
  {
    DEBUG_puts("    http_write of length failed!");
    return (-1);
  }

  if ((bytes = http_write(http, buffer, length)) < 0)
  {
    DEBUG_puts("    http_write of buffer failed!");
    return (-1);
  }

  if (http_write(http, "\r\n", 2) < 0)
  {
    DEBUG_puts("    http_write of CR LF failed!");
    return (-1);
  }

  return (bytes);
}


#ifdef HAVE_SSL
/*
 * 'http_write_ssl()' - Write to a SSL/TLS connection.
 */

static int				/* O - Bytes written */
http_write_ssl(http_t     *http,	/* I - HTTP connection */
	       const char *buf,		/* I - Buffer holding data */
	       int        len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_write((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  return (gnutls_record_send(((http_tls_t *)(http->tls))->session, buf, len));
#  elif defined(HAVE_CDSASSL)
  int		result;			/* Return value */
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLWrite(((http_tls_t *)http->tls)->session, buf, len, &processed);

  switch (error)
  {
    case 0 :
	result = (int)processed;
	break;
    case errSSLClosedGraceful :
	result = 0;
	break;
    case errSSLWouldBlock :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno = EINTR;
	}
	break;
    default :
	errno = EPIPE;
	result = -1;
	break;
  }

  return (result);
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


/*
 * End of "$Id$".
 */
