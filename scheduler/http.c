/*
 * "$Id: http.c,v 1.4 1998/10/13 18:27:05 mike Exp $"
 *
 *   HTTP server test code for CUPS.
 *
 * Contents:
 *
 *   StartListening()     - Initialize networking and create a listening
 *                          socket...
 *   AcceptConnection()   - Accept a new connection.
 *   CloseConnection()    - Close a remote connection.
 *   ReadConnection()     - Read data from a connection.
 *   WriteConnection()    - Write data to a connection as needed.
 *   SendCommand()        - Send output from a command via HTTP.
 *   SendError()          - Send an error message via HTTP.
 *   SendFile()           - Send a file via HTTP.
 *   SendHeader()         - Send an HTTP header.
 *   IsAuthorized()       - Check to see if the user is authorized...
 *   conprintf()          - Do a printf() to a connection...
 *   decode_basic_auth()  - Decode a Basic authorization string.
 *   decode_digest_auth() - Decode an MD5 Digest authorization string.
 *   get_datetime()       - Get a data/time string for the given time.
 *   get_extension()      - Get the extension for a filename.
 *   get_file()           - Get a filename and state info.
 *   get_line()           - Get a request line terminated with a CR and LF.
 *   get_type()           - Get MIME type from the given extension.
 *   signal_handler()     - Handle 'broken pipe' signals from lost network
 *                          connections.
 *
 * Revision History:
 *
 *   $Log: http.c,v $
 *   Revision 1.4  1998/10/13 18:27:05  mike
 *   Added Host: line checking & enforcement.
 *
 *   Revision 1.3  1998/10/13  18:24:15  mike
 *   Added activity timeout code.
 *   Added Basic authorization code.
 *   Fixed problem with main loop that would cause a core dump.
 *
 *   Revision 1.2  1998/10/12  15:31:08  mike
 *   Switched from stdio files to file descriptors.
 *   Added FD_CLOEXEC flags to all non-essential files.
 *   Added pipe_command() function.
 *   Added write checks for all writes.
 *
 *   Revision 1.1  1998/10/12  13:57:19  mike
 *   Initial revision
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#ifdef HAVE_SHADOW_H
#  include <shadow.h>
#endif /* HAVE_SHADOW_H */
#include <crypt.h>


/*
 * Local globals...
 */

static char	*days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char	*months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


/*
 * Local functions...
 */

static int	conprintf(connection_t *con, char *format, ...);
static void	decode_basic_auth(connection_t *con, char *line);
static void	decode_if_modified(connection_t *con, char *line);
static char	*get_datetime(time_t t);
static char	*get_extension(char *filename);
static char	*get_file(connection_t *con, struct stat *filestats);
static char	*get_line(connection_t *con, char *line, int length);
static char	*get_type(char *extension);
static int	pipe_command(int infile, int *outfile, char *command);
static void	signal_handler(int sig);


/*
 * 'StartListening()' - Initialize networking and create a listening socket...
 */

