/*
 * "$Id: http.c,v 1.35 1999/05/17 18:03:31 mike Exp $"
 *
 *   HTTP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
 *
 *   These statusd instructions, statements, and computer programs are the
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
 *   httpInitialize()    - Initialize the HTTP interface library and set the
 *                         default HTTP proxy (if any).
 *   httpClose()         - Close an HTTP connection...
 *   httpConnect()       - Connect to a HTTP server.
 *   httpReconnect()     - Reconnect to a HTTP server...
 *   httpSeparate()      - Separate a Universal Resource Identifier into its
 *                         components.
 *   httpSetField()      - Set the value of an HTTP header.
 *   httpDelete()        - Send a DELETE request to the server.
 *   httpGet()           - Send a GET request to the server.
 *   httpHead()          - Send a HEAD request to the server.
 *   httpOptions()       - Send an OPTIONS request to the server.
 *   httpPost()          - Send a POST request to the server.
 *   httpPut()           - Send a PUT request to the server.
 *   httpTrace()         - Send an TRACE request to the server.
 *   httpFlush()         - Flush data from a HTTP connection.
 *   httpRead()          - Read data from a HTTP connection.
 *   httpWrite()         - Write data to a HTTP connection.
 *   httpGets()          - Get a line of text from a HTTP connection.
 *   httpPrintf()        - Print a formatted string to a HTTP connection.
 *   httpStatus()        - Return a short string describing a HTTP status code.
 *   httpGetDateString() - Get a formatted date/time string from a time value.
 *   httpGetDateTime()   - Get a time value from a formatted date/time string.
 *   httpUpdate()        - Update the current HTTP state for incoming data.
 *   httpDecode64()      - Base64-decode a string.
 *   httpEncode64()      - Base64-encode a string.
 *   httpGetLength()     - Get the amount of data remaining from the
 *                         content-length or transfer-encoding fields.
 *   http_field()        - Return the field index for a field name.
 *   http_send()         - Send a request with all fields and the trailing
 *                         blank line.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "string.h"
#include <fcntl.h>

#include "http.h"
#include "ipp.h"
#include "debug.h"

#if !defined(WIN32) && !defined(__EMX__)
#  include <signal.h>
#endif /* !WIN32 && !__EMX__ */

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

static http_field_t	http_field(char *name);
static int		http_send(http_t *http, http_state_t request, char *uri);


/*
 * Local globals...
 */

static char		*http_fields[] =
			{
			  "Accept",
			  "Accept-Charset",
			  "Accept-Encoding",
			  "Accept-Language",
			  "Accept-Ranges",
			  "Age",
			  "Allow",
			  "Alternates",
			  "Authorization",
			  "Cache-Control",
			  "Connection",
			  "Content-Base",
			  "Content-Encoding",
			  "Content-Language",
			  "Content-Length",
			  "Content-Location",
			  "Content-MD5",
			  "Content-Range",
			  "Content-Type",
			  "Content-Version",
			  "Date",
			  "Derived-From",
			  "Etag",
			  "Expires",
			  "From",
			  "Host",
			  "If-Match",
			  "If-Modified-Since",
			  "If-None-Match",
			  "If-Range",
			  "If-Unmodified-since",
			  "Keep-Alive",
			  "Last-Modified",
			  "Link",
			  "Location",
			  "Max-Forwards",
			  "Message-Id",
			  "MIME-Version",
			  "Pragma",
			  "Proxy-Authenticate",
			  "Proxy-Authorization",
			  "Public",
			  "Range",
			  "Referer",
			  "Retry-After",
			  "Server",
			  "Transfer-Encoding",
			  "Upgrade",
			  "URI",
			  "User-Agent",
			  "Vary",
			  "Via",
			  "Warning",
			  "WWW-Authenticate"
			};
static char		*days[7] =
			{
			  "Sun",
			  "Mon",
			  "Tue",
			  "Wed",
			  "Thu",
			  "Fri",
			  "Sat"
			};
static char		*months[12] =
			{
			  "Jan",
			  "Feb",
			  "Mar",
			  "Apr",
			  "May",
			  "Jun",
		          "Jul",
			  "Aug",
			  "Sep",
			  "Oct",
			  "Nov",
			  "Dec"
			};


/*
 * 'httpInitialize()' - Initialize the HTTP interface library and set the
 *                      default HTTP proxy (if any).
 */

