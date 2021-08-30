dnl
dnl TLS stuff for CUPS.
dnl
dnl Copyright 2007-2019 by Apple Inc.
dnl Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
dnl

AC_ARG_ENABLE([ssl], AS_HELP_STRING([--disable-ssl], [disable SSL/TLS support]))
AC_ARG_ENABLE([cdsassl], AS_HELP_STRING([--enable-cdsassl], [use CDSA for SSL/TLS support, default=first]))
AC_ARG_ENABLE([gnutls], AS_HELP_STRING([--enable-gnutls], [use GNU TLS for SSL/TLS support, default=second]))

SSLFLAGS=""
SSLLIBS=""
have_ssl="0"
CUPS_SERVERKEYCHAIN=""

AS_IF([test "x$enable_ssl" != xno], [
    dnl Look for CDSA...
    AS_IF([test $have_ssl = 0 -a "x$enable_cdsassl" != "xno"], [
	AS_IF([test $host_os_name = darwin], [
	    AC_CHECK_HEADER(Security/SecureTransport.h, [
	    	have_ssl="1"
		AC_DEFINE([HAVE_SSL], [1], [Do we support TLS/SSL?])
		AC_DEFINE([HAVE_CDSASSL], [1], [Do we have the macOS SecureTransport API?])
		CUPS_SERVERKEYCHAIN="/Library/Keychains/System.keychain"

		dnl Check for the various security headers...
	    AC_CHECK_HEADER([Security/SecCertificate.h], [
		AC_DEFINE([HAVE_SECCERTIFICATE_H], [1], [Have the <Security/SecCertificate.h> header?])
	    ])
	    AC_CHECK_HEADER([Security/SecItem.h], [
		AC_DEFINE([HAVE_SECITEM_H], [1], [Have the <Security/SecItem.h> header?])
	    ])
	    AC_CHECK_HEADER([Security/SecPolicy.h], [
		AC_DEFINE([HAVE_SECPOLICY_H], [1], [Have the <Security/SecPolicy.h header?])
	    ])
	])
	])
	])

    dnl Then look for GNU TLS...
    AS_IF([test $have_ssl = 0 -a "x$enable_gnutls" != "xno" -a "x$PKGCONFIG" != x], [
    	AC_PATH_TOOL([LIBGNUTLSCONFIG],[libgnutls-config])
	AS_IF([test "x$PKGCONFIG" != x], [
        AC_MSG_CHECKING([for gnutls package])
	AS_IF([$PKGCONFIG --exists gnutls], [
	    AC_MSG_RESULT([yes])
	    have_ssl="1"
	    SSLLIBS="$($PKGCONFIG --libs gnutls)"
	    SSLFLAGS="$($PKGCONFIG --cflags gnutls)"
	    PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES gnutls"
	    AC_DEFINE([HAVE_SSL], [1], [Do we support SSL/TLS?])
	    AC_DEFINE([HAVE_GNUTLS], [1], [Do we have the GNU TLS library?])
	], [
	    AC_MSG_RESULT([no])
	])
    ])

	AS_IF([test $have_ssl = 0 -a "x$LIBGNUTLSCONFIG" != x], [
	have_ssl="1"
	SSLLIBS="$($LIBGNUTLSCONFIG --libs)"
	SSLFLAGS="$($LIBGNUTLSCONFIG --cflags)"
	PKGCONFIG_LIBS_STATIC="$PKGCONFIG_LIBS_STATIC $SSLLIBS"
	AC_DEFINE([HAVE_TLS], [1], [Do we support TLS?])
	AC_DEFINE([HAVE_GNUTLS], [1], [Do we have the GNU TLS library?])
    ])

	AS_IF([test $have_ssl = 1], [
	CUPS_SERVERKEYCHAIN="ssl"

	SAVELIBS="$LIBS"
	LIBS="$LIBS $SSLLIBS"
	AC_CHECK_FUNC([gnutls_transport_set_pull_timeout_function], [
	    AC_DEFINE([HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION], [1], [Do we have the gnutls_transport_set_pull_timeout_function function?])
	])
	AC_CHECK_FUNC([gnutls_priority_set_direct], [
	    AC_DEFINE([HAVE_GNUTLS_PRIORITY_SET_DIRECT], [1], [Do we have the gnutls_priority_set_direct function?])
	])
	LIBS="$SAVELIBS"
    ])
	])
])

IPPALIASES="http"
AS_IF([test $have_ssl = 1], [
    AC_MSG_NOTICE([    Using SSLLIBS="$SSLLIBS"])
    AC_MSG_NOTICE([    Using SSLFLAGS="$SSLFLAGS"])
    IPPALIASES="http https ipps"
], [test x$enable_cdsa = xyes -o x$enable_gnutls = xyes], [
    AC_MSG_ERROR([Unable to enable SSL support.])
])

AC_SUBST([CUPS_SERVERKEYCHAIN])
AC_SUBST([IPPALIASES])
AC_SUBST([SSLFLAGS])
AC_SUBST([SSLLIBS])

EXPORT_SSLLIBS="$SSLLIBS"
AC_SUBST([EXPORT_SSLLIBS])
