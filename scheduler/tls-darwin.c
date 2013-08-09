/*
 * "$Id$"
 *
 *   TLS support code for the CUPS scheduler on OS X.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cupsdEndTLS()	     - Shutdown a secure session with the client.
 *   cupsdStartTLS()	     - Start a secure session with the client.
 *   copy_cdsa_certificate() - Copy a SSL/TLS certificate from the System
 *			       keychain.
 *   make_certificate()      - Make a self-signed SSL/TLS certificate.
 */


/*
 * Local functions...
 */

static CFArrayRef	copy_cdsa_certificate(cupsd_client_t *con);
static int		make_certificate(cupsd_client_t *con);


/*
 * 'cupsdEndTLS()' - Shutdown a secure session with the client.
 */

int					/* O - 1 on success, 0 on error */
cupsdEndTLS(cupsd_client_t *con)	/* I - Client connection */
{
  while (SSLClose(con->http.tls) == errSSLWouldBlock)
    usleep(1000);

  CFRelease(con->http.tls);
  con->http.tls = NULL;

  if (con->http.tls_credentials)
    CFRelease(con->http.tls_credentials);

  return (1);
}


/*
 * 'cupsdStartTLS()' - Start a secure session with the client.
 */

int					/* O - 1 on success, 0 on error */
cupsdStartTLS(cupsd_client_t *con)	/* I - Client connection */
{
  OSStatus	error = 0;		/* Error code */
  SecTrustRef	peerTrust;		/* Peer certificates */


  cupsdLogMessage(CUPSD_LOG_DEBUG, "[Client %d] Encrypting connection.",
                  con->http.fd);

  con->http.tls_credentials = copy_cdsa_certificate(con);

  if (!con->http.tls_credentials)
  {
   /*
    * No keychain (yet), make a self-signed certificate...
    */

    if (make_certificate(con))
      con->http.tls_credentials = copy_cdsa_certificate(con);
  }

  if (!con->http.tls_credentials)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
        	    "Could not find signing key in keychain \"%s\"",
		    ServerCertificate);
    error = errSSLBadConfiguration;
  }

  if (!error)
    con->http.tls = SSLCreateContext(kCFAllocatorDefault, kSSLServerSide,
                                     kSSLStreamType);

  if (!error)
    error = SSLSetIOFuncs(con->http.tls, _httpReadCDSA, _httpWriteCDSA);

  if (!error)
    error = SSLSetConnection(con->http.tls, HTTP(con));

  if (!error)
    error = SSLSetCertificate(con->http.tls, con->http.tls_credentials);

  if (!error)
  {
   /*
    * Perform SSL/TLS handshake
    */

    while ((error = SSLHandshake(con->http.tls)) == errSSLWouldBlock)
      usleep(1000);
  }

  if (error)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to encrypt connection from %s - %s (%d)",
                    con->http.hostname, cssmErrorString(error), (int)error);

    con->http.error  = error;
    con->http.status = HTTP_ERROR;

    if (con->http.tls)
    {
      CFRelease(con->http.tls);
      con->http.tls = NULL;
    }

    if (con->http.tls_credentials)
    {
      CFRelease(con->http.tls_credentials);
      con->http.tls_credentials = NULL;
    }

    return (0);
  }

  cupsdLogMessage(CUPSD_LOG_DEBUG, "Connection from %s now encrypted.",
                  con->http.hostname);

  if (!SSLCopyPeerTrust(con->http.tls, &peerTrust) && peerTrust)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Received %d peer certificates!",
		    (int)SecTrustGetCertificateCount(peerTrust));
    CFRelease(peerTrust);
  }
  else
    cupsdLogMessage(CUPSD_LOG_DEBUG, "Received NO peer certificates!");

  return (1);
}


/*
 * 'copy_cdsa_certificate()' - Copy a SSL/TLS certificate from the System
 *                             keychain.
 */

