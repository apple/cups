/*
 * "$Id: client.c,v 1.91.2.58 2003/04/10 20:15:49 mike Exp $"
 *
 *   Client routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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
 *   EncryptClient()       - Enable encryption for the client...
 *   IsCGI()               - Is the resource a CGI script/program?
 *   ReadClient()          - Read data from a client.
 *   SendCommand()         - Send output from a command via HTTP.
 *   SendError()           - Send an error message via HTTP.
 *   SendFile()            - Send a file via HTTP.
 *   SendHeader()          - Send an HTTP request.
 *   ShutdownClient()      - Shutdown the receiving end of a connection.
 *   UpdateCGI()           - Read status messages from CGI scripts and programs.
 *   WriteClient()         - Write data to a client as needed.
 *   check_if_modified()   - Decode an "If-Modified-Since" line.
 *   decode_auth()         - Decode an authorization string.
 *   get_file()            - Get a filename and state info.
 *   install_conf_file()   - Install a configuration file.
 *   pipe_command()        - Pipe the output of a command to the remote client.
 *   CDSAReadFunc()        - Read function for CDSA decryption code.
 *   CDSAWriteFunc()       - Write function for CDSA encryption code.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include "cupsd.h"
#include <grp.h>


/*
 * Local functions...
 */

static int		check_if_modified(client_t *con,
			                  struct stat *filestats);
static void		decode_auth(client_t *con);
static char		*get_file(client_t *con, struct stat *filestats, 
			          char *filename, int len);
static http_status_t	install_conf_file(client_t *con);
static int		pipe_command(client_t *con, int infile, int *outfile,
			             char *command, char *options);

#ifdef HAVE_CDSASSL
static OSStatus		CDSAReadFunc(SSLConnectionRef connection, void *data,
			             size_t *dataLength);
static OSStatus		CDSAWriteFunc(SSLConnectionRef connection,
			              const void *data, size_t *dataLength);
#endif /* HAVE_CDSASSL */


/*
 * 'AcceptClient()' - Accept a new client.
 */

void
AcceptClient(listener_t *lis)	/* I - Listener socket */
{
  int			i;	/* Looping var */
  int			count;	/* Count of connections on a host */
  int			val;	/* Parameter value */
  client_t		*con;	/* New client pointer */
  const struct hostent	*host;	/* Host entry for address */
  char			*hostname;/* Hostname for address */
  http_addr_t		temp;	/* Temporary address variable */
  static time_t		last_dos = 0;
				/* Time of last DoS attack */


  LogMessage(L_DEBUG2, "AcceptClient(%p) %d NumClients = %d",
             lis, lis->fd, NumClients);

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
    LogMessage(L_ERROR, "Unable to accept client connection - %s.",
               strerror(errno));
    return;
  }

#ifdef AF_INET6
  if (lis->address.addr.sa_family == AF_INET6)
    con->http.hostaddr.ipv6.sin6_port = lis->address.ipv6.sin6_port;
  else
#endif /* AF_INET6 */
  con->http.hostaddr.ipv4.sin_port = lis->address.ipv4.sin_port;

 /*
  * Check the number of clients on the same address...
  */

  for (i = 0, count = 0; i < NumClients; i ++)
    if (memcmp(&(Clients[i].http.hostaddr), &(con->http.hostaddr),
	       sizeof(con->http.hostaddr)) == 0)
    {
      count ++;
      if (count >= MaxClientsPerHost)
	break;
    }

  if (count >= MaxClientsPerHost)
  {
    if ((time(NULL) - last_dos) >= 60)
    {
      last_dos = time(NULL);
      LogMessage(L_WARN, "Possible DoS attack - more than %d clients connecting from %s!",
	         MaxClientsPerHost, Clients[i].http.hostname);
    }

#ifdef WIN32
    closesocket(con->http.fd);
#else
    close(con->http.fd);
#endif /* WIN32 */

    return;
  }
  
 /*
  * Get the hostname or format the IP address as needed...
  */

  if (HostNameLookups)
    hostname = httpAddrLookup(&(con->http.hostaddr), con->http.hostname,
                              sizeof(con->http.hostname));
  else
  {
    hostname = NULL;
    httpAddrString(&(con->http.hostaddr), con->http.hostname,
                   sizeof(con->http.hostname));
  }

  if (httpAddrLocalhost(&(con->http.hostaddr)))
  {
   /*
    * Map accesses from the loopback interface to "localhost"...
    */

    strlcpy(con->http.hostname, "localhost", sizeof(con->http.hostname));
  }
  else if (httpAddrEqual(&(con->http.hostaddr), &ServerAddr))
  {
   /*
    * Map accesses from the same host to the server name.
    */

    strlcpy(con->http.hostname, ServerName, sizeof(con->http.hostname));
  }

  if (hostname == NULL && HostNameLookups == 2)
  {
   /*
    * Can't have an unresolved IP address with double-lookups enabled...
    */

    LogMessage(L_DEBUG2, "AcceptClient: Closing connection %d...",
               con->http.fd);

#ifdef WIN32
    closesocket(con->http.fd);
#else
    close(con->http.fd);
#endif /* WIN32 */

    LogMessage(L_WARN, "Name lookup failed - connection from %s closed!",
               con->http.hostname);
    return;
  }

  if (HostNameLookups == 2)
  {
   /*
    * Do double lookups as needed...
    */

    if ((host = httpGetHostByName(con->http.hostname)) != NULL)
    {
     /*
      * See if the hostname maps to the same IP address...
      */

      if (host->h_addrtype != con->http.hostaddr.addr.sa_family)
      {
       /*
        * Not the right type of address...
	*/

	host = NULL;
      }
      else
      {
       /*
        * Compare all of the addresses against this one...
	*/

	for (i = 0; host->h_addr_list[i]; i ++)
	{
	  httpAddrLoad(host, 0, i, &temp);

          if (httpAddrEqual(&(con->http.hostaddr), &temp))
	    break;
        }

        if (!host->h_addr_list[i])
	  host = NULL;
      }
    }

    if (host == NULL)
    {
     /*
      * Can't have a hostname that doesn't resolve to the same IP address
      * with double-lookups enabled...
      */

      LogMessage(L_DEBUG2, "AcceptClient: Closing connection %d...",
        	 con->http.fd);

#ifdef WIN32
      closesocket(con->http.fd);
#else
      close(con->http.fd);
#endif /* WIN32 */

      LogMessage(L_WARN, "IP lookup failed - connection from %s closed!",
                 con->http.hostname);
      return;
    }
  }

#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
    LogMessage(L_DEBUG, "AcceptClient: %d from %s:%d.", con->http.fd,
               con->http.hostname, ntohs(con->http.hostaddr.ipv6.sin6_port));
  else
#endif /* AF_INET6 */
  LogMessage(L_DEBUG, "AcceptClient: %d from %s:%d.", con->http.fd,
             con->http.hostname, ntohs(con->http.hostaddr.ipv4.sin_port));

 /*
  * Using TCP_NODELAY improves responsiveness, especially on systems
  * with a slow loopback interface...  Since we write large buffers
  * when sending print files and requests, there shouldn't be any
  * performance penalty for this...
  */

  val = 1;
  setsockopt(con->http.fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)); 

 /*
  * Close this file on all execs...
  */

  fcntl(con->http.fd, F_SETFD, fcntl(con->http.fd, F_GETFD) | FD_CLOEXEC);

 /*
  * Add the socket to the select() input mask.
  */

  LogMessage(L_DEBUG2, "AcceptClient: Adding fd %d to InputSet...",
             con->http.fd);
  FD_SET(con->http.fd, InputSet);

  NumClients ++;

 /*
  * Temporarily suspend accept()'s until we lose a client...
  */

  if (NumClients == MaxClients)
    PauseListening();

#ifdef HAVE_SSL
 /*
  * See if we are connecting on a secure port...
  */

  if (lis->encryption == HTTP_ENCRYPT_ALWAYS)
  {
   /*
    * https connection; go secure...
    */

    con->http.encryption = HTTP_ENCRYPT_ALWAYS;

    EncryptClient(con);
  }
#endif /* HAVE_SSL */
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
  int		status;		/* Exit status of pipe command */
#if defined(HAVE_LIBSSL)
  SSL_CTX	*context;	/* Context for encryption */
  SSL		*conn;		/* Connection for encryption */
  unsigned long	error;		/* Error code */
