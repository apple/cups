/*
 * "$Id: client.c,v 1.5 1999/02/26 15:11:11 mike Exp $"
 *
 *   Client routines for the Common UNIX Printing System (CUPS) scheduler.
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
 *   AcceptClient()        - Accept a new client.
 *   CloseAllClients()     - Close all remote clients immediately.
 *   CloseClient()         - Close a remote client.
 *   ReadClient()          - Read data from a client.
 *   SendCommand()         - Send output from a command via HTTP.
 *   SendError()           - Send an error message via HTTP.
 *   SendFile()            - Send a file via HTTP.
 *   SendHeader()          - Send an HTTP request.
 *   StartListening()      - Create all listening sockets...
 *   StopListening()       - Close all listening sockets...
 *   WriteClient()         - Write data to a client as needed.
 *   check_if_modified()   - Decode an "If-Modified-Since" line.
 *   decode_basic_auth()   - Decode a Basic authorization string.
 *   get_file()            - Get a filename and state info.
 *   pipe_command()        - Pipe the output of a command to the remote client.
 *   process_ipp_request() - Process an incoming IPP request...
 *   send_ipp_error()      - Send an error status back to the IPP client.
 *   sigpipe_handler()     - Handle 'broken pipe' signals from lost network
 *                           clients.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <cups/debug.h>


/*
 * Local functions...
 */

static int	check_if_modified(client_t *con, struct stat *filestats);
static void	decode_basic_auth(client_t *con);
static char	*get_file(client_t *con, struct stat *filestats);
static int	pipe_command(client_t *con, int infile, int *outfile, char *command, char *options);
static void	process_ipp_request(client_t *con);
static void	send_ipp_error(client_t *con, ipp_status_t status);
static void	sigpipe_handler(int sig);

/**** OBSOLETE ****/
static char	*get_extension(char *filename);
static char	*get_type(char *extension);


/*
 * 'AcceptClient()' - Accept a new client.
 */

