/*
 * "$Id$"
 *
 * TLS support code for CUPS using GNU TLS.
 *
 * Copyright 2007-2014 by Apple Inc.
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
 * Local globals...
 */

static int		tls_auto_create = 0;
					/* Auto-create self-signed certs? */
static char		*tls_common_name = NULL;
					/* Default common name */
static char		*tls_keypath = NULL;
					/* Server cert keychain path */
static _cups_mutex_t	tls_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex for keychain/certs */


/*
 * Local functions...
 */

//static int		make_certificate(cupsd_client_t *con);
static ssize_t	http_gnutls_read(gnutls_transport_ptr_t ptr, void *data, size_t length);
static ssize_t	http_gnutls_write(gnutls_transport_ptr_t ptr, const void *data, size_t length);


/*
 * 'cupsMakeServerCredentials()' - Make a self-signed certificate and private key pair.
 *
 * @since CUPS 2.0@
 */

int					/* O - 1 on success, 0 on failure */
cupsMakeServerCredentials(
    const char *path,			/* I - Path to keychain/directory */
    const char *common_name,		/* I - Common name */
    int        num_alt_names,		/* I - Number of subject alternate names */
    const char **alt_names,		/* I - Subject Alternate Names */
    time_t     expiration_date)		/* I - Expiration date */
{
  gnutls_x509_crt_t	crt;		/* Self-signed certificate */
  gnutls_x509_privkey_t	key;		/* Encryption private key */
  char			temp[1024],	/* Temporary directory name */
 			crtfile[1024],	/* Certificate filename */
			keyfile[1024];	/* Private key filename */
  cups_lang_t		*language;	/* Default language info */
  cups_file_t		*fp;		/* Key/cert file */
  unsigned char		buffer[8192];	/* Buffer for x509 data */
  size_t		bytes;		/* Number of bytes of data */
  unsigned char		serial[4];	/* Serial number buffer */
  time_t		curtime;	/* Current time */
  int			result;		/* Result of GNU TLS calls */


  DEBUG_printf(("cupsMakeServerCredentials(path=\"%s\", common_name=\"%s\", num_alt_names=%d, alt_names=%p, expiration_date=%d)", path, common_name, num_alt_names, alt_names, (int)expiration_date));

 /*
  * Filenames...
  */

  if (!path)
  {
    const char *home = getenv("HOME");	/* HOME environment variable */

    if (getuid() && home)
      snprintf(temp, sizeof(temp), "%s/.cups/ssl", home);
    else
      snprintf(temp, sizeof(temp), "%s/ssl", CUPS_SERVERROOT);

    path = temp;

    DEBUG_printf(("1cupsMakeServerCredentials: Using default path \"%s\".", path));
  }

  snprintf(crtfile, sizeof(crtfile), "%s/%s.crt", path, common_name);
  snprintf(keyfile, sizeof(keyfile), "%s/%s.key", path, common_name);

 /*
  * Create the encryption key...
  */

  gnutls_x509_privkey_init(&key);
  gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0);

 /*
  * Save it...
  */

  bytes = sizeof(buffer);

  if ((result = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM,
                                           buffer, &bytes)) < 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(result), 0);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }
  else if ((fp = cupsFileOpen(keyfile, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }

 /*
  * Create the self-signed certificate...
  */

  DEBUG_puts("1cupsMakeServerCredentials: Generating self-signed SSL certificate.");

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
                                common_name, strlen(common_name));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATION_NAME, 0,
                                common_name, strlen(common_name));
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_ORGANIZATIONAL_UNIT_NAME,
                                0, "Unknown", 7);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_STATE_OR_PROVINCE_NAME, 0,
                                "Unknown", 7);
  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_X520_LOCALITY_NAME, 0,
                                "Unknown", 7);
/*  gnutls_x509_crt_set_dn_by_oid(crt, GNUTLS_OID_PKCS9_EMAIL, 0,
                                ServerAdmin, strlen(ServerAdmin));*/
  gnutls_x509_crt_set_key(crt, key);
  gnutls_x509_crt_set_serial(crt, serial, sizeof(serial));
  gnutls_x509_crt_set_activation_time(crt, curtime);
  gnutls_x509_crt_set_expiration_time(crt, curtime + 10 * 365 * 86400);
  gnutls_x509_crt_set_ca_status(crt, 0);
  if (num_alt_names > 0)
    gnutls_x509_crt_set_subject_alternative_name(crt, GNUTLS_SAN_DNSNAME, alt_names[0]);
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
  if ((result = gnutls_x509_crt_export(crt, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(result), 0);
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }
  else if ((fp = cupsFileOpen(crtfile, "w")) != NULL)
  {
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }

 /*
  * Cleanup...
  */

  gnutls_x509_crt_deinit(crt);
  gnutls_x509_privkey_deinit(key);

  return (1);
}