#elif defined(HAVE_GNUTLS)
  http_tls_t     *conn;		/* TLS connection information */
  int            error;		/* Error code */
  gnutls_certificate_server_credentials *credentials;
				/* TLS credentials */
#endif /* HAVE_GNUTLS */


  LogMessage(L_DEBUG, "CloseClient() %d", con->http.fd);

  if (con->http.input_set)
    free(con->http.input_set);

  httpClearCookie(HTTP(con));

  ClearString(&con->filename);
  ClearString(&con->command);
  ClearString(&con->options);

#ifdef HAVE_SSL
 /*
  * Shutdown encryption as needed...
  */

  if (con->http.tls)
  {
#  ifdef HAVE_LIBSSL
    conn    = (SSL *)(con->http.tls);
    context = SSL_get_SSL_CTX(conn);

    switch (SSL_shutdown(conn))
    {
      case 1 :
          LogMessage(L_INFO, "CloseClient: SSL shutdown successful!");
	  break;
      case -1 :
          LogMessage(L_ERROR, "CloseClient: Fatal error during SSL shutdown!");
      default :
	  while ((error = ERR_get_error()) != 0)
	    LogMessage(L_ERROR, "CloseClient: %s", ERR_error_string(error, NULL));
          break;
    }

    SSL_CTX_free(context);
    SSL_free(conn);

#  elif defined(HAVE_GNUTLS)
    conn        = (http_tls_t *)(con->http.tls);
    credentials = (gnutls_certificate_server_credentials *)(conn->credentials);

    error = gnutls_bye(conn->session, GNUTLS_SHUT_WR);
    switch (error)
    {
      case GNUTLS_E_SUCCESS:
	LogMessage(L_INFO, "CloseClient: SSL shutdown successful!");
	break;
      default:
	LogMessage(L_ERROR, "CloseClient: %s", gnutls_strerror(error));
	break;
    }

    gnutls_deinit(conn->session);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    free(conn);

#  elif defined(HAVE_CDSASSL)
    status = SSLClose((SSLContextRef)con->http.tls);
    SSLDisposeContext((SSLContextRef)con->http.tls);
#  endif /* HAVE_LIBSSL */

    con->http.tls = NULL;
  }
#endif /* HAVE_SSL */

 /*
  * Close the socket and clear the file from the input set for select()...
  */

  if (con->http.fd >= 0)
  {
    LogMessage(L_DEBUG2, "CloseClient: Removing fd %d from InputSet and OutputSet...",
               con->http.fd);
    close(con->http.fd);
    FD_CLR(con->http.fd, InputSet);
    FD_CLR(con->http.fd, OutputSet);
    con->http.fd = 0;
  }

  if (con->pipe_pid != 0)
  {
    LogMessage(L_DEBUG2, "CloseClient: Removing fd %d from InputSet...",
               con->file);
    FD_CLR(con->file, InputSet);
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

    LogMessage(L_DEBUG2, "CloseClient() %d Closing data file %d.",
               con->http.fd, con->file);
    LogMessage(L_DEBUG2, "CloseClient() %d Removing fd %d from InputSet.",
               con->http.fd, con->file);

    FD_CLR(con->file, InputSet);
    close(con->file);
    con->file = 0;
  }

  if (con->request)
  {
    ippDelete(con->request);
    con->request = NULL;
  }

  if (con->response)
  {
    ippDelete(con->response);
    con->response = NULL;
  }

  if (con->language)
  {
    cupsLangFree(con->language);
    con->language = NULL;
  }

 /*
  * Re-enable new client connections if we are going back under the
  * limit...
  */

  if (NumClients == MaxClients)
    ResumeListening();

 /*
  * Compact the list of clients as necessary...
  */

  NumClients --;

  if (con < (Clients + NumClients))
    memcpy(con, con + 1, (Clients + NumClients - con) * sizeof(client_t));
}


/*
 * 'EncryptClient()' - Enable encryption for the client...
 */

int				/* O - 1 on success, 0 on error */
EncryptClient(client_t *con)	/* I - Client to encrypt */
{
#if defined HAVE_LIBSSL
  SSL_CTX	*context;	/* Context for encryption */
  SSL		*conn;		/* Connection for encryption */
  unsigned long	error;		/* Error code */


 /*
  * Create the SSL context and accept the connection...
  */

  context = SSL_CTX_new(SSLv23_server_method());

  SSL_CTX_use_PrivateKey_file(context, ServerKey, SSL_FILETYPE_PEM);
  SSL_CTX_use_certificate_file(context, ServerCertificate, SSL_FILETYPE_PEM);

  conn = SSL_new(context);

  SSL_set_fd(conn, con->http.fd);
  if (SSL_accept(conn) != 1)
  {
    while ((error = ERR_get_error()) != 0)
      LogMessage(L_ERROR, "EncryptClient: %s", ERR_error_string(error, NULL));

    SSL_CTX_free(context);
    SSL_free(conn);
    return (0);
  }

  con->http.tls = conn;
  return (1);
  
#elif defined(HAVE_GNUTLS)
  http_tls_t	*conn;		/* TLS session object */
  int		error;		/* Error code */
  gnutls_certificate_server_credentials *credentials;
				/* TLS credentials */

 /*
  * Create the SSL object and perform the SSL handshake...
  */

  conn = (http_tls_t *)malloc(sizeof(gnutls_session));

  if (conn == NULL)
    return (0);

  credentials = (gnutls_certificate_server_credentials *)
                    malloc(sizeof(gnutls_certificate_server_credentials));
  if (credentials == NULL)
  {
    free(conn);
    return (0);
  }

  gnutls_certificate_allocate_credentials(credentials);
  gnutls_certificate_set_x509_key_file(*credentials, ServerCertificate, 
				       ServerKey, GNUTLS_X509_FMT_PEM);

  gnutls_init(&(conn->session), GNUTLS_SERVER);
  gnutls_set_default_priority(conn->session);
  gnutls_credentials_set(conn->session, GNUTLS_CRD_CERTIFICATE, *credentials);
  gnutls_transport_set_ptr(conn->session, con->http.fd);

  error = gnutls_handshake(conn->session);

  if (error != GNUTLS_E_SUCCESS)
  {
    LogMessage(L_ERROR, "EncryptClient: %s", gnutls_strerror(error));
    gnutls_deinit(conn->session);
    gnutls_certificate_free_credentials(*credentials);
    free(conn);
    free(credentials);
    return (0);
  }

  LogMessage(L_DEBUG, "EncryptClient() %d Connection now encrypted.",
             con->http.fd);

  conn->credentials = credentials;
  con->http.tls = conn;
  return (1);

#elif defined(HAVE_CDSASSL)
  OSStatus		error;		/* Error info */
  SSLContextRef		conn;		/* New connection */
  SSLProtocol		tryVersion;	/* Protocol version */
  const char		*hostName;	/* Local hostname */
  int			allowExpired;	/* Allow expired certificates? */
  int			allowAnyRoot;	/* Allow any root certificate? */
  SSLProtocol		*negVersion;	/* Negotiated protocol version */
  SSLCipherSuite	*negCipher;	/* Negotiated cypher */
  CFArrayRef		*peerCerts;	/* Certificates */


  conn         = NULL;
  error        = SSLNewContext(true, &conn);
  allowExpired = 1;
  allowAnyRoot = 1;

  if (!error)
    error = SSLSetIOFuncs(conn, CDSAReadFunc, CDSAWriteFunc);

  if (!error)
    error = SSLSetProtocolVersion(conn, kSSLProtocol3);

  if (!error)
    error = SSLSetConnection(conn, (SSLConnectionRef)con->http.fd);

  if (!error)
  {
    hostName = ServerName;	/* MRS: ??? */
    error    = SSLSetPeerDomainName(conn, hostName, strlen(hostName) + 1);
  }

  /* have to do these options befor setting server certs */
  if (!error && allowExpired)
    error = SSLSetAllowsExpiredCerts(conn, true);

  if (!error && allowAnyRoot)
    error = SSLSetAllowsAnyRoot(conn, true);

  if (!error && ServerCertificatesArray != NULL)
    error = SSLSetCertificate(conn, ServerCertificatesArray);

 /*
  * Perform SSL/TLS handshake
  */

  do
  {
    error = SSLHandshake(conn);
  }
  while (error == errSSLWouldBlock);

  if (error)
  {
    LogMessage(L_ERROR, "EncryptClient: %d", error);

    con->http.error  = error;
    con->http.status = HTTP_ERROR;

    if (conn != NULL)
      SSLDisposeContext(conn);

    return (0);
  }

  LogMessage(L_DEBUG, "EncryptClient() %d Connection now encrypted.",
             con->http.fd);
  con->http.tls = conn;
  return (1);

#else
  return (0);
#endif /* HAVE_GNUTLS */
}


