/*
 * "$Id: http.c,v 1.7 1999/01/24 14:18:43 mike Exp $"
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
 *   AcceptClient()       - Accept a new client.
 *   CloseAllClients()    - Close all remote clients immediately.
 *   CloseClient()        - Close a remote client.
 *   ReadClient()         - Read data from a client.
 *   SendCommand()        - Send output from a command via HTTP.
 *   SendError()          - Send an error message via HTTP.
 *   SendFile()           - Send a file via HTTP.
 *   SendHeader()         - Send an HTTP header.
 *   StartListening()     - Create all listening sockets...
 *   StopListening()      - Close all listening sockets...
 *   WriteClient()        - Write data to a client as needed.
 *   chunkprintf()        - Do a printf() to a client (chunked)...
 *   conprintf()          - Do a printf() to a client...
 *   destatus_basic_auth()  - Destatus a Basic authorization string.
 *   destatus_digest_auth() - Destatus an MD5 Digest authorization string.
 *   get_datetime()       - Get a data/time string for the given time.
 *   get_extension()      - Get the extension for a filename.
 *   get_file()           - Get a filename and state info.
 *   get_line()           - Get a request line terminated with a CR and LF.
 *   get_message()        - Get a message string for the given HTTP status.
 *   get_type()           - Get MIME type from the given extension.
 *   sigpipe_handler()    - Handle 'broken pipe' signals from lost network
 *                          clients.
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
static void		http_basic_auth(http_t *http, char *line);
static void		http_if_modified(http_t *http, char *line);
static char		*http_get_line(http_t *http, char *line, int length);
static void		http_sighandler(int sig);


/*
 * Local globals...
 */

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
 * 'httpAccept()' - Accept a HTTP connection on the given socket.
 */

http_t *		/* O - New HTTP connection */
httpAccept(int fd)	/* I - Socket to accept() */
{
  http_t	*http;	/* New HTTP connection */
  int		val;	/* Parameter value */


 /*
  * Allocate memory for the structure...
  */

  http = calloc(sizeof(http_t), 1);
  if (http == NULL)
    return (NULL);

  http->activity = time(NULL);

 /*
  * Accept the client and get the remote address...
  */

  val = sizeof(struct sockaddr_in);

  if ((http->fd = accept(fd, (struct sockaddr *)&(http->hostaddr), &val)) < 0)
  {
    free(http);
    return (NULL);
  }

  return (http);
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
}


int
httpReconnect(http_t *http)
{
}


void
httpSeparate(char *uri,		/* I - Universal Resource Identifier */
             char *method,	/* O - Method (http, https, etc.) */
	     char *host,	/* O - Hostname */
	     int  *port,	/* O - Port number to use */
             char *resource)	/* O - Resource/filename */
{
}


void
httpSetField(http_t       *http,
             http_field_t field,
	     char         *value)
{
  strncpy(http->fields[field], value, HTTP_MAX_VALUE - 1);
  http->fields[field][HTTP_MAX_VALUE - 1] = '\0';
}


int
httpDelete(http_t *http,
           char   *uri)
{
}


int
httpGet(http_t *http,
        char   *uri)
{
}


int
httpHead(http_t *http,
         char   *uri)
{
}


int
httpOptions(http_t *http,
            char   *uri)
{
}


int
httpPost(http_t *http,
         char   *uri)
{
}


int
httpPut(http_t *http,
        char   *uri)
{
}


int
httpTrace(http_t *http,
          char   *uri)
{
}


int
httpRead(http_t *http,
         char   *buffer,
	 int    length)
{
}


int
httpWrite(http_t *http,
          char   *buffer,
	  int    length)
{
}


char *
httpGets(char   *buffer,
         int    length,
	 http_t *http)
{
}


int
httpPrintf(http_t     *http,
           const char *format,
	   ...)
{
}


int
httpChunkf(http_t     *http,
           const char *format,
	   ...)
{
}


char *
httpStatus(http_status_t status)
{
}


char *
httpLongStatus(http_status_t status)
{
}


char *
httpGetDateString(time_t t)
{
}


time_t
httpGetDateTime(char *s)
{
}


int
httpUpdate(http_t *http)
{
}


/*
 * 'AcceptClient()' - Accept a new client.
 */

