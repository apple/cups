dnl
dnl "$Id: cups-openssl.m4,v 1.4.2.4 2002/04/26 14:57:33 mike Exp $"
dnl
dnl   OpenSSL stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
dnl       Hollywood, Maryland 20636-3111 USA
dnl
dnl       Voice: (301) 373-9603
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

AC_ARG_ENABLE(ssl, [  --enable-ssl            turn on SSL/TLS support [default=yes]])
AC_ARG_WITH(openssl-libs, [  --with-openssl-libs     set directory for OpenSSL library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(openssl-includes, [  --with-openssl-includes set directory for OpenSSL includes],
    CFLAGS="-I$withval $CFLAGS"
    CXXFLAGS="-I$withval $CXXFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

SSLLIBS=""

if test x$enable_ssl != xno; then
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
		[SSLLIBS="-lssl $libcrypto"
		 AC_DEFINE(HAVE_LIBSSL)],,
		$libcrypto)

	    if test "x${SSLLIBS}" != "x"; then
		break
	    fi
	done

	LIBS="$SAVELIBS")
fi

AC_SUBST(SSLLIBS)

dnl
dnl End of "$Id: cups-openssl.m4,v 1.4.2.4 2002/04/26 14:57:33 mike Exp $".
dnl
