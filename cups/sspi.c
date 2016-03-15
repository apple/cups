/*
 * "$Id: sspi.c 11760 2014-03-28 12:58:24Z msweet $"
 *
 * Windows SSPI SSL implementation for CUPS.
 *
 * Copyright 2010-2014 by Apple Inc.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include "sspi-private.h"
#include "debug-private.h"


/* required to link this library for certificate functions */
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Ws2_32.lib")


#if !defined(SECURITY_FLAG_IGNORE_UNKNOWN_CA)
#  define SECURITY_FLAG_IGNORE_UNKNOWN_CA         0x00000100 /* Untrusted root */
#endif

#if !defined(SECURITY_FLAG_IGNORE_CERT_DATE_INVALID)
#  define SECURITY_FLAG_IGNORE_CERT_DATE_INVALID  0x00002000 /* Expired X509 Cert. */
#endif

static DWORD sspi_verify_certificate(PCCERT_CONTEXT  serverCert,
                                     const CHAR      *serverName,
                                     DWORD           dwCertFlags);


/*
 * 'sspi_alloc()' - Allocate SSPI ssl object
 */
_sspi_struct_t*				/* O  - New SSPI/SSL object */
_sspiAlloc(void)
{
  _sspi_struct_t *conn = calloc(sizeof(_sspi_struct_t), 1);

  if (conn)
    conn->sock = INVALID_SOCKET;

  return (conn);
}


/*
 * '_sspiGetCredentials()' - Retrieve an SSL/TLS certificate from the system store
 *                              If one cannot be found, one is created.
 */
