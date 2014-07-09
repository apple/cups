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
 * Include necessary headers...
 */

#include <sys/stat.h>


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

static const char	*http_gnutls_default_path(char *buffer, size_t bufsize);
static ssize_t		http_gnutls_read(gnutls_transport_ptr_t ptr, void *data, size_t length);
static ssize_t		http_gnutls_write(gnutls_transport_ptr_t ptr, const void *data, size_t length);


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
    path = http_gnutls_default_path(temp, sizeof(temp));

  if (!path || !common_name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (0);
  }
    
  snprintf(crtfile, sizeof(crtfile), "%s/%s.crt", path, common_name);
  snprintf(keyfile, sizeof(keyfile), "%s/%s.key", path, common_name);

 /*
  * Create the encryption key...
  */

  DEBUG_puts("1cupsMakeServerCredentials: Creating key pair.");

  gnutls_x509_privkey_init(&key);
  gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0);

  DEBUG_puts("1cupsMakeServerCredentials: Key pair created.");

 /*
  * Save it...
  */

  bytes = sizeof(buffer);

  if ((result = gnutls_x509_privkey_export(key, GNUTLS_X509_FMT_PEM, buffer, &bytes)) < 0)
  {
    DEBUG_printf(("1cupsMakeServerCredentials: Unable to export private key: %s", gnutls_strerror(result)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(result), 0);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }
  else if ((fp = cupsFileOpen(keyfile, "w")) != NULL)
  {
    DEBUG_printf(("1cupsMakeServerCredentials: Writing private key to \"%s\".", keyfile));
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    DEBUG_printf(("1cupsMakeServerCredentials: Unable to create private key file \"%s\": %s", keyfile, strerror(errno)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(errno), 0);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }

 /*
  * Create the self-signed certificate...
  */

  DEBUG_puts("1cupsMakeServerCredentials: Generating self-signed X.509 certificate.");

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
    DEBUG_printf(("1cupsMakeServerCredentials: Unable to export public key and X.509 certificate: %s", gnutls_strerror(result)));
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, gnutls_strerror(result), 0);
    gnutls_x509_crt_deinit(crt);
    gnutls_x509_privkey_deinit(key);
    return (0);
  }
  else if ((fp = cupsFileOpen(crtfile, "w")) != NULL)
  {
    DEBUG_printf(("1cupsMakeServerCredentials: Writing public key and X.509 certificate to \"%s\".", crtfile));
    cupsFileWrite(fp, (char *)buffer, bytes);
    cupsFileClose(fp);
  }
  else
  {
    DEBUG_printf(("1cupsMakeServerCredentials: Unable to create public key and X.509 certificate file \"%s\": %s", crtfile, strerror(errno)));
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

  DEBUG_puts("1cupsMakeServerCredentials: Successfully created credentials.");

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

 /*
  * Use defaults as needed...
  */

  if (!path)
    path = http_gnutls_default_path(temp, sizeof(temp));

 /*
  * Range check input...
  */

  if (!path || !common_name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, strerror(EINVAL), 0);
    return (0);
  }

  _cupsMutexLock(&tls_mutex);

 /*
  * Free old values...
  */

  if (tls_keypath)
    _cupsStrFree(tls_keypath);

  if (tls_common_name)
    _cupsStrFree(tls_common_name);

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
 * 'httpCredentialsAreValidForName()' - Return whether the credentials are valid for the given name.
 *
 * @since CUPS 2.0@
 */

