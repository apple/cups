/*
 * "$Id: client.c,v 1.66 2000/09/05 19:45:08 mike Exp $"
 *
 *   Client routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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
 *   WriteClient()         - Write data to a client as needed.
 *   check_if_modified()   - Decode an "If-Modified-Since" line.
 *   decode_auth()         - Decode an authorization string.
 *   get_file()            - Get a filename and state info.
 *   pipe_command()        - Pipe the output of a command to the remote client.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"


/*
 * Local functions...
 */

static int	check_if_modified(client_t *con, struct stat *filestats);
static void	decode_auth(client_t *con);
static char	*get_file(client_t *con, struct stat *filestats);
static int	pipe_command(client_t *con, int infile, int *outfile, char *command, char *options);


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


  DEBUG_printf(("AcceptClient(%08x) %d NumClients = %d\n",
                lis, lis->fd, NumClients));

 /*
  * Make sure we don't have a full set of clients already...
  */

  if (NumClients == MaxClients)
    return;

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

  if ((con->http.fd = accept(lis->fd, (struct sockaddr *)&(con->http.hostaddr),
                             &val)) < 0)
  {
    LogMessage(L_ERROR, "accept() failed - %s.", strerror(errno));
    return;
  }

  con->http.hostaddr.sin_port = lis->address.sin_port;

 /*
  * Get the hostname or format the IP address as needed...
  */

  address = ntohl(con->http.hostaddr.sin_addr.s_addr);

  if (HostNameLookups)
#ifndef __sgi
    host = gethostbyaddr((char *)&(con->http.hostaddr.sin_addr),
                         sizeof(struct in_addr), AF_INET);
#else
    host = gethostbyaddr(&(con->http.hostaddr.sin_addr),
                         sizeof(struct in_addr), AF_INET);
