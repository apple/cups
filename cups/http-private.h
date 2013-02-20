/*
 * "$Id: http-private.h 7850 2008-08-20 00:07:25Z mike $"
 *
 *   Private HTTP definitions for CUPS.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_HTTP_PRIVATE_H_
#  define _CUPS_HTTP_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include "config.h"
#  include <stddef.h>
#  include <stdlib.h>

#  ifdef __sun
#    include <sys/select.h>
#  endif /* __sun */

#  include <limits.h>
#  ifdef WIN32
#    include <io.h>
#    include <winsock2.h>
#  else
#    include <unistd.h>
#    include <fcntl.h>
#    include <sys/socket.h>
#    define closesocket(f) close(f)
#  endif /* WIN32 */

#  ifdef HAVE_GSSAPI
#    ifdef HAVE_GSS_GSSAPI_H
#      include <GSS/gssapi.h>
#    elif defined(HAVE_GSSAPI_GSSAPI_H)
#      include <gssapi/gssapi.h>
#    elif defined(HAVE_GSSAPI_H)
#      include <gssapi.h>
#    endif /* HAVE_GSS_GSSAPI_H */
#    ifndef HAVE_GSS_C_NT_HOSTBASED_SERVICE
#      define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
#    endif /* !HAVE_GSS_C_NT_HOSTBASED_SERVICE */
#  endif /* HAVE_GSSAPI */

#  ifdef HAVE_AUTHORIZATION_H
#    include <Security/Authorization.h>
#  endif /* HAVE_AUTHORIZATION_H */

#  if defined(__sgi) || (defined(__APPLE__) && !defined(_SOCKLEN_T))
/*
 * IRIX and MacOS X 10.2.x do not define socklen_t, and in fact use an int instead of
 * unsigned type for length values...
 */

typedef int socklen_t;
#  endif /* __sgi || (__APPLE__ && !_SOCKLEN_T) */

#  include <cups/http.h>
#  include "md5-private.h"
#  include "ipp-private.h"

#  if defined HAVE_LIBSSL
#    include <openssl/err.h>
#    include <openssl/rand.h>
#    include <openssl/ssl.h>
#  elif defined HAVE_GNUTLS
#    include <gnutls/gnutls.h>
#    include <gnutls/x509.h>
#    include <gcrypt.h>
#  elif defined(HAVE_CDSASSL)
#    include <CoreFoundation/CoreFoundation.h>
#    include <Security/Security.h>
#    include <Security/SecureTransport.h>
#    ifdef HAVE_SECURETRANSPORTPRIV_H
#      include <Security/SecureTransportPriv.h>
#    endif /* HAVE_SECURETRANSPORTPRIV_H */
#    ifdef HAVE_SECITEM_H
#      include <Security/SecItem.h>
#    endif /* HAVE_SECITEM_H */
#    ifdef HAVE_SECBASEPRIV_H
#      include <Security/SecBasePriv.h>
#    endif /* HAVE_SECBASEPRIV_H */
#    ifdef HAVE_SECCERTIFICATE_H
#      include <Security/SecCertificate.h>
#      include <Security/SecIdentity.h>
#    endif /* HAVE_SECCERTIFICATE_H */
#    ifdef HAVE_SECITEMPRIV_H
#      include <Security/SecItemPriv.h>
#    endif /* HAVE_SECITEMPRIV_H */
#    ifdef HAVE_SECIDENTITYSEARCHPRIV_H
#      include <Security/SecIdentitySearchPriv.h>
#    endif /* HAVE_SECIDENTITYSEARCHPRIV_H */
#    ifdef HAVE_SECPOLICYPRIV_H
#      include <Security/SecPolicyPriv.h>
#    endif /* HAVE_SECPOLICYPRIV_H */
#  elif defined(HAVE_SSPISSL)
#    include "sspi-private.h"
#  endif /* HAVE_LIBSSL */

#  ifndef WIN32
#    include <net/if.h>
#    ifdef HAVE_GETIFADDRS
#      include <ifaddrs.h>
#    else
#      include <sys/ioctl.h>
#      ifdef HAVE_SYS_SOCKIO_H
#        include <sys/sockio.h>
#      endif /* HAVE_SYS_SOCKIO_H */
#    endif /* HAVE_GETIFADDRS */
#  endif /* !WIN32 */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Constants...
 */


#define _HTTP_RESOLVE_DEFAULT	0	/* Just resolve with default options */
#define _HTTP_RESOLVE_STDERR	1	/* Log resolve progress to stderr */
#define _HTTP_RESOLVE_FQDN	2	/* Resolve to a FQDN */
#define _HTTP_RESOLVE_FAXOUT	4	/* Resolve FaxOut service? */


/*
 * Types and functions for SSL support...
 */

#  if defined HAVE_LIBSSL
/*
 * The OpenSSL library provides its own SSL/TLS context structure for its
 * IO and protocol management.  However, we need to provide our own BIO
 * (basic IO) implementation to do timeouts...
 */

typedef SSL  *http_tls_t;
typedef void *http_tls_credentials_t;

extern BIO_METHOD *_httpBIOMethods(void);