void
AcceptClient(listener_t *lis)	/* I - Listener socket */
{
}


/*
 * 'CloseAllClients()' - Close all remote clients immediately.
 */

void
CloseAllClients(void)
{
  while (NumClients > 0)
    CloseClient(Clients);
}


/*
 * 'CloseClient()' - Close a remote client.
 */

void
CloseClient(http_t *http)	/* I - Client to close */
{
  int	i;			/* Looping var */
  int	status;			/* Exit status of pipe command */


  fprintf(stderr, "cupsd: Closed client #%d\n", http->fd);

 /*
  * Close the socket and clear the file from the input set for select()...
  */

#ifdef WIN32		/* Windows doesn't have a unified IO system... */
  closesocket(http->fd);
#else
  close(http->fd);
#endif /* WIN32 */

  for (i = 0; i < NumListeners; i ++)
    FD_SET(Listeners[i].fd, &InputSet);

  FD_CLR(http->fd, &InputSet);
  if (http->pipe_pid != 0)
    FD_CLR(http->file, &InputSet);
  FD_CLR(http->fd, &OutputSet);

 /*
  * If we have a data file open, close it...
  */

  if (http->file > 0)
  {
    if (http->pipe_pid)
    {
      kill(http->pipe_pid, SIGKILL);
      waitpid(http->pipe_pid, &status, WNOHANG);
    }

    close(http->file);
  };

 /*
  * Compact the list of clients as necessary...
  */

  NumClients --;

  if (con < (Clients + NumClients))
    memcpy(con, con + 1, (Clients + NumClients - con) * sizeof(http_t));
}


/*
 * 'ReadClient()' - Read data from a client.
 */

int				/* O - 1 on success, 0 on error */
ReadClient(http_t *http)	/* I - Client to read from */
{
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
	  * Destatus the string as needed...
	  */

	  if (strcmp(value, "Basic") == 0)
	    destatus_basic_auth(con, valptr);
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
	  
	  destatus_if_modified(con, valptr);
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
	  if (http->data_remaining > http->bufused)
	    bytes = http->bufused;
	  else 
	    bytes = http->data_remaining;

          fprintf(stderr, "cupsd: Writing %d bytes to temp file...\n", bytes);

          write(http->file, http->buf, bytes);

	  http->bufused        -= bytes;
	  http->data_remaining -= bytes;

	  if (http->bufused > 0)
	    memcpy(http->buf, http->buf + bytes, http->bufused);
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
}


/*
 * 'SendCGI()' - Launch a CGI script...
 */

int				/* O - 1 on success, 0 on failure */
SendCGI(http_t *http)		/* I - Connection to use */
{
 /**** Insert pipe status - need to read data from request file, and then
       set state to HTTP_POST_RESPONSE ****/

 /**** When program is done need to remove temp file and so forth ****/
 /**** Don't forget to put CONTENT_TYPE and REQUEST_METHOD... ****/
}


/*
 * 'SendCommand()' - Send output from a command via HTTP.
 */

int
SendCommand(http_t    *http,
            http_status_t status,
	    char        *command,
	    char        *type)
{
  http->pipe_pid = pipe_command(0, &(http->file), command);

  if (http->pipe_pid == 0)
    return (0);

  fcntl(http->file, F_SETFD, fcntl(http->file, F_GETFD) | FD_CLOEXEC);

  FD_SET(http->file, &InputSet);
  FD_SET(http->fd, &OutputSet);

  if (!SendHeader(con, HTTP_OK, type))
    return (0);

  if (http->version == HTTP_1_1)
  {
    http->data_encoding = HTTP_DATA_CHUNKED;

    if (conprintf(con, "Transfer-Encoding: chunked\r\n") < 0)
      return (0);
  }

  if (conprintf(con, "\r\n") < 0)
    return (0);

  return (1);
}


/*
 * 'SendError()' - Send an error message via HTTP.
 */

