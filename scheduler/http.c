/*
 * "$Id: http.c,v 1.6 1998/10/16 18:28:01 mike Exp $"
 *
 *   HTTP routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-1998 by Easy Software Products, all rights reserved.
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
 *   conprintf()          - Do a printf() to a client...
 *   decode_basic_auth()  - Decode a Basic authorization string.
 *   decode_digest_auth() - Decode an MD5 Digest authorization string.
 *   get_datetime()       - Get a data/time string for the given time.
 *   get_extension()      - Get the extension for a filename.
 *   get_file()           - Get a filename and state info.
 *   get_line()           - Get a request line terminated with a CR and LF.
 *   get_message()        - Get a message string for the given HTTP code.
 *   get_type()           - Get MIME type from the given extension.
 *   sigpipe_handler()    - Handle 'broken pipe' signals from lost network
 *                          clients.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>


/*
 * Local globals...
 */

static char	*days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char	*months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


/*
 * Local functions...
 */

static int	conprintf(client_t *con, char *format, ...);
static void	decode_basic_auth(client_t *con, char *line);
static void	decode_if_modified(client_t *con, char *line);
static char	*get_datetime(time_t t);
static char	*get_extension(char *filename);
static char	*get_file(client_t *con, struct stat *filestats);
static char	*get_line(client_t *con, char *line, int length);
static char	*get_long_message(int code);
static char	*get_message(int code);
static char	*get_type(char *extension);
static int	pipe_command(int infile, int *outfile, char *command);
static void	sigpipe_handler(int sig);


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
  con->activity = time(NULL);

 /*
  * Accept the client and get the remote address...
  */

  val = sizeof(struct sockaddr_in);

  if ((con->fd = accept(lis->fd, (struct sockaddr *)&(con->remote), &val)) < 0)
  {
    fprintf(stderr, "cupsd: Client acceptance failed - %s\n",
            strerror(errno));
    return;
  }

 /*
  * Get the hostname or format the IP address as needed...
  */

  address = ntohl(con->remote.sin_addr.s_addr);

  if (HostNameLookups)
    host = gethostbyaddr(&address, sizeof(address), AF_INET);
  else
    host = NULL;

  if (host == NULL)
    sprintf(con->remote_host, "%d.%d.%d.%d", (address >> 24) & 255,
            (address >> 16) & 255, (address >> 8) & 255, address & 255);
  else
    strncpy(con->remote_host, host->h_name, sizeof(con->remote_host) - 1);

  fprintf(stderr, "cupsd: New client %d from %s accepted.\n",
          con->fd, con->remote_host);

 /*
  * Add the socket to the select() input mask.
  */

  fcntl(con->fd, F_SETFD, fcntl(con->fd, F_GETFD) | FD_CLOEXEC);

  FD_SET(con->fd, &InputSet);

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


  fprintf(stderr, "cupsd: Closed client #%d\n", con->fd);

 /*
  * Close the socket and clear the file from the input set for select()...
  */

#ifdef WIN32		/* Windows doesn't have a unified IO system... */
  closesocket(con->fd);
#else
  close(con->fd);
