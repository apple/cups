/*
 * TLS support code for CUPS using GNU TLS.
 *
 * Copyright © 2007-2018 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/**** This file is included from tls.c ****/

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
static gnutls_x509_crl_t tls_crl = NULL;/* Certificate revocation list */
static char		*tls_keypath = NULL;
					/* Server cert keychain path */
static _cups_mutex_t	tls_mutex = _CUPS_MUTEX_INITIALIZER;
					/* Mutex for keychain/certs */
static int		tls_options = -1,/* Options for TLS connections */
			tls_min_version = _HTTP_TLS_1_0,
			tls_max_version = _HTTP_TLS_MAX;


/*
 * Local functions...
 */

static gnutls_x509_crt_t http_gnutls_create_credential(http_credential_t *credential);
static const char	*http_gnutls_default_path(char *buffer, size_t bufsize);
static void		http_gnutls_load_crl(void);
static const char	*http_gnutls_make_path(char *buffer, size_t bufsize, const char *dirname, const char *filename, const char *ext);
static ssize_t		http_gnutls_read(gnutls_transport_ptr_t ptr, void *data, size_t length);
static ssize_t		http_gnutls_write(gnutls_transport_ptr_t ptr, const void *data, size_t length);


/*
 * 'cupsMakeServerCredentials()' - Make a self-signed certificate and private key pair.
 *
 * @since CUPS 2.0/OS 10.10@
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

  http_gnutls_make_path(crtfile, sizeof(crtfile), path, common_name, "crt");
  http_gnutls_make_path(keyfile, sizeof(keyfile), path, common_name, "key");

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
 * @since CUPS 2.0/OS 10.10@
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
 * @since CUPS 1.5/macOS 10.7@
 */

