dnl
dnl "$Id: cups-ssl.m4 7241 2008-01-22 22:34:52Z mike $"
dnl
dnl   OpenSSL/GNUTLS stuff for CUPS.
dnl
dnl   Copyright 2007-2012 by Apple Inc.
dnl   Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(ssl, [  --disable-ssl           disable SSL/TLS support])
AC_ARG_ENABLE(cdsassl, [  --enable-cdsassl        use CDSA for SSL/TLS support, default=first])
AC_ARG_ENABLE(gnutls, [  --enable-gnutls         use GNU TLS for SSL/TLS support, default=second])
AC_ARG_ENABLE(openssl, [  --enable-openssl        use OpenSSL for SSL/TLS support, default=third])
AC_ARG_WITH(openssl-libs, [  --with-openssl-libs     set directory for OpenSSL library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(openssl-includes, [  --with-openssl-includes set directory for OpenSSL includes],
    CFLAGS="-I$withval $CFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

SSLFLAGS=""
SSLLIBS=""
have_ssl=0

if test x$enable_ssl != xno; then
    dnl Look for CDSA...
    if test $have_ssl = 0 -a "x$enable_cdsassl" != "xno"; then
	if test $uname = Darwin; then
	    AC_CHECK_HEADER(Security/SecureTransport.h, [
	    	have_ssl=1
		AC_DEFINE(HAVE_SSL)
		AC_DEFINE(HAVE_CDSASSL)

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

		dnl Check for SSLSetProtocolVersionMax...
		SAVELIBS="$LIBS"
		LIBS="$LIBS -framework Security"
		AC_CHECK_FUNC(SSLSetProtocolVersionMax)
		LIBS="$SAVELIBS"

		dnl Check for SecCertificateCopyData..
		AC_MSG_CHECKING(for SecCertificateCopyData)
		if test $uversion -ge 100; then
		    AC_DEFINE(HAVE_SECCERTIFICATECOPYDATA)
		    AC_MSG_RESULT(yes)
		else
		    AC_MSG_RESULT(no)
		fi

		dnl Check for SecIdentitySearchCreateWithPolicy...
		AC_MSG_CHECKING(for SecIdentitySearchCreateWithPolicy)
		if test $uversion -ge 80; then
		    AC_DEFINE(HAVE_SECIDENTITYSEARCHCREATEWITHPOLICY)
		    AC_MSG_RESULT(yes)
		else
		    AC_MSG_RESULT(no)
		fi

		dnl Check for SecPolicyCreateSSL...
		AC_MSG_CHECKING(for SecPolicyCreateSSL)
		if test $uversion -ge 110; then
		    AC_DEFINE(HAVE_SECPOLICYCREATESSL)
		    AC_MSG_RESULT(yes)
		else
		    AC_MSG_RESULT(no)
		fi])

		AC_DEFINE(HAVE_CSSMERRORSTRING)
	fi
    fi

    dnl Then look for GNU TLS...
    if test $have_ssl = 0 -a "x$enable_gnutls" != "xno" -a "x$PKGCONFIG" != x; then
    	AC_PATH_PROG(LIBGNUTLSCONFIG,libgnutls-config)
    	AC_PATH_PROG(LIBGCRYPTCONFIG,libgcrypt-config)
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
            if $PKGCONFIG --exists gcrypt; then
	        SSLLIBS="$SSLLIBS `$PKGCONFIG --libs gcrypt`"
	        SSLFLAGS="$SSLFLAGS `$PKGCONFIG --cflags gcrypt`"
	    elif test "x$LIBGCRYPTCONFIG" != x; then
	        SSLLIBS="$SSLLIBS `$LIBGCRYPTCONFIG --libs`"
	        SSLFLAGS="$SSLFLAGS `$LIBGCRYPTCONFIG --cflags`"
	    fi
	fi
    fi

    dnl Check for the OpenSSL library last...
    if test $have_ssl = 0 -a "x$enable_openssl" != "xno"; then
	AC_CHECK_HEADER(openssl/ssl.h,[
	    dnl Save the current libraries so the crypto stuff isn't always
	    dnl included...
	    SAVELIBS="$LIBS"

	    dnl Some ELF systems can't resolve all the symbols in libcrypto
	    dnl if libcrypto was linked against RSAREF, and fail to link the
	    dnl test program correctly, even though a correct installation
	    dnl of OpenSSL exists.  So we test the linking three times in
	    dnl case the RSAREF libraries are needed.

	    for libcrypto in \
		"-lcrypto" \
		"-lcrypto -lrsaref" \
		"-lcrypto -lRSAglue -lrsaref"
	    do
		AC_CHECK_LIB(ssl,SSL_new,
		    [have_ssl=1
		     SSLFLAGS="-DOPENSSL_DISABLE_OLD_DES_SUPPORT"
		     SSLLIBS="-lssl $libcrypto"
		     AC_DEFINE(HAVE_SSL)
		     AC_DEFINE(HAVE_LIBSSL)],,
		    $libcrypto)

		if test "x${SSLLIBS}" != "x"; then
		    break
		fi
	    done

	    if test "x${SSLLIBS}" != "x"; then
		LIBS="$SAVELIBS $SSLLIBS"
		AC_CHECK_FUNCS(SSL_set_tlsext_host_name)
	    fi

	    LIBS="$SAVELIBS"])
    fi
fi

IPPALIASES="http"
if test $have_ssl = 1; then
    AC_MSG_RESULT([    Using SSLLIBS="$SSLLIBS"])
    AC_MSG_RESULT([    Using SSLFLAGS="$SSLFLAGS"])
    IPPALIASES="http https ipps"
elif test x$enable_cdsa = xyes -o x$enable_gnutls = xyes -o x$enable_openssl = xyes; then
    AC_MSG_ERROR([Unable to enable SSL support.])
fi

AC_SUBST(IPPALIASES)
AC_SUBST(SSLFLAGS)
AC_SUBST(SSLLIBS)

EXPORT_SSLLIBS="$SSLLIBS"
AC_SUBST(EXPORT_SSLLIBS)

dnl
dnl End of "$Id: cups-ssl.m4 7241 2008-01-22 22:34:52Z mike $".
dnl
