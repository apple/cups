/*
 *   Private SSPI definitions for CUPS.
 *
 *   Copyright 2010 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#ifndef _CUPS_SSPI_PRIVATE_H_
#	define _CUPS_SSPI_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <config.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <wincrypt.h>
#  include <wintrust.h>
#  include <schannel.h>
#  define SECURITY_WIN32
#  include <security.h>
#  include <sspi.h>

/*
 * C++ magic...
 */

#	ifdef __cplusplus
extern "C" {
#	endif /* __cplusplus */


typedef struct						/**** SSPI/SSL data structure ****/
{
  SOCKET			sock;			/* TCP/IP socket */
  CredHandle			creds;			/* Credentials */
  CtxtHandle			context;		/* SSL context */
  BOOL				contextInitialized;	/* Is context init'd? */
  SecPkgContext_StreamSizes	streamSizes;		/* SSL data stream sizes */
  BYTE				*decryptBuffer;		/* Data pre-decryption*/
  size_t			decryptBufferLength;	/* Length of decrypt buffer */
  size_t			decryptBufferUsed;	/* Bytes used in buffer */
  BYTE				*readBuffer;		/* Data post-decryption */
  size_t			readBufferLength;	/* Length of read buffer */
  size_t			readBufferUsed;		/* Bytes used in buffer */
  DWORD				certFlags;		/* Cert verification flags */
} _sspi_struct_t;


/*
 * Prototypes...
 */
_sspi_struct_t	*_sspiAlloc(void);
BOOL		_sspiAccept(_sspi_struct_t *conn);
BOOL		_sspiConnect(_sspi_struct_t *conn,
		             const CHAR *hostname);
void		_sspiFree(_sspi_struct_t *conn);
BOOL		_sspiGetCredentials(_sspi_struct_t *conn,
		                    const LPWSTR containerName,
		                    const TCHAR  *commonName,
		                    BOOL server);
int		_sspiPending(_sspi_struct_t *conn);
int		_sspiRead(_sspi_struct_t *conn,
		          void *buf, size_t len);
void		_sspiSetAllowsAnyRoot(_sspi_struct_t *conn,
		                      BOOL allow);
void		_sspiSetAllowsExpiredCerts(_sspi_struct_t *conn,
		                           BOOL allow);
int		_sspiWrite(_sspi_struct_t *conn,
		           void *buf, size_t len);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_SSPI_PRIVATE_H_ */