void
AcceptClient(listener_t *lis)	/* I - Listener socket */
{
  int			i;	/* Looping var */
  int			val;	/* Parameter value */
  client_t		*con;	/* New client pointer */
  unsigned		address;/* Address of client */
  struct hostent	*host;	/* Host entry for address */


 /*
  * Get a pointer to the next available client...
  */

  con = Clients + NumClients;

  memset(con, 0, sizeof(client_t));
  con->http.activity = time(NULL);

 /*
  * Accept the client and get the remote address...
  */

  val = sizeof(struct sockaddr_in);

  if ((con->http.fd = accept(lis->fd, (struct sockaddr *)&(con->http.hostaddr), &val)) < 0)
  {
    LogMessage(LOG_ERROR, "accept() failed - %s.", strerror(errno));
    return;
  }

 /*
  * Get the hostname or format the IP address as needed...
  */

  address = ntohl(con->http.hostaddr.sin_addr.s_addr);

  if (HostNameLookups)
    host = gethostbyaddr(&address, sizeof(address), AF_INET);
  else
    host = NULL;

  if (host == NULL)
    sprintf(con->http.hostname, "%d.%d.%d.%d", (address >> 24) & 255,
            (address >> 16) & 255, (address >> 8) & 255, address & 255);
  else
    strncpy(con->http.hostname, host->h_name, sizeof(con->http.hostname) - 1);

  LogMessage(LOG_INFO, "accept() %d from %s.", con->http.fd, con->http.hostname);

 /*
  * Add the socket to the select() input mask.
  */

  fcntl(con->http.fd, F_SETFD, fcntl(con->http.fd, F_GETFD) | FD_CLOEXEC);

  FD_SET(con->http.fd, &InputSet);

  NumClients ++;

 /*
  * Temporarily suspend accept()'s until we lose a client...
  */

  if (NumClients == MAX_CLIENTS)
    for (i = 0; i < NumListeners; i ++)
      FD_CLR(Listeners[i].fd, &InputSet);
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
CloseClient(client_t *con)	/* I - Client to close */
{
  int	i;			/* Looping var */
  int	status;			/* Exit status of pipe command */


  LogMessage(LOG_INFO, "CloseClient() %d", con->http.fd);

 /*
  * Close the socket and clear the file from the input set for select()...
  */

#if defined(WIN32) || defined(__EMX__)
 /*
  * Microsoft Windows and IBM OS/2 don't have a unified IO system...
  */

  closesocket(con->http.fd);
#else
 /*
  * UNIX *does*....
  */

  close(con->http.fd);
#endif /* WIN32 || __EMX__ */

  for (i = 0; i < NumListeners; i ++)
    FD_SET(Listeners[i].fd, &InputSet);

  FD_CLR(con->http.fd, &InputSet);
  if (con->pipe_pid != 0)
    FD_CLR(con->file, &InputSet);
  FD_CLR(con->http.fd, &OutputSet);

 /*
  * If we have a data file open, close it...
  */

  if (con->file > 0)
  {
    if (con->pipe_pid)
    {
      kill(con->pipe_pid, SIGKILL);
      waitpid(con->pipe_pid, &status, WNOHANG);
    }

    close(con->file);
  }

 /*
  * Compact the list of clients as necessary...
  */

  NumClients --;

  if (con < (Clients + NumClients))
    memcpy(con, con + 1, (Clients + NumClients - con) * sizeof(client_t));
}


/*
 * 'ReadClient()' - Read data from a client.
 */

int				/* O - 1 on success, 0 on error */
ReadClient(client_t *con)	/* I - Client to read from */
{
  char		line[8192],	/* Line from client... */
		operation[64],	/* Operation code from socket */
		version[64];	/* HTTP version number string */
  int		major, minor;	/* HTTP version numbers */
  http_status_t	status;		/* Transfer status */
  int		bytes;		/* Number of bytes to POST */
  char		*filename,	/* Name of file for GET/HEAD */
		*extension,	/* Extension of file */
		*type;		/* MIME type */
  struct stat	filestats;	/* File information */
  char		command[1024],	/* Command to run */
		*options;	/* Options/CGI data */


  status = HTTP_CONTINUE;

  switch (con->http.state)
  {
    case HTTP_WAITING :
       /*
        * See if we've received a request line...
	*/

        if (httpGets(line, sizeof(line) - 1, HTTP(con)) == NULL)
	{
          CloseClient(con);
	  return (0);
	}

       /*
        * Ignore blank request lines...
	*/

        if (line[0] == '\0')
	  break;

       /*
        * Clear other state variables...
	*/

        httpClearFields(HTTP(con));

        con->http.activity       = time(NULL);
        con->http.version        = HTTP_1_0;
	con->http.keep_alive     = HTTP_KEEPALIVE_OFF;
	con->http.data_encoding  = HTTP_ENCODE_LENGTH;
	con->http.data_remaining = 0;
	con->operation           = HTTP_WAITING;
	con->bytes               = 0;
	con->file                = 0;
	con->pipe_pid            = 0;
	con->username[0]         = '\0';
	con->password[0]         = '\0';
	con->uri[0]              = '\0';

	if (con->language != NULL)
	{
	  cupsLangFree(con->language);
	  con->language = NULL;
	}

       /*
        * Grab the request line...
	*/

        switch (sscanf(line, "%63s%1023s%s", operation, con->uri, version))
	{
	  case 1 :
	      SendError(con, HTTP_BAD_REQUEST);
	      CloseClient(con);
	      return (0);
	  case 2 :
	      con->http.version = HTTP_0_9;
	      break;
	  case 3 :
	      if (sscanf(version, "HTTP/%d.%d", &major, &minor) != 2)
	      {
		SendError(con, HTTP_BAD_REQUEST);
		CloseClient(con);
		return (0);
	      }

	      if (major < 2)
	      {
	        con->http.version = (http_version_t)(major * 100 + minor);
		if (con->http.version == HTTP_1_1)
		  con->http.keep_alive = HTTP_KEEPALIVE_ON;
		else
		  con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	      }
	      else
	      {
	        SendError(con, HTTP_NOT_SUPPORTED);
	        CloseClient(con);
	        return (0);
	      }
	      break;
	}

       /*
        * Process the request...
	*/

        if (strcmp(operation, "GET") == 0)
	  con->http.state = HTTP_GET;
        else if (strcmp(operation, "PUT") == 0)
	  con->http.state = HTTP_PUT;
        else if (strcmp(operation, "POST") == 0)
	  con->http.state = HTTP_POST;
        else if (strcmp(operation, "DELETE") == 0)
	  con->http.state = HTTP_DELETE;
        else if (strcmp(operation, "TRACE") == 0)
	  con->http.state = HTTP_TRACE;
        else if (strcmp(operation, "CLOSE") == 0)
	  con->http.state = HTTP_CLOSE;
        else if (strcmp(operation, "OPTIONS") == 0)
	  con->http.state = HTTP_OPTIONS;
        else if (strcmp(operation, "HEAD") == 0)
	  con->http.state = HTTP_HEAD;
	else
	{
	  SendError(con, HTTP_BAD_REQUEST);
	  CloseClient(con);
	  return (0);
	}

        con->start     = time(NULL);
        con->operation = con->http.state;

        LogMessage(LOG_INFO, "ReadClient() %d %s %s HTTP/%d.%d", con->http.fd,
	           operation, con->uri,
		   con->http.version / 100, con->http.version % 100);

	con->http.status = HTTP_OK;
        break;

    case HTTP_CLOSE :
    case HTTP_DELETE :
    case HTTP_GET :
    case HTTP_HEAD :
    case HTTP_POST :
    case HTTP_PUT :
    case HTTP_TRACE :
       /*
        * Parse incoming parameters until the status changes...
	*/

        status = httpUpdate(HTTP(con));

	if (status != HTTP_OK && status != HTTP_CONTINUE)
	{
	  SendError(con, HTTP_BAD_REQUEST);
	  CloseClient(con);
	  return (0);
	}
	break;
  }

 /*
  * Handle new transfers...
  */

  if (status == HTTP_OK)
  {
    con->language = cupsLangGet(con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE]);

    decode_basic_auth(con);

    if (con->http.fields[HTTP_FIELD_HOST][0] == '\0' &&
        con->http.version >= HTTP_1_0)
    {
      if (!SendError(con, HTTP_BAD_REQUEST))
      {
	CloseClient(con);
	return (0);
      }
    }
    else if (strncmp(con->uri, "..", 2) == 0)
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
    else if (con->uri[0] != '/')
    {
     /*
      * Don't allow proxying (yet)...
      */

      if (!SendError(con, HTTP_METHOD_NOT_ALLOWED))
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
    else switch (con->http.state)
    {
      case HTTP_GET_SEND :
	  if (strncmp(con->uri, "/printers", 9) == 0 ||
	      strncmp(con->uri, "/classes", 8) == 0 ||
	      strncmp(con->uri, "/jobs", 5) == 0)
	  {
	   /*
	    * Send CGI output...
	    */

            if (strncmp(con->uri, "/printers", 9) == 0)
	    {
	      sprintf(command, "%s/cgi-bin/printers", ServerRoot);
	      options = con->uri + 9;
	    }
	    else if (strncmp(con->uri, "/classes", 8) == 0)
	    {
	      sprintf(command, "%s/cgi-bin/classes", ServerRoot);
	      options = con->uri + 8;
	    }
	    else
	    {
	      sprintf(command, "%s/cgi-bin/jobs", ServerRoot);
	      options = con->uri + 5;
	    }

	    if (*options == '/')
	      options ++;

            if (!SendCommand(con, command, options))
	    {
	      if (!SendError(con, HTTP_NOT_FOUND))
	      {
	        CloseClient(con);
		return (0);
	      }
            }
	    else
              LogRequest(con, HTTP_OK);

	    if (con->http.version <= HTTP_1_0)
	      con->http.keep_alive = HTTP_KEEPALIVE_OFF;
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
	    else if (!check_if_modified(con, &filestats))
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
	    }
	  }
          break;

      case HTTP_POST_RECV :
         /*
	  * See what kind of POST request this is; for IPP requests the
	  * content-type field will be "application/ipp"...
	  */

          httpGetLength(&(con->http));

	  if (strcmp(con->http.fields[HTTP_FIELD_CONTENT_TYPE], "application/ipp") == 0)
            con->request = ippNew();
	  else if (strcmp(con->http.fields[HTTP_FIELD_CONTENT_TYPE], "application/ipp") == 0 &&
	           (strncmp(con->uri, "/printers", 9) == 0 ||
	            strncmp(con->uri, "/classes", 8) == 0 ||
	            strncmp(con->uri, "/jobs", 5) == 0))
	  {
	   /*
	    * CGI request...
	    */

            if (strncmp(con->uri, "/printers", 9) == 0)
	    {
	      sprintf(command, "%s/cgi-bin/printers", ServerRoot);
	      options = con->uri + 9;
	    }
	    else if (strncmp(con->uri, "/classes", 8) == 0)
	    {
	      sprintf(command, "%s/cgi-bin/classes", ServerRoot);
	      options = con->uri + 8;
	    }
	    else
	    {
	      sprintf(command, "%s/cgi-bin/jobs", ServerRoot);
	      options = con->uri + 5;
	    }

	    if (*options == '/')
	      options ++;

            if (!SendCommand(con, command, options))
	    {
	      if (!SendError(con, HTTP_NOT_FOUND))
	      {
	        CloseClient(con);
		return (0);
	      }
            }
	    else
              LogRequest(con, HTTP_OK);

	    if (con->http.version <= HTTP_1_0)
	      con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	  }
	  else if (!SendError(con, HTTP_UNAUTHORIZED))
	  {
	    CloseClient(con);
	    return (0);
	  }
	  break;

      case HTTP_PUT_RECV :
      case HTTP_DELETE :
      case HTTP_TRACE :
          SendError(con, HTTP_NOT_IMPLEMENTED);

      case HTTP_CLOSE :
          CloseClient(con);
	  return (0);

      case HTTP_HEAD :
	  if (strncmp(con->uri, "/printers/", 10) == 0 ||
	      strncmp(con->uri, "/classes/", 9) == 0 ||
	      strncmp(con->uri, "/jobs/", 6) == 0)
	  {
	   /*
	    * CGI output...
	    */

            if (!SendHeader(con, HTTP_OK, "text/html"))
	    {
	      CloseClient(con);
	      return (0);
	    }

	    if (httpPrintf(HTTP(con), "\r\n") < 0)
	    {
	      CloseClient(con);
	      return (0);
	    }

            LogRequest(con, HTTP_OK);
	  }
	  else if ((filename = get_file(con, &filestats)) == NULL)
	  {
	    if (!SendHeader(con, HTTP_NOT_FOUND, "text/html"))
	    {
	      CloseClient(con);
	      return (0);
	    }

            LogRequest(con, HTTP_NOT_FOUND);
	  }
	  else if (!check_if_modified(con, &filestats))
          {
            if (!SendError(con, HTTP_NOT_MODIFIED))
	    {
              CloseClient(con);
	      return (0);
	    }

            LogRequest(con, HTTP_NOT_MODIFIED);
	  }
	  else
	  {
	   /*
	    * Serve a file...
	    */

	    extension = get_extension(filename);
	    type      = get_type(extension);

            if (!SendHeader(con, HTTP_OK, type))
	    {
	      CloseClient(con);
	      return (0);
	    }

	    if (httpPrintf(HTTP(con), "Last-Modified: %s\r\n",
	                   httpGetDateString(filestats.st_mtime)) < 0)
	    {
	      CloseClient(con);
	      return (0);
	    }

	    if (httpPrintf(HTTP(con), "Content-Length: %d\r\n",
	                   filestats.st_size) < 0)
	    {
	      CloseClient(con);
	      return (0);
	    }

            LogRequest(con, HTTP_OK);
	  }

          if (send(con->http.fd, "\r\n", 2, 0) < 0)
	  {
	    CloseClient(con);
	    return (0);
	  }

          con->http.state = HTTP_WAITING;
          break;
    }
  }

 /*
  * Handle any incoming data...
  */

  switch (con->http.state)
  {
    case HTTP_PUT_RECV :
        break;

    case HTTP_POST_RECV :
        LogMessage(LOG_DEBUG, "ReadClient() %d con->data_encoding = %s con->data_remaining = %d\n",
		   con->http.fd,
		   con->http.data_encoding == HTTP_ENCODE_CHUNKED ? "chunked" : "length",
		   con->http.data_remaining);

        if (con->request != NULL)
	{
	 /*
	  * Grab any request data from the connection...
	  */

	  if (ippRead(&(con->http), con->request) != IPP_DATA)
	    break;

         /*
	  * Then create a file as needed for the request data...
	  */

          if (con->file == 0 &&
	      (con->http.data_remaining > 0 ||
	       con->http.data_encoding == HTTP_ENCODE_CHUNKED))
	  {
            sprintf(con->filename, "%s/requests/XXXXXX", ServerRoot);
	    con->file = mkstemp(con->filename);

            LogMessage(LOG_INFO, "ReadClient() %d REQUEST %s", con->http.fd,
	               con->filename);

	    if (con->file < 0)
	    {
	      if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	  }
        }

        if ((bytes = httpRead(HTTP(con), line, sizeof(line))) < 0)
	{
	  CloseClient(con);
	  return (0);
	}
	else if (bytes > 0)
	{
	  con->bytes += bytes;

          LogMessage(LOG_DEBUG, "ReadClient() %d writing %d bytes", bytes);

          if (write(con->file, line, bytes) < bytes)
	  {
	    close(con->file);
	    unlink(con->filename);

            if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	    {
	      CloseClient(con);
	      return (0);
	    }
	  }
	}

	if (con->http.state == HTTP_POST_SEND)
	{
	  close(con->file);

          if (con->request)
            process_ipp_request(con);
	}
        break;
  }

  if (!con->http.keep_alive && con->http.state == HTTP_WAITING)
  {
    CloseClient(con);
    return (0);
  }
  else
    return (1);
}


/*
 * 'SendCommand()' - Send output from a command via HTTP.
 */

int
SendCommand(client_t      *con,
	    char          *command,
	    char          *options)
{
  con->pipe_pid = pipe_command(con, 0, &(con->file), command, options);

  LogMessage(LOG_DEBUG, "SendCommand() %d command=\"%s\" file=%d pipe_pid=%d",
             con->http.fd, command, con->file, con->pipe_pid);

  if (con->pipe_pid == 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  FD_SET(con->file, &InputSet);
  FD_SET(con->http.fd, &OutputSet);

  if (!SendHeader(con, HTTP_OK, "text/html"))
    return (0);

  if (con->http.version == HTTP_1_1)
  {
    con->http.data_encoding = HTTP_ENCODE_CHUNKED;

    if (httpPrintf(HTTP(con), "Transfer-Encoding: chunked\r\n") < 0)
      return (0);
  }

  if (httpPrintf(HTTP(con), "\r\n") < 0)
    return (0);

  return (1);
}


/*
 * 'SendError()' - Send an error message via HTTP.
 */

int				/* O - 1 if successful, 0 otherwise */
SendError(client_t      *con,	/* I - Connection */
          http_status_t code)	/* I - Error code */
{
  char	message[1024];		/* Text version of error code */


 /*
  * Put the request in the access_log file...
  */

  LogRequest(con, code);

 /*
  * To work around bugs in some proxies, don't use Keep-Alive for some
  * error messages...
  */

  if (code >= HTTP_BAD_REQUEST)
    con->http.keep_alive = HTTP_KEEPALIVE_OFF;

 /*
  * Send an error message back to the client.  If the error code is a
  * 400 or 500 series, make sure the message contains some text, too!
  */

  if (!SendHeader(con, code, NULL))
    return (0);

  if (code == HTTP_UNAUTHORIZED)
  {
    if (httpPrintf(HTTP(con), "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
      return (0);
  }

  if (con->http.version >= HTTP_1_1 && !con->http.keep_alive)
  {
    if (httpPrintf(HTTP(con), "Connection: close\r\n") < 0)
      return (0);
  }

  if (code >= HTTP_BAD_REQUEST)
  {
   /*
    * Send a human-readable error message.
    */

    sprintf(message, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>"
                     "<BODY><H1>%s</H1>%s</BODY></HTML>\n",
            code, httpStatus(code), httpStatus(code),
	    con->language ? (char *)con->language->messages[code] : httpStatus(code));

    if (httpPrintf(HTTP(con), "Content-Type: text/html\r\n") < 0)
      return (0);
    if (httpPrintf(HTTP(con), "Content-Length: %d\r\n", strlen(message)) < 0)
      return (0);
    if (httpPrintf(HTTP(con), "\r\n") < 0)
      return (0);
    if (send(con->http.fd, message, strlen(message), 0) < 0)
      return (0);
  }
  else if (httpPrintf(HTTP(con), "\r\n") < 0)
    return (0);

  con->http.state = HTTP_WAITING;

  return (1);
}


/*
 * 'SendFile()' - Send a file via HTTP.
 */

int
SendFile(client_t    *con,
         http_status_t code,
	 char        *filename,
	 char        *type,
	 struct stat *filestats)
{
  con->file = open(filename, O_RDONLY);

  LogMessage(LOG_DEBUG, "SendFile() %d file=%d", con->http.fd, con->file);

  if (con->file < 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  con->pipe_pid = 0;

  if (!SendHeader(con, code, type))
    return (0);

  if (httpPrintf(HTTP(con), "Last-Modified: %s\r\n", httpGetDateString(filestats->st_mtime)) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Content-Length: %d\r\n", filestats->st_size) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "\r\n") < 0)
    return (0);

  FD_SET(con->http.fd, &OutputSet);

  return (1);
}


/*
 * 'SendHeader()' - Send an HTTP request.
 */

int				/* O - 1 on success, 0 on failure */
SendHeader(client_t    *con,	/* I - Client to send to */
           http_status_t code,	/* I - HTTP status code */
	   char        *type)	/* I - MIME type of document */
{
  if (httpPrintf(HTTP(con), "HTTP/%d.%d %d %s\r\n", con->http.version / 100,
                con->http.version % 100, code, httpStatus(code)) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Date: %s\r\n", httpGetDateString(time(NULL))) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Server: CUPS/1.0\r\n") < 0)
    return (0);
  if (con->http.keep_alive && con->http.version >= HTTP_1_0)
  {
    if (httpPrintf(HTTP(con), "Connection: Keep-Alive\r\n") < 0)
      return (0);
    if (httpPrintf(HTTP(con), "Keep-Alive: timeout=%d\r\n", KeepAliveTimeout) < 0)
      return (0);
  }
  if (con->language != NULL)
  {
    if (httpPrintf(HTTP(con), "Content-Language: %s\r\n",
                   con->language->language) < 0)
      return (0);

    if (type != NULL)
      if (httpPrintf(HTTP(con), "Content-Type: %s; charset=%s\r\n", type,
                     cupsLangEncoding(con->language)) < 0)
        return (0);
  }
  else if (type != NULL)
    if (httpPrintf(HTTP(con), "Content-Type: %s\r\n", type) < 0)
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


#if defined(WIN32) || defined(__EMX__)
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
    LogMessage(LOG_INFO, "StartListening() address=%08x port=%d",
               ntohl(lis->address.sin_addr.s_addr),
	       ntohs(lis->address.sin_port));

    if ((lis->fd = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) == -1)
    {
      LogMessage(LOG_ERROR, "StartListening() Unable to open listen socket - %s.",
                 strerror(errno));
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
      LogMessage(LOG_ERROR, "StartListening() Unable to bind socket - %s.", strerror(errno));
      exit(errno);
    }

   /*
    * Listen for new clients.
    */

    if (listen(lis->fd, SOMAXCONN) < 0)
    {
      LogMessage(LOG_ERROR, "StartListening() Unable to listen for clients - %s.",
                 strerror(errno));
      exit(errno);
    }

   /*
    * Setup the select() input mask to contain the listening socket we have.
    */

    FD_SET(lis->fd, &InputSet);
  }

  LogMessage(LOG_INFO, "StartListening() NumListeners=%d", NumListeners);
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
#if defined(WIN32) || defined(__EMX__)
    closesocket(lis->fd);
#else
    close(lis->fd);
#endif /* WIN32 || __EMX__ */

    FD_CLR(lis->fd, &InputSet);
  }

  LogMessage(LOG_INFO, "StopListening()");
}


/*
 * 'WriteClient()' - Write data to a client as needed.
 */

int
WriteClient(client_t *con)
{
  int	bytes;
  int	status;
  char	buf[HTTP_MAX_BUFFER];


  if (con->http.state != HTTP_GET_SEND &&
      con->http.state != HTTP_POST_SEND)
    return (1);

  if (con->response != NULL)
    bytes = ippWrite(&(con->http), con->response) != IPP_DATA;
  else if ((bytes = read(con->file, buf, sizeof(buf))) > 0)
  {
    if (httpWrite(HTTP(con), buf, bytes) < 0)
    {
      CloseClient(con);
      return (0);
    }

    con->bytes += bytes;
  }

  if (bytes <= 0)
  {
    LogRequest(con, HTTP_OK);

    if (con->http.data_encoding == HTTP_ENCODE_CHUNKED)
    {
      if (httpPrintf(HTTP(con), "0\r\n\r\n") < 0)
      {
        CloseClient(con);
	return (0);
      }
    }

    FD_CLR(con->http.fd, &OutputSet);
    FD_CLR(con->file, &InputSet);

    if (con->pipe_pid)
    {
      kill(con->pipe_pid, SIGKILL);
      waitpid(con->pipe_pid, &status, WNOHANG);
    }

    close(con->file);

    if (!con->http.keep_alive)
    {
      CloseClient(con);
      return (0);
    }

    con->http.state = HTTP_WAITING;
    con->file       = 0;
    con->pipe_pid   = 0;

    if (con->request != NULL)
    {
      ippDelete(con->request);
      con->request = NULL;
    }

    if (con->response != NULL)
    {
      ippDelete(con->response);
      con->response = NULL;
    }
  }

  LogMessage(LOG_DEBUG, "WriteClient() %d %d bytes", con->http.fd, bytes);

  con->http.activity = time(NULL);

  return (1);
}


/*
 * 'check_if_modified()' - Decode an "If-Modified-Since" line.
 */

static int					/* O - 1 if modified since */
check_if_modified(client_t    *con,		/* I - Client connection */
                  struct stat *filestats)	/* I - File information */
{
  char		*ptr;				/* Pointer into field */
  time_t	date;				/* Time/date value */
  int		size;				/* Size/length value */


  size = 0;
  date = 0;
  ptr  = con->http.fields[HTTP_FIELD_IF_MODIFIED_SINCE];

  if (*ptr == '\0')
    return (1);

  LogMessage(LOG_DEBUG, "check_if_modified() %d If-Modified-Since=\"%s\"",
             con->http.fd, ptr);

  while (*ptr != '\0')
  {
    while (isspace(*ptr) || *ptr == ';')
      ptr ++;

    if (strncasecmp(ptr, "length=", 7) == 0)
    {
      ptr += 7;
      size = atoi(ptr);

      while (isdigit(*ptr))
        ptr ++;
    }
    else if (isalpha(*ptr))
    {
      date = httpGetDateTime(ptr);
      while (*ptr != '\0' && *ptr != ';')
        ptr ++;
    }
  }

  LogMessage(LOG_DEBUG, "check_if_modified() %d sizes=%d,%d dates=%d,%d",
             con->http.fd, size, filestats->st_size, date, filestats->st_mtime);

  return ((size != filestats->st_size && size != 0) ||
          (date < filestats->st_mtime && date != 0) ||
	  (size == 0 && date == 0));
}


/*
 * 'decode_basic_auth()' - Decode a Basic authorization string.
 */

static void
decode_basic_auth(client_t *con)	/* I - Client to decode to */
{
  char	value[1024];			/* Value string */


 /*
  * Decode the string and pull the username and password out...
  */

  httpDecode64(value, con->http.fields[HTTP_FIELD_AUTHORIZATION]);

  LogMessage(LOG_DEBUG, "decode_basic_auth() %d Authorization=\"%s\"",
             con->http.fd, value);

  sscanf(value, "%[^:]:%[^\n]", con->username, con->password);
}


/*
 * 'pipe_command()' - Pipe the output of a command to the remote client.
 */

static int			/* O - Process ID */
pipe_command(client_t *con,	/* I - Client connection */
             int      infile,	/* I - Standard input for command */
             int      *outfile,	/* O - Standard output for command */
	     char     *command,	/* I - Command to run */
	     char     *options)	/* I - Options for command */
{
  int	pid;			/* Process ID */
  char	*commptr;		/* Command string pointer */
  int	fds[2];			/* Pipe FDs */
  int	argc;			/* Number of arguments */
  char	argbuf[1024],		/* Argument buffer */
	*argv[100],		/* Argument strings */
	*envp[100];		/* Environment variables */
  static char	lang[1024];		/* LANG env variable */
  static char	content_length[1024];	/* CONTENT_LENGTH env variable */
  static char	content_type[1024];	/* CONTENT_TYPE env variable */
  static char	server_port[1024];	/* Default listen port */
  static char	remote_host[1024];	/* REMOTE_HOST env variable */
  static char	remote_user[1024];	/* REMOTE_HOST env variable */


 /*
  * Copy the command string...
  */

  strncpy(argbuf, options, sizeof(argbuf) - 1);
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

  if (argv[1][0] == '\0')
    argv[1] = strrchr(command, '/') + 1;

 /*
  * Setup the environment variables as needed...
  */

  sprintf(lang, "LANG=%s", con->language ? con->language->language : "C");
  sprintf(server_port, "SERVER_PORT=%d", ntohs(con->http.hostaddr.sin_port));
  sprintf(remote_host, "REMOTE_HOST=%s", con->http.hostname);
  sprintf(remote_user, "REMOTE_USER=%s", con->username);

  envp[0] = "PATH=/bin:/usr/bin";
  envp[1] = "SERVER_SOFTWARE=CUPS/1.0";
  envp[2] = "SERVER_NAME=localhost";	/* This isn't 100% correct... */
  envp[3] = "GATEWAY_INTERFACE=CGI/1.1";
  envp[4] = "SERVER_PROTOCOL=HTTP/1.1";
  envp[5] = server_port;
  envp[6] = remote_host;
  envp[7] = remote_user;
  envp[8] = lang;
  envp[9] = "TZ=GMT";

  if (con->operation == HTTP_GET)
  {
    envp[10] = "REQUEST_METHOD=GET";
    envp[11] = NULL;
  }
  else
  {
    sprintf(content_length, "CONTENT_LENGTH=%d", con->http.data_remaining);
    sprintf(content_type, "CONTENT_TYPE=%s",
            con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

    envp[10] = "REQUEST_METHOD=POST";
    envp[11] = content_length;
    envp[12] = content_type;
    envp[13] = NULL;
  }

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

    setuid(User);
    setgid(Group);

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

    execve(command, argv, envp);
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
 * 'process_ipp_request()' - Process an incoming IPP request...
 */

static void
process_ipp_request(client_t *con)	/* I - Client connection */
{
  ipp_tag_t		group;		/* Current group tag */
  ipp_attribute_t	*attr;		/* Current attribute */
  ipp_attribute_t	*charset;	/* Character set attribute */
  ipp_attribute_t	*language;	/* Language attribute */
  ipp_attribute_t	*uri;		/* Printer URI attribute */
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


  DEBUG_printf(("process_ipp_request(%08x)\n", con));
  DEBUG_printf(("process_ipp_request: operation_id = %04x\n",
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

      DEBUG_puts("process_ipp_request: attribute groups are out of order!");
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
  if (attr != NULL && strcmp(attr->name, "attributes-charset") == 0)
    charset = attr;
  else
    charset = NULL;

  attr = attr->next;
  if (attr != NULL && strcmp(attr->name, "attributes-natural-language") == 0)
    language = attr;
  else
    language = NULL;

  attr = attr->next;
  if (attr != NULL && strcmp(attr->name, "printer-uri") == 0)
    uri = attr;
  else if (attr != NULL && strcmp(attr->name, "job-uri") == 0)
    uri = attr;
  else
    uri = NULL;

  if (charset == NULL || language == NULL || uri == NULL)
  {
   /*
    * Return an error, since attributes-charset,
    * attributes-natural-language, and printer-uri/job-uri are required
    * for all operations.
    */

    DEBUG_printf(("process_ipp_request: missing attributes (%08x, %08x, %08x)!\n",
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

  httpSeparate(uri->values[0].string, method, username, host, &port, resource);

 /*
  * OK, all the checks pass so far; try processing the operation...
  */

  switch (con->request->request.op.operation_id)
  {
    case IPP_PRINT_JOB :
       /*
        * OK, see if the client is sending the document compressed - CUPS
	* doesn't support compression yet...
	*/

        if ((attr = ippFindAttribute(con->request, "compression")) != NULL)
	{
	  DEBUG_puts("process_ipp_request: Unsupported compression attribute!");
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
	  DEBUG_puts("process_ipp_request: No filename!?!");
	  send_ipp_error(con, IPP_BAD_REQUEST);
	  return;
	}

       /*
        * Is the destination valid?
	*/

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

          DEBUG_printf(("process_ipp_request: resource name \'%s\' no good!\n",
	                resource));
          send_ipp_error(con, IPP_BAD_REQUEST);
	  return;
	}

       /*
        * Create the job and set things up...
	*/

        if ((attr = ippFindAttribute(con->request, "job-priority")) != NULL)
          priority = attr->values[0].integer;
	else
	  priority = 50;

        if ((job = AddJob(priority, dest)) == NULL)
	{
	  send_ipp_error(con, IPP_INTERNAL_ERROR);
	  return;
	}

	job->dtype = dtype;

	job->attrs   = con->request;
	con->request = NULL;

	strcpy(con->filename, job->filename);
	con->filename[0] = '\0';

	job->state = IPP_JOB_PENDING;

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
        break;

    case IPP_VALIDATE_JOB :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case IPP_CANCEL_JOB :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case IPP_GET_JOB_ATTRIBUTES :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case IPP_GET_JOBS :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case IPP_GET_PRINTER_ATTRIBUTES :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_GET_DEFAULT :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_GET_PRINTERS :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_ADD_PRINTER :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_DELETE_PRINTER :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_GET_CLASSES :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_ADD_CLASS :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    case CUPS_DELETE_CLASS :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
        break;

    default :
        send_ipp_error(con, IPP_OPERATION_NOT_SUPPORTED);
	return;
  }

  FD_SET(con->http.fd, &OutputSet);
}


/*
 * 'send_ipp_error()' - Send an error status back to the IPP client.
 */

static void
send_ipp_error(client_t     *con,	/* I - Client connection */
               ipp_status_t status)	/* I - IPP status code */
{
  ipp_attribute_t	*attr;		/* Current attribute */


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


/*
 * 'sigpipe_handler()' - Handle 'broken pipe' signals from lost network
 *                       clients.
 */

static void
sigpipe_handler(int sig)	/* I - Signal number */
{
  (void)sig;
/* IGNORE */
}




/**** OBSOLETE ****/

/*
 * 'get_extension()' - Get the extension for a filename.
 */

static char *			/* O - Pointer to extension */
get_extension(char *filename)	/* I - Filename or URI */
{
  char	*ext;			/* Pointer to extension */


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

static char *			/* O - Real filename */
get_file(client_t    *con,	/* I - Client connection */
         struct stat *filestats)/* O - File information */
{
  int		status;		/* Status of filesystem calls */
  char		*params;	/* Pointer to parameters in URI */
  static char	filename[1024];	/* Filename buffer */


 /*
  * Need to add DocumentRoot global...
  */

  if (con->language != NULL)
    sprintf(filename, "%s/%s%s", DocumentRoot, con->language->language,
            con->uri);
  else
    sprintf(filename, "%s%s", DocumentRoot, con->uri);

  if ((params = strchr(filename, '?')) != NULL)
    *params = '\0';

 /*
  * Grab the status for this language; if there isn't a language-specific file
  * then fallback to the default one...
  */

  if ((status = stat(filename, filestats)) != 0 && con->language != NULL)
  {
   /*
    * Drop the language prefix and try the current directory...
    */

    sprintf(filename, "%s%s", DocumentRoot, con->uri);

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
  }

  LogMessage(LOG_DEBUG, "get_filename() %d filename=%s size=%d",
             con->http.fd, filename, filestats->st_size);

  if (status)
    return (NULL);
  else
    return (filename);
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
 * 'show_printer_status()' - Show the current printer status.
 */

static int
show_printer_status(client_t *con)
{
  printer_t	*p;
  char		buffer[HTTP_MAX_BUFFER];
  static char	*states[] =
		{
		  "", "", "", "Idle", "Processing", "Stopped"
		};


  p = NULL;
  if (con->uri[10] != '\0')
    if ((p = FindPrinter(con->uri + 10)) == NULL)
      return (0);

  if (!SendHeader(con, HTTP_OK, "text/html"))
    return (0);

  if (con->http.version == HTTP_1_1)
  {
    con->http.data_encoding = HTTP_ENCODE_CHUNKED;

    if (httpPrintf(HTTP(con), "Transfer-Encoding: chunked\r\n") < 0)
      return (0);
  }

  if (httpPrintf(HTTP(con), "\r\n") < 0)
    return (0);

  if (p != NULL)
  {
   /*
    * Send printer information...
    */

    sprintf(buffer, "<HTML>\n"
                    "<HEAD>\n"
		    "\t<TITLE>Printer Status for %s</TITLE>\n"
		    "\t<META HTTP-EQUIV=\"Refresh\" CONTENT=\"10\">\n"
		    "</HEAD>\n"
		    "<BODY BGCOLOR=#ffffff>\n"
		    "<TABLE WIDTH=100%%><TR>\n"
		    "\t<TD ALIGN=LEFTV ALIGN=MIDDLE><IMG SRC=\"/images/cups-small.gif\"></TD>\n"
		    "\t<TD ALIGN=CENTER VALIGN=MIDDLE><H1>Printer Status for %s</H1></TD>\n"
		    "\t<TD ALIGN=RIGHT VALIGN=MIDDLE><IMG SRC=\"/images/vendor.gif\"></TD>\n"
		    "</TR></TABLE>\n"
                    "<CENTER><TABLE BORDER=1 WIDTH=80%%>\n"
                    "<TR><TH>Name</TH><TH>Value</TH></TR>\n"
		    "<TR><TD>Info</TD><TD>%s</TD></TR>\n"
		    "<TR><TD>MoreInfo</TD><TD>%s</TD></TR>\n"
		    "<TR><TD>Location</TD><TD>%s</TD></TR>\n"
		    "<TR><TD>Device URI</TD><TD>%s</TD></TR>\n"
		    "<TR><TD>PPDFile</TD><TD>%s</TD></TR>\n",
		    p->name, p->name, p->info, p->more_info,
		    p->location, p->device_uri, p->ppd);

    con->bytes += httpWrite(HTTP(con), buffer, strlen(buffer));
  }
  else
  {
    strcpy(buffer, "<HTML>\n"
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

    con->bytes += httpWrite(HTTP(con), buffer, strlen(buffer));

    for (p = Printers; p != NULL; p = p->next)
    {
      sprintf(buffer, "<TR><TD><A HREF=/printers/%s>%s</A></TD><TD>%s</TD><TD>%s</TD></TR>\n",
	      p->name, p->name, states[p->state], p->info);
      con->bytes += httpWrite(HTTP(con), buffer, strlen(buffer));
    }
  }

  strcpy(buffer, "</TABLE></CENTER>\n"
                 "<HR>\n"
		 "CUPS Copyright 1997-1999 by Easy Software Products, "
		 "All Rights Reserved.  CUPS and the CUPS logo are the "
		 "trademark property of Easy Software Products.\n"
		 "</BODY>\n"
                 "</HTML>\n");
  con->bytes += httpWrite(HTTP(con), buffer, strlen(buffer));
  httpWrite(HTTP(con), buffer, 0);

  return (1);
}


/*
 * End of "$Id: client.c,v 1.5 1999/02/26 15:11:11 mike Exp $".
 */
