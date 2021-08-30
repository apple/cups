dnl
dnl Networking stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2016 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

AC_CHECK_HEADER([resolv.h], [
    AC_DEFINE([HAVE_RESOLV_H], [1], [Have the <resolv.h> header?])
], [
]. [
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netinet/in_systm.h>
    #include <netinet/ip.h>
])
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([gethostbyaddr], [nsl])
AC_SEARCH_LIBS([getifaddrs], [nsl], [
    AC_DEFINE([HAVE_GETIFADDRS], [1], [Have the getifaddrs function?])
])
AC_SEARCH_LIBS([hstrerror], [nsl socket resolv], [
    AC_DEFINE([HAVE_HSTRERROR], [1], [Have the hstrerror function?])
])
AC_SEARCH_LIBS([rresvport_af], [nsl], [
    AC_DEFINE([HAVE_RRESVPORT_AF], [1], [Have the rresvport_af function?])
])
AC_SEARCH_LIBS([__res_init], [resolv bind], [
    AC_DEFINE([HAVE_RES_INIT], [1], [Have res_init function?])
], [
    AC_SEARCH_LIBS([res_9_init], [resolv bind], [
        AC_DEFINE([HAVE_RES_INIT], [1], [Have res_init function?])
    ], [
	AC_SEARCH_LIBS([res_init], [resolv bind], [
	    AC_DEFINE([HAVE_RES_INIT], [1], [Have res_init function?])
	])
    ])
])

AC_SEARCH_LIBS([getaddrinfo], [nsl], [
    AC_DEFINE([HAVE_GETADDRINFO], [1], [Have the getaddrinfo function?])
])
AC_SEARCH_LIBS([getnameinfo], [nsl], [
    AC_DEFINE([HAVE_GETNAMEINFO], [1], [Have the getnameinfo function?])
])

AC_CHECK_MEMBER([struct sockaddr.sa_len],,, [#include <sys/socket.h>])
AC_CHECK_HEADER([sys/sockio.h], [
    AC_DEFINE([HAVE_SYS_SOCKIO_H], [1], [Have <sys/sockio.h> header?])
])

dnl Domain socket support...
CUPS_DEFAULT_DOMAINSOCKET=""

AC_ARG_WITH([domainsocket], AS_HELP_STRING([--with-domainsocket], [set unix domain socket name]), [
    default_domainsocket="$withval"
], [
    default_domainsocket=""
])

AS_IF([test x$enable_domainsocket != xno -a x$default_domainsocket != xno], [
    AS_IF([test "x$default_domainsocket" = x], [
        AS_CASE(["$host_os_name"], [darwin*], [
	    # Darwin and macOS do their own thing...
	    CUPS_DEFAULT_DOMAINSOCKET="$localstatedir/run/cupsd"
	], [*], [
	    # All others use FHS standard...
	    CUPS_DEFAULT_DOMAINSOCKET="$CUPS_STATEDIR/cups.sock"
	])
    ], [
	CUPS_DEFAULT_DOMAINSOCKET="$default_domainsocket"
    ])

    CUPS_LISTEN_DOMAINSOCKET="Listen $CUPS_DEFAULT_DOMAINSOCKET"

    AC_DEFINE_UNQUOTED([CUPS_DEFAULT_DOMAINSOCKET], ["$CUPS_DEFAULT_DOMAINSOCKET"], [Domain socket path.])
], [
    CUPS_LISTEN_DOMAINSOCKET=""
])

AC_SUBST([CUPS_DEFAULT_DOMAINSOCKET])
AC_SUBST([CUPS_LISTEN_DOMAINSOCKET])