#  elif defined HAVE_GNUTLS
/*
 * The GNU TLS library is more of a "bare metal" SSL/TLS library...
 */

typedef gnutls_session http_tls_t;
typedef void *http_tls_credentials_t;

extern ssize_t	_httpReadGNUTLS(gnutls_transport_ptr ptr, void *data,
		                size_t length);
extern ssize_t	_httpWriteGNUTLS(gnutls_transport_ptr ptr, const void *data,
		                 size_t length);

#  elif defined(HAVE_CDSASSL)
/*
 * Darwin's Security framework provides its own SSL/TLS context structure
 * for its IO and protocol management...
 */

#    if !defined(HAVE_SECBASEPRIV_H) && defined(HAVE_CSSMERRORSTRING) /* Declare prototype for function in that header... */
extern const char *cssmErrorString(int error);
#    endif /* !HAVE_SECBASEPRIV_H && HAVE_CSSMERRORSTRING */
#    ifndef HAVE_SECITEMPRIV_H /* Declare constants from that header... */
extern const CFTypeRef kSecClassCertificate;
extern const CFTypeRef kSecClassIdentity;
#    endif /* !HAVE_SECITEMPRIV_H */
#    if !defined(HAVE_SECIDENTITYSEARCHPRIV_H) && defined(HAVE_SECIDENTITYSEARCHCREATEWITHPOLICY) /* Declare prototype for function in that header... */
extern OSStatus SecIdentitySearchCreateWithPolicy(SecPolicyRef policy,
				CFStringRef idString, CSSM_KEYUSE keyUsage,
				CFTypeRef keychainOrArray,
				Boolean returnOnlyValidIdentities,
				SecIdentitySearchRef* searchRef);
#    endif /* !HAVE_SECIDENTITYSEARCHPRIV_H && HAVE_SECIDENTITYSEARCHCREATEWITHPOLICY */
#    if !defined(HAVE_SECPOLICYPRIV_H) && defined(HAVE_SECPOLICYSETVALUE) /* Declare prototype for function in that header... */
extern OSStatus SecPolicySetValue(SecPolicyRef policyRef,
                                  const CSSM_DATA *value);
#    endif /* !HAVE_SECPOLICYPRIV_H && HAVE_SECPOLICYSETVALUE */

typedef SSLContextRef	http_tls_t;
typedef CFArrayRef	http_tls_credentials_t;

extern OSStatus	_httpReadCDSA(SSLConnectionRef connection, void *data,
		              size_t *dataLength);
extern OSStatus	_httpWriteCDSA(SSLConnectionRef connection, const void *data,
		               size_t *dataLength);

#  elif defined(HAVE_SSPISSL)
/*
 * Windows' SSPI library gets a CUPS wrapper...
 */

typedef _sspi_struct_t * http_tls_t;
typedef void *http_tls_credentials_t;

#  else
/*
 * Otherwise define stub types since we have no SSL support...
 */

typedef void *http_tls_t;
typedef void *http_tls_credentials_t;
#  endif /* HAVE_LIBSSL */

struct _http_s				/**** HTTP connection structure. ****/
{
  int			fd;		/* File descriptor for this socket */
  int			blocking;	/* To block or not to block */
  int			error;		/* Last error on read */
  time_t		activity;	/* Time since last read/write */
  http_state_t		state;		/* State of client */
  http_status_t		status;		/* Status of last request */
  http_version_t	version;	/* Protocol version */
  http_keepalive_t	keep_alive;	/* Keep-alive supported? */
  struct sockaddr_in	_hostaddr;	/* Address of connected host (deprecated) */
  char			hostname[HTTP_MAX_HOST],
  					/* Name of connected host */
			fields[HTTP_FIELD_MAX][HTTP_MAX_VALUE];
					/* Field values */
  char			*data;		/* Pointer to data buffer */
  http_encoding_t	data_encoding;	/* Chunked or not */
  int			_data_remaining;/* Number of bytes left (deprecated) */
  int			used;		/* Number of bytes used in buffer */
  char			buffer[HTTP_MAX_BUFFER];
					/* Buffer for incoming data */
  int			auth_type;	/* Authentication in use */
  _cups_md5_state_t	md5_state;	/* MD5 state */
  char			nonce[HTTP_MAX_VALUE];
					/* Nonce value */
  int			nonce_count;	/* Nonce count */
  http_tls_t		tls;		/* TLS state information */
  http_encryption_t	encryption;	/* Encryption requirements */
  /**** New in CUPS 1.1.19 ****/
  fd_set		*input_set;	/* select() set for httpWait() (deprecated) */
  http_status_t		expect;		/* Expect: header */
  char			*cookie;	/* Cookie value(s) */
  /**** New in CUPS 1.1.20 ****/
  char			_authstring[HTTP_MAX_VALUE],
					/* Current Authentication value (deprecated) */
			userpass[HTTP_MAX_VALUE];
					/* Username:password string */
  int			digest_tries;	/* Number of tries for digest auth */
  /**** New in CUPS 1.2 ****/
  off_t			data_remaining;	/* Number of bytes left */
  http_addr_t		*hostaddr;	/* Current host address and port */
  http_addrlist_t	*addrlist;	/* List of valid addresses */
  char			wbuffer[HTTP_MAX_BUFFER];
					/* Buffer for outgoing data */
  int			wused;		/* Write buffer bytes used */
  /**** New in CUPS 1.3 ****/
  char			*field_authorization;
					/* Authorization field */
  char			*authstring;	/* Current authorization field */
#  ifdef HAVE_GSSAPI
  gss_OID 		gssmech;	/* Authentication mechanism */
  gss_ctx_id_t		gssctx;		/* Authentication context */
  gss_name_t		gssname;	/* Authentication server name */
#  endif /* HAVE_GSSAPI */
#  ifdef HAVE_AUTHORIZATION_H
  AuthorizationRef	auth_ref;	/* Authorization ref */
#  endif /* HAVE_AUTHORIZATION_H */
  /**** New in CUPS 1.5 ****/
  http_tls_credentials_t tls_credentials;
					/* TLS credentials */
  http_timeout_cb_t	timeout_cb;	/* Timeout callback */
  void			*timeout_data;	/* User data pointer */
  double		timeout_value;	/* Timeout in seconds */
  int			wait_value;	/* httpWait value for timeout */
#  ifdef HAVE_GSSAPI
  char			gsshost[256];	/* Hostname for Kerberos */
#  endif /* HAVE_GSSAPI */
};


