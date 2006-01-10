/*
 * "$Id$"
 *
 *   Client routines for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 * Contents:
 *
 *   cupsdAcceptClient()     - Accept a new client.
 *   cupsdCloseAllClients()  - Close all remote clients immediately.
 *   cupsdCloseClient()      - Close a remote client.
 *   cupsdEncryptClient()    - Enable encryption for the client...
 *   cupsdIsCGI()            - Is the resource a CGI script/program?
 *   cupsdReadClient()       - Read data from a client.
 *   cupsdSendCommand()      - Send output from a command via HTTP.
 *   cupsdSendError()        - Send an error message via HTTP.
 *   cupsdSendFile()         - Send a file via HTTP.
 *   cupsdSendHeader()       - Send an HTTP request.
 *   cupsdUpdateCGI()        - Read status messages from CGI scripts and programs.
 *   cupsdWriteClient()      - Write data to a client as needed.
 *   check_if_modified()     - Decode an "If-Modified-Since" line.
 *   get_cdsa_server_certs() - Convert a keychain name into the CFArrayRef
 *			       required by SSLSetCertificate.
 *   get_file()              - Get a filename and state info.
 *   install_conf_file()     - Install a configuration file.
 *   is_path_absolute()      - Is a path absolute and free of relative elements.
 *   pipe_command()          - Pipe the output of a command to the remote client.
 */

/*
 * Include necessary headers...
 */

#include <cups/http-private.h>
#include "cupsd.h"

#ifdef HAVE_CDSASSL
#  include <Security/Security.h>
#endif /* HAVE_CDSASSL */


/*
 * Local functions...
 */

static int		check_if_modified(cupsd_client_t *con,
			                  struct stat *filestats);
#ifdef HAVE_CDSASSL
static CFArrayRef	get_cdsa_server_certs(void);
#endif /* HAVE_CDSASSL */
static char		*get_file(cupsd_client_t *con, struct stat *filestats, 
			          char *filename, int len);
static http_status_t	install_conf_file(cupsd_client_t *con);
static int		is_path_absolute(const char *path);
static int		pipe_command(cupsd_client_t *con, int infile, int *outfile,
			             char *command, char *options, int root);


/*
 * 'cupsdAcceptClient()' - Accept a new client.
 */

void
cupsdAcceptClient(cupsd_listener_t *lis)/* I - Listener socket */
{
  int			i;		/* Looping var */
  int			count;		/* Count of connections on a host */
  int			val;		/* Parameter value */
  cupsd_client_t	*con;		/* New client pointer */
  http_addrlist_t	*addrlist,	/* List of adddresses for host */
			*addr;		/* Current address */
  socklen_t		addrlen;	/* Length of address */
  char			*hostname;	/* Hostname for address */
  http_addr_t		temp;		/* Temporary address variable */
  static time_t		last_dos = 0;	/* Time of last DoS attack */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdAcceptClient(lis=%p) %d NumClients = %d",
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

  memset(con, 0, sizeof(cupsd_client_t));
  con->http.activity = time(NULL);
  con->file          = -1;
  con->http.hostaddr = &(con->clientaddr);

 /*
  * Accept the client and get the remote address...
  */

  addrlen = sizeof(http_addr_t);

  if ((con->http.fd = accept(lis->fd, (struct sockaddr *)con->http.hostaddr,
                             &addrlen)) < 0)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to accept client connection - %s.",
                    strerror(errno));
    return;
  }

#ifdef AF_INET6
  if (lis->address.addr.sa_family == AF_INET6)
  {
   /*
    * Save the connected port number...
    */

    con->http.hostaddr->ipv6.sin6_port = lis->address.ipv6.sin6_port;

   /*
    * Convert IPv4 over IPv6 addresses (::ffff:n.n.n.n) to IPv4 forms we
    * can more easily use...
    */

    if (con->http.hostaddr->ipv6.sin6_addr.s6_addr32[0] == 0 &&
        con->http.hostaddr->ipv6.sin6_addr.s6_addr32[1] == 0 &&
        ntohl(con->http.hostaddr->ipv6.sin6_addr.s6_addr32[2]) == 0xffff)
      con->http.hostaddr->ipv6.sin6_addr.s6_addr32[2] = 0;
  }
  else
#endif /* AF_INET6 */
  if (lis->address.addr.sa_family == AF_INET)
    con->http.hostaddr->ipv4.sin_port = lis->address.ipv4.sin_port;

 /*
  * Check the number of clients on the same address...
  */

  for (i = 0, count = 0; i < NumClients; i ++)
    if (httpAddrEqual(Clients[i].http.hostaddr, con->http.hostaddr))
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
      cupsdLogMessage(CUPSD_LOG_WARN,
                      "Possible DoS attack - more than %d clients connecting from %s!",
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

  if (httpAddrLocalhost(con->http.hostaddr))
  {
   /*
    * Map accesses from the loopback interface to "localhost"...
    */

    strlcpy(con->http.hostname, "localhost", sizeof(con->http.hostname));
    hostname = con->http.hostname;
  }
  else
  {
   /*
    * Map accesses from the same host to the server name.
    */

    for (addr = ServerAddrs; addr; addr = addr->next)
      if (httpAddrEqual(con->http.hostaddr, &(addr->addr)))
        break;

    if (addr)
    {
      strlcpy(con->http.hostname, ServerName, sizeof(con->http.hostname));
      hostname = con->http.hostname;
    }
    else if (HostNameLookups)
      hostname = httpAddrLookup(con->http.hostaddr, con->http.hostname,
                                sizeof(con->http.hostname));
    else
    {
      hostname = NULL;
      httpAddrString(con->http.hostaddr, con->http.hostname,
                     sizeof(con->http.hostname));
    }
  }

  if (hostname == NULL && HostNameLookups == 2)
  {
   /*
    * Can't have an unresolved IP address with double-lookups enabled...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdAcceptClient: Closing connection %d...",
                    con->http.fd);

#ifdef WIN32
    closesocket(con->http.fd);
#else
    close(con->http.fd);
#endif /* WIN32 */

    cupsdLogMessage(CUPSD_LOG_WARN,
                    "Name lookup failed - connection from %s closed!",
                    con->http.hostname);
    return;
  }

  if (HostNameLookups == 2)
  {
   /*
    * Do double lookups as needed...
    */

    if ((addrlist = httpAddrGetList(con->http.hostname, AF_UNSPEC, NULL)) != NULL)
    {
     /*
      * See if the hostname maps to the same IP address...
      */

      for (addr = addrlist; addr; addr = addr->next)
        if (httpAddrEqual(con->http.hostaddr, &(addr->addr)))
          break;
    }
    else
      addr = NULL;

    httpAddrFreeList(addrlist);

    if (!addr)
    {
     /*
      * Can't have a hostname that doesn't resolve to the same IP address
      * with double-lookups enabled...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdAcceptClient: Closing connection %d...",
        	      con->http.fd);

#ifdef WIN32
      closesocket(con->http.fd);
#else
      close(con->http.fd);
#endif /* WIN32 */

      cupsdLogMessage(CUPSD_LOG_WARN,
                      "IP lookup failed - connection from %s closed!",
                      con->http.hostname);
      return;
    }
  }

#ifdef AF_INET6
  if (con->http.hostaddr->addr.sa_family == AF_INET6)
    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdAcceptClient: %d from %s:%d (IPv6)",
                    con->http.fd, con->http.hostname,
		    ntohs(con->http.hostaddr->ipv6.sin6_port));
  else
#endif /* AF_INET6 */
#ifdef AF_LOCAL
  if (con->http.hostaddr->addr.sa_family == AF_LOCAL)
    cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdAcceptClient: %d from %s (Domain)",
                    con->http.fd, con->http.hostname);
  else
#endif /* AF_LOCAL */
  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdAcceptClient: %d from %s:%d (IPv4)",
                  con->http.fd, con->http.hostname,
		  ntohs(con->http.hostaddr->ipv4.sin_port));

 /*
  * Get the local address the client connected to...
  */

  addrlen = sizeof(temp);
  if (getsockname(con->http.fd, (struct sockaddr *)&temp, &addrlen))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to get local address - %s",
                    strerror(errno));

    strcpy(con->servername, "localhost");
    con->serverport = LocalPort;
  }
  else
  {
#ifdef AF_INET6
    if (temp.addr.sa_family == AF_INET6)
    {
      httpAddrLookup(&temp, con->servername, sizeof(con->servername));
      con->serverport = ntohs(lis->address.ipv6.sin6_port);
    }
    else
#endif /* AF_INET6 */
    if (temp.addr.sa_family == AF_INET)
    {
      httpAddrLookup(&temp, con->servername, sizeof(con->servername));
      con->serverport = ntohs(lis->address.ipv4.sin_port);
    }
    else
    {
      strcpy(con->servername, "localhost");
      con->serverport = LocalPort;
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdAcceptClient: %d connected to server on %s:%d",
                  con->http.fd, con->servername, con->serverport);

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

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdAcceptClient: Adding fd %d to InputSet...",
                  con->http.fd);
  FD_SET(con->http.fd, InputSet);

  NumClients ++;

 /*
  * Temporarily suspend accept()'s until we lose a client...
  */

  if (NumClients == MaxClients)
    cupsdPauseListening();

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

    cupsdEncryptClient(con);
  }
  else
    con->auto_ssl = 1;
#endif /* HAVE_SSL */
}


/*
 * 'cupsdCloseAllClients()' - Close all remote clients immediately.
 */

void
cupsdCloseAllClients(void)
{
  while (NumClients > 0)
    cupsdCloseClient(Clients);
}


/*
 * 'cupsdCloseClient()' - Close a remote client.
 */

