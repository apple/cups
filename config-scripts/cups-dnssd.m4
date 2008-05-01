dnl
dnl "$Id$"
dnl
dnl   DNS Service Discovery (aka Bonjour) stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   http://www.dns-sd.org
dnl   http://www.multicastdns.org/
dnl   http://developer.apple.com/networking/bonjour/
dnl
dnl   Copyright 2007-2008 by Apple Inc.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(dnssd, [  --enable-dnssd          turn on DNS Service Discovery support, default=yes])
AC_ARG_WITH(dnssd-libs, [  --with-dnssd-libs       set directory for DNS Service Discovery library],
	LDFLAGS="-L$withval $LDFLAGS"
	DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(dnssd-includes, [  --with-dnssd-includes   set directory for DNS Service Discovery includes],
	CFLAGS="-I$withval $CFLAGS"
	CPPFLAGS="-I$withval $CPPFLAGS",)

DNSSDLIBS=""
MDNS=""

if test x$enable_dnssd != xno; then
	AC_CHECK_HEADER(dns_sd.h, [
		case "$uname" in
			Darwin*)
				# Darwin and MacOS X...
				DNSSDLIBS="-framework CoreFoundation -framework SystemConfiguration"
				AC_DEFINE(HAVE_DNSSD)
				AC_DEFINE(HAVE_COREFOUNDATION)
				AC_DEFINE(HAVE_SYSTEMCONFIGURATION)
				MDNS="mdns"
				;;
			*)
				# All others...
				AC_CHECK_LIB(dns_sd,DNSServiceCreateConnection,
					AC_DEFINE(HAVE_DNSSD)
					DNSSDLIBS="-ldns_sd")
				;;
		esac
	])
fi

AC_SUBST(DNSSDLIBS)
AC_SUBST(MDNS)

dnl
dnl End of "$Id$".
dnl