/*
 * 'cupsSetServerCredentials()' - Set the default server credentials.
 *
 * Note: The server credentials are used by all threads in the running process.
 * This function is threadsafe.
 *
 * @since CUPS 2.0@
 */

int					/* O - 1 on success, 0 on failure */
cupsSetServerCredentials(
    const char *path,			/* I - Path to keychain/directory */
    const char *common_name,		/* I - Default common name for server */
    int        auto_create)		/* I - 1 = automatically create self-signed certificates */
{
  char	temp[1024];			/* Default path buffer */


  DEBUG_printf(("cupsSetServerCredentials(path=\"%s\", common_name=\"%s\", auto_create=%d)", path, common_name, auto_create));

  _cupsMutexLock(&tls_mutex);

 /*
  * Free old values...
  */

  if (tls_keypath)
    _cupsStrFree(tls_keypath);

  if (tls_common_name)
    _cupsStrFree(tls_common_name);

 /*
  * Use defaults as needed...
  */

  if (!path)
  {
    const char *home = getenv("HOME");	/* HOME environment variable */

    if (getuid() && home)
      snprintf(temp, sizeof(temp), "%s/.cups/ssl", home);
    else
      snprintf(temp, sizeof(temp), "%s/ssl", CUPS_SERVERROOT);

    path = temp;

    DEBUG_printf(("1cupsSetServerCredentials: Using default path \"%s\".", path));
  }

 /*
  * Save the new values...
  */

  tls_keypath     = _cupsStrAlloc(path);
  tls_auto_create = auto_create;
  tls_common_name = _cupsStrAlloc(common_name);

  _cupsMutexUnlock(&tls_mutex);

  return (1);
}


/*
 * 'httpCopyCredentials()' - Copy the credentials associated with the peer in
 *                           an encrypted connection.
 *
 * @since CUPS 1.5/OS X 10.7@
 */

int					/* O - Status of call (0 = success) */
httpCopyCredentials(
    http_t	 *http,			/* I - Connection to server */
    cups_array_t **credentials)		/* O - Array of credentials */
{
  if (credentials)
    *credentials = NULL;

  if (!http || !http->tls || !credentials)
    return (-1);

  return (0);
}


/*
 * '_httpCreateCredentials()' - Create credentials in the internal format.
 */

http_tls_credentials_t			/* O - Internal credentials */
_httpCreateCredentials(
    cups_array_t *credentials)		/* I - Array of credentials */
{
  (void)credentials;

  return (NULL);
}


/*
 * '_httpFreeCredentials()' - Free internal credentials.
 */

void
_httpFreeCredentials(
    http_tls_credentials_t credentials)	/* I - Internal credentials */
{
  (void)credentials;
}


/*
 * 'http_gnutls_read()' - Read function for the GNU TLS library.
 */

static ssize_t				/* O - Number of bytes read or -1 on error */
http_gnutls_read(
    gnutls_transport_ptr_t ptr,		/* I - Connection to server */
    void                   *data,	/* I - Buffer */
    size_t                 length)	/* I - Number of bytes to read */
{
  http_t	*http;			/* HTTP connection */
  ssize_t	bytes;			/* Bytes read */


  DEBUG_printf(("6http_gnutls_read(ptr=%p, data=%p, length=%d)", ptr, data, (int)length));

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
  DEBUG_printf(("6http_gnutls_read: bytes=%d", (int)bytes));
  return (bytes);
}


/*
 * 'http_gnutls_write()' - Write function for the GNU TLS library.
 */

static ssize_t				/* O - Number of bytes written or -1 on error */
http_gnutls_write(
    gnutls_transport_ptr_t ptr,		/* I - Connection to server */
    const void             *data,	/* I - Data buffer */
    size_t                 length)	/* I - Number of bytes to write */
{
  ssize_t bytes;			/* Bytes written */


  DEBUG_printf(("6http_gnutls_write(ptr=%p, data=%p, length=%d)", ptr, data,
                (int)length));
  bytes = send(((http_t *)ptr)->fd, data, length, 0);
  DEBUG_printf(("http_gnutls_write: bytes=%d", (int)bytes));

  return (bytes);
}