int					/* O - 1 if partial close, 0 if fully closed */
cupsdCloseClient(cupsd_client_t *con)	/* I - Client to close */
{
  int		partial;		/* Do partial close for SSL? */
#ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */
  unsigned long	error;			/* Error code */
#elif defined(HAVE_GNUTLS)
  http_tls_t     *conn;			/* TLS connection information */
  int            error;			/* Error code */
  gnutls_certificate_server_credentials *credentials;
					/* TLS credentials */
#endif /* HAVE_LIBSSL */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdCloseClient: %d", con->http.fd);

 /*
  * Flush pending writes before closing...
  */

  httpFlushWrite(HTTP(con));

  partial = 0;

#ifdef HAVE_SSL
 /*
  * Shutdown encryption as needed...
  */

  if (con->http.tls)
  {
    partial = 1;

#  ifdef HAVE_LIBSSL
    conn    = (SSL *)(con->http.tls);
    context = SSL_get_SSL_CTX(conn);

    switch (SSL_shutdown(conn))
    {
      case 1 :
          cupsdLogMessage(CUPSD_LOG_INFO,
	                  "cupsdCloseClient: SSL shutdown successful!");
	  break;
      case -1 :
          cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "cupsdCloseClient: Fatal error during SSL shutdown!");
      default :
	  while ((error = ERR_get_error()) != 0)
	    cupsdLogMessage(CUPSD_LOG_ERROR, "cupsdCloseClient: %s",
	                    ERR_error_string(error, NULL));
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
	cupsdLogMessage(CUPSD_LOG_INFO,
	                "cupsdCloseClient: SSL shutdown successful!");
	break;
      default:
	cupsdLogMessage(CUPSD_LOG_ERROR,
	                "cupsdCloseClient: %s", gnutls_strerror(error));
	break;
    }

    gnutls_deinit(conn->session);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    free(conn);

#  elif defined(HAVE_CDSASSL)
    SSLClose((SSLContextRef)con->http.tls);
    SSLDisposeContext((SSLContextRef)con->http.tls);
#  endif /* HAVE_LIBSSL */

    con->http.tls = NULL;
  }
#endif /* HAVE_SSL */

  if (con->pipe_pid != 0)
  {
   /*
    * Stop any CGI process...
    */

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdCloseClient: %d Killing process ID %d...",
                    con->http.fd, con->pipe_pid);
    cupsdEndProcess(con->pipe_pid, 1);
    con->pipe_pid = 0;
  }

  if (con->file >= 0)
  {
    if (FD_ISSET(con->file, InputSet))
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdCloseClient: %d Removing fd %d from InputSet...",
        	      con->http.fd, con->file);
      FD_CLR(con->file, InputSet);
    }

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdCloseClient: %d Closing data file %d.",
                    con->http.fd, con->file);

    close(con->file);
    con->file = -1;
  }

 /*
  * Close the socket and clear the file from the input set for select()...
  */

  if (con->http.fd > 0)
  {
    if (partial)
    {
     /*
      * Only do a partial close so that the encrypted client gets everything.
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdCloseClient: Removing fd %d from OutputSet...",
        	      con->http.fd);
      shutdown(con->http.fd, 0);
      FD_CLR(con->http.fd, OutputSet);
    }
    else
    {
     /*
      * Shut the socket down fully...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdCloseClient: Removing fd %d from InputSet and OutputSet...",
        	      con->http.fd);
      close(con->http.fd);
      FD_CLR(con->http.fd, InputSet);
      FD_CLR(con->http.fd, OutputSet);
      con->http.fd = -1;
    }
  }

  if (!partial)
  {
   /*
    * Free memory...
    */

    if (con->http.input_set)
      free(con->http.input_set);

    httpClearCookie(HTTP(con));

    cupsdClearString(&con->filename);
    cupsdClearString(&con->command);
    cupsdClearString(&con->options);

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
      cupsdResumeListening();

   /*
    * Compact the list of clients as necessary...
    */

    NumClients --;

    if (con < (Clients + NumClients))
      memmove(con, con + 1, (Clients + NumClients - con) * sizeof(cupsd_client_t));
  }

  return (partial);
}


/*
 * 'cupsdEncryptClient()' - Enable encryption for the client...
 */

int					/* O - 1 on success, 0 on error */
cupsdEncryptClient(cupsd_client_t *con)	/* I - Client to encrypt */
{
#ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */
  SSL		*conn;			/* Connection for encryption */
  unsigned long	error;			/* Error code */


 /*
  * Create the SSL context and accept the connection...
  */

  context = SSL_CTX_new(SSLv23_server_method());

  SSL_CTX_set_options(context, SSL_OP_NO_SSLv2); /* Only use SSLv3 or TLS */
  SSL_CTX_use_PrivateKey_file(context, ServerKey, SSL_FILETYPE_PEM);
  SSL_CTX_use_certificate_file(context, ServerCertificate, SSL_FILETYPE_PEM);

  conn = SSL_new(context);

  SSL_set_fd(conn, con->http.fd);
  if (SSL_accept(conn) != 1)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdEncryptClient: Unable to encrypt connection from %s!",
                    con->http.hostname);

    while ((error = ERR_get_error()) != 0)
      cupsdLogMessage(CUPSD_LOG_ERROR, "cupsdEncryptClient: %s",
                      ERR_error_string(error, NULL));

    SSL_CTX_free(context);
    SSL_free(conn);
    return (0);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdEncryptClient: %d Connection from %s now encrypted.",
                  con->http.fd, con->http.hostname);

  con->http.tls = conn;
  return (1);
  
#elif defined(HAVE_GNUTLS)
  http_tls_t	*conn;			/* TLS session object */
  int		error;			/* Error code */
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
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdEncryptClient: Unable to encrypt connection from %s!",
                    con->http.hostname);
    cupsdLogMessage(CUPSD_LOG_ERROR, "cupsdEncryptClient: %s", strerror(errno));

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
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdEncryptClient: Unable to encrypt connection from %s!",
                    con->http.hostname);
    cupsdLogMessage(CUPSD_LOG_ERROR, "cupsdEncryptClient: %s",
                    gnutls_strerror(error));

    gnutls_deinit(conn->session);
    gnutls_certificate_free_credentials(*credentials);
    free(conn);
    free(credentials);
    return (0);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdEncryptClient: %d Connection from %s now encrypted.",
                  con->http.fd, con->http.hostname);

  conn->credentials = credentials;
  con->http.tls = conn;
  return (1);

#elif defined(HAVE_CDSASSL)
  OSStatus	error;			/* Error info */
  SSLContextRef	conn;			/* New connection */
  int		allowExpired;		/* Allow expired certificates? */
  int		allowAnyRoot;		/* Allow any root certificate? */


  conn         = NULL;
  error        = SSLNewContext(true, &conn);
  allowExpired = 1;
  allowAnyRoot = 1;

  if (!ServerCertificatesArray)
    ServerCertificatesArray = get_cdsa_server_certs();

  if (!ServerCertificatesArray)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
        	    "EncryptClient: Could not find signing key in keychain "
		    "\"%s\"", ServerCertificate);
    error = errSSLBadConfiguration;
  }

  if (!error)
    error = SSLSetIOFuncs(conn, _httpReadCDSA, _httpWriteCDSA);

  if (!error)
    error = SSLSetProtocolVersion(conn, kSSLProtocol3);

  if (!error)
    error = SSLSetConnection(conn, (SSLConnectionRef)con->http.fd);

  if (!error)
    error = SSLSetPeerDomainName(conn, ServerName, strlen(ServerName) + 1);

  /* have to do these options before setting server certs */
  if (!error && allowExpired)
    error = SSLSetAllowsExpiredCerts(conn, true);

  if (!error && allowAnyRoot)
    error = SSLSetAllowsAnyRoot(conn, true);

  if (!error && ServerCertificatesArray)
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
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdEncryptClient: Unable to encrypt connection from %s!",
                    con->http.hostname);

    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdEncryptClient: CDSA error code is %d", (int)error);

    con->http.error  = error;
    con->http.status = HTTP_ERROR;

    if (conn != NULL)
      SSLDisposeContext(conn);

    return (0);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "cupsdEncryptClient: %d Connection from %s now encrypted.",
                  con->http.fd, con->http.hostname);

  con->http.tls = conn;
  return (1);

#else
  return (0);
#endif /* HAVE_LIBSSL */
}


/*
 * 'cupsdIsCGI()' - Is the resource a CGI script/program?
 */

int					/* O - 1 = CGI, 0 = file */
cupsdIsCGI(cupsd_client_t *con,		/* I - Client connection */
	   const char     *filename,	/* I - Real filename */
	   struct stat    *filestats,	/* I - File information */
	   mime_type_t    *type)	/* I - MIME type */
{
  const char	*options;		/* Options on URL */


  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdIsCGI(con=%p, filename=\"%s\", filestats=%p, type=%s/%s)\n",
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

  if (!type || strcasecmp(type->super, "application"))
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdIsCGI: Returning 0...");
    return (0);
  }

  if (!strcasecmp(type->type, "x-httpd-cgi") &&
      (filestats->st_mode & 0111))
  {
   /*
    * "application/x-httpd-cgi" is a CGI script.
    */

    cupsdSetString(&con->command, filename);

    filename = strrchr(filename, '/') + 1; /* Filename always absolute */

    if (options)
      cupsdSetStringf(&con->options, "%s %s", filename, options);
    else
      cupsdSetStringf(&con->options, "%s", filename);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
                    con->command, con->options);

    return (1);
  }
#ifdef HAVE_JAVA
  else if (!strcasecmp(type->type, "x-httpd-java"))
  {
   /*
    * "application/x-httpd-java" is a Java servlet.
    */

    cupsdSetString(&con->command, CUPS_JAVA);

    if (options)
      cupsdSetStringf(&con->options, "java %s %s", filename, options);
    else
      cupsdSetStringf(&con->options, "java %s", filename);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
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

    cupsdSetString(&con->command, CUPS_PERL);

    if (options)
      cupsdSetStringf(&con->options, "perl %s %s", filename, options);
    else
      cupsdSetStringf(&con->options, "perl %s", filename);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
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

    cupsdSetString(&con->command, CUPS_PHP);

    if (options)
      cupsdSetStringf(&con->options, "php %s %s", filename, options);
    else
      cupsdSetStringf(&con->options, "php %s", filename);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
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

    cupsdSetString(&con->command, CUPS_PYTHON);

    if (options)
      cupsdSetStringf(&con->options, "python %s %s", filename, options);
    else
      cupsdSetStringf(&con->options, "python %s", filename);

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdIsCGI: Returning 1 with command=\"%s\" and options=\"%s\"",
                    con->command, con->options);

    return (1);
  }
#endif /* HAVE_PYTHON */

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdIsCGI: Returning 0...");

  return (0);
}


/*
 * 'cupsdReadClient()' - Read data from a client.
 */