void
httpInitialize(void)
{
#if defined(WIN32) || defined(__EMX__)
  WSADATA	winsockdata;	/* WinSock data */
  static int	initialized = 0;/* Has WinSock been initialized? */


  if (!initialized)
    WSAStartup(MAKEWORD(1,1), &winsockdata);
#elif defined(HAVE_SIGSET)
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  struct sigaction	action;	/* POSIX sigaction data */


 /*
  * Ignore SIGPIPE signals...
  */

  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif /* WIN32 || __EMX__ */
}


/*
 * 'httpClose()' - Close an HTTP connection...
 */

void
httpClose(http_t *http)		/* I - Connection to close */
{
  if (http == NULL)
    return;

#ifdef WIN32
  closesocket(http->fd);
#else
  close(http->fd);
#endif /* WIN32 */

  free(http);
}


/*
 * 'httpConnect()' - Connect to a HTTP server.
 */

http_t *			/* O - New HTTP connection */
httpConnect(char *host,		/* I - Host to connect to */
            int  port)		/* I - Port number */
{
  http_t		*http;		/* New HTTP connection */
  struct hostent	*hostaddr;	/* Host address data */


  httpInitialize();

 /*
  * Lookup the host...
  */

  if ((hostaddr = gethostbyname(host)) == NULL)
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

 /*
  * Copy the hostname and port and then "reconnect"...
  */

  strcpy(http->hostname, host);
  memset((char *)&(http->hostaddr), 0, sizeof(http->hostaddr));
  memcpy((char *)&(http->hostaddr.sin_addr), hostaddr->h_addr, hostaddr->h_length);
  http->hostaddr.sin_family = hostaddr->h_addrtype;
#ifdef WIN32
  http->hostaddr.sin_port   = htons((u_short)port);
#else
  http->hostaddr.sin_port   = htons(port);
#endif /* WIN32 */
  if (httpReconnect(http))
  {
    free(http);
    return (NULL);
  }
  else
    return (http);
}


/*
 * 'httpReconnect()' - Reconnect to a HTTP server...
 */