static CFArrayRef				/* O - Array of certificates */
copy_cdsa_certificate(
    cupsd_client_t *con)			/* I - Client connection */
{
  OSStatus		err;		/* Error info */
  SecKeychainRef	keychain = NULL;/* Keychain reference */
  SecIdentitySearchRef	search = NULL;	/* Search reference */
  SecIdentityRef	identity = NULL;/* Identity */
  CFArrayRef		certificates = NULL;
					/* Certificate array */
  SecPolicyRef		policy = NULL;	/* Policy ref */
  CFStringRef		servername = NULL;
					/* Server name */
  CFMutableDictionaryRef query = NULL;	/* Query qualifiers */
  CFArrayRef		list = NULL;	/* Keychain list */
#    if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  char			localname[1024];/* Local hostname */
#    endif /* HAVE_DNSSD || HAVE_AVAHI */


  cupsdLogMessage(CUPSD_LOG_DEBUG,
                  "copy_cdsa_certificate: Looking for certs for \"%s\"...",
		  con->servername);

  if ((err = SecKeychainOpen(ServerCertificate, &keychain)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Cannot open keychain \"%s\" - %s (%d)",
	            ServerCertificate, cssmErrorString(err), (int)err);
    goto cleanup;
  }

  servername = CFStringCreateWithCString(kCFAllocatorDefault, con->servername,
					 kCFStringEncodingUTF8);

  policy = SecPolicyCreateSSL(1, servername);

  if (servername)
    CFRelease(servername);

  if (!policy)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Cannot create ssl policy reference");
    goto cleanup;
  }

  if (!(query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Cannot create query dictionary");
    goto cleanup;
  }

  list = CFArrayCreate(kCFAllocatorDefault, (const void **)&keychain, 1,
                       &kCFTypeArrayCallBacks);

  CFDictionaryAddValue(query, kSecClass, kSecClassIdentity);
  CFDictionaryAddValue(query, kSecMatchPolicy, policy);
  CFDictionaryAddValue(query, kSecReturnRef, kCFBooleanTrue);
  CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitOne);
  CFDictionaryAddValue(query, kSecMatchSearchList, list);

  CFRelease(list);

  err = SecItemCopyMatching(query, (CFTypeRef *)&identity);

#    if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (err && DNSSDHostName)
  {
   /*
    * Search for the connection server name failed; try the DNS-SD .local
    * hostname instead...
    */

    snprintf(localname, sizeof(localname), "%s.local", DNSSDHostName);

    cupsdLogMessage(CUPSD_LOG_DEBUG,
		    "copy_cdsa_certificate: Looking for certs for \"%s\"...",
		    localname);

    servername = CFStringCreateWithCString(kCFAllocatorDefault, localname,
					   kCFStringEncodingUTF8);

    CFRelease(policy);

    policy = SecPolicyCreateSSL(1, servername);

    if (servername)
      CFRelease(servername);

    if (!policy)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR, "Cannot create ssl policy reference");
      goto cleanup;
    }

    CFDictionarySetValue(query, kSecMatchPolicy, policy);

    err = SecItemCopyMatching(query, (CFTypeRef *)&identity);
  }
#    endif /* HAVE_DNSSD || HAVE_AVAHI */

  if (err)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG,
		    "Cannot find signing key in keychain \"%s\": %s (%d)",
		    ServerCertificate, cssmErrorString(err), (int)err);
    goto cleanup;
  }

  if (CFGetTypeID(identity) != SecIdentityGetTypeID())
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "SecIdentity CFTypeID failure!");
    goto cleanup;
  }

  if ((certificates = CFArrayCreate(NULL, (const void **)&identity,
				  1, &kCFTypeArrayCallBacks)) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Cannot create certificate array");
    goto cleanup;
  }

  cleanup :

  if (keychain)
    CFRelease(keychain);
  if (search)
    CFRelease(search);
  if (identity)
    CFRelease(identity);

  if (policy)
    CFRelease(policy);
  if (query)
    CFRelease(query);

  return (certificates);
}


/*
 * 'make_certificate()' - Make a self-signed SSL/TLS certificate.
 */