int					/* O - 1 on success, 0 on error */
cupsdReadClient(cupsd_client_t *con)	/* I - Client to read from */
{
  char			line[32768],	/* Line from client... */
			operation[64],	/* Operation code from socket */
			version[64],	/* HTTP version number string */
			locale[64],	/* Locale */
			*ptr;		/* Pointer into strings */
  int			major, minor;	/* HTTP version numbers */
  http_status_t		status;		/* Transfer status */
  ipp_state_t		ipp_state;	/* State of IPP transfer */
  int			bytes;		/* Number of bytes to POST */
  char			*filename;	/* Name of file for GET/HEAD */
  char			buf[1024];	/* Buffer for real filename */
  struct stat		filestats;	/* File information */
  mime_type_t		*type;		/* MIME type of file */
  cupsd_printer_t	*p;		/* Printer */
  static unsigned	request_id = 0;	/* Request ID for temp files */


  status = HTTP_CONTINUE;

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdReadClient: %d, used=%d, file=%d state=%d",
                  con->http.fd, con->http.used, con->file, con->http.state);

  if (con->http.error)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdReadClient: http error seen...");
    return (cupsdCloseClient(con));
  }

#ifdef HAVE_SSL
  if (con->auto_ssl)
  {
   /*
    * Automatically check for a SSL/TLS handshake...
    */

    con->auto_ssl = 0;

    if (recv(con->http.fd, buf, 1, MSG_PEEK) == 1 &&
        (!buf[0] || !strchr("DGHOPT", buf[0])))
    {
     /*
      * Encrypt this connection...
      */

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdReadClient: Saw first byte %02X, auto-negotiating SSL/TLS session...",
                      buf[0] & 255);

      cupsdEncryptClient(con);
      return (1);
    }
  }
