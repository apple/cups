/*
 * "$Id$"
 *
 * TLS support code for CUPS using GNU TLS.
 *
 * Copyright 2007-2013 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */


/*
 * Local functions...
 */

static int		make_certificate(cupsd_client_t *con);


/*
 * 'http_tls_initialize()' - Initialize the TLS stack.
 */

static void
http_tls_initialize(void)
{
#ifdef HAVE_GNUTLS
 /*
  * Initialize GNU TLS...
  */

  gnutls_global_init();

#elif defined(HAVE_LIBSSL)
 /*
  * Initialize OpenSSL...
  */

  SSL_load_error_strings();
  SSL_library_init();

 /*
  * Using the current time is a dubious random seed, but on some systems
  * it is the best we can do (on others, this seed isn't even used...)
  */

  CUPS_SRAND(time(NULL));

  for (i = 0; i < sizeof(data); i ++)
    data[i] = CUPS_RAND();

  RAND_seed(data, sizeof(data));
#endif /* HAVE_GNUTLS */
}


#ifdef HAVE_SSL
/*
 * 'http_tls_read()' - Read from a SSL/TLS connection.
 */

static int				/* O - Bytes read */
http_tls_read(http_t *http,		/* I - Connection to server */
	      char   *buf,		/* I - Buffer to store data */
	      int    len)		/* I - Length of buffer */
{
#  if defined(HAVE_LIBSSL)
  return (SSL_read((SSL *)(http->tls), buf, len));

#  elif defined(HAVE_GNUTLS)
  ssize_t	result;			/* Return value */


  result = gnutls_record_recv(http->tls, buf, len);

  if (result < 0 && !errno)
  {
   /*
    * Convert GNU TLS error to errno value...
    */

    switch (result)
    {
      case GNUTLS_E_INTERRUPTED :
	  errno = EINTR;
	  break;

      case GNUTLS_E_AGAIN :
          errno = EAGAIN;
          break;

      default :
          errno = EPIPE;
          break;
    }

    result = -1;
  }

  return ((int)result);

#  elif defined(HAVE_CDSASSL)
  int		result;			/* Return value */
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLRead(http->tls, buf, len, &processed);
  DEBUG_printf(("6http_tls_read: error=%d, processed=%d", (int)error,
                (int)processed));
  switch (error)
  {
    case 0 :
	result = (int)processed;
	break;

    case errSSLWouldBlock :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno  = EINTR;
	}
	break;

    case errSSLClosedGraceful :
    default :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno  = EPIPE;
	}
	break;
  }

  return (result);

#  elif defined(HAVE_SSPISSL)
  return _sspiRead((_sspi_struct_t*) http->tls, buf, len);
#  endif /* HAVE_LIBSSL */
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_setup_ssl()' - Set up SSL/TLS support on a connection.
 */