BOOL					/* O - 1 on success, 0 on failure */
_sspiGetCredentials(_sspi_struct_t *conn,
					/* I - Client connection */
                    const LPWSTR   container,
					/* I - Cert container name */
                    const TCHAR    *cn,	/* I - Common name of certificate */
                    BOOL           isServer)
					/* I - Is caller a server? */
{
  HCERTSTORE		store = NULL;	/* Certificate store */
  PCCERT_CONTEXT	storedContext = NULL;
					/* Context created from the store */
  PCCERT_CONTEXT	createdContext = NULL;
					/* Context created by us */
  DWORD			dwSize = 0;	/* 32 bit size */
  PBYTE			p = NULL;	/* Temporary storage */
  HCRYPTPROV		hProv = (HCRYPTPROV) NULL;
					/* Handle to a CSP */
  CERT_NAME_BLOB	sib;		/* Arbitrary array of bytes */
  SCHANNEL_CRED		SchannelCred;	/* Schannel credential data */
  TimeStamp		tsExpiry;	/* Time stamp */
  SECURITY_STATUS	Status;		/* Status */
  HCRYPTKEY		hKey = (HCRYPTKEY) NULL;
					/* Handle to crypto key */
  CRYPT_KEY_PROV_INFO	kpi;		/* Key container info */
  SYSTEMTIME		et;		/* System time */
  CERT_EXTENSIONS	exts;		/* Array of cert extensions */
  CRYPT_KEY_PROV_INFO	ckp;		/* Handle to crypto key */
  BOOL			ok = TRUE;	/* Return value */


  DEBUG_printf(("_sspiGetCredentials(conn=%p, container=%p, cn=\"%s\", isServer=%d)", conn, container, cn, isServer));

  if (!conn)
    return (FALSE);
  if (!cn)
    return (FALSE);

  if (!CryptAcquireContextW(&hProv, (LPWSTR) container, MS_DEF_PROV_W,
                           PROV_RSA_FULL,
                           CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET))
  {
    if (GetLastError() == NTE_EXISTS)
    {
      if (!CryptAcquireContextW(&hProv, (LPWSTR) container, MS_DEF_PROV_W,
                               PROV_RSA_FULL, CRYPT_MACHINE_KEYSET))
      {
        DEBUG_printf(("_sspiGetCredentials: CryptAcquireContext failed: %x\n",
                      GetLastError()));
        ok = FALSE;
        goto cleanup;
      }
    }
  }

  store = CertOpenStore(CERT_STORE_PROV_SYSTEM,
                        X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,
                        hProv,
                        CERT_SYSTEM_STORE_LOCAL_MACHINE |
                        CERT_STORE_NO_CRYPT_RELEASE_FLAG |
                        CERT_STORE_OPEN_EXISTING_FLAG,
                        L"MY");

  if (!store)
  {
    DEBUG_printf(("_sspiGetCredentials: CertOpenSystemStore failed: %x\n",
                  GetLastError()));
    ok = FALSE;
    goto cleanup;
  }

  dwSize = 0;

  if (!CertStrToName(X509_ASN_ENCODING, cn, CERT_OID_NAME_STR,
                     NULL, NULL, &dwSize, NULL))
  {
    DEBUG_printf(("_sspiGetCredentials: CertStrToName failed: %x\n",
                   GetLastError()));
    ok = FALSE;
    goto cleanup;
  }

  p = (PBYTE) malloc(dwSize);

  if (!p)
  {
    DEBUG_printf(("_sspiGetCredentials: malloc failed for %d bytes", dwSize));
    ok = FALSE;
    goto cleanup;
  }

  if (!CertStrToName(X509_ASN_ENCODING, cn, CERT_OID_NAME_STR, NULL,
                     p, &dwSize, NULL))
  {
    DEBUG_printf(("_sspiGetCredentials: CertStrToName failed: %x",
                 GetLastError()));
    ok = FALSE;
    goto cleanup;
  }

  sib.cbData = dwSize;
  sib.pbData = p;

  storedContext = CertFindCertificateInStore(store, X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,
                                             0, CERT_FIND_SUBJECT_NAME, &sib, NULL);

  if (!storedContext)
  {
   /*
    * If we couldn't find the context, then we'll
    * create a new one
    */
    if (!CryptGenKey(hProv, AT_KEYEXCHANGE, CRYPT_EXPORTABLE, &hKey))
    {
      DEBUG_printf(("_sspiGetCredentials: CryptGenKey failed: %x",
                    GetLastError()));
      ok = FALSE;
      goto cleanup;
    }

    ZeroMemory(&kpi, sizeof(kpi));
    kpi.pwszContainerName = (LPWSTR) container;
    kpi.pwszProvName = MS_DEF_PROV_W;
    kpi.dwProvType = PROV_RSA_FULL;
    kpi.dwFlags = CERT_SET_KEY_CONTEXT_PROP_ID;
    kpi.dwKeySpec = AT_KEYEXCHANGE;

    GetSystemTime(&et);
    et.wYear += 10;

    ZeroMemory(&exts, sizeof(exts));

    createdContext = CertCreateSelfSignCertificate(hProv, &sib, 0, &kpi, NULL, NULL,
                                                   &et, &exts);

    if (!createdContext)
    {
      DEBUG_printf(("_sspiGetCredentials: CertCreateSelfSignCertificate failed: %x",
                   GetLastError()));
      ok = FALSE;
      goto cleanup;
    }

    if (!CertAddCertificateContextToStore(store, createdContext,
                                          CERT_STORE_ADD_REPLACE_EXISTING,
                                          &storedContext))
    {
      DEBUG_printf(("_sspiGetCredentials: CertAddCertificateContextToStore failed: %x",
                    GetLastError()));
      ok = FALSE;
      goto cleanup;
    }

    ZeroMemory(&ckp, sizeof(ckp));
    ckp.pwszContainerName = (LPWSTR) container;
    ckp.pwszProvName = MS_DEF_PROV_W;
    ckp.dwProvType = PROV_RSA_FULL;
    ckp.dwFlags = CRYPT_MACHINE_KEYSET;
    ckp.dwKeySpec = AT_KEYEXCHANGE;

    if (!CertSetCertificateContextProperty(storedContext,
                                           CERT_KEY_PROV_INFO_PROP_ID,
                                           0, &ckp))
    {
      DEBUG_printf(("_sspiGetCredentials: CertSetCertificateContextProperty failed: %x",
                    GetLastError()));
      ok = FALSE;
      goto cleanup;
    }
  }

  ZeroMemory(&SchannelCred, sizeof(SchannelCred));

  SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
  SchannelCred.cCreds = 1;
  SchannelCred.paCred = &storedContext;

 /*
  * SSPI doesn't seem to like it if grbitEnabledProtocols
  * is set for a client
  */
  if (isServer)
    SchannelCred.grbitEnabledProtocols = SP_PROT_SSL3TLS1;

 /*
  * Create an SSPI credential.
  */
  Status = AcquireCredentialsHandle(NULL, UNISP_NAME,
                                    isServer ? SECPKG_CRED_INBOUND:SECPKG_CRED_OUTBOUND,
                                    NULL, &SchannelCred, NULL, NULL, &conn->creds,
                                    &tsExpiry);
  if (Status != SEC_E_OK)
  {
    DEBUG_printf(("_sspiGetCredentials: AcquireCredentialsHandle failed: %x", Status));
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
 * '_sspiConnect()' - Make an SSL connection. This function
 *                    assumes a TCP/IP connection has already
 *                    been successfully made
 */
BOOL					/* O - 1 on success, 0 on failure */
_sspiConnect(_sspi_struct_t *conn,	/* I - Client connection */
             const CHAR *hostname)	/* I - Server hostname */
{
  PCCERT_CONTEXT	serverCert;	/* Server certificate */
  DWORD			dwSSPIFlags;	/* SSL connection attributes we want */
  DWORD			dwSSPIOutFlags;	/* SSL connection attributes we got */
  TimeStamp		tsExpiry;	/* Time stamp */
  SECURITY_STATUS	scRet;		/* Status */
  DWORD			cbData;		/* Data count */
  SecBufferDesc		inBuffer;	/* Array of SecBuffer structs */
  SecBuffer		inBuffers[2];	/* Security package buffer */
  SecBufferDesc		outBuffer;	/* Array of SecBuffer structs */
  SecBuffer		outBuffers[1];	/* Security package buffer */
  BOOL			ok = TRUE;	/* Return value */

  serverCert  = NULL;

  dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT   |
                ISC_REQ_REPLAY_DETECT     |
                ISC_REQ_CONFIDENTIALITY   |
                ISC_RET_EXTENDED_ERROR    |
                ISC_REQ_ALLOCATE_MEMORY   |
                ISC_REQ_STREAM;

 /*
  * Initiate a ClientHello message and generate a token.
  */
  outBuffers[0].pvBuffer   = NULL;
  outBuffers[0].BufferType = SECBUFFER_TOKEN;
  outBuffers[0].cbBuffer   = 0;

  outBuffer.cBuffers = 1;
  outBuffer.pBuffers = outBuffers;
  outBuffer.ulVersion = SECBUFFER_VERSION;

  scRet = InitializeSecurityContext(&conn->creds, NULL, TEXT(""), dwSSPIFlags,
                                    0, SECURITY_NATIVE_DREP, NULL, 0, &conn->context,
                                    &outBuffer, &dwSSPIOutFlags, &tsExpiry);

  if (scRet != SEC_I_CONTINUE_NEEDED)
  {
    DEBUG_printf(("_sspiConnect: InitializeSecurityContext(1) failed: %x", scRet));
    ok = FALSE;
    goto cleanup;
  }

 /*
  * Send response to server if there is one.
  */
  if (outBuffers[0].cbBuffer && outBuffers[0].pvBuffer)
  {
    cbData = send(conn->sock, outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0);

    if ((cbData == SOCKET_ERROR) || !cbData)
    {
      DEBUG_printf(("_sspiConnect: send failed: %d", WSAGetLastError()));
      FreeContextBuffer(outBuffers[0].pvBuffer);
      DeleteSecurityContext(&conn->context);
      ok = FALSE;
      goto cleanup;
    }

    DEBUG_printf(("_sspiConnect: %d bytes of handshake data sent", cbData));

   /*
    * Free output buffer.
    */
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

  conn->decryptBufferUsed = 0;

 /*
  * Loop until the handshake is finished or an error occurs.
  */
  scRet = SEC_I_CONTINUE_NEEDED;

  while(scRet == SEC_I_CONTINUE_NEEDED        ||
        scRet == SEC_E_INCOMPLETE_MESSAGE     ||
        scRet == SEC_I_INCOMPLETE_CREDENTIALS)
  {
    if ((conn->decryptBufferUsed == 0) || (scRet == SEC_E_INCOMPLETE_MESSAGE))
    {
      if (conn->decryptBufferLength <= conn->decryptBufferUsed)
      {
        conn->decryptBufferLength += 4096;
        conn->decryptBuffer = (BYTE*) realloc(conn->decryptBuffer, conn->decryptBufferLength);

        if (!conn->decryptBuffer)
        {
          DEBUG_printf(("_sspiConnect: unable to allocate %d byte decrypt buffer",
                        conn->decryptBufferLength));
          SetLastError(E_OUTOFMEMORY);
          ok = FALSE;
          goto cleanup;
        }
      }

      cbData = recv(conn->sock, conn->decryptBuffer + conn->decryptBufferUsed,
                    (int) (conn->decryptBufferLength - conn->decryptBufferUsed), 0);

      if (cbData == SOCKET_ERROR)
      {
        DEBUG_printf(("_sspiConnect: recv failed: %d", WSAGetLastError()));
        ok = FALSE;
        goto cleanup;
      }
      else if (cbData == 0)
      {
        DEBUG_printf(("_sspiConnect: server unexpectedly disconnected"));
        ok = FALSE;
        goto cleanup;
      }

      DEBUG_printf(("_sspiConnect: %d bytes of handshake data received",
                    cbData));

      conn->decryptBufferUsed += cbData;
    }

   /*
    * Set up the input buffers. Buffer 0 is used to pass in data
    * received from the server. Schannel will consume some or all
    * of this. Leftover data (if any) will be placed in buffer 1 and
    * given a buffer type of SECBUFFER_EXTRA.
    */
    inBuffers[0].pvBuffer   = conn->decryptBuffer;
    inBuffers[0].cbBuffer   = (unsigned long) conn->decryptBufferUsed;
    inBuffers[0].BufferType = SECBUFFER_TOKEN;

    inBuffers[1].pvBuffer   = NULL;
    inBuffers[1].cbBuffer   = 0;
    inBuffers[1].BufferType = SECBUFFER_EMPTY;

    inBuffer.cBuffers       = 2;
    inBuffer.pBuffers       = inBuffers;
    inBuffer.ulVersion      = SECBUFFER_VERSION;

   /*
    * Set up the output buffers. These are initialized to NULL
    * so as to make it less likely we'll attempt to free random
    * garbage later.
    */
    outBuffers[0].pvBuffer  = NULL;
    outBuffers[0].BufferType= SECBUFFER_TOKEN;
    outBuffers[0].cbBuffer  = 0;

    outBuffer.cBuffers      = 1;
    outBuffer.pBuffers      = outBuffers;
    outBuffer.ulVersion     = SECBUFFER_VERSION;

   /*
    * Call InitializeSecurityContext.
    */
    scRet = InitializeSecurityContext(&conn->creds, &conn->context, NULL, dwSSPIFlags,
                                      0, SECURITY_NATIVE_DREP, &inBuffer, 0, NULL,
                                      &outBuffer, &dwSSPIOutFlags, &tsExpiry);

   /*
    * If InitializeSecurityContext was successful (or if the error was
    * one of the special extended ones), send the contends of the output
    * buffer to the server.
    */
    if (scRet == SEC_E_OK                ||
        scRet == SEC_I_CONTINUE_NEEDED   ||
        FAILED(scRet) && (dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR))
    {
      if (outBuffers[0].cbBuffer && outBuffers[0].pvBuffer)
      {
        cbData = send(conn->sock, outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0);

        if ((cbData == SOCKET_ERROR) || !cbData)
        {
          DEBUG_printf(("_sspiConnect: send failed: %d", WSAGetLastError()));
          FreeContextBuffer(outBuffers[0].pvBuffer);
          DeleteSecurityContext(&conn->context);
          ok = FALSE;
          goto cleanup;
        }

        DEBUG_printf(("_sspiConnect: %d bytes of handshake data sent", cbData));

       /*
        * Free output buffer.
        */
        FreeContextBuffer(outBuffers[0].pvBuffer);
        outBuffers[0].pvBuffer = NULL;
      }
    }

   /*
    * If InitializeSecurityContext returned SEC_E_INCOMPLETE_MESSAGE,
    * then we need to read more data from the server and try again.
    */
    if (scRet == SEC_E_INCOMPLETE_MESSAGE)
      continue;

   /*
    * If InitializeSecurityContext returned SEC_E_OK, then the
    * handshake completed successfully.
    */
    if (scRet == SEC_E_OK)
    {
     /*
      * If the "extra" buffer contains data, this is encrypted application
      * protocol layer stuff. It needs to be saved. The application layer
      * will later decrypt it with DecryptMessage.
      */
      DEBUG_printf(("_sspiConnect: Handshake was successful"));

      if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
      {
        if (conn->decryptBufferLength < inBuffers[1].cbBuffer)
        {
          conn->decryptBuffer = realloc(conn->decryptBuffer, inBuffers[1].cbBuffer);

          if (!conn->decryptBuffer)
          {
            DEBUG_printf(("_sspiConnect: unable to allocate %d bytes for decrypt buffer",
                          inBuffers[1].cbBuffer));
            SetLastError(E_OUTOFMEMORY);
            ok = FALSE;
            goto cleanup;
          }
        }

        memmove(conn->decryptBuffer,
                conn->decryptBuffer + (conn->decryptBufferUsed - inBuffers[1].cbBuffer),
                inBuffers[1].cbBuffer);

        conn->decryptBufferUsed = inBuffers[1].cbBuffer;

        DEBUG_printf(("_sspiConnect: %d bytes of app data was bundled with handshake data",
                      conn->decryptBufferUsed));
      }
      else
        conn->decryptBufferUsed = 0;

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
      DEBUG_printf(("_sspiConnect: InitializeSecurityContext(2) failed: %x", scRet));
      ok = FALSE;
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
      DEBUG_printf(("_sspiConnect: server requested client credentials"));
      ok = FALSE;
      break;
    }

   /*
    * Copy any leftover data from the "extra" buffer, and go around
    * again.
    */
    if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
    {
      memmove(conn->decryptBuffer,
              conn->decryptBuffer + (conn->decryptBufferUsed - inBuffers[1].cbBuffer),
              inBuffers[1].cbBuffer);

      conn->decryptBufferUsed = inBuffers[1].cbBuffer;
    }
    else
    {
      conn->decryptBufferUsed = 0;
    }
  }

  if (ok)
  {
    conn->contextInitialized = TRUE;

   /*
    * Get the server cert
    */
    scRet = QueryContextAttributes(&conn->context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, (VOID*) &serverCert );

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("_sspiConnect: QueryContextAttributes failed(SECPKG_ATTR_REMOTE_CERT_CONTEXT): %x", scRet));
      ok = FALSE;
      goto cleanup;
    }

    scRet = sspi_verify_certificate(serverCert, hostname, conn->certFlags);

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("_sspiConnect: sspi_verify_certificate failed: %x", scRet));
      ok = FALSE;
      goto cleanup;
    }

   /*
    * Find out how big the header/trailer will be:
    */
    scRet = QueryContextAttributes(&conn->context, SECPKG_ATTR_STREAM_SIZES, &conn->streamSizes);

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("_sspiConnect: QueryContextAttributes failed(SECPKG_ATTR_STREAM_SIZES): %x", scRet));
      ok = FALSE;
    }
  }

