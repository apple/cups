dnl
dnl "$Id$"
dnl
dnl   Networking stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007-2008 by Apple Inc.
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_CHECK_HEADER(resolv.h,AC_DEFINE(HAVE_RESOLV_H))
AC_SEARCH_LIBS(socket, socket)
AC_SEARCH_LIBS(gethostbyaddr, nsl)
AC_SEARCH_LIBS(getifaddrs, nsl, AC_DEFINE(HAVE_GETIFADDRS))
AC_SEARCH_LIBS(hstrerror, nsl socket resolv, AC_DEFINE(HAVE_HSTRERROR))
AC_SEARCH_LIBS(rresvport_af, nsl, AC_DEFINE(HAVE_RRESVPORT_AF))
AC_SEARCH_LIBS(__res_init, resolv bind, AC_DEFINE(HAVE_RES_INIT),
	AC_SEARCH_LIBS(res_9_init, resolv bind, AC_DEFINE(HAVE_RES_INIT),
	AC_SEARCH_LIBS(res_init, resolv bind, AC_DEFINE(HAVE_RES_INIT))))

# Tru64 5.1b leaks file descriptors with these functions; disable until
# we can come up with a test for this...
if test "$uname" != "OSF1"; then
	AC_SEARCH_LIBS(getaddrinfo, nsl, AC_DEFINE(HAVE_GETADDRINFO))
	AC_SEARCH_LIBS(getnameinfo, nsl, AC_DEFINE(HAVE_GETNAMEINFO))
fi

AC_CHECK_MEMBER(struct sockaddr.sa_len,,, [#include <sys/socket.h>])
AC_CHECK_HEADER(sys/sockio.h, AC_DEFINE(HAVE_SYS_SOCKIO_H))

CUPS_DEFAULT_DOMAINSOCKET=""

dnl Domain socket support...
AC_ARG_WITH(domainsocket, [  --with-domainsocket     set unix domain socket name],
	default_domainsocket="$withval",
	default_domainsocket="")

if test x$enable_domainsocket != xno -a x$default_domainsocket != xno; then
	if test "x$default_domainsocket" = x; then
		case "$uname" in
			Darwin*)
				# Darwin and MaxOS X do their own thing...
				CUPS_DEFAULT_DOMAINSOCKET="$localstatedir/run/cupsd"
				;;
			*)
				# All others use FHS standard...
				CUPS_DEFAULT_DOMAINSOCKET="$CUPS_STATEDIR/cups.sock"
				;;
		esac
	else
		CUPS_DEFAULT_DOMAINSOCKET="$default_domainsocket"
	fi

	CUPS_LISTEN_DOMAINSOCKET="Listen $CUPS_DEFAULT_DOMAINSOCKET"

	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DOMAINSOCKET, "$CUPS_DEFAULT_DOMAINSOCKET")
else
	CUPS_LISTEN_DOMAINSOCKET=""
fi

AC_SUBST(CUPS_DEFAULT_DOMAINSOCKET)
AC_SUBST(CUPS_LISTEN_DOMAINSOCKET)

AC_CHECK_HEADERS(AppleTalk/at_proto.h,AC_DEFINE(HAVE_APPLETALK_AT_PROTO_H),,
	[#include <netat/appletalk.h>])

dnl
dnl End of "$Id$".
dnl
