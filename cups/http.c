/*
 * "$Id: http.c,v 1.9 1999/01/28 22:00:44 mike Exp $"
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
 *   httpRead()          - Read data from a HTTP connection.
 *   httpWrite()         - Write data to a HTTP connection.
 *   httpGets()          - Get a line of text from a HTTP connection.
 *   httpPrintf()        - Print a formatted string to a HTTP connection.
 *   httpStatus()        - Return a short string describing a HTTP status code.
 *   httpLongStatus()    - Return a long string describing a HTTP status code.
 *   httpGetDateString() - Get a formatted date/time string from a time value.
 *   httpGetDateTime()   - Get a time value from a formatted date/time string.
 *   httpUpdate()        - Update the current HTTP state for incoming data.
 *   httpDecode64()      - Base64-decode a string.
 *   httpEncode64()      - Base64-encode a string.
 *   http_field()        - Return the field index for a field name.
 *   http_sighandler()   - Handle broken pipe signals from lost network
 *                         clients.
 */

/*
 * Include necessary headers...
 */

#include "http.h"
#include <stdarg.h>


/*
 * Local functions...
 */

static http_field_t	http_field(char *name);
static void		http_sighandler(int sig);


/*
 * Local globals...
 */

static char		http_proxyhost[HTTP_MAX_URI] = "";
static int		http_proxyport = 0;
static char		*http_fields[] =
			{
			  "Accept",
			  "Accept-Charset",
			  "Accept-Encoding",
			  "Accept-Language",
			  "Age",
			  "Allow",
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
			  "Date",
			  "Etag",
			  "Expires",
			  "From",
			  "Host",
			  "If-Match",
			  "If-Modified-Since",
			  "If-None-Match",
			  "If-Range",
			  "If-Unmodified-since",
			  "Last-Modified",
			  "Location",
			  "Max-Forwards",
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
httpInitialize(char *proxyhost,	/* I - Proxy hostname */
               int  port)	/* I - Port to connect to */
{
#ifdef WIN32
  WSADATA	winsockdata;	/* WinSock data */
  static int	initialized = 0;/* Has WinSock been initialized? */


  if (!initialized)
    WSAStartup(MAKEWORD(1,1), &winsockdata);
#endif /* WIN32 */

  if (proxyhost != NULL)
    strcpy(http_proxyhost, proxyhost);
  http_proxyport = port;
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


 /*
  * Lookup the host...
  */

  if (http_proxyhost[0] != '\0')
    hostaddr = gethostbyname(http_proxyhost);
  else
    hostaddr = gethostbyname(host);

  if (hostaddr == NULL)
    return (NULL);

 /*
  * Allocate memory for the structure...
  */

  http = calloc(sizeof(http_t), 1);
  if (http == NULL)
    return (NULL);

  http->activity = time(NULL);

 /*
  * Copy the hostname and port and then "reconnect"...
  */

  strcpy(http->hostname, host);
  http->hostlength = strlen(host);
  http->hostport   = port;

  memcpy((char *)&(http->hostaddr), hostaddr->h_addr, hostaddr->h_length);
  http->hostaddr.sin_family = hostaddr->h_addrtype;

  if (http_proxyhost[0] != '\0')
    http->hostaddr.sin_port = htons(http_proxyport);
  else
    http->hostaddr.sin_port = htons(port);

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

  if ((http->fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) < 0)
    return (-1);

#ifdef FD_CLOEXEC
  fcntl(http->fd, F_SETFD, FD_CLOEXEC);	/* Close this socket when starting *
					 * other processes...              */
#endif /* FD_CLOEXEC */

  val = 1;
  setsockopt(http->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

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
  };

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
      port == NULL || resource)
    return;

 /*
  * Grab the method portion of the URI...
  */

  ptr = method;
  while (*uri != ':' && *uri != '\0')
    *ptr ++ = *uri ++;

  *ptr = '\0';
  if (*uri == ':')
    uri ++;

 /*
  * If the method contains a period or slash, then it's probably a
  * filename...
  */

  if (strchr(method, '.') != NULL || strchr(method, '/') != NULL)
  {
    strcpy(resource, method);
    strcpy(method, "file");
    username[0] = '\0';
    host[0]     = '\0';
    *port       = 0;
    return;
  }

 /*
  * If the method is "file" then copy the rest as a filename and
  * return...
  */

  if (strcasecmp(method, "file") == 0)
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
      *port = 631;
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
}


/*
 * 'httpGet()' - Send a GET request to the server.
 */

int					/* O - Status of call (0 = success) */
httpGet(http_t *http,			/* I - HTTP data */
        char   *uri)			/* I - URI to get */
{
}