cleanup:

  if (serverCert)
    CertFreeCertificateContext(serverCert);

  return (ok);
}


/*
 * '_sspiAccept()' - Accept an SSL/TLS connection
 */
BOOL					/* O - 1 on success, 0 on failure */
_sspiAccept(_sspi_struct_t *conn)	/* I  - Client connection */
{
  DWORD			dwSSPIFlags;	/* SSL connection attributes we want */
  DWORD			dwSSPIOutFlags;	/* SSL connection attributes we got */
  TimeStamp		tsExpiry;	/* Time stamp */
  SECURITY_STATUS	scRet;		/* SSPI Status */
  SecBufferDesc		inBuffer;	/* Array of SecBuffer structs */
  SecBuffer		inBuffers[2];	/* Security package buffer */
  SecBufferDesc		outBuffer;	/* Array of SecBuffer structs */
  SecBuffer		outBuffers[1];	/* Security package buffer */
  DWORD			num = 0;	/* 32 bit status value */
  BOOL			fInitContext = TRUE;
					/* Has the context been init'd? */
  BOOL			ok = TRUE;	/* Return value */

  if (!conn)
    return (FALSE);

  dwSSPIFlags = ASC_REQ_SEQUENCE_DETECT  |
                ASC_REQ_REPLAY_DETECT    |
                ASC_REQ_CONFIDENTIALITY  |
                ASC_REQ_EXTENDED_ERROR   |
                ASC_REQ_ALLOCATE_MEMORY  |
                ASC_REQ_STREAM;

  conn->decryptBufferUsed = 0;

 /*
  * Set OutBuffer for AcceptSecurityContext call
  */
  outBuffer.cBuffers = 1;
  outBuffer.pBuffers = outBuffers;
  outBuffer.ulVersion = SECBUFFER_VERSION;

  scRet = SEC_I_CONTINUE_NEEDED;

  while (scRet == SEC_I_CONTINUE_NEEDED    ||
         scRet == SEC_E_INCOMPLETE_MESSAGE ||
         scRet == SEC_I_INCOMPLETE_CREDENTIALS)
  {
    if ((conn->decryptBufferUsed == 0) || (scRet == SEC_E_INCOMPLETE_MESSAGE))
    {
      if (conn->decryptBufferLength <= conn->decryptBufferUsed)
      {
        conn->decryptBufferLength += 4096;
        conn->decryptBuffer = (BYTE*) realloc(conn->decryptBuffer,
                                              conn->decryptBufferLength);

        if (!conn->decryptBuffer)
        {
          DEBUG_printf(("_sspiAccept: unable to allocate %d byte decrypt buffer",
                        conn->decryptBufferLength));
          ok = FALSE;
          goto cleanup;
        }
      }

      for (;;)
      {
        num = recv(conn->sock,
                   conn->decryptBuffer + conn->decryptBufferUsed,
                   (int)(conn->decryptBufferLength - conn->decryptBufferUsed),
                   0);

        if ((num == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK))
          Sleep(1);
        else
          break;
      }

      if (num == SOCKET_ERROR)
      {
        DEBUG_printf(("_sspiAccept: recv failed: %d", WSAGetLastError()));
        ok = FALSE;
        goto cleanup;
      }
      else if (num == 0)
      {
        DEBUG_printf(("_sspiAccept: client disconnected"));
        ok = FALSE;
        goto cleanup;
      }

      DEBUG_printf(("_sspiAccept: received %d (handshake) bytes from client",
                    num));
      conn->decryptBufferUsed += num;
    }

   /*
    * InBuffers[1] is for getting extra data that
    * SSPI/SCHANNEL doesn't proccess on this
    * run around the loop.
    */
    inBuffers[0].pvBuffer   = conn->decryptBuffer;
    inBuffers[0].cbBuffer   = (unsigned long) conn->decryptBufferUsed;
    inBuffers[0].BufferType = SECBUFFER_TOKEN;

    inBuffers[1].pvBuffer   = NULL;
    inBuffers[1].cbBuffer   = 0;
    inBuffers[1].BufferType = SECBUFFER_EMPTY;

    inBuffer.cBuffers       = 2;
    inBuffer.pBuffers       = inBuffers;
    inBuffer.ulVersion      = SECBUFFER_VERSION;

   /*
    * Initialize these so if we fail, pvBuffer contains NULL,
    * so we don't try to free random garbage at the quit
    */
    outBuffers[0].pvBuffer   = NULL;
    outBuffers[0].BufferType = SECBUFFER_TOKEN;
    outBuffers[0].cbBuffer   = 0;

    scRet = AcceptSecurityContext(&conn->creds, (fInitContext?NULL:&conn->context),
                                  &inBuffer, dwSSPIFlags, SECURITY_NATIVE_DREP,
                                  (fInitContext?&conn->context:NULL), &outBuffer,
                                  &dwSSPIOutFlags, &tsExpiry);

    fInitContext = FALSE;

    if (scRet == SEC_E_OK              ||
        scRet == SEC_I_CONTINUE_NEEDED ||
        (FAILED(scRet) && ((dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR) != 0)))
    {
      if (outBuffers[0].cbBuffer && outBuffers[0].pvBuffer)
      {
       /*
        * Send response to server if there is one
        */
        num = send(conn->sock, outBuffers[0].pvBuffer, outBuffers[0].cbBuffer, 0);

        if ((num == SOCKET_ERROR) || (num == 0))
        {
          DEBUG_printf(("_sspiAccept: handshake send failed: %d", WSAGetLastError()));
          ok = FALSE;
          goto cleanup;
        }

        DEBUG_printf(("_sspiAccept: send %d handshake bytes to client",
                     outBuffers[0].cbBuffer));

        FreeContextBuffer(outBuffers[0].pvBuffer);
        outBuffers[0].pvBuffer = NULL;
      }
    }

    if (scRet == SEC_E_OK)
    {
     /*
      * If there's extra data then save it for
      * next time we go to decrypt
      */
      if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
      {
        memcpy(conn->decryptBuffer,
               (LPBYTE) (conn->decryptBuffer + (conn->decryptBufferUsed - inBuffers[1].cbBuffer)),
               inBuffers[1].cbBuffer);
        conn->decryptBufferUsed = inBuffers[1].cbBuffer;
      }
      else
      {
        conn->decryptBufferUsed = 0;
      }

      ok = TRUE;
      break;
    }
    else if (FAILED(scRet) && (scRet != SEC_E_INCOMPLETE_MESSAGE))
    {
      DEBUG_printf(("_sspiAccept: AcceptSecurityContext failed: %x", scRet));
      ok = FALSE;
      break;
    }

    if (scRet != SEC_E_INCOMPLETE_MESSAGE &&
        scRet != SEC_I_INCOMPLETE_CREDENTIALS)
    {
      if (inBuffers[1].BufferType == SECBUFFER_EXTRA)
      {
        memcpy(conn->decryptBuffer,
               (LPBYTE) (conn->decryptBuffer + (conn->decryptBufferUsed - inBuffers[1].cbBuffer)),
               inBuffers[1].cbBuffer);
        conn->decryptBufferUsed = inBuffers[1].cbBuffer;
      }
      else
      {
        conn->decryptBufferUsed = 0;
      }
    }
  }

  if (ok)
  {
    conn->contextInitialized = TRUE;

   /*
    * Find out how big the header will be:
    */
    scRet = QueryContextAttributes(&conn->context, SECPKG_ATTR_STREAM_SIZES, &conn->streamSizes);

    if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("_sspiAccept: QueryContextAttributes failed: %x", scRet));
      ok = FALSE;
    }
  }