static int				/* O - 1 on success, 0 on failure */
make_certificate(cupsd_client_t *con)	/* I - Client connection */
{
#if 1
  int		pid,			/* Process ID of command */
		status;			/* Status of command */
  char		command[1024],		/* Command */
		*argv[4],		/* Command-line arguments */
		*envp[MAX_ENV + 1],	/* Environment variables */
		keychain[1024],		/* Keychain argument */
		infofile[1024],		/* Type-in information for cert */
#  if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
		localname[1024],	/* Local hostname */
#  endif /* HAVE_DNSSD || HAVE_AVAHI */
		*servername;		/* Name of server in cert */
  cups_file_t	*fp;			/* Seed/info file */
  int		infofd;			/* Info file descriptor */


#  if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (con->servername && isdigit(con->servername[0] & 255) && DNSSDHostName)
  {
    snprintf(localname, sizeof(localname), "%s.local", DNSSDHostName);
    servername = localname;
  }
  else
#  endif /* HAVE_DNSSD || HAVE_AVAHI */
    servername = con->servername;

 /*
  * Run the "certtool" command to generate a self-signed certificate...
  */

  if (!cupsFileFind("certtool", getenv("PATH"), 1, command, sizeof(command)))
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "No SSL certificate and certtool command not found!");
    return (0);
  }

 /*
  * Create a file with the certificate information fields...
  *
  * Note: This assumes that the default questions are asked by the certtool
  * command...
  */

  if ((fp = cupsTempFile2(infofile, sizeof(infofile))) == NULL)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create certificate information file %s - %s",
                    infofile, strerror(errno));
    return (0);
  }

  cupsFilePrintf(fp,
                 "%s\n"			/* Enter key and certificate label */
                 "r\n"			/* Generate RSA key pair */
                 "2048\n"		/* Key size in bits */
                 "y\n"			/* OK (y = yes) */
                 "b\n"			/* Usage (b=signing/encryption) */
                 "s\n"			/* Sign with SHA1 */
                 "y\n"			/* OK (y = yes) */
                 "%s\n"			/* Common name */
                 "\n"			/* Country (default) */
                 "\n"			/* Organization (default) */
                 "\n"			/* Organizational unit (default) */
                 "\n"			/* State/Province (default) */
                 "%s\n"			/* Email address */
                 "y\n",			/* OK (y = yes) */
        	 servername, servername, ServerAdmin);
  cupsFileClose(fp);

  cupsdLogMessage(CUPSD_LOG_INFO,
                  "Generating SSL server key and certificate...");

  snprintf(keychain, sizeof(keychain), "k=%s", ServerCertificate);

  argv[0] = "certtool";
  argv[1] = "c";
  argv[2] = keychain;
  argv[3] = NULL;

  cupsdLoadEnv(envp, MAX_ENV);

  infofd = open(infofile, O_RDONLY);

  if (!cupsdStartProcess(command, argv, envp, infofd, -1, -1, -1, -1, 1, NULL,
                         NULL, &pid))
  {
    close(infofd);
    unlink(infofile);
    return (0);
  }

  close(infofd);
  unlink(infofile);

  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR)
    {
      status = 1;
      break;
    }

  cupsdFinishProcess(pid, command, sizeof(command), NULL);

  if (status)
  {
    if (WIFEXITED(status))
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to create SSL server key and certificate - "
		      "the certtool command stopped with status %d!",
	              WEXITSTATUS(status));
    else
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to create SSL server key and certificate - "
		      "the certtool command crashed on signal %d!",
	              WTERMSIG(status));
  }
  else
  {
    cupsdLogMessage(CUPSD_LOG_INFO,
                    "Created SSL server certificate file \"%s\"...",
		    ServerCertificate);
  }

  return (!status);
#else
  int			status = 0;	/* Return status */
  OSStatus		err;		/* Error code (if any) */
#  if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  char			localname[1024];/* Local hostname */
#  endif /* HAVE_DNSSD || HAVE_AVAHI */
  const char		*servername;	/* Name of server in cert */
  CFStringRef		cfservername = NULL;
					/* CF string for server name */
  SecIdentityRef	ident = NULL;	/* Identity */
  SecKeyRef		publicKey = NULL,
					/* Public key */
			privateKey = NULL;
					/* Private key */
  CFDictionaryRef	keyParams = NULL;
					/* Key generation parameters */


#  if defined(HAVE_DNSSD) || defined(HAVE_AVAHI)
  if (con->servername && isdigit(con->servername[0] & 255) && DNSSDHostName)
  {
    snprintf(localname, sizeof(localname), "%s.local", DNSSDHostName);
    servername = localname;
  }
  else