static int				/* O - 0 on success, -1 on failure */
http_setup_ssl(http_t *http)		/* I - Connection to server */
{
  char			hostname[256],	/* Hostname */
			*hostptr;	/* Pointer into hostname */

#  ifdef HAVE_LIBSSL
  SSL_CTX		*context;	/* Context for encryption */
  BIO			*bio;		/* BIO data */
  const char		*message = NULL;/* Error message */
#  elif defined(HAVE_GNUTLS)
  int			status;		/* Status of handshake */
  gnutls_certificate_client_credentials *credentials;
					/* TLS credentials */
#  elif defined(HAVE_CDSASSL)
  _cups_globals_t	*cg = _cupsGlobals();
					/* Pointer to library globals */
  OSStatus		error;		/* Error code */
  const char		*message = NULL;/* Error message */
  cups_array_t		*credentials;	/* Credentials array */
  cups_array_t		*names;		/* CUPS distinguished names */
  CFArrayRef		dn_array;	/* CF distinguished names array */
  CFIndex		count;		/* Number of credentials */
  CFDataRef		data;		/* Certificate data */
  int			i;		/* Looping var */
  http_credential_t	*credential;	/* Credential data */
#  elif defined(HAVE_SSPISSL)
  TCHAR			username[256];	/* Username returned from GetUserName() */
  TCHAR			commonName[256];/* Common name for certificate */
  DWORD			dwSize;		/* 32 bit size */
#  endif /* HAVE_LIBSSL */


  DEBUG_printf(("7http_setup_ssl(http=%p)", http));

 /*
  * Get the hostname to use for SSL...
  */

  if (httpAddrLocalhost(http->hostaddr))
  {
    strlcpy(hostname, "localhost", sizeof(hostname));
  }
  else
  {
   /*
    * Otherwise make sure the hostname we have does not end in a trailing dot.
    */

    strlcpy(hostname, http->hostname, sizeof(hostname));
    if ((hostptr = hostname + strlen(hostname) - 1) >= hostname &&
        *hostptr == '.')
      *hostptr = '\0';
  }

#  ifdef HAVE_LIBSSL
  context = SSL_CTX_new(SSLv23_client_method());

  SSL_CTX_set_options(context, SSL_OP_NO_SSLv2); /* Only use SSLv3 or TLS */

  bio = BIO_new(_httpBIOMethods());
  BIO_ctrl(bio, BIO_C_SET_FILE_PTR, 0, (char *)http);

  http->tls = SSL_new(context);
  SSL_set_bio(http->tls, bio, bio);

#   ifdef HAVE_SSL_SET_TLSEXT_HOST_NAME
  SSL_set_tlsext_host_name(http->tls, hostname);
#   endif /* HAVE_SSL_SET_TLSEXT_HOST_NAME */

  if (SSL_connect(http->tls) != 1)
  {
    unsigned long	error;	/* Error code */

    while ((error = ERR_get_error()) != 0)
    {
      message = ERR_error_string(error, NULL);
      DEBUG_printf(("8http_setup_ssl: %s", message));
    }

    SSL_CTX_free(context);
    SSL_free(http->tls);
    http->tls = NULL;

#    ifdef WIN32
    http->error  = WSAGetLastError();
#    else
    http->error  = errno;
#    endif /* WIN32 */
    http->status = HTTP_STATUS_ERROR;

    if (!message)
      message = _("Unable to establish a secure connection to host.");

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, message, 1);

    return (-1);
  }

#  elif defined(HAVE_GNUTLS)
  credentials = (gnutls_certificate_client_credentials *)
                    malloc(sizeof(gnutls_certificate_client_credentials));
  if (credentials == NULL)
  {
    DEBUG_printf(("8http_setup_ssl: Unable to allocate credentials: %s",
                  strerror(errno)));
    http->error  = errno;
    http->status = HTTP_STATUS_ERROR;
    _cupsSetHTTPError(HTTP_STATUS_ERROR);

    return (-1);
  }

  gnutls_certificate_allocate_credentials(credentials);

  gnutls_init(&http->tls, GNUTLS_CLIENT);
  gnutls_set_default_priority(http->tls);
  gnutls_server_name_set(http->tls, GNUTLS_NAME_DNS, hostname,
                         strlen(hostname));
  gnutls_credentials_set(http->tls, GNUTLS_CRD_CERTIFICATE, *credentials);
  gnutls_transport_set_ptr(http->tls, (gnutls_transport_ptr)http);
  gnutls_transport_set_pull_function(http->tls, _httpReadGNUTLS);
  gnutls_transport_set_push_function(http->tls, _httpWriteGNUTLS);

  while ((status = gnutls_handshake(http->tls)) != GNUTLS_E_SUCCESS)
  {
    DEBUG_printf(("8http_setup_ssl: gnutls_handshake returned %d (%s)",
                  status, gnutls_strerror(status)));

    if (gnutls_error_is_fatal(status))
    {
      http->error  = EIO;
      http->status = HTTP_STATUS_ERROR;

      _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, gnutls_strerror(status), 0);

      gnutls_deinit(http->tls);
      gnutls_certificate_free_credentials(*credentials);
      free(credentials);
      http->tls = NULL;

      return (-1);
    }
  }

  http->tls_credentials = credentials;