int				/* O - 0 on success, non-zero on failure */
httpReconnect(http_t *http)	/* I - HTTP data */
{
  int	val;			/* Socket option value */


 /*
  * Close any previously open socket...
  */

  if (http->fd)
#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif /* WIN32 */

 /*
  * Create the socket and set options to allow reuse.
  */

  if ((http->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return (-1);

#ifdef FD_CLOEXEC
  fcntl(http->fd, F_SETFD, FD_CLOEXEC);	/* Close this socket when starting *
					 * other processes...              */
#endif /* FD_CLOEXEC */

  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

#ifdef SO_REUSEPORT
  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
#endif /* SO_REUSEPORT */

 /*
  * Connect to the server...
  */

  if (connect(http->fd, (struct sockaddr *)&(http->hostaddr),
              sizeof(http->hostaddr)) < 0)
  {
#ifdef WIN32
    closesocket(http->fd);
#else
    close(http->fd);
#endif

    return (-1);
  }

  return (0);
}


/*
 * 'httpSeparate()' - Separate a Universal Resource Identifier into its
 *                    components.
 */

void
httpSeparate(char *uri,		/* I - Universal Resource Identifier */
             char *method,	/* O - Method (http, https, etc.) */
	     char *username,	/* O - Username */
	     char *host,	/* O - Hostname */
	     int  *port,	/* O - Port number to use */
             char *resource)	/* O - Resource/filename */
{
  char	*ptr;			/* Pointer into string... */


  if (uri == NULL || method == NULL || username == NULL || host == NULL ||
      port == NULL || resource == NULL)
    return;

 /*
  * Grab the method portion of the URI...
  */

  ptr = host;
  while (*uri != ':' && *uri != '\0')
    *ptr++ = *uri++;

  *ptr = '\0';
  if (*uri == ':')
    uri ++;

 /*
  * If the method contains a period or slash, then it's probably
  * hostname/filename...
  */

  if (strchr(host, '.') != NULL || strchr(host, '/') != NULL || *uri == '\0')
  {
    if ((ptr = strchr(host, '/')) != NULL)
    {
      strcpy(resource, ptr);
      *ptr = '\0';
    }
    else
      resource[0] = '\0';

    if (isdigit(*uri))
    {
     /*
      * OK, we have "hostname:port[/resource]"...
      */

      *port = strtol(uri, &uri, 10);

      if (*uri == '/')
        strcpy(resource, uri);
    }
    else
      *port = 0;

    strcpy(method, "http");
    username[0] = '\0';
    return;
  }
  else
    strcpy(method, host);

 /*
  * If the method starts with less than 2 slashes then it is a local resource...
  */

  if (strncmp(uri, "//", 2) != 0)
  {
    strcpy(resource, uri);
    username[0] = '\0';
    host[0]     = '\0';
    *port       = 0;
    return;
  }

 /*
  * Grab the hostname...
  */

  while (*uri == '/')
    uri ++;

  ptr = host;
  while (*uri != ':' && *uri != '@' && *uri != '/' && *uri != '\0')
    *ptr ++ = *uri ++;

  *ptr = '\0';

  if (*uri == '@')
  {
   /*
    * Got a username...
    */

    strcpy(username, host);
    
    ptr = host;
    while (*uri != ':' && *uri != '/' && *uri != '\0')
      *ptr ++ = *uri ++;

    *ptr = '\0';
  }
  else
    username[0] = '\0';

  if (*uri == '\0')
  {
   /*
    * Hostname but no port or path...
    */

    *port = 0;
    resource[0] = '/';
    resource[1] = '\0';
    return;
  }
  else if (*uri == ':')
  {
   /*
    * Parse port number...
    */

    *port = 0;
    uri ++;
    while (isdigit(*uri))
    {
      *port = (*port * 10) + *uri - '0';
      uri ++;
    }
  }
  else
  {
   /*
    * Figure out the default port number based on the method...
    */

    if (strcasecmp(method, "http") == 0)
      *port = 80;
    else if (strcasecmp(method, "https") == 0)
      *port = 443;
    else if (strcasecmp(method, "ipp") == 0)	/* Not registered yet... */
      *port = ippPort();
    else
      *port = 0;
  }

 /*
  * The remaining portion is the resource string...
  */

  strcpy(resource, uri);
}


/*
 * 'httpSetField()' - Set the value of an HTTP header.
 */

void
httpSetField(http_t       *http,	/* I - HTTP data */
             http_field_t field,	/* I - Field index */
	     char         *value)	/* I - Value */
{
  strncpy(http->fields[field], value, HTTP_MAX_VALUE - 1);
  http->fields[field][HTTP_MAX_VALUE - 1] = '\0';
}


/*
 * 'httpDelete()' - Send a DELETE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpDelete(http_t *http,		/* I - HTTP data */
           char   *uri)			/* I - URI to delete */
{
  return (http_send(http, HTTP_DELETE, uri));
}


/*
 * 'httpGet()' - Send a GET request to the server.
 */

int					/* O - Status of call (0 = success) */
httpGet(http_t *http,			/* I - HTTP data */
        char   *uri)			/* I - URI to get */
{
  return (http_send(http, HTTP_GET, uri));
}


/*
 * 'httpHead()' - Send a HEAD request to the server.
 */

int					/* O - Status of call (0 = success) */
httpHead(http_t *http,			/* I - HTTP data */
         char   *uri)			/* I - URI for head */
{
  return (http_send(http, HTTP_HEAD, uri));
}


/*
 * 'httpOptions()' - Send an OPTIONS request to the server.
 */

int					/* O - Status of call (0 = success) */
httpOptions(http_t *http,		/* I - HTTP data */
            char   *uri)		/* I - URI for options */
{
  return (http_send(http, HTTP_OPTIONS, uri));
}


/*
 * 'httpPost()' - Send a POST request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPost(http_t *http,			/* I - HTTP data */
         char   *uri)			/* I - URI for post */
{
  httpGetLength(http);

  return (http_send(http, HTTP_POST, uri));
}


/*
 * 'httpPut()' - Send a PUT request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPut(http_t *http,			/* I - HTTP data */
        char   *uri)			/* I - URI to put */
{
  httpGetLength(http);

  return (http_send(http, HTTP_PUT, uri));
}


/*
 * 'httpTrace()' - Send an TRACE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpTrace(http_t *http,			/* I - HTTP data */
          char   *uri)			/* I - URI for trace */
{
  return (http_send(http, HTTP_TRACE, uri));
}


/*
 * 'httpFlush()' - Flush data from a HTTP connection.
 */