#endif /* HAVE_SSL */

  switch (con->http.state)
  {
    case HTTP_WAITING :
       /*
        * See if we've received a request line...
	*/

        if (httpGets(line, sizeof(line) - 1, HTTP(con)) == NULL)
	{
	  cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdReadClient: httpGets returned EOF...");
          return (cupsdCloseClient(con));
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

        con->http.activity        = time(NULL);
        con->http.version         = HTTP_1_0;
	con->http.keep_alive      = HTTP_KEEPALIVE_OFF;
	con->http.data_encoding   = HTTP_ENCODE_LENGTH;
	con->http.data_remaining  = 0;
	con->http._data_remaining = 0;
	con->operation            = HTTP_WAITING;
	con->bytes                = 0;
	con->file                 = -1;
	con->file_ready           = 0;
	con->pipe_pid             = 0;
	con->username[0]          = '\0';
	con->password[0]          = '\0';
	con->uri[0]               = '\0';

	cupsdClearString(&con->command);
	cupsdClearString(&con->options);

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
	      cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "Bad request line \"%s\" from %s!", line,
	                      con->http.hostname);
	      cupsdSendError(con, HTTP_BAD_REQUEST);
	      return (cupsdCloseClient(con));
	  case 2 :
	      con->http.version = HTTP_0_9;
	      break;
	  case 3 :
	      if (sscanf(version, "HTTP/%d.%d", &major, &minor) != 2)
	      {
		cupsdLogMessage(CUPSD_LOG_ERROR,
		                "Bad request line \"%s\" from %s!", line,
	                        con->http.hostname);
		cupsdSendError(con, HTTP_BAD_REQUEST);
		return (cupsdCloseClient(con));
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
	        cupsdSendError(con, HTTP_NOT_SUPPORTED);
	        return (cupsdCloseClient(con));
	      }
	      break;
	}

       /*
        * Handle full URLs in the request line...
	*/

        if (con->uri[0] != '/' && strcmp(con->uri, "*"))
	{
	  char	method[HTTP_MAX_URI],	/* Method/scheme */
		userpass[HTTP_MAX_URI],	/* Username:password */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource path */
          int	port;			/* Port number */


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

	    cupsdLogMessage(CUPSD_LOG_ERROR, "Bad URI \"%s\" in request!",
	                    con->uri);
	    cupsdSendError(con, HTTP_METHOD_NOT_ALLOWED);
	    return (cupsdCloseClient(con));
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

        if (!strcmp(operation, "GET"))
	  con->http.state = HTTP_GET;
        else if (!strcmp(operation, "PUT"))
	  con->http.state = HTTP_PUT;
        else if (!strcmp(operation, "POST"))
	  con->http.state = HTTP_POST;
        else if (!strcmp(operation, "DELETE"))
	  con->http.state = HTTP_DELETE;
        else if (!strcmp(operation, "TRACE"))
	  con->http.state = HTTP_TRACE;
        else if (!strcmp(operation, "OPTIONS"))
	  con->http.state = HTTP_OPTIONS;
        else if (!strcmp(operation, "HEAD"))
	  con->http.state = HTTP_HEAD;
	else
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR, "Bad operation \"%s\"!", operation);
	  cupsdSendError(con, HTTP_BAD_REQUEST);
	  return (cupsdCloseClient(con));
	}

        con->start     = time(NULL);
        con->operation = con->http.state;

        cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdReadClient: %d %s %s HTTP/%d.%d", con->http.fd,
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
	  cupsdSendError(con, HTTP_BAD_REQUEST);
	  return (cupsdCloseClient(con));
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
    if (con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE][0])
    {
     /*
      * Figure out the locale from the Accept-Language and Content-Type
      * fields...
      */

      if ((ptr = strchr(con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE], ',')) != NULL)
        *ptr = '\0';

      if ((ptr = strchr(con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE], ';')) != NULL)
        *ptr = '\0';

      if ((ptr = strstr(con->http.fields[HTTP_FIELD_CONTENT_TYPE], "charset=")) != NULL)
      {
       /*
        * Combine language and charset, and trim any extra params in the
	* content-type.
	*/

        snprintf(locale, sizeof(locale), "%s.%s",
	         con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE], ptr + 8);

	if ((ptr = strchr(locale, ',')) != NULL)
	  *ptr = '\0';
      }
      else
        snprintf(locale, sizeof(locale), "%s.%s",
	         con->http.fields[HTTP_FIELD_ACCEPT_LANGUAGE], DefaultCharset);
        
      con->language = cupsLangGet(locale);
    }
    else
      con->language = cupsLangGet(DefaultLocale);

    cupsdAuthorize(con);

    if (!strncmp(con->http.fields[HTTP_FIELD_CONNECTION], "Keep-Alive", 10) &&
        KeepAlive)
      con->http.keep_alive = HTTP_KEEPALIVE_ON;

    if (!con->http.fields[HTTP_FIELD_HOST][0] &&
        con->http.version >= HTTP_1_1)
    {
     /*
      * HTTP/1.1 and higher require the "Host:" field...
      */

      if (!cupsdSendError(con, HTTP_BAD_REQUEST))
	return (cupsdCloseClient(con));
    }
    else if (con->operation == HTTP_OPTIONS)
    {
     /*
      * Do OPTIONS command...
      */

      if (con->best && con->best->type != AUTH_NONE)
      {
	if (!cupsdSendHeader(con, HTTP_UNAUTHORIZED, NULL))
	  return (cupsdCloseClient(con));
      }

      if (!strcasecmp(con->http.fields[HTTP_FIELD_CONNECTION], "Upgrade") &&
	  con->http.tls == NULL)
      {
#ifdef HAVE_SSL
       /*
        * Do encryption stuff...
	*/

	if (!cupsdSendHeader(con, HTTP_SWITCHING_PROTOCOLS, NULL))
	  return (cupsdCloseClient(con));

	httpPrintf(HTTP(con), "Connection: Upgrade\r\n");
	httpPrintf(HTTP(con), "Upgrade: TLS/1.0,HTTP/1.1\r\n");
	httpPrintf(HTTP(con), "Content-Length: 0\r\n");
	httpPrintf(HTTP(con), "\r\n");

        cupsdEncryptClient(con);
#else
	if (!cupsdSendError(con, HTTP_NOT_IMPLEMENTED))
	  return (cupsdCloseClient(con));
#endif /* HAVE_SSL */
      }

      if (con->http.expect)
      {
        /**** TODO: send expected header ****/
      }

      if (!cupsdSendHeader(con, HTTP_OK, NULL))
	return (cupsdCloseClient(con));

      httpPrintf(HTTP(con), "Allow: GET, HEAD, OPTIONS, POST, PUT\r\n");
      httpPrintf(HTTP(con), "Content-Length: 0\r\n");
      httpPrintf(HTTP(con), "\r\n");
    }
    else if (!is_path_absolute(con->uri))
    {
     /*
      * Protect against malicious users!
      */

      if (!cupsdSendError(con, HTTP_FORBIDDEN))
	return (cupsdCloseClient(con));
    }
    else
    {
      if (!strcasecmp(con->http.fields[HTTP_FIELD_CONNECTION], "Upgrade") &&
	  con->http.tls == NULL)
      {
#ifdef HAVE_SSL
       /*
        * Do encryption stuff...
	*/

	if (!cupsdSendHeader(con, HTTP_SWITCHING_PROTOCOLS, NULL))
	  return (cupsdCloseClient(con));

	httpPrintf(HTTP(con), "Connection: Upgrade\r\n");
	httpPrintf(HTTP(con), "Upgrade: TLS/1.0,HTTP/1.1\r\n");
	httpPrintf(HTTP(con), "Content-Length: 0\r\n");
	httpPrintf(HTTP(con), "\r\n");

        cupsdEncryptClient(con);
#else
	if (!cupsdSendError(con, HTTP_NOT_IMPLEMENTED))
	  return (cupsdCloseClient(con));
#endif /* HAVE_SSL */
      }

      if ((status = cupsdIsAuthorized(con, NULL)) != HTTP_OK)
      {
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "cupsdReadClient: Unauthorized request for %s...\n",
	                con->uri);
	cupsdSendError(con, status);
	return (cupsdCloseClient(con));
      }

      if (con->http.expect)
      {
        /**** TODO: send expected header ****/
      }

      switch (con->http.state)
      {
	case HTTP_GET_SEND :
            if (!strncmp(con->uri, "/printers/", 10) &&
		!strcmp(con->uri + strlen(con->uri) - 4, ".ppd"))
	    {
	     /*
	      * Send PPD file - get the real printer name since printer
	      * names are not case sensitive but filenames can be...
	      */

              con->uri[strlen(con->uri) - 4] = '\0';	/* Drop ".ppd" */

              if ((p = cupsdFindPrinter(con->uri + 10)) != NULL)
		snprintf(con->uri, sizeof(con->uri), "/ppd/%s.ppd", p->name);
	      else
	      {
		if (!cupsdSendError(con, HTTP_NOT_FOUND))
		  return (cupsdCloseClient(con));

		break;
	      }
	    }

	    if ((!strncmp(con->uri, "/admin", 6) &&
	         strncmp(con->uri, "/admin/conf/", 12) &&
	         strncmp(con->uri, "/admin/log/", 11)) ||
		!strncmp(con->uri, "/printers", 9) ||
		!strncmp(con->uri, "/classes", 8) ||
		!strncmp(con->uri, "/help", 5) ||
		!strncmp(con->uri, "/jobs", 5))
	    {
	     /*
	      * Send CGI output...
	      */

              if (!strncmp(con->uri, "/admin", 6))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/admin.cgi",
		                ServerBin);

		if ((ptr = strchr(con->uri + 6, '?')) != NULL)
		  cupsdSetStringf(&con->options, "admin%s", ptr);
		else
		  cupsdSetString(&con->options, "admin");
	      }
              else if (!strncmp(con->uri, "/printers", 9))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/printers.cgi",
		                ServerBin);
		cupsdSetString(&con->options, con->uri + 9);
	      }
	      else if (!strncmp(con->uri, "/classes", 8))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/classes.cgi",
		                ServerBin);
		cupsdSetString(&con->options, con->uri + 8);
	      }
	      else if (!strncmp(con->uri, "/jobs", 5))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/jobs.cgi",
		                ServerBin);
                cupsdSetString(&con->options, con->uri + 5);
	      }
	      else
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/help.cgi",
		                ServerBin);
                cupsdSetString(&con->options, con->uri + 5);
	      }

              if (con->options[0] == '/')
	        _cups_strcpy(con->options, con->options + 1);

              if (!cupsdSendCommand(con, con->command, con->options, 0))
	      {
		if (!cupsdSendError(con, HTTP_NOT_FOUND))
		  return (cupsdCloseClient(con));
              }
	      else
        	cupsdLogRequest(con, HTTP_OK);

	      if (con->http.version <= HTTP_1_0)
		con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	    }
            else if ((!strncmp(con->uri, "/admin/conf/", 12) &&
	              (strchr(con->uri + 12, '/') ||
		       strlen(con->uri) == 12)) ||
		     (!strncmp(con->uri, "/admin/log/", 11) &&
	              (strchr(con->uri + 11, '/') ||
		       strlen(con->uri) == 11)))
	    {
	     /*
	      * GET can only be done to configuration files under
	      * /admin/conf...
	      */

	      if (!cupsdSendError(con, HTTP_FORBIDDEN))
		return (cupsdCloseClient(con));

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
		if (!cupsdSendError(con, HTTP_NOT_FOUND))
		  return (cupsdCloseClient(con));

		break;
	      }

	      type = mimeFileType(MimeDatabase, filename, NULL);

              if (cupsdIsCGI(con, filename, &filestats, type))
	      {
	       /*
	        * Note: con->command and con->options were set by
		* cupsdIsCGI()...
		*/

        	if (!cupsdSendCommand(con, con->command, con->options, 0))
		{
		  if (!cupsdSendError(con, HTTP_NOT_FOUND))
		    return (cupsdCloseClient(con));
        	}
		else
        	  cupsdLogRequest(con, HTTP_OK);

		if (con->http.version <= HTTP_1_0)
		  con->http.keep_alive = HTTP_KEEPALIVE_OFF;
	        break;
	      }

	      if (!check_if_modified(con, &filestats))
              {
        	if (!cupsdSendError(con, HTTP_NOT_MODIFIED))
		  return (cupsdCloseClient(con));
	      }
	      else
              {
		if (type == NULL)
	          strcpy(line, "text/plain");
		else
	          snprintf(line, sizeof(line), "%s/%s", type->super, type->type);

        	if (!cupsdSendFile(con, HTTP_OK, filename, line, &filestats))
		  return (cupsdCloseClient(con));
	      }
	    }
            break;

	case HTTP_POST_RECV :
           /*
	    * See if the POST request includes a Content-Length field, and if
	    * so check the length against any limits that are set...
	    */

            cupsdLogMessage(CUPSD_LOG_DEBUG2, "POST %s", con->uri);
	    cupsdLogMessage(CUPSD_LOG_DEBUG2, "CONTENT_TYPE = %s",
	                    con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

            if (con->http.fields[HTTP_FIELD_CONTENT_LENGTH][0] &&
		MaxRequestSize > 0 &&
		con->http.data_remaining > MaxRequestSize)
	    {
	     /*
	      * Request too large...
	      */

              if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
		return (cupsdCloseClient(con));

	      break;
            }
	    else if (con->http.data_remaining < 0)
	    {
	     /*
	      * Negative content lengths are invalid!
	      */

              if (!cupsdSendError(con, HTTP_BAD_REQUEST))
		return (cupsdCloseClient(con));

	      break;
	    }

           /*
	    * See what kind of POST request this is; for IPP requests the
	    * content-type field will be "application/ipp"...
	    */

	    if (!strcmp(con->http.fields[HTTP_FIELD_CONTENT_TYPE],
	                "application/ipp"))
              con->request = ippNew();
	    else if ((!strncmp(con->uri, "/admin", 6) &&
	              strncmp(con->uri, "/admin/conf/", 12) &&
	              strncmp(con->uri, "/admin/log/", 11)) ||
	             !strncmp(con->uri, "/printers", 9) ||
	             !strncmp(con->uri, "/classes", 8) ||
	             !strncmp(con->uri, "/help", 5) ||
	             !strncmp(con->uri, "/jobs", 5))
	    {
	     /*
	      * CGI request...
	      */

              if (!strncmp(con->uri, "/admin", 6))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/admin.cgi",
		                ServerBin);

		if ((ptr = strchr(con->uri + 6, '?')) != NULL)
		  cupsdSetStringf(&con->options, "admin%s", ptr);
		else
		  cupsdSetString(&con->options, "admin");
	      }
              else if (!strncmp(con->uri, "/printers", 9))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/printers.cgi",
		                ServerBin);
		cupsdSetString(&con->options, con->uri + 9);
	      }
	      else if (!strncmp(con->uri, "/classes", 8))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/classes.cgi",
		                ServerBin);
		cupsdSetString(&con->options, con->uri + 8);
	      }
	      else if (!strncmp(con->uri, "/jobs", 5))
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/jobs.cgi",
		                ServerBin);
		cupsdSetString(&con->options, con->uri + 5);
	      }
	      else
	      {
		cupsdSetStringf(&con->command, "%s/cgi-bin/help.cgi",
		                ServerBin);
		cupsdSetString(&con->options, con->uri + 5);
	      }

	      if (con->options[0] == '/')
		_cups_strcpy(con->options, con->options + 1);

              cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                      "cupsdReadClient: %d command=\"%s\", options = \"%s\"",
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
		if (!cupsdSendError(con, HTTP_NOT_FOUND))
		  return (cupsdCloseClient(con));

		break;
	      }

	      type = mimeFileType(MimeDatabase, filename, NULL);

              if (!cupsdIsCGI(con, filename, &filestats, type))
	      {
	       /*
	        * Only POST to CGI's...
		*/

		if (!cupsdSendError(con, HTTP_UNAUTHORIZED))
		  return (cupsdCloseClient(con));
	      }
	    }
	    break;

	case HTTP_PUT_RECV :
	   /*
	    * Validate the resource name...
	    */

            if (strncmp(con->uri, "/admin/conf/", 12) ||
	        strchr(con->uri + 12, '/') ||
		strlen(con->uri) == 12)
	    {
	     /*
	      * PUT can only be done to configuration files under
	      * /admin/conf...
	      */

	      if (!cupsdSendError(con, HTTP_FORBIDDEN))
		return (cupsdCloseClient(con));

	      break;
	    }

           /*
	    * See if the PUT request includes a Content-Length field, and if
	    * so check the length against any limits that are set...
	    */

            cupsdLogMessage(CUPSD_LOG_DEBUG2, "PUT %s", con->uri);
	    cupsdLogMessage(CUPSD_LOG_DEBUG2, "CONTENT_TYPE = %s",
	                    con->http.fields[HTTP_FIELD_CONTENT_TYPE]);

            if (con->http.fields[HTTP_FIELD_CONTENT_LENGTH][0] &&
		MaxRequestSize > 0 &&
		con->http.data_remaining > MaxRequestSize)
	    {
	     /*
	      * Request too large...
	      */

              if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
		return (cupsdCloseClient(con));

	      break;
            }
	    else if (con->http.data_remaining < 0)
	    {
	     /*
	      * Negative content lengths are invalid!
	      */

              if (!cupsdSendError(con, HTTP_BAD_REQUEST))
		return (cupsdCloseClient(con));

	      break;
	    }

           /*
	    * Open a temporary file to hold the request...
	    */

            cupsdSetStringf(&con->filename, "%s/%08x", RequestRoot,
	                    request_id ++);
	    con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);

            cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                    "cupsdReadClient: %d REQUEST %s=%d", con->http.fd,
	                    con->filename, con->file);

	    if (con->file < 0)
	    {
	      if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
		return (cupsdCloseClient(con));
	    }

	    fchmod(con->file, 0640);
	    fchown(con->file, RunUser, Group);
	    fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);
	    break;

	case HTTP_DELETE :
	case HTTP_TRACE :
            cupsdSendError(con, HTTP_NOT_IMPLEMENTED);
	    return (cupsdCloseClient(con));

	case HTTP_HEAD :
            if (!strncmp(con->uri, "/printers/", 10) &&
		!strcmp(con->uri + strlen(con->uri) - 4, ".ppd"))
	    {
	     /*
	      * Send PPD file - get the real printer name since printer
	      * names are not case sensitive but filenames can be...
	      */

              con->uri[strlen(con->uri) - 4] = '\0';	/* Drop ".ppd" */

              if ((p = cupsdFindPrinter(con->uri + 10)) != NULL)
		snprintf(con->uri, sizeof(con->uri), "/ppd/%s.ppd", p->name);
	      else
	      {
		if (!cupsdSendError(con, HTTP_NOT_FOUND))
		  return (cupsdCloseClient(con));

		break;
	      }
	    }

	    if ((!strncmp(con->uri, "/admin", 6) &&
	         strncmp(con->uri, "/admin/conf/", 12) &&
	         strncmp(con->uri, "/admin/log/", 11)) ||
		!strncmp(con->uri, "/printers", 9) ||
		!strncmp(con->uri, "/classes", 8) ||
		!strncmp(con->uri, "/help", 5) ||
		!strncmp(con->uri, "/jobs", 5))
	    {
	     /*
	      * CGI output...
	      */

              if (!cupsdSendHeader(con, HTTP_OK, "text/html"))
		return (cupsdCloseClient(con));

	      if (httpPrintf(HTTP(con), "\r\n") < 0)
		return (cupsdCloseClient(con));

              cupsdLogRequest(con, HTTP_OK);
	    }
            else if ((!strncmp(con->uri, "/admin/conf/", 12) &&
	              (strchr(con->uri + 12, '/') ||
		       strlen(con->uri) == 12)) ||
		     (!strncmp(con->uri, "/admin/log/", 11) &&
	              (strchr(con->uri + 11, '/') ||
		       strlen(con->uri) == 11)))
	    {
	     /*
	      * HEAD can only be done to configuration files under
	      * /admin/conf...
	      */

	      if (!cupsdSendError(con, HTTP_FORBIDDEN))
		return (cupsdCloseClient(con));

	      break;
	    }
	    else if ((filename = get_file(con, &filestats, buf,
	                                  sizeof(buf))) == NULL)
	    {
	      if (!cupsdSendHeader(con, HTTP_NOT_FOUND, "text/html"))
		return (cupsdCloseClient(con));

              cupsdLogRequest(con, HTTP_NOT_FOUND);
	    }
	    else if (!check_if_modified(con, &filestats))
            {
              if (!cupsdSendError(con, HTTP_NOT_MODIFIED))
		return (cupsdCloseClient(con));

              cupsdLogRequest(con, HTTP_NOT_MODIFIED);
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

              if (!cupsdSendHeader(con, HTTP_OK, line))
		return (cupsdCloseClient(con));

	      if (httpPrintf(HTTP(con), "Last-Modified: %s\r\n",
	                     httpGetDateString(filestats.st_mtime)) < 0)
		return (cupsdCloseClient(con));

	      if (httpPrintf(HTTP(con), "Content-Length: %lu\r\n",
	                     (unsigned long)filestats.st_size) < 0)
		return (cupsdCloseClient(con));

              cupsdLogRequest(con, HTTP_OK);
	    }

            if (httpPrintf(HTTP(con), "\r\n") < 0)
	      return (cupsdCloseClient(con));

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
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	        	"cupsdReadClient: %d con->data_encoding=HTTP_ENCODE_%s, "
			"con->data_remaining=" CUPS_LLFMT ", con->file=%d",
			con->http.fd,
			con->http.data_encoding == HTTP_ENCODE_CHUNKED ?
			    "CHUNKED" : "LENGTH",
			CUPS_LLCAST con->http.data_remaining, con->file);

        if ((bytes = httpRead(HTTP(con), line, sizeof(line))) < 0)
	  return (cupsdCloseClient(con));
	else if (bytes > 0)
	{
	  con->bytes += bytes;

          cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdReadClient: %d writing %d bytes to %d",
	                  con->http.fd, bytes, con->file);

          if (write(con->file, line, bytes) < bytes)
	  {
            cupsdLogMessage(CUPSD_LOG_ERROR,
	                    "cupsdReadClient: Unable to write %d bytes to %s: %s",
	                    bytes, con->filename, strerror(errno));

	    cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                    "cupsdReadClient: Closing data file %d...",
        	            con->file);

	    close(con->file);
	    con->file = -1;
	    unlink(con->filename);
	    cupsdClearString(&con->filename);

            if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
	      return (cupsdCloseClient(con));
	  }
	}

        if (con->http.state == HTTP_WAITING)
	{
	 /*
	  * End of file, see how big it is...
	  */

	  fstat(con->file, &filestats);

          cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                  "cupsdReadClient: %d Closing data file %d, size="
			  CUPS_LLFMT ".",
                          con->http.fd, con->file,
			  CUPS_LLCAST filestats.st_size);

	  close(con->file);
	  con->file = -1;

          if (filestats.st_size > MaxRequestSize &&
	      MaxRequestSize > 0)
	  {
	   /*
	    * Request is too big; remove it and send an error...
	    */

            cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                    "cupsdReadClient: %d Removing temp file %s",
	                    con->http.fd, con->filename);
	    unlink(con->filename);
	    cupsdClearString(&con->filename);

            if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
	      return (cupsdCloseClient(con));
	  }

         /*
	  * Install the configuration file...
	  */

          status = install_conf_file(con);

         /*
	  * Return the status to the client...
	  */

          if (!cupsdSendError(con, status))
	    return (cupsdCloseClient(con));
	}
        break;

    case HTTP_POST_RECV :
        cupsdLogMessage(CUPSD_LOG_DEBUG2,
	        	"cupsdReadClient: %d con->data_encoding=HTTP_ENCODE_"
			"%s, con->data_remaining=" CUPS_LLFMT ", con->file=%d",
			con->http.fd,
			con->http.data_encoding == HTTP_ENCODE_CHUNKED ?
			    "CHUNKED" : "LENGTH",
			CUPS_LLCAST con->http.data_remaining, con->file);

        if (con->request != NULL)
	{
	 /*
	  * Grab any request data from the connection...
	  */

	  if ((ipp_state = ippRead(&(con->http), con->request)) == IPP_ERROR)
	  {
            cupsdLogMessage(CUPSD_LOG_ERROR,
	                    "cupsdReadClient: %d IPP Read Error!",
			    con->http.fd);

	    cupsdSendError(con, HTTP_BAD_REQUEST);
	    return (cupsdCloseClient(con));
	  }
	  else if (ipp_state != IPP_DATA)
	  {
            if (con->http.state == HTTP_POST_SEND)
	    {
	      cupsdSendError(con, HTTP_BAD_REQUEST);
	      return (cupsdCloseClient(con));
	    }

	    break;
          }
	  else
	    con->bytes += ippLength(con->request);
	}

        if (con->file < 0 && con->http.state != HTTP_POST_SEND)
	{
         /*
	  * Create a file as needed for the request data...
	  */

          cupsdSetStringf(&con->filename, "%s/%08x", RequestRoot, request_id ++);
	  con->file = open(con->filename, O_WRONLY | O_CREAT | O_TRUNC, 0640);

          cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdReadClient: %d REQUEST %s=%d", con->http.fd,
	                  con->filename, con->file);

	  if (con->file < 0)
	  {
	    if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
	      return (cupsdCloseClient(con));
	  }

	  fchmod(con->file, 0640);
	  fchown(con->file, RunUser, Group);
          fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);
	}

	if (con->http.state != HTTP_POST_SEND)
	{
          if ((bytes = httpRead(HTTP(con), line, sizeof(line))) < 0)
	    return (cupsdCloseClient(con));
	  else if (bytes > 0)
	  {
	    con->bytes += bytes;

            cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                    "cupsdReadClient: %d writing %d bytes to %d",
	                    con->http.fd, bytes, con->file);

            if (write(con->file, line, bytes) < bytes)
	    {
              cupsdLogMessage(CUPSD_LOG_ERROR,
	                      "cupsdReadClient: Unable to write %d bytes to %s: %s",
	        	      bytes, con->filename, strerror(errno));

	      cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                      "cupsdReadClient: Closing file %d...",
        		      con->file);

	      close(con->file);
	      con->file = -1;
	      unlink(con->filename);
	      cupsdClearString(&con->filename);

              if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
		return (cupsdCloseClient(con));
	    }
	  }
	  else if (con->http.state == HTTP_POST_RECV)
            return (1); /* ??? */
	  else if (con->http.state != HTTP_POST_SEND)
	    return (cupsdCloseClient(con));
	}

	if (con->http.state == HTTP_POST_SEND)
	{
	  if (con->file >= 0)
	  {
	    fstat(con->file, &filestats);

            cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                    "cupsdReadClient: %d Closing data file %d, "
			    "size=" CUPS_LLFMT ".",
                            con->http.fd, con->file,
			    CUPS_LLCAST filestats.st_size);

	    close(con->file);
	    con->file = -1;

            if (filestats.st_size > MaxRequestSize &&
	        MaxRequestSize > 0)
	    {
	     /*
	      * Request is too big; remove it and send an error...
	      */

              cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                      "cupsdReadClient: %d Removing temp file %s",
	                      con->http.fd, con->filename);
	      unlink(con->filename);
	      cupsdClearString(&con->filename);

	      if (con->request)
	      {
	       /*
	        * Delete any IPP request data...
		*/

	        ippDelete(con->request);
		con->request = NULL;
              }

              if (!cupsdSendError(con, HTTP_REQUEST_TOO_LARGE))
		return (cupsdCloseClient(con));
	    }

	    if (con->command)
	    {
	      if (!cupsdSendCommand(con, con->command, con->options, 0))
	      {
		if (!cupsdSendError(con, HTTP_NOT_FOUND))
		  return (cupsdCloseClient(con));
              }
	      else
        	cupsdLogRequest(con, HTTP_OK);
            }
	  }

          if (con->request)
	    return (cupsdProcessIPPRequest(con));
	}
        break;

    default :
        break; /* Anti-compiler-warning-code */
  }

  if (!con->http.keep_alive && con->http.state == HTTP_WAITING)
    return (cupsdCloseClient(con));
  else
    return (1);
}


