dnl
dnl "$Id$"
dnl
dnl   OpenSSL/GNUTLS stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Easy Software Products and are protected by Federal
dnl   copyright law.  Distribution and use rights are outlined in the file
dnl   "LICENSE.txt" which should have been included with this file.  If this
dnl   file is missing or damaged please contact Easy Software Products
dnl   at:
dnl
dnl       Attn: CUPS Licensing Information
dnl       Easy Software Products
dnl       44141 Airport View Drive, Suite 204
dnl       Hollywood, Maryland 20636 USA
dnl
dnl       Voice: (301) 373-9600
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

AC_ARG_ENABLE(ssl, [  --enable-ssl            turn on SSL/TLS support, default=yes])
AC_ARG_ENABLE(cdsassl, [  --enable-cdsassl        use CDSA for SSL/TLS support, default=first])
AC_ARG_ENABLE(gnutls, [  --enable-gnutls         use GNU TLS for SSL/TLS support, default=second])
AC_ARG_ENABLE(openssl, [  --enable-openssl        use OpenSSL for SSL/TLS support, default=third])
AC_ARG_WITH(openssl-libs, [  --with-openssl-libs     set directory for OpenSSL library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(openssl-includes, [  --with-openssl-includes set directory for OpenSSL includes],
    CFLAGS="-I$withval $CFLAGS"
    CXXFLAGS="-I$withval $CXXFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

SSLFLAGS=""
SSLLIBS=""
ENCRYPTION_REQUIRED=""

if test x$enable_ssl != xno; then
    dnl Look for CDSA...
    if test "x${SSLLIBS}" = "x" -a "x${enable_cdsassl}" != "xno"; then
	if test $uname = Darwin; then
	    AC_CHECK_HEADER(Security/SecureTransport.h,
		[SSLLIBS="-framework CoreFoundation -framework Security"
		 ENCRYPTION_REQUIRED="  Encryption Required"
		 AC_DEFINE(HAVE_SSL)
		 AC_DEFINE(HAVE_CDSASSL)])
	fi
    fi

    dnl Then look for GNU TLS...
    if test "x${SSLLIBS}" = "x" -a "x${enable_gnutls}" != "xno"; then
	AC_CHECK_HEADER(gnutls/gnutls.h,
	    dnl Save the current libraries so the crypto stuff isn't always
	    dnl included...
	    SAVELIBS="$LIBS"

	    AC_CHECK_LIB(gnutls, gnutls_x509_crt_set_dn_by_oid,
		[SSLLIBS="-lgnutls"
		 ENCRYPTION_REQUIRED="  Encryption Required"
		 AC_DEFINE(HAVE_SSL)
		 AC_DEFINE(HAVE_GNUTLS)])

	    LIBS="$SAVELIBS")
    fi

    dnl Check for the OpenSSL library last...
    if test "x${SSLLIBS}" = "x" -a "x${enable_openssl}" != "xno"; then
	AC_CHECK_HEADER(openssl/ssl.h,
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
		    [SSLFLAGS="-DOPENSSL_DISABLE_OLD_DES_SUPPORT"
		     SSLLIBS="-lssl $libcrypto"
		     ENCRYPTION_REQUIRED="  Encryption Required"
		     AC_DEFINE(HAVE_SSL)
		     AC_DEFINE(HAVE_LIBSSL)],,
		    $libcrypto)

		if test "x${SSLLIBS}" != "x"; then
		    break
		fi
	    done

	    LIBS="$SAVELIBS")
    fi
fi

AC_SUBST(SSLFLAGS)
AC_SUBST(SSLLIBS)
AC_SUBST(ENCRYPTION_REQUIRED)

EXPORT_SSLLIBS="$SSLLIBS"
AC_SUBST(EXPORT_SSLLIBS)


dnl
dnl End of "$Id$".
dnl
