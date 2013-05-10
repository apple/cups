/*
 * "$Id$"
 *
 *   Private HTTP definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_HTTP_PRIVATE_H_
#  define _CUPS_HTTP_PRIVATE_H_

/*
 * Include necessary headers...
 */

#  include <stdlib.h>
#  include <config.h>

#  ifdef __sun
/*
 * Define FD_SETSIZE to CUPS_MAX_FDS on Solaris to get the correct version of
 * select() for large numbers of file descriptors.
 */

#    define FD_SETSIZE	CUPS_MAX_FDS
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

#  if defined(__sgi) || (defined(__APPLE__) && !defined(_SOCKLEN_T))
/*
 * IRIX and MacOS X 10.2.x do not define socklen_t, and in fact use an int instead of
 * unsigned type for length values...
 */

typedef int socklen_t;
#  endif /* __sgi || (__APPLE__ && !_SOCKLEN_T) */

#  include "http.h"
#  include "ipp-private.h"

#  if defined HAVE_LIBSSL
/*
 * The OpenSSL library provides its own SSL/TLS context structure for its
 * IO and protocol management...
 */

#    include <openssl/err.h>
#    include <openssl/rand.h>
#    include <openssl/ssl.h>

typedef SSL http_tls_t;

#  elif defined HAVE_GNUTLS
/*
 * The GNU TLS library is more of a "bare metal" SSL/TLS library...
 */
#    include <gnutls/gnutls.h>

typedef struct
{
  gnutls_session	session;	/* GNU TLS session object */
  void			*credentials;	/* GNU TLS credentials object */
} http_tls_t;

#  elif defined(HAVE_CDSASSL)
/*
 * Darwin's Security framework provides its own SSL/TLS context structure
 * for its IO and protocol management...
 */

#    include <Security/SecureTransport.h>

typedef struct				/**** CDSA connection information ****/
{
  SSLContextRef		session;	/* CDSA session object */
  CFArrayRef		certsArray;	/* Certificates array */
} http_tls_t;

typedef union _cdsa_conn_ref_u		/**** CDSA Connection reference union
					 **** used to resolve 64-bit casting
					 **** warnings.
					 ****/
{
  SSLConnectionRef connection;		/* SSL connection pointer */
  int		   sock;		/* Socket */
} cdsa_conn_ref_t;

extern OSStatus	_httpReadCDSA(SSLConnectionRef connection, void *data,
		              size_t *dataLength);
extern OSStatus	_httpWriteCDSA(SSLConnectionRef connection, const void *data,
		               size_t *dataLength);
#  endif /* HAVE_LIBSSL */

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

#  include <net/if.h>
#  ifdef HAVE_GETIFADDRS
#    include <ifaddrs.h>
#  else
#    include <sys/ioctl.h>
#    ifdef HAVE_SYS_SOCKIO_H
#      include <sys/sockio.h>
#    endif /* HAVE_SYS_SOCKIO_H */

#  ifdef ifa_dstaddr
#    undef ifa_dstaddr
#  endif /* ifa_dstaddr */
#  ifndef ifr_netmask
#    define ifr_netmask ifr_addr
#  endif /* !ifr_netmask */

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

#  ifndef ifa_broadaddr
#    define ifa_broadaddr ifa_ifu.ifu_broadaddr
#  endif /* !ifa_broadaddr */
#  ifndef ifa_dstaddr
#    define ifa_dstaddr ifa_ifu.ifu_dstaddr
#  endif /* !ifa_dstaddr */

extern int	_cups_getifaddrs(struct ifaddrs **addrs);
#    define getifaddrs _cups_getifaddrs
extern void	_cups_freeifaddrs(struct ifaddrs *addrs);
#    define freeifaddrs _cups_freeifaddrs
#  endif /* HAVE_GETIFADDRS */

#endif /* !_CUPS_HTTP_PRIVATE_H_ */

/*
 * End of "$Id$".
 */