int					/* O - Status of call (0 = success) */
httpCopyCredentials(
    http_t	 *http,			/* I - Connection to server */
    cups_array_t **credentials)		/* O - Array of credentials */
{
  unsigned		count;		/* Number of certificates */
  const gnutls_datum_t *certs;		/* Certificates */


  DEBUG_printf(("httpCopyCredentials(http=%p, credentials=%p)", http, credentials));

  if (credentials)
    *credentials = NULL;

  if (!http || !http->tls || !credentials)
    return (-1);

  *credentials = cupsArrayNew(NULL, NULL);
  certs        = gnutls_certificate_get_peers(http->tls, &count);

  DEBUG_printf(("1httpCopyCredentials: certs=%p, count=%u", certs, count));

  if (certs && count)
  {
    while (count > 0)
    {
      httpAddCredential(*credentials, certs->data, certs->size);
      certs ++;
      count --;
    }
  }

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
 * @since CUPS 2.0/OS 10.10@
 */

int					/* O - 1 if valid, 0 otherwise */
httpCredentialsAreValidForName(
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Name to check */
{
  gnutls_x509_crt_t	cert;		/* Certificate */
  int			result = 0;	/* Result */


  cert = http_gnutls_create_credential((http_credential_t *)cupsArrayFirst(credentials));
  if (cert)
  {
    result = gnutls_x509_crt_check_hostname(cert, common_name) != 0;

    if (result)
    {
      int		i,		/* Looping var */
			count;		/* Number of revoked certificates */
      unsigned char	cserial[1024],	/* Certificate serial number */
			rserial[1024];	/* Revoked serial number */
      size_t		cserial_size,	/* Size of cert serial number */
			rserial_size;	/* Size of revoked serial number */

      _cupsMutexLock(&tls_mutex);

      count = gnutls_x509_crl_get_crt_count(tls_crl);

      if (count > 0)
      {
        cserial_size = sizeof(cserial);
        gnutls_x509_crt_get_serial(cert, cserial, &cserial_size);

        for (i = 0; i < count; i ++)
	{
	  rserial_size = sizeof(rserial);
          if (!gnutls_x509_crl_get_crt_serial(tls_crl, (unsigned)i, rserial, &rserial_size, NULL) && cserial_size == rserial_size && !memcmp(cserial, rserial, rserial_size))
	  {
	    result = 0;
	    break;
	  }
	}
      }

      _cupsMutexUnlock(&tls_mutex);
    }

    gnutls_x509_crt_deinit(cert);
  }

  return (result);
}


/*
 * 'httpCredentialsGetTrust()' - Return the trust of credentials.
 *
 * @since CUPS 2.0/OS 10.10@
 */

http_trust_t				/* O - Level of trust */
httpCredentialsGetTrust(
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Common name for trust lookup */
{
  http_trust_t		trust = HTTP_TRUST_OK;
					/* Trusted? */
  gnutls_x509_crt_t	cert;		/* Certificate */
  cups_array_t		*tcreds = NULL;	/* Trusted credentials */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Per-thread globals */


  if (!common_name)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No common name specified."), 1);
    return (HTTP_TRUST_UNKNOWN);
  }

  if ((cert = http_gnutls_create_credential((http_credential_t *)cupsArrayFirst(credentials))) == NULL)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Unable to create credentials from array."), 1);
    return (HTTP_TRUST_UNKNOWN);
  }

  if (cg->any_root < 0)
  {
    _cupsSetDefaults();
    http_gnutls_load_crl();
  }

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

      if (!cg->trust_first)
      {
       /*
        * Do not trust certificates on first use...
	*/

        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Trust on first use is disabled."), 1);

        trust = HTTP_TRUST_INVALID;
      }
      else if (httpCredentialsGetExpiration(credentials) <= httpCredentialsGetExpiration(tcreds))
      {
       /*
        * The new credentials are not newly issued...
	*/

        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("New credentials are older than stored credentials."), 1);

        trust = HTTP_TRUST_INVALID;
      }
      else if (!httpCredentialsAreValidForName(credentials, common_name))
      {
       /*
        * The common name does not match the issued certificate...
	*/

        _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("New credentials are not valid for name."), 1);

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
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("No stored credentials, not valid for name."), 1);
    trust = HTTP_TRUST_INVALID;
  }
  else if (!cg->trust_first)
  {
   /*
    * See if we have a site CA certificate we can compare...
    */

    if (!httpLoadCredentials(NULL, &tcreds, "site"))
    {
      if (cupsArrayCount(credentials) != (cupsArrayCount(tcreds) + 1))
      {
       /*
        * Certificate isn't directly generated from the CA cert...
	*/

        trust = HTTP_TRUST_INVALID;
      }
      else
      {
       /*
        * Do a tail comparison of the two certificates...
	*/

        http_credential_t	*a, *b;		/* Certificates */

        for (a = (http_credential_t *)cupsArrayFirst(tcreds), b = (http_credential_t *)cupsArrayIndex(credentials, 1);
	     a && b;
	     a = (http_credential_t *)cupsArrayNext(tcreds), b = (http_credential_t *)cupsArrayNext(credentials))
	  if (a->datalen != b->datalen || memcmp(a->data, b->data, a->datalen))
	    break;

        if (a || b)
	  trust = HTTP_TRUST_INVALID;
      }

      if (trust != HTTP_TRUST_OK)
	_cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Credentials do not validate against site CA certificate."), 1);
    }
    else
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Trust on first use is disabled."), 1);
      trust = HTTP_TRUST_INVALID;
    }
  }

  if (trust == HTTP_TRUST_OK && !cg->expired_certs)
  {
    time_t	curtime;		/* Current date/time */

    time(&curtime);
    if (curtime < gnutls_x509_crt_get_activation_time(cert) ||
        curtime > gnutls_x509_crt_get_expiration_time(cert))
    {
      _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Credentials have expired."), 1);
      trust = HTTP_TRUST_EXPIRED;
    }
  }

  if (trust == HTTP_TRUST_OK && !cg->any_root && cupsArrayCount(credentials) == 1)
  {
    _cupsSetError(IPP_STATUS_ERROR_INTERNAL, _("Self-signed credentials are blocked."), 1);
    trust = HTTP_TRUST_INVALID;
  }

  gnutls_x509_crt_deinit(cert);

  return (trust);
}


/*
 * 'httpCredentialsGetExpiration()' - Return the expiration date of the credentials.
 *
 * @since CUPS 2.0/OS 10.10@
 */