void
httpFlush(http_t *http)	/* I - HTTP data */
{
  char	buffer[8192];	/* Junk buffer */


  while (httpRead(http, buffer, sizeof(buffer)) > 0);
}


/*
 * 'httpRead()' - Read data from a HTTP connection.
 */

int					/* O - Number of bytes read */
httpRead(http_t *http,			/* I - HTTP data */
         char   *buffer,		/* I - Buffer for data */
	 int    length)			/* I - Maximum number of bytes */
{
  int		bytes;			/* Bytes read */
  char		len[32];		/* Length string */


  DEBUG_printf(("httpRead(%08x, %08x, %d)\n", http, buffer, length));

  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (length <= 0)
    return (0);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED &&
      http->data_remaining <= 0 &&
      (http->state == HTTP_GET_SEND || http->state == HTTP_POST_RECV ||
       http->state == HTTP_POST_SEND || http->state == HTTP_PUT_RECV))
  {
    if (httpGets(len, sizeof(len), http) == NULL)
      return (0);

    http->data_remaining = strtol(len, NULL, 16);
  }

  DEBUG_printf(("httpRead: data_remaining = %d\n", http->data_remaining));

  if (http->data_remaining == 0)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are reading POST
    * data, go idle...
    */

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;

    return (0);
  }
  else if (length > http->data_remaining)
    length = http->data_remaining;

  if (http->used > 0)
  {
    if (length > http->used)
      length = http->used;

    bytes = length;

    DEBUG_printf(("httpRead: grabbing %d bytes from input buffer...\n", bytes));

    memcpy(buffer, http->buffer, length);
    http->used -= length;

    if (http->used > 0)
      memcpy(http->buffer, http->buffer + length, http->used);
  }
  else
  {
    DEBUG_printf(("httpRead: reading %d bytes from socket...\n", length));
    bytes = recv(http->fd, buffer, length, 0);
    DEBUG_printf(("httpRead: read %d bytes from socket...\n", bytes));
  }

  if (bytes > 0)
    http->data_remaining -= bytes;

  if (http->data_remaining == 0 && http->data_encoding != HTTP_ENCODE_CHUNKED)
  {
    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;
  }

  return (bytes);
}


/*
 * 'httpWrite()' - Write data to a HTTP connection.
 */
 
int					/* O - Number of bytes written */
httpWrite(http_t *http,			/* I - HTTP data */
          char   *buffer,		/* I - Buffer for data */
	  int    length)		/* I - Number of bytes to write */
{
  int		tbytes,			/* Total bytes sent */
		bytes;			/* Bytes sent */
  char		len[32];		/* Length string */


  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (http->data_encoding == HTTP_ENCODE_CHUNKED &&
      (http->state == HTTP_GET_SEND || http->state == HTTP_POST_RECV ||
       http->state == HTTP_POST_SEND || http->state == HTTP_PUT_RECV))
  {
    sprintf(len, "%x\r\n", length);
    if (send(http->fd, len, strlen(len), 0) < 3)
      return (-1);
  }

  if (length == 0)
  {
   /*
    * A zero-length chunk ends a transfer; unless we are sending POST
    * data, go idle...
    */

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;

    return (0);
  }

  tbytes = 0;

  while (length > 0)
  {
    bytes = send(http->fd, buffer, length, 0);
    if (bytes < 0)
    {
      DEBUG_puts("httpWrite: error writing data...\n");
      return (-1);
    }

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
    if (http->data_encoding == HTTP_ENCODE_LENGTH)
      http->data_remaining -= bytes;
  }

  if (http->data_remaining == 0 && http->data_encoding == HTTP_ENCODE_LENGTH)
  {
   /*
    * Finished with the transfer; unless we are sending POST data, go idle...
    */

    if (http->state == HTTP_POST_RECV)
      http->state ++;
    else
      http->state = HTTP_WAITING;
  }

  DEBUG_printf(("httpWrite: wrote %d bytes...\n", tbytes));

  return (tbytes);
}


/*
 * 'httpGets()' - Get a line of text from a HTTP connection.
 */