void
StartListening(void)
{
  int			val;		/* Parameter value */
  struct servent	*service;	/* Service definition */
  struct sockaddr_in	sock;		/* Socket address */


#ifdef WIN32
 /*
  * Initialize Windows sockets...  This handles loading the DLL, etc. since
  * Windows doesn't have built-in support for sockets like UNIX...
  */

  WSADATA wsadata;	/* Socket data for application from DLL */

  val = WSAStartup(MAKEWORD(1,1), &wsadata);
  if (val != 0)
    exit(1);
#else
 /*
  * Setup a 'broken pipe' signal handler for lost connections.
  */

  sigset(SIGPIPE, signal_handler);
#endif /* !WIN32 */

 /*
  * Try to find the 'ipp' service in the /etc/services file...
  */

  service = getservbyname("ipp", NULL);

 /*
  * Setup socket listener...
  */

  if ((Listener = socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) == -1)
  {
    fprintf(stderr, "cupsd: Unable to open socket - %s\n", strerror(errno));
    exit(errno);
  }

  fcntl(Listener, F_SETFD, fcntl(Listener, F_GETFD) | FD_CLOEXEC);

 /*
  * Set things up to reuse the local address for this port.
  */

  val = 1;
  setsockopt(Listener, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

 /*
  * Bind to the port we found...
  */

  memset((char *)&sock, 0, sizeof(sock));
  sock.sin_addr.s_addr = htonl(INADDR_ANY);
  sock.sin_family      = AF_INET;

  if (service == NULL)
    sock.sin_port = htons(IPP_PORT);
  else
    sock.sin_port = htons(service->s_port);

  if (bind(Listener, (struct sockaddr *)&sock, sizeof(sock)) < 0)
  {
    fprintf(stderr, "cupsd: Unable to bind the \'ipp\' service - %s\n",
            strerror(errno));
    exit(errno);
  }

 /*
  * Listen for new connections.
  */

  if (listen(Listener, SOMAXCONN) < 0)
  {
    fprintf(stderr, "cupsd: Unable to listen for connections - %s\n",
            strerror(errno));
    exit(errno);
  }

 /*
  * Setup the select() input mask to contain the listening socket we have.
  */

  FD_ZERO(&InputSet);
  FD_SET(Listener, &InputSet);

  FD_ZERO(&OutputSet);
}


/*
 * 'AcceptConnection()' - Accept a new connection.
 */

void
AcceptConnection(void)
{
  int		val;	/* Parameter value */
  connection_t	*con;	/* New connection pointer */


 /*
  * Get a pointer to the next available connection...
  */

  con = Connection + NumConnections;

  memset(con, 0, sizeof(connection_t));
  con->activity = time(NULL);

 /*
  * Accept the connection and get the remote address...
  */

  val = sizeof(struct sockaddr_in);

  if ((con->fd = accept(Listener, (struct sockaddr *)&(con->remote), &val)) < 0)
  {
    fprintf(stderr, "cupsd: Connection acceptance failed - %s\n",
            strerror(errno));
    return;
  }
  else
    fprintf(stderr, "cupsd: New connection %d from %08x accepted.\n",
            con->fd, con->remote.sin_addr);

 /*
  * Add the socket to the select() input mask.
  */

  fcntl(con->fd, F_SETFD, fcntl(con->fd, F_GETFD) | FD_CLOEXEC);

  FD_SET(con->fd, &InputSet);

  NumConnections ++;

 /*
  * Temporarily suspend accept()'s until we lose a client...
  */

  if (NumConnections == MAX_CLIENTS)
    FD_CLR(Listener, &InputSet);
}


/*
 * 'CloseConnection()' - Close a remote connection.
 */

void
CloseConnection(connection_t *con)	/* I - Connection to close */
{
  int	status;				/* Exit status of pipe command */


  fprintf(stderr, "cupsd: Closed connection #%d\n", con->fd);

 /*
  * Close the socket and clear the file from the input set for select()...
  */

#ifdef WIN32		/* Windows doesn't have a unified IO system... */
  closesocket(con->fd);
#else
  close(con->fd);
#endif /* WIN32 */

  FD_SET(Listener, &InputSet);
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
  * Compact the list of connections as necessary...
  */

  NumConnections --;

  if (con < (Connection + NumConnections))
    memcpy(con, con + 1, (Connection + NumConnections - con) *
                         sizeof(connection_t));
}


/*
 * 'ReadConnection()' - Read data from a connection.
 */

int					/* O - 1 on success, 0 on error */
ReadConnection(connection_t *con)	/* I - Connection to read from */
{
  char		line[1024],		/* Line from socket... */
		name[256],		/* Name on request line */
		value[1024],		/* Value on request line */
		version[64],		/* Protocol version on request line */
		*valptr;		/* Pointer to value */
  int		major, minor;		/* HTTP version numbers */
  int		start;			/* TRUE if we need to start the transfer */
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
	      CloseConnection(con);
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
	        CloseConnection(con);
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
	  CloseConnection(con);
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
	  CloseConnection(con);
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
	    CloseConnection(con);
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
	    CloseConnection(con);
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
    switch (con->state)
    {
      case HTTP_GET :
          if (con->host[0] == '\0' && con->version >= HTTP_1_0)
	  {
	    if (!SendError(con, HTTP_BAD_REQUEST))
	    {
	      CloseConnection(con);
	      return (0);
	    }
	  }
	  else if (strncmp(con->uri, "/printers", 9) == 0)
	  {
	   /*
	    * Check authorization...
	    */

	    if (!IsAuthorized(con, 1))
	    {
	      if (!SendError(con, HTTP_UNAUTHORIZED))
	      {
	        CloseConnection(con);
		return (0);
	      }
	    }
	    else
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
		CloseConnection(con);
		return (0);
	      }

              con->state = HTTP_GET_DATA;

	      if (con->data_length == 0 &&
	          con->data_encoding == HTTP_DATA_SINGLE &&
		  con->version <= HTTP_1_0)
		con->keep_alive = 0;
	    }
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
	        CloseConnection(con);
		return (0);
	      }
	    }
	    else if (filestats.st_size == con->remote_size &&
	             filestats.st_mtime == con->remote_time)
            {
              if (!SendHeader(con, HTTP_NOT_MODIFIED, NULL))
	      {
		CloseConnection(con);
		return (0);
	      }

	      if (conprintf(con, "\r\n") < 0)
	      {
		CloseConnection(con);
		return (0);
	      }

              con->state = HTTP_WAITING;
	    }
	    else
            {
	      extension = get_extension(filename);
	      type      = get_type(extension);

              if (!SendFile(con, HTTP_OK, filename, type, &filestats))
	      {
		CloseConnection(con);
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
          CloseConnection(con);
	  return (0);

      case HTTP_HEAD :
          if (con->host[0] == '\0' && con->version >= HTTP_1_0)
	  {
	    if (!SendError(con, HTTP_BAD_REQUEST))
	    {
	      CloseConnection(con);
	      return (0);
	    }
	  }
	  else if (strncmp(con->uri, "/printers/", 10) == 0)
	  {
	    if (!IsAuthorized(con, 1))
	    {
	      if (!SendError(con, HTTP_UNAUTHORIZED))
	      {
	        CloseConnection(con);
		return (0);
	      }
	    }
	    else
	    {
	     /*
	      * Do a command...
	      */

              if (!SendHeader(con, HTTP_OK, "text/plain"))
	      {
		CloseConnection(con);
		return (0);
	      }

	      if (conprintf(con, "\r\n") < 0)
	      {
		CloseConnection(con);
		return (0);
	      }
	    }
	  }
	  else if (filestats.st_size == con->remote_size &&
	           filestats.st_mtime == con->remote_time)
          {
            if (!SendHeader(con, HTTP_NOT_MODIFIED, NULL))
	    {
              CloseConnection(con);
	      return (0);
	    }

	    if (conprintf(con, "\r\n") < 0)
	    {
	      CloseConnection(con);
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
		CloseConnection(con);
		return (0);
	      }
	    }
	    else
            {
	      extension = get_extension(filename);
	      type      = get_type(extension);

              if (!SendHeader(con, HTTP_OK, type))
	      {
		CloseConnection(con);
		return (0);
	      }

	      if (conprintf(con, "Last-Modified: %s\r\n",
	                    get_datetime(filestats.st_mtime)) < 0)
	      {
		CloseConnection(con);
		return (0);
	      }

	      if (conprintf(con, "Content-Length: %d\r\n", filestats.st_size) < 0)
	      {
		CloseConnection(con);
		return (0);
	      }
	    }
	  }

          if (conprintf(con, "\r\n") < 0)
	  {
	    CloseConnection(con);
	    return (0);
	  }

          if (!con->keep_alive)
	  {
	    CloseConnection(con);
	    return (0);
	  }

          con->state = HTTP_WAITING;
          break;
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

  return (1);
}