cleanup:

  return (ok);
}


/*
 * '_sspiSetAllowsAnyRoot()' - Set the client cert policy for untrusted root certs
 */
void
_sspiSetAllowsAnyRoot(_sspi_struct_t *conn,
					/* I  - Client connection */
                      BOOL           allow)
					/* I  - Allow any root */
{
  conn->certFlags = (allow) ? conn->certFlags | SECURITY_FLAG_IGNORE_UNKNOWN_CA :
                              conn->certFlags & ~SECURITY_FLAG_IGNORE_UNKNOWN_CA;
}


/*
 * '_sspiSetAllowsExpiredCerts()' - Set the client cert policy for expired root certs
 */
void
_sspiSetAllowsExpiredCerts(_sspi_struct_t *conn,
					/* I  - Client connection */
                           BOOL           allow)
					/* I  - Allow expired certs */
{
  conn->certFlags = (allow) ? conn->certFlags | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID :
                              conn->certFlags & ~SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
}


/*
 * '_sspiWrite()' - Write a buffer to an ssl socket
 */
int					/* O  - Bytes written or SOCKET_ERROR */
_sspiWrite(_sspi_struct_t *conn,	/* I  - Client connection */
           void           *buf,		/* I  - Buffer */
           size_t         len)		/* I  - Buffer length */
{
  SecBufferDesc	message;		/* Array of SecBuffer struct */
  SecBuffer	buffers[4] = { 0 };	/* Security package buffer */
  BYTE		*buffer = NULL;		/* Scratch buffer */
  int		bufferLen;		/* Buffer length */
  size_t	bytesLeft;		/* Bytes left to write */
  int		index = 0;		/* Index into buffer */
  int		num = 0;		/* Return value */

  if (!conn || !buf || !len)
  {
    WSASetLastError(WSAEINVAL);
    num = SOCKET_ERROR;
    goto cleanup;
  }

  bufferLen = conn->streamSizes.cbMaximumMessage +
              conn->streamSizes.cbHeader +
              conn->streamSizes.cbTrailer;

  buffer = (BYTE*) malloc(bufferLen);

  if (!buffer)
  {
    DEBUG_printf(("_sspiWrite: buffer alloc of %d bytes failed", bufferLen));
    WSASetLastError(E_OUTOFMEMORY);
    num = SOCKET_ERROR;
    goto cleanup;
  }

  bytesLeft = len;

  while (bytesLeft)
  {
    size_t chunk = min(conn->streamSizes.cbMaximumMessage,	/* Size of data to write */
                       bytesLeft);
    SECURITY_STATUS scRet;					/* SSPI status */

   /*
    * Copy user data into the buffer, starting
    * just past the header
    */
    memcpy(buffer + conn->streamSizes.cbHeader,
           ((BYTE*) buf) + index,
           chunk);

   /*
    * Setup the SSPI buffers
    */
    message.ulVersion = SECBUFFER_VERSION;
    message.cBuffers = 4;
    message.pBuffers = buffers;
    buffers[0].pvBuffer = buffer;
    buffers[0].cbBuffer = conn->streamSizes.cbHeader;
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[1].pvBuffer = buffer + conn->streamSizes.cbHeader;
    buffers[1].cbBuffer = (unsigned long) chunk;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[2].pvBuffer = buffer + conn->streamSizes.cbHeader + chunk;
    buffers[2].cbBuffer = conn->streamSizes.cbTrailer;
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[3].BufferType = SECBUFFER_EMPTY;

   /*
    * Encrypt the data
    */
    scRet = EncryptMessage(&conn->context, 0, &message, 0);

    if (FAILED(scRet))
    {
      DEBUG_printf(("_sspiWrite: EncryptMessage failed: %x", scRet));
      WSASetLastError(WSASYSCALLFAILURE);
      num = SOCKET_ERROR;
      goto cleanup;
    }

   /*
    * Send the data. Remember the size of
    * the total data to send is the size
    * of the header, the size of the data
    * the caller passed in and the size
    * of the trailer
    */
    num = send(conn->sock,
               buffer,
               buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer,
               0);

    if ((num == SOCKET_ERROR) || (num == 0))
    {
      DEBUG_printf(("_sspiWrite: send failed: %ld", WSAGetLastError()));
      goto cleanup;
    }

    bytesLeft -= (int) chunk;
    index += (int) chunk;
  }

  num = (int) len;

cleanup:

  if (buffer)
    free(buffer);

  return (num);
}