/*
 * 'cupsdSendCommand()' - Send output from a command via HTTP.
 */

int					/* O - 1 on success, 0 on failure */
cupsdSendCommand(
    cupsd_client_t *con,		/* I - Client connection */
    char           *command,		/* I - Command to run */
    char           *options,		/* I - Command-line options */
    int            root)		/* I - Run as root? */
{
  int	fd;				/* Standard input file descriptor */


  if (con->filename)
    fd = open(con->filename, O_RDONLY);
  else
    fd = open("/dev/null", O_RDONLY);

  if (fd < 0)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "cupsdSendCommand: %d Unable to open \"%s\" for reading: %s",
                    con->http.fd, con->filename ? con->filename : "/dev/null",
	            strerror(errno));
    return (0);
  }

  fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

  con->pipe_pid = pipe_command(con, fd, &(con->file), command, options, root);

  close(fd);

  cupsdLogMessage(CUPSD_LOG_INFO, "Started \"%s\" (pid=%d)", command,
                  con->pipe_pid);

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSendCommand: %d file=%d",
                  con->http.fd, con->file);

  if (con->pipe_pid == 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdSendCommand: Adding fd %d to InputSet...", con->file);
  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdSendCommand: Adding fd %d to OutputSet...",
                  con->http.fd);

  FD_SET(con->file, InputSet);
  FD_SET(con->http.fd, OutputSet);

  con->sent_header = 0;
  con->file_ready  = 0;
  con->got_fields  = 0;
  con->field_col   = 0;

  return (1);
}