#endif /* WIN32 */

  for (i = 0; i < NumListeners; i ++)
    FD_SET(Listeners[i].fd, &InputSet);

  FD_CLR(con->fd, &InputSet);
  if (con->pipe_pid != 0)
    FD_CLR(con->file, &InputSet);
  FD_CLR(con->fd, &OutputSet);

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
  };

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
  char		line[1024],		/* Line from socket... */
		name[256],		/* Name on request line */
		value[1024],		/* Value on request line */
		version[64],		/* Protocol version on request line */
		*valptr;		/* Pointer to value */
  int		major, minor;		/* HTTP version numbers */
  int		start;			/* TRUE if we need to start the transfer */
  int		code;			/* Authorization code */
  char		*filename,		/* Name of file for GET/HEAD */
		*extension,		/* Extension of file */
		*type;			/* MIME type */
  struct stat	filestats;		/* File information */


  start = 0;

  switch (con->state)
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

        con->activity        = time(NULL);
        con->version         = HTTP_1_0;
	con->keep_alive      = 0;
	con->data_encoding   = HTTP_DATA_SINGLE;
	con->data_length     = 0;
	con->file            = 0;
	con->pipe_pid        = 0;
        con->host[0]         = '\0';
	con->user_agent[0]   = '\0';
	con->username[0]     = '\0';
	con->password[0]     = '\0';
	con->uri[0]          = '\0';
	con->content_type[0] = '\0';
	con->remote_time     = 0;
	con->remote_size     = 0;

	strcpy(con->language, "en");

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
	      con->version = HTTP_0_9;
	      break;
	  case 3 :
	      sscanf(version, "HTTP/%d.%d", &major, &minor);

	      if (major == 1 && minor == 1)
	      {
	        con->version    = HTTP_1_1;
		con->keep_alive = 1;
	      }
	      else if (major == 1 && minor == 0)
	        con->version = HTTP_1_0;
	      else if (major == 0 && minor == 9)
	        con->version == HTTP_0_9;
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

	  strcpy(con->uri, valptr);
	}
	else
	  strcpy(con->uri, value);

       /*
        * Process the request...
	*/

        if (strcmp(name, "GET") == 0)
	{
	  con->state = HTTP_GET;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "PUT") == 0)
	{
	  con->state = HTTP_PUT;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "POST") == 0)
	{
	  con->state = HTTP_POST;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "DELETE") == 0)
	{
	  con->state = HTTP_DELETE;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "TRACE") == 0)
	{
	  con->state = HTTP_TRACE;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "CLOSE") == 0)
	{
	  con->state = HTTP_CLOSE;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "OPTIONS") == 0)
	{
	  con->state = HTTP_OPTIONS;
	  start      = (con->version == HTTP_0_9);
	}
        else if (strcmp(name, "HEAD") == 0)
	{
	  con->state = HTTP_HEAD;
	  start      = (con->version == HTTP_0_9);
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

	if (strcmp(name, "Content-Type") == 0)
	{
	  strncpy(con->content_type, value, sizeof(con->content_type) - 1);
	  con->content_type[sizeof(con->content_type) - 1] = '\0';
	}
	else if (strcmp(name, "Content-Length") == 0)
	{
	  con->data_encoding = HTTP_DATA_SINGLE;
	  con->data_length   = atoi(value);
	}
	else if (strcmp(name, "Accept-Language") == 0)
	{
	  strncpy(con->language, value, sizeof(con->language) - 1);
	  con->language[sizeof(con->language) - 1] = '\0';

         /*
	  * Strip trailing data in language string...
	  */

	  for (valptr = con->language; *valptr != '\0'; valptr ++)
	    if (!isalnum(*valptr) && *valptr != '-')
	    {
	      *valptr = '\0';
	      break;
	    }
	}
	else if (strcmp(name, "Authorization") == 0)
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
	else if (strcmp(name, "Transfer-Encoding") == 0)
	{
	  if (strcmp(value, "chunked") == 0)
	  {
	    con->data_encoding = HTTP_DATA_CHUNKED;
	    con->data_length   = 0;
          }
	  else
	  {
	    SendError(con, HTTP_NOT_IMPLEMENTED);
	    CloseClient(con);
	    return (0);
	  }
	}
	else if (strcmp(name, "User-Agent") == 0)
	{
	  strncpy(con->user_agent, value, sizeof(con->user_agent) - 1);
	  con->user_agent[sizeof(con->user_agent) - 1] = '\0';
	}
	else if (strcmp(name, "Host") == 0)
	{
	  strncpy(con->host, value, sizeof(con->host) - 1);
	  con->host[sizeof(con->host) - 1] = '\0';
	}
	else if (strcmp(name, "Connection") == 0)
	{
	  if (strcmp(value, "Keep-Alive") == 0)
	    con->keep_alive = 1;
	}
	else if (strcmp(name, "If-Modified-Since") == 0)
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
    if (con->host[0] == '\0' && con->version >= HTTP_1_0)
    {
      if (!SendError(con, HTTP_BAD_REQUEST))
      {
	CloseClient(con);
	return (0);
      }
    }
    else if ((code = IsAuthorized(con)) != HTTP_OK)
    {
      if (!SendError(con, code))
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
    else switch (con->state)
    {
      case HTTP_GET :
	  if (strncmp(con->uri, "/printers", 9) == 0)
	  {
	   /*
	    * Do a command...
	    */

	    if (strlen(con->uri) > 9)
	      sprintf(line, "lpstat -p %s -o %s", con->uri + 10, con->uri + 10);
	    else
	      strcpy(line, "lpstat -d -p -o");

	    if (!SendCommand(con, HTTP_OK, line, "text/plain"))
	    {
	      CloseClient(con);
	      return (0);
	    }

            con->state = HTTP_GET_DATA;

	    if (con->data_length == 0 &&
	        con->data_encoding == HTTP_DATA_SINGLE &&
		con->version <= HTTP_1_0)
	      con->keep_alive = 0;
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
	    else if (filestats.st_size == con->remote_size &&
	             filestats.st_mtime == con->remote_time)
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

              con->state = HTTP_GET_DATA;
	    }
	  }
          break;

      case HTTP_PUT :
      case HTTP_POST :
      case HTTP_DELETE :
      case HTTP_TRACE :
          SendError(con, HTTP_NOT_IMPLEMENTED);

      case HTTP_CLOSE :
          CloseClient(con);
	  return (0);

      case HTTP_HEAD :
	  if (strncmp(con->uri, "/printers/", 10) == 0)
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
	  else if (filestats.st_size == con->remote_size &&
	           filestats.st_mtime == con->remote_time)
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

          con->state = HTTP_WAITING;
          break;
    }
  }

 /*
  * Handle any incoming data...
  */

  switch (con->state)
  {
    case HTTP_PUT_DATA :
        break;

    case HTTP_POST_DATA :
        break;
  }

  if (!con->keep_alive && con->state == HTTP_WAITING)
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
SendCommand(client_t *con,
            int          code,
	    char         *command,
	    char         *type)
{
  con->pipe_pid = pipe_command(0, &(con->file), command);

  if (con->pipe_pid == 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  FD_SET(con->file, &InputSet);
  FD_SET(con->fd, &OutputSet);

  if (!SendHeader(con, HTTP_OK, type))
    return (0);

  if (con->version == HTTP_1_1)
  {
    con->data_encoding = HTTP_DATA_CHUNKED;

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
SendError(client_t *con,	/* I - Connection */
          int      code)	/* I - Error code */
{
  char	message[1024];		/* Text version of error code */


 /*
  * To work around bugs in some proxies, don't use Keep-Alive for some
  * error messages...
  */

  if (code >= 400)
    con->keep_alive = 0;

 /*
  * Send an error message back to the client.  If the error code is a
  * 400 or 500 series, make sure the message contains some text, too!
  */

  if (!SendHeader(con, code, NULL))
    return (0);

  if (code == HTTP_UNAUTHORIZED)
  {
    if (conprintf(con, "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
      return (0);
  }

  if (con->version >= HTTP_1_1 && !con->keep_alive)
  {
    if (conprintf(con, "Connection: close\r\n") < 0)
      return (0);
  }

  if (code >= 400)
  {
   /*
    * Send a human-readable error message.
    */

    sprintf(message, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>"
                     "<BODY><H1>%s</H1>%s</BODY></HTML>\n",
            code, get_message(code), get_message(code),
	    get_long_message(code));

    if (conprintf(con, "Content-Type: text/html\r\n") < 0)
      return (0);
    if (conprintf(con, "Content-Length: %d\r\n", strlen(message)) < 0)
      return (0);
    if (conprintf(con, "\r\n") < 0)
      return (0);
    if (send(con->fd, message, strlen(message), 0) < 0)
      return (0);
  }
  else if (conprintf(con, "\r\n") < 0)
    return (0);

  con->state = HTTP_WAITING;

  return (1);
}


/*
 * 'SendFile()' - Send a file via HTTP.
 */

int
SendFile(client_t *con,
         int          code,
	 char         *filename,
	 char         *type,
	 struct stat  *filestats)
{
  con->file = open(filename, O_RDONLY);

  fprintf(stderr, "cupsd: filename=\'%s\', file = %d\n", filename, con->file);

  if (con->file < 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  con->pipe_pid = 0;

  if (!SendHeader(con, code, type))
    return (0);

  if (conprintf(con, "Last-Modified: %s\r\n", get_datetime(filestats->st_mtime)) < 0)
    return (0);
  if (conprintf(con, "Content-Length: %d\r\n", filestats->st_size) < 0)
    return (0);
  if (conprintf(con, "\r\n") < 0)
    return (0);

  FD_SET(con->fd, &OutputSet);

  return (1);
}


/*
 * 'SendHeader()' - Send an HTTP header.
 */

int				/* O - 1 on success, 0 on failure */
SendHeader(client_t *con,	/* I - Client to send to */
           int          code,	/* I - HTTP status code */
	   char         *type)	/* I - MIME type of document */
{
  if (conprintf(con, "HTTP/%d.%d %d %s\r\n", con->version / 100,
                con->version % 100, code, get_message(code)) < 0)
    return (0);
  if (conprintf(con, "Date: %s\r\n", get_datetime(time(NULL))) < 0)
    return (0);
  if (conprintf(con, "Server: CUPS/1.0\r\n") < 0)
    return (0);
  if (con->keep_alive && con->version == HTTP_1_0)
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
WriteClient(client_t *con)
{
  int	bytes;
  int	status;
  char	buf[MAX_BUFFER];


  if (con->state != HTTP_GET_DATA &&
      con->state != HTTP_POST_DATA)
    return (1);

  if ((bytes = read(con->file, buf, sizeof(buf))) > 0)
  {
    if (con->data_encoding == HTTP_DATA_CHUNKED)
    {
      if (conprintf(con, "%d\r\n", bytes) < 0)
      {
        CloseClient(con);
	return (0);
      }

      if (send(con->fd, buf, bytes, 0) < 0)
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
    else if (send(con->fd, buf, bytes, 0) < 0)
    {
      CloseClient(con);
      return (0);
    }
  }
  else
  {
    if (con->data_encoding == HTTP_DATA_CHUNKED)
    {
      if (conprintf(con, "0\r\n\r\n") < 0)
      {
        CloseClient(con);
	return (0);
      }
    }

    FD_CLR(con->fd, &OutputSet);
    FD_CLR(con->file, &InputSet);

    if (con->pipe_pid)
    {
      kill(con->pipe_pid, SIGKILL);
      waitpid(con->pipe_pid, &status, WNOHANG);
    }

    close(con->file);

    if (!con->keep_alive)
    {
      CloseClient(con);
      return (0);
    }

    con->state    = HTTP_WAITING;
    con->file     = 0;
    con->pipe_pid = 0;
  }

  fprintf(stderr, "cupsd: SEND %d bytes to #%d\n", bytes, con->fd);

  con->activity = time(NULL);

  return (1);
}


/*
 * 'conprintf()' - Do a printf() to a client...
 */

static int			/* O - Number of bytes written or -1 on error */
conprintf(client_t *con,	/* I - Client to write to */
          char         *format,	/* I - printf()-style format string */
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

  con->activity = time(NULL);

  return (send(con->fd, buf, bytes, 0));
}


/*
 * 'decode_basic_auth()' - Decode a Basic authorization string.
 */

static void
decode_basic_auth(client_t *con,	/* I - Client to decode to */
                  char         *line)	/* I - Line to decode */
{
  int	pos,				/* Bit position */
	base64;				/* Value of this character */
  char	value[1024],			/* Value string */
	*valptr;			/* Pointer into value string */


  for (valptr = value, pos = 0; *line != '\0'; line ++)
  {
   /*
    * Decode this character into a number from 0 to 63...
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

  fprintf(stderr, "cupsd: Decoded authorization string = %s\n", value);

  sscanf(value, "%[^:]:%[^\n]", con->username, con->password);

  fprintf(stderr, "cupsd: username = %s, password = %s\n",
          con->username, con->password);
}


/*
 * 'decode_if_modified()' - Decode an "If-Modified-Since" line.
 */

static void
decode_if_modified(client_t *con,
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
      con->remote_size = atoi(value + 7);
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

  con->remote_time = mktime(&date);
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
get_file(client_t *con,
         struct stat  *filestats)
{
  int		status;
  char		*params;
  static char	filename[1024];


 /*
  * Need to add DocumentRoot global...
  */

#define DocumentRoot "/development/CUPS/www"

  if (con->language[0] != '\0')
    sprintf(filename, "%s/%s%s", DocumentRoot, con->language, con->uri);
  else
    sprintf(filename, "%s%s", DocumentRoot, con->uri);

  if ((params = strchr(filename, '?')) != NULL)
    *params = '\0';

 /*
  * Grab the status for this language; if there isn't a language-specific file
  * then fallback to the default one...
  */

  if ((status = stat(filename, filestats)) != 0 && con->language[0] != '\0')
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
get_line(client_t *con,
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
  bufptr  = con->buf;
  bufend  = con->buf + con->bufused;
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

      con->bufused -= bytes;
      if (con->bufused > 0)
	memcpy(con->buf, bufptr, con->bufused);

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
 * 'get_long_message()' - Get a long message string for the given HTTP code.
 */

static char *			/* O - Message string */
get_long_message(int code)	/* I - Message code */
{
  switch (code)
  {
    case HTTP_BAD_REQUEST :
        return ("The server reported that a bad or incomplete request was received.");
    case HTTP_UNAUTHORIZED :
        return ("You must provide a valid username and password to access this page.");
    case HTTP_FORBIDDEN :
        return ("You are not allowed to access this page.");
    case HTTP_NOT_FOUND :
        return ("The specified file or directory was not found.");
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
 * 'get_message()' - Get a message string for the given HTTP code.
 */

static char *		/* O - Message string */
get_message(int code)	/* I - Message code */
{
  switch (code)
  {
    case HTTP_OK :
        return ("OK");
    case HTTP_CREATED :
        return ("Created");
    case HTTP_ACCEPTED :
        return ("Accepted");
    case HTTP_NO_CONTENT :
        return ("No Content");
	break;
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
 * 'sigpipe_handler()' - Handle 'broken pipe' signals from lost network
 *                       clients.
 */

static void
sigpipe_handler(int sig)	/* I - Signal number */
{
/* IGNORE */
}


/*
 * End of "$Id: http.c,v 1.6 1998/10/16 18:28:01 mike Exp $".
 */