int				/* O - 1 if successful, 0 otherwise */
SendError(http_t    *http,	/* I - Connection */
          http_status_t status)	/* I - Error status */
{
  char	message[1024];		/* Text version of error status */


 /*
  * To work around bugs in some proxies, don't use Keep-Alive for some
  * error messages...
  */

  if (status >= HTTP_BAD_REQUEST)
    http->keep_alive = 0;

 /*
  * Send an error message back to the client.  If the error status is a
  * 400 or 500 series, make sure the message contains some text, too!
  */

  if (!SendHeader(con, status, NULL))
    return (0);

  if (status == HTTP_UNAUTHORIZED)
  {
    if (conprintf(con, "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
      return (0);
  }

  if (http->version >= HTTP_1_1 && !http->keep_alive)
  {
    if (conprintf(con, "Connection: close\r\n") < 0)
      return (0);
  }

  if (status >= HTTP_BAD_REQUEST)
  {
   /*
    * Send a human-readable error message.
    */

    sprintf(message, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>"
                     "<BODY><H1>%s</H1>%s</BODY></HTML>\n",
            status, get_message(status), get_message(status),
	    get_long_message(status));

    if (conprintf(con, "Content-Type: text/html\r\n") < 0)
      return (0);
    if (conprintf(con, "Content-Length: %d\r\n", strlen(message)) < 0)
      return (0);
    if (conprintf(con, "\r\n") < 0)
      return (0);
    if (send(http->fd, message, strlen(message), 0) < 0)
      return (0);
  }
  else if (conprintf(con, "\r\n") < 0)
    return (0);

  http->state = HTTP_WAITING;

  return (1);
}


/*
 * 'SendFile()' - Send a file via HTTP.
 */

int
SendFile(http_t    *http,
         http_status_t status,
	 char        *filename,
	 char        *type,
	 struct stat *filestats)
{
  http->file = open(filename, O_RDONLY);

  fprintf(stderr, "cupsd: filename=\'%s\', file = %d\n", filename, http->file);

  if (http->file < 0)
    return (0);

  fcntl(http->file, F_SETFD, fcntl(http->file, F_GETFD) | FD_CLOEXEC);

  http->pipe_pid = 0;

  if (!SendHeader(con, status, type))
    return (0);

  if (conprintf(con, "Last-Modified: %s\r\n", get_datetime(filestats->st_mtime)) < 0)
    return (0);
  if (conprintf(con, "Content-Length: %d\r\n", filestats->st_size) < 0)
    return (0);
  if (conprintf(con, "\r\n") < 0)
    return (0);

  FD_SET(http->fd, &OutputSet);

  return (1);
}


/*
 * 'SendHeader()' - Send an HTTP header.
 */

int				/* O - 1 on success, 0 on failure */
SendHeader(http_t    *http,	/* I - Client to send to */
           http_status_t status,	/* I - HTTP status status */
	   char        *type)	/* I - MIME type of document */
{
  if (conprintf(con, "HTTP/%d.%d %d %s\r\n", http->version / 100,
                http->version % 100, status, get_message(status)) < 0)
    return (0);
  if (conprintf(con, "Date: %s\r\n", get_datetime(time(NULL))) < 0)
    return (0);
  if (conprintf(con, "Server: CUPS/1.0\r\n") < 0)
    return (0);
  if (http->keep_alive && http->version == HTTP_1_0)
  {
    if (conprintf(con, "Connection: Keep-Alive\r\n") < 0)
      return (0);
    if (conprintf(con, "Keep-Alive: timeout=%d\r\n", KeepAliveTimeout) < 0)
      return (0);
  }
  if (type != NULL)
    if (conprintf(con, "Content-Type: %s\r\n", type) < 0)
      return (0);

  return (1);
}


/*
 * 'StartListening()' - Create all listening sockets...
 */

void
StartListening(void)
{
  int		i,		/* Looping var */
		val;		/* Parameter value */
  listener_t	*lis;		/* Current listening socket */


#ifdef WIN32
 /*
  * Initialize Windows sockets...  This handles loading the DLL, etc. since
  * Windows doesn't have built-in support for sockets like UNIX...
  */

  WSADATA	wsadata;	/* Socket data for application from DLL */
  static int	initialized = 0;/* Whether or not we've initialized things */


  if (!initialized)
  {
    initialized = 1;

    if (WSAStartup(MAKEWORD(1,1), &wsadata) != 0)
      exit(1);
  }
#else
 /*
  * Setup a 'broken pipe' signal handler for lost clients.
  */

  sigset(SIGPIPE, sigpipe_handler);
#endif /* !WIN32 */

 /*
  * Setup socket listeners...
  */

  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
    if ((lis->fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) == -1)
    {
      fprintf(stderr, "cupsd: Unable to open socket - %s\n", strerror(errno));
      exit(errno);
    }

    fcntl(lis->fd, F_SETFD, fcntl(lis->fd, F_GETFD) | FD_CLOEXEC);

   /*
    * Set things up to reuse the local address for this port.
    */

    val = 1;
    setsockopt(lis->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

   /*
    * Bind to the port we found...
    */

    if (bind(lis->fd, (struct sockaddr *)&(lis->address), sizeof(lis->address)) < 0)
    {
      fprintf(stderr, "cupsd: Unable to bind socket - %s\n", strerror(errno));
      fprintf(stderr, "cupsd: address = %08x, port = %d\n",
              lis->address.sin_addr.s_addr, lis->address.sin_port);
      exit(errno);
    }

   /*
    * Listen for new clients.
    */

    if (listen(lis->fd, SOMAXCONN) < 0)
    {
      fprintf(stderr, "cupsd: Unable to listen for clients - %s\n",
              strerror(errno));
      exit(errno);
    }

   /*
    * Setup the select() input mask to contain the listening socket we have.
    */

    FD_SET(lis->fd, &InputSet);
  }

  fprintf(stderr, "cupsd: Listening on %d sockets...\n", NumListeners);
}


/*
 * 'StopListening()' - Close all listening sockets...
 */

void
StopListening(void)
{
  int		i;		/* Looping var */
  listener_t	*lis;		/* Current listening socket */


  for (i = NumListeners, lis = Listeners; i > 0; i --, lis ++)
  {
#ifdef WIN32
    closesocket(lis->fd);
#else
    close(lis->fd);
#endif /* WIN32 */

    FD_CLR(lis->fd, &InputSet);
  }

  fputs("cupsd: No longer listening for connections...\n", stderr);
}


/*
 * 'WriteClient()' - Write data to a client as needed.
 */

int
WriteClient(http_t *http)
{
  int	bytes;
  int	status;
  char	buf[MAX_BUFFER];


  if (http->state != HTTP_GET_DATA &&
      http->state != HTTP_POST_DATA)
    return (1);

  if ((bytes = read(http->file, buf, sizeof(buf))) > 0)
  {
    if (http->data_encoding == HTTP_DATA_CHUNKED)
    {
      if (conprintf(con, "%d\r\n", bytes) < 0)
      {
        CloseClient(con);
	return (0);
      }

      if (send(http->fd, buf, bytes, 0) < 0)
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
    else if (send(http->fd, buf, bytes, 0) < 0)
    {
      CloseClient(con);
      return (0);
    }
  }
  else
  {
    if (http->data_encoding == HTTP_DATA_CHUNKED)
    {
      if (conprintf(con, "0\r\n\r\n") < 0)
      {
        CloseClient(con);
	return (0);
      }
    }

    FD_CLR(http->fd, &OutputSet);
    FD_CLR(http->file, &InputSet);

    if (http->pipe_pid)
    {
      kill(http->pipe_pid, SIGKILL);
      waitpid(http->pipe_pid, &status, WNOHANG);
    }

    close(http->file);

    if (!http->keep_alive)
    {
      CloseClient(con);
      return (0);
    }

    http->state    = HTTP_WAITING;
    http->file     = 0;
    http->pipe_pid = 0;
  }

  fprintf(stderr, "cupsd: SEND %d bytes to #%d\n", bytes, http->fd);

  http->activity = time(NULL);

  return (1);
}


/*
 * 'chunkprintf()' - Do a printf() to a client...
 */

static int			/* O - Number of bytes written or -1 on error */
chunkprintf(http_t *http,	/* I - Client to write to */
            char     *format,	/* I - printf()-style format string */
            ...)		/* I - Additional args as needed */
{
  int		bytes;		/* Number of bytes to write */
  char		buf[MAX_BUFFER];/* Buffer for formatted string */
  char		len[32];	/* Length string */
  va_list	ap;		/* Variable argument pointer */


  va_start(ap, format);
  bytes = vsprintf(buf, format, ap);
  va_end(ap);

  fprintf(stderr, "cupsd: SEND %s", buf);
  if (buf[bytes - 1] != '\n')
    putc('\n', stderr);

  http->activity = time(NULL);

  if (http->version == HTTP_1_1)
  {
    sprintf(len, "%d\r\n", bytes);
    if (send(http->fd, len, strlen(len), 0) < 3)
      return (-1);
  }

  return (send(http->fd, buf, bytes, 0));
}


/*
 * 'conprintf()' - Do a printf() to a client...
 */

static int			/* O - Number of bytes written or -1 on error */
conprintf(http_t *http,	/* I - Client to write to */
          char     *format,	/* I - printf()-style format string */
          ...)			/* I - Additional args as needed */
{
  int		bytes;		/* Number of bytes to write */
  char		buf[MAX_BUFFER];/* Buffer for formatted string */
  va_list	ap;		/* Variable argument pointer */


  va_start(ap, format);
  bytes = vsprintf(buf, format, ap);
  va_end(ap);

  fprintf(stderr, "cupsd: SEND %s", buf);
  if (buf[bytes - 1] != '\n')
    putc('\n', stderr);

  http->activity = time(NULL);

  return (send(http->fd, buf, bytes, 0));
}


/*
 * 'destatus_basic_auth()' - Destatus a Basic authorization string.
 */

static void
destatus_basic_auth(http_t *http,	/* I - Client to destatus to */
                  char         *line)	/* I - Line to destatus */
{
  int	pos,				/* Bit position */
	base64;				/* Value of this character */
  char	value[1024],			/* Value string */
	*valptr;			/* Pointer into value string */


  for (valptr = value, pos = 0; *line != '\0'; line ++)
  {
   /*
    * Destatus this character into a number from 0 to 63...
    */

    if (*line >= 'A' && *line <= 'Z')
      base64 = *line - 'A';
    else if (*line >= 'a' && *line <= 'z')
      base64 = *line - 'a' + 26;
    else if (*line >= '0' && *line <= '9')
      base64 = *line - '0' + 52;
    else if (*line == '+')
      base64 = 62;
    else if (*line == '/')
      base64 = 63;
    else if (*line == '=')
      break;
    else
      continue;

   /*
    * Store the result in the appropriate chars...
    */

    switch (pos)
    {
      case 0 :
          *valptr = base64 << 2;
	  pos ++;
	  break;
      case 1 :
          *valptr++ |= (base64 >> 4) & 3;
	  *valptr = (base64 << 4) & 255;
	  pos ++;
	  break;
      case 2 :
          *valptr++ |= (base64 >> 2) & 15;
	  *valptr = (base64 << 6) & 255;
	  pos ++;
	  break;
      case 3 :
          *valptr++ |= base64;
	  pos = 0;
	  break;
    }
  }

 /*
  * OK, done decoding the string; pull the username and password out...
  */

  *valptr = '\0';

  fprintf(stderr, "cupsd: Destatusd authorization string = %s\n", value);

  sscanf(value, "%[^:]:%[^\n]", http->username, http->password);

  fprintf(stderr, "cupsd: username = %s, password = %s\n",
          http->username, http->password);
}


/*
 * 'destatus_if_modified()' - Destatus an "If-Modified-Since" line.
 */

static void
destatus_if_modified(http_t *http,
                   char         *line)
{
  int		i;			/* Looping var */
  char		*valptr,
		value[1024],
		month[16];
  struct tm	date;


  memset(&date, 0, sizeof(date));

  while (*line != '\0')
  {
    for (valptr = value; *line != '\0' && *line != ' ' && *line != '-';)
      *valptr++ = *line++;
    *valptr = '\0';

    while (*line == ' ' || *line == '-')
      line ++;

    if (valptr == value)
      continue;

    valptr --;
    if (*valptr == ',')
      *valptr = '\0';

    if (strchr(value, ':') != NULL)
      sscanf(value, "%d:%d:%d", &(date.tm_hour), &(date.tm_min), &(date.tm_sec));
    else if (isdigit(value[0]))
    {
      i = atoi(value);

      if (date.tm_mday == 0 && i > 0 && i < 32)
        date.tm_mday = i;
      else if (i > 100)
        date.tm_year = i - 1900;
      else
        date.tm_year = i;
    }
    else if (strncmp(value, "length=", 7) == 0)
      http->remote_size = atoi(value + 7);
    else
    {
      for (i = 0; i < 7; i ++)
        if (strncasecmp(days[i], value, 3) == 0)
	{
	  date.tm_wday = i;
	  break;
	}

      for (i = 0; i < 12; i ++)
        if (strncasecmp(months[i], value, 3) == 0)
	{
	  date.tm_mon = i;
	  break;
	}
    }
  }

  http->remote_time = mktime(&date);
}


/*
 * 'get_datetime()' - Get a data/time string for the given time.
 */

static char *
get_datetime(time_t t)
{
  struct tm	*tdate;
  static char	datetime[256];


  tdate = gmtime(&t);
  strftime(datetime, sizeof(datetime) - 1, "%a, %d %h %Y %T GMT", tdate);

  return (datetime);
}


/*
 * 'get_extension()' - Get the extension for a filename.
 */

static char *
get_extension(char *filename)
{
  char	*ext;


  ext = filename + strlen(filename) - 1;

  while (ext > filename && *ext != '/')
    if (*ext == '.')
      return (ext + 1);
    else
      ext --;

  return ("");
}


/*
 * 'get_file()' - Get a filename and state info.
 */

static char *
get_file(http_t *http,
         struct stat  *filestats)
{
  int		status;
  char		*params;
  static char	filename[1024];


 /*
  * Need to add DocumentRoot global...
  */

  if (http->language[0] != '\0')
    sprintf(filename, "%s/%s%s", DocumentRoot, http->language, http->uri);
  else
    sprintf(filename, "%s%s", DocumentRoot, http->uri);

  if ((params = strchr(filename, '?')) != NULL)
    *params = '\0';

 /*
  * Grab the status for this language; if there isn't a language-specific file
  * then fallback to the default one...
  */

  if ((status = stat(filename, filestats)) != 0 && http->language[0] != '\0')
  {
   /*
    * Drop the language prefix and try the current directory...
    */

    sprintf(filename, "%s%s", DocumentRoot, http->uri);

    status = stat(filename, filestats);
  }

 /*
  * If we're found a directory, get the index.html file instead...
  */

  if (!status && S_ISDIR(filestats->st_mode))
  {
    if (filename[strlen(filename) - 1] == '/')
      strcat(filename, "index.html");
    else
      strcat(filename, "/index.html");

    status = stat(filename, filestats);
  };

  if (status)
    return (NULL);
  else
    return (filename);
}


/*
 * 'get_line()' - Get a request line terminated with a CR and LF.
 */

static char *
get_line(http_t *http,
         char     *line,
	 int      length)
{
  char	*lineptr,
	*bufptr,
	*bufend;
  int	bytes;
  static char	*chars[] =
		{
		  "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
		  "BS", "HT", "NL", "VT", "NP", "CR", "SO", "SI",
		  "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
		  "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
		};


  lineptr = line;
  bufptr  = http->buf;
  bufend  = http->buf + http->bufused;
  bytes   = 0;

  while (bufptr < bufend && bytes < length)
  {
    bytes ++;

    if (*bufptr == 0x0a)
    {
      bufptr ++;
      *lineptr = '\0';

      if (bufptr[-1] == 0x0d)
        fprintf(stderr, "cupsd: RECV %s CR LF\n", line);
      else
        fprintf(stderr, "cupsd: RECV %s LF\n", line);

      http->bufused -= bytes;
      if (http->bufused > 0)
	memcpy(http->buf, bufptr, http->bufused);

      return (line);
    }
    else if (*bufptr == 0x0d)
      bufptr ++;
    else
      *lineptr++ = *bufptr++;
  }

  *lineptr = '\0';

  fputs("cupsd: RERR ", stderr);

  for (lineptr = line; *lineptr != '\0'; lineptr ++)
    if (*lineptr < ' ')
    {
      if (lineptr > line && lineptr[-1] < ' ')
        putc(' ', stderr);

      fputs(chars[*lineptr], stderr);
      putc(' ', stderr);
    }
    else
      putc(*lineptr, stderr);

  putc('\n', stderr);
    
  return (NULL);
}


/*
 * 'get_long_message()' - Get a long message string for the given HTTP status.
 */

static char *			/* O - Message string */
get_long_message(http_status_t status)	/* I - Message status */
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
 * 'get_message()' - Get a message string for the given HTTP status.
 */

static char *		/* O - Message string */
get_message(http_status_t status)	/* I - Message status */
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
 * 'get_type()' - Get MIME type from the given extension.
 */

static char *
get_type(char *extension)
{
  if (strcmp(extension, "html") == 0 || strcmp(extension, "htm") == 0)
    return ("text/html");
  else if (strcmp(extension, "txt") == 0)
    return ("text/plain");
  else if (strcmp(extension, "gif") == 0)
    return ("image/gif");
  else if (strcmp(extension, "jpg") == 0)
    return ("image/jpg");
  else if (strcmp(extension, "png") == 0)
    return ("image/png");
  else if (strcmp(extension, "ps") == 0)
    return ("application/postscript");
  else if (strcmp(extension, "pdf") == 0)
    return ("application/pdf");
  else if (strcmp(extension, "gz") == 0)
    return ("application/gzip");
  else
    return ("application/unknown");
}


/*
 * 'pipe_command()' - Pipe the output of a command to the remote client.
 */

static int			/* O - Process ID */
pipe_command(int infile,	/* I - Standard input for command */
             int *outfile,	/* O - Standard output for command */
	     char *command)	/* I - Command to run */
{
  int	pid;			/* Process ID */
  char	*commptr;		/* Command string pointer */
  int	fds[2];			/* Pipe FDs */
  int	argc;			/* Number of arguments */
  char	argbuf[1024],		/* Argument buffer */
	*argv[100];		/* Argument strings */


 /*
  * Copy the command string...
  */

  strncpy(argbuf, command, sizeof(argbuf) - 1);
  argbuf[sizeof(argbuf) - 1] = '\0';

 /*
  * Parse the string; arguments can be separated by spaces or by ? or +...
  */

  argv[0] = argbuf;

  for (commptr = argbuf, argc = 1; *commptr != '\0'; commptr ++)
    if (*commptr == ' ' || *commptr == '?' || *commptr == '+')
    {
      *commptr++ = '\0';

      while (*commptr == ' ')
        commptr ++;

      if (*commptr != '\0')
      {
        argv[argc] = commptr;
	argc ++;
      }

      commptr --;
    }
    else if (*commptr == '%')
    {
      if (commptr[1] >= '0' && commptr[1] <= '9')
        *commptr = (commptr[1] - '0') << 4;
      else
        *commptr = (tolower(commptr[1]) - 'a' + 10) << 4;

      if (commptr[2] >= '0' && commptr[2] <= '9')
        *commptr |= commptr[2] - '0';
      else
        *commptr |= tolower(commptr[2]) - 'a' + 10;

      strcpy(commptr + 1, commptr + 3);
    }

  argv[argc] = NULL;

 /*
  * Create a pipe for the output...
  */

  if (pipe(fds))
    return (0);

 /*
  * Then execute the pipe command...
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...  Close stdin if necessary and dup the pipe to stdout.
    */

    if (infile)
    {
      close(0);
      dup(infile);
    }

    close(1);
    dup(fds[1]);

    close(fds[0]);
    close(fds[1]);

   /*
    * Execute the pipe program; if an error occurs, exit with status 1...
    */

    execvp(argv[0], argv);
    exit(1);
    return (0);
  }
  else if (pid < 0)
  {
   /*
    * Error - can't fork!
    */

    close(fds[0]);
    close(fds[1]);
    return (0);
  }
  else
  {
   /*
    * Fork successful - return the PID...
    */

    *outfile = fds[0];
    close(fds[1]);

    return (pid);
  }
}


/*
 * 'show_printer_status()' - Show the current printer status.
 */

static int
show_printer_status(http_t *http)
{
  printer_t	*p;
  static char	*states[] =
  {
    "Idle", "Busy", "Faulted", "Unavailable",
    "Disabled", "Disabled", "Disabled", "Disabled",
    "Rejecting", "Rejecting", "Rejecting", "Rejecting",
    "Rejecting", "Rejecting", "Rejecting", "Rejecting"
  };


  p = NULL;
  if (http->uri[10] != '\0')
    if ((p = FindPrinter(http->uri + 10)) == NULL)
      return (0);

  if (!SendHeader(con, HTTP_OK, "text/html"))
    return (0);

  if (http->version == HTTP_1_1)
  {
    http->data_encoding = HTTP_DATA_CHUNKED;

    if (conprintf(con, "Transfer-Encoding: chunked\r\n") < 0)
      return (0);
  }

  if (conprintf(con, "\r\n") < 0)
    return (0);

  if (p != NULL)
  {
   /*
    * Send printer information...
    */

    chunkprintf(con, "<HTML>\n"
                     "<HEAD>\n"
		     "\t<TITLE>Printer Status for %s</TITLE>\n"
		     "\t<META HTTP-EQUIV=\"Refresh\" CONTENT=\"10\">\n"
		     "</HEAD>\n"
		     "<BODY BGCOLOR=#ffffff>\n"
		     "<TABLE WIDTH=100%%><TR>\n"
		     "\t<TD ALIGN=LEFTV ALIGN=MIDDLE><IMG SRC=\"/images/cups-small.gif\"></TD>\n"
		     "\t<TD ALIGN=CENTER VALIGN=MIDDLE><H1>Printer Status for %s</H1></TD>\n"
		     "\t<TD ALIGN=RIGHT VALIGN=MIDDLE><IMG SRC=\"/images/vendor.gif\"></TD>\n"
		     "</TR></TABLE>\n", p->name, p->name);

    chunkprintf(con, "<CENTER><TABLE BORDER=1 WIDTH=80%%>\n"
                     "<TR><TH>Name</TH><TH>Value</TH></TR>\n"
		     "<TR><TD>Info</TD><TD>%s</TD></TR>\n"
		     "<TR><TD>MoreInfo</TD><TD>%s</TD></TR>\n"
		     "<TR><TD>LocationCode</TD><TD>%s</TD></TR>\n"
		     "<TR><TD>LocationText</TD><TD>%s</TD></TR>\n"
		     "<TR><TD>Device</TD><TD>%s</TD></TR>\n"
		     "<TR><TD>PPDFile</TD><TD>%s</TD></TR>\n",
		     p->info, p->more_info, p->location_status, p->location_text,
		     p->device_uri, p->ppd);
  }
  else
  {
    chunkprintf(con, "<HTML>\n"
                     "<HEAD>\n"
		     "\t<TITLE>Available Printers</TITLE>\n"
		     "\t<META HTTP-EQUIV=\"Refresh\" CONTENT=\"10\">\n"
		     "</HEAD>\n"
		     "<BODY BGCOLOR=#ffffff>\n"
		     "<TABLE WIDTH=100%%><TR>\n"
		     "\t<TD ALIGN=LEFT VALIGN=MIDDLE><IMG SRC=\"/images/cups-small.gif\"></TD>\n"
		     "\t<TD ALIGN=CENTER VALIGN=MIDDLE><H1>Available Printers</H1></TD>\n"
		     "\t<TD ALIGN=RIGHT VALIGN=MIDDLE><IMG SRC=\"/images/vendor.gif\"></TD>\n"
		     "</TR></TABLE>\n"
		     "<CENTER><TABLE BORDER=1 WIDTH=80%%>\n"
                     "<TR><TH>Name</TH><TH>State</TH><TH>Info</TH></TR>\n");

    for (p = Printers; p != NULL; p = p->next)
      chunkprintf(con, "<TR><TD><A HREF=/printers/%s>%s</A></TD><TD>%s</TD><TD>%s</TD></TR>\n",
		       p->name, p->name, states[p->state], p->info);
  }

  chunkprintf(con, "</TABLE></CENTER>\n"
                   "<HR>\n"
		   "CUPS Copyright 1997-1999 by Easy Software Products, "
		   "All Rights Reserved.  CUPS and the CUPS logo are the "
		   "trademark property of Easy Software Products.\n"
		   "</BODY>\n"
                   "</HTML>\n");
  chunkprintf(con, "");

  return (1);
}


/*
 * 'sigpipe_handler()' - Handle 'broken pipe' signals from lost network
 *                       clients.
 */

static void
sigpipe_handler(int sig)	/* I - Signal number */
{
/* IGNORE */
}


/*
 * End of "$Id: http.c,v 1.7 1999/01/24 14:18:43 mike Exp $".
 */