#  elif defined(HAVE_CDSASSL)
  if ((http->tls = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide,
                                    kSSLStreamType)) == NULL)
  {
    DEBUG_puts("4http_setup_ssl: SSLCreateContext failed.");
    http->error  = errno = ENOMEM;
    http->status = HTTP_STATUS_ERROR;
    _cupsSetHTTPError(HTTP_STATUS_ERROR);

    return (-1);
  }

  error = SSLSetConnection(http->tls, http);
  DEBUG_printf(("4http_setup_ssl: SSLSetConnection, error=%d", (int)error));

  if (!error)
  {
    error = SSLSetIOFuncs(http->tls, _httpReadCDSA, _httpWriteCDSA);
    DEBUG_printf(("4http_setup_ssl: SSLSetIOFuncs, error=%d", (int)error));
  }

  if (!error)
  {
    error = SSLSetSessionOption(http->tls, kSSLSessionOptionBreakOnServerAuth,
                                true);
    DEBUG_printf(("4http_setup_ssl: SSLSetSessionOption, error=%d",
                  (int)error));
  }

  if (!error)
  {
    if (cg->client_cert_cb)
    {
      error = SSLSetSessionOption(http->tls,
				  kSSLSessionOptionBreakOnCertRequested, true);
      DEBUG_printf(("4http_setup_ssl: kSSLSessionOptionBreakOnCertRequested, "
                    "error=%d", (int)error));
    }
    else
    {
      error = http_set_credentials(http);
      DEBUG_printf(("4http_setup_ssl: http_set_credentials, error=%d",
                    (int)error));
    }
  }

 /*
  * Let the server know which hostname/domain we are trying to connect to
  * in case it wants to serve up a certificate with a matching common name.
  */

  if (!error)
  {
    error = SSLSetPeerDomainName(http->tls, hostname, strlen(hostname));

    DEBUG_printf(("4http_setup_ssl: SSLSetPeerDomainName, error=%d",
                  (int)error));
  }

  if (!error)
  {
    int done = 0;			/* Are we done yet? */

    while (!error && !done)
    {
      error = SSLHandshake(http->tls);

      DEBUG_printf(("4http_setup_ssl: SSLHandshake returned %d.", (int)error));

      switch (error)
      {
	case noErr :
	    done = 1;
	    break;

	case errSSLWouldBlock :
	    error = noErr;		/* Force a retry */
	    usleep(1000);		/* in 1 millisecond */
	    break;

	case errSSLServerAuthCompleted :
	    error = 0;
	    if (cg->server_cert_cb)
	    {
	      error = httpCopyCredentials(http, &credentials);
	      if (!error)
	      {
		error = (cg->server_cert_cb)(http, http->tls, credentials,
					     cg->server_cert_data);
		httpFreeCredentials(credentials);
	      }

	      DEBUG_printf(("4http_setup_ssl: Server certificate callback "
	                    "returned %d.", (int)error));
	    }
	    break;

	case errSSLClientCertRequested :
	    error = 0;

	    if (cg->client_cert_cb)
	    {
	      names = NULL;
	      if (!(error = SSLCopyDistinguishedNames(http->tls, &dn_array)) &&
		  dn_array)
	      {
		if ((names = cupsArrayNew(NULL, NULL)) != NULL)
		{
		  for (i = 0, count = CFArrayGetCount(dn_array); i < count; i++)
		  {
		    data = (CFDataRef)CFArrayGetValueAtIndex(dn_array, i);

		    if ((credential = malloc(sizeof(*credential))) != NULL)
		    {
		      credential->datalen = CFDataGetLength(data);
		      if ((credential->data = malloc(credential->datalen)))
		      {
			memcpy((void *)credential->data, CFDataGetBytePtr(data),
			       credential->datalen);
			cupsArrayAdd(names, credential);
		      }
		      else
		        free(credential);
		    }
		  }
		}

		CFRelease(dn_array);
	      }

	      if (!error)
	      {
		error = (cg->client_cert_cb)(http, http->tls, names,
					     cg->client_cert_data);

		DEBUG_printf(("4http_setup_ssl: Client certificate callback "
		              "returned %d.", (int)error));
	      }

	      httpFreeCredentials(names);
	    }
	    break;

	case errSSLUnknownRootCert :
	    message = _("Unable to establish a secure connection to host "
	                "(untrusted certificate).");
	    break;

	case errSSLNoRootCert :
	    message = _("Unable to establish a secure connection to host "
	                "(self-signed certificate).");
	    break;

	case errSSLCertExpired :
	    message = _("Unable to establish a secure connection to host "
	                "(expired certificate).");
	    break;

	case errSSLCertNotYetValid :
	    message = _("Unable to establish a secure connection to host "
	                "(certificate not yet valid).");
	    break;

	case errSSLHostNameMismatch :
	    message = _("Unable to establish a secure connection to host "
	                "(host name mismatch).");
	    break;

	case errSSLXCertChainInvalid :
	    message = _("Unable to establish a secure connection to host "
	                "(certificate chain invalid).");
	    break;

	case errSSLConnectionRefused :
	    message = _("Unable to establish a secure connection to host "
	                "(peer dropped connection before responding).");
	    break;

 	default :
	    break;
      }
    }
  }

  if (error)
  {
    http->error  = error;
    http->status = HTTP_STATUS_ERROR;
    errno        = ECONNREFUSED;

    CFRelease(http->tls);
    http->tls = NULL;

   /*
    * If an error string wasn't set by the callbacks use a generic one...
    */

    if (!message)
#ifdef HAVE_CSSMERRORSTRING
      message = cssmErrorString(error);
#else
      message = _("Unable to establish a secure connection to host.");
#endif /* HAVE_CSSMERRORSTRING */

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, message, 1);

    return (-1);
  }

