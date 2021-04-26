/*
 * getifaddrs definitions for CUPS.
 *
 * Copyright 2007-2018 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

#ifndef _CUPS_GETIFADDRS_INTERNAL_H_
#  define _CUPS_GETIFADDRS_INTERNAL_H_

/*
 * Include necessary headers...
 */

#  include "config.h"
#  ifdef _WIN32
#    define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#    include <io.h>
#    include <winsock2.h>
#    define CUPS_SOCAST (const char *)
#  else
#    include <unistd.h>
#    include <fcntl.h>
#    include <sys/socket.h>
#    include <netinet/in.h>
#    define CUPS_SOCAST
#  endif /* _WIN32 */

#  if defined(__APPLE__) && !defined(_SOCKLEN_T)
/*
 * macOS 10.2.x does not define socklen_t, and in fact uses an int instead of
 * unsigned type for length values...
 */

typedef int socklen_t;
#  endif /* __APPLE__ && !_SOCKLEN_T */

#  ifndef _WIN32
#    include <net/if.h>
#    include <resolv.h>
#    ifdef HAVE_GETIFADDRS
#      include <ifaddrs.h>
#    else
#      include <sys/ioctl.h>
#      ifdef HAVE_SYS_SOCKIO_H
#        include <sys/sockio.h>
#      endif /* HAVE_SYS_SOCKIO_H */
#    endif /* HAVE_GETIFADDRS */
#  endif /* !_WIN32 */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Some OS's don't have getifaddrs() and freeifaddrs()...
 */

#  if !defined(_WIN32) && !defined(HAVE_GETIFADDRS)
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

extern int	_cups_getifaddrs(struct ifaddrs **addrs) _CUPS_PRIVATE;
#    define getifaddrs _cups_getifaddrs
extern void	_cups_freeifaddrs(struct ifaddrs *addrs) _CUPS_PRIVATE;
#    define freeifaddrs _cups_freeifaddrs
#  endif /* !_WIN32 && !HAVE_GETIFADDRS */


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPS_GETIFADDRS_INTERNAL_H_ */