time_t					/* O - Expiration date of credentials */
httpCredentialsGetExpiration(
    cups_array_t *credentials)		/* I - Credentials */
{
  gnutls_x509_crt_t	cert;		/* Certificate */
  time_t		result = 0;	/* Result */


  cert = http_gnutls_create_credential((http_credential_t *)cupsArrayFirst(credentials));
  if (cert)
  {
    result = gnutls_x509_crt_get_expiration_time(cert);
    gnutls_x509_crt_deinit(cert);
  }

  return (result);
}


/*
 * 'httpCredentialsString()' - Return a string representing the credentials.
 *
 * @since CUPS 2.0/OS 10.10@
 */

size_t					/* O - Total size of credentials string */
httpCredentialsString(
    cups_array_t *credentials,		/* I - Credentials */
    char         *buffer,		/* I - Buffer or @code NULL@ */
    size_t       bufsize)		/* I - Size of buffer */
{
  http_credential_t	*first;		/* First certificate */
  gnutls_x509_crt_t	cert;		/* Certificate */


  DEBUG_printf(("httpCredentialsString(credentials=%p, buffer=%p, bufsize=" CUPS_LLFMT ")", credentials, buffer, CUPS_LLCAST bufsize));

  if (!buffer)
    return (0);

  if (buffer && bufsize > 0)
    *buffer = '\0';

  if ((first = (http_credential_t *)cupsArrayFirst(credentials)) != NULL &&
      (cert = http_gnutls_create_credential(first)) != NULL)
  {
    char		name[256];	/* Common name associated with cert */
    size_t		namelen;	/* Length of name */
    time_t		expiration;	/* Expiration date of cert */
    unsigned char	md5_digest[16];	/* MD5 result */

    namelen = sizeof(name) - 1;
    if (gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0, name, &namelen) >= 0)
      name[namelen] = '\0';
    else
      strlcpy(name, "unknown", sizeof(name));

    expiration = gnutls_x509_crt_get_expiration_time(cert);

    cupsHashData("md5", first->data, first->datalen, md5_digest, sizeof(md5_digest));

    snprintf(buffer, bufsize, "%s / %s / %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", name, httpGetDateString(expiration), md5_digest[0], md5_digest[1], md5_digest[2], md5_digest[3], md5_digest[4], md5_digest[5], md5_digest[6], md5_digest[7], md5_digest[8], md5_digest[9], md5_digest[10], md5_digest[11], md5_digest[12], md5_digest[13], md5_digest[14], md5_digest[15]);

    gnutls_x509_crt_deinit(cert);
  }

  DEBUG_printf(("1httpCredentialsString: Returning \"%s\".", buffer));

  return (strlen(buffer));
}


/*
 * 'httpLoadCredentials()' - Load X.509 credentials from a keychain file.
 *
 * @since CUPS 2.0/OS 10.10@
 */