/*
 * 'httpHead()' - Send a HEAD request to the server.
 */

int					/* O - Status of call (0 = success) */
httpHead(http_t *http,			/* I - HTTP data */
         char   *uri)			/* I - URI for head */
{
}


/*
 * 'httpOptions()' - Send an OPTIONS request to the server.
 */

int					/* O - Status of call (0 = success) */
httpOptions(http_t *http,		/* I - HTTP data */
            char   *uri)		/* I - URI for options */
{
}


/*
 * 'httpPost()' - Send a POST request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPost(http_t *http,			/* I - HTTP data */
         char   *uri)			/* I - URI for post */
{
}


/*
 * 'httpPut()' - Send a PUT request to the server.
 */

int					/* O - Status of call (0 = success) */
httpPut(http_t *http,			/* I - HTTP data */
        char   *uri)			/* I - URI to put */
{
}


/*
 * 'httpTrace()' - Send an TRACE request to the server.
 */

int					/* O - Status of call (0 = success) */
httpTrace(http_t *http,			/* I - HTTP data */
          char   *uri)			/* I - URI for trace */
{
}


/*
 * 'httpRead()' - Read data from a HTTP connection.
 */

int					/* O - Number of bytes read */
httpRead(http_t *http,			/* I - HTTP data */
         char   *buffer,		/* I - Buffer for data */
	 int    length)			/* I - Maximum number of bytes */
{
  int		tbytes,			/* Total bytes read */
		bytes;			/* Bytes read */
  char		len[32];		/* Length string */


  if (http == NULL || buffer == NULL)
    return (-1);

  http->activity = time(NULL);

  if (length <= 0)
    return (0);

  if (http->version == HTTP_1_1 && http->data_remaining <= 0 &&
      (http->state == HTTP_GET_SEND || http->state == HTTP_POST_RECV ||
       http->state == HTTP_POST_SEND || http->state == HTTP_PUT_RECV))
  {
    if (httpGets(len, sizeof(len), http) == NULL)
      return (0);

    http->data_remaining = atoi(len);
  }

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

  tbytes = 0;
  while (length > 0)
  {
    bytes = recv(http->fd, buffer, length, MSG_DONTWAIT);
    if (bytes < 0)
      return (-1);
    else if (bytes == 0)
      break;

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
  }

  return (tbytes);
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

  if (http->version == HTTP_1_1 &&
      (http->state == HTTP_GET_SEND || http->state == HTTP_POST_RECV ||
       http->state == HTTP_POST_SEND || http->state == HTTP_PUT_RECV))
  {
    sprintf(len, "%d\r\n", length);
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
      return (-1);

    buffer += bytes;
    tbytes += bytes;
    length -= bytes;
  }

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


  if (http == NULL || line == NULL)
    return (NULL);

 /*
  * Pre-scan the buffer and see if there is a newline in there...
  */

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

    if ((bytes = recv(http->fd, bufend, HTTP_MAX_BUFFER - http->used,
                      MSG_DONTWAIT)) < 1)
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
      bufend        += bytes;
    }
  }

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

  return (httpWrite(http, buf, bytes));
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
 * 'httpLongStatus()' - Return a long string describing a HTTP status code.
 */