/*
 * Some OS's don't have hstrerror(), most notably Solaris...
 */

#  ifndef HAVE_HSTRERROR
extern const char *_cups_hstrerror(int error);
#    define hstrerror _cups_hstrerror
#  elif defined(_AIX) || defined(__osf__)
/*
 * AIX and Tru64 UNIX don't provide a prototype but do provide the function...
 */
extern const char *hstrerror(int error);
#  endif /* !HAVE_HSTRERROR */


/*
 * Some OS's don't have getifaddrs() and freeifaddrs()...
 */

#  if !defined(WIN32) && !defined(HAVE_GETIFADDRS)
#    ifdef ifa_dstaddr
#      undef ifa_dstaddr
#    endif /* ifa_dstaddr */
#    ifndef ifr_netmask
#      define ifr_netmask ifr_addr
#    endif /* !ifr_netmask */

struct ifaddrs				/**** Interface Structure ****/
{
  struct ifaddrs	*ifa_next;	/* Next interface in list */
  char			*ifa_name;	/* Name of interface */
  unsigned int		ifa_flags;	/* Flags (up, point-to-point, etc.) */
  struct sockaddr	*ifa_addr,	/* Network address */
			*ifa_netmask;	/* Address mask */
  union
  {
    struct sockaddr	*ifu_broadaddr;	/* Broadcast address of this interface. */
    struct sockaddr	*ifu_dstaddr;	/* Point-to-point destination address. */
  } ifa_ifu;

  void			*ifa_data;	/* Interface statistics */
};

#    ifndef ifa_broadaddr
#      define ifa_broadaddr ifa_ifu.ifu_broadaddr
#    endif /* !ifa_broadaddr */
#    ifndef ifa_dstaddr
#      define ifa_dstaddr ifa_ifu.ifu_dstaddr
#    endif /* !ifa_dstaddr */

extern int	_cups_getifaddrs(struct ifaddrs **addrs);
#    define getifaddrs _cups_getifaddrs
extern void	_cups_freeifaddrs(struct ifaddrs *addrs);
#    define freeifaddrs _cups_freeifaddrs
#  endif /* !WIN32 && !HAVE_GETIFADDRS */


/*
 * Prototypes...
 */

#define			_httpAddrFamily(addrp) (addrp)->addr.sa_family
extern int		_httpAddrPort(http_addr_t *addr);
extern void		_httpAddrSetPort(http_addr_t *addr, int port);
extern char		*_httpAssembleUUID(const char *server, int port,
					   const char *name, int number,
					   char *buffer, size_t bufsize);
extern http_t		*_httpCreate(const char *host, int port,
			             http_addrlist_t *addrlist,
				     http_encryption_t encryption,
				     int family);
extern http_tls_credentials_t
			_httpCreateCredentials(cups_array_t *credentials);
extern char		*_httpDecodeURI(char *dst, const char *src,
			                size_t dstsize);
extern void		_httpDisconnect(http_t *http);
extern char		*_httpEncodeURI(char *dst, const char *src,
			                size_t dstsize);
extern void		_httpFreeCredentials(http_tls_credentials_t credentials);
extern ssize_t		_httpPeek(http_t *http, char *buffer, size_t length);
extern const char	*_httpResolveURI(const char *uri, char *resolved_uri,
			                 size_t resolved_size, int options,
					 int (*cb)(void *context),
					 void *context);
extern int		_httpUpdate(http_t *http, http_status_t *status);
extern int		_httpWait(http_t *http, int msec, int usessl);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_HTTP_PRIVATE_H_ */

/*
 * End of "$Id: http-private.h 7850 2008-08-20 00:07:25Z mike $".
 */