int					/* O - 1 if valid, 0 otherwise */
httpCredentialsAreValidForName(
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Name to check */
{
#if 0
  char			cert_name[256];	/* Certificate's common name (C string) */
  int			valid = 1;	/* Valid name? */


  if ((secCert = http_cdsa_create_credential((http_credential_t *)cupsArrayFirst(credentials))) == NULL)
    return (0);

 /*
  * Compare the common names...
  */

  if ((cfcert_name = SecCertificateCopySubjectSummary(secCert)) == NULL)
  {
   /*
    * Can't get common name, cannot be valid...
    */

    valid = 0;
  }
  else if (CFStringGetCString(cfcert_name, cert_name, sizeof(cert_name), kCFStringEncodingUTF8) &&
           _cups_strcasecmp(common_name, cert_name))
  {
   /*
    * Not an exact match for the common name, check for wildcard certs...
    */

    const char	*domain = strchr(common_name, '.');
					/* Domain in common name */

    if (strncmp(cert_name, "*.", 2) || !domain || _cups_strcasecmp(domain, cert_name + 1))
    {
     /*
      * Not a wildcard match.
      */

      /* TODO: Check subject alternate names */
      valid = 0;
    }
  }

  if (cfcert_name)
    CFRelease(cfcert_name);

  CFRelease(secCert);

  return (valid);
#else
  return (1);
#endif /* 0 */
}


/*
 * 'httpCredentialsGetTrust()' - Return the trust of credentials.
 *
 * @since CUPS 2.0@
 */

http_trust_t				/* O - Level of trust */
httpCredentialsGetTrust(
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Common name for trust lookup */
{
  http_trust_t		trust = HTTP_TRUST_OK;
					/* Trusted? */

#if 0
  cups_array_t		*tcreds = NULL;	/* Trusted credentials */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Per-thread globals */
#endif /* 0 */


  if (!common_name)
    return (HTTP_TRUST_UNKNOWN);

#if 0
  if ((secCert = http_cdsa_create_credential((http_credential_t *)cupsArrayFirst(credentials))) == NULL)
    return (HTTP_TRUST_UNKNOWN);

 /*
  * Look this common name up in the default keychains...
  */

  httpLoadCredentials(NULL, &tcreds, common_name);

  if (tcreds)
  {
    char	credentials_str[1024],	/* String for incoming credentials */
		tcreds_str[1024];	/* String for saved credentials */

    httpCredentialsString(credentials, credentials_str, sizeof(credentials_str));
    httpCredentialsString(tcreds, tcreds_str, sizeof(tcreds_str));

    if (strcmp(credentials_str, tcreds_str))
    {
     /*
      * Credentials don't match, let's look at the expiration date of the new
      * credentials and allow if the new ones have a later expiration...
      */

      if (httpCredentialsGetExpiration(credentials) <= httpCredentialsGetExpiration(tcreds) ||
          !httpCredentialsAreValidForName(credentials, common_name))
      {
       /*
        * Either the new credentials are not newly issued, or the common name
	* does not match the issued certificate...
	*/

        trust = HTTP_TRUST_INVALID;
      }
      else if (httpCredentialsGetExpiration(tcreds) < time(NULL))
      {
       /*
        * Save the renewed credentials...
	*/

	trust = HTTP_TRUST_RENEWED;

        httpSaveCredentials(NULL, credentials, common_name);
      }
    }

    httpFreeCredentials(tcreds);
  }
  else if (cg->validate_certs && !httpCredentialsAreValidForName(credentials, common_name))
    trust = HTTP_TRUST_INVALID;

  if (!cg->expired_certs && !SecCertificateIsValid(secCert, CFAbsoluteTimeGetCurrent()))
    trust = HTTP_TRUST_EXPIRED;
  else if (!cg->any_root && cupsArrayCount(credentials) == 1)
    trust = HTTP_TRUST_INVALID;

  CFRelease(secCert);
#endif /* 0 */

  return (trust);
}


/*
 * 'httpCredentialsGetExpiration()' - Return the expiration date of the credentials.
 *
 * @since CUPS 2.0@
 */

time_t					/* O - Expiration date of credentials */
httpCredentialsGetExpiration(
    cups_array_t *credentials)		/* I - Credentials */
{
#if 0
  expiration = (time_t)(SecCertificateNotValidAfter(secCert) + kCFAbsoluteTimeIntervalSince1970);

  return (expiration);
#else
  return (INT_MAX);
#endif /* 0 */
}


/*
 * 'httpCredentialsString()' - Return a string representing the credentials.
 *
 * @since CUPS 2.0@
 */

size_t					/* O - Total size of credentials string */
httpCredentialsString(
    cups_array_t *credentials,		/* I - Credentials */
    char         *buffer,		/* I - Buffer or @code NULL@ */
    size_t       bufsize)		/* I - Size of buffer */
{
  DEBUG_printf(("httpCredentialsString(credentials=%p, buffer=%p, bufsize=" CUPS_LLFMT ")", credentials, buffer, CUPS_LLCAST bufsize));

  if (!buffer)
    return (0);

  if (buffer && bufsize > 0)
    *buffer = '\0';

#if 0
  if ((first = (http_credential_t *)cupsArrayFirst(credentials)) != NULL &&
      (secCert = http_cdsa_create_credential(first)) != NULL)
  {
    CFStringRef		cf_name;	/* CF common name string */
    char		name[256];	/* Common name associated with cert */
    time_t		expiration;	/* Expiration date of cert */
    _cups_md5_state_t	md5_state;	/* MD5 state */
    unsigned char	md5_digest[16];	/* MD5 result */

    if ((cf_name = SecCertificateCopySubjectSummary(secCert)) != NULL)
    {
      CFStringGetCString(cf_name, name, (CFIndex)sizeof(name), kCFStringEncodingUTF8);
      CFRelease(cf_name);
    }
    else
      strlcpy(name, "unknown", sizeof(name));

    expiration = (time_t)(SecCertificateNotValidAfter(secCert) + kCFAbsoluteTimeIntervalSince1970);

    _cupsMD5Init(&md5_state);
    _cupsMD5Append(&md5_state, first->data, (int)first->datalen);
    _cupsMD5Finish(&md5_state, md5_digest);

    snprintf(buffer, bufsize, "%s / %s / %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", name, httpGetDateString(expiration), md5_digest[0], md5_digest[1], md5_digest[2], md5_digest[3], md5_digest[4], md5_digest[5], md5_digest[6], md5_digest[7], md5_digest[8], md5_digest[9], md5_digest[10], md5_digest[11], md5_digest[12], md5_digest[13], md5_digest[14], md5_digest[15]);

    CFRelease(secCert);
  }
#endif // 0

  DEBUG_printf(("1httpCredentialsString: Returning \"%s\".", buffer));

  return (strlen(buffer));
}


/*
 * 'httpLoadCredentials()' - Load X.509 credentials from a keychain file.
 *
 * @since CUPS 2.0@
 */

int					/* O - 0 on success, -1 on error */
httpLoadCredentials(
    const char   *path,			/* I  - Keychain/PKCS#12 path */
    cups_array_t **credentials,		/* IO - Credentials */
    const char   *common_name)		/* I  - Common name for credentials */
{
  (void)path;
  (void)credentials;
  (void)common_name;

  return (-1);
}


/*
 * 'httpSaveCredentials()' - Save X.509 credentials to a keychain file.
 *
 * @since CUPS 2.0@
 */

int					/* O - -1 on error, 0 on success */
httpSaveCredentials(
    const char   *path,			/* I - Keychain/PKCS#12 path */
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Common name for credentials */
{
  (void)path;
  (void)credentials;
  (void)common_name;

  return (-1);
}


/*
 * 'http_gnutls_default_path()' - Get the default credential store path.
 */

static const char *			/* O - Path or NULL on error */
http_gnutls_default_path(char   *buffer,/* I - Path buffer */
                         size_t bufsize)/* I - Size of path buffer */
{
  const char *home = getenv("HOME");	/* HOME environment variable */


  if (getuid() && home)
  {
    snprintf(buffer, bufsize, "%s/.cups", home);
    if (access(buffer, 0))
    {
      DEBUG_printf(("1http_gnutls_default_path: Making directory \"%s\".", buffer));
      if (mkdir(buffer, 0700))
      {
        DEBUG_printf(("1http_gnutls_default_path: Failed to make directory: %s", strerror(errno)));
        return (NULL);
      }
    }

    snprintf(buffer, bufsize, "%s/.cups/ssl", home);
    if (access(buffer, 0))
    {
      DEBUG_printf(("1http_gnutls_default_path: Making directory \"%s\".", buffer));
      if (mkdir(buffer, 0700))
      {
        DEBUG_printf(("1http_gnutls_default_path: Failed to make directory: %s", strerror(errno)));
        return (NULL);
      }
    }
  }
  else
    strlcpy(buffer, CUPS_SERVERROOT "/ssl", bufsize);

  DEBUG_printf(("1http_gnutls_default_path: Using default path \"%s\".", buffer));

  return (buffer);
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
  status = gnutls_init(&http->tls, http->mode == _HTTP_MODE_CLIENT ? GNUTLS_CLIENT : GNUTLS_SERVER);
  if (!status)
    status = gnutls_set_default_priority(http->tls);

  if (status)
  {
    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    DEBUG_printf(("4_httpTLSStart: Unable to initialize common TLS parameters: %s", gnutls_strerror(status)));
    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, gnutls_strerror(status), 0);

    gnutls_deinit(http->tls);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    http->tls = NULL;

    return (-1);
  }

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

    status = gnutls_server_name_set(http->tls, GNUTLS_NAME_DNS, hostname, strlen(hostname));
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

    DEBUG_printf(("4_httpTLSStart: Using certificate \"%s\" and private key \"%s\".", crtfile, keyfile));

    status = gnutls_certificate_set_x509_key_file(*credentials, crtfile, keyfile, GNUTLS_X509_FMT_PEM);
  }

  if (!status)
    status = gnutls_credentials_set(http->tls, GNUTLS_CRD_CERTIFICATE, *credentials);

  if (status)
  {
    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    DEBUG_printf(("4_httpTLSStart: Unable to complete client/server setup: %s", gnutls_strerror(status)));
    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI, gnutls_strerror(status), 0);

    gnutls_deinit(http->tls);
    gnutls_certificate_free_credentials(*credentials);
    free(credentials);
    http->tls = NULL;

    return (-1);
  }

  gnutls_transport_set_ptr(http->tls, (gnutls_transport_ptr_t)http);
  gnutls_transport_set_pull_function(http->tls, http_gnutls_read);
  gnutls_transport_set_pull_timeout_function(http->tls, (gnutls_pull_timeout_func)httpWait);
  gnutls_transport_set_push_function(http->tls, http_gnutls_write);

  while ((status = gnutls_handshake(http->tls)) != GNUTLS_E_SUCCESS)
  {
    DEBUG_printf(("5_httpStartTLS: gnutls_handshake returned %d (%s)",
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

  if (http->tls_credentials)
  {
    gnutls_certificate_free_credentials(*(http->tls_credentials));
    free(http->tls_credentials);
    http->tls_credentials = NULL;
  }
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


/*
 * End of "$Id$".
 */