/*
 * '_httpTLSInitialize()' - Initialize the TLS stack.
 */

void
_httpTLSInitialize(void)
{
 /*
  * Initialize GNU TLS...
  */

  gnutls_global_init();
}


/*
 * '_httpTLSPending()' - Return the number of pending TLS-encrypted bytes.
 */

size_t					/* O - Bytes available */
_httpTLSPending(http_t *http)		/* I - HTTP connection */
{
  return (gnutls_record_check_pending(http->tls));
}


/*
 * '_httpTLSRead()' - Read from a SSL/TLS connection.
 */

int					/* O - Bytes read */
_httpTLSRead(http_t *http,		/* I - Connection to server */
	     char   *buf,		/* I - Buffer to store data */
	     int    len)		/* I - Length of buffer */
{
  ssize_t	result;			/* Return value */


  result = gnutls_record_recv(http->tls, buf, (size_t)len);

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
}


/*
 * '_httpTLSSetCredentials()' - Set the TLS credentials.
 */

int					/* O - Status of connection */
_httpTLSSetCredentials(http_t *http)	/* I - Connection to server */
{
  (void)http;

  return (0);
}


/*
 * '_httpTLSStart()' - Set up SSL/TLS support on a connection.
 */

int					/* O - 0 on success, -1 on failure */
_httpTLSStart(http_t *http)		/* I - Connection to server */
{
  char			hostname[256],	/* Hostname */
			*hostptr;	/* Pointer into hostname */
  int			status;		/* Status of handshake */
  gnutls_certificate_credentials_t *credentials;
					/* TLS credentials */


  DEBUG_printf(("7_httpTLSStart(http=%p)", http));

  if (http->mode == _HTTP_MODE_SERVER && !tls_keypath)
  {
    DEBUG_puts("4_httpTLSStart: cupsSetServerCredentials not called.");
    http->error  = errno = EINVAL;
    http->status = HTTP_STATUS_ERROR;
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Server credentials not set."), 1);

    return (-1);
  }

  credentials = (gnutls_certificate_credentials_t *)
                    malloc(sizeof(gnutls_certificate_credentials_t));
  if (credentials == NULL)
  {
    DEBUG_printf(("8_httpStartTLS: Unable to allocate credentials: %s",
                  strerror(errno)));
    http->error  = errno;
    http->status = HTTP_STATUS_ERROR;
    _cupsSetHTTPError(HTTP_STATUS_ERROR);

    return (-1);
  }

  gnutls_certificate_allocate_credentials(credentials);
  gnutls_init(&http->tls, http->mode == _HTTP_MODE_CLIENT ? GNUTLS_CLIENT : GNUTLS_SERVER);
  gnutls_set_default_priority(http->tls);
  gnutls_transport_set_ptr(http->tls, (gnutls_transport_ptr_t)http);
  gnutls_transport_set_pull_function(http->tls, http_gnutls_read);
  gnutls_transport_set_push_function(http->tls, http_gnutls_write);

  if (http->mode == _HTTP_MODE_CLIENT)
  {
   /*
    * Client: get the hostname to use for TLS...
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

    gnutls_server_name_set(http->tls, GNUTLS_NAME_DNS, hostname, strlen(hostname));
    gnutls_credentials_set(http->tls, GNUTLS_CRD_CERTIFICATE, *credentials);
  }
  else
  {
   /*
    * Server: get certificate and private key...
    */

    char	crtfile[1024],		/* Certificate file */
		keyfile[1024];		/* Private key file */
    int		have_creds = 0;		/* Have credentials? */


    if (http->fields[HTTP_FIELD_HOST][0])
    {
     /*
      * Use hostname for TLS upgrade...
      */

      strlcpy(hostname, http->fields[HTTP_FIELD_HOST], sizeof(hostname));
    }
    else
    {
     /*
      * Resolve hostname from connection address...
      */

      http_addr_t	addr;		/* Connection address */
      socklen_t		addrlen;	/* Length of address */

      addrlen = sizeof(addr);
      if (getsockname(http->fd, (struct sockaddr *)&addr, &addrlen))
      {
	DEBUG_printf(("4_httpTLSStart: Unable to get socket address: %s", strerror(errno)));
	hostname[0] = '\0';
      }
      else if (httpAddrLocalhost(&addr))
	hostname[0] = '\0';
      else
      {
	httpAddrLookup(&addr, hostname, sizeof(hostname));
        DEBUG_printf(("4_httpTLSStart: Resolved socket address to \"%s\".", hostname));
      }
    }

    if (isdigit(hostname[0] & 255) || hostname[0] == '[')
      hostname[0] = '\0';		/* Don't allow numeric addresses */

    if (hostname[0])
    {
      snprintf(crtfile, sizeof(crtfile), "%s/%s.crt", tls_keypath, hostname);
      snprintf(keyfile, sizeof(keyfile), "%s/%s.key", tls_keypath, hostname);

      have_creds = !access(crtfile, 0) && !access(keyfile, 0);
    }
    else if (tls_common_name)
    {
      snprintf(crtfile, sizeof(crtfile), "%s/%s.crt", tls_keypath, tls_common_name);
      snprintf(keyfile, sizeof(keyfile), "%s/%s.key", tls_keypath, tls_common_name);

      have_creds = !access(crtfile, 0) && !access(keyfile, 0);
    }

    if (!have_creds && tls_auto_create && (hostname[0] || tls_common_name))
    {
      DEBUG_printf(("4_httpTLSStart: Auto-create credentials for \"%s\".", hostname[0] ? hostname : tls_common_name));

      if (!cupsMakeServerCredentials(tls_keypath, hostname[0] ? hostname : tls_common_name, 0, NULL, time(NULL) + 365 * 86400))
      {
	DEBUG_puts("4_httpTLSStart: cupsMakeServerCredentials failed.");
	http->error  = errno = EINVAL;
	http->status = HTTP_STATUS_ERROR;
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to create server credentials."), 1);

	return (-1);
      }
    }

    gnutls_certificate_set_x509_key_file(*credentials, crtfile, keyfile, GNUTLS_X509_FMT_PEM);
  }

  while ((status = gnutls_handshake(http->tls)) != GNUTLS_E_SUCCESS)
  {
    DEBUG_printf(("8_httpStartTLS: gnutls_handshake returned %d (%s)",
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

  // TODO: Put this in the right place; no-op for now, this to get things to compile
//  http_tls_set_credentials(http);

  return (0);
}


/*
 * '_httpTLSStop()' - Shut down SSL/TLS on a connection.
 */

void
_httpTLSStop(http_t *http)		/* I - Connection to server */
{
  int	error;				/* Error code */


  error = gnutls_bye(http->tls, http->mode == _HTTP_MODE_CLIENT ? GNUTLS_SHUT_RDWR : GNUTLS_SHUT_WR);
  if (error != GNUTLS_E_SUCCESS)
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(errno), 0);

  gnutls_deinit(http->tls);
  http->tls = NULL;

  gnutls_certificate_free_credentials(*(http->tls_credentials));
  free(http->tls_credentials);
  http->tls_credentials = NULL;
}


/*
 * '_httpTLSWrite()' - Write to a SSL/TLS connection.
 */

int					/* O - Bytes written */
_httpTLSWrite(http_t     *http,		/* I - Connection to server */
	      const char *buf,		/* I - Buffer holding data */
	      int        len)		/* I - Length of buffer */
{
  ssize_t	result;			/* Return value */


  DEBUG_printf(("2http_write_ssl(http=%p, buf=%p, len=%d)", http, buf, len));

  result = gnutls_record_send(http->tls, buf, (size_t)len);

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

  DEBUG_printf(("3http_write_ssl: Returning %d.", (int)result));

  return ((int)result);
}


#if 0
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
  gnutls_certificate_credentials_t *credentials;
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

  credentials = (gnutls_certificate_credentials_t *)
                    malloc(sizeof(gnutls_certificate_credentials_t));
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
  gnutls_transport_set_ptr(con->http.tls, (gnutls_transport_ptr_t)HTTP(con));
  gnutls_transport_set_pull_function(con->http.tls, http_gnutls_read);
  gnutls_transport_set_push_function(con->http.tls, http_gnutls_write);

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
#endif /* 0 */


/*
 * End of "$Id$".
 */