#  elif defined(HAVE_SSPISSL)
  http->tls = _sspiAlloc();

  if (!http->tls)
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);
    return (-1);
  }

  http->tls->sock = http->fd;
  dwSize          = sizeof(username) / sizeof(TCHAR);
  GetUserName(username, &dwSize);
  _sntprintf_s(commonName, sizeof(commonName) / sizeof(TCHAR),
               sizeof(commonName) / sizeof(TCHAR), TEXT("CN=%s"), username);

  if (!_sspiGetCredentials(http->tls_credentials, L"ClientContainer",
                           commonName, FALSE))
  {
    _sspiFree(http->tls_credentials);
    http->tls_credentials = NULL;

    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI,
                  _("Unable to establish a secure connection to host."), 1);

    return (-1);
  }

  _sspiSetAllowsAnyRoot(http->tls_credentials, TRUE);
  _sspiSetAllowsExpiredCerts(http->tls_credentials, TRUE);

  if (!_sspiConnect(http->tls_credentials, hostname))
  {
    _sspiFree(http->tls_credentials);
    http->tls_credentials = NULL;

    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI,
                  _("Unable to establish a secure connection to host."), 1);

    return (-1);
  }
#  endif /* HAVE_CDSASSL */

  return (0);
}


/*
 * 'http_shutdown_ssl()' - Shut down SSL/TLS on a connection.
 */

static void
http_shutdown_ssl(http_t *http)		/* I - Connection to server */
{
#  ifdef HAVE_LIBSSL
  SSL_CTX	*context;		/* Context for encryption */

  context = SSL_get_SSL_CTX(http->tls);

  SSL_shutdown(http->tls);
  SSL_CTX_free(context);
  SSL_free(http->tls);

#  elif defined(HAVE_GNUTLS)
  gnutls_certificate_client_credentials *credentials;
					/* TLS credentials */

  credentials = (gnutls_certificate_client_credentials *)(http->tls_credentials);

  gnutls_bye(http->tls, GNUTLS_SHUT_RDWR);
  gnutls_deinit(http->tls);
  gnutls_certificate_free_credentials(*credentials);
  free(credentials);

#  elif defined(HAVE_CDSASSL)
  while (SSLClose(http->tls) == errSSLWouldBlock)
    usleep(1000);

  CFRelease(http->tls);

  if (http->tls_credentials)
    CFRelease(http->tls_credentials);

#  elif defined(HAVE_SSPISSL)
  _sspiFree(http->tls_credentials);
#  endif /* HAVE_LIBSSL */

  http->tls             = NULL;
  http->tls_credentials = NULL;
}
#endif /* HAVE_SSL */


#ifdef HAVE_SSL
/*
 * 'http_write_ssl()' - Write to a SSL/TLS connection.
 */