/*
 * 'IsCGI()' - Is the resource a CGI script/program?
 */

int						/* O - 1 = CGI, 0 = file */
IsCGI(client_t    *con,				/* I - Client connection */
      const char  *filename,			/* I - Real filename */
      struct stat *filestats,			/* I - File information */
      mime_type_t *type)			/* I - MIME type */
{
  const char	*options;			/* Options on URL */


  LogMessage(L_DEBUG2, "IsCGI(con=%p, filename=\"%s\", filestats=%p, type=%s/%s)\n",
             con, filename, filestats, type ? type->super : "unknown",
	     type ? type->type : "unknown");

 /*
  * Get the options, if any...
  */

  if ((options = strchr(con->uri, '?')) != NULL)
    options ++;

 /*
  * Check for known types...
  */

  if (strcasecmp(type->super, "application"))
  {
    LogMessage(L_DEBUG2, "IsCGI: Returning 0...");
    return (0);
  }

  if (!strcasecmp(type->type, "x-httpd-cgi") &&
      (filestats->st_mode & 0111))
  {
   /*
    * "application/x-httpd-cgi" is a CGI script.
    */

    SetString(&con->command, filename);

    filename = strrchr(filename, '/') + 1; /* Filename always absolute */

    if (options)
      SetStringf(&con->options, "%s %s", filename, options);
    else
      SetStringf(&con->options, "%s", filename);

    LogMessage(L_DEBUG2, "IsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
               con->command, con->options);

    return (1);
  }
#ifdef HAVE_JAVA
  else if (!strcasecmp(type->type, "x-httpd-java"))
  {
   /*
    * "application/x-httpd-java" is a Java servlet.
    */

    SetString(&con->command, CUPS_JAVA);

    if (options)
      SetStringf(&con->options, "java %s %s", filename, options);
    else
      SetStringf(&con->options, "java %s", filename);

    LogMessage(L_DEBUG2, "IsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
               con->command, con->options);

    return (1);
  }
#endif /* HAVE_JAVA */
#ifdef HAVE_PERL
  else if (!strcasecmp(type->type, "x-httpd-perl"))
  {
   /*
    * "application/x-httpd-perl" is a Perl page.
    */

    SetString(&con->command, CUPS_PERL);

    if (options)
      SetStringf(&con->options, "perl %s %s", filename, options);
    else
      SetStringf(&con->options, "perl %s", filename);

    LogMessage(L_DEBUG2, "IsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
               con->command, con->options);

    return (1);
  }
#endif /* HAVE_PERL */
#ifdef HAVE_PHP
  else if (!strcasecmp(type->type, "x-httpd-php"))
  {
   /*
    * "application/x-httpd-php" is a PHP page.
    */

    SetString(&con->command, CUPS_PHP);

    if (options)
      SetStringf(&con->options, "php %s %s", filename, options);
    else
      SetStringf(&con->options, "php %s", filename);

    LogMessage(L_DEBUG2, "IsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
               con->command, con->options);

    return (1);
  }
#endif /* HAVE_PHP */
#ifdef HAVE_PYTHON
  else if (!strcasecmp(type->type, "x-httpd-python"))
  {
   /*
    * "application/x-httpd-python" is a Python page.
    */

    SetString(&con->command, CUPS_PYTHON);

    if (options)
      SetStringf(&con->options, "python %s %s", filename, options);
    else
      SetStringf(&con->options, "python %s", filename);

    LogMessage(L_DEBUG2, "IsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
               con->command, con->options);

    return (1);
  }
#endif /* HAVE_PYTHON */

  LogMessage(L_DEBUG2, "IsCGI: Returning 0...");

  return (0);
}


/*
 * 'ReadClient()' - Read data from a client.
 */

