dnl
dnl "$Id: cups-ssl.m4 12645 2015-05-20 01:20:52Z msweet $"
dnl
dnl TLS stuff for CUPS.
dnl
dnl Copyright 2007-2015 by Apple Inc.
dnl Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl These coded instructions, statements, and computer programs are the
dnl property of Apple Inc. and are protected by Federal copyright
dnl law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl which should have been included with this file.  If this file is
dnl file is missing or damaged, see the license at "http://www.cups.org/".
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
	if test $uname = Darwin; then
	    AC_CHECK_HEADER(Security/SecureTransport.h, [
	    	have_ssl=1
		AC_DEFINE(HAVE_SSL)
		AC_DEFINE(HAVE_CDSASSL)
		CUPS_SERVERKEYCHAIN="/Library/Keychains/System.keychain"

		dnl Check for the various security headers...
		AC_CHECK_HEADER(Security/SecureTransportPriv.h,
		    AC_DEFINE(HAVE_SECURETRANSPORTPRIV_H))
		AC_CHECK_HEADER(Security/SecCertificate.h,
		    AC_DEFINE(HAVE_SECCERTIFICATE_H))
		AC_CHECK_HEADER(Security/SecItem.h,
		    AC_DEFINE(HAVE_SECITEM_H))
		AC_CHECK_HEADER(Security/SecItemPriv.h,
		    AC_DEFINE(HAVE_SECITEMPRIV_H),,
		    [#include <Security/SecItem.h>])
		AC_CHECK_HEADER(Security/SecPolicy.h,
		    AC_DEFINE(HAVE_SECPOLICY_H))
		AC_CHECK_HEADER(Security/SecPolicyPriv.h,
		    AC_DEFINE(HAVE_SECPOLICYPRIV_H))
		AC_CHECK_HEADER(Security/SecBasePriv.h,
		    AC_DEFINE(HAVE_SECBASEPRIV_H))
		AC_CHECK_HEADER(Security/SecIdentitySearchPriv.h,
		    AC_DEFINE(HAVE_SECIDENTITYSEARCHPRIV_H))

		AC_DEFINE(HAVE_CSSMERRORSTRING)
		AC_DEFINE(HAVE_SECKEYCHAINOPEN)])

		if test $uversion -ge 150; then
			AC_DEFINE(HAVE_SSLSETENABLEDCIPHERS)
		fi
	fi
    fi

    dnl Then look for GNU TLS...
    if test $have_ssl = 0 -a "x$enable_gnutls" != "xno" -a "x$PKGCONFIG" != x; then
    	AC_PATH_TOOL(LIBGNUTLSCONFIG,libgnutls-config)
    	AC_PATH_TOOL(LIBGCRYPTCONFIG,libgcrypt-config)
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

dnl
dnl End of "$Id: cups-ssl.m4 12645 2015-05-20 01:20:52Z msweet $".
dnl