/*
 * '_sspiRead()' - Read a buffer from an ssl socket
 */
int					/* O  - Bytes read or SOCKET_ERROR */
_sspiRead(_sspi_struct_t *conn,		/* I  - Client connection */
          void           *buf,		/* I  - Buffer */
          size_t         len)		/* I  - Buffer length */
{
  SecBufferDesc	message;		/* Array of SecBuffer struct */
  SecBuffer	buffers[4] = { 0 };	/* Security package buffer */
  int		num = 0;		/* Return value */

  if (!conn)
  {
    WSASetLastError(WSAEINVAL);
    num = SOCKET_ERROR;
    goto cleanup;
  }

 /*
  * If there are bytes that have already been
  * decrypted and have not yet been read, return
  * those
  */
  if (buf && (conn->readBufferUsed > 0))
  {
    int bytesToCopy = (int) min(conn->readBufferUsed, len);	/* Amount of bytes to copy */
								/* from read buffer */

    memcpy(buf, conn->readBuffer, bytesToCopy);
    conn->readBufferUsed -= bytesToCopy;

    if (conn->readBufferUsed > 0)
     /*
      * If the caller didn't request all the bytes
      * we have in the buffer, then move the unread
      * bytes down
      */
      memmove(conn->readBuffer,
              conn->readBuffer + bytesToCopy,
              conn->readBufferUsed);

    num = bytesToCopy;
  }
  else
  {
    PSecBuffer		pDataBuffer;	/* Data buffer */
    PSecBuffer		pExtraBuffer;	/* Excess data buffer */
    SECURITY_STATUS	scRet;		/* SSPI status */
    int			i;		/* Loop control variable */

   /*
    * Initialize security buffer structs
    */
    message.ulVersion = SECBUFFER_VERSION;
    message.cBuffers = 4;
    message.pBuffers = buffers;

    do
    {
     /*
      * If there is not enough space in the
      * buffer, then increase it's size
      */
      if (conn->decryptBufferLength <= conn->decryptBufferUsed)
      {
        conn->decryptBufferLength += 4096;
        conn->decryptBuffer = (BYTE*) realloc(conn->decryptBuffer,
                                              conn->decryptBufferLength);

        if (!conn->decryptBuffer)
        {
          DEBUG_printf(("_sspiRead: unable to allocate %d byte buffer",
                        conn->decryptBufferLength));
          WSASetLastError(E_OUTOFMEMORY);
          num = SOCKET_ERROR;
          goto cleanup;
        }
      }

      buffers[0].pvBuffer	= conn->decryptBuffer;
      buffers[0].cbBuffer	= (unsigned long) conn->decryptBufferUsed;
      buffers[0].BufferType	= SECBUFFER_DATA;
      buffers[1].BufferType	= SECBUFFER_EMPTY;
      buffers[2].BufferType	= SECBUFFER_EMPTY;
      buffers[3].BufferType	= SECBUFFER_EMPTY;

      scRet = DecryptMessage(&conn->context, &message, 0, NULL);

      if (scRet == SEC_E_INCOMPLETE_MESSAGE)
      {
        if (buf)
        {
          num = recv(conn->sock,
                     conn->decryptBuffer + conn->decryptBufferUsed,
                     (int)(conn->decryptBufferLength - conn->decryptBufferUsed),
                     0);
          if (num == SOCKET_ERROR)
          {
            DEBUG_printf(("_sspiRead: recv failed: %d", WSAGetLastError()));
            goto cleanup;
          }
          else if (num == 0)
          {
            DEBUG_printf(("_sspiRead: server disconnected"));
            goto cleanup;
          }

          conn->decryptBufferUsed += num;
        }
        else
        {
          num = (int) conn->readBufferUsed;
          goto cleanup;
        }
      }
    }
    while (scRet == SEC_E_INCOMPLETE_MESSAGE);

    if (scRet == SEC_I_CONTEXT_EXPIRED)
    {
      DEBUG_printf(("_sspiRead: context expired"));
      WSASetLastError(WSAECONNRESET);
      num = SOCKET_ERROR;
      goto cleanup;
    }
    else if (scRet != SEC_E_OK)
    {
      DEBUG_printf(("_sspiRead: DecryptMessage failed: %lx", scRet));
      WSASetLastError(WSASYSCALLFAILURE);
      num = SOCKET_ERROR;
      goto cleanup;
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
    * If a data buffer is found, then copy
    * the decrypted bytes to the passed-in
    * buffer
    */
    if (pDataBuffer)
    {
      int bytesToCopy = min(pDataBuffer->cbBuffer, (int) len);
                           		/* Number of bytes to copy into buf */
      int bytesToSave = pDataBuffer->cbBuffer - bytesToCopy;
                           		/* Number of bytes to save in our read buffer */

      if (bytesToCopy)
        memcpy(buf, pDataBuffer->pvBuffer, bytesToCopy);

     /*
      * If there are more decrypted bytes than can be
      * copied to the passed in buffer, then save them
      */
      if (bytesToSave)
      {
        if ((int)(conn->readBufferLength - conn->readBufferUsed) < bytesToSave)
        {
          conn->readBufferLength = conn->readBufferUsed + bytesToSave;
          conn->readBuffer = realloc(conn->readBuffer,
                                     conn->readBufferLength);

          if (!conn->readBuffer)
          {
            DEBUG_printf(("_sspiRead: unable to allocate %d bytes", conn->readBufferLength));
            WSASetLastError(E_OUTOFMEMORY);
            num = SOCKET_ERROR;
            goto cleanup;
          }
        }

        memcpy(((BYTE*) conn->readBuffer) + conn->readBufferUsed,
               ((BYTE*) pDataBuffer->pvBuffer) + bytesToCopy,
               bytesToSave);

        conn->readBufferUsed += bytesToSave;
      }

      num = (buf) ? bytesToCopy : (int) conn->readBufferUsed;
    }
    else
    {
      DEBUG_printf(("_sspiRead: unable to find data buffer"));
      WSASetLastError(WSASYSCALLFAILURE);
      num = SOCKET_ERROR;
      goto cleanup;
    }

   /*
    * If the decryption process left extra bytes,
    * then save those back in decryptBuffer. They will
    * be processed the next time through the loop.
    */
    if (pExtraBuffer)
    {
      memmove(conn->decryptBuffer, pExtraBuffer->pvBuffer, pExtraBuffer->cbBuffer);
      conn->decryptBufferUsed = pExtraBuffer->cbBuffer;
    }
    else
    {
      conn->decryptBufferUsed = 0;
    }
  }

cleanup:

  return (num);
}


/*
 * '_sspiPending()' - Returns the number of available bytes
 */
int					/* O  - Number of available bytes */
_sspiPending(_sspi_struct_t *conn)	/* I  - Client connection */
{
  return (_sspiRead(conn, NULL, 0));
}


/*
 * '_sspiFree()' - Close a connection and free resources
 */
void
_sspiFree(_sspi_struct_t *conn)		/* I  - Client connection */
{
  if (!conn)
    return;

  if (conn->contextInitialized)
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

   status = ApplyControlToken(&conn->context, &message);

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

     status = AcceptSecurityContext(&conn->creds, &conn->context, NULL,
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
          cbData = send(conn->sock, pbMessage, cbMessage, 0);
          if ((cbData == SOCKET_ERROR) || (cbData == 0))
          {
            status = WSAGetLastError();
            DEBUG_printf(("_sspiFree: sending close notify failed: %d", status));
          }
          else
          {
            FreeContextBuffer(pbMessage);
          }
        }
      }
      else
      {
        DEBUG_printf(("_sspiFree: AcceptSecurityContext failed: %x", status));
      }
    }
    else
    {
      DEBUG_printf(("_sspiFree: ApplyControlToken failed: %x", status));
    }

    DeleteSecurityContext(&conn->context);
    conn->contextInitialized = FALSE;
  }

  if (conn->decryptBuffer)
  {
    free(conn->decryptBuffer);
    conn->decryptBuffer = NULL;
  }

  if (conn->readBuffer)
  {
    free(conn->readBuffer);
    conn->readBuffer = NULL;
  }

  if (conn->sock != INVALID_SOCKET)
  {
    closesocket(conn->sock);
    conn->sock = INVALID_SOCKET;
  }

  free(conn);
}