#  endif /* HAVE_DNSSD || HAVE_AVAHI */
  servername = con->servername;

  cfservername = CFStringCreateFromCString(kCFAllocatorDefault, servername,
                                           kCFStringEncodingUTF8);
  if (!cfservername)
    goto cleanup;

 /*
  * Create a public/private key pair...
  */

  keyParams = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks)))
  if (!keyParams)
    goto cleanup;

  CFDictionaryAddValue(keyParams, kSecAttrKeyType, kSecAttrKeyTypeRSA);
  CFDictionaryAddValue(keyParams, kSecAttrKeySizeInBits, CFSTR("2048"));
  CFDictionaryAddValue(keyParams, kSecAttrLabel,
                       CFSTR("CUPS Self-Signed Certificate"));

  err = SecKeyGeneratePair(keyParams, &publicKey, &privateKey);
  if (err != noErr)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "SecKeyGeneratePair returned %ld.",
                    (long)err);
    goto cleanup;
  }

 /*
  * Create a self-signed certificate...
  */

  CE_DataAndType		exts[2],
				*extp = exts;
  unsigned			numExts = 0;
  CSSM_DATA			refId;        // mallocd by CSSM_TP_SubmitCredRequest
  CSSM_APPLE_TP_CERT_REQUEST	certReq;
  CSSM_TP_REQUEST_SET		reqSet;
  sint32			estTime;
  CSSM_BOOL			confirmRequired;
  CSSM_TP_RESULT_SET_PTR	resultSet;
  CSSM_ENCODED_CERT		*encCert;
  CSSM_APPLE_TP_NAME_OID	subjectNames[MAX_NAMES];
  uint32			numNames = 0;
  CSSM_TP_CALLERAUTH_CONTEXT	CallerAuthContext;
  CSSM_FIELD			policyId;

  certReq.challengeString  = NULL;

  extp->type               = DT_KeyUsage;
  extp->critical           = CSSM_FALSE;
  extp->extension.keyUsage = CE_KU_DigitalSignature | CE_KU_KeyCertSign |
			     CE_KU_KeyEncipherment | CE_KU_DataEncipherment;
  extp ++;
  numExts ++;

  extp->type = DT_BasicConstraints;
  extp->critical = CSSM_TRUE;
  extp->extension.basicConstraints.cA = CSSM_FALSE;
  extp->extension.basicConstraints.pathLenConstraintPresent = CSSM_FALSE;
  extp ++;
  numExts ++;

  subjectNames[0].string = servername;
  subjectNames[0].oid    = &CSSMOID_CommonName;
  subjectNames[1].string = "";
  subjectNames[1].oid    = &CSSMOID_OrganizationName;
  subjectNames[2].string = "";
  subjectNames[2].oid    = &CSSMOID_CountryName;
  subjectNames[3].string = "";
  subjectNames[3].oid    = &CSSMOID_StateProvinceName;
  numNames = 4;

  /* certReq */
  certReq.cspHand          = cspHand;
  certReq.clHand           = clHand;
  certReq.serialNumber     = 0x12345678;        // TBD - random? From user?
  certReq.numSubjectNames  = numNames;
  certReq.subjectNames     = subjectNames;
  certReq.numIssuerNames   = 0;
  certReq.issuerNames      = NULL;
  certReq.issuerNameX509   = NULL;
  certReq.certPublicKey    = subjPubKey;
  certReq.issuerPrivateKey = signerPrivKey;
  certReq.signatureAlg     = sigAlg;
  certReq.signatureOid     = *sigOid;
  certReq.notBefore        = 0;                    // TBD - from user
  certReq.notAfter         = 10 * 365 * 86400;
  certReq.numExtensions    = numExts;
  certReq.extensions       = exts;
  reqSet.NumberOfRequests  = 1;
  reqSet.Requests          = &certReq;

  memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));

  memset(&policyId, 0, sizeof(CSSM_FIELD));
  policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;

  CallerAuthContext.Policy.NumberOfPolicyIds = 1;
  CallerAuthContext.Policy.PolicyIds         = &policyId;

  CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tpHand,
					    NULL,                // PreferredAuthority
					    CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
					    &reqSet,
					    &CallerAuthContext,
					    &estTime,
					    &refId);
  /* before proceeding, free resources allocated thus far */
#if CERT_TOOL
  if (!useAllDefaults) {
      freeNameOids(subjectNames, numNames);
  }
#endif

  if (crtn) {
      cuPrintError("CSSM_TP_SubmitCredRequest", crtn);
      return crtn;
  }
  crtn = CSSM_TP_RetrieveCredResult(tpHand,
				 &refId,
				 NULL,                // CallerAuthCredentials
				 &estTime,
				 &confirmRequired,
				 &resultSet);
  if (crtn) {
      cuPrintError("CSSM_TP_RetrieveCredResult", crtn);
      return crtn;
  }
  if (resultSet == NULL) {
      coreLog("httpserver", "***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
      return ioErr;
  }
  encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
  *certData = encCert->CertBlob;
  /* free resources allocated by TP */
  APP_FREE(refId.Data);
  APP_FREE(encCert);
  APP_FREE(resultSet);
  return noErr;

 /*
  * Cleanup and return...
  */

cleanup:

  if (cfservername)
    CFRelease(cfservername);

  if (keyParams)
    CFRelease(keyParams);

  if (ident)
    CFRelease(ident);

  if (publicKey)
    CFRelease(publicKey);

  if (privateKey)
    CFRelease(publicKey);

  if (!status)
    cupsdLogMessage(CUPSD_LOG_ERROR,
                    "Unable to create SSL server key and certificate.");

  return (status);
#endif // 0
}


/*
 * End of "$Id$".
 */