/*
 * 'WriteConnection()' - Write data to a connection as needed.
 */

int
WriteConnection(connection_t *con)
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
        CloseConnection(con);
	return (0);
      }

      if (send(con->fd, buf, bytes, 0) < 0)
      {
        CloseConnection(con);
	return (0);
      }

      if (conprintf(con, "\r\n") < 0)
      {
        CloseConnection(con);
	return (0);
      }
    }
    else if (send(con->fd, buf, bytes, 0) < 0)
    {
      CloseConnection(con);
      return (0);
    }
  }
  else
  {
    if (con->data_encoding == HTTP_DATA_CHUNKED)
    {
      if (conprintf(con, "0\r\n\r\n") < 0)
      {
        CloseConnection(con);
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
      CloseConnection(con);
      return (0);
    }

    con->state = HTTP_WAITING;
  }

  fprintf(stderr, "cupsd: SEND %d bytes to #%d\n", bytes, con->fd);

  con->activity = time(NULL);

  return (1);
}


/*
 * 'SendCommand()' - Send output from a command via HTTP.
 */

int
SendCommand(connection_t *con,
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
    if (conprintf(con, "Transfer-Encoding: chunked\r\n") < 0)
      return (0);

  if (conprintf(con, "\r\n") < 0)
    return (0);

  return (1);
}


