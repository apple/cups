/*
 * "$Id: tls-sspi.c 12647 2015-05-20 18:37:52Z msweet $"
 *
 * TLS support for CUPS on Windows using the Security Support Provider
 * Interface (SSPI).
 *
 * Copyright 2010-2015 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/**** This file is included from tls.c ****/

/*
 * Include necessary headers...
 */

#include "debug-private.h"


/*
 * Include necessary libraries...
 */

#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Ws2_32.lib")


/*
 * Constants...
 */

#ifndef SECURITY_FLAG_IGNORE_UNKNOWN_CA
#  define SECURITY_FLAG_IGNORE_UNKNOWN_CA         0x00000100 /* Untrusted root */
#endif /* SECURITY_FLAG_IGNORE_UNKNOWN_CA */

#ifndef SECURITY_FLAG_IGNORE_CERT_CN_INVALID
#  define SECURITY_FLAG_IGNORE_CERT_CN_INVALID	  0x00001000 /* Common name does not match */
#endif /* !SECURITY_FLAG_IGNORE_CERT_CN_INVALID */

#ifndef SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
#  define SECURITY_FLAG_IGNORE_CERT_DATE_INVALID  0x00002000 /* Expired X509 Cert. */
#endif /* !SECURITY_FLAG_IGNORE_CERT_DATE_INVALID */


/*
 * Local globals...
 */

static int		tls_options = -1;/* Options for TLS connections */


/*
 * Local functions...
 */

static _http_sspi_t *http_sspi_alloc(void);
static int	http_sspi_client(http_t *http, const char *hostname);
static PCCERT_CONTEXT http_sspi_create_credential(http_credential_t *cred);
static BOOL	http_sspi_find_credentials(http_t *http, const LPWSTR containerName, const char *common_name);
static void	http_sspi_free(_http_sspi_t *sspi);
static BOOL	http_sspi_make_credentials(_http_sspi_t *sspi, const LPWSTR containerName, const char *common_name, _http_mode_t mode, int years);
static int	http_sspi_server(http_t *http, const char *hostname);
static void	http_sspi_set_allows_any_root(_http_sspi_t *sspi, BOOL allow);
static void	http_sspi_set_allows_expired_certs(_http_sspi_t *sspi, BOOL allow);
static const char *http_sspi_strerror(char *buffer, size_t bufsize, DWORD code);
static DWORD	http_sspi_verify(PCCERT_CONTEXT cert, const char *common_name, DWORD dwCertFlags);


/*
 * 'cupsMakeServerCredentials()' - Make a self-signed certificate and private key pair.
 *
 * @since CUPS 2.0/OS 10.10@
 */

int					/* O - 1 on success, 0 on failure */
cupsMakeServerCredentials(
    const char *path,			/* I - Keychain path or @code NULL@ for default */
    const char *common_name,		/* I - Common name */
    int        num_alt_names,		/* I - Number of subject alternate names */
    const char **alt_names,		/* I - Subject Alternate Names */
    time_t     expiration_date)		/* I - Expiration date */
{
  _http_sspi_t	*sspi;			/* SSPI data */
  int		ret;			/* Return value */


  DEBUG_printf(("cupsMakeServerCredentials(path=\"%s\", common_name=\"%s\", num_alt_names=%d, alt_names=%p, expiration_date=%d)", path, common_name, num_alt_names, alt_names, (int)expiration_date));

  (void)path;
  (void)num_alt_names;
  (void)alt_names;

  sspi = http_sspi_alloc();
  ret  = http_sspi_make_credentials(sspi, L"ServerContainer", common_name, _HTTP_MODE_SERVER, (int)((expiration_date - time(NULL) + 86399) / 86400 / 365));

  http_sspi_free(sspi);

  return (ret);
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
    const char *path,			/* I - Keychain path or @code NULL@ for default */
    const char *common_name,		/* I - Default common name for server */
    int        auto_create)		/* I - 1 = automatically create self-signed certificates */
{
  DEBUG_printf(("cupsSetServerCredentials(path=\"%s\", common_name=\"%s\", auto_create=%d)", path, common_name, auto_create));

  (void)path;
  (void)common_name;
  (void)auto_create;

  return (0);
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
  DEBUG_printf(("httpCopyCredentials(http=%p, credentials=%p)", http, credentials));

  if (!http || !http->tls || !http->tls->remoteCert || !credentials)
  {
    if (credentials)
      *credentials = NULL;

    return (-1);
  }

  *credentials = cupsArrayNew(NULL, NULL);
  httpAddCredential(*credentials, http->tls->remoteCert->pbCertEncoded, http->tls->remoteCert->cbCertEncoded);

  return (0);
}


/*
 * '_httpCreateCredentials()' - Create credentials in the internal format.
 */

http_tls_credentials_t			/* O - Internal credentials */
_httpCreateCredentials(
    cups_array_t *credentials)		/* I - Array of credentials */
{
  return (http_sspi_create_credential((http_credential_t *)cupsArrayFirst(credentials)));
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
  int		valid = 1;		/* Valid name? */
  PCCERT_CONTEXT cert = http_sspi_create_credential((http_credential_t *)cupsArrayFirst(credentials));
					/* Certificate */
  char		cert_name[1024];	/* Name from certificate */


  if (cert)
  {
    if (CertNameToStr(X509_ASN_ENCODING, &(cert->pCertInfo->Subject), CERT_SIMPLE_NAME_STR, cert_name, sizeof(cert_name)))
    {
     /*
      * Extract common name at end...
      */

      char  *ptr = strrchr(cert_name, ',');
      if (ptr && ptr[1])
        _cups_strcpy(cert_name, ptr + 2);
    }
    else
      strlcpy(cert_name, "unknown", sizeof(cert_name));

    CertFreeCertificateContext(cert);
  }
  else
    strlcpy(cert_name, "unknown", sizeof(cert_name));

 /*
  * Compare the common names...
  */

  if (_cups_strcasecmp(common_name, cert_name))
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

  return (valid);
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
  http_trust_t	trust = HTTP_TRUST_OK;	/* Level of trust */
  PCCERT_CONTEXT cert = NULL;		/* Certificate to validate */
  DWORD		certFlags = 0;		/* Cert verification flags */
  _cups_globals_t *cg = _cupsGlobals();	/* Per-thread global data */


  if (!common_name)
    return (HTTP_TRUST_UNKNOWN);

  cert = http_sspi_create_credential((http_credential_t *)cupsArrayFirst(credentials));
  if (!cert)
    return (HTTP_TRUST_UNKNOWN);

  if (cg->any_root < 0)
    _cupsSetDefaults();

  if (cg->any_root)
    certFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;

  if (cg->expired_certs)
    certFlags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

  if (!cg->validate_certs)
    certFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;

  if (http_sspi_verify(cert, common_name, certFlags) != SEC_E_OK)
    trust = HTTP_TRUST_INVALID;

  CertFreeCertificateContext(cert);

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
  time_t	expiration_date = 0;	/* Expiration data of credentials */
  PCCERT_CONTEXT cert = http_sspi_create_credential((http_credential_t *)cupsArrayFirst(credentials));
					/* Certificate */

  if (cert)
  {
    SYSTEMTIME	systime;		/* System time */
    struct tm	tm;			/* UNIX date/time */

    FileTimeToSystemTime(&(cert->pCertInfo->NotAfter), &systime);

    tm.tm_year = systime.wYear - 1900;
    tm.tm_mon  = systime.wMonth - 1;
    tm.tm_mday = systime.wDay;
    tm.tm_hour = systime.wHour;
    tm.tm_min  = systime.wMinute;
    tm.tm_sec  = systime.wSecond;

    expiration_date = mktime(&tm);

    CertFreeCertificateContext(cert);
  }

  return (expiration_date);
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
  http_credential_t	*first = (http_credential_t *)cupsArrayFirst(credentials);
					/* First certificate */
  PCCERT_CONTEXT 	cert;		/* Certificate */


  DEBUG_printf(("httpCredentialsString(credentials=%p, buffer=%p, bufsize=" CUPS_LLFMT ")", credentials, buffer, CUPS_LLCAST bufsize));

  if (!buffer)
    return (0);

  if (buffer && bufsize > 0)
    *buffer = '\0';

  cert = http_sspi_create_credential(first);

  if (cert)
  {
    char		cert_name[256];	/* Common name */
    SYSTEMTIME		systime;	/* System time */
    struct tm		tm;		/* UNIX date/time */
    time_t		expiration;	/* Expiration date of cert */
    _cups_md5_state_t	md5_state;	/* MD5 state */
    unsigned char	md5_digest[16];	/* MD5 result */

    FileTimeToSystemTime(&(cert->pCertInfo->NotAfter), &systime);

    tm.tm_year = systime.wYear - 1900;
    tm.tm_mon  = systime.wMonth - 1;
    tm.tm_mday = systime.wDay;
    tm.tm_hour = systime.wHour;
    tm.tm_min  = systime.wMinute;
    tm.tm_sec  = systime.wSecond;

    expiration = mktime(&tm);

    if (CertNameToStr(X509_ASN_ENCODING, &(cert->pCertInfo->Subject), CERT_SIMPLE_NAME_STR, cert_name, sizeof(cert_name)))
    {
     /*
      * Extract common name at end...
      */

      char  *ptr = strrchr(cert_name, ',');
      if (ptr && ptr[1])
        _cups_strcpy(cert_name, ptr + 2);
    }
    else
      strlcpy(cert_name, "unknown", sizeof(cert_name));

    _cupsMD5Init(&md5_state);
    _cupsMD5Append(&md5_state, first->data, (int)first->datalen);
    _cupsMD5Finish(&md5_state, md5_digest);

    snprintf(buffer, bufsize, "%s / %s / %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", cert_name, httpGetDateString(expiration), md5_digest[0], md5_digest[1], md5_digest[2], md5_digest[3], md5_digest[4], md5_digest[5], md5_digest[6], md5_digest[7], md5_digest[8], md5_digest[9], md5_digest[10], md5_digest[11], md5_digest[12], md5_digest[13], md5_digest[14], md5_digest[15]);

    CertFreeCertificateContext(cert);
  }

  DEBUG_printf(("1httpCredentialsString: Returning \"%s\".", buffer));

  return (strlen(buffer));
}