/*
 * 'cupsdSendError()' - Send an error message via HTTP.
 */

int					/* O - 1 if successful, 0 otherwise */
cupsdSendError(cupsd_client_t *con,	/* I - Connection */
               http_status_t  code)	/* I - Error code */
{
 /*
  * Put the request in the access_log file...
  */

  cupsdLogRequest(con, code);

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSendError: %d code=%d (%s)",
                  con->http.fd, code, httpStatus(code));

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

  if (!cupsdSendHeader(con, code, NULL))
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

    char	message[1024];		/* Message for user */
    const char	*text;			/* Status-specific text */

    if (code == HTTP_UNAUTHORIZED)
      text = _cupsLangString(con->language,
                             _("Enter your username and password or the "
			       "root username and password to access this "
			       "page."));
    else if (code == HTTP_UPGRADE_REQUIRED)
      text = _cupsLangString(con->language,
                             _("You must use a https: URL to access this "
			       "page."));
    else
      text = "";

    snprintf(message, sizeof(message),
             "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
	     "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n"
	     "<HTML>\n"
	     "<HEAD>\n"
             "\t<META HTTP-EQUIV=\"Content-Type\" "
	     "CONTENT=\"text/html; charset=utf-8\">\n"
	     "\t<TITLE>%d %s</TITLE>\n"
	     "\t<LINK REL=\"STYLESHEET\" TYPE=\"text/css\" "
	     "HREF=\"/cups.css\">\n"
	     "</HEAD>\n"
             "<BODY>\n"
	     "<H1>%d %s</H1>\n"
	     "<P>%s</P>\n"
	     "</BODY>\n"
	     "</HTML>\n",
	     code, httpStatus(code), code, httpStatus(code), text);

    if (httpPrintf(HTTP(con), "Content-Type: text/html; charset=utf-8\r\n") < 0)
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
 * 'cupsdSendFile()' - Send a file via HTTP.
 */

int					/* O - 0 on failure, 1 on success */
cupsdSendFile(cupsd_client_t *con,	/* I - Client connection */
              http_status_t  code,	/* I - HTTP status */
	      char           *filename,	/* I - Filename */
	      char           *type,	/* I - File type */
	      struct stat    *filestats)/* O - File information */
{
  con->file = open(filename, O_RDONLY);

  cupsdLogMessage(CUPSD_LOG_DEBUG, "cupsdSendFile: %d file=%d", con->http.fd,
                  con->file);

  if (con->file < 0)
    return (0);

  fcntl(con->file, F_SETFD, fcntl(con->file, F_GETFD) | FD_CLOEXEC);

  con->pipe_pid = 0;

  if (!cupsdSendHeader(con, code, type))
    return (0);

  if (httpPrintf(HTTP(con), "Last-Modified: %s\r\n",
                 httpGetDateString(filestats->st_mtime)) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Content-Length: " CUPS_LLFMT "\r\n",
                 CUPS_LLCAST filestats->st_size) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "\r\n") < 0)
    return (0);

  con->http.data_encoding  = HTTP_ENCODE_LENGTH;
  con->http.data_remaining = filestats->st_size;

  if (con->http.data_remaining <= INT_MAX)
    con->http._data_remaining = con->http.data_remaining;
  else
    con->http._data_remaining = INT_MAX;

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdSendFile: Adding fd %d to OutputSet...", con->http.fd);

  FD_SET(con->http.fd, OutputSet);

  return (1);
}


/*
 * 'cupsdSendHeader()' - Send an HTTP request.
 */

int					/* O - 1 on success, 0 on failure */
cupsdSendHeader(cupsd_client_t *con,	/* I - Client to send to */
                http_status_t  code,	/* I - HTTP status code */
	        char           *type)	/* I - MIME type of document */
{
  if (httpPrintf(HTTP(con), "HTTP/%d.%d %d %s\r\n", con->http.version / 100,
                 con->http.version % 100, code, httpStatus(code)) < 0)
    return (0);
  if (httpPrintf(HTTP(con), "Date: %s\r\n", httpGetDateString(time(NULL))) < 0)
    return (0);
  if (ServerHeader)
    if (httpPrintf(HTTP(con), "Server: %s\r\n", ServerHeader) < 0)
      return (0);
  if (con->http.keep_alive && con->http.version >= HTTP_1_0)
  {
    if (httpPrintf(HTTP(con), "Connection: Keep-Alive\r\n") < 0)
      return (0);
    if (httpPrintf(HTTP(con), "Keep-Alive: timeout=%d\r\n",
                   KeepAliveTimeout) < 0)
      return (0);
  }
  if (code == HTTP_METHOD_NOT_ALLOWED)
    if (httpPrintf(HTTP(con), "Allow: GET, HEAD, OPTIONS, POST\r\n") < 0)
      return (0);

  if (code == HTTP_UNAUTHORIZED)
  {
    int	auth_type;			/* Authentication type */


    if (!con->best || con->best->type == AUTH_NONE)
      auth_type = DefaultAuthType;
    else
      auth_type = con->best->type;
      
    if (auth_type != AUTH_DIGEST)
    {
      if (httpPrintf(HTTP(con),
                     "WWW-Authenticate: Basic realm=\"CUPS\"\r\n") < 0)
	return (0);
    }
    else
    {
      if (httpPrintf(HTTP(con),
                     "WWW-Authenticate: Digest realm=\"CUPS\", nonce=\"%s\"\r\n",
		     con->http.hostname) < 0)
	return (0);
    }
  }

  if (con->language != NULL)
  {
    if (httpPrintf(HTTP(con), "Content-Language: %s\r\n",
                   con->language->language) < 0)
      return (0);
  }

  if (type != NULL)
  {
    if (!strcmp(type, "text/html"))
    {
      if (httpPrintf(HTTP(con),
                     "Content-Type: text/html; charset=utf-8\r\n") < 0)
        return (0);
    }
    else if (httpPrintf(HTTP(con), "Content-Type: %s\r\n", type) < 0)
      return (0);
  }

  return (1);
}


/*
 * 'cupsdUpdateCGI()' - Read status messages from CGI scripts and programs.
 */

void
cupsdUpdateCGI(void)
{
  char		*ptr,			/* Pointer to end of line in buffer */
		message[1024];		/* Pointer to message text */
  int		loglevel;		/* Log level for message */


  while ((ptr = cupsdStatBufUpdate(CGIStatusBuffer, &loglevel,
                                   message, sizeof(message))) != NULL)
    if (!strchr(CGIStatusBuffer->buffer, '\n'))
      break;

  if (ptr == NULL && errno)
  {
   /*
    * Fatal error on pipe - should never happen!
    */

    cupsdLogMessage(CUPSD_LOG_CRIT,
                    "cupsdUpdateCGI: error reading from CGI error pipe - %s",
                    strerror(errno));
  }
}


/*
 * 'cupsdWriteClient()' - Write data to a client as needed.
 */

int					/* O - 1 if success, 0 if fail */
cupsdWriteClient(cupsd_client_t *con)	/* I - Client connection */
{
  int		bytes;			/* Number of bytes written */
  char		buf[16385];		/* Data buffer */
  char		*bufptr;		/* Pointer into buffer */
  ipp_state_t	ipp_state;		/* IPP state value */


#ifdef DEBUG
  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "cupsdWriteClient(con=%p) %d response=%p, file=%d "
		  "pipe_pid=%d state=%d",
                  con, con->http.fd, con->response, con->file, con->pipe_pid,
		  con->http.state);
#endif /* DEBUG */

  if (con->http.state != HTTP_GET_SEND &&
      con->http.state != HTTP_POST_SEND)
    return (1);

  if (con->response != NULL)
  {
    ipp_state = ippWrite(&(con->http), con->response);
    bytes     = ipp_state != IPP_ERROR && ipp_state != IPP_DATA;
  }
  else if ((bytes = read(con->file, buf, sizeof(buf) - 1)) > 0)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdWriteClient: Read %d bytes from file %d...",
                    bytes, con->file);

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

          cupsdLogMessage(CUPSD_LOG_DEBUG2, "Script header: %s", buf);

          if (!con->sent_header)
	  {
	   /*
	    * Handle redirection and CGI status codes...
	    */

            if (!strncasecmp(buf, "Location:", 9))
  	      cupsdSendHeader(con, HTTP_SEE_OTHER, NULL);
	    else if (!strncasecmp(buf, "Status:", 7))
  	      cupsdSendHeader(con, atoi(buf + 7), NULL);
	    else
  	      cupsdSendHeader(con, HTTP_OK, NULL);

	    if (con->http.version == HTTP_1_1)
	    {
	      con->http.data_encoding = HTTP_ENCODE_CHUNKED;

	      if (httpPrintf(HTTP(con), "Transfer-Encoding: chunked\r\n") < 0)
		return (0);
	    }

	    con->sent_header = 1;
	  }

	  if (strncasecmp(buf, "Status:", 7))
	    httpPrintf(HTTP(con), "%s\r\n", buf);

	  cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdWriteClient: %d %s",
	                  con->http.fd, buf);

         /*
	  * Update buffer...
	  */

	  bytes -= (bufptr - buf);
	  memmove(buf, bufptr, bytes + 1);
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

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdWriteClient: %d bytes=%d, got_fields=%d",
                      con->http.fd, bytes, con->got_fields);

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
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdWriteClient: %d Write of %d bytes failed!",
                      con->http.fd, bytes);

      cupsdCloseClient(con);
      return (0);
    }

    con->bytes += bytes;

    if (con->http.state == HTTP_WAITING)
      bytes = 0;
  }

  if (bytes <= 0)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG2, "cupsdWriteClient: %d bytes < 0",
                    con->http.fd);

    cupsdLogRequest(con, HTTP_OK);

    httpFlushWrite(HTTP(con));

    if (con->http.data_encoding == HTTP_ENCODE_CHUNKED)
    {
      if (httpPrintf(HTTP(con), "0\r\n\r\n") < 0)
      {
        cupsdCloseClient(con);
	return (0);
      }
    }

    con->http.state = HTTP_WAITING;

    cupsdLogMessage(CUPSD_LOG_DEBUG2,
                    "cupsdWriteClient: Removing fd %d from OutputSet...",
                    con->http.fd);

    FD_CLR(con->http.fd, OutputSet);

    if (con->file >= 0)
    {
      if (FD_ISSET(con->file, InputSet))
      {
	cupsdLogMessage(CUPSD_LOG_DEBUG2,
	                "cupsdWriteClient: Removing fd %d from InputSet...",
                        con->file);
	FD_CLR(con->file, InputSet);
      }

      if (con->pipe_pid)
	cupsdEndProcess(con->pipe_pid, 0);

      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdWriteClient: %d Closing data file %d.",
                      con->http.fd, con->file);

      close(con->file);
      con->file     = -1;
      con->pipe_pid = 0;
    }

    if (con->filename)
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdWriteClient: %d Removing temp file %s",
                      con->http.fd, con->filename);
      unlink(con->filename);
      cupsdClearString(&con->filename);
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

    cupsdClearString(&con->command);
    cupsdClearString(&con->options);

    if (!con->http.keep_alive)
    {
      cupsdCloseClient(con);
      return (0);
    }
  }
  else
  {
    con->file_ready = 0;

    if (con->pipe_pid && !FD_ISSET(con->file, InputSet))
    {
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "cupsdWriteClient: Adding fd %d to InputSet...",
		      con->file);
      FD_SET(con->file, InputSet);
    }
  }

  con->http.activity = time(NULL);

  return (1);
}