char *					/* O - Line or NULL */
httpGets(char   *line,			/* I - Line to read into */
         int    length,			/* I - Max length of buffer */
	 http_t *http)			/* I - HTTP data */
{
  char	*lineptr,			/* Pointer into line */
	*bufptr,			/* Pointer into input buffer */
	*bufend;			/* Pointer to end of buffer */
  int	bytes;				/* Number of bytes read */


  DEBUG_printf(("httpGets(%08x, %d, %08x)\n", line, length, http));

  if (http == NULL || line == NULL)
    return (NULL);

 /*
  * Pre-scan the buffer and see if there is a newline in there...
  */

  do
  {
    bufptr  = http->buffer;
    bufend  = http->buffer + http->used;

    while (bufptr < bufend)
      if (*bufptr == 0x0a)
	break;
      else
	bufptr ++;

    if (bufptr >= bufend)
    {
     /*
      * No newline; see if there is more data to be read...
      */

      if ((bytes = recv(http->fd, bufend, HTTP_MAX_BUFFER - http->used, 0)) < 1)
      {
       /*
	* Nope, can't get a line this time...
	*/

        return (NULL);
      }
      else
      {
       /*
	* Yup, update the amount used and the end pointer...
	*/

	http->used += bytes;
	bufend     += bytes;
      }
    }
  }
  while (bufptr >= bufend);

  http->activity = time(NULL);

 /*
  * Read a line from the buffer...
  */
    
  lineptr = line;
  bufptr  = http->buffer;
  bytes   = 0;

  while (bufptr < bufend && bytes < length)
  {
    bytes ++;

    if (*bufptr == 0x0a)
    {
      bufptr ++;
      *lineptr = '\0';

      http->used -= bytes;
      if (http->used > 0)
	memcpy(http->buffer, bufptr, http->used);

      return (line);
    }
    else if (*bufptr == 0x0d)
      bufptr ++;
    else
      *lineptr++ = *bufptr++;
  }

  return (NULL);
}


/*
 * 'httpPrintf()' - Print a formatted string to a HTTP connection.
 */

int					/* O - Number of bytes written */
httpPrintf(http_t     *http,		/* I - HTTP data */
           const char *format,		/* I - printf-style format string */
	   ...)				/* I - Additional args as needed */
{
  int		bytes;			/* Number of bytes to write */
  char		buf[HTTP_MAX_BUFFER];	/* Buffer for formatted string */
  va_list	ap;			/* Variable argument pointer */


  va_start(ap, format);
  bytes = vsprintf(buf, format, ap);
  va_end(ap);

  DEBUG_printf(("httpPrintf: %s", buf));

  return (send(http->fd, buf, bytes, 0));
}


/*
 * 'httpStatus()' - Return a short string describing a HTTP status code.
 */

char *					/* O - String or NULL */
httpStatus(http_status_t status)	/* I - HTTP status code */
{
  switch (status)
  {
    case HTTP_OK :
        return ("OK");
    case HTTP_CREATED :
        return ("Created");
    case HTTP_ACCEPTED :
        return ("Accepted");
    case HTTP_NO_CONTENT :
        return ("No Content");
    case HTTP_NOT_MODIFIED :
        return ("Not Modified");
    case HTTP_BAD_REQUEST :
        return ("Bad Request");
    case HTTP_UNAUTHORIZED :
        return ("Unauthorized");
    case HTTP_FORBIDDEN :
        return ("Forbidden");
    case HTTP_NOT_FOUND :
        return ("Not Found");
    case HTTP_REQUEST_TOO_LARGE :
        return ("Request Entity Too Large");
    case HTTP_URI_TOO_LONG :
        return ("URI Too Long");
    case HTTP_NOT_IMPLEMENTED :
        return ("Not Implemented");
    case HTTP_NOT_SUPPORTED :
        return ("Not Supported");
    default :
        return ("Unknown");
  }
}


/*
 * 'httpGetDateString()' - Get a formatted date/time string from a time value.
 */

char *					/* O - Date/time string */
httpGetDateString(time_t t)		/* I - UNIX time */
{
  struct tm	*tdate;
  static char	datetime[256];


  tdate = gmtime(&t);
  sprintf(datetime, "%s, %02d %s %d %02d:%02d:%02d GMT",
          days[tdate->tm_wday], tdate->tm_mday, months[tdate->tm_mon],
	  tdate->tm_year + 1900, tdate->tm_hour, tdate->tm_min, tdate->tm_sec);

  return (datetime);
}


/*
 * 'httpGetDateTime()' - Get a time value from a formatted date/time string.
 */