static int				/* O - Bytes written */
http_write_ssl(http_t     *http,	/* I - Connection to server */
	       const char *buf,		/* I - Buffer holding data */
	       int        len)		/* I - Length of buffer */
{
  ssize_t	result;			/* Return value */


  DEBUG_printf(("2http_write_ssl(http=%p, buf=%p, len=%d)", http, buf, len));

#  if defined(HAVE_LIBSSL)
  result = SSL_write((SSL *)(http->tls), buf, len);

#  elif defined(HAVE_GNUTLS)
  result = gnutls_record_send(http->tls, buf, len);

  if (result < 0 && !errno)
  {
   /*
    * Convert GNU TLS error to errno value...
    */

    switch (result)
    {
      case GNUTLS_E_INTERRUPTED :
	  errno = EINTR;
	  break;

      case GNUTLS_E_AGAIN :
          errno = EAGAIN;
          break;

      default :
          errno = EPIPE;
          break;
    }

    result = -1;
  }

#  elif defined(HAVE_CDSASSL)
  OSStatus	error;			/* Error info */
  size_t	processed;		/* Number of bytes processed */


  error = SSLWrite(http->tls, buf, len, &processed);

  switch (error)
  {
    case 0 :
	result = (int)processed;
	break;

    case errSSLWouldBlock :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno  = EINTR;
	}
	break;

    case errSSLClosedGraceful :
    default :
	if (processed)
	  result = (int)processed;
	else
	{
	  result = -1;
	  errno  = EPIPE;
	}
	break;
  }
#  elif defined(HAVE_SSPISSL)
  return _sspiWrite((_sspi_struct_t *)http->tls, (void *)buf, len);
#  endif /* HAVE_LIBSSL */

  DEBUG_printf(("3http_write_ssl: Returning %d.", (int)result));

  return ((int)result);
}
#endif /* HAVE_SSL */


/*
 * 'http_tls_pending()' - Return the number of pending TLS-encrypted bytes.
 */

static size_t
http_tls_pending(http_t *http)		/* I - HTTP connection */
{
  if (http->tls && usessl)
  {
#  ifdef HAVE_LIBSSL
    if (SSL_pending(http->tls))
    {
      DEBUG_puts("5_httpWait: Return 1 since there is pending SSL data.");
      return (1);
    }

#  elif defined(HAVE_GNUTLS)
    if (gnutls_record_check_pending(http->tls))
    {
      DEBUG_puts("5_httpWait: Return 1 since there is pending SSL data.");
      return (1);
    }

#  elif defined(HAVE_CDSASSL)
    size_t bytes;			/* Bytes that are available */

    if (!SSLGetBufferedReadSize(http->tls, &bytes) &&
        bytes > 0)
    {
      DEBUG_puts("5_httpWait: Return 1 since there is pending SSL data.");
      return (1);
    }
#  endif /* HAVE_LIBSSL */
}


#if defined(HAVE_SSL) && defined(HAVE_GNUTLS)
/*
 * '_httpReadGNUTLS()' - Read function for the GNU TLS library.
 */

ssize_t					/* O - Number of bytes read or -1 on error */
_httpReadGNUTLS(
    gnutls_transport_ptr ptr,		/* I - Connection to server */
    void                 *data,		/* I - Buffer */
    size_t               length)	/* I - Number of bytes to read */
{
  http_t	*http;			/* HTTP connection */
  ssize_t	bytes;			/* Bytes read */


  DEBUG_printf(("6_httpReadGNUTLS(ptr=%p, data=%p, length=%d)", ptr, data, (int)length));

  http = (http_t *)ptr;

  if (!http->blocking)
  {
   /*
    * Make sure we have data before we read...
    */

    while (!_httpWait(http, http->wait_value, 0))
    {
      if (http->timeout_cb && (*http->timeout_cb)(http, http->timeout_data))
	continue;

      http->error = ETIMEDOUT;
      return (-1);
    }
  }

  bytes = recv(http->fd, data, length, 0);
  DEBUG_printf(("6_httpReadGNUTLS: bytes=%d", (int)bytes));
  return (bytes);
}
#endif /* HAVE_SSL && HAVE_GNUTLS */


#if defined(HAVE_SSL) && defined(HAVE_GNUTLS)
/*
 * '_httpWriteGNUTLS()' - Write function for the GNU TLS library.
 */

ssize_t					/* O - Number of bytes written or -1 on error */
_httpWriteGNUTLS(
    gnutls_transport_ptr ptr,		/* I - Connection to server */
    const void           *data,		/* I - Data buffer */
    size_t               length)	/* I - Number of bytes to write */
{
  ssize_t bytes;			/* Bytes written */


  DEBUG_printf(("6_httpWriteGNUTLS(ptr=%p, data=%p, length=%d)", ptr, data,
                (int)length));
#ifdef DEBUG
  http_debug_hex("_httpWriteGNUTLS", data, (int)length);
#endif /* DEBUG */

  bytes = send(((http_t *)ptr)->fd, data, length, 0);
  DEBUG_printf(("_httpWriteGNUTLS: bytes=%d", (int)bytes));

  return (bytes);
}
#endif /* HAVE_SSL && HAVE_GNUTLS */


/*
 * 'cupsdEndTLS()' - Shutdown a secure session with the client.
 */

int					/* O - 1 on success, 0 on error */
cupsdEndTLS(cupsd_client_t *con)	/* I - Client connection */
{
  int		error;			/* Error code */
  gnutls_certificate_server_credentials *credentials;
					/* TLS credentials */


  credentials = (gnutls_certificate_server_credentials *)
                    (con->http.tls_credentials);

  error = gnutls_bye(con->http.tls, GNUTLS_SHUT_WR);
  switch (error)
  {
    case GNUTLS_E_SUCCESS:
      cupsdLogMessage(CUPSD_LOG_DEBUG,
		      "SSL shutdown successful!");
      break;
    default:
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "SSL shutdown failed: %s", gnutls_strerror(error));
      break;
  }

  gnutls_deinit(con->http.tls);
  con->http.tls = NULL;

  gnutls_certificate_free_credentials(*credentials);
  free(credentials);

  return (1);
}