char *					/* O - String or NULL */
httpLongStatus(http_status_t status)	/* I - HTTP status code */
{
  switch (status)
  {
    case HTTP_BAD_REQUEST :
        return ("The server reported that a bad or incomplete request was received.");
    case HTTP_UNAUTHORIZED :
        return ("You must provide a valid username and password to access this page.");
    case HTTP_FORBIDDEN :
        return ("You are not allowed to access this page.");
    case HTTP_NOT_FOUND :
        return ("The specified file or directory was not found.");
    case HTTP_REQUEST_TOO_LARGE :
        return ("The server reported that the request is too large.");
    case HTTP_URI_TOO_LONG :
        return ("The server reported that the URI is too long.");
    case HTTP_NOT_IMPLEMENTED :
        return ("That feature is not implemented");
    case HTTP_NOT_SUPPORTED :
        return ("That feature is not supported");
    default :
        return ("An unknown error occurred.");
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
  strftime(datetime, sizeof(datetime) - 1, "%a, %d %h %Y %T GMT", tdate);

  return (datetime);
}


/*
 * 'httpGetDateTime()' - Get a time value from a formatted date/time string.
 */

time_t					/* O - UNIX time */
httpGetDateTime(char *s)		/* I - Date/time string */
{
}


/*
 * 'httpUpdate()' - Update the current HTTP state for incoming data.
 */

int					/* O - 0 if nothing happened */
httpUpdate(http_t *http)		/* I - HTTP data */
{
#if 0
  char		line[1024],		/* Line from socket... */
		name[256],		/* Name on request line */
		value[1024],		/* Value on request line */
		version[64],		/* Protocol version on request line */
		*valptr;		/* Pointer to value */
  int		major, minor;		/* HTTP version numbers */
  int		start;			/* TRUE if we need to start the transfer */
  http_status_t	status;			/* Authorization status */
  int		bytes;			/* Number of bytes to POST */
  char		*filename,		/* Name of file for GET/HEAD */
		*extension,		/* Extension of file */
		*type;			/* MIME type */
  struct stat	filestats;		/* File information */


  start = 0;

  switch (http->state)
  {
    case HTTP_WAITING :
       /*
        * See if we've received a request line...
	*/

        if (get_line(con, line, sizeof(line) - 1) == NULL)
	  break;

       /*
        * Ignore blank request lines...
	*/

        if (line[0] == '\0')
	  break;

       /*
        * Clear other state variables...
	*/

        http->activity        = time(NULL);
        http->version         = HTTP_1_0;
	http->keep_alive      = 0;
	http->data_encoding   = HTTP_DATA_SINGLE;
	http->data_length     = 0;
	http->file            = 0;
	http->pipe_pid        = 0;
        http->host[0]         = '\0';
	http->user_agent[0]   = '\0';
	http->username[0]     = '\0';
	http->password[0]     = '\0';
	http->uri[0]          = '\0';
	http->content_type[0] = '\0';
	http->remote_time     = 0;
	http->remote_size     = 0;

	strcpy(http->language, "en");

       /*
        * Grab the request line...
	*/

        switch (sscanf(line, "%255s%1023s%s", name, value, version))
	{
	  case 1 :
	      SendError(con, HTTP_BAD_REQUEST);
	      CloseClient(con);
	      return (0);
	  case 2 :
	      http->version = HTTP_0_9;
	      break;
	  case 3 :
	      sscanf(version, "HTTP/%d.%d", &major, &minor);

	      if (major == 1 && minor == 1)
	      {
	        http->version    = HTTP_1_1;
		http->keep_alive = 1;
	      }
	      else if (major == 1 && minor == 0)
	        http->version = HTTP_1_0;
	      else if (major == 0 && minor == 9)
	        http->version = HTTP_0_9;
	      else
	      {
	        SendError(con, HTTP_NOT_SUPPORTED);
	        CloseClient(con);
	        return (0);
	      };
	      break;
	}

       /*
        * Copy the request URI...
	*/

        if (strncmp(value, "http://", 7) == 0)
	{
	  if ((valptr = strchr(value + 7, '/')) == NULL)
	    valptr = "/";

	  strcpy(http->uri, valptr);
	}
	else
	  strcpy(http->uri, value);

       /*
        * Process the request...
	*/

        if (strcmp(name, "GET") == 0)
	{
	  http->state = HTTP_GET;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "PUT") == 0)
	{
	  http->state = HTTP_PUT;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "POST") == 0)
	{
	  http->state = HTTP_POST;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "DELETE") == 0)
	{
	  http->state = HTTP_DELETE;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "TRACE") == 0)
	{
	  http->state = HTTP_TRACE;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "CLOSE") == 0)
	{
	  http->state = HTTP_CLOSE;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "OPTIONS") == 0)
	{
	  http->state = HTTP_OPTIONS;
	  start      = (http->version == HTTP_0_9);
	}
        else if (strcmp(name, "HEAD") == 0)
	{
	  http->state = HTTP_HEAD;
	  start      = (http->version == HTTP_0_9);
	}
	else
	{
	  SendError(con, HTTP_BAD_REQUEST);
	  CloseClient(con);
	}
        break;

    case HTTP_GET :
    case HTTP_PUT :
    case HTTP_POST :
    case HTTP_DELETE :
    case HTTP_TRACE :
    case HTTP_CLOSE :
    case HTTP_HEAD :
       /*
        * See if we've received a request line...
	*/

        if (get_line(con, line, sizeof(line) - 1) == NULL)
	  break;

       /*
        * A blank request line starts the transfer...
	*/

        if (line[0] == '\0')
	{
	  fputs("cupsd: START\n", stderr);
	  start = 1;
	  break;
	}

       /*
        * Grab the name:value line...
        */

        if (sscanf(line, "%255[^:]:%1023s", name, value) < 2)
	{
	  SendError(con, HTTP_BAD_REQUEST);
	  CloseClient(con);
	  return (0);
	}

       /*
	* Copy the parameters that we need...
	*/

	if (strcasecmp(name, "Content-type") == 0)
	{
	  strncpy(http->content_type, value, sizeof(http->content_type) - 1);
	  http->content_type[sizeof(http->content_type) - 1] = '\0';
	}
	else if (strcasecmp(name, "Content-length") == 0)
	{
	  http->data_encoding  = HTTP_DATA_SINGLE;
	  http->data_length    = atoi(value);
	  http->data_remaining = http->data_length;
	}
	else if (strcasecmp(name, "Accept-Language") == 0)
	{
	  strncpy(http->language, value, sizeof(http->language) - 1);
	  http->language[sizeof(http->language) - 1] = '\0';

         /*
	  * Strip trailing data in language string...
	  */

	  for (valptr = http->language; *valptr != '\0'; valptr ++)
	    if (!isalnum(*valptr) && *valptr != '-')
	    {
	      *valptr = '\0';
	      break;
	    }
	}
	else if (strcasecmp(name, "Authorization") == 0)
	{
	 /*
	  * Get the authorization string...
	  */

          valptr = strstr(line, value);
	  valptr += strlen(value);
	  while (*valptr == ' ' || *valptr == '\t')
	    valptr ++;

	 /*
	  * Decode the string as needed...
	  */

	  if (strcmp(value, "Basic") == 0)
	    decode_basic_auth(con, valptr);
	  else
	  {
	    SendError(con, HTTP_NOT_IMPLEMENTED);
	    CloseClient(con);
	    return (0);
	  }
	}
	else if (strcasecmp(name, "Transfer-Encoding") == 0)
	{
	  if (strcasecmp(value, "chunked") == 0)
	  {
	    http->data_encoding = HTTP_DATA_CHUNKED;
	    http->data_length   = 0;
          }
	  else
	  {
	    SendError(con, HTTP_NOT_IMPLEMENTED);
	    CloseClient(con);
	    return (0);
	  }
	}
	else if (strcasecmp(name, "User-Agent") == 0)
	{
	  strncpy(http->user_agent, value, sizeof(http->user_agent) - 1);
	  http->user_agent[sizeof(http->user_agent) - 1] = '\0';
	}
	else if (strcasecmp(name, "Host") == 0)
	{
	  strncpy(http->host, value, sizeof(http->host) - 1);
	  http->host[sizeof(http->host) - 1] = '\0';
	}
	else if (strcasecmp(name, "Connection") == 0)
	{
	  if (strcmp(value, "Keep-Alive") == 0)
	    http->keep_alive = 1;
	}
	else if (strcasecmp(name, "If-Modified-Since") == 0)
	{
	  valptr = strchr(line, ':') + 1;
	  while (*valptr == ' ' || *valptr == '\t')
	    valptr ++;
	  
	  decode_if_modified(con, valptr);
	}
	break;
  }

 /*
  * Handle new transfers...
  */

  if (start)
  {
    if (http->host[0] == '\0' && http->version >= HTTP_1_0)
    {
      if (!SendError(con, HTTP_BAD_REQUEST))
      {
	CloseClient(con);
	return (0);
      }
    }
    else if ((status = IsAuthorized(con)) != HTTP_OK)
    {
      if (!SendError(con, status))
      {
	CloseClient(con);
        return (0);
      }
    }
    else if (strncmp(http->uri, "..", 2) == 0)
    {
     /*
      * Protect against malicious users!
      */

      if (!SendError(con, HTTP_FORBIDDEN))
      {
	CloseClient(con);
        return (0);
      }
    }
    else switch (http->state)
    {
      case HTTP_GET :
	  if (strncmp(http->uri, "/printers", 9) == 0)
	  {
	   /*
	    * Show printer status...
	    */

            if (!show_printer_status(con))
	    {
	      if (!SendError(con, HTTP_NOT_FOUND))
	      {
	        CloseClient(con);
		return (0);
	      }
            }

            http->state = HTTP_WAITING;

	    if (http->data_length == 0 &&
	        http->data_encoding == HTTP_DATA_SINGLE &&
		http->version <= HTTP_1_0)
	      http->keep_alive = 0;
	  }
	  else
	  {
	   /*
	    * Serve a file...
	    */

            if ((filename = get_file(con, &filestats)) == NULL)
	    {
	      if (!SendError(con, HTTP_NOT_FOUND))
	      {
	        CloseClient(con);
		return (0);
	      }
	    }
	    else if (filestats.st_size == http->remote_size &&
	             filestats.st_mtime == http->remote_time)
            {
              if (!SendError(con, HTTP_NOT_MODIFIED))
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	    else
            {
	      extension = get_extension(filename);
	      type      = get_type(extension);

              if (!SendFile(con, HTTP_OK, filename, type, &filestats))
	      {
		CloseClient(con);
		return (0);
	      }

              http->state = HTTP_GET_DATA;
	    }
	  }
          break;

      case HTTP_POST :
          sprintf(http->filename, "%s/requests/XXXXXX", ServerRoot);
	  http->file = mkstemp(http->filename);

          fprintf(stderr, "cupsd: POST %s, http->file = %d...\n", http->filename,
	          http->file);

	  if (http->file < 0)
	  {
	    if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	    {
	      CloseClient(con);
	      return (0);
	    }
	  }
	  else
	    http->state = HTTP_POST_DATA;
	  break;

      case HTTP_PUT :
      case HTTP_DELETE :
      case HTTP_TRACE :
          SendError(con, HTTP_NOT_IMPLEMENTED);

      case HTTP_CLOSE :
          CloseClient(con);
	  return (0);

      case HTTP_HEAD :
	  if (strncmp(http->uri, "/printers/", 10) == 0)
	  {
	   /*
	    * Do a command...
	    */

            if (!SendHeader(con, HTTP_OK, "text/plain"))
	    {
	      CloseClient(con);
	      return (0);
	    }

	    if (conprintf(con, "\r\n") < 0)
	    {
	      CloseClient(con);
	      return (0);
	    }
	  }
	  else if (filestats.st_size == http->remote_size &&
	           filestats.st_mtime == http->remote_time)
          {
            if (!SendError(con, HTTP_NOT_MODIFIED))
	    {
              CloseClient(con);
	      return (0);
	    }
	  }
	  else
	  {
	   /*
	    * Serve a file...
	    */

            if ((filename = get_file(con, &filestats)) == NULL)
	    {
	      if (!SendHeader(con, HTTP_NOT_FOUND, "text/html"))
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	    else
            {
	      extension = get_extension(filename);
	      type      = get_type(extension);

              if (!SendHeader(con, HTTP_OK, type))
	      {
		CloseClient(con);
		return (0);
	      }

	      if (conprintf(con, "Last-Modified: %s\r\n",
	                    get_datetime(filestats.st_mtime)) < 0)
	      {
		CloseClient(con);
		return (0);
	      }

	      if (conprintf(con, "Content-Length: %d\r\n", filestats.st_size) < 0)
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	  }

          if (conprintf(con, "\r\n") < 0)
	  {
	    CloseClient(con);
	    return (0);
	  }

          http->state = HTTP_WAITING;
          break;
    }
  }

 /*
  * Handle any incoming data...
  */

  switch (http->state)
  {
    case HTTP_PUT_DATA :
        break;

    case HTTP_POST_DATA :
        printf("cupsd: http->data_encoding = %s, http->data_length = %d...\n",
	       http->data_encoding == HTTP_DATA_CHUNKED ? "chunked" : "single",
	       http->data_length);

        if (http->data_encoding == HTTP_DATA_CHUNKED &&
	    http->data_remaining == 0)
	{
          if (get_line(con, line, sizeof(line) - 1) == NULL)
	    break;

	  http->data_remaining = atoi(line);
	  http->data_length    += http->data_remaining;
	  if (http->data_remaining == 0)
	    http->data_encoding = HTTP_DATA_SINGLE;
	}

        if (http->data_remaining > 0)
	{
	  if (http->data_remaining > http->used)
	    bytes = http->used;
	  else 
	    bytes = http->data_remaining;

          fprintf(stderr, "cupsd: Writing %d bytes to temp file...\n", bytes);

          write(http->file, http->buffer, bytes);

	  http->used        -= bytes;
	  http->data_remaining -= bytes;

	  if (http->used > 0)
	    memcpy(http->buffer, http->buffer + bytes, http->used);
	}

	if (http->data_remaining == 0 &&
	    http->data_encoding == HTTP_DATA_SINGLE)
	{
	  close(http->file);
	  
          if (!SendError(con, HTTP_ACCEPTED))
	  {
	    CloseClient(con);
	    return (0);
	  }
	}
        break;
  }

  if (!http->keep_alive && http->state == HTTP_WAITING)
  {
    CloseClient(con);
    return (0);
  }
  else
    return (1);
#endif /* 0 */
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
 * 'http_sighandler()' - Handle 'broken pipe' signals from lost network
 *                       clients.
 */

static void
http_sighandler(int sig)	/* I - Signal number */
{
/* IGNORE */
}


/*
 * End of "$Id: http.c,v 1.9 1999/01/28 22:00:44 mike Exp $".
 */