time_t					/* O - UNIX time */
httpGetDateTime(char *s)		/* I - Date/time string */
{
  int		i;			/* Looping var */
  struct tm	tdate;			/* Time/date structure */
  char		mon[16];		/* Abbreviated month name */
  int		day, year;		/* Day of month and year */
  int		hour, min, sec;		/* Time */


  if (sscanf(s, "%*s%d%s%d%d:%d:%d", &day, mon, &year, &hour, &min, &sec) < 6)
    return (0);

  for (i = 0; i < 12; i ++)
    if (strcasecmp(mon, months[i]) == 0)
      break;

  if (i >= 12)
    return (0);

  tdate.tm_mon   = i;
  tdate.tm_mday  = day;
  tdate.tm_year  = year - 1900;
  tdate.tm_hour  = hour;
  tdate.tm_min   = min;
  tdate.tm_sec   = sec;
  tdate.tm_isdst = 0;

  return (mktime(&tdate));
}


/*
 * 'httpUpdate()' - Update the current HTTP state for incoming data.
 */

http_status_t				/* O - HTTP status */
httpUpdate(http_t *http)		/* I - HTTP data */
{
  char		line[1024],		/* Line from connection... */
		*value;			/* Pointer to value on line */
  http_field_t	field;			/* Field index */
  int		major, minor;		/* HTTP version numbers */
  http_status_t	status;			/* Authorization status */


  DEBUG_printf(("httpUpdate(%08x)\n", http));

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
    DEBUG_puts(line);

    if (line[0] == '\0')
    {
     /*
      * Blank line means the start of the data section (if any).  Return
      * the result code, too...
      */

      httpGetLength(http);

      switch (http->state)
      {
        case HTTP_GET :
	case HTTP_POST :
	case HTTP_POST_RECV :
	case HTTP_PUT :
	    http->state ++;
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
      http->status  = status;
    }
    else if ((value = strchr(line, ':')) != NULL)
    {
     /*
      * Got a value...
      */

      *value++ = '\0';
      while (isspace(*value))
        value ++;

     /*
      * Be tolerants of servers that send unknown attribute fields...
      */

      if ((field = http_field(line)) == HTTP_FIELD_UNKNOWN)
      {
        DEBUG_printf(("httpUpdate: unknown field %s seen!\n", line));
        continue;
      }

      httpSetField(http, field, value);
    }
    else
      return (HTTP_ERROR);
  }

 /*
  * If we haven't already returned, then there is nothing new...
  */

  return (HTTP_CONTINUE);
}


/*
 * 'httpDecode64()' - Base64-decode a string.
 */

char *				/* O - Decoded string */
httpDecode64(char *out,		/* I - String to write to */
             char *in)		/* I - String to read from */
{
  int	pos,			/* Bit position */
	base64;			/* Value of this character */
  char	*outptr;		/* Output pointer */


  for (outptr = out, pos = 0; *in != '\0'; in ++)
  {
   /*
    * Decode this character into a number from 0 to 63...
    */

    if (*in >= 'A' && *in <= 'Z')
      base64 = *in - 'A';
    else if (*in >= 'a' && *in <= 'z')
      base64 = *in - 'a' + 26;
    else if (*in >= '0' && *in <= '9')
      base64 = *in - '0' + 52;
    else if (*in == '+')
      base64 = 62;
    else if (*in == '/')
      base64 = 63;
    else if (*in == '=')
      break;
    else
      continue;

   /*
    * Store the result in the appropriate chars...
    */

    switch (pos)
    {
      case 0 :
          *outptr = base64 << 2;
	  pos ++;
	  break;
      case 1 :
          *outptr++ |= (base64 >> 4) & 3;
	  *outptr = (base64 << 4) & 255;
	  pos ++;
	  break;
      case 2 :
          *outptr++ |= (base64 >> 2) & 15;
	  *outptr = (base64 << 6) & 255;
	  pos ++;
	  break;
      case 3 :
          *outptr++ |= base64;
	  pos = 0;
	  break;
    }
  }

  *outptr = '\0';

 /*
  * Return the decoded string...
  */

  return (out);
}


/*
 * 'httpEncode64()' - Base64-encode a string.
 */