/*
 * 'check_if_modified()' - Decode an "If-Modified-Since" line.
 */

static int				/* O - 1 if modified since */
check_if_modified(
    cupsd_client_t *con,		/* I - Client connection */
    struct stat    *filestats)		/* I - File information */
{
  char		*ptr;			/* Pointer into field */
  time_t	date;			/* Time/date value */
  off_t		size;			/* Size/length value */


  size = 0;
  date = 0;
  ptr  = con->http.fields[HTTP_FIELD_IF_MODIFIED_SINCE];

  if (*ptr == '\0')
    return (1);

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "check_if_modified: %d If-Modified-Since=\"%s\"",
                  con->http.fd, ptr);

  while (*ptr != '\0')
  {
    while (isspace(*ptr) || *ptr == ';')
      ptr ++;

    if (strncasecmp(ptr, "length=", 7) == 0)
    {
      ptr += 7;
      size = strtoll(ptr, NULL, 10);

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

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "check_if_modified: %d sizes=" CUPS_LLFMT ","
		  CUPS_LLFMT " dates=%d,%d",
                  con->http.fd, CUPS_LLCAST size,
		  CUPS_LLCAST filestats->st_size, (int)date,
	          (int)filestats->st_mtime);

  return ((size != filestats->st_size && size != 0) ||
          (date < filestats->st_mtime && date != 0) ||
	  (size == 0 && date == 0));
}


#ifdef HAVE_CDSASSL
/*
 * 'get_cdsa_server_certs()' - Convert a keychain name into the CFArrayRef
 *			       required by SSLSetCertificate.
 *
 * For now we assumes that there is exactly one SecIdentity in the
 * keychain - i.e. there is exactly one matching cert/private key pair.
 * In the future we will search a keychain for a SecIdentity matching a
 * specific criteria.  We also skip the operation of adding additional
 * non-signing certs from the keychain to the CFArrayRef.
 *
 * To create a self-signed certificate for testing use the certtool.
 * Executing the following as root will do it:
 *
 *     certtool c c v k=CUPS
 */

static CFArrayRef			/* O - Array of certificates */
get_cdsa_server_certs(void)
{
  OSStatus		err;		/* Error info */
  SecKeychainRef	kcRef;		/* Keychain reference */
  SecIdentitySearchRef	srchRef;	/* Search reference */
  SecIdentityRef	identity;	/* Identity */
  CFArrayRef		ca;		/* Certificate array */


  kcRef    = NULL;
  srchRef  = NULL;
  identity = NULL;
  ca	   = NULL;
  err      = SecKeychainOpen(ServerCertificate, &kcRef);

  if (err)
    cupsdLogMessage(CUPSD_LOG_ERROR, "Cannot open keychain \"%s\", error %d.",
	            ServerCertificate, (int)err);
  else
  {
   /*
    * Search for "any" identity matching specified key use; 
    * in this app, we expect there to be exactly one. 
    */

    err = SecIdentitySearchCreate(kcRef, CSSM_KEYUSE_SIGN, &srchRef);

    if (err)
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
		      "Cannot find signing key in keychain \"%s\", error %d",
		      ServerCertificate, (int)err);
    else
    {
      err = SecIdentitySearchCopyNext(srchRef, &identity);

      if (err)
	cupsdLogMessage(CUPSD_LOG_DEBUG2,
			"Cannot find signing key in keychain \"%s\", error %d",
			ServerCertificate, (int)err);
      else
      {
	if (CFGetTypeID(identity) != SecIdentityGetTypeID())
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "SecIdentitySearchCopyNext CFTypeID failure!");
	else
	{
	 /* 
	  * Found one. Place it in a CFArray. 
	  * TBD: snag other (non-identity) certs from keychain and add them
	  * to array as well.
	  */

	  ca = CFArrayCreate(NULL, (const void **)&identity, 1, NULL);

	  if (ca == nil)
	    cupsdLogMessage(CUPSD_LOG_ERROR, "CFArrayCreate error");
	}

	/*CFRelease(identity);*/
      }

      /*CFRelease(srchRef);*/
    }

    /*CFRelease(kcRef);*/
  }

  return (ca);
}
#endif /* HAVE_CDSASSL */


/*
 * 'get_file()' - Get a filename and state info.
 */

static char *				/* O  - Real filename */
get_file(cupsd_client_t *con,		/* I  - Client connection */
         struct stat    *filestats,	/* O  - File information */
         char           *filename,	/* IO - Filename buffer */
         int            len)		/* I  - Buffer length */
{
  int		status;			/* Status of filesystem calls */
  char		*ptr;			/* Pointer info filename */
  int		plen;			/* Remaining length after pointer */


 /*
  * Need to add DocumentRoot global...
  */

  if (!strncmp(con->uri, "/ppd/", 5))
    snprintf(filename, len, "%s%s", ServerRoot, con->uri);
  else if (!strncmp(con->uri, "/admin/conf/", 12))
    snprintf(filename, len, "%s%s", ServerRoot, con->uri + 11);
  else if (!strncmp(con->uri, "/admin/log/", 11))
  {
    if (!strcmp(con->uri + 11, "access_log") && AccessLog[0] == '/')
      strlcpy(filename, AccessLog, len);
    else if (!strcmp(con->uri + 11, "error_log") && ErrorLog[0] == '/')
      strlcpy(filename, ErrorLog, len);
    else if (!strcmp(con->uri + 11, "page_log") && PageLog[0] == '/')
      strlcpy(filename, PageLog, len);
    else
      return (NULL);
  }
  else if (con->language != NULL)
    snprintf(filename, len, "%s/%s%s", DocumentRoot, con->language->language,
            con->uri);
  else
    snprintf(filename, len, "%s%s", DocumentRoot, con->uri);

  if ((ptr = strchr(filename, '?')) != NULL)
    *ptr = '\0';

 /*
  * Grab the status for this language; if there isn't a language-specific file
  * then fallback to the default one...
  */

  if ((status = stat(filename, filestats)) != 0 && con->language != NULL)
  {
   /*
    * Drop the language prefix and try the current directory...
    */

    if (strncmp(con->uri, "/ppd/", 5) &&
        strncmp(con->uri, "/admin/conf/", 12) &&
        strncmp(con->uri, "/admin/log/", 11))
    {
      snprintf(filename, len, "%s%s", DocumentRoot, con->uri);

      if ((ptr = strchr(filename, '?')) != NULL)
	*ptr = '\0';

      status = stat(filename, filestats);
    }
  }

 /*
  * If we're found a directory, get the index.html file instead...
  */

  if (!status && S_ISDIR(filestats->st_mode))
  {
    if (filename[strlen(filename) - 1] != '/')
      strlcat(filename, "/", len);

    ptr  = filename + strlen(filename);
    plen = len - (ptr - filename);

    strlcpy(ptr, "index.html", plen);
    status = stat(filename, filestats);

#ifdef HAVE_JAVA
    if (status)
    {
      strlcpy(ptr, "index.class", plen);
      status = stat(filename, filestats);
    }
#endif /* HAVE_JAVA */

#ifdef HAVE_PERL
    if (status)
    {
      strlcpy(ptr, "index.pl", plen);
      status = stat(filename, filestats);
    }
#endif /* HAVE_PERL */

#ifdef HAVE_PHP
    if (status)
    {
      strlcpy(ptr, "index.php", plen);
      status = stat(filename, filestats);
    }
#endif /* HAVE_PHP */

#ifdef HAVE_PYTHON
    if (status)
    {
      strlcpy(ptr, "index.pyc", plen);
      status = stat(filename, filestats);
    }

    if (status)
    {
      strlcpy(ptr, "index.py", plen);
      status = stat(filename, filestats);
    }
#endif /* HAVE_PYTHON */
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG2, "get_file: %d filename=%s size=%d",
                  con->http.fd, filename,
		  status ? -1 : (int)filestats->st_size);

  if (!status)
    con->http.data_remaining = (int)filestats->st_size;

  if (status)
    return (NULL);
  else
    return (filename);
}


/*
 * 'install_conf_file()' - Install a configuration file.
 */