/*
 * 'cupsdStartTLS()' - Start a secure session with the client.
 */

int					/* O - 1 on success, 0 on error */
cupsdStartTLS(cupsd_client_t *con)	/* I - Client connection */
{
  int		status;			/* Error code */
  gnutls_certificate_server_credentials *credentials;
					/* TLS credentials */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] Encrypting connection.",
                  con->http.fd);

 /*
  * Verify that we have a certificate...
  */

  if (access(ServerKey, 0) || access(ServerCertificate, 0))
  {
   /*
    * Nope, make a self-signed certificate...
    */

    if (!make_certificate(con))
      return (0);
  }

 /*
  * Create the SSL object and perform the SSL handshake...
  */

  credentials = (gnutls_certificate_server_credentials *)
                    malloc(sizeof(gnutls_certificate_server_credentials));
  if (credentials == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to encrypt connection from %s - %s",
                    con->http.hostname, strerror(errno));

    return (0);
  }

  gnutls_certificate_allocate_credentials(credentials);
  gnutls_certificate_set_x509_key_file(*credentials, ServerCertificate,
				       ServerKey, GNUTLS_X509_FMT_PEM);

  gnutls_init(&con->http.tls, GNUTLS_SERVER);
  gnutls_set_default_priority(con->http.tls);

  gnutls_credentials_set(con->http.tls, GNUTLS_CRD_CERTIFICATE, *credentials);
  gnutls_transport_set_ptr(con->http.tls, (gnutls_transport_ptr)HTTP(con));
  gnutls_transport_set_pull_function(con->http.tls, _httpReadGNUTLS);
  gnutls_transport_set_push_function(con->http.tls, _httpWriteGNUTLS);

  while ((status = gnutls_handshake(con->http.tls)) != GNUTLS_E_SUCCESS)
  {
    if (gnutls_error_is_fatal(status))
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to encrypt connection from %s - %s",
                      con->http.hostname, gnutls_strerror(status));

      gnutls_deinit(con->http.tls);
      gnutls_certificate_free_credentials(*credentials);
      con->http.tls = NULL;
      free(credentials);
      return (0);
    }
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "Connection from %s now encrypted.",
                  con->http.hostname);

  con->http.tls_credentials = credentials;
  return (1);
}


/*
 * 'make_certificate()' - Make a self-signed SSL/TLS certificate.
 */