int					/* O - 0 on success, -1 on error */
httpLoadCredentials(
    const char   *path,			/* I  - Keychain/PKCS#12 path */
    cups_array_t **credentials,		/* IO - Credentials */
    const char   *common_name)		/* I  - Common name for credentials */
{
  cups_file_t		*fp;		/* Certificate file */
  char			filename[1024],	/* filename.crt */
			temp[1024],	/* Temporary string */
			line[256];	/* Base64-encoded line */
  unsigned char		*data = NULL;	/* Buffer for cert data */
  size_t		alloc_data = 0,	/* Bytes allocated */
			num_data = 0;	/* Bytes used */
  int			decoded;	/* Bytes decoded */
  int			in_certificate = 0;
					/* In a certificate? */


  if (!credentials || !common_name)
    return (-1);

  if (!path)
    path = http_gnutls_default_path(temp, sizeof(temp));
  if (!path)
    return (-1);

  http_gnutls_make_path(filename, sizeof(filename), path, common_name, "crt");

  if ((fp = cupsFileOpen(filename, "r")) == NULL)
    return (-1);

  while (cupsFileGets(fp, line, sizeof(line)))
  {
    if (!strcmp(line, "-----BEGIN CERTIFICATE-----"))
    {
      if (in_certificate)
      {
       /*
	* Missing END CERTIFICATE...
	*/

        httpFreeCredentials(*credentials);
	*credentials = NULL;
        break;
      }

      in_certificate = 1;
    }
    else if (!strcmp(line, "-----END CERTIFICATE-----"))
    {
      if (!in_certificate || !num_data)
      {
       /*
	* Missing data...
	*/

        httpFreeCredentials(*credentials);
	*credentials = NULL;
        break;
      }

      if (!*credentials)
        *credentials = cupsArrayNew(NULL, NULL);

      if (httpAddCredential(*credentials, data, num_data))
      {
        httpFreeCredentials(*credentials);
	*credentials = NULL;
        break;
      }

      num_data       = 0;
      in_certificate = 0;
    }
    else if (in_certificate)
    {
      if (alloc_data == 0)
      {
        data       = malloc(2048);
	alloc_data = 2048;

        if (!data)
	  break;
      }
      else if ((num_data + strlen(line)) >= alloc_data)
      {
        unsigned char *tdata = realloc(data, alloc_data + 1024);
					/* Expanded buffer */

	if (!tdata)
	{
	  httpFreeCredentials(*credentials);
	  *credentials = NULL;
	  break;
	}

	data       = tdata;
        alloc_data += 1024;
      }

      decoded = alloc_data - num_data;
      httpDecode64_2((char *)data + num_data, &decoded, line);
      num_data += (size_t)decoded;
    }
  }

  cupsFileClose(fp);

  if (in_certificate)
  {
   /*
    * Missing END CERTIFICATE...
    */

    httpFreeCredentials(*credentials);
    *credentials = NULL;
  }

  if (data)
    free(data);

  return (*credentials ? 0 : -1);
}


/*
 * 'httpSaveCredentials()' - Save X.509 credentials to a keychain file.
 *
 * @since CUPS 2.0/OS 10.10@
 */

