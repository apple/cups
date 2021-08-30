dnl
dnl DNS Service Discovery (aka Bonjour) stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2019 by Apple Inc.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

AC_ARG_WITH([dnssd], AS_HELP_STRING([--with-dnssd=...], [enable DNS Service Discovery support (avahi, mdnsresponder, no, yes)]))
AS_IF([test x$with_dnssd = x], [
    with_dnssd="yes"
], [test "$with_dnssd" != avahi -a "$with_dnssd" != mdnsresponder -a "$with_dnssd" != no -a "$with_dnssd" != yes], [
    AC_MSG_ERROR([Unsupported --with-dnssd value "$with_dnssd".])
])
AC_ARG_WITH([dnssd_libs], AS_HELP_STRING([--with-dnssd-libs], [set directory for DNS Service Discovery library]), [
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS"
])
AC_ARG_WITH([dnssd_includes], AS_HELP_STRING([--with-dnssd-includes], [set directory for DNS Service Discovery header files]), [
    CFLAGS="-I$withval $CFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS"
])

DNSSDLIBS=""
DNSSD_BACKEND=""
IPPFIND_BIN=""
IPPFIND_MAN=""

dnl First try using mDNSResponder...
AS_IF([test $with_dnssd = yes -o $with_dnssd = mdnsresponder], [
    AC_CHECK_HEADER([dns_sd.h], [
	AS_CASE(["$host_os_name"], [darwin*], [
	    # Darwin and macOS...
	    with_dnssd="mdnsresponder"
	    AC_DEFINE([HAVE_DNSSD], [1], [Have DNS-SD support?])
	    AC_DEFINE([HAVE_MDNSRESPONDER], [1], [Have mDNSResponder library?])
	    DNSSD_BACKEND="dnssd"
	    IPPFIND_BIN="ippfind"
	    IPPFIND_MAN="ippfind.1"
	], [*], [
	    # All others...
	    AC_MSG_CHECKING([for current version of dns_sd library])
	    SAVELIBS="$LIBS"
	    LIBS="$LIBS -ldns_sd"
	    AC_COMPILE_IFELSE([
	        AC_LANG_PROGRAM([[#include <dns_sd.h>]], [[
		    int constant = kDNSServiceFlagsShareConnection;
		    unsigned char txtRecord[100];
		    uint8_t valueLen;
		    TXTRecordGetValuePtr(sizeof(txtRecord), txtRecord, "value", &valueLen);
		]])
	    ], [
		AC_MSG_RESULT([yes])
		with_dnssd="mdnsresponder"
		AC_DEFINE([HAVE_DNSSD], [1], [Have DNS-SD support?])
		AC_DEFINE([HAVE_MDNSRESPONDER], [1], [Have mDNSResponder library?])
		DNSSDLIBS="-ldns_sd"
		DNSSD_BACKEND="dnssd"
		IPPFIND_BIN="ippfind"
		IPPFIND_MAN="ippfind.1"
		PKGCONFIG_LIBS_STATIC="$PKGCONFIG_LIBS_STATIC $DNSSDLIBS"
	    ], [
		AC_MSG_RESULT([no])
		AS_IF([test $with_dnssd = mdnsresponder], [
		    AC_MSG_ERROR([--with-dnssd=mdnsresponder specified but dns_sd library not present.])
		])
	    ])
	    LIBS="$SAVELIBS"
	])
    ])
])

dnl Then try Avahi...
AS_IF([test $with_dnssd = avahi -o $with_dnssd = yes], [
    AS_IF([test "x$PKGCONFIG" = x], [
	AS_IF([test $with_dnssd = avahi], [
	    AC_MSG_ERROR([Avahi requires pkg-config.])
	])
    ], [
	AC_MSG_CHECKING([for Avahi client])
	AS_IF([$PKGCONFIG --exists avahi-client], [
	    AC_MSG_RESULT([yes])
	    CFLAGS="$CFLAGS `$PKGCONFIG --cflags avahi-client`"
	    DNSSDLIBS="`$PKGCONFIG --libs avahi-client`"
	    DNSSD_BACKEND="dnssd"
	    IPPFIND_BIN="ippfind"
	    IPPFIND_MAN="ippfind.1"
		PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES avahi-client"
	    AC_DEFINE([HAVE_AVAHI], [1], [Have Avahi client library?])
	    AC_DEFINE([HAVE_DNSSD], [1], [Have DNS-SD support?])
	], [
	    AC_MSG_RESULT([no])
	    AS_IF([test $with_dnssd = avahi], [
		AC_MSG_ERROR([--with-dnssd=avahi specified but Avahi client not present.])
	    ])
	])
    ])
])

AC_SUBST([DNSSDLIBS])
AC_SUBST([DNSSD_BACKEND])
AC_SUBST([IPPFIND_BIN])
AC_SUBST([IPPFIND_MAN])