int				/* O - 1 on success, 0 on error */
ReadClient(client_t *con)	/* I - Client to read from */
{
  char		line[32768],	/* Line from client... */
		operation[64],	/* Operation code from socket */
		version[64];	/* HTTP version number string */
  int		major, minor;	/* HTTP version numbers */
  http_status_t	status;		/* Transfer status */
  ipp_state_t   ipp_state;	/* State of IPP transfer */
  int		bytes;		/* Number of bytes to POST */
  char		*filename;	/* Name of file for GET/HEAD */
  char		buf[1024];	/* Buffer for real filename */
  struct stat	filestats;	/* File information */
  mime_type_t	*type;		/* MIME type of file */
  printer_t	*p;		/* Printer */
  location_t	*best;		/* Best match for authentication */
  static unsigned request_id = 0;/* Request ID for temp files */


  status = HTTP_CONTINUE;

  LogMessage(L_DEBUG2, "ReadClient() %d, used=%d", con->http.fd,
             con->http.used);

  if (con->http.error)
  {
    CloseClient(con);
    return (0);
  }

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

	ClearString(&con->command);
	ClearString(&con->options);

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
	      LogMessage(L_ERROR, "Bad request line \"%s\"!", line);
	      SendError(con, HTTP_BAD_REQUEST);
	      ShutdownClient(con);
	      return (1);
	  case 2 :
	      con->http.version = HTTP_0_9;
	      break;
	  case 3 :
	      if (sscanf(version, "HTTP/%d.%d", &major, &minor) != 2)
	      {
		LogMessage(L_ERROR, "Bad request line \"%s\"!", line);
		SendError(con, HTTP_BAD_REQUEST);
		ShutdownClient(con);
		return (1);
	      }

	      if (major < 2)
	      {
	        con->http.version = (http_version_t)(major * 100 + minor);
		if (con->http.version == HTTP_1_1 && KeepAlive)
		  con->http.keep_alive = HTTP_KEEPALIVE_ON;
		else
		  con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	      }
	      else
	      {
	        SendError(con, HTTP_NOT_SUPPORTED);
	        ShutdownClient(con);
	        return (1);
	      }
	      break;
	}

       /*
        * Handle full URLs in the request line...
	*/

        if (con->uri[0] != '/' && strcmp(con->uri, "*"))
	{
	  char	method[HTTP_MAX_URI],		/* Method/scheme */
		userpass[HTTP_MAX_URI],		/* Username:password */
		hostname[HTTP_MAX_URI],		/* Hostname */
		resource[HTTP_MAX_URI];		/* Resource path */
          int	port;				/* Port number */


         /*
	  * Separate the URI into its components...
	  */

          httpSeparate(con->uri, method, userpass, hostname, &port, resource);

         /*
	  * Only allow URIs with the servername, localhost, or an IP
	  * address...
	  */

	  if (strcasecmp(hostname, ServerName) &&
	      strcasecmp(hostname, "localhost") &&
	      !isdigit(hostname[0]))
	  {
	   /*
	    * Nope, we don't do proxies...
	    */

	    LogMessage(L_ERROR, "Bad URI \"%s\" in request!", con->uri);
	    SendError(con, HTTP_METHOD_NOT_ALLOWED);
	    ShutdownClient(con);
	    return (1);
	  }

         /*
	  * Copy the resource portion back into the URI; both resource and
	  * con->uri are HTTP_MAX_URI bytes in size...
	  */

          strcpy(con->uri, resource);
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
	  LogMessage(L_ERROR, "Bad operation \"%s\"!", operation);
	  SendError(con, HTTP_BAD_REQUEST);
	  ShutdownClient(con);
	  return (1);
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
	  ShutdownClient(con);
	  return (1);
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

    if (strncmp(con->http.fields[HTTP_FIELD_CONNECTION], "Keep-Alive", 10) == 0 &&
        KeepAlive)
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

      if ((best = FindBest(con->uri, con->http.state)) != NULL &&
          best->type != AUTH_NONE)
      {
	if (!SendHeader(con, HTTP_UNAUTHORIZED, NULL))
	{
	  CloseClient(con);
	  return (0);
	}
      }

      if (strcasecmp(con->http.fields[HTTP_FIELD_CONNECTION], "Upgrade") == 0 &&
	  con->http.tls == NULL)
      {
#ifdef HAVE_SSL
       /*
        * Do encryption stuff...
	*/

	if (!SendHeader(con, HTTP_SWITCHING_PROTOCOLS, NULL))
	{
	  CloseClient(con);
	  return (0);
	}

	httpPrintf(HTTP(con), "Connection: Upgrade\r\n");
	httpPrintf(HTTP(con), "Upgrade: TLS/1.0,HTTP/1.1\r\n");
	httpPrintf(HTTP(con), "Content-Length: 0\r\n");
	httpPrintf(HTTP(con), "\r\n");

        EncryptClient(con);
#else
	if (!SendError(con, HTTP_NOT_IMPLEMENTED))
	{
	  CloseClient(con);
          return (0);
	}
#endif /* HAVE_SSL */
      }

      if (con->http.expect)
      {
        /**** TODO: send expected header ****/
      }

      if (!SendHeader(con, HTTP_OK, NULL))
      {
	CloseClient(con);
	return (0);
      }

      httpPrintf(HTTP(con), "Allow: GET, HEAD, OPTIONS, POST, PUT\r\n");
      httpPrintf(HTTP(con), "Content-Length: 0\r\n");
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
    else
    {
      if (strcasecmp(con->http.fields[HTTP_FIELD_CONNECTION], "Upgrade") == 0 &&
	  con->http.tls == NULL)
      {
#ifdef HAVE_SSL
       /*
        * Do encryption stuff...
	*/

	if (!SendHeader(con, HTTP_SWITCHING_PROTOCOLS, NULL))
	{
	  CloseClient(con);
	  return (0);
	}

	httpPrintf(HTTP(con), "Connection: Upgrade\r\n");
	httpPrintf(HTTP(con), "Upgrade: TLS/1.0,HTTP/1.1\r\n");
	httpPrintf(HTTP(con), "Content-Length: 0\r\n");
	httpPrintf(HTTP(con), "\r\n");

        EncryptClient(con);

	status = IsAuthorized(con);
#else
	if (!SendError(con, HTTP_NOT_IMPLEMENTED))
	{
	  CloseClient(con);
          return (0);
	}
#endif /* HAVE_SSL */
      }

      if (status != HTTP_OK)
      {
        LogMessage(L_DEBUG2, "ReadClient: Unauthorized request for %s...\n",
	           con->uri);
	SendError(con, status);
        ShutdownClient(con);
	return (1);
      }

      if (con->http.expect)
      {
        /**** TODO: send expected header ****/
      }

      switch (con->http.state)
      {
	case HTTP_GET_SEND :
            if (strncmp(con->uri, "/printers/", 10) == 0 &&
		strcmp(con->uri + strlen(con->uri) - 4, ".ppd") == 0)
	    {
	     /*
	      * Send PPD file - get the real printer name since printer
	      * names are not case sensitive but filenames can be...
	      */

              con->uri[strlen(con->uri) - 4] = '\0';	/* Drop ".ppd" */

              if ((p = FindPrinter(con->uri + 10)) != NULL)
		snprintf(con->uri, sizeof(con->uri), "/ppd/%s.ppd", p->name);
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

	    if ((strncmp(con->uri, "/admin", 6) == 0 &&
	         strncmp(con->uri, "/admin/conf/", 12) != 0) ||
		strncmp(con->uri, "/printers", 9) == 0 ||
		strncmp(con->uri, "/classes", 8) == 0 ||
		strncmp(con->uri, "/jobs", 5) == 0)
	    {
	     /*
	      * Send CGI output...
	      */

              if (strncmp(con->uri, "/admin", 6) == 0)
	      {
		SetStringf(&con->command, "%s/cgi-bin/admin.cgi", ServerBin);
		SetString(&con->options, con->uri + 6);
	      }
              else if (strncmp(con->uri, "/printers", 9) == 0)
	      {
		SetStringf(&con->command, "%s/cgi-bin/printers.cgi", ServerBin);
		SetString(&con->options, con->uri + 9);
	      }
	      else if (strncmp(con->uri, "/classes", 8) == 0)
	      {
		SetStringf(&con->command, "%s/cgi-bin/classes.cgi", ServerBin);
		SetString(&con->options, con->uri + 8);
	      }
	      else
	      {
		SetStringf(&con->command, "%s/cgi-bin/jobs.cgi", ServerBin);
		SetString(&con->options, con->uri + 5);
	      }

	      if (con->options[0] == '/')
		strcpy(con->options, con->options + 1);

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
            else if (strncmp(con->uri, "/admin/conf/", 12) == 0 &&
	             (strchr(con->uri + 12, '/') != NULL ||
		      strlen(con->uri) == 12))
	    {
	     /*
	      * GET can only be done to configuration files under
	      * /admin/conf...
	      */

	      if (!SendError(con, HTTP_FORBIDDEN))
	      {
	        CloseClient(con);
		return (0);
	      }

	      break;
	    }
	    else
	    {
	     /*
	      * Serve a file...
	      */

              if ((filename = get_file(con, &filestats, buf,
	                               sizeof(buf))) == NULL)
	      {
		if (!SendError(con, HTTP_NOT_FOUND))
		{
	          CloseClient(con);
		  return (0);
		}

		break;
	      }

	      type = mimeFileType(MimeDatabase, filename, NULL);

              if (IsCGI(con, filename, &filestats, type))
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

		if (con->http.version <= HTTP_1_0)
		  con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	        break;
	      }

	      if (!check_if_modified(con, &filestats))
              {
        	if (!SendError(con, HTTP_NOT_MODIFIED))
		{
		  CloseClient(con);
		  return (0);
		}
	      }
	      else
              {
		if (type == NULL)
	          strcpy(line, "text/plain");
		else
	          snprintf(line, sizeof(line), "%s/%s", type->super, type->type);

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

            LogMessage(L_DEBUG2, "POST %s", con->uri);
	    LogMessage(L_DEBUG2, "CONTENT_TYPE = %s", con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

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
	    else if (atoi(con->http.fields[HTTP_FIELD_CONTENT_LENGTH]) < 0)
	    {
	     /*
	      * Negative content lengths are invalid!
	      */

              if (!SendError(con, HTTP_BAD_REQUEST))
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
	    else if ((strncmp(con->uri, "/admin", 6) == 0 &&
	              strncmp(con->uri, "/admin/conf/", 12) != 0) ||
	             strncmp(con->uri, "/printers", 9) == 0 ||
	             strncmp(con->uri, "/classes", 8) == 0 ||
	             strncmp(con->uri, "/jobs", 5) == 0)
	    {
	     /*
	      * CGI request...
	      */

              if (strncmp(con->uri, "/admin", 6) == 0)
	      {
		SetStringf(&con->command, "%s/cgi-bin/admin.cgi", ServerBin);
		SetString(&con->options, con->uri + 6);
	      }
              else if (strncmp(con->uri, "/printers", 9) == 0)
	      {
		SetStringf(&con->command, "%s/cgi-bin/printers.cgi", ServerBin);
		SetString(&con->options, con->uri + 9);
	      }
	      else if (strncmp(con->uri, "/classes", 8) == 0)
	      {
		SetStringf(&con->command, "%s/cgi-bin/classes.cgi", ServerBin);
		SetString(&con->options, con->uri + 8);
	      }
	      else
	      {
		SetStringf(&con->command, "%s/cgi-bin/jobs.cgi", ServerBin);
		SetString(&con->options, con->uri + 5);
	      }

	      if (con->options[0] == '/')
		strcpy(con->options, con->options + 1);

              LogMessage(L_DEBUG2, "ReadClient() %d command=\"%s\", options = \"%s\"",
	        	 con->http.fd, con->command, con->options);

	      if (con->http.version <= HTTP_1_0)
		con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	    }
	    else
	    {
	     /*
	      * POST to a file...
	      */

              if ((filename = get_file(con, &filestats, buf,
	                               sizeof(buf))) == NULL)
	      {
		if (!SendError(con, HTTP_NOT_FOUND))
		{
	          CloseClient(con);
		  return (0);
		}

		break;
	      }

	      type = mimeFileType(MimeDatabase, filename, NULL);

              if (!IsCGI(con, filename, &filestats, type))
	      {
	       /*
	        * Only POST to CGI's...
		*/

		if (!SendError(con, HTTP_UNAUTHORIZED))
		{
		  CloseClient(con);
		  return (0);
		}
	      }
	    }
	    break;

	case HTTP_PUT_RECV :
	   /*
	    * Validate the resource name...
	    */

            if (strncmp(con->uri, "/admin/conf/", 12) != 0 ||
	        strchr(con->uri + 12, '/') != NULL ||
		strlen(con->uri) == 12)
	    {
	     /*
	      * PUT can only be done to configuration files under
	      * /admin/conf...
	      */

	      if (!SendError(con, HTTP_FORBIDDEN))
	      {
	        CloseClient(con);
		return (0);
	      }

	      break;
	    }

           /*
	    * See if the PUT request includes a Content-Length field, and if
	    * so check the length against any limits that are set...
	    */

            LogMessage(L_DEBUG2, "PUT %s", con->uri);
	    LogMessage(L_DEBUG2, "CONTENT_TYPE = %s", con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

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
	    * Open a temporary file to hold the request...
	    */

            SetStringf(&con->filename, "%s/%08x", RequestRoot, request_id ++);
	    con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	    fchmod(con->file, 0640);
	    fchown(con->file, User, Group);

            LogMessage(L_DEBUG2, "ReadClient() %d REQUEST %s=%d", con->http.fd,
	               con->filename, con->file);

	    if (con->file < 0)
	    {
	      if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	      {
		CloseClient(con);
		return (0);
	      }
	    }
	    break;

	case HTTP_DELETE :
	case HTTP_TRACE :
            SendError(con, HTTP_NOT_IMPLEMENTED);
            ShutdownClient(con);
	    return (1);

	case HTTP_HEAD :
            if (strncmp(con->uri, "/printers/", 10) == 0 &&
		strcmp(con->uri + strlen(con->uri) - 4, ".ppd") == 0)
	    {
	     /*
	      * Send PPD file - get the real printer name since printer
	      * names are not case sensitive but filenames can be...
	      */

              con->uri[strlen(con->uri) - 4] = '\0';	/* Drop ".ppd" */

              if ((p = FindPrinter(con->uri + 10)) != NULL)
		snprintf(con->uri, sizeof(con->uri), "/ppd/%s.ppd", p->name);
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

	    if ((strncmp(con->uri, "/admin/", 7) == 0 &&
	         strncmp(con->uri, "/admin/conf/", 12) != 0) ||
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
            else if (strncmp(con->uri, "/admin/conf/", 12) == 0 &&
	             (strchr(con->uri + 12, '/') != NULL ||
		      strlen(con->uri) == 12))
	    {
	     /*
	      * HEAD can only be done to configuration files under
	      * /admin/conf...
	      */

	      if (!SendError(con, HTTP_FORBIDDEN))
	      {
	        CloseClient(con);
		return (0);
	      }

	      break;
	    }
	    else if ((filename = get_file(con, &filestats, buf,
	                                  sizeof(buf))) == NULL)
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

	      type = mimeFileType(MimeDatabase, filename, NULL);
	      if (type == NULL)
		strcpy(line, "text/plain");
	      else
		snprintf(line, sizeof(line), "%s/%s", type->super, type->type);

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

	      if (httpPrintf(HTTP(con), "Content-Length: %lu\r\n",
	                     (unsigned long)filestats.st_size) < 0)
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
  }

 /*
  * Handle any incoming data...
  */

  switch (con->http.state)
  {
    case HTTP_PUT_RECV :
        LogMessage(L_DEBUG2, "ReadClient() %d con->data_encoding = %s, con->data_remaining = %d, con->file = %d",
		   con->http.fd,
		   con->http.data_encoding == HTTP_ENCODE_CHUNKED ? "chunked" : "length",
		   con->http.data_remaining, con->file);

        if ((bytes = httpRead(HTTP(con), line, sizeof(line))) < 0)
	{
	  CloseClient(con);
	  return (0);
	}
	else if (bytes > 0)
	{
	  con->bytes += bytes;

          LogMessage(L_DEBUG2, "ReadClient() %d writing %d bytes to %d",
	             con->http.fd, bytes, con->file);

          if (write(con->file, line, bytes) < bytes)
	  {
            LogMessage(L_ERROR, "ReadClient: Unable to write %d bytes to %s: %s",
	               bytes, con->filename, strerror(errno));

	    LogMessage(L_DEBUG2, "ReadClient: Closing data file %d...",
        	       con->file);

	    close(con->file);
	    con->file = 0;
	    unlink(con->filename);
	    ClearString(&con->filename);

            if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	    {
	      CloseClient(con);
	      return (0);
	    }
	  }
	}

        if (con->http.state == HTTP_WAITING)
	{
	 /*
	  * End of file, see how big it is...
	  */

	  fstat(con->file, &filestats);

          LogMessage(L_DEBUG2, "ReadClient() %d Closing data file %d, size = %d.",
                     con->http.fd, con->file, (int)filestats.st_size);

	  close(con->file);
	  con->file = 0;

          if (filestats.st_size > MaxRequestSize &&
	      MaxRequestSize > 0)
	  {
	   /*
	    * Request is too big; remove it and send an error...
	    */

            LogMessage(L_DEBUG2, "ReadClient() %d Removing temp file %s",
	               con->http.fd, con->filename);
	    unlink(con->filename);
	    ClearString(&con->filename);

            if (!SendError(con, HTTP_REQUEST_TOO_LARGE))
	    {
	      CloseClient(con);
	      return (0);
	    }
	  }

         /*
	  * Install the configuration file...
	  */

          status = install_conf_file(con);

         /*
	  * Return the status to the client...
	  */

          if (!SendError(con, status))
	  {
	    CloseClient(con);
	    return (0);
	  }
	}
        break;

    case HTTP_POST_RECV :
        LogMessage(L_DEBUG2, "ReadClient() %d con->data_encoding = %s, con->data_remaining = %d, con->file = %d",
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

	    if (!SendError(con, HTTP_BAD_REQUEST))
	    {
	      CloseClient(con);
	      return (0);
	    }

	    ShutdownClient(con);
	    return (1);
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

          SetStringf(&con->filename, "%s/%08x", RequestRoot, request_id ++);
	  con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	  fchmod(con->file, 0640);
	  fchown(con->file, User, Group);

          LogMessage(L_DEBUG2, "ReadClient() %d REQUEST %s=%d", con->http.fd,
	             con->filename, con->file);

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

            LogMessage(L_DEBUG2, "ReadClient() %d writing %d bytes to %d",
	               con->http.fd, bytes, con->file);

            if (write(con->file, line, bytes) < bytes)
	    {
              LogMessage(L_ERROR, "ReadClient: Unable to write %d bytes to %s: %s",
	        	 bytes, con->filename, strerror(errno));

	      LogMessage(L_DEBUG2, "ReadClient: Closing file %d...",
        		 con->file);

	      close(con->file);
	      con->file = 0;
	      unlink(con->filename);
	      ClearString(&con->filename);

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

            LogMessage(L_DEBUG2, "ReadClient() %d Closing data file %d, size = %d.",
                       con->http.fd, con->file, (int)filestats.st_size);

	    close(con->file);
	    con->file = 0;

            if (filestats.st_size > MaxRequestSize &&
	        MaxRequestSize > 0)
	    {
	     /*
	      * Request is too big; remove it and send an error...
	      */

              LogMessage(L_DEBUG2, "ReadClient() %d Removing temp file %s",
	                 con->http.fd, con->filename);
	      unlink(con->filename);
	      ClearString(&con->filename);

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

	    if (con->command)
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
	    return (ProcessIPPRequest(con));
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


  if (con->filename)
    fd = open(con->filename, O_RDONLY);
  else
    fd = open("/dev/null", O_RDONLY);

  con->pipe_pid = pipe_command(con, fd, &(con->file), command, options);

  close(fd);

  LogMessage(L_INFO, "Started \"%s\" (pid=%d)", command, con->pipe_pid);

  LogMessage(L_DEBUG, "SendCommand() %d file=%d", con->http.fd, con->file);

  if (con->pipe_pid == 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  LogMessage(L_DEBUG2, "SendCommand: Adding fd %d to InputSet...", con->file);
  LogMessage(L_DEBUG2, "SendCommand: Adding fd %d to OutputSet...",
             con->http.fd);

  FD_SET(con->file, InputSet);
  FD_SET(con->http.fd, OutputSet);

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

  LogMessage(L_DEBUG, "SendError() %d code=%d", con->http.fd, code);

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

#ifdef HAVE_SSL
  if (code == HTTP_UPGRADE_REQUIRED)
    if (httpPrintf(HTTP(con), "Connection: Upgrade\r\n") < 0)
      return (0);

  if (httpPrintf(HTTP(con), "Upgrade: TLS/1.0,HTTP/1.1\r\n") < 0)
    return (0);
#endif /* HAVE_SSL */

  if ((con->http.version >= HTTP_1_1 && !con->http.keep_alive) ||
      (code >= HTTP_BAD_REQUEST && code != HTTP_UPGRADE_REQUIRED))
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
    if (httpPrintf(HTTP(con), "Content-Length: %d\r\n",
                   (int)strlen(message)) < 0)
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
  if (httpPrintf(HTTP(con), "Content-Length: %lu\r\n",
                 (unsigned long)filestats->st_size) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "\r\n") < 0)
    return (0);

  LogMessage(L_DEBUG2, "SendFile: Adding fd %d to OutputSet...", con->http.fd);

  FD_SET(con->http.fd, OutputSet);

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
   /*
    * This already succeeded in IsAuthorized...
    */

    loc = FindBest(con->uri, con->http.state);

    if (loc->type != AUTH_DIGEST)
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
 * 'ShutdownClient()' - Shutdown the receiving end of a connection.
 */

void
ShutdownClient(client_t *con)		/* I - Client connection */
{
 /*
  * Shutdown the receiving end of the socket, since the client
  * still needs to read the error message...
  */

  shutdown(con->http.fd, 0);
  con->http.used = 0;

 /*
  * Update the activity time so that we timeout after 30 seconds rather
  * then the current Timeout setting (300 by default).  This prevents
  * some DoS situations...
  */

  con->http.activity = time(NULL) - Timeout + 30;

  LogMessage(L_DEBUG2, "ShutdownClient: Removing fd %d from InputSet...",
             con->http.fd);

  FD_CLR(con->http.fd, InputSet);
}


/*
 * 'UpdateCGI()' - Read status messages from CGI scripts and programs.
 */

void
UpdateCGI(void)
{
  int		bytes;		/* Number of bytes read */
  char		*lineptr,	/* Pointer to end of line in buffer */
		*message;	/* Pointer to message text */
  int		loglevel;	/* Log level for message */
  static int	bufused = 0;	/* Number of bytes used in buffer */
  static char	buffer[1024];	/* Status buffer */


  if ((bytes = read(CGIPipes[0], buffer + bufused,
                    sizeof(buffer) - bufused - 1)) > 0)
  {
    bufused += bytes;
    buffer[bufused] = '\0';
    lineptr = strchr(buffer, '\n');
  }
  else if (bytes < 0 && errno == EINTR)
    return;
  else
  {
    lineptr    = buffer + bufused;
    lineptr[1] = 0;
  }

  if (bytes == 0 && bufused == 0)
    lineptr = NULL;

  while (lineptr != NULL)
  {
   /*
    * Terminate each line and process it...
    */

    *lineptr++ = '\0';

   /*
    * Figure out the logging level...
    */

    if (strncmp(buffer, "EMERG:", 6) == 0)
    {
      loglevel = L_EMERG;
      message  = buffer + 6;
    }
    else if (strncmp(buffer, "ALERT:", 6) == 0)
    {
      loglevel = L_ALERT;
      message  = buffer + 6;
    }
    else if (strncmp(buffer, "CRIT:", 5) == 0)
    {
      loglevel = L_CRIT;
      message  = buffer + 5;
    }
    else if (strncmp(buffer, "ERROR:", 6) == 0)
    {
      loglevel = L_ERROR;
      message  = buffer + 6;
    }
    else if (strncmp(buffer, "WARNING:", 8) == 0)
    {
      loglevel = L_WARN;
      message  = buffer + 8;
    }
    else if (strncmp(buffer, "NOTICE:", 6) == 0)
    {
      loglevel = L_NOTICE;
      message  = buffer + 6;
    }
    else if (strncmp(buffer, "INFO:", 5) == 0)
    {
      loglevel = L_INFO;
      message  = buffer + 5;
    }
    else if (strncmp(buffer, "DEBUG:", 6) == 0)
    {
      loglevel = L_DEBUG;
      message  = buffer + 6;
    }
    else if (strncmp(buffer, "DEBUG2:", 7) == 0)
    {
      loglevel = L_DEBUG2;
      message  = buffer + 7;
    }
    else if (strncmp(buffer, "PAGE:", 5) == 0)
    {
      loglevel = L_PAGE;
      message  = buffer + 5;
    }
    else
    {
      loglevel = L_DEBUG;
      message  = buffer;
    }

   /*
    * Skip leading whitespace in the message...
    */

    while (isspace(*message))
      message ++;

    LogMessage(loglevel, "[CGI] %s", message);

   /*
    * Copy over the buffer data we've used up...
    */

    strcpy(buffer, lineptr);
    bufused -= lineptr - buffer;

    if (bufused < 0)
      bufused = 0;

    lineptr = strchr(buffer, '\n');
  }

  if (bytes <= 0)
  {
   /*
    * Fatal error on pipe - should never happen!
    */

    LogMessage(L_ERROR, "UpdateCGI: error reading from CGI error pipe - %s",
               strerror(errno));
  }
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
	  LogMessage(L_DEBUG2, "WriteClient() %d %s", con->http.fd, buf);

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

    LogMessage(L_DEBUG2, "WriteClient() Removing fd %d from OutputSet...",
               con->http.fd);

    FD_CLR(con->http.fd, OutputSet);

    if (con->file)
    {
      LogMessage(L_DEBUG2, "WriteClient() Removing fd %d from InputSet...",
                 con->file);
      FD_CLR(con->file, InputSet);

      if (con->pipe_pid)
	kill(con->pipe_pid, SIGTERM);

      LogMessage(L_DEBUG2, "WriteClient() %d Closing data file %d.",
                 con->http.fd, con->file);

      close(con->file);
      con->file     = 0;
      con->pipe_pid = 0;
    }

    if (con->filename)
    {
      LogMessage(L_DEBUG2, "WriteClient() %d Removing temp file %s",
                 con->http.fd, con->filename);
      unlink(con->filename);
      ClearString(&con->filename);
    }

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

    ClearString(&con->command);
    ClearString(&con->options);

    if (!con->http.keep_alive)
    {
      CloseClient(con);
      return (0);
    }
  }

  if (bytes >= 1024)
    LogMessage(L_DEBUG2, "WriteClient() %d %d bytes", con->http.fd, bytes);

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

  LogMessage(L_DEBUG2, "check_if_modified() %d If-Modified-Since=\"%s\"",
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

  LogMessage(L_DEBUG2, "check_if_modified() %d sizes=%d,%d dates=%d,%d",
             con->http.fd, size, (int)filestats->st_size, (int)date,
	     (int)filestats->st_mtime);

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

  LogMessage(L_DEBUG2, "decode_auth(%p): Authorization string = \"%s\"",
             con, s);

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

    strlcpy(con->username, value, sizeof(con->username));
    strlcpy(con->password, s, sizeof(con->password));
  }
  else if (strncmp(s, "Local", 5) == 0)
  {
    s += 5;
    while (isspace(*s))
      s ++;

    if ((username = FindCert(s)) != NULL)
      strlcpy(con->username, username, sizeof(con->username));
  }
  else if (strncmp(s, "Digest", 5) == 0)
  {
   /*
    * Get the username and password from the Digest attributes...
    */

    if (httpGetSubField(&(con->http), HTTP_FIELD_WWW_AUTHENTICATE, "username",
                        value))
      strlcpy(con->username, value, sizeof(con->username));

    if (httpGetSubField(&(con->http), HTTP_FIELD_WWW_AUTHENTICATE, "response",
                        value))
      strlcpy(con->password, value, sizeof(con->password) - 1);
  }

  LogMessage(L_DEBUG2, "decode_auth() %d username=\"%s\"",
             con->http.fd, con->username);
}


/*
 * 'get_file()' - Get a filename and state info.
 */

static char *			/* O  - Real filename */
get_file(client_t    *con,	/* I  - Client connection */
         struct stat *filestats,/* O  - File information */
         char        *filename,	/* IO - Filename buffer */
         int         len)	/* I  - Buffer length */
{
  int		status;		/* Status of filesystem calls */
  char		*params;	/* Pointer to parameters in URI */


 /*
  * Need to add DocumentRoot global...
  */

  if (strncmp(con->uri, "/ppd/", 5) == 0)
    snprintf(filename, len, "%s%s", ServerRoot, con->uri);
  else if (strncmp(con->uri, "/admin/conf/", 12) == 0)
    snprintf(filename, len, "%s%s", ServerRoot, con->uri + 11);
  else if (con->language != NULL)
    snprintf(filename, len, "%s/%s%s", DocumentRoot, con->language->language,
            con->uri);
  else
    snprintf(filename, len, "%s%s", DocumentRoot, con->uri);

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

    if (strncmp(con->uri, "/ppd/", 5) != 0 &&
        strncmp(con->uri, "/admin/conf/", 12) != 0)
    {
      snprintf(filename, len, "%s%s", DocumentRoot, con->uri);

      status = stat(filename, filestats);
    }
  }

 /*
  * If we're found a directory, get the index.html file instead...
  */

  if (!status && S_ISDIR(filestats->st_mode))
  {
    if (filename[strlen(filename) - 1] == '/')
      strlcat(filename, "index.html", len);
    else
      strlcat(filename, "/index.html", len);

    status = stat(filename, filestats);
  }

  LogMessage(L_DEBUG2, "get_file() %d filename=%s size=%d",
             con->http.fd, filename, status ? -1 : (int)filestats->st_size);

  if (status)
    return (NULL);
  else
    return (filename);
}


/*
 * 'install_conf_file()' - Install a configuration file.
 */

static http_status_t			/* O - Status */
install_conf_file(client_t *con)	/* I - Connection */
{
  cups_file_t	*in,			/* Input file */
		*out;			/* Output file */
  char		buffer[1024];		/* Copy buffer */
  int		bytes;			/* Number of bytes */
  char		conffile[1024],		/* Configuration filename */
		newfile[1024],		/* New config filename */
		oldfile[1024];		/* Old config filename */
  struct stat	confinfo;		/* Config file info */


 /*
  * First construct the filenames...
  */

  snprintf(conffile, sizeof(conffile), "%s%s", ServerRoot, con->uri + 11);
  snprintf(newfile, sizeof(newfile), "%s%s.N", ServerRoot, con->uri + 11);
  snprintf(oldfile, sizeof(oldfile), "%s%s.O", ServerRoot, con->uri + 11);

  LogMessage(L_INFO, "Installing config file \"%s\"...", conffile);

 /*
  * Get the owner, group, and permissions of the configuration file.
  * If it doesn't exist, assign it to the User and Group in the
  * cupsd.conf file with mode 0640 permissions.
  */

  if (stat(conffile, &confinfo))
  {
    confinfo.st_uid  = User;
    confinfo.st_gid  = Group;
    confinfo.st_mode = ConfigFilePerm;
  }

 /*
  * Open the request file and new config file...
  */

  if ((in = cupsFileOpen(con->filename, "rb")) == NULL)
  {
    LogMessage(L_ERROR, "Unable to open request file \"%s\" - %s",
               con->filename, strerror(errno));
    return (HTTP_SERVER_ERROR);
  }

  if ((out = cupsFileOpen(newfile, "wb")) == NULL)
  {
    cupsFileClose(in);
    LogMessage(L_ERROR, "Unable to open config file \"%s\" - %s",
               newfile, strerror(errno));
    return (HTTP_SERVER_ERROR);
  }

  fchmod(cupsFileNumber(out), confinfo.st_mode);
  fchown(cupsFileNumber(out), confinfo.st_uid, confinfo.st_gid);

 /*
  * Copy from the request to the new config file...
  */

  while ((bytes = cupsFileRead(in, buffer, sizeof(buffer))) > 0)
    if (cupsFileWrite(out, buffer, bytes) < bytes)
    {
      LogMessage(L_ERROR, "Unable to copy to config file \"%s\" - %s",
        	 newfile, strerror(errno));

      cupsFileClose(in);
      cupsFileClose(out);
      unlink(newfile);

      return (HTTP_SERVER_ERROR);
    }

 /*
  * Close the files...
  */

  cupsFileClose(in);
  if (cupsFileClose(out))
  {
    LogMessage(L_ERROR, "Error file closing config file \"%s\" - %s",
               newfile, strerror(errno));

    unlink(newfile);

    return (HTTP_SERVER_ERROR);
  }

 /*
  * Remove the request file...
  */

  unlink(con->filename);
  ClearString(&con->filename);

 /*
  * Unlink the old backup, rename the current config file to the backup
  * filename, and rename the new config file to the config file name...
  */

  if (unlink(oldfile))
    if (errno != ENOENT)
    {
      LogMessage(L_ERROR, "Unable to remove backup config file \"%s\" - %s",
        	 oldfile, strerror(errno));

      unlink(newfile);

      return (HTTP_SERVER_ERROR);
    }

  if (rename(conffile, oldfile))
    if (errno != ENOENT)
    {
      LogMessage(L_ERROR, "Unable to rename old config file \"%s\" - %s",
        	 conffile, strerror(errno));

      unlink(newfile);

      return (HTTP_SERVER_ERROR);
    }

  if (rename(newfile, conffile))
  {
    LogMessage(L_ERROR, "Unable to rename new config file \"%s\" - %s",
               newfile, strerror(errno));

    rename(oldfile, conffile);
    unlink(newfile);

    return (HTTP_SERVER_ERROR);
  }

 /*
  * If the cupsd.conf file was updated, set the NeedReload flag...
  */

  if (strcmp(con->uri, "/admin/conf/cupsd.conf") == 0)
    NeedReload = RELOAD_CUPSD;
  else
    NeedReload = RELOAD_ALL;

 /*
  * Return that the file was created successfully...
  */

  return (HTTP_CREATED);
}


/*
 * 'pipe_command()' - Pipe the output of a command to the remote client.
 */

static int				/* O - Process ID */
pipe_command(client_t *con,		/* I - Client connection */
             int      infile,		/* I - Standard input for command */
             int      *outfile,		/* O - Standard output for command */
	     char     *command,		/* I - Command to run */
	     char     *options)		/* I - Options for command */
{
  int		i;			/* Looping var */
  int		pid;			/* Process ID */
  char		*commptr;		/* Command string pointer */
  char		*uriptr;		/* URI string pointer */
  int		fd;			/* Looping var */
  int		fds[2];			/* Pipe FDs */
  int		argc;			/* Number of arguments */
  int		envc;			/* Number of environment variables */
  char		argbuf[10240],		/* Argument buffer */
		*argv[100],		/* Argument strings */
		*envp[100];		/* Environment variables */
  char		content_length[1024],	/* CONTENT_LENGTH environment variable */
		content_type[1024],	/* CONTENT_TYPE environment variable */
		cups_datadir[1024],	/* CUPS_DATADIR environment variable */
		cups_serverroot[1024],	/* CUPS_SERVERROOT environment variable */
		http_cookie[1024],	/* HTTP_COOKIE environment variable */
		http_user_agent[1024],	/* HTTP_USER_AGENT environment variable */
		ipp_port[1024],		/* IPP_PORT environment variable */
		lang[1024],		/* LANG environment variable */
		ld_library_path[1024],	/* LD_LIBRARY_PATH environment variable */
		ld_preload[1024],	/* LD_PRELOAD environment variable */
		dyld_library_path[1024],/* DYLD_LIBRARY_PATH environment variable */
		shlib_path[1024],	/* SHLIB_PATH environment variable */
		nlspath[1024],		/* NLSPATH environment variable */
		query_string[10240],	/* QUERY_STRING env variable */
		remote_addr[1024],	/* REMOTE_ADDR environment variable */
		remote_host[1024],	/* REMOTE_HOST environment variable */
		remote_user[1024],	/* REMOTE_USER environment variable */
		script_name[1024],	/* SCRIPT_NAME environment variable */
		server_name[1024],	/* SERVER_NAME environment variable */
		server_port[1024],	/* SERVER_PORT environment variable */
		tmpdir[1024];		/* TMPDIR environment variable */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;		/* POSIX signal handler */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


 /*
  * Copy the command string...
  */

  strlcpy(argbuf, options, sizeof(argbuf));

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

  snprintf(lang, sizeof(lang), "LANG=%s",
           con->language ? con->language->language : "C");
#ifdef AF_INET6
  if (con->http.hostaddr.addr.sa_family == AF_INET6)
  {
    sprintf(ipp_port, "IPP_PORT=%d", ntohs(con->http.hostaddr.ipv6.sin6_port));
    sprintf(server_port, "SERVER_PORT=%d",
            ntohs(con->http.hostaddr.ipv6.sin6_port));
  }
  else
#endif /* AF_INET6 */
  {
    sprintf(ipp_port, "IPP_PORT=%d", ntohs(con->http.hostaddr.ipv4.sin_port));
    sprintf(server_port, "SERVER_PORT=%d",
            ntohs(con->http.hostaddr.ipv4.sin_port));
  }

  if (strcmp(con->http.hostname, "localhost") == 0)
    strlcpy(server_name, "SERVER_NAME=localhost", sizeof(server_name));
  else
    snprintf(server_name, sizeof(server_name), "SERVER_NAME=%s", ServerName);
  snprintf(remote_host, sizeof(remote_host), "REMOTE_HOST=%s", con->http.hostname);
  strcpy(remote_addr, "REMOTE_ADDR=");
  httpAddrString(&(con->http.hostaddr), remote_addr + 12,
                 sizeof(remote_addr) - 12);
  snprintf(remote_user, sizeof(remote_user), "REMOTE_USER=%s", con->username);
  snprintf(tmpdir, sizeof(tmpdir), "TMPDIR=%s", TempDir);
  snprintf(cups_datadir, sizeof(cups_datadir), "CUPS_DATADIR=%s", DataDir);
  snprintf(cups_serverroot, sizeof(cups_serverroot), "CUPS_SERVERROOT=%s", ServerRoot);

  envc = 0;

  envp[envc ++] = "PATH=/bin:/usr/bin";
  envp[envc ++] = "SERVER_SOFTWARE=CUPS/1.1";
  envp[envc ++] = "GATEWAY_INTERFACE=CGI/1.1";
  if (con->http.version == HTTP_1_1)
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/1.1";
  else if (con->http.version == HTTP_1_0)
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/1.0";
  else
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/0.9";
  envp[envc ++] = "REDIRECT_STATUS=1";
  envp[envc ++] = ipp_port;
  envp[envc ++] = server_name;
  envp[envc ++] = server_port;
  envp[envc ++] = remote_addr;
  envp[envc ++] = remote_host;
  envp[envc ++] = remote_user;
  envp[envc ++] = lang;
  envp[envc ++] = TZ;
  envp[envc ++] = tmpdir;
  envp[envc ++] = cups_datadir;
  envp[envc ++] = cups_serverroot;

  if (getenv("LD_LIBRARY_PATH") != NULL)
  {
    snprintf(ld_library_path, sizeof(ld_library_path), "LD_LIBRARY_PATH=%s",
             getenv("LD_LIBRARY_PATH"));
    envp[envc ++] = ld_library_path;
  }

  if (getenv("LD_PRELOAD") != NULL)
  {
    snprintf(ld_preload, sizeof(ld_preload), "LD_PRELOAD=%s",
             getenv("LD_PRELOAD"));
    envp[envc ++] = ld_preload;
  }

  if (getenv("DYLD_LIBRARY_PATH") != NULL)
  {
    snprintf(dyld_library_path, sizeof(dyld_library_path), "DYLD_LIBRARY_PATH=%s",
             getenv("DYLD_LIBRARY_PATH"));
    envp[envc ++] = dyld_library_path;
  }

  if (getenv("SHLIB_PATH") != NULL)
  {
    snprintf(shlib_path, sizeof(shlib_path), "SHLIB_PATH=%s",
             getenv("SHLIB_PATH"));
    envp[envc ++] = shlib_path;
  }

  if (getenv("NLSPATH") != NULL)
  {
    snprintf(nlspath, sizeof(nlspath), "NLSPATH=%s", getenv("NLSPATH"));
    envp[envc ++] = nlspath;
  }

  if (con->http.cookie)
  {
    snprintf(http_cookie, sizeof(http_cookie), "HTTP_COOKIE=%s",
             con->http.cookie);
    envp[envc ++] = http_cookie;
  }

  if (con->http.fields[HTTP_FIELD_USER_AGENT][0])
  {
    snprintf(http_user_agent, sizeof(http_user_agent), "HTTP_USER_AGENT=%s",
             con->http.fields[HTTP_FIELD_USER_AGENT]);
    envp[envc ++] = http_user_agent;
  }

  snprintf(script_name, sizeof(script_name), "SCRIPT_NAME=%s", con->uri);
  if ((uriptr = strchr(script_name, '?')) != NULL)
    *uriptr = '\0';
  envp[envc ++] = script_name;

  if (con->operation == HTTP_GET)
  {
    for (i = 0; i < argc; i ++)
      LogMessage(L_DEBUG2, "argv[%d] = \"%s\"", i, argv[i]);
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

 /*
  * Tell the CGI if we are using encryption...
  */

  if (con->http.encryption == HTTP_ENCRYPT_ALWAYS)
  {
    envp[envc ++] = "HTTPS=ON";
    envp[envc ++] = "CUPS_ENCRYPTION=Always";
  }

  envp[envc] = NULL;

  if (LogLevel == L_DEBUG2)
  {
    for (i = 0; i < argc; i ++)
      LogMessage(L_DEBUG2, "argv[%d] = \"%s\"", i, argv[i]);
    for (i = 0; i < envc; i ++)
      LogMessage(L_DEBUG2, "envp[%d] = \"%s\"", i, envp[i]);
  }

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
  * Block signals before forking...
  */

  HoldSignals();

 /*
  * Then execute the command...
  */

  if ((pid = fork()) == 0)
  {
   /*
    * Child comes here...  Close stdin if necessary and dup the pipe to stdout.
    */

    if (getuid() == 0)
    {
     /*
      * Running as root, so change to a non-priviledged user...
      */

      if (setgid(Group))
        exit(errno);

      if (setuid(User))
        exit(errno);
    }

   /*
    * Reset group membership to just the main one we belong to.
    */

    setgroups(0, NULL);

   /*
    * Update stdin/stdout/stderr...
    */

    if (infile)
    {
      close(0);
      if (dup(infile) < 0)
	exit(errno);
    }

    close(1);
    if (dup(fds[1]) < 0)
      exit(errno);

    close(2);
    dup(CGIPipes[1]);

   /*
    * Close extra file descriptors...
    */

    for (fd = 3; fd < MaxFDs; fd ++)
      close(fd);

   /*
    * Change umask to restrict permissions on created files...
    */

    umask(077);

   /*
    * Unblock signals before doing the exec...
    */

#ifdef HAVE_SIGSET
    sigset(SIGTERM, SIG_DFL);
    sigset(SIGCHLD, SIG_DFL);
#elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));

    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;

    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGCHLD, &action, NULL);
#else
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_SIGSET */

    ReleaseSignals();

   /*
    * Execute the pipe program; if an error occurs, exit with status 1...
    */

    execve(command, argv, envp);
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
    pid = 0;
  }
  else
  {
   /*
    * Fork successful - return the PID...
    */

    AddCert(pid, con->username);

    LogMessage(L_DEBUG, "CGI %s started - PID = %d", command, pid);

    *outfile = fds[0];
    close(fds[1]);
  }

  ReleaseSignals();

  return (pid);
}


#if defined(HAVE_CDSASSL)
/*
 * 'CDSAReadFunc()' - Read function for CDSA decryption code.
 */

static OSStatus					/* O  - -1 on error, 0 on success */
CDSAReadFunc(SSLConnectionRef connection,	/* I  - SSL/TLS connection */
             void             *data,		/* I  - Data buffer */
	     size_t           *dataLength)	/* IO - Number of bytes */
{
  ssize_t	bytes;				/* Number of bytes read */


  bytes = recv((int)connection, data, *dataLength, 0);
  if (bytes >= 0)
  {
    *dataLength = bytes;
    return (0);
  }
  else
    return (-1);
}


/*
 * 'CDSAWriteFunc()' - Write function for CDSA encryption code.
 */

static OSStatus					/* O  - -1 on error, 0 on success */
CDSAWriteFunc(SSLConnectionRef connection,	/* I  - SSL/TLS connection */
              const void       *data,		/* I  - Data buffer */
	      size_t           *dataLength)	/* IO - Number of bytes */
{
  ssize_t bytes;


  bytes = write((int)connection, data, *dataLength);
  if (bytes >= 0)
  {
    *dataLength = bytes;
    return (0);
  }
  else
    return (-1);
}
#endif /* HAVE_CDSASSL */


/*
 * End of "$Id: client.c,v 1.91.2.58 2003/04/10 20:15:49 mike Exp $".
 */