/*
 * 'SendError()' - Send an error message via HTTP.
 */

int
SendError(connection_t *con,
          int          code)
{
  char		*filename;
  struct stat	filestats;


  sprintf(con->uri, "/errors/%d.html", code);
  filename = get_file(con, &filestats);

  if (filename != NULL)
    return (SendFile(con, code, filename, "text/html", &filestats));

  if (!SendHeader(con, code, NULL))
    return (0);
  if (code == HTTP_UNAUTHORIZED)
    if (conprintf(con, "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
      return (0);
  if (conprintf(con, "\r\n") < 0)
    return (0);

  return (0);
}


/*
 * 'SendFile()' - Send a file via HTTP.
 */

int
SendFile(connection_t *con,
         int          code,
	 char         *filename,
	 char         *type,
	 struct stat  *filestats)
{
  con->file = open(filename, O_RDONLY);

  fprintf(stderr, "cupsd: filename=\'%s\', file = %d\n", filename, con->file);

  if (con->file <= 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  con->pipe_pid = 0;

  if (!SendHeader(con, code, type))
    return (0);

  if (conprintf(con, "Last-Modified: %s\r\n", get_datetime(filestats->st_mtime)) < 0)
    return (0);
  if (conprintf(con, "Content-Length: %d\r\n", filestats->st_size) < 0)
    return (0);
  if (code == HTTP_UNAUTHORIZED)
    if (conprintf(con, "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
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
SendHeader(connection_t *con,	/* I - Connection to send to */
           int          code,	/* I - HTTP status code */
	   char         *type)	/* I - MIME type of document */
{
  char	*message;		/* Textual message for code */


  switch (code)
  {
    case HTTP_OK :
        message = "OK";
	break;
    case HTTP_CREATED :
        message = "CREATED";
	break;
    case HTTP_ACCEPTED :
        message = "ACCEPTED";
	break;
    case HTTP_NO_CONTENT :
        message = "NO CONTENT";
	break;
    case HTTP_NOT_MODIFIED :
        message = "NOT MODIFIED";
	break;
    case HTTP_BAD_REQUEST :
        message = "BAD REQUEST";
	break;
    case HTTP_UNAUTHORIZED :
        message = "UNAUTHORIZED";
	break;
    case HTTP_FORBIDDEN :
        message = "FORBIDDEN";
	break;
    case HTTP_NOT_FOUND :
        message = "NOT FOUND";
	break;
    case HTTP_URI_TOO_LONG :
        message = "URI TOO LONG";
	break;
    case HTTP_NOT_IMPLEMENTED :
        message = "NOT IMPLEMENTED";
	break;
    case HTTP_NOT_SUPPORTED :
        message = "NOT SUPPORTED";
	break;
    default :
        message = "UNKNOWN";
	break;
  }

  if (conprintf(con, "HTTP/%d.%d %d %s\r\n", con->version / 100,
                con->version % 100, code, message) < 0)
    return (0);
  if (conprintf(con, "Date: %s\r\n", get_datetime(time(NULL))) < 0)
    return (0);
  if (conprintf(con, "Server: CUPS/1.0\r\n") < 0)
    return (0);
  if (con->keep_alive && con->version == HTTP_1_0)
  {
    if (conprintf(con, "Connection: Keep-Alive\r\n") < 0)
      return (0);
    if (conprintf(con, "Keep-Alive: timeout=30, max=100\r\n") < 0)
      return (0);
  }
  if (type != NULL)
    if (conprintf(con, "Content-Type: %s\r\n", type) < 0)
      return (0);

  return (1);
}


/*
 * 'IsAuthorized()' - Check to see if the user is authorized...
 */

int				/* O - 1 if authorized, 0 otherwise */
IsAuthorized(connection_t *con,	/* I - Connection */
             int          level)/* I - Access level required */
{
  int		i;		/* Looping var */
  struct passwd	*pw;		/* User password data */
#ifdef HAVE_SHADOW_H
  struct spwd	*spw;		/* Shadow password data */
#endif /* HAVE_SHADOW_H */
  struct group	*grp;		/* Group data */


  if (level == 0)
    return (1);

  if (con->username[0] == '\0' || con->password[0] == '\0')
    return (0);

 /*
  * Check the user's password...
  */

  pw = getpwnam(con->username);	/* Get the current password */
  endpwent();			/* Close the password file */

  if (pw == NULL)		/* No such user... */
    return (0);

  if (pw->pw_passwd[0] == '\0')
    return (0);			/* Don't allow blank passwords! */

#ifdef HAVE_SHADOW_H
  spw = getspnam(con->username);
  endspent();

  if (spw == NULL)		/* No such user or damaged shadow file */
    return (0);

  if (spw->sp_pwdp[0] == '\0')	/* Don't allow blank passwords! */
    return (0);
#endif /* HAVE_SHADOW_H */

 /*
  * OK, the password isn't blank, so compare with what came from the client...
  */

  if (strcmp(pw->pw_passwd, crypt(con->password, pw->pw_passwd)) != 0)
  {
#ifdef HAVE_SHADOW_H
    if (spw != NULL)
    {
      if (strcmp(spw->sp_pwdp, crypt(con->password, spw->sp_pwdp)) != 0)
        return (0);
    }
    else
#endif /* HAVE_SHADOW_H */
      return (0);
  }

 /*
  * OK, the password is good.  See if we need normal user access, or group
  * sys access...
  */

  if (level == 1)
    return (1);

 /*
  * Check to see if this user is in the "sys" group...
  */

  grp = getgrgid(0);
  endgrent();

  if (grp == NULL)		/* No sys group??? */
    return (0);

  for (i = 0; grp->gr_mem[i] != NULL; i ++)
    if (strcmp(con->username, grp->gr_mem[i]) == 0)
      return (1);

  return (0);
}


/*
 * 'conprintf()' - Do a printf() to a connection...
 */

static int			/* O - Number of bytes written or -1 on error */
conprintf(connection_t *con,	/* I - Connection to write to */
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
decode_basic_auth(connection_t *con,	/* I - Connection to decode to */
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
decode_if_modified(connection_t *con,
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
get_file(connection_t *con,
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
get_line(connection_t *con,
         char         *line,
	 int          length)
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
 * 'pipe_command()' - Pipe the output of a command to the remote connection.
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
 * 'signal_handler()' - Handle 'broken pipe' signals from lost network
 *                      connections.
 */

static void
signal_handler(int sig)	/* I - Signal number */
{
/* IGNORE */
}


/*
 * End of "$Id: http.c,v 1.4 1998/10/13 18:27:05 mike Exp $".
 */
