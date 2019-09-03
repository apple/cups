dnl
dnl TLS stuff for CUPS.
dnl
dnl Copyright 2007-2019 by Apple Inc.
dnl Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
dnl

AC_ARG_ENABLE(ssl, [  --disable-ssl           disable SSL/TLS support])
AC_ARG_ENABLE(cdsassl, [  --enable-cdsassl        use CDSA for SSL/TLS support, default=first])
AC_ARG_ENABLE(gnutls, [  --enable-gnutls         use GNU TLS for SSL/TLS support, default=second])

SSLFLAGS=""
SSLLIBS=""
have_ssl=0
CUPS_SERVERKEYCHAIN=""

if test x$enable_ssl != xno; then
    dnl Look for CDSA...
    if test $have_ssl = 0 -a "x$enable_cdsassl" != "xno"; then
	if test $host_os_name = darwin; then
	    AC_CHECK_HEADER(Security/SecureTransport.h, [
	    	have_ssl=1
		AC_DEFINE(HAVE_SSL)
		AC_DEFINE(HAVE_CDSASSL)
		CUPS_SERVERKEYCHAIN="/Library/Keychains/System.keychain"

		dnl Check for the various security headers...
		AC_CHECK_HEADER(Security/SecCertificate.h,
		    AC_DEFINE(HAVE_SECCERTIFICATE_H))
		AC_CHECK_HEADER(Security/SecItem.h,
		    AC_DEFINE(HAVE_SECITEM_H))
		AC_CHECK_HEADER(Security/SecPolicy.h,
		    AC_DEFINE(HAVE_SECPOLICY_H))])
	fi
    fi

    dnl Then look for GNU TLS...
    if test $have_ssl = 0 -a "x$enable_gnutls" != "xno" -a "x$PKGCONFIG" != x; then
    	AC_PATH_TOOL(LIBGNUTLSCONFIG,libgnutls-config)
	if $PKGCONFIG --exists gnutls; then
	    have_ssl=1
	    SSLLIBS=`$PKGCONFIG --libs gnutls`
	    SSLFLAGS=`$PKGCONFIG --cflags gnutls`
	    AC_DEFINE(HAVE_SSL)
	    AC_DEFINE(HAVE_GNUTLS)
	elif test "x$LIBGNUTLSCONFIG" != x; then
	    have_ssl=1
	    SSLLIBS=`$LIBGNUTLSCONFIG --libs`
	    SSLFLAGS=`$LIBGNUTLSCONFIG --cflags`
	    AC_DEFINE(HAVE_SSL)
	    AC_DEFINE(HAVE_GNUTLS)
	fi

	if test $have_ssl = 1; then
	    CUPS_SERVERKEYCHAIN="ssl"

	    SAVELIBS="$LIBS"
	    LIBS="$LIBS $SSLLIBS"
	    AC_CHECK_FUNC(gnutls_transport_set_pull_timeout_function, AC_DEFINE(HAVE_GNUTLS_TRANSPORT_SET_PULL_TIMEOUT_FUNCTION))
	    AC_CHECK_FUNC(gnutls_priority_set_direct, AC_DEFINE(HAVE_GNUTLS_PRIORITY_SET_DIRECT))
	    LIBS="$SAVELIBS"
	fi
    fi
fi

IPPALIASES="http"
if test $have_ssl = 1; then
    AC_MSG_RESULT([    Using SSLLIBS="$SSLLIBS"])
    AC_MSG_RESULT([    Using SSLFLAGS="$SSLFLAGS"])
    IPPALIASES="http https ipps"
elif test x$enable_cdsa = xyes -o x$enable_gnutls = xyes; then
    AC_MSG_ERROR([Unable to enable SSL support.])
fi

AC_SUBST(CUPS_SERVERKEYCHAIN)
AC_SUBST(IPPALIASES)
AC_SUBST(SSLFLAGS)
AC_SUBST(SSLLIBS)

EXPORT_SSLLIBS="$SSLLIBS"
AC_SUBST(EXPORT_SSLLIBS)