static int				/* O - 1 on success, 0 on failure */
make_certificate(cupsd_client_t *con)	/* I - Client connection */
{
  gnutls_x509_crt	crt;		/* Self-signed certificate */
  gnutls_x509_privkey	key;		/* Encryption key */
  cups_lang_t		*language;	/* Default language info */
  cups_file_t		*fp;		/* Key/cert file */
  unsigned char		buffer[8192];	/* Buffer for x509 data */
  size_t		bytes;		/* Number of bytes of data */
  unsigned char		serial[4];	/* Serial number buffer */
  time_t		curtime;	/* Current time */
  int			result;		/* Result of GNU TLS calls */


 /*
  * Create the encryption key...
  */

  cupsdLogMessage(CUPSD_LOG_INFO, "Generating SSL server key...");

  gnutls_x509_privkey_init(&key);
  gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0);

 /*
  * Save it...
  */

  bytes = sizeof(buffer);

  if ((result = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM,
                                           buffer, &bytes)) < 0)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to export SSL server key - %s",
                    gnutls_strerror(result));
    gnutls_x509_privkey_deinit(key);
    return (0);
  }
  else if ((fp = cupsFileOpen(ServerKey, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);

    cupsdLogMessage(CUPSD_LOG_INFO, "Created SSL server key file \"%s\"...",
		    ServerKey);
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create SSL server key file \"%s\" - %s",
		    ServerKey, strerror(errno));
    gnutls_x509_privkey_deinit(key);
    return (0);
  }

 /*
  * Create the self-signed certificate...
  */

  cupsdLogMessage(CUPSD_LOG_INFO, "Generating self-signed SSL certificate...");

  language  = cupsLangDefault();
  curtime   = time(NULL);
  serial[0] = curtime >> 24;
  serial[1] = curtime >> 16;
  serial[2] = curtime >> 8;
  serial[3] = curtime;

  gnutls_x509_crt_init(&crt);
  if (strlen(language->language) == 5)
    gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COUNTRY_NAME, 0,
                                  language->language + 3, 2);
  else
    gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COUNTRY_NAME, 0,
                                  "US", 2);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_COMMON_NAME, 0,
                                ServerName, strlen(ServerName));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATION_NAME, 0,
                                ServerName, strlen(ServerName));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,
                                0, "Unknown", 7);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0,
                                "Unknown", 7);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_LOCALITY_NAME, 0,
                                "Unknown", 7);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_PKCS9_EMAIL, 0,
                                ServerAdmin, strlen(ServerAdmin));
  gnutls_x509_crt_set_key(crt, key);
  gnutls_x509_crt_set_serial(crt, serial, sizeof(serial));
  gnutls_x509_crt_set_activation_time(crt, curtime);
  gnutls_x509_crt_set_expiration_time(crt, curtime + 10 * 365 * 86400);
  gnutls_x509_crt_set_ca_status(crt, 0);
  gnutls_x509_crt_set_subject_alternative_name(crt, GNUTLS_SAN_DNSNAME,
                                               ServerName);
  gnutls_x509_crt_set_key_purpose_oid(crt, GNUTLS_KP_TLS_WWW_SERVER, 0);
  gnutls_x509_crt_set_key_usage(crt, GNUTLS_KEY_KEY_ENCIPHERMENT);
  gnutls_x509_crt_set_version(crt, 3);

  bytes = sizeof(buffer);
  if (gnutls_x509_crt_get_key_id(crt, 0, buffer, &bytes) >= 0)
    gnutls_x509_crt_set_subject_key_id(crt, buffer, bytes);

  gnutls_x509_crt_sign(crt, crt, key);

 /*
  * Save it...
  */

  bytes = sizeof(buffer);
  if ((result = gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_PEM,
                                       buffer, &bytes)) < 0)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to export SSL server certificate - %s",
		    gnutls_strerror(result));
  else if ((fp = cupsFileOpen(ServerCertificate, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);

    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Created SSL server certificate file \"%s\"...",
		    ServerCertificate);
  }
  else
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create SSL server certificate file \"%s\" - %s",
		    ServerCertificate, strerror(errno));

 /*
  * Cleanup...
  */

  gnutls_x509_crt_deinit(crt);
  gnutls_x509_privkey_deinit(key);

  return (1);
}


/*
 * End of "$Id$".
 */
