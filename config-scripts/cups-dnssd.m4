dnl
dnl "$Id: cups-dnssd.m4 11324 2013-10-04 03:11:42Z msweet $"
dnl
dnl   DNS Service Discovery (aka Bonjour) stuff for CUPS.
dnl
dnl   Copyright 2007-2012 by Apple Inc.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(avahi, [  --disable-avahi         disable DNS Service Discovery support using Avahi])
AC_ARG_ENABLE(dnssd, [  --disable-dnssd         disable DNS Service Discovery support using mDNSResponder])
AC_ARG_WITH(dnssd-libs, [  --with-dnssd-libs       set directory for DNS Service Discovery library],
	LDFLAGS="-L$withval $LDFLAGS"
	DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(dnssd-includes, [  --with-dnssd-includes   set directory for DNS Service Discovery includes],
	CFLAGS="-I$withval $CFLAGS"
	CPPFLAGS="-I$withval $CPPFLAGS",)

DNSSDLIBS=""
DNSSD_BACKEND=""
IPPFIND_BIN=""
IPPFIND_MAN=""

if test "x$PKGCONFIG" != x -a x$enable_avahi != xno; then
	AC_MSG_CHECKING(for Avahi)
	if $PKGCONFIG --exists avahi-client; then
		AC_MSG_RESULT(yes)
		CFLAGS="$CFLAGS `$PKGCONFIG --cflags avahi-client`"
		DNSSDLIBS="`$PKGCONFIG --libs avahi-client`"
		DNSSD_BACKEND="dnssd"
		IPPFIND_BIN="ippfind"
		IPPFIND_MAN="ippfind.\$(MAN1EXT)"
		AC_DEFINE(HAVE_AVAHI)
	else
		AC_MSG_RESULT(no)
	fi
fi

if test "x$DNSSD_BACKEND" = x -a x$enable_dnssd != xno; then
	AC_CHECK_HEADER(dns_sd.h, [
		case "$uname" in
			Darwin*)
				# Darwin and MacOS X...
				AC_DEFINE(HAVE_DNSSD)
				DNSSDLIBS="-framework CoreFoundation -framework SystemConfiguration"
				DNSSD_BACKEND="dnssd"
				IPPFIND_BIN="ippfind"
				IPPFIND_MAN="ippfind.\$(MAN1EXT)"
				;;
			*)
				# All others...
				AC_MSG_CHECKING(for current version of dns_sd library)
				SAVELIBS="$LIBS"
				LIBS="$LIBS -ldns_sd"
				AC_TRY_COMPILE([#include <dns_sd.h>],
					[int constant = kDNSServiceFlagsShareConnection;
					unsigned char txtRecord[100];
					uint8_t valueLen;
					TXTRecordGetValuePtr(sizeof(txtRecord),
					    txtRecord, "value", &valueLen);],
					AC_MSG_RESULT(yes)
					AC_DEFINE(HAVE_DNSSD)
					DNSSDLIBS="-ldns_sd"
					DNSSD_BACKEND="dnssd",
					IPPFIND_BIN="ippfind"
					IPPFIND_MAN="ippfind.\$(MAN1EXT)"
					AC_MSG_RESULT(no))
				LIBS="$SAVELIBS"
				;;
		esac
	])
fi

AC_SUBST(DNSSDLIBS)
AC_SUBST(DNSSD_BACKEND)
AC_SUBST(IPPFIND_BIN)
AC_SUBST(IPPFIND_MAN)

dnl
dnl End of "$Id: cups-dnssd.m4 11324 2013-10-04 03:11:42Z msweet $".
dnl