#endif /* !__sgi */
  else
    host = NULL;

  if (host == NULL)
    sprintf(con->http.hostname, "%d.%d.%d.%d", (address >> 24) & 255,
            (address >> 16) & 255, (address >> 8) & 255, address & 255);
  else
    strncpy(con->http.hostname, host->h_name, sizeof(con->http.hostname) - 1);

  LogMessage(L_DEBUG, "accept() %d from %s:%d.", con->http.fd,
             con->http.hostname, ntohs(con->http.hostaddr.sin_port));

 /*
  * Add the socket to the select() input mask.
  */

  fcntl(con->http.fd, F_SETFD, fcntl(con->http.fd, F_GETFD) | FD_CLOEXEC);

  DEBUG_printf(("AcceptClient: Adding fd %d to InputSet...\n", con->http.fd));
  FD_SET(con->http.fd, &InputSet);

  NumClients ++;

 /*
  * Temporarily suspend accept()'s until we lose a client...
  */

  if (NumClients == MaxClients)
    for (i = 0; i < NumListeners; i ++)
    {
      DEBUG_printf(("AcceptClient: Removing fd %d from InputSet...\n", Listeners[i].fd));
      FD_CLR(Listeners[i].fd, &InputSet);
    }
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


  LogMessage(L_DEBUG, "CloseClient() %d", con->http.fd);

 /*
  * Close the socket and clear the file from the input set for select()...
  */

  if (con->http.fd > 0)
  {
    DEBUG_printf(("CloseClient: Removing fd %d from InputSet...\n", con->http.fd));
    close(con->http.fd);
    FD_CLR(con->http.fd, &InputSet);
    FD_CLR(con->http.fd, &OutputSet);
    con->http.fd = 0;
  }

  if (con->pipe_pid != 0)
  {
    DEBUG_printf(("CloseClient: Removing fd %d from InputSet...\n", con->file));
    FD_CLR(con->file, &InputSet);
  }

  if (con->file)
  {
   /*
    * Close the open data file...
    */

    if (con->pipe_pid)
    {
      kill(con->pipe_pid, SIGKILL);
      waitpid(con->pipe_pid, &status, WNOHANG);
    }

    FD_CLR(con->file, &InputSet);
    close(con->file);
    con->file = 0;
  }

 /*
  * Re-enable new client connections if we are going back under the
  * limit...
  */

  if (NumClients == MaxClients)
  {

    for (i = 0; i < NumListeners; i ++)
    {
      DEBUG_printf(("CloseClient: Adding fd %d to InputSet...\n", Listeners[i].fd));
      FD_SET(Listeners[i].fd, &InputSet);
    }
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
  ipp_state_t   ipp_state;	/* State of IPP transfer */
  int		bytes;		/* Number of bytes to POST */
  char		*filename;	/* Name of file for GET/HEAD */
  struct stat	filestats;	/* File information */
  mime_type_t	*type;		/* MIME type of file */
  printer_t	*p;		/* Printer */
  location_t	*best;		/* Best match for authentication */
  static unsigned request_id = 0;/* Request ID for temp files */


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
	con->command[0]          = '\0';
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

        switch (sscanf(line, "%63s%1023s%63s", operation, con->uri, version))
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

        LogMessage(L_DEBUG, "ReadClient() %d %s %s HTTP/%d.%d", con->http.fd,
	           operation, con->uri,
		   con->http.version / 100, con->http.version % 100);

	con->http.status = HTTP_OK;

    case HTTP_OPTIONS :
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

    default :
        break; /* Anti-compiler-warning-code */
  }

 /*
  * Handle new transfers...
  */

  if (status == HTTP_OK)
  {
    con->language = cupsLangGet(con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE]);

    decode_auth(con);

    if (strncmp(con->http.fields[HTTP_FIELD_CONNECTION], "Keep-Alive", 10) == 0)
      con->http.keep_alive = HTTP_KEEPALIVE_ON;

    if (con->http.fields[HTTP_FIELD_HOST][0] == '\0' &&
        con->http.version >= HTTP_1_1)
    {
     /*
      * HTTP/1.1 and higher require the "Host:" field...
      */

      if (!SendError(con, HTTP_BAD_REQUEST))
      {
	CloseClient(con);
	return (0);
      }
    }
    else if (con->operation == HTTP_OPTIONS)
    {
     /*
      * Do OPTIONS command...
      */

      if ((best = FindBest(con)) != NULL &&
          best->type != AUTH_NONE)
      {
	if (!SendHeader(con, HTTP_UNAUTHORIZED, NULL))
	{
	  CloseClient(con);
	  return (0);
	}
      }
      else
      {
	if (!SendHeader(con, HTTP_OK, NULL))
	{
	  CloseClient(con);
	  return (0);
	}
      }

      httpPrintf(HTTP(con), "Allow: GET, HEAD, OPTIONS, POST\r\n");
      httpPrintf(HTTP(con), "\r\n");
    }
    else if (strstr(con->uri, "..") != NULL)
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
      SendError(con, status);
      CloseClient(con);
      return (0);
    }
    else switch (con->http.state)
    {
      case HTTP_GET_SEND :
          if (strncmp(con->uri, "/printers/", 10) == 0 &&
	      strcmp(con->uri + strlen(con->uri) - 4, ".ppd") == 0)
	  {
	   /*
	    * Send PPD file - get the real printer name since printer
	    * names are not case sensitive but filename can be...
	    */

            con->uri[strlen(con->uri) - 4] = '\0';	/* Drop ".ppd" */

            if ((p = FindPrinter(con->uri + 10)) != NULL)
	      sprintf(con->uri, "/ppd/%s.ppd", p->name);
	    else
	    {
	      if (!SendError(con, HTTP_NOT_FOUND))
	      {
	        CloseClient(con);
		return (0);
	      }

	      break;
	    }
	  }

	  if (strncmp(con->uri, "/admin", 6) == 0 ||
	      strncmp(con->uri, "/printers", 9) == 0 ||
	      strncmp(con->uri, "/classes", 8) == 0 ||
	      strncmp(con->uri, "/jobs", 5) == 0)
	  {
	   /*
	    * Send CGI output...
	    */

            if (strncmp(con->uri, "/admin", 6) == 0)
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/admin.cgi", ServerBin);
	      con->options = con->uri + 6;
	    }
            else if (strncmp(con->uri, "/printers", 9) == 0)
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/printers.cgi", ServerBin);
	      con->options = con->uri + 9;
	    }
	    else if (strncmp(con->uri, "/classes", 8) == 0)
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/classes.cgi", ServerBin);
	      con->options = con->uri + 8;
	    }
	    else
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/jobs.cgi", ServerBin);
	      con->options = con->uri + 5;
	    }

	    if (con->options[0] == '/')
	      con->options ++;

            if (!SendCommand(con, con->command, con->options))
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
	      type = mimeFileType(MimeDatabase, filename);
	      if (type == NULL)
	        strcpy(line, "text/plain");
	      else
	        sprintf(line, "%s/%s", type->super, type->type);

              if (!SendFile(con, HTTP_OK, filename, line, &filestats))
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	  }
          break;

      case HTTP_POST_RECV :
         /*
	  * See if the POST request includes a Content-Length field, and if
	  * so check the length against any limits that are set...
	  */

          LogMessage(L_DEBUG, "POST %s", con->uri);
	  LogMessage(L_DEBUG, "CONTENT_TYPE = %s", con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

          if (con->http.fields[HTTP_FIELD_CONTENT_LENGTH][0] &&
	      atoi(con->http.fields[HTTP_FIELD_CONTENT_LENGTH]) > MaxRequestSize &&
	      MaxRequestSize > 0)
	  {
	   /*
	    * Request too large...
	    */

            if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	    {
	      CloseClient(con);
	      return (0);
	    }

	    break;
          }

         /*
	  * See what kind of POST request this is; for IPP requests the
	  * content-type field will be "application/ipp"...
	  */

	  if (strcmp(con->http.fields[HTTP_FIELD_CONTENT_TYPE], "application/ipp") == 0)
            con->request = ippNew();
	  else if (strncmp(con->uri, "/admin", 6) == 0 ||
	           strncmp(con->uri, "/printers", 9) == 0 ||
	           strncmp(con->uri, "/classes", 8) == 0 ||
	           strncmp(con->uri, "/jobs", 5) == 0)
	  {
	   /*
	    * CGI request...
	    */

            if (strncmp(con->uri, "/admin", 6) == 0)
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/admin.cgi", ServerBin);
	      con->options = con->uri + 6;
	    }
            else if (strncmp(con->uri, "/printers", 9) == 0)
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/printers.cgi", ServerBin);
	      con->options = con->uri + 9;
	    }
	    else if (strncmp(con->uri, "/classes", 8) == 0)
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/classes.cgi", ServerBin);
	      con->options = con->uri + 8;
	    }
	    else
	    {
	      snprintf(con->command, sizeof(con->command),
	               "%s/cgi-bin/jobs.cgi", ServerBin);
	      con->options = con->uri + 5;
	    }

	    if (con->options[0] == '/')
	      con->options ++;

            LogMessage(L_DEBUG, "ReadClient() %d command=\"%s\", options = \"%s\"",
	               con->http.fd, con->command, con->options);

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
          CloseClient(con);
	  return (0);

      case HTTP_HEAD :
          if (strncmp(con->uri, "/printers", 9) == 0 &&
	      strcmp(con->uri + strlen(con->uri) - 4, ".ppd") == 0)
	  {
	   /*
	    * Send PPD file...
	    */

            snprintf(con->command, sizeof(con->command),
	             "/ppd/%s", con->uri + 10);
	    strcpy(con->uri, con->command);
	    con->command[0] = '\0';
	  }

	  if (strncmp(con->uri, "/admin/", 7) == 0 ||
	      strncmp(con->uri, "/printers/", 10) == 0 ||
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

	    type = mimeFileType(MimeDatabase, filename);
	    if (type == NULL)
	      strcpy(line, "text/plain");
	    else
	      sprintf(line, "%s/%s", type->super, type->type);

            if (!SendHeader(con, HTTP_OK, line))
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

          if (httpPrintf(HTTP(con), "\r\n") < 0)
	  {
	    CloseClient(con);
	    return (0);
	  }

          con->http.state = HTTP_WAITING;
          break;

      default :
          break; /* Anti-compiler-warning-code */
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
        LogMessage(L_DEBUG, "ReadClient() %d con->data_encoding = %s, con->data_remaining = %d, con->file = %d",
		   con->http.fd,
		   con->http.data_encoding == HTTP_ENCODE_CHUNKED ? "chunked" : "length",
		   con->http.data_remaining, con->file);

        if (con->request != NULL)
	{
	 /*
	  * Grab any request data from the connection...
	  */

	  if ((ipp_state = ippRead(&(con->http), con->request)) == IPP_ERROR)
	  {
            LogMessage(L_ERROR, "ReadClient() %d IPP Read Error!",
	               con->http.fd);
	    CloseClient(con);
	    return (0);
	  }
	  else if (ipp_state != IPP_DATA)
	    break;
	  else
	    con->bytes += ippLength(con->request);
	}

        if (con->file == 0 && con->http.state != HTTP_POST_SEND)
	{
         /*
	  * Create a file as needed for the request data...
	  */

          snprintf(con->filename, sizeof(con->filename), "%s/%08x",
	           RequestRoot, request_id ++);
	  con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	  fchmod(con->file, 0640);
	  fchown(con->file, User, Group);

          LogMessage(L_DEBUG, "ReadClient() %d REQUEST %s", con->http.fd,
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

	if (con->http.state != HTTP_POST_SEND)
	{
          if ((bytes = httpRead(HTTP(con), line, sizeof(line))) < 0)
	  {
	    CloseClient(con);
	    return (0);
	  }
	  else if (bytes > 0)
	  {
	    con->bytes += bytes;

            LogMessage(L_DEBUG, "ReadClient() %d writing %d bytes",
	               con->http.fd, bytes);

            if (write(con->file, line, bytes) < bytes)
	    {
	      close(con->file);
	      con->file = 0;
	      unlink(con->filename);

              if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	  }
	  else if (con->http.state != HTTP_POST_SEND)
	  {
	    CloseClient(con);
	    return (0);
	  }
	}

	if (con->http.state == HTTP_POST_SEND)
	{
	  if (con->file)
	  {
	    fstat(con->file, &filestats);
	    close(con->file);
	    con->file = 0;

            if (filestats.st_size > MaxRequestSize &&
	        MaxRequestSize > 0)
	    {
	     /*
	      * Request is too big; remove it and send an error...
	      */

	      unlink(con->filename);

	      if (con->request)
	      {
	       /*
	        * Delete any IPP request data...
		*/

	        ippDelete(con->request);
		con->request = NULL;
              }

              if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	      {
		CloseClient(con);
		return (0);
	      }
	    }

	    if (con->command[0])
	    {
	      if (!SendCommand(con, con->command, con->options))
	      {
		if (!SendError(con, HTTP_NOT_FOUND))
		{
	          CloseClient(con);
		  return (0);
		}
              }
	      else
        	LogRequest(con, HTTP_OK);
            }
	  }

          if (con->request)
            ProcessIPPRequest(con);
	}
        break;

    default :
        break; /* Anti-compiler-warning-code */
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
  int	fd;


  if (con->filename[0])
    fd = open(con->filename, O_RDONLY);
  else
    fd = open("/dev/null", O_RDONLY);

  con->pipe_pid = pipe_command(con, fd, &(con->file), command, options);

  close(fd);

  LogMessage(L_DEBUG, "SendCommand() %d command=\"%s\" file=%d pipe_pid=%d",
             con->http.fd, command, con->file, con->pipe_pid);

  if (con->pipe_pid == 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  DEBUG_printf(("SendCommand: Adding fd %d to InputSet...\n", con->file));
  FD_SET(con->file, &InputSet);
  FD_SET(con->http.fd, &OutputSet);

  if (!SendHeader(con, HTTP_OK, NULL))
    return (0);

  if (con->http.version == HTTP_1_1)
  {
    con->http.data_encoding = HTTP_ENCODE_CHUNKED;

    if (httpPrintf(HTTP(con), "Transfer-Encoding: chunked\r\n") < 0)
      return (0);
  }

  con->got_fields = 0;
  con->field_col  = 0;

  return (1);
}


/*
 * 'SendError()' - Send an error message via HTTP.
 */

int				/* O - 1 if successful, 0 otherwise */
SendError(client_t      *con,	/* I - Connection */
          http_status_t code)	/* I - Error code */
{
  char		message[1024];	/* Message for user */


 /*
  * Put the request in the access_log file...
  */

  if (con->operation > HTTP_WAITING)
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

    snprintf(message, sizeof(message),
             "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>"
             "<BODY><H1>%s</H1>%s</BODY></HTML>\n",
             code, httpStatus(code), httpStatus(code),
	     con->language ? con->language->messages[code] :
	 	            httpStatus(code));

    if (httpPrintf(HTTP(con), "Content-Type: text/html\r\n") < 0)
      return (0);
    if (httpPrintf(HTTP(con), "Content-Length: %d\r\n", strlen(message)) < 0)
      return (0);
    if (httpPrintf(HTTP(con), "\r\n") < 0)
      return (0);
    if (httpPrintf(HTTP(con), "%s", message) < 0)
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

  LogMessage(L_DEBUG, "SendFile() %d file=%d", con->http.fd, con->file);

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
  location_t	*loc;		/* Authentication location */


  if (httpPrintf(HTTP(con), "HTTP/%d.%d %d %s\r\n", con->http.version / 100,
                con->http.version % 100, code, httpStatus(code)) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Date: %s\r\n", httpGetDateString(time(NULL))) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Server: CUPS/1.1\r\n") < 0)
    return (0);
  if (con->http.keep_alive && con->http.version >= HTTP_1_0)
  {
    if (httpPrintf(HTTP(con), "Connection: Keep-Alive\r\n") < 0)
      return (0);
    if (httpPrintf(HTTP(con), "Keep-Alive: timeout=%d\r\n", KeepAliveTimeout) < 0)
      return (0);
  }
  if (code == HTTP_METHOD_NOT_ALLOWED)
    if (httpPrintf(HTTP(con), "Allow: GET, HEAD, OPTIONS, POST\r\n") < 0)
      return (0);

  if (code == HTTP_UNAUTHORIZED)
  {
    loc = FindBest(con); /* This already succeeded in IsAuthorized */

    if (loc->type == AUTH_BASIC)
    {
      if (httpPrintf(HTTP(con), "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
	return (0);
    }
    else
    {
      if (httpPrintf(HTTP(con), "WWW-Authenticate: Digest realm=\"CUPS\" "
                                "nonce=\"%s\"\r\n", con->http.hostname) < 0)
	return (0);
    }
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
 * 'WriteClient()' - Write data to a client as needed.
 */

int					/* O - 1 if success, 0 if fail */
WriteClient(client_t *con)		/* I - Client connection */
{
  int		bytes;			/* Number of bytes written */
  char		buf[HTTP_MAX_BUFFER + 1];/* Data buffer */
  char		*bufptr;		/* Pointer into buffer */
  ipp_state_t	ipp_state;		/* IPP state value */


  if (con->http.state != HTTP_GET_SEND &&
      con->http.state != HTTP_POST_SEND)
    return (1);

  if (con->response != NULL)
  {
    ipp_state = ippWrite(&(con->http), con->response);
    bytes     = ipp_state != IPP_ERROR && ipp_state != IPP_DATA;
  }
  else if ((bytes = read(con->file, buf, HTTP_MAX_BUFFER)) > 0)
  {
    if (con->pipe_pid && !con->got_fields)
    {
     /*
      * Inspect the data for Content-Type and other fields.
      */

      buf[bytes] = '\0';

      for (bufptr = buf; !con->got_fields && *bufptr; bufptr ++)
        if (*bufptr == '\n')
	{
	 /*
	  * Send line to client...
	  */

	  if (bufptr > buf && bufptr[-1] == '\r')
	    bufptr[-1] = '\0';
	  *bufptr++ = '\0';

	  httpPrintf(HTTP(con), "%s\r\n", buf);
	  LogMessage(L_DEBUG, "WriteClient() %d %s", con->http.fd, buf);

         /*
	  * Update buffer...
	  */

	  bytes -= (bufptr - buf);
	  memcpy(buf, bufptr, bytes + 1);
	  bufptr = buf - 1;

         /*
	  * See if the line was empty...
	  */

	  if (con->field_col == 0)
	    con->got_fields = 1;
	  else
	    con->field_col = 0;
	}
	else if (*bufptr != '\r')
	  con->field_col ++;

      if (bytes > 0 && !con->got_fields)
      {
       /*
        * Remaining text needs to go out...
	*/

        httpPrintf(HTTP(con), "%s", buf);

        con->http.activity = time(NULL);
        return (1);
      }
      else if (bytes == 0)
      {
        con->http.activity = time(NULL);
        return (1);
      }
    }

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

    con->http.state = HTTP_WAITING;

    FD_CLR(con->http.fd, &OutputSet);

    if (con->file)
    {
      DEBUG_printf(("WriteClient: Removing fd %d from InputSet...\n", con->file));
      FD_CLR(con->file, &InputSet);

      if (con->pipe_pid)
	kill(con->pipe_pid, SIGTERM);

      close(con->file);
      con->file     = 0;
      con->pipe_pid = 0;
    }

    if (con->filename[0])
      unlink(con->filename);

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

    if (!con->http.keep_alive)
    {
      CloseClient(con);
      return (0);
    }
  }

  if (bytes >= 1024)
    LogMessage(L_DEBUG, "WriteClient() %d %d bytes", con->http.fd, bytes);

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

  LogMessage(L_DEBUG, "check_if_modified() %d If-Modified-Since=\"%s\"",
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

  LogMessage(L_DEBUG, "check_if_modified() %d sizes=%d,%d dates=%d,%d",
             con->http.fd, size, filestats->st_size, date, filestats->st_mtime);

  return ((size != filestats->st_size && size != 0) ||
          (date < filestats->st_mtime && date != 0) ||
	  (size == 0 && date == 0));
}


/*
 * 'decode_auth()' - Decode an authorization string.
 */

static void
decode_auth(client_t *con)		/* I - Client to decode to */
{
  char		*s,			/* Authorization string */
		value[1024];		/* Value string */
  const char	*username;		/* Certificate username */


 /*
  * Decode the string...
  */

  s = con->http.fields[HTTP_FIELD_AUTHORIZATION];

  DEBUG_printf(("decode_auth(%08x): Authorization string = \"%s\"\n",
                con, s));

  if (strncmp(s, "Basic", 5) == 0)
  {
    s += 5;
    while (isspace(*s))
      s ++;

    httpDecode64(value, s);

   /*
    * Pull the username and password out...
    */

    if ((s = strchr(value, ':')) == NULL)
    {
      LogMessage(L_DEBUG, "decode_auth() %d no colon in auth string \"%s\"",
        	 con->http.fd, value);
      return;
    }

    *s++ = '\0';

    strncpy(con->username, value, sizeof(con->username) - 1);
    con->username[sizeof(con->username) - 1] = '\0';

    strncpy(con->password, s, sizeof(con->password) - 1);
    con->password[sizeof(con->password) - 1] = '\0';
  }
  else if (strncmp(s, "Local", 5) == 0)
  {
    s += 5;
    while (isspace(*s))
      s ++;

    if ((username = FindCert(s)) != NULL)
      strcpy(con->username, username);
  }
  else if (strncmp(s, "Digest", 5) == 0)
  {
   /*
    * Get the username and password from the Digest attributes...
    */

    if (httpGetSubField(&(con->http), HTTP_FIELD_WWW_AUTHENTICATE, "username",
                        value))
    {
      strncpy(con->username, value, sizeof(con->username) - 1);
      con->username[sizeof(con->username) - 1] = '\0';
    }

    if (httpGetSubField(&(con->http), HTTP_FIELD_WWW_AUTHENTICATE, "response",
                        value))
    {
      strncpy(con->password, value, sizeof(con->password) - 1);
      con->password[sizeof(con->password) - 1] = '\0';
    }
  }

  LogMessage(L_DEBUG, "decode_auth() %d username=\"%s\"",
             con->http.fd, con->username);
  DEBUG_printf(("decode_auth() %d username=\"%s\" password=\"%s\"\n",
                con->http.fd, con->username, con->password));
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

  if (strncmp(con->uri, "/ppd/", 5) == 0)
    snprintf(filename, sizeof(filename), "%s%s", ServerRoot, con->uri);
  else if (con->language != NULL)
    snprintf(filename, sizeof(filename), "%s/%s%s", DocumentRoot, con->language->language,
            con->uri);
  else
    snprintf(filename, sizeof(filename), "%s%s", DocumentRoot, con->uri);

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

    if (strncmp(con->uri, "/ppd/", 5) != 0)
    {
      snprintf(filename, sizeof(filename), "%s%s", DocumentRoot, con->uri);

      status = stat(filename, filestats);
    }
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

  LogMessage(L_DEBUG, "get_file() %d filename=%s size=%d",
             con->http.fd, filename, status ? -1 : filestats->st_size);

  if (status)
    return (NULL);
  else
    return (filename);
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
  int	fd;			/* Looping var */
  int	fds[2];			/* Pipe FDs */
  int	argc;			/* Number of arguments */
  int	envc;			/* Number of environment variables */
  char	argbuf[10240],		/* Argument buffer */
	*argv[100],		/* Argument strings */
	*envp[100];		/* Environment variables */
  char	hostname[1024],		/* Hostname string */
	lang[1024],		/* LANG env variable */
	content_length[1024],	/* CONTENT_LENGTH env variable */
	content_type[1024],	/* CONTENT_TYPE env variable */
	ipp_port[1024],		/* Default listen port */
	server_port[1024],	/* Default server port */
	server_name[1024],	/* Default listen hostname */
	remote_host[1024],	/* REMOTE_HOST env variable */
	remote_user[1024],	/* REMOTE_USER env variable */
	tmpdir[1024],		/* TMPDIR environment variable */
	ldpath[1024],		/* LD_LIBRARY_PATH environment variable */
	datadir[1024],		/* CUPS_DATADIR environment variable */
	root[1024],		/* CUPS_SERVERROOT environment variable */
	query_string[10240];	/* QUERY_STRING env variable */


 /*
  * Copy the command string...
  */

  strncpy(argbuf, options, sizeof(argbuf) - 1);
  argbuf[sizeof(argbuf) - 1] = '\0';

 /*
  * Parse the string; arguments can be separated by + and are terminated
  * by ?...
  */

  argv[0] = argbuf;

  for (commptr = argbuf, argc = 1; *commptr != '\0' && argc < 99; commptr ++)
    if (*commptr == ' ' || *commptr == '+')
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
    else if (*commptr == '?')
      break;

  argv[argc] = NULL;

  if (argv[0][0] == '\0')
    argv[0] = strrchr(command, '/') + 1;

 /*
  * Setup the environment variables as needed...
  */

  gethostname(hostname, sizeof(hostname) - 1);

  sprintf(lang, "LANG=%s", con->language ? con->language->language : "C");
  sprintf(ipp_port, "IPP_PORT=%d", ntohs(con->http.hostaddr.sin_port));
  sprintf(server_port, "SERVER_PORT=%d", ntohs(con->http.hostaddr.sin_port));
  snprintf(server_name, sizeof(server_name), "SERVER_NAME=%s", hostname);
  snprintf(remote_host, sizeof(remote_host), "REMOTE_HOST=%s", con->http.hostname);
  snprintf(remote_user, sizeof(remote_user), "REMOTE_USER=%s", con->username);
  snprintf(tmpdir, sizeof(tmpdir), "TMPDIR=%s", TempDir);
  snprintf(datadir, sizeof(datadir), "CUPS_DATADIR=%s", DataDir);
  snprintf(root, sizeof(root), "CUPS_SERVERROOT=%s", ServerRoot);

  if (getenv("LD_LIBRARY_PATH") != NULL)
    snprintf(ldpath, sizeof(ldpath), "LD_LIBRARY_PATH=%s", getenv("LD_LIBRARY_PATH"));
  else
    ldpath[0] = '\0';

  envp[0]  = "PATH=/bin:/usr/bin";
  envp[1]  = "SERVER_SOFTWARE=CUPS/1.1";
  envp[2]  = "GATEWAY_INTERFACE=CGI/1.1";
  envp[3]  = "SERVER_PROTOCOL=HTTP/1.1";
  envp[4]  = ipp_port;
  envp[5]  = server_name;
  envp[6]  = server_port;
  envp[7]  = remote_host;
  envp[8]  = remote_user;
  envp[9]  = lang;
  envp[10] = TZ;
  envp[11] = tmpdir;
  envp[12] = datadir;
  envp[13] = root;

  envc = 14;

  if (ldpath[0])
    envp[envc ++] = ldpath;

  if (con->operation == HTTP_GET)
  {
    envp[envc ++] = "REQUEST_METHOD=GET";

    if (*commptr)
    {
     /*
      * Add GET form variables after ?...
      */

      *commptr++ = '\0';

      snprintf(query_string, sizeof(query_string), "QUERY_STRING=%s", commptr);
      envp[envc ++] = query_string;
    }
  }
  else
  {
    sprintf(content_length, "CONTENT_LENGTH=%d", con->bytes);
    snprintf(content_type, sizeof(content_type), "CONTENT_TYPE=%s",
             con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

    envp[envc ++] = "REQUEST_METHOD=POST";
    envp[envc ++] = content_length;
    envp[envc ++] = content_type;
  }

  envp[envc] = NULL;

 /*
  * Create a pipe for the output...
  */

  if (pipe(fds))
  {
    LogMessage(L_ERROR, "Unable to create pipes for CGI %s - %s",
               argv[0], strerror(errno));
    return (0);
  }

 /*
  * Then execute the command...
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...  Close stdin if necessary and dup the pipe to stdout.
    */

    setgid(Group);
    setuid(User);

    if (infile)
    {
      close(0);
      dup(infile);
    }

    close(1);
    dup(fds[1]);

   /*
    * Close extra file descriptors...
    */

    for (fd = 3; fd < 1024; fd ++)
      close(fd);

   /*
    * Change umask to restrict permissions on created files...
    */

    umask(077);

   /*
    * Execute the pipe program; if an error occurs, exit with status 1...
    */

    execve(command, argv, envp);
    perror("execve failed");
    exit(errno);
    return (0);
  }
  else if (pid < 0)
  {
   /*
    * Error - can't fork!
    */

    LogMessage(L_ERROR, "Unable to fork for CGI %s - %s", argv[0],
               strerror(errno));

    close(fds[0]);
    close(fds[1]);
    return (0);
  }
  else
  {
   /*
    * Fork successful - return the PID...
    */

    AddCert(pid, con->username);

    LogMessage(L_DEBUG, "CGI %s started - PID = %d", argv[0], pid);

    *outfile = fds[0];
    close(fds[1]);

    return (pid);
  }
}


/*
 * End of "$Id: client.c,v 1.66 2000/09/05 19:45:08 mike Exp $".
 */
