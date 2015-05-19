/*
 * "$Id$"
 *
 * TLS check program for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products.
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

#include "cups-private.h"


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  http_t	*http;			/* HTTP connection */
  const char	*server = argv[1];	/* Hostname from command-line */
  int		port = 631;		/* Port number */
  const char	*cipherName = "UNKNOWN";/* Cipher suite name */
  int		tlsVersion = 0;		/* TLS version number */


  if (argc < 2 || argc > 3)
  {
    puts("Usage: ./tlscheck server [port]");
    puts("");
    puts("The default port is 631.");
    return (1);
  }

  if (argc == 3)
    port = atoi(argv[2]);

  http = httpConnect2(server, port, NULL, AF_UNSPEC, HTTP_ENCRYPTION_ALWAYS, 1, 30000, NULL);
  if (!http)
  {
    printf("%s: ERROR (%s)\n", server, cupsLastErrorString());
    return (1);
  }

#ifdef __APPLE__
  SSLProtocol protocol;
  SSLCipherSuite cipher;
  char unknownCipherName[256];
  int paramsNeeded = 0;
  const void *params;
  size_t paramsLen;
  OSStatus err;

  if ((err = SSLGetNegotiatedProtocolVersion(http->tls, &protocol)) != noErr)
  {
    printf("%s: ERROR (No protocol version - %d)\n", server, (int)err);
    httpClose(http);
    return (1);
  }

  switch (protocol)
  {
    default :
        tlsVersion = 0;
        break;
    case kSSLProtocol3 :
        tlsVersion = 30;
        break;
    case kTLSProtocol1 :
        tlsVersion = 10;
        break;
    case kTLSProtocol11 :
        tlsVersion = 11;
        break;
    case kTLSProtocol12 :
        tlsVersion = 12;
        break;
  }

  if ((err = SSLGetNegotiatedCipher(http->tls, &cipher)) != noErr)
  {
    printf("%s: ERROR (No cipher suite - %d)\n", server, (int)err);
    httpClose(http);
    return (1);
  }

  switch (cipher)
  {
    case TLS_NULL_WITH_NULL_NULL:
	cipherName = "TLS_NULL_WITH_NULL_NULL";
	break;
    case TLS_RSA_WITH_NULL_MD5:
	cipherName = "TLS_RSA_WITH_NULL_MD5";
	break;
    case TLS_RSA_WITH_NULL_SHA:
	cipherName = "TLS_RSA_WITH_NULL_SHA";
	break;
    case TLS_RSA_WITH_RC4_128_MD5:
	cipherName = "TLS_RSA_WITH_RC4_128_MD5";
	break;
    case TLS_RSA_WITH_RC4_128_SHA:
	cipherName = "TLS_RSA_WITH_RC4_128_SHA";
	break;
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_RSA_WITH_3DES_EDE_CBC_SHA";
	break;
    case TLS_RSA_WITH_NULL_SHA256:
	cipherName = "TLS_RSA_WITH_NULL_SHA256";
	break;
    case TLS_RSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_RSA_WITH_AES_128_CBC_SHA256";
	break;
    case TLS_RSA_WITH_AES_256_CBC_SHA256:
	cipherName = "TLS_RSA_WITH_AES_256_CBC_SHA256";
	break;
    case TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DH_DSS_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_DH_DSS_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_RSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_DH_RSA_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_DSS_WITH_AES_256_CBC_SHA256:
	cipherName = "TLS_DH_DSS_WITH_AES_256_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_RSA_WITH_AES_256_CBC_SHA256:
	cipherName = "TLS_DH_RSA_WITH_AES_256_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA256:
	cipherName = "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
	cipherName = "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_anon_WITH_RC4_128_MD5:
	cipherName = "TLS_DH_anon_WITH_RC4_128_MD5";
	paramsNeeded = 1;
	break;
    case TLS_DH_anon_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_DH_anon_WITH_3DES_EDE_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DH_anon_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_DH_anon_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_anon_WITH_AES_256_CBC_SHA256:
	cipherName = "TLS_DH_anon_WITH_AES_256_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_PSK_WITH_RC4_128_SHA:
	cipherName = "TLS_PSK_WITH_RC4_128_SHA";
	break;
    case TLS_PSK_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_PSK_WITH_3DES_EDE_CBC_SHA";
	break;
    case TLS_PSK_WITH_AES_128_CBC_SHA:
	cipherName = "TLS_PSK_WITH_AES_128_CBC_SHA";
	break;
    case TLS_PSK_WITH_AES_256_CBC_SHA:
	cipherName = "TLS_PSK_WITH_AES_256_CBC_SHA";
	break;
    case TLS_DHE_PSK_WITH_RC4_128_SHA:
	cipherName = "TLS_DHE_PSK_WITH_RC4_128_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_AES_128_CBC_SHA:
	cipherName = "TLS_DHE_PSK_WITH_AES_128_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_AES_256_CBC_SHA:
	cipherName = "TLS_DHE_PSK_WITH_AES_256_CBC_SHA";
	paramsNeeded = 1;
	break;
    case TLS_RSA_PSK_WITH_RC4_128_SHA:
	cipherName = "TLS_RSA_PSK_WITH_RC4_128_SHA";
	break;
    case TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA:
	cipherName = "TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA";
	break;
    case TLS_RSA_PSK_WITH_AES_128_CBC_SHA:
	cipherName = "TLS_RSA_PSK_WITH_AES_128_CBC_SHA";
	break;
    case TLS_RSA_PSK_WITH_AES_256_CBC_SHA:
	cipherName = "TLS_RSA_PSK_WITH_AES_256_CBC_SHA";
	break;
    case TLS_PSK_WITH_NULL_SHA:
	cipherName = "TLS_PSK_WITH_NULL_SHA";
	break;
    case TLS_DHE_PSK_WITH_NULL_SHA:
	cipherName = "TLS_DHE_PSK_WITH_NULL_SHA";
	paramsNeeded = 1;
	break;
    case TLS_RSA_PSK_WITH_NULL_SHA:
	cipherName = "TLS_RSA_PSK_WITH_NULL_SHA";
	break;
    case TLS_RSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_RSA_WITH_AES_128_GCM_SHA256";
	break;
    case TLS_RSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_RSA_WITH_AES_256_GCM_SHA384";
	break;
    case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_DHE_RSA_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_DHE_RSA_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_DH_RSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_DH_RSA_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_RSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_DH_RSA_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_DHE_DSS_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_DHE_DSS_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_DSS_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_DHE_DSS_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_DH_DSS_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_DH_DSS_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_DSS_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_DH_DSS_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_DH_anon_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_DH_anon_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DH_anon_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_DH_anon_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_PSK_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_PSK_WITH_AES_128_GCM_SHA256";
	break;
    case TLS_PSK_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_PSK_WITH_AES_256_GCM_SHA384";
	break;
    case TLS_DHE_PSK_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_DHE_PSK_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_DHE_PSK_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_RSA_PSK_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_RSA_PSK_WITH_AES_128_GCM_SHA256";
	break;
    case TLS_RSA_PSK_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_RSA_PSK_WITH_AES_256_GCM_SHA384";
	break;
    case TLS_PSK_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_PSK_WITH_AES_128_CBC_SHA256";
	break;
    case TLS_PSK_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_PSK_WITH_AES_256_CBC_SHA384";
	break;
    case TLS_PSK_WITH_NULL_SHA256:
	cipherName = "TLS_PSK_WITH_NULL_SHA256";
	break;
    case TLS_PSK_WITH_NULL_SHA384:
	cipherName = "TLS_PSK_WITH_NULL_SHA384";
	break;
    case TLS_DHE_PSK_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_DHE_PSK_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_DHE_PSK_WITH_AES_256_CBC_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_NULL_SHA256:
	cipherName = "TLS_DHE_PSK_WITH_NULL_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_DHE_PSK_WITH_NULL_SHA384:
	cipherName = "TLS_DHE_PSK_WITH_NULL_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_RSA_PSK_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_RSA_PSK_WITH_AES_128_CBC_SHA256";
	break;
    case TLS_RSA_PSK_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_RSA_PSK_WITH_AES_256_CBC_SHA384";
	break;
    case TLS_RSA_PSK_WITH_NULL_SHA256:
	cipherName = "TLS_RSA_PSK_WITH_NULL_SHA256";
	break;
    case TLS_RSA_PSK_WITH_NULL_SHA384:
	cipherName = "TLS_RSA_PSK_WITH_NULL_SHA384";
	break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256:
	cipherName = "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384:
	cipherName = "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256:
	cipherName = "TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256";
	paramsNeeded = 1;
	break;
    case TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384:
	cipherName = "TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384";
	paramsNeeded = 1;
	break;
    default :
        snprintf(unknownCipherName, sizeof(unknownCipherName), "UNKNOWN_%04X", cipher);
        cipherName = unknownCipherName;
        break;
  }

  if (cipher == TLS_RSA_WITH_RC4_128_MD5 ||
      cipher == TLS_RSA_WITH_RC4_128_SHA)
  {
    printf("%s: ERROR (Insecure RC4 negotiated)\n", server);
    httpClose(http);
    return (1);
  }

  if ((err = SSLGetDiffieHellmanParams(http->tls, &params, &paramsLen)) != noErr && paramsNeeded)
  {
    printf("%s: ERROR (Unable to get Diffie Hellman parameters - %d)\n", server, (int)err);
    httpClose(http);
    return (1);
  }

  if (paramsLen < 128 && paramsLen != 0)
  {
    printf("%s: ERROR (Diffie Hellman parameters only %d bytes/%d bits)\n", server, (int)paramsLen, (int)paramsLen * 8);
    httpClose(http);
    return (1);
  }
#endif /* __APPLE__ */

  printf("%s: OK (%d.%d, %s)\n", server, tlsVersion / 10, tlsVersion % 10, cipherName);

  httpClose(http);

  return (0);
}


/*
 * End of "$Id$".
 */