/*
 * '_httpFreeCredentials()' - Free internal credentials.
 */

void
_httpFreeCredentials(
    http_tls_credentials_t credentials)	/* I - Internal credentials */
{
  if (!credentials)
    return;

  CertFreeCertificateContext(credentials);
}


/*
 * 'httpLoadCredentials()' - Load X.509 credentials from a keychain file.
 *
 * @since CUPS 2.0/OS 10.10@
 */

int					/* O - 0 on success, -1 on error */
httpLoadCredentials(
    const char   *path,			/* I  - Keychain path or @code NULL@ for default */
    cups_array_t **credentials,		/* IO - Credentials */
    const char   *common_name)		/* I  - Common name for credentials */
{
  HCERTSTORE	store = NULL;		/* Certificate store */
  PCCERT_CONTEXT storedContext = NULL;	/* Context created from the store */
  DWORD		dwSize = 0; 		/* 32 bit size */
  PBYTE		p = NULL;		/* Temporary storage */
  HCRYPTPROV	hProv = (HCRYPTPROV)NULL;
					/* Handle to a CSP */
  CERT_NAME_BLOB sib;			/* Arbitrary array of bytes */
#ifdef DEBUG
  char		error[1024];		/* Error message buffer */
#endif /* DEBUG */


  DEBUG_printf(("httpLoadCredentials(path=\"%s\", credentials=%p, common_name=\"%s\")", path, credentials, common_name));

  (void)path;

  if (credentials)
  {
    *credentials = NULL;
  }
  else
  {
    DEBUG_puts("1httpLoadCredentials: NULL credentials pointer, returning -1.");
    return (-1);
  }

  if (!common_name)
  {
    DEBUG_puts("1httpLoadCredentials: Bad common name, returning -1.");
    return (-1);
  }

  if (!CryptAcquireContextW(&hProv, L"RememberedContainer", MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET))
  {
    if (GetLastError() == NTE_EXISTS)
    {
      if (!CryptAcquireContextW(&hProv, L"RememberedContainer", MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
      {
        DEBUG_printf(("1httpLoadCredentials: CryptAcquireContext failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
        goto cleanup;
      }
    }
  }

  store = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING|PKCS_7_ASN_ENCODING, hProv, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_NO_CRYPT_RELEASE_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, L"MY");

  if (!store)
  {
    DEBUG_printf(("1httpLoadCredentials: CertOpenSystemStore failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  dwSize = 0;

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, NULL, &dwSize, NULL))
  {
    DEBUG_printf(("1httpLoadCredentials: CertStrToName failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  p = (PBYTE)malloc(dwSize);

  if (!p)
  {
    DEBUG_printf(("1httpLoadCredentials: malloc failed for %d bytes.", dwSize));
    goto cleanup;
  }

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, p, &dwSize, NULL))
  {
    DEBUG_printf(("1httpLoadCredentials: CertStrToName failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  sib.cbData = dwSize;
  sib.pbData = p;

  storedContext = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_NAME, &sib, NULL);

  if (!storedContext)
  {
    DEBUG_printf(("1httpLoadCredentials: Unable to find credentials for \"%s\".", common_name));
    goto cleanup;
  }

  *credentials = cupsArrayNew(NULL, NULL);
  httpAddCredential(*credentials, storedContext->pbCertEncoded, storedContext->cbCertEncoded);

cleanup:

 /*
  * Cleanup
  */

  if (storedContext)
    CertFreeCertificateContext(storedContext);

  if (p)
    free(p);

  if (store)
    CertCloseStore(store, 0);

  if (hProv)
    CryptReleaseContext(hProv, 0);

  DEBUG_printf(("1httpLoadCredentials: Returning %d.", *credentials ? 0 : -1));

  return (*credentials ? 0 : -1);
}


/*
 * 'httpSaveCredentials()' - Save X.509 credentials to a keychain file.
 *
 * @since CUPS 2.0/OS 10.10@
 */

int					/* O - -1 on error, 0 on success */
httpSaveCredentials(
    const char   *path,			/* I - Keychain path or @code NULL@ for default */
    cups_array_t *credentials,		/* I - Credentials */
    const char   *common_name)		/* I - Common name for credentials */
{
  HCERTSTORE	store = NULL;		/* Certificate store */
  PCCERT_CONTEXT storedContext = NULL;	/* Context created from the store */
  PCCERT_CONTEXT createdContext = NULL;	/* Context created by us */
  DWORD		dwSize = 0; 		/* 32 bit size */
  PBYTE		p = NULL;		/* Temporary storage */
  HCRYPTPROV	hProv = (HCRYPTPROV)NULL;
					/* Handle to a CSP */
  CRYPT_KEY_PROV_INFO ckp;		/* Handle to crypto key */
  int		ret = -1;		/* Return value */
#ifdef DEBUG
  char		error[1024];		/* Error message buffer */
#endif /* DEBUG */


  DEBUG_printf(("httpSaveCredentials(path=\"%s\", credentials=%p, common_name=\"%s\")", path, credentials, common_name));

  (void)path;

  if (!common_name)
  {
    DEBUG_puts("1httpSaveCredentials: Bad common name, returning -1.");
    return (-1);
  }

  createdContext = http_sspi_create_credential((http_credential_t *)cupsArrayFirst(credentials));
  if (!createdContext)
  {
    DEBUG_puts("1httpSaveCredentials: Bad credentials, returning -1.");
    return (-1);
  }

  if (!CryptAcquireContextW(&hProv, L"RememberedContainer", MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET))
  {
    if (GetLastError() == NTE_EXISTS)
    {
      if (!CryptAcquireContextW(&hProv, L"RememberedContainer", MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
      {
        DEBUG_printf(("1httpSaveCredentials: CryptAcquireContext failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
        goto cleanup;
      }
    }
  }

  store = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING|PKCS_7_ASN_ENCODING, hProv, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_NO_CRYPT_RELEASE_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, L"MY");

  if (!store)
  {
    DEBUG_printf(("1httpSaveCredentials: CertOpenSystemStore failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  dwSize = 0;

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, NULL, &dwSize, NULL))
  {
    DEBUG_printf(("1httpSaveCredentials: CertStrToName failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  p = (PBYTE)malloc(dwSize);

  if (!p)
  {
    DEBUG_printf(("1httpSaveCredentials: malloc failed for %d bytes.", dwSize));
    goto cleanup;
  }

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, p, &dwSize, NULL))
  {
    DEBUG_printf(("1httpSaveCredentials: CertStrToName failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

 /*
  * Add the created context to the named store, and associate it with the named
  * container...
  */

  if (!CertAddCertificateContextToStore(store, createdContext, CERT_STORE_ADD_REPLACE_EXISTING, &storedContext))
  {
    DEBUG_printf(("1httpSaveCredentials: CertAddCertificateContextToStore failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  ZeroMemory(&ckp, sizeof(ckp));
  ckp.pwszContainerName = L"RememberedContainer";
  ckp.pwszProvName      = MS_DEF_PROV_W;
  ckp.dwProvType        = PROV_RSA_FULL;
  ckp.dwFlags           = CRYPT_MACHINE_KEYSET;
  ckp.dwKeySpec         = AT_KEYEXCHANGE;

  if (!CertSetCertificateContextProperty(storedContext, CERT_KEY_PROV_INFO_PROP_ID, 0, &ckp))
  {
    DEBUG_printf(("1httpSaveCredentials: CertSetCertificateContextProperty failed: %s", http_sspi_strerror(error, sizeof(error), GetLastError())));
    goto cleanup;
  }

  ret = 0;

cleanup:

 /*
  * Cleanup
  */

  if (createdContext)
    CertFreeCertificateContext(createdContext);

  if (storedContext)
    CertFreeCertificateContext(storedContext);

  if (p)
    free(p);

  if (store)
    CertCloseStore(store, 0);

  if (hProv)
    CryptReleaseContext(hProv, 0);

  DEBUG_printf(("1httpSaveCredentials: Returning %d.", ret));
  return (ret);
}


/*
 * '_httpTLSInitialize()' - Initialize the TLS stack.
 */

void
_httpTLSInitialize(void)
{
 /*
  * Nothing to do...
  */
}


/*
 * '_httpTLSPending()' - Return the number of pending TLS-encrypted bytes.
 */

size_t					/* O - Bytes available */
_httpTLSPending(http_t *http)		/* I - HTTP connection */
{
  if (http->tls)
    return (http->tls->readBufferUsed);
  else
    return (0);
}


/*
 * '_httpTLSRead()' - Read from a SSL/TLS connection.
 */

int					/* O - Bytes read */
_httpTLSRead(http_t *http,		/* I - HTTP connection */
	     char   *buf,		/* I - Buffer to store data */
	     int    len)		/* I - Length of buffer */
{
  int		i;			/* Looping var */
  _http_sspi_t	*sspi = http->tls;	/* SSPI data */
  SecBufferDesc	message;		/* Array of SecBuffer struct */
  SecBuffer	buffers[4] = { 0 };	/* Security package buffer */
  int		num = 0;		/* Return value */
  PSecBuffer	pDataBuffer;		/* Data buffer */
  PSecBuffer	pExtraBuffer;		/* Excess data buffer */
  SECURITY_STATUS scRet;		/* SSPI status */


  DEBUG_printf(("4_httpTLSRead(http=%p, buf=%p, len=%d)", http, buf, len));

 /*
  * If there are bytes that have already been decrypted and have not yet been
  * read, return those...
  */

  if (sspi->readBufferUsed > 0)
  {
    int bytesToCopy = min(sspi->readBufferUsed, len);
					/* Number of bytes to copy */

    memcpy(buf, sspi->readBuffer, bytesToCopy);
    sspi->readBufferUsed -= bytesToCopy;

    if (sspi->readBufferUsed > 0)
      memmove(sspi->readBuffer, sspi->readBuffer + bytesToCopy, sspi->readBufferUsed);

    DEBUG_printf(("5_httpTLSRead: Returning %d bytes previously decrypted.", bytesToCopy));

    return (bytesToCopy);
  }

 /*
  * Initialize security buffer structs
  */

  message.ulVersion = SECBUFFER_VERSION;
  message.cBuffers  = 4;
  message.pBuffers  = buffers;

  do
  {
   /*
    * If there is not enough space in the buffer, then increase its size...
    */

    if (sspi->decryptBufferLength <= sspi->decryptBufferUsed)
    {
      BYTE *temp;			/* New buffer */

      if (sspi->decryptBufferLength >= 262144)
      {
	WSASetLastError(E_OUTOFMEMORY);
        DEBUG_puts("_httpTLSRead: Decryption buffer too large (>256k)");
	return (-1);
      }

      if ((temp = realloc(sspi->decryptBuffer, sspi->decryptBufferLength + 4096)) == NULL)
      {
	DEBUG_printf(("_httpTLSRead: Unable to allocate %d byte decryption buffer.", sspi->decryptBufferLength + 4096));
	WSASetLastError(E_OUTOFMEMORY);
	return (-1);
      }

      sspi->decryptBufferLength += 4096;
      sspi->decryptBuffer       = temp;

      DEBUG_printf(("_httpTLSRead: Resized decryption buffer to %d bytes.", sspi->decryptBufferLength));
    }

    buffers[0].pvBuffer	  = sspi->decryptBuffer;
    buffers[0].cbBuffer	  = (unsigned long)sspi->decryptBufferUsed;
    buffers[0].BufferType = SECBUFFER_DATA;
    buffers[1].BufferType = SECBUFFER_EMPTY;
    buffers[2].BufferType = SECBUFFER_EMPTY;
    buffers[3].BufferType = SECBUFFER_EMPTY;

    DEBUG_printf(("5_httpTLSRead: decryptBufferUsed=%d", sspi->decryptBufferUsed));

    scRet = DecryptMessage(&sspi->context, &message, 0, NULL);

    if (scRet == SEC_E_INCOMPLETE_MESSAGE)
    {
      num = recv(http->fd, sspi->decryptBuffer + sspi->decryptBufferUsed, (int)(sspi->decryptBufferLength - sspi->decryptBufferUsed), 0);
      if (num < 0)
      {
	DEBUG_printf(("5_httpTLSRead: recv failed: %d", WSAGetLastError()));
	return (-1);
      }
      else if (num == 0)
      {
	DEBUG_puts("5_httpTLSRead: Server disconnected.");
	return (0);
      }

      DEBUG_printf(("5_httpTLSRead: Read %d bytes into decryption buffer.", num));

      sspi->decryptBufferUsed += num;
    }
  }
  while (scRet == SEC_E_INCOMPLETE_MESSAGE);

  if (scRet == SEC_I_CONTEXT_EXPIRED)
  {
    DEBUG_puts("5_httpTLSRead: Context expired.");
    WSASetLastError(WSAECONNRESET);
    return (-1);
  }
  else if (scRet != SEC_E_OK)
  {
    DEBUG_printf(("5_httpTLSRead: DecryptMessage failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
    WSASetLastError(WSASYSCALLFAILURE);
    return (-1);
  }

 /*
  * The decryption worked.  Now, locate data buffer.
  */

  pDataBuffer  = NULL;
  pExtraBuffer = NULL;

  for (i = 1; i < 4; i++)
  {
    if (buffers[i].BufferType == SECBUFFER_DATA)
      pDataBuffer = &buffers[i];
    else if (!pExtraBuffer && (buffers[i].BufferType == SECBUFFER_EXTRA))
      pExtraBuffer = &buffers[i];
  }

 /*
  * If a data buffer is found, then copy the decrypted bytes to the passed-in
  * buffer...
  */

  if (pDataBuffer)
  {
    int bytesToCopy = min((int)pDataBuffer->cbBuffer, len);
				      /* Number of bytes to copy into buf */
    int bytesToSave = pDataBuffer->cbBuffer - bytesToCopy;
				      /* Number of bytes to save in our read buffer */

    if (bytesToCopy)
      memcpy(buf, pDataBuffer->pvBuffer, bytesToCopy);

   /*
    * If there are more decrypted bytes than can be copied to the passed in
    * buffer, then save them...
    */

    if (bytesToSave)
    {
      if ((sspi->readBufferLength - sspi->readBufferUsed) < bytesToSave)
      {
        BYTE *temp;			/* New buffer pointer */

        if ((temp = realloc(sspi->readBuffer, sspi->readBufferUsed + bytesToSave)) == NULL)
	{
	  DEBUG_printf(("_httpTLSRead: Unable to allocate %d bytes.", sspi->readBufferUsed + bytesToSave));
	  WSASetLastError(E_OUTOFMEMORY);
	  return (-1);
	}

	sspi->readBufferLength = sspi->readBufferUsed + bytesToSave;
	sspi->readBuffer       = temp;
      }

      memcpy(((BYTE *)sspi->readBuffer) + sspi->readBufferUsed, ((BYTE *)pDataBuffer->pvBuffer) + bytesToCopy, bytesToSave);

      sspi->readBufferUsed += bytesToSave;
    }

    num = bytesToCopy;
  }
  else
  {
    DEBUG_puts("_httpTLSRead: Unable to find data buffer.");
    WSASetLastError(WSASYSCALLFAILURE);
    return (-1);
  }

 /*
  * If the decryption process left extra bytes, then save those back in
  * decryptBuffer.  They will be processed the next time through the loop.
  */

  if (pExtraBuffer)
  {
    memmove(sspi->decryptBuffer, pExtraBuffer->pvBuffer, pExtraBuffer->cbBuffer);
    sspi->decryptBufferUsed = pExtraBuffer->cbBuffer;
  }
  else
  {
    sspi->decryptBufferUsed = 0;
  }

  return (num);
}


/*
 * '_httpTLSSetOptions()' - Set TLS protocol and cipher suite options.
 */

void
_httpTLSSetOptions(int options)		/* I - Options */
{
  tls_options = options;
}


/*
 * '_httpTLSStart()' - Set up SSL/TLS support on a connection.
 */

int					/* O - 0 on success, -1 on failure */
_httpTLSStart(http_t *http)		/* I - HTTP connection */
{
  char	hostname[256],			/* Hostname */
	*hostptr;			/* Pointer into hostname */


  DEBUG_printf(("3_httpTLSStart(http=%p)", http));

  if (tls_options < 0)
  {
    DEBUG_puts("4_httpTLSStart: Setting defaults.");
    _cupsSetDefaults();
    DEBUG_printf(("4_httpTLSStart: tls_options=%x", tls_options));
  }

  if ((http->tls = http_sspi_alloc()) == NULL)
    return (-1);

  if (http->mode == _HTTP_MODE_CLIENT)
  {
   /*
    * Client: determine hostname...
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

    return (http_sspi_client(http, hostname));
  }
  else
  {
   /*
    * Server: determine hostname to use...
    */

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

    return (http_sspi_server(http, hostname));
  }
}


/*
 * '_httpTLSStop()' - Shut down SSL/TLS on a connection.
 */

void
_httpTLSStop(http_t *http)		/* I - HTTP connection */
{
  _http_sspi_t	*sspi = http->tls;	/* SSPI data */


  if (sspi->contextInitialized && http->fd >= 0)
  {
    SecBufferDesc	message;	/* Array of SecBuffer struct */
    SecBuffer		buffers[1] = { 0 };
					/* Security package buffer */
    DWORD		dwType;		/* Type */
    DWORD		status;		/* Status */

  /*
   * Notify schannel that we are about to close the connection.
   */

   dwType = SCHANNEL_SHUTDOWN;

   buffers[0].pvBuffer   = &dwType;
   buffers[0].BufferType = SECBUFFER_TOKEN;
   buffers[0].cbBuffer   = sizeof(dwType);

   message.cBuffers  = 1;
   message.pBuffers  = buffers;
   message.ulVersion = SECBUFFER_VERSION;

   status = ApplyControlToken(&sspi->context, &message);

   if (SUCCEEDED(status))
   {
     PBYTE	pbMessage;		/* Message buffer */
     DWORD	cbMessage;		/* Message buffer count */
     DWORD	cbData;			/* Data count */
     DWORD	dwSSPIFlags;		/* SSL attributes we requested */
     DWORD	dwSSPIOutFlags;		/* SSL attributes we received */
     TimeStamp	tsExpiry;		/* Time stamp */

     dwSSPIFlags = ASC_REQ_SEQUENCE_DETECT     |
                   ASC_REQ_REPLAY_DETECT       |
                   ASC_REQ_CONFIDENTIALITY     |
                   ASC_REQ_EXTENDED_ERROR      |
                   ASC_REQ_ALLOCATE_MEMORY     |
                   ASC_REQ_STREAM;

     buffers[0].pvBuffer   = NULL;
     buffers[0].BufferType = SECBUFFER_TOKEN;
     buffers[0].cbBuffer   = 0;

     message.cBuffers  = 1;
     message.pBuffers  = buffers;
     message.ulVersion = SECBUFFER_VERSION;

     status = AcceptSecurityContext(&sspi->creds, &sspi->context, NULL,
                                    dwSSPIFlags, SECURITY_NATIVE_DREP, NULL,
                                    &message, &dwSSPIOutFlags, &tsExpiry);

      if (SUCCEEDED(status))
      {
        pbMessage = buffers[0].pvBuffer;
        cbMessage = buffers[0].cbBuffer;

       /*
        * Send the close notify message to the client.
        */

        if (pbMessage && cbMessage)
        {
          cbData = send(http->fd, pbMessage, cbMessage, 0);
          if ((cbData == SOCKET_ERROR) || (cbData == 0))
          {
            status = WSAGetLastError();
            DEBUG_printf(("_httpTLSStop: sending close notify failed: %d", status));
          }
          else
          {
            FreeContextBuffer(pbMessage);
          }
        }
      }
      else
      {
        DEBUG_printf(("_httpTLSStop: AcceptSecurityContext failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), status)));
      }
    }
    else
    {
      DEBUG_printf(("_httpTLSStop: ApplyControlToken failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), status)));
    }
  }

  http_sspi_free(sspi);

  http->tls = NULL;
}


/*
 * '_httpTLSWrite()' - Write to a SSL/TLS connection.
 */

int					/* O - Bytes written */
_httpTLSWrite(http_t     *http,		/* I - HTTP connection */
	      const char *buf,		/* I - Buffer holding data */
	      int        len)		/* I - Length of buffer */
{
  _http_sspi_t	*sspi = http->tls;	/* SSPI data */
  SecBufferDesc	message;		/* Array of SecBuffer struct */
  SecBuffer	buffers[4] = { 0 };	/* Security package buffer */
  int		bufferLen;		/* Buffer length */
  int		bytesLeft;		/* Bytes left to write */
  const char	*bufptr;		/* Pointer into buffer */
  int		num = 0;		/* Return value */


  bufferLen = sspi->streamSizes.cbMaximumMessage + sspi->streamSizes.cbHeader + sspi->streamSizes.cbTrailer;

  if (bufferLen > sspi->writeBufferLength)
  {
    BYTE *temp;				/* New buffer pointer */

    if ((temp = (BYTE *)realloc(sspi->writeBuffer, bufferLen)) == NULL)
    {
      DEBUG_printf(("_httpTLSWrite: Unable to allocate buffer of %d bytes.", bufferLen));
      WSASetLastError(E_OUTOFMEMORY);
      return (-1);
    }

    sspi->writeBuffer       = temp;
    sspi->writeBufferLength = bufferLen;
  }

  bytesLeft = len;
  bufptr    = buf;

  while (bytesLeft)
  {
    int chunk = min((int)sspi->streamSizes.cbMaximumMessage, bytesLeft);
					/* Size of data to write */
    SECURITY_STATUS scRet;		/* SSPI status */

   /*
    * Copy user data into the buffer, starting just past the header...
    */

    memcpy(sspi->writeBuffer + sspi->streamSizes.cbHeader, bufptr, chunk);

   /*
    * Setup the SSPI buffers
    */

    message.ulVersion = SECBUFFER_VERSION;
    message.cBuffers  = 4;
    message.pBuffers  = buffers;

    buffers[0].pvBuffer   = sspi->writeBuffer;
    buffers[0].cbBuffer   = sspi->streamSizes.cbHeader;
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[1].pvBuffer   = sspi->writeBuffer + sspi->streamSizes.cbHeader;
    buffers[1].cbBuffer   = (unsigned long) chunk;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[2].pvBuffer   = sspi->writeBuffer + sspi->streamSizes.cbHeader + chunk;
    buffers[2].cbBuffer   = sspi->streamSizes.cbTrailer;
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[3].BufferType = SECBUFFER_EMPTY;

   /*
    * Encrypt the data
    */

    scRet = EncryptMessage(&sspi->context, 0, &message, 0);

    if (FAILED(scRet))
    {
      DEBUG_printf(("_httpTLSWrite: EncryptMessage failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
      WSASetLastError(WSASYSCALLFAILURE);
      return (-1);
    }

   /*
    * Send the data. Remember the size of the total data to send is the size
    * of the header, the size of the data the caller passed in and the size
    * of the trailer...
    */

    num = send(http->fd, sspi->writeBuffer, buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer, 0);

    if (num <= 0)
    {
      DEBUG_printf(("_httpTLSWrite: send failed: %ld", WSAGetLastError()));
      return (num);
    }

    bytesLeft -= chunk;
    bufptr    += chunk;
  }

  return (len);
}


#if 0
/*
 * 'http_setup_ssl()' - Set up SSL/TLS support on a connection.
 */

static int				/* O - 0 on success, -1 on failure */
http_setup_ssl(http_t *http)		/* I - Connection to server */
{
  char			hostname[256],	/* Hostname */
			*hostptr;	/* Pointer into hostname */

  TCHAR			username[256];	/* Username returned from GetUserName() */
  TCHAR			commonName[256];/* Common name for certificate */
  DWORD			dwSize;		/* 32 bit size */


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

  http->tls = http_sspi_alloc();

  if (!http->tls)
  {
    _cupsSetHTTPError(HTTP_STATUS_ERROR);
    return (-1);
  }

  dwSize          = sizeof(username) / sizeof(TCHAR);
  GetUserName(username, &dwSize);
  _sntprintf_s(commonName, sizeof(commonName) / sizeof(TCHAR),
               sizeof(commonName) / sizeof(TCHAR), TEXT("CN=%s"), username);

  if (!_sspiGetCredentials(http->tls, L"ClientContainer",
                           commonName, FALSE))
  {
    _sspiFree(http->tls);
    http->tls = NULL;

    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI,
                  _("Unable to establish a secure connection to host."), 1);

    return (-1);
  }

  _sspiSetAllowsAnyRoot(http->tls, TRUE);
  _sspiSetAllowsExpiredCerts(http->tls, TRUE);

  if (!_sspiConnect(http->tls, hostname))
  {
    _sspiFree(http->tls);
    http->tls = NULL;

    http->error  = EIO;
    http->status = HTTP_STATUS_ERROR;

    _cupsSetError(IPP_STATUS_ERROR_CUPS_PKI,
                  _("Unable to establish a secure connection to host."), 1);

    return (-1);
  }

  return (0);
}
#endif // 0


/*
 * 'http_sspi_alloc()' - Allocate SSPI object.
 */

static _http_sspi_t *			/* O  - New SSPI/SSL object */
http_sspi_alloc(void)
{
  return ((_http_sspi_t *)calloc(sizeof(_http_sspi_t), 1));
}


/*
 * 'http_sspi_client()' - Negotiate a TLS connection as a client.
 */

static int				/* O - 0 on success, -1 on failure */
http_sspi_client(http_t     *http,	/* I - Client connection */
                 const char *hostname)	/* I - Server hostname */
{
  _http_sspi_t	*sspi = http->tls;	/* SSPI data */
  DWORD		dwSize;			/* Size for buffer */
  DWORD		dwSSPIFlags;		/* SSL connection attributes we want */
  DWORD		dwSSPIOutFlags;		/* SSL connection attributes we got */
  TimeStamp	tsExpiry;		/* Time stamp */
  SECURITY_STATUS scRet;		/* Status */
  int		cbData;			/* Data count */
  SecBufferDesc	inBuffer;		/* Array of SecBuffer structs */
  SecBuffer	inBuffers[2];		/* Security package buffer */
  SecBufferDesc	outBuffer;		/* Array of SecBuffer structs */
  SecBuffer	outBuffers[1];		/* Security package buffer */
  int		ret = 0;		/* Return value */
  char		username[1024],		/* Current username */
		common_name[1024];	/* CN=username */


  DEBUG_printf(("4http_sspi_client(http=%p, hostname=\"%s\")", http, hostname));

  dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT   |
                ISC_REQ_REPLAY_DETECT     |
                ISC_REQ_CONFIDENTIALITY   |
                ISC_RET_EXTENDED_ERROR    |
                ISC_REQ_ALLOCATE_MEMORY   |
                ISC_REQ_STREAM;

 /*
  * Lookup the client certificate...
  */

  dwSize = sizeof(username);
  GetUserName(username, &dwSize);
  snprintf(common_name, sizeof(common_name), "CN=%s", username);

  if (!http_sspi_find_credentials(http, L"ClientContainer", common_name))
    if (!http_sspi_make_credentials(http->tls, L"ClientContainer", common_name, _HTTP_MODE_CLIENT, 10))
    {
      DEBUG_puts("5http_sspi_client: Unable to get client credentials.");
      return (-1);
    }

 /*
  * Initiate a ClientHello message and generate a token.
  */

  outBuffers[0].pvBuffer   = NULL;
  outBuffers[0].BufferType = SECBUFFER_TOKEN;
  outBuffers[0].cbBuffer   = 0;

  outBuffer.cBuffers  = 1;
  outBuffer.pBuffers  = outBuffers;
  outBuffer.ulVersion = SECBUFFER_VERSION;

  scRet = InitializeSecurityContext(&sspi->creds, NULL, TEXT(""), dwSSPIFlags, 0, SECURITY_NATIVE_DREP, NULL, 0, &sspi->context, &outBuffer, &dwSSPIOutFlags, &tsExpiry);

  if (scRet != SEC_I_CONTINUE_NEEDED)
  {
    DEBUG_printf(("5http_sspi_client: InitializeSecurityContext(1) failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
    return (-1);
  }

 /*
  * Send response to server if there is one.
  */

  if (outBuffers[0].cbBuffer && outBuffers[0].pvBuffer)
  {
    if ((cbData = send(http->fd, outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0)) <= 0)
    {
      DEBUG_printf(("5http_sspi_client: send failed: %d", WSAGetLastError()));
      FreeContextBuffer(outBuffers[0].pvBuffer);
      DeleteSecurityContext(&sspi->context);
      return (-1);
    }

    DEBUG_printf(("5http_sspi_client: %d bytes of handshake data sent.", cbData));

    FreeContextBuffer(outBuffers[0].pvBuffer);
    outBuffers[0].pvBuffer = NULL;
  }

  dwSSPIFlags = ISC_REQ_MANUAL_CRED_VALIDATION |
		ISC_REQ_SEQUENCE_DETECT        |
                ISC_REQ_REPLAY_DETECT          |
                ISC_REQ_CONFIDENTIALITY        |
                ISC_RET_EXTENDED_ERROR         |
                ISC_REQ_ALLOCATE_MEMORY        |
                ISC_REQ_STREAM;

  sspi->decryptBufferUsed = 0;

 /*
  * Loop until the handshake is finished or an error occurs.
  */

  scRet = SEC_I_CONTINUE_NEEDED;

  while(scRet == SEC_I_CONTINUE_NEEDED        ||
        scRet == SEC_E_INCOMPLETE_MESSAGE     ||
        scRet == SEC_I_INCOMPLETE_CREDENTIALS)
  {
    if (sspi->decryptBufferUsed == 0 || scRet == SEC_E_INCOMPLETE_MESSAGE)
    {
      if (sspi->decryptBufferLength <= sspi->decryptBufferUsed)
      {
	BYTE *temp;			/* New buffer */

	if (sspi->decryptBufferLength >= 262144)
	{
	  WSASetLastError(E_OUTOFMEMORY);
	  DEBUG_puts("5http_sspi_client: Decryption buffer too large (>256k)");
	  return (-1);
	}

	if ((temp = realloc(sspi->decryptBuffer, sspi->decryptBufferLength + 4096)) == NULL)
	{
	  DEBUG_printf(("5http_sspi_client: Unable to allocate %d byte buffer.", sspi->decryptBufferLength + 4096));
	  WSASetLastError(E_OUTOFMEMORY);
	  return (-1);
	}

	sspi->decryptBufferLength += 4096;
	sspi->decryptBuffer       = temp;
      }

      cbData = recv(http->fd, sspi->decryptBuffer + sspi->decryptBufferUsed, (int)(sspi->decryptBufferLength - sspi->decryptBufferUsed), 0);

      if (cbData < 0)
      {
        DEBUG_printf(("5http_sspi_client: recv failed: %d", WSAGetLastError()));
        return (-1);
      }
      else if (cbData == 0)
      {
        DEBUG_printf(("5http_sspi_client: Server unexpectedly disconnected."));
        return (-1);
      }

      DEBUG_printf(("5http_sspi_client: %d bytes of handshake data received", cbData));

      sspi->decryptBufferUsed += cbData;
    }

   /*
    * Set up the input buffers. Buffer 0 is used to pass in data received from
    * the server.  Schannel will consume some or all of this.  Leftover data
    * (if any) will be placed in buffer 1 and given a buffer type of
    * SECBUFFER_EXTRA.
    */

    inBuffers[0].pvBuffer   = sspi->decryptBuffer;
    inBuffers[0].cbBuffer   = (unsigned long)sspi->decryptBufferUsed;
    inBuffers[0].BufferType = SECBUFFER_TOKEN;

    inBuffers[1].pvBuffer   = NULL;
    inBuffers[1].cbBuffer   = 0;
    inBuffers[1].BufferType = SECBUFFER_EMPTY;

    inBuffer.cBuffers       = 2;
    inBuffer.pBuffers       = inBuffers;
    inBuffer.ulVersion      = SECBUFFER_VERSION;

   /*
    * Set up the output buffers. These are initialized to NULL so as to make it
    * less likely we'll attempt to free random garbage later.
    */

    outBuffers[0].pvBuffer   = NULL;
    outBuffers[0].BufferType = SECBUFFER_TOKEN;
    outBuffers[0].cbBuffer   = 0;

    outBuffer.cBuffers       = 1;
    outBuffer.pBuffers       = outBuffers;
    outBuffer.ulVersion      = SECBUFFER_VERSION;

   /*
    * Call InitializeSecurityContext.
    */

    scRet = InitializeSecurityContext(&sspi->creds, &sspi->context, NULL, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, &inBuffer, 0, NULL, &outBuffer, &dwSSPIOutFlags, &tsExpiry);

   /*
    * If InitializeSecurityContext was successful (or if the error was one of
    * the special extended ones), send the contents of the output buffer to the
    * server.
    */

    if (scRet == SEC_E_OK                ||
        scRet == SEC_I_CONTINUE_NEEDED   ||
        FAILED(scRet) && (dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR))
    {
      if (outBuffers[0].cbBuffer && outBuffers[0].pvBuffer)
      {
        cbData = send(http->fd, outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0);

        if (cbData <= 0)
        {
          DEBUG_printf(("5http_sspi_client: send failed: %d", WSAGetLastError()));
          FreeContextBuffer(outBuffers[0].pvBuffer);
          DeleteSecurityContext(&sspi->context);
          return (-1);
        }

        DEBUG_printf(("5http_sspi_client: %d bytes of handshake data sent.", cbData));

       /*
        * Free output buffer.
        */

        FreeContextBuffer(outBuffers[0].pvBuffer);
        outBuffers[0].pvBuffer = NULL;
      }
    }

   /*
    * If InitializeSecurityContext returned SEC_E_INCOMPLETE_MESSAGE, then we
    * need to read more data from the server and try again.
    */

    if (scRet == SEC_E_INCOMPLETE_MESSAGE)
      continue;

   /*
    * If InitializeSecurityContext returned SEC_E_OK, then the handshake
    * completed successfully.
    */

    if (scRet == SEC_E_OK)
    {
     /*
      * If the "extra" buffer contains data, this is encrypted application
      * protocol layer stuff. It needs to be saved. The application layer will
      * later decrypt it with DecryptMessage.
      */

      DEBUG_puts("5http_sspi_client: Handshake was successful.");

      if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
      {
        memmove(sspi->decryptBuffer, sspi->decryptBuffer + sspi->decryptBufferUsed - inBuffers[1].cbBuffer, inBuffers[1].cbBuffer);

        sspi->decryptBufferUsed = inBuffers[1].cbBuffer;

        DEBUG_printf(("5http_sspi_client: %d bytes of app data was bundled with handshake data", sspi->decryptBufferUsed));
      }
      else
        sspi->decryptBufferUsed = 0;

     /*
      * Bail out to quit
      */

      break;
    }

   /*
    * Check for fatal error.
    */

    if (FAILED(scRet))
    {
      DEBUG_printf(("5http_sspi_client: InitializeSecurityContext(2) failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
      ret = -1;
      break;
    }

   /*
    * If InitializeSecurityContext returned SEC_I_INCOMPLETE_CREDENTIALS,
    * then the server just requested client authentication.
    */

    if (scRet == SEC_I_INCOMPLETE_CREDENTIALS)
    {
     /*
      * Unimplemented
      */

      DEBUG_printf(("5http_sspi_client: server requested client credentials."));
      ret = -1;
      break;
    }

   /*
    * Copy any leftover data from the "extra" buffer, and go around again.
    */

    if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
    {
      memmove(sspi->decryptBuffer, sspi->decryptBuffer + sspi->decryptBufferUsed - inBuffers[1].cbBuffer, inBuffers[1].cbBuffer);

      sspi->decryptBufferUsed = inBuffers[1].cbBuffer;
    }
    else
    {
      sspi->decryptBufferUsed = 0;
    }
  }

  if (!ret)
  {
   /*
    * Success!  Get the server cert
    */

    sspi->contextInitialized = TRUE;

    scRet = QueryContextAttributes(&sspi->context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, (VOID *)&(sspi->remoteCert));

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("5http_sspi_client: QueryContextAttributes failed(SECPKG_ATTR_REMOTE_CERT_CONTEXT): %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
      return (-1);
    }

   /*
    * Find out how big the header/trailer will be:
    */

    scRet = QueryContextAttributes(&sspi->context, SECPKG_ATTR_STREAM_SIZES, &sspi->streamSizes);

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("5http_sspi_client: QueryContextAttributes failed(SECPKG_ATTR_STREAM_SIZES): %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
      ret = -1;
    }
  }

  return (ret);
}


/*
 * 'http_sspi_create_credential()' - Create an SSPI certificate context.
 */

static PCCERT_CONTEXT			/* O - Certificate context */
http_sspi_create_credential(
    http_credential_t *cred)		/* I - Credential */
{
  if (cred)
    return (CertCreateCertificateContext(X509_ASN_ENCODING, cred->data, cred->datalen));
  else
    return (NULL);
}


/*
 * 'http_sspi_find_credentials()' - Retrieve a TLS certificate from the system store.
 */

static BOOL				/* O - 1 on success, 0 on failure */
http_sspi_find_credentials(
    http_t       *http,			/* I - HTTP connection */
    const LPWSTR container,		/* I - Cert container name */
    const char   *common_name)		/* I - Common name of certificate */
{
  _http_sspi_t	*sspi = http->tls;	/* SSPI data */
  HCERTSTORE	store = NULL;		/* Certificate store */
  PCCERT_CONTEXT storedContext = NULL;	/* Context created from the store */
  DWORD		dwSize = 0; 		/* 32 bit size */
  PBYTE		p = NULL;		/* Temporary storage */
  HCRYPTPROV	hProv = (HCRYPTPROV)NULL;
					/* Handle to a CSP */
  CERT_NAME_BLOB sib;			/* Arbitrary array of bytes */
  SCHANNEL_CRED	SchannelCred;		/* Schannel credential data */
  TimeStamp	tsExpiry;		/* Time stamp */
  SECURITY_STATUS Status;		/* Status */
  BOOL		ok = TRUE;		/* Return value */


  if (!CryptAcquireContextW(&hProv, (LPWSTR)container, MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET))
  {
    if (GetLastError() == NTE_EXISTS)
    {
      if (!CryptAcquireContextW(&hProv, (LPWSTR)container, MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
      {
        DEBUG_printf(("5http_sspi_find_credentials: CryptAcquireContext failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
        ok = FALSE;
        goto cleanup;
      }
    }
  }

  store = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING|PKCS_7_ASN_ENCODING, hProv, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_NO_CRYPT_RELEASE_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, L"MY");

  if (!store)
  {
    DEBUG_printf(("5http_sspi_find_credentials: CertOpenSystemStore failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  dwSize = 0;

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, NULL, &dwSize, NULL))
  {
    DEBUG_printf(("5http_sspi_find_credentials: CertStrToName failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  p = (PBYTE)malloc(dwSize);

  if (!p)
  {
    DEBUG_printf(("5http_sspi_find_credentials: malloc failed for %d bytes.", dwSize));
    ok = FALSE;
    goto cleanup;
  }

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, p, &dwSize, NULL))
  {
    DEBUG_printf(("5http_sspi_find_credentials: CertStrToName failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  sib.cbData = dwSize;
  sib.pbData = p;

  storedContext = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_NAME, &sib, NULL);

  if (!storedContext)
  {
    DEBUG_printf(("5http_sspi_find_credentials: Unable to find credentials for \"%s\".", common_name));
    ok = FALSE;
    goto cleanup;
  }

  ZeroMemory(&SchannelCred, sizeof(SchannelCred));

  SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
  SchannelCred.cCreds    = 1;
  SchannelCred.paCred    = &storedContext;

 /*
  * Set supported protocols (can also be overriden in the registry...)
  */

#ifdef SP_PROT_TLS1_2_SERVER
  if (http->mode == _HTTP_MODE_SERVER)
  {
    if (tls_options & _HTTP_TLS_DENY_TLS10)
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER | SP_PROT_TLS1_1_SERVER;
    else if (tls_options & _HTTP_TLS_ALLOW_SSL3)
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER | SP_PROT_TLS1_1_SERVER | SP_PROT_TLS1_0_SERVER | SP_PROT_SSL3_SERVER;
    else
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER | SP_PROT_TLS1_1_SERVER | SP_PROT_TLS1_0_SERVER;
  }
  else
  {
    if (tls_options & _HTTP_TLS_DENY_TLS10)
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_1_CLIENT;
    else if (tls_options & _HTTP_TLS_ALLOW_SSL3)
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_0_CLIENT | SP_PROT_SSL3_CLIENT;
    else
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_0_CLIENT;
  }

#else
  if (http->mode == _HTTP_MODE_SERVER)
  {
    if (tls_options & _HTTP_TLS_ALLOW_SSL3)
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER | SP_PROT_SSL3_SERVER;
    else
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER;
  }
  else
  {
    if (tls_options & _HTTP_TLS_ALLOW_SSL3)
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_SSL3_CLIENT;
    else
      SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT;
  }
#endif /* SP_PROT_TLS1_2_SERVER */

  /* TODO: Support _HTTP_TLS_ALLOW_RC4 and _HTTP_TLS_ALLOW_DH options; right now we'll rely on Windows registry to enable/disable RC4/DH... */

 /*
  * Create an SSPI credential.
  */

  Status = AcquireCredentialsHandle(NULL, UNISP_NAME, http->mode == _HTTP_MODE_SERVER ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND, NULL, &SchannelCred, NULL, NULL, &sspi->creds, &tsExpiry);
  if (Status != SEC_E_OK)
  {
    DEBUG_printf(("5http_sspi_find_credentials: AcquireCredentialsHandle failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), Status)));
    ok = FALSE;
    goto cleanup;
  }

cleanup:

 /*
  * Cleanup
  */

  if (storedContext)
    CertFreeCertificateContext(storedContext);

  if (p)
    free(p);

  if (store)
    CertCloseStore(store, 0);

  if (hProv)
    CryptReleaseContext(hProv, 0);

  return (ok);
}


/*
 * 'http_sspi_free()' - Close a connection and free resources.
 */

static void
http_sspi_free(_http_sspi_t *sspi)	/* I - SSPI data */
{
  if (!sspi)
    return;

  if (sspi->contextInitialized)
    DeleteSecurityContext(&sspi->context);

  if (sspi->decryptBuffer)
    free(sspi->decryptBuffer);

  if (sspi->readBuffer)
    free(sspi->readBuffer);

  if (sspi->writeBuffer)
    free(sspi->writeBuffer);

  if (sspi->localCert)
    CertFreeCertificateContext(sspi->localCert);

  if (sspi->remoteCert)
    CertFreeCertificateContext(sspi->remoteCert);

  free(sspi);
}


/*
 * 'http_sspi_make_credentials()' - Create a TLS certificate in the system store.
 */

static BOOL				/* O - 1 on success, 0 on failure */
http_sspi_make_credentials(
    _http_sspi_t *sspi,			/* I - SSPI data */
    const LPWSTR container,		/* I - Cert container name */
    const char   *common_name,		/* I - Common name of certificate */
    _http_mode_t mode,			/* I - Client or server? */
    int          years)			/* I - Years until expiration */
{
  HCERTSTORE	store = NULL;		/* Certificate store */
  PCCERT_CONTEXT storedContext = NULL;	/* Context created from the store */
  PCCERT_CONTEXT createdContext = NULL;	/* Context created by us */
  DWORD		dwSize = 0; 		/* 32 bit size */
  PBYTE		p = NULL;		/* Temporary storage */
  HCRYPTPROV	hProv = (HCRYPTPROV)NULL;
					/* Handle to a CSP */
  CERT_NAME_BLOB sib;			/* Arbitrary array of bytes */
  SCHANNEL_CRED	SchannelCred;		/* Schannel credential data */
  TimeStamp	tsExpiry;		/* Time stamp */
  SECURITY_STATUS Status;		/* Status */
  HCRYPTKEY	hKey = (HCRYPTKEY)NULL;	/* Handle to crypto key */
  CRYPT_KEY_PROV_INFO kpi;		/* Key container info */
  SYSTEMTIME	et;			/* System time */
  CERT_EXTENSIONS exts;			/* Array of cert extensions */
  CRYPT_KEY_PROV_INFO ckp;		/* Handle to crypto key */
  BOOL		ok = TRUE;		/* Return value */


  DEBUG_printf(("4http_sspi_make_credentials(sspi=%p, container=%p, common_name=\"%s\", mode=%d, years=%d)", sspi, container, common_name, mode, years));

  if (!CryptAcquireContextW(&hProv, (LPWSTR)container, MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET))
  {
    if (GetLastError() == NTE_EXISTS)
    {
      if (!CryptAcquireContextW(&hProv, (LPWSTR)container, MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
      {
        DEBUG_printf(("5http_sspi_make_credentials: CryptAcquireContext failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
        ok = FALSE;
        goto cleanup;
      }
    }
  }

  store = CertOpenStore(CERT_STORE_PROV_SYSTEM, X509_ASN_ENCODING|PKCS_7_ASN_ENCODING, hProv, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_NO_CRYPT_RELEASE_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, L"MY");

  if (!store)
  {
    DEBUG_printf(("5http_sspi_make_credentials: CertOpenSystemStore failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  dwSize = 0;

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, NULL, &dwSize, NULL))
  {
    DEBUG_printf(("5http_sspi_make_credentials: CertStrToName failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  p = (PBYTE)malloc(dwSize);

  if (!p)
  {
    DEBUG_printf(("5http_sspi_make_credentials: malloc failed for %d bytes", dwSize));
    ok = FALSE;
    goto cleanup;
  }

  if (!CertStrToName(X509_ASN_ENCODING, common_name, CERT_OID_NAME_STR, NULL, p, &dwSize, NULL))
  {
    DEBUG_printf(("5http_sspi_make_credentials: CertStrToName failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

 /*
  * Create a private key and self-signed certificate...
  */

  if (!CryptGenKey(hProv, AT_KEYEXCHANGE, CRYPT_EXPORTABLE, &hKey))
  {
    DEBUG_printf(("5http_sspi_make_credentials: CryptGenKey failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  ZeroMemory(&kpi, sizeof(kpi));
  kpi.pwszContainerName = (LPWSTR)container;
  kpi.pwszProvName      = MS_DEF_PROV_W;
  kpi.dwProvType        = PROV_RSA_FULL;
  kpi.dwFlags           = CERT_SET_KEY_CONTEXT_PROP_ID;
  kpi.dwKeySpec         = AT_KEYEXCHANGE;

  GetSystemTime(&et);
  et.wYear += years;

  ZeroMemory(&exts, sizeof(exts));

  createdContext = CertCreateSelfSignCertificate(hProv, &sib, 0, &kpi, NULL, NULL, &et, &exts);

  if (!createdContext)
  {
    DEBUG_printf(("5http_sspi_make_credentials: CertCreateSelfSignCertificate failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

 /*
  * Add the created context to the named store, and associate it with the named
  * container...
  */

  if (!CertAddCertificateContextToStore(store, createdContext, CERT_STORE_ADD_REPLACE_EXISTING, &storedContext))
  {
    DEBUG_printf(("5http_sspi_make_credentials: CertAddCertificateContextToStore failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

  ZeroMemory(&ckp, sizeof(ckp));
  ckp.pwszContainerName = (LPWSTR) container;
  ckp.pwszProvName      = MS_DEF_PROV_W;
  ckp.dwProvType        = PROV_RSA_FULL;
  ckp.dwFlags           = CRYPT_MACHINE_KEYSET;
  ckp.dwKeySpec         = AT_KEYEXCHANGE;

  if (!CertSetCertificateContextProperty(storedContext, CERT_KEY_PROV_INFO_PROP_ID, 0, &ckp))
  {
    DEBUG_printf(("5http_sspi_make_credentials: CertSetCertificateContextProperty failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), GetLastError())));
    ok = FALSE;
    goto cleanup;
  }

 /*
  * Get a handle to use the certificate...
  */

  ZeroMemory(&SchannelCred, sizeof(SchannelCred));

  SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
  SchannelCred.cCreds    = 1;
  SchannelCred.paCred    = &storedContext;

 /*
  * SSPI doesn't seem to like it if grbitEnabledProtocols is set for a client.
  */

  if (mode == _HTTP_MODE_SERVER)
    SchannelCred.grbitEnabledProtocols = SP_PROT_SSL3TLS1;

 /*
  * Create an SSPI credential.
  */

  Status = AcquireCredentialsHandle(NULL, UNISP_NAME, mode == _HTTP_MODE_SERVER ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND, NULL, &SchannelCred, NULL, NULL, &sspi->creds, &tsExpiry);
  if (Status != SEC_E_OK)
  {
    DEBUG_printf(("5http_sspi_make_credentials: AcquireCredentialsHandle failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), Status)));
    ok = FALSE;
    goto cleanup;
  }

cleanup:

 /*
  * Cleanup
  */

  if (hKey)
    CryptDestroyKey(hKey);

  if (createdContext)
    CertFreeCertificateContext(createdContext);

  if (storedContext)
    CertFreeCertificateContext(storedContext);

  if (p)
    free(p);

  if (store)
    CertCloseStore(store, 0);

  if (hProv)
    CryptReleaseContext(hProv, 0);

  return (ok);
}


/*
 * 'http_sspi_server()' - Negotiate a TLS connection as a server.
 */

static int				/* O - 0 on success, -1 on failure */
http_sspi_server(http_t     *http,	/* I - HTTP connection */
                 const char *hostname)	/* I - Hostname of server */
{
  _http_sspi_t	*sspi = http->tls;	/* I - SSPI data */
  char		common_name[512];	/* Common name for cert */
  DWORD		dwSSPIFlags;		/* SSL connection attributes we want */
  DWORD		dwSSPIOutFlags;		/* SSL connection attributes we got */
  TimeStamp	tsExpiry;		/* Time stamp */
  SECURITY_STATUS scRet;		/* SSPI Status */
  SecBufferDesc	inBuffer;		/* Array of SecBuffer structs */
  SecBuffer	inBuffers[2];		/* Security package buffer */
  SecBufferDesc	outBuffer;		/* Array of SecBuffer structs */
  SecBuffer	outBuffers[1];		/* Security package buffer */
  int		num = 0;		/* 32 bit status value */
  BOOL		fInitContext = TRUE;	/* Has the context been init'd? */
  int		ret = 0;		/* Return value */


  DEBUG_printf(("4http_sspi_server(http=%p, hostname=\"%s\")", http, hostname));

  dwSSPIFlags = ASC_REQ_SEQUENCE_DETECT  |
                ASC_REQ_REPLAY_DETECT    |
                ASC_REQ_CONFIDENTIALITY  |
                ASC_REQ_EXTENDED_ERROR   |
                ASC_REQ_ALLOCATE_MEMORY  |
                ASC_REQ_STREAM;

  sspi->decryptBufferUsed = 0;

 /*
  * Lookup the server certificate...
  */

  snprintf(common_name, sizeof(common_name), "CN=%s", hostname);

  if (!http_sspi_find_credentials(http, L"ServerContainer", common_name))
    if (!http_sspi_make_credentials(http->tls, L"ServerContainer", common_name, _HTTP_MODE_SERVER, 10))
    {
      DEBUG_puts("5http_sspi_server: Unable to get server credentials.");
      return (-1);
    }

 /*
  * Set OutBuffer for AcceptSecurityContext call
  */

  outBuffer.cBuffers  = 1;
  outBuffer.pBuffers  = outBuffers;
  outBuffer.ulVersion = SECBUFFER_VERSION;

  scRet = SEC_I_CONTINUE_NEEDED;

  while (scRet == SEC_I_CONTINUE_NEEDED    ||
         scRet == SEC_E_INCOMPLETE_MESSAGE ||
         scRet == SEC_I_INCOMPLETE_CREDENTIALS)
  {
    if (sspi->decryptBufferUsed == 0 || scRet == SEC_E_INCOMPLETE_MESSAGE)
    {
      if (sspi->decryptBufferLength <= sspi->decryptBufferUsed)
      {
	BYTE *temp;			/* New buffer */

	if (sspi->decryptBufferLength >= 262144)
	{
	  WSASetLastError(E_OUTOFMEMORY);
	  DEBUG_puts("5http_sspi_server: Decryption buffer too large (>256k)");
	  return (-1);
	}

	if ((temp = realloc(sspi->decryptBuffer, sspi->decryptBufferLength + 4096)) == NULL)
	{
	  DEBUG_printf(("5http_sspi_server: Unable to allocate %d byte buffer.", sspi->decryptBufferLength + 4096));
	  WSASetLastError(E_OUTOFMEMORY);
	  return (-1);
	}

	sspi->decryptBufferLength += 4096;
	sspi->decryptBuffer       = temp;
      }

      for (;;)
      {
        num = recv(http->fd, sspi->decryptBuffer + sspi->decryptBufferUsed, (int)(sspi->decryptBufferLength - sspi->decryptBufferUsed), 0);

        if (num == -1 && WSAGetLastError() == WSAEWOULDBLOCK)
          Sleep(1);
        else
          break;
      }

      if (num < 0)
      {
        DEBUG_printf(("5http_sspi_server: recv failed: %d", WSAGetLastError()));
        return (-1);
      }
      else if (num == 0)
      {
        DEBUG_puts("5http_sspi_server: client disconnected");
        return (-1);
      }

      DEBUG_printf(("5http_sspi_server: received %d (handshake) bytes from client.", num));
      sspi->decryptBufferUsed += num;
    }

   /*
    * InBuffers[1] is for getting extra data that SSPI/SCHANNEL doesn't process
    * on this run around the loop.
    */

    inBuffers[0].pvBuffer   = sspi->decryptBuffer;
    inBuffers[0].cbBuffer   = (unsigned long)sspi->decryptBufferUsed;
    inBuffers[0].BufferType = SECBUFFER_TOKEN;

    inBuffers[1].pvBuffer   = NULL;
    inBuffers[1].cbBuffer   = 0;
    inBuffers[1].BufferType = SECBUFFER_EMPTY;

    inBuffer.cBuffers       = 2;
    inBuffer.pBuffers       = inBuffers;
    inBuffer.ulVersion      = SECBUFFER_VERSION;

   /*
    * Initialize these so if we fail, pvBuffer contains NULL, so we don't try to
    * free random garbage at the quit.
    */

    outBuffers[0].pvBuffer   = NULL;
    outBuffers[0].BufferType = SECBUFFER_TOKEN;
    outBuffers[0].cbBuffer   = 0;

    scRet = AcceptSecurityContext(&sspi->creds, (fInitContext?NULL:&sspi->context), &inBuffer, dwSSPIFlags, SECURITY_NATIVE_DREP, (fInitContext?&sspi->context:NULL), &outBuffer, &dwSSPIOutFlags, &tsExpiry);

    fInitContext = FALSE;

    if (scRet == SEC_E_OK              ||
        scRet == SEC_I_CONTINUE_NEEDED ||
        (FAILED(scRet) && ((dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR) != 0)))
    {
      if (outBuffers[0].cbBuffer && outBuffers[0].pvBuffer)
      {
       /*
        * Send response to server if there is one.
        */

        num = send(http->fd, outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0);

        if (num <= 0)
        {
          DEBUG_printf(("5http_sspi_server: handshake send failed: %d", WSAGetLastError()));
	  return (-1);
        }

        DEBUG_printf(("5http_sspi_server: sent %d handshake bytes to client.", outBuffers[0].cbBuffer));

        FreeContextBuffer(outBuffers[0].pvBuffer);
        outBuffers[0].pvBuffer = NULL;
      }
    }

    if (scRet == SEC_E_OK)
    {
     /*
      * If there's extra data then save it for next time we go to decrypt.
      */

      if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
      {
        memcpy(sspi->decryptBuffer, (LPBYTE)(sspi->decryptBuffer + sspi->decryptBufferUsed - inBuffers[1].cbBuffer), inBuffers[1].cbBuffer);
        sspi->decryptBufferUsed = inBuffers[1].cbBuffer;
      }
      else
      {
        sspi->decryptBufferUsed = 0;
      }
      break;
    }
    else if (FAILED(scRet) && scRet != SEC_E_INCOMPLETE_MESSAGE)
    {
      DEBUG_printf(("5http_sspi_server: AcceptSecurityContext failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
      ret = -1;
      break;
    }

    if (scRet != SEC_E_INCOMPLETE_MESSAGE &&
        scRet != SEC_I_INCOMPLETE_CREDENTIALS)
    {
      if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
      {
        memcpy(sspi->decryptBuffer, (LPBYTE)(sspi->decryptBuffer + sspi->decryptBufferUsed - inBuffers[1].cbBuffer), inBuffers[1].cbBuffer);
        sspi->decryptBufferUsed = inBuffers[1].cbBuffer;
      }
      else
      {
        sspi->decryptBufferUsed = 0;
      }
    }
  }

  if (!ret)
  {
    sspi->contextInitialized = TRUE;

   /*
    * Find out how big the header will be:
    */

    scRet = QueryContextAttributes(&sspi->context, SECPKG_ATTR_STREAM_SIZES, &sspi->streamSizes);

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("5http_sspi_server: QueryContextAttributes failed: %s", http_sspi_strerror(sspi->error, sizeof(sspi->error), scRet)));
      ret = -1;
    }
  }

  return (ret);
}


/*
 * 'http_sspi_strerror()' - Return a string for the specified error code.
 */

static const char *			/* O - String for error */
http_sspi_strerror(char   *buffer,	/* I - Error message buffer */
                   size_t bufsize,	/* I - Size of buffer */
                   DWORD  code)		/* I - Error code */
{
  if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, 0, buffer, bufsize, NULL))
  {
   /*
    * Strip trailing CR + LF...
    */

    char	*ptr;			/* Pointer into error message */

    for (ptr = buffer + strlen(buffer) - 1; ptr >= buffer; ptr --)
      if (*ptr == '\n' || *ptr == '\r')
        *ptr = '\0';
      else
        break;
  }
  else
    snprintf(buffer, bufsize, "Unknown error %x", code);

  return (buffer);
}


/*
 * 'http_sspi_verify()' - Verify a certificate.
 */

static DWORD				/* O - Error code (0 == No error) */
http_sspi_verify(
    PCCERT_CONTEXT cert,		/* I - Server certificate */
    const char     *common_name,	/* I - Common name */
    DWORD          dwCertFlags)		/* I - Verification flags */
{
  HTTPSPolicyCallbackData httpsPolicy;	/* HTTPS Policy Struct */
  CERT_CHAIN_POLICY_PARA policyPara;	/* Cert chain policy parameters */
  CERT_CHAIN_POLICY_STATUS policyStatus;/* Cert chain policy status */
  CERT_CHAIN_PARA	chainPara;	/* Used for searching and matching criteria */
  PCCERT_CHAIN_CONTEXT	chainContext = NULL;
					/* Certificate chain */
  PWSTR			commonNameUnicode = NULL;
					/* Unicode common name */
  LPSTR			rgszUsages[] = { szOID_PKIX_KP_SERVER_AUTH,
                                         szOID_SERVER_GATED_CRYPTO,
                                         szOID_SGC_NETSCAPE };
					/* How are we using this certificate? */
  DWORD			cUsages = sizeof(rgszUsages) / sizeof(LPSTR);
					/* Number of ites in rgszUsages */
  DWORD			count;		/* 32 bit count variable */
  DWORD			status;		/* Return value */
#ifdef DEBUG
  char			error[1024];	/* Error message string */
#endif /* DEBUG */


  if (!cert)
    return (SEC_E_WRONG_PRINCIPAL);

 /*
  * Convert common name to Unicode.
  */

  if (!common_name || !*common_name)
    return (SEC_E_WRONG_PRINCIPAL);

  count             = MultiByteToWideChar(CP_ACP, 0, common_name, -1, NULL, 0);
  commonNameUnicode = LocalAlloc(LMEM_FIXED, count * sizeof(WCHAR));
  if (!commonNameUnicode)
    return (SEC_E_INSUFFICIENT_MEMORY);

  if (!MultiByteToWideChar(CP_ACP, 0, common_name, -1, commonNameUnicode, count))
  {
    LocalFree(commonNameUnicode);
    return (SEC_E_WRONG_PRINCIPAL);
  }

 /*
  * Build certificate chain.
  */

  ZeroMemory(&chainPara, sizeof(chainPara));

  chainPara.cbSize					= sizeof(chainPara);
  chainPara.RequestedUsage.dwType			= USAGE_MATCH_TYPE_OR;
  chainPara.RequestedUsage.Usage.cUsageIdentifier	= cUsages;
  chainPara.RequestedUsage.Usage.rgpszUsageIdentifier	= rgszUsages;

  if (!CertGetCertificateChain(NULL, cert, NULL, cert->hCertStore, &chainPara, 0, NULL, &chainContext))
  {
    status = GetLastError();

    DEBUG_printf(("CertGetCertificateChain returned: %s", http_sspi_strerror(error, sizeof(error), status)));

    LocalFree(commonNameUnicode);
    return (status);
  }

 /*
  * Validate certificate chain.
  */

  ZeroMemory(&httpsPolicy, sizeof(HTTPSPolicyCallbackData));
  httpsPolicy.cbStruct		= sizeof(HTTPSPolicyCallbackData);
  httpsPolicy.dwAuthType	= AUTHTYPE_SERVER;
  httpsPolicy.fdwChecks		= dwCertFlags;
  httpsPolicy.pwszServerName	= commonNameUnicode;

  memset(&policyPara, 0, sizeof(policyPara));
  policyPara.cbSize		= sizeof(policyPara);
  policyPara.pvExtraPolicyPara	= &httpsPolicy;

  memset(&policyStatus, 0, sizeof(policyStatus));
  policyStatus.cbSize = sizeof(policyStatus);

  if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chainContext, &policyPara, &policyStatus))
  {
    status = GetLastError();

    DEBUG_printf(("CertVerifyCertificateChainPolicy returned %s", http_sspi_strerror(error, sizeof(error), status)));
  }
  else if (policyStatus.dwError)
    status = policyStatus.dwError;
  else
    status = SEC_E_OK;

  if (chainContext)
    CertFreeCertificateChain(chainContext);

  if (commonNameUnicode)
    LocalFree(commonNameUnicode);

  return (status);
}


/*
 * End of "$Id: tls-sspi.c 12647 2015-05-20 18:37:52Z msweet $".
 */