static http_status_t			/* O - Status */
install_conf_file(cupsd_client_t *con)	/* I - Connection */
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

  cupsdLogMessage(CUPSD_LOG_INFO, "Installing config file \"%s\"...", conffile);

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
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open request file \"%s\" - %s",
                    con->filename, strerror(errno));
    return (HTTP_SERVER_ERROR);
  }

  if ((out = cupsFileOpen(newfile, "wb")) == NULL)
  {
    cupsFileClose(in);
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to open config file \"%s\" - %s",
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
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to copy to config file \"%s\" - %s",
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
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Error file closing config file \"%s\" - %s",
                    newfile, strerror(errno));

    unlink(newfile);

    return (HTTP_SERVER_ERROR);
  }

 /*
  * Remove the request file...
  */

  unlink(con->filename);
  cupsdClearString(&con->filename);

 /*
  * Unlink the old backup, rename the current config file to the backup
  * filename, and rename the new config file to the config file name...
  */

  if (unlink(oldfile))
    if (errno != ENOENT)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to remove backup config file \"%s\" - %s",
        	      oldfile, strerror(errno));

      unlink(newfile);

      return (HTTP_SERVER_ERROR);
    }

  if (rename(conffile, oldfile))
    if (errno != ENOENT)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to rename old config file \"%s\" - %s",
        	      conffile, strerror(errno));

      unlink(newfile);

      return (HTTP_SERVER_ERROR);
    }

  if (rename(newfile, conffile))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to rename new config file \"%s\" - %s",
                    newfile, strerror(errno));

    rename(oldfile, conffile);
    unlink(newfile);

    return (HTTP_SERVER_ERROR);
  }

 /*
  * If the cupsd.conf file was updated, set the NeedReload flag...
  */

  if (!strcmp(con->uri, "/admin/conf/cupsd.conf"))
    NeedReload = RELOAD_CUPSD;
  else
    NeedReload = RELOAD_ALL;

  ReloadTime = time(NULL);

 /*
  * Return that the file was created successfully...
  */

  return (HTTP_CREATED);
}


/*
 * 'is_path_absolute()' - Is a path absolute and free of relative elements (i.e. "..").
 */

static int				/* O - 0 if relative, 1 if absolute */
is_path_absolute(const char *path)	/* I - Input path */
{
 /*
  * Check for a leading slash...
  */

  if (path[0] != '/')
    return (0);

 /*
  * Check for "/.." in the path...
  */

  while ((path = strstr(path, "/..")) != NULL)
  {
    if (!path[3] || path[3] == '/')
      return (0);

    path ++;
  }

 /*
  * If we haven't found any relative paths, return 1 indicating an
  * absolute path...
  */

  return (1);
}


/*
 * 'pipe_command()' - Pipe the output of a command to the remote client.
 */

static int				/* O - Process ID */
pipe_command(cupsd_client_t *con,	/* I - Client connection */
             int            infile,	/* I - Standard input for command */
             int            *outfile,	/* O - Standard output for command */
	     char           *command,	/* I - Command to run */
	     char           *options,	/* I - Options for command */
	     int            root)	/* I - Run as root? */
{
  int		i;			/* Looping var */
  int		pid;			/* Process ID */
  char		*commptr;		/* Command string pointer */
  char		*uriptr;		/* URI string pointer */
  int		fds[2];			/* Pipe FDs */
  int		argc;			/* Number of arguments */
  int		envc;			/* Number of environment variables */
  char		argbuf[10240],		/* Argument buffer */
		*argv[100],		/* Argument strings */
		*envp[100];		/* Environment variables */
  char		content_length[1024],	/* CONTENT_LENGTH environment variable */
		content_type[1024],	/* CONTENT_TYPE environment variable */
		http_cookie[32768],	/* HTTP_COOKIE environment variable */
		http_user_agent[1024],	/* HTTP_USER_AGENT environment variable */
		lang[1024],		/* LANG environment variable */
		*query_string,		/* QUERY_STRING env variable */
		remote_addr[1024],	/* REMOTE_ADDR environment variable */
		remote_host[1024],	/* REMOTE_HOST environment variable */
		remote_user[1024],	/* REMOTE_USER environment variable */
		script_name[1024],	/* SCRIPT_NAME environment variable */
		server_name[1024],	/* SERVER_NAME environment variable */
		server_port[1024];	/* SERVER_PORT environment variable */


 /*
  * Parse a copy of the options string, which is of the form:
  *
  *     name argument+argument+argument
  *     name?argument+argument+argument
  *     name param=value&param=value
  *     name?param=value&param=value
  *
  * If the string contains an "=" character after the initial name,
  * then we treat it as a HTTP GET form request and make a copy of
  * the remaining string for the environment variable.
  *
  * The string is always parsed out as command-line arguments, to
  * be consistent with Apache...
  */

  cupsdLogMessage(CUPSD_LOG_DEBUG2,
                  "pipe_command: command=\"%s\", options=\"%s\"",
                  command, options);

  strlcpy(argbuf, options, sizeof(argbuf));

  argv[0]      = argbuf;
  query_string = NULL;

  for (commptr = argbuf, argc = 1; *commptr != '\0' && argc < 99; commptr ++)
  {
   /*
    * Break arguments whenever we see a + or space...
    */

    if (*commptr == ' ' || *commptr == '+' || (*commptr == '?' && argc == 1))
    {
     /*
      * Terminate the current string and skip trailing whitespace...
      */

      *commptr++ = '\0';

      while (*commptr == ' ')
        commptr ++;

     /*
      * If we don't have a blank string, save it as another argument...
      */

      if (*commptr)
      {
        argv[argc] = commptr;
	argc ++;
      }
      else
        break;

     /*
      * If we see an "=" in the remaining string, make a copy of it since
      * it will be query data...
      */

      if (argc == 2 && strchr(commptr, '=') && con->operation == HTTP_GET)
	cupsdSetStringf(&query_string, "QUERY_STRING=%s", commptr);

     /*
      * Don't skip the first non-blank character...
      */

      commptr --;
    }
    else if (*commptr == '%' && isxdigit(commptr[1] & 255) &&
             isxdigit(commptr[2] & 255))
    {
     /*
      * Convert the %xx notation to the individual character.
      */

      if (commptr[1] >= '0' && commptr[1] <= '9')
        *commptr = (commptr[1] - '0') << 4;
      else
        *commptr = (tolower(commptr[1]) - 'a' + 10) << 4;

      if (commptr[2] >= '0' && commptr[2] <= '9')
        *commptr |= commptr[2] - '0';
      else
        *commptr |= tolower(commptr[2]) - 'a' + 10;

      _cups_strcpy(commptr + 1, commptr + 3);

     /*
      * Check for a %00 and break if that is the case...
      */

      if (!*commptr)
        break;
    }
  }

  argv[argc] = NULL;

  if (argv[0][0] == '\0')
    argv[0] = strrchr(command, '/') + 1;

 /*
  * Setup the environment variables as needed...
  */

  if (con->language)
    snprintf(lang, sizeof(lang), "LANG=%s.UTF-8", con->language->language);
  else
    strcpy(lang, "LANG=C");

  strcpy(remote_addr, "REMOTE_ADDR=");
  httpAddrString(con->http.hostaddr, remote_addr + 12,
                 sizeof(remote_addr) - 12);

  snprintf(remote_host, sizeof(remote_host), "REMOTE_HOST=%s",
           con->http.hostname);

  snprintf(script_name, sizeof(script_name), "SCRIPT_NAME=%s", con->uri);
  if ((uriptr = strchr(script_name, '?')) != NULL)
    *uriptr = '\0';

  sprintf(server_port, "SERVER_PORT=%d", con->serverport);

  snprintf(server_name, sizeof(server_name), "SERVER_NAME=%s",
           con->servername);

  envc = cupsdLoadEnv(envp, (int)(sizeof(envp) / sizeof(envp[0])));

  envp[envc ++] = lang;
  envp[envc ++] = "REDIRECT_STATUS=1";
  envp[envc ++] = server_name;
  envp[envc ++] = server_port;
  envp[envc ++] = remote_addr;
  envp[envc ++] = remote_host;
  envp[envc ++] = script_name;

  if (con->username[0])
  {
    snprintf(remote_user, sizeof(remote_user), "REMOTE_USER=%s", con->username);

    envp[envc ++] = remote_user;
  }

  if (con->http.version == HTTP_1_1)
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/1.1";
  else if (con->http.version == HTTP_1_0)
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/1.0";
  else
    envp[envc ++] = "SERVER_PROTOCOL=HTTP/0.9";

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

  if (con->operation == HTTP_GET)
  {
    for (i = 0; i < argc; i ++)
      cupsdLogMessage(CUPSD_LOG_DEBUG2, "argv[%d] = \"%s\"", i, argv[i]);

    envp[envc ++] = "REQUEST_METHOD=GET";

    if (query_string)
    {
     /*
      * Add GET form variables after ?...
      */

      envp[envc ++] = query_string;
    }
  }
  else
  {
    sprintf(content_length, "CONTENT_LENGTH=" CUPS_LLFMT, con->bytes);
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
    envp[envc ++] = "HTTPS=ON";

 /*
  * Terminate the environment array...
  */

  envp[envc] = NULL;

  if (LogLevel == CUPSD_LOG_DEBUG2)
  {
    for (i = 0; i < argc; i ++)
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "pipe_command: argv[%d] = \"%s\"", i, argv[i]);
    for (i = 0; i < envc; i ++)
      cupsdLogMessage(CUPSD_LOG_DEBUG2,
                      "pipe_command: envp[%d] = \"%s\"", i, envp[i]);
  }

 /*
  * Create a pipe for the output...
  */

  if (cupsdOpenPipe(fds))
  {
    cupsdClearString(&query_string);

    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to create pipes for CGI %s - %s",
                    argv[0], strerror(errno));
    return (0);
  }

 /*
  * Then execute the command...
  */

  if (cupsdStartProcess(command, argv, envp, infile, fds[1], CGIPipes[1],
			-1, root, &pid) < 0)
  {
   /*
    * Error - can't fork!
    */

    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to fork for CGI %s - %s", argv[0],
                    strerror(errno));

    cupsdClosePipe(fds);
    pid = 0;
  }
  else
  {
   /*
    * Fork successful - return the PID...
    */

    if (con->username[0])
      cupsdAddCert(pid, con->username);

    cupsdLogMessage(CUPSD_LOG_DEBUG, "CGI %s started - PID = %d", command, pid);

    *outfile = fds[0];
    close(fds[1]);
  }

  cupsdClearString(&query_string);

  return (pid);
}


/*
 * End of "$Id$".
 */