int					/* O - -1 on error, 0 on success */
httpSaveCredentials(
    const char   *path,			/* I - Keychain/PKCS#12 path */
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Common name for credentials */
{
  cups_file_t		*fp;		/* Certificate file */
  char			filename[1024],	/* filename.crt */
			nfilename[1024],/* filename.crt.N */
			temp[1024],	/* Temporary string */
			line[256];	/* Base64-encoded line */
  const unsigned char	*ptr;		/* Pointer into certificate */
  ssize_t		remaining;	/* Bytes left */
  http_credential_t	*cred;		/* Current credential */


  if (!credentials || !common_name)
    return (-1);

  if (!path)
    path = http_gnutls_default_path(temp, sizeof(temp));
  if (!path)
    return (-1);

  http_gnutls_make_path(filename, sizeof(filename), path, common_name, "crt");
  snprintf(nfilename, sizeof(nfilename), "%s.N", filename);

  if ((fp = cupsFileOpen(nfilename, "w")) == NULL)
    return (-1);

  fchmod(cupsFileNumber(fp), 0600);

  for (cred = (http_credential_t *)cupsArrayFirst(credentials);
       cred;
       cred = (http_credential_t *)cupsArrayNext(credentials))
  {
    cupsFilePuts(fp, "-----BEGIN CERTIFICATE-----\n");
    for (ptr = cred->data, remaining = (ssize_t)cred->datalen; remaining > 0; remaining -= 45, ptr += 45)
    {
      httpEncode64_2(line, sizeof(line), (char *)ptr, remaining > 45 ? 45 : remaining);
      cupsFilePrintf(fp, "%s\n", line);
    }
    cupsFilePuts(fp, "-----END CERTIFICATE-----\n");
  }

  cupsFileClose(fp);

  return (rename(nfilename, filename));
}


/*
 * 'http_gnutls_create_credential()' - Create a single credential in the internal format.
 */

static gnutls_x509_crt_t			/* O - Certificate */
http_gnutls_create_credential(
    http_credential_t *credential)		/* I - Credential */
{
  int			result;			/* Result from GNU TLS */
  gnutls_x509_crt_t	cert;			/* Certificate */
  gnutls_datum_t	datum;			/* Data record */


  DEBUG_printf(("3http_gnutls_create_credential(credential=%p)", credential));

  if (!credential)
    return (NULL);

  if ((result = gnutls_x509_crt_init(&cert)) < 0)
  {
    DEBUG_printf(("4http_gnutls_create_credential: init error: %s", gnutls_strerror(result)));
    return (NULL);
  }

  datum.data = credential->data;
  datum.size = credential->datalen;

  if ((result = gnutls_x509_crt_import(cert, &datum, GNUTLS_X509_FMT_DER)) < 0)
  {
    DEBUG_printf(("4http_gnutls_create_credential: import error: %s", gnutls_strerror(result)));

    gnutls_x509_crt_deinit(cert);
    return (NULL);
  }

  return (cert);
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
 * 'http_gnutls_load_crl()' - Load the certificate revocation list, if any.
 */

static void
http_gnutls_load_crl(void)
{
  _cupsMutexLock(&tls_mutex);

  if (!gnutls_x509_crl_init(&tls_crl))
  {
    cups_file_t		*fp;		/* CRL file */
    char		filename[1024],	/* site.crl */
			line[256];	/* Base64-encoded line */
    unsigned char	*data = NULL;	/* Buffer for cert data */
    size_t		alloc_data = 0,	/* Bytes allocated */
			num_data = 0;	/* Bytes used */
    int			decoded;	/* Bytes decoded */
    gnutls_datum_t	datum;		/* Data record */


    http_gnutls_make_path(filename, sizeof(filename), CUPS_SERVERROOT, "site", "crl");

    if ((fp = cupsFileOpen(filename, "r")) != NULL)
    {
      while (cupsFileGets(fp, line, sizeof(line)))
      {
	if (!strcmp(line, "-----BEGIN X509 CRL-----"))
	{
	  if (num_data)
	  {
	   /*
	    * Missing END X509 CRL...
	    */

	    break;
	  }
	}
	else if (!strcmp(line, "-----END X509 CRL-----"))
	{
	  if (!num_data)
	  {
	   /*
	    * Missing data...
	    */

	    break;
	  }

          datum.data = data;
	  datum.size = num_data;

	  gnutls_x509_crl_import(tls_crl, &datum, GNUTLS_X509_FMT_PEM);

	  num_data = 0;
	}
	else
	{
	  if (alloc_data == 0)
	  {
	    data       = malloc(2048);
	    alloc_data = 2048;

	    if (!data)
	      break;
	  }
	  else if ((num_data + strlen(line)) >= alloc_data)
	  {
	    unsigned char *tdata = realloc(data, alloc_data + 1024);
					    /* Expanded buffer */

	    if (!tdata)
	      break;

	    data       = tdata;
	    alloc_data += 1024;
	  }

	  decoded = alloc_data - num_data;
	  httpDecode64_2((char *)data + num_data, &decoded, line);
	  num_data += (size_t)decoded;
	}
      }

      cupsFileClose(fp);

      if (data)
	free(data);
    }
  }

  _cupsMutexUnlock(&tls_mutex);
}


/*
 * 'http_gnutls_make_path()' - Format a filename for a certificate or key file.
 */

static const char *			/* O - Filename */
http_gnutls_make_path(
    char       *buffer,			/* I - Filename buffer */
    size_t     bufsize,			/* I - Size of buffer */
    const char *dirname,		/* I - Directory */
    const char *filename,		/* I - Filename (usually hostname) */
    const char *ext)			/* I - Extension */
{
  char	*bufptr,			/* Pointer into buffer */
	*bufend = buffer + bufsize - 1;	/* End of buffer */


  snprintf(buffer, bufsize, "%s/", dirname);
  bufptr = buffer + strlen(buffer);

  while (*filename && bufptr < bufend)
  {
    if (_cups_isalnum(*filename) || *filename == '-' || *filename == '.')
      *bufptr++ = *filename;
    else
      *bufptr++ = '_';

    filename ++;
  }

  if (bufptr < bufend)
    *bufptr++ = '.';

  strlcpy(bufptr, ext, (size_t)(bufend - bufptr + 1));

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

  if (!http->blocking || http->timeout_value > 0.0)
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
 * '_httpTLSSetOptions()' - Set TLS protocol and cipher suite options.
 */

void
_httpTLSSetOptions(int options,		/* I - Options */
                   int min_version,	/* I - Minimum TLS version */
                   int max_version)	/* I - Maximum TLS version */
{
  if (!(options & _HTTP_TLS_SET_DEFAULT) || tls_options < 0)
  {
    tls_options     = options;
    tls_min_version = min_version;
    tls_max_version = max_version;
  }
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
  char			priority_string[2048];
					/* Priority string */
  int			version;	/* Current version */
  double		old_timeout;	/* Old timeout value */
  http_timeout_cb_t	old_cb;		/* Old timeout callback */
  void			*old_data;	/* Old timeout data */
  static const char * const versions[] =/* SSL/TLS versions */
  {
    "VERS-SSL3.0",
    "VERS-TLS1.0",
    "VERS-TLS1.1",
    "VERS-TLS1.2",
    "VERS-TLS1.3",
    "VERS-TLS-ALL"
  };


  DEBUG_printf(("3_httpTLSStart(http=%p)", http));

  if (tls_options < 0)
  {
    DEBUG_puts("4_httpTLSStart: Setting defaults.");
    _cupsSetDefaults();
    DEBUG_printf(("4_httpTLSStart: tls_options=%x", tls_options));
  }

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
     /*
      * First look in the CUPS keystore...
      */

      http_gnutls_make_path(crtfile, sizeof(crtfile), tls_keypath, hostname, "crt");
      http_gnutls_make_path(keyfile, sizeof(keyfile), tls_keypath, hostname, "key");

      if (access(crtfile, R_OK) || access(keyfile, R_OK))
      {
       /*
        * No CUPS-managed certs, look for CA certs...
        */

        char cacrtfile[1024], cakeyfile[1024];	/* CA cert files */

        snprintf(cacrtfile, sizeof(cacrtfile), "/etc/letsencrypt/live/%s/fullchain.pem", hostname);
        snprintf(cakeyfile, sizeof(cakeyfile), "/etc/letsencrypt/live/%s/privkey.pem", hostname);

        if ((access(cacrtfile, R_OK) || access(cakeyfile, R_OK)) && (hostptr = strchr(hostname, '.')) != NULL)
        {
         /*
          * Try just domain name...
          */

          hostptr ++;
          if (strchr(hostptr, '.'))
          {
            snprintf(cacrtfile, sizeof(cacrtfile), "/etc/letsencrypt/live/%s/fullchain.pem", hostptr);
            snprintf(cakeyfile, sizeof(cakeyfile), "/etc/letsencrypt/live/%s/privkey.pem", hostptr);
          }
        }

        if (!access(cacrtfile, R_OK) && !access(cakeyfile, R_OK))
        {
         /*
          * Use the CA certs...
          */

          strlcpy(crtfile, cacrtfile, sizeof(crtfile));
          strlcpy(keyfile, cakeyfile, sizeof(keyfile));
        }
      }

      have_creds = !access(crtfile, R_OK) && !access(keyfile, R_OK);
    }
    else if (tls_common_name)
    {
     /*
      * First look in the CUPS keystore...
      */

      http_gnutls_make_path(crtfile, sizeof(crtfile), tls_keypath, tls_common_name, "crt");
      http_gnutls_make_path(keyfile, sizeof(keyfile), tls_keypath, tls_common_name, "key");

      if (access(crtfile, R_OK) || access(keyfile, R_OK))
      {
       /*
        * No CUPS-managed certs, look for CA certs...
        */

        char cacrtfile[1024], cakeyfile[1024];	/* CA cert files */

        snprintf(cacrtfile, sizeof(cacrtfile), "/etc/letsencrypt/live/%s/fullchain.pem", tls_common_name);
        snprintf(cakeyfile, sizeof(cakeyfile), "/etc/letsencrypt/live/%s/privkey.pem", tls_common_name);

        if ((access(cacrtfile, R_OK) || access(cakeyfile, R_OK)) && (hostptr = strchr(tls_common_name, '.')) != NULL)
        {
         /*
          * Try just domain name...
          */

          hostptr ++;
          if (strchr(hostptr, '.'))
          {
            snprintf(cacrtfile, sizeof(cacrtfile), "/etc/letsencrypt/live/%s/fullchain.pem", hostptr);
            snprintf(cakeyfile, sizeof(cakeyfile), "/etc/letsencrypt/live/%s/privkey.pem", hostptr);
          }
        }

        if (!access(cacrtfile, R_OK) && !access(cakeyfile, R_OK))
        {
         /*
          * Use the CA certs...
          */

          strlcpy(crtfile, cacrtfile, sizeof(crtfile));
          strlcpy(keyfile, cakeyfile, sizeof(keyfile));
        }
      }

      have_creds = !access(crtfile, R_OK) && !access(keyfile, R_OK);
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

    if (!status)
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

  strlcpy(priority_string, "NORMAL", sizeof(priority_string));

  if (tls_max_version < _HTTP_TLS_MAX)
  {
   /*
    * Require specific TLS versions...
    */

    strlcat(priority_string, ":-VERS-TLS-ALL", sizeof(priority_string));
    for (version = tls_min_version; version <= tls_max_version; version ++)
    {
      strlcat(priority_string, ":+", sizeof(priority_string));
      strlcat(priority_string, versions[version], sizeof(priority_string));
    }
  }
  else if (tls_min_version == _HTTP_TLS_SSL3)
  {
   /*
    * Allow all versions of TLS and SSL/3.0...
    */

    strlcat(priority_string, ":+VERS-TLS-ALL:+VERS-SSL3.0", sizeof(priority_string));
  }
  else
  {
   /*
    * Require a minimum version...
    */

    strlcat(priority_string, ":+VERS-TLS-ALL", sizeof(priority_string));
    for (version = 0; version < tls_min_version; version ++)
    {
      strlcat(priority_string, ":-", sizeof(priority_string));
      strlcat(priority_string, versions[version], sizeof(priority_string));
    }
  }

  if (tls_options & _HTTP_TLS_ALLOW_RC4)
    strlcat(priority_string, ":+ARCFOUR-128", sizeof(priority_string));
  else
    strlcat(priority_string, ":!ARCFOUR-128", sizeof(priority_string));

  strlcat(priority_string, ":!ANON-DH", sizeof(priority_string));

  if (tls_options & _HTTP_TLS_DENY_CBC)
    strlcat(priority_string, ":!AES-128-CBC:!AES-256-CBC:!CAMELLIA-128-CBC:!CAMELLIA-256-CBC:!3DES-CBC", sizeof(priority_string));

#ifdef HAVE_GNUTLS_PRIORITY_SET_DIRECT
  gnutls_priority_set_direct(http->tls, priority_string, NULL);

#else
  gnutls_priority_t priority;		/* Priority */

  gnutls_priority_init(&priority, priority_string, NULL);
  gnutls_priority_set(http->tls, priority);
  gnutls_priority_deinit(priority);
#endif /* HAVE_GNUTLS_PRIORITY_SET_DIRECT */

  gnutls_transport_set_ptr(http->tls, (gnutls_transport_ptr_t)http);
  gnutls_transport_set_pull_function(http->tls, http_gnutls_read);
#ifdef HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION
  gnutls_transport_set_pull_timeout_function(http->tls, (gnutls_pull_timeout_func)httpWait);
#endif /* HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION */
  gnutls_transport_set_push_function(http->tls, http_gnutls_write);

 /*
  * Enforce a minimum timeout of 10 seconds for the TLS handshake...
  */

  old_timeout  = http->timeout_value;
  old_cb       = http->timeout_cb;
  old_data     = http->timeout_data;

  if (!old_cb || old_timeout < 10.0)
  {
    DEBUG_puts("4_httpTLSStart: Setting timeout to 10 seconds.");
    httpSetTimeout(http, 10.0, NULL, NULL);
  }

 /*
  * Do the TLS handshake...
  */

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

      httpSetTimeout(http, old_timeout, old_cb, old_data);

      return (-1);
    }
  }

 /*
  * Restore the previous timeout settings...
  */

  httpSetTimeout(http, old_timeout, old_cb, old_data);

  http->tls_credentials = credentials;

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