/*
 * 'sspi_verify_certificate()' - Verify a server certificate
 */
static DWORD				/* 0  - Error code (0 == No error) */
sspi_verify_certificate(PCCERT_CONTEXT  serverCert,
					/* I  - Server certificate */
                        const CHAR      *serverName,
					/* I  - Server name */
                        DWORD           dwCertFlags)
					/* I  - Verification flags */
{
  HTTPSPolicyCallbackData	httpsPolicy;
					/* HTTPS Policy Struct */
  CERT_CHAIN_POLICY_PARA	policyPara;
					/* Cert chain policy parameters */
  CERT_CHAIN_POLICY_STATUS	policyStatus;
					/* Cert chain policy status */
  CERT_CHAIN_PARA		chainPara;
					/* Used for searching and matching criteria */
  PCCERT_CHAIN_CONTEXT		chainContext = NULL;
					/* Certificate chain */
  PWSTR				serverNameUnicode = NULL;
					/* Unicode server name */
  LPSTR				rgszUsages[] = { szOID_PKIX_KP_SERVER_AUTH,
                                                 szOID_SERVER_GATED_CRYPTO,
                                                 szOID_SGC_NETSCAPE };
					/* How are we using this certificate? */
  DWORD				cUsages = sizeof(rgszUsages) / sizeof(LPSTR);
					/* Number of ites in rgszUsages */
  DWORD				count;	/* 32 bit count variable */
  DWORD				status;	/* Return value */

  if (!serverCert)
  {
    status = SEC_E_WRONG_PRINCIPAL;
    goto cleanup;
  }

 /*
  *  Convert server name to unicode.
  */
  if (!serverName || (strlen(serverName) == 0))
  {
    status = SEC_E_WRONG_PRINCIPAL;
    goto cleanup;
  }

  count = MultiByteToWideChar(CP_ACP, 0, serverName, -1, NULL, 0);
  serverNameUnicode = LocalAlloc(LMEM_FIXED, count * sizeof(WCHAR));
  if (!serverNameUnicode)
  {
    status = SEC_E_INSUFFICIENT_MEMORY;
    goto cleanup;
  }
  count = MultiByteToWideChar(CP_ACP, 0, serverName, -1, serverNameUnicode, count);
  if (count == 0)
  {
    status = SEC_E_WRONG_PRINCIPAL;
    goto cleanup;
  }

 /*
  * Build certificate chain.
  */
  ZeroMemory(&chainPara, sizeof(chainPara));
  chainPara.cbSize					= sizeof(chainPara);
  chainPara.RequestedUsage.dwType			= USAGE_MATCH_TYPE_OR;
  chainPara.RequestedUsage.Usage.cUsageIdentifier	= cUsages;
  chainPara.RequestedUsage.Usage.rgpszUsageIdentifier	= rgszUsages;

  if (!CertGetCertificateChain(NULL, serverCert, NULL, serverCert->hCertStore,
                               &chainPara, 0, NULL, &chainContext))
  {
    status = GetLastError();
    DEBUG_printf(("CertGetCertificateChain returned 0x%x\n", status));
    goto cleanup;
  }

 /*
  * Validate certificate chain.
  */
  ZeroMemory(&httpsPolicy, sizeof(HTTPSPolicyCallbackData));
  httpsPolicy.cbStruct		= sizeof(HTTPSPolicyCallbackData);
  httpsPolicy.dwAuthType	= AUTHTYPE_SERVER;
  httpsPolicy.fdwChecks		= dwCertFlags;
  httpsPolicy.pwszServerName	= serverNameUnicode;

  memset(&policyPara, 0, sizeof(policyPara));
  policyPara.cbSize		= sizeof(policyPara);
  policyPara.pvExtraPolicyPara	= &httpsPolicy;

  memset(&policyStatus, 0, sizeof(policyStatus));
  policyStatus.cbSize = sizeof(policyStatus);

  if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chainContext,
                                        &policyPara, &policyStatus))
  {
    status = GetLastError();
    DEBUG_printf(("CertVerifyCertificateChainPolicy returned %d", status));
    goto cleanup;
  }

  if (policyStatus.dwError)
  {
    status = policyStatus.dwError;
    goto cleanup;
  }

  status = SEC_E_OK;

cleanup:

  if (chainContext)
    CertFreeCertificateChain(chainContext);

  if (serverNameUnicode)
    LocalFree(serverNameUnicode);

  return (status);
}


/*
 * End of "$Id: sspi.c 11760 2014-03-28 12:58:24Z msweet $".
 */