char *				/* O - Encoded string */
httpEncode64(char *out,		/* I - String to write to */
             char *in)		/* I - String to read from */
{
  char		*outptr;	/* Output pointer */
  static char	base64[] =	/* Base64 characters... */
  		{
		  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		  "abcdefghijklmnopqrstuvwxyz"
		  "0123456789"
		  "+/"
  		};


  for (outptr = out; *in != '\0'; in ++)
  {
   /*
    * Encode the up to 3 characters as 4 Base64 numbers...
    */

    *outptr ++ = base64[in[0] >> 2];
    *outptr ++ = base64[((in[0] << 4) | (in[1] >> 4)) & 63];

    in ++;
    if (*in == '\0')
      break;

    *outptr ++ = base64[((in[0] << 2) | (in[1] >> 6)) & 63];

    in ++;
    if (*in == '\0')
      break;

    *outptr ++ = base64[in[0] & 63];
  }

  *outptr ++ = '=';
  *outptr = '\0';

 /*
  * Return the encoded string...
  */

  return (out);
}


/*
 * 'httpGetLength()' - Get the amount of data remaining from the
 *                     content-length or transfer-encoding fields.
 */

int
httpGetLength(http_t *http)	/* I - HTTP data */
{
  if (strcasecmp(http->fields[HTTP_FIELD_TRANSFER_ENCODING], "chunked") == 0)
  {
    http->data_encoding  = HTTP_ENCODE_CHUNKED;
    http->data_remaining = 0;
  }
  else
  {
    http->data_encoding  = HTTP_ENCODE_LENGTH;

   /*
    * The following is a hack for HTTP servers that don't send a
    * content-length or transfer-encoding field...
    *
    * If there is no content-length then the connection must close
    * after the transfer is complete...
    */

    if (http->fields[HTTP_FIELD_CONTENT_LENGTH][0] == '\0')
      http->data_remaining = 2147483647;
    else
      http->data_remaining = atoi(http->fields[HTTP_FIELD_CONTENT_LENGTH]);
  }

  return (http->data_remaining);
}


/*
 * 'http_field()' - Return the field index for a field name.
 */

static http_field_t		/* O - Field index */
http_field(char *name)		/* I - String name */
{
  int	i;			/* Looping var */


  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (strcasecmp(name, http_fields[i]) == 0)
      return ((http_field_t)i);

  return (HTTP_FIELD_UNKNOWN);
}


/*
 * 'http_send()' - Send a request with all fields and the trailing blank line.
 */

static int			/* O - 0 on success, non-zero on error */
http_send(http_t       *http,	/* I - HTTP data */
          http_state_t request,	/* I - Request code */
	  char         *uri)	/* I - URI */
{
  int		i;		/* Looping var */
  char		*ptr,		/* Pointer in buffer */
		buf[1024];	/* Encoded URI buffer */
  static char	*codes[] =	/* Request code strings */
		{
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
  static char	*hex = "0123456789ABCDEF";
				/* Hex digits */


  if (http == NULL || uri == NULL)
    return (-1);

 /*
  * Encode the URI as needed...
  */

  for (ptr = buf; *uri != '\0'; uri ++)
    if (*uri <= ' ' || *uri >= 127)
    {
      *ptr ++ = '%';
      *ptr ++ = hex[(*uri >> 4) & 15];
      *ptr ++ = hex[*uri & 15];
    }
    else
      *ptr ++ = *uri;

  *ptr = '\0';

 /*
  * Send the request header...
  */

  http->state = request;
  if (request == HTTP_POST || request == HTTP_PUT)
    http->state ++;

  if (httpPrintf(http, "%s %s HTTP/1.1\n", codes[request], buf) < 1)
  {
   /*
    * Might have lost connection; try to reconnect...
    */

    if (httpReconnect(http))
      return (-1);

   /*
    * OK, we've reconnected, send the request again...
    */

    if (httpPrintf(http, "%s %s HTTP/%d.%d\n", codes[request], buf,
                   http->version / 100, http->version % 100) < 1)
      return (-1);
  }

  for (i = 0; i < HTTP_FIELD_MAX; i ++)
    if (http->fields[i][0] != '\0')
    {
      DEBUG_printf(("%s: %s\n", http_fields[i], http->fields[i]));

      if (httpPrintf(http, "%s: %s\n", http_fields[i], http->fields[i]) < 1)
        return (-1);
    }

  if (httpPrintf(http, "\n") < 1)
    return (-1);

  httpClearFields(http);

  return (0);
}


/*
 * End of "$Id: http.c,v 1.35 1999/05/17 18:03:31 mike Exp $".
 */
