dnl
dnl   "$Id$"
dnl
dnl   GSSAPI/Kerberos library detection.
dnl
dnl   Copyright 2006 by Easy Software Products.
dnl
dnl   This file contains Kerberos support code, copyright 2006 by
dnl   Jelmer Vernooij.
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

AC_ARG_ENABLE(gssapi, [  --enable-gssapi         turn on GSSAPI support, default=yes])

LIBGSSAPI=""

if test x$enable_gssapi != xno; then
	AC_PATH_PROG(KRB5CONFIG, krb5-config)
	if test "x$KRB5CONFIG" != x; then
		CFLAGS="`$KRB5CONFIG --cflags gssapi` $CFLAGS"		
		CPPFLAGS="`$KRB5CONFIG --cflags gssapi` $CPPFLAGS"		
		LIBGSSAPI="`$KRB5CONFIG --libs gssapi`"
		AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
	else
		# Solaris provides its own GSSAPI implementation...
		AC_CHECK_LIB(gss, gss_display_status,
			AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
			LIBGSSAPI="-lgss")
	fi

	if test "x$LIBGSSAPI" != x; then
		AC_CHECK_HEADER(krb5.h, AC_DEFINE(HAVE_KRB5_H))
		AC_CHECK_HEADER(gssapi.h, AC_DEFINE(HAVE_GSSAPI_H))
		AC_CHECK_HEADER(gssapi/gssapi.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_H))
		AC_CHECK_HEADER(gssapi/gssapi_generic.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_GENERIC_H))
		AC_CHECK_HEADER(gssapi/gssapi_krb5.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_KRB5_H))

		SAVELIBS="$LIBS"
		LIBS="$LIBS $LIBGSSAPI"
		AC_CHECK_FUNC(gsskrb5_register_acceptor_identity, 
			      AC_DEFINE(HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY))
		LIBS="$SAVELIBS"
		AC_TRY_COMPILE([ #include <krb5.h> ],
			       [ char *tmp = heimdal_version; ],
			       AC_DEFINE(HAVE_HEIMDAL))
	fi
fi

AC_SUBST(LIBGSSAPI)

dnl
dnl End of "$Id$".
dnl
dnl
dnl   "$Id: cups-gssapi.m4 5916 2006-08-30 20:46:47Z mike $"
dnl
dnl   GSSAPI/Kerberos library detection.
dnl
dnl   Copyright 2006 by Easy Software Products.
dnl
dnl   This file contains Kerberos support code, copyright 2006 by
dnl   Jelmer Vernooij.
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

AC_ARG_ENABLE(gssapi, [  --enable-gssapi         turn on GSSAPI support, default=yes])

LIBGSSAPI=""

if test x$enable_gssapi != xno; then
	AC_PATH_PROG(KRB5CONFIG, krb5-config)
	if test "x$KRB5CONFIG" != x; then
		CFLAGS="`$KRB5CONFIG --cflags gssapi` $CFLAGS"		
		CPPFLAGS="`$KRB5CONFIG --cflags gssapi` $CPPFLAGS"		
		LIBGSSAPI="`$KRB5CONFIG --libs gssapi`"
		AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
	else
		# Solaris provides its own GSSAPI implementation...
		AC_CHECK_LIB(gss, gss_display_status,
			AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
			LIBGSSAPI="-lgss")
	fi

	if test "x$LIBGSSAPI" != x; then
		AC_CHECK_HEADER(krb5.h, AC_DEFINE(HAVE_KRB5_H))
		AC_CHECK_HEADER(gssapi.h, AC_DEFINE(HAVE_GSSAPI_H))
		AC_CHECK_HEADER(gssapi/gssapi.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_H))
		AC_CHECK_HEADER(gssapi/gssapi_generic.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_GENERIC_H))
		AC_CHECK_HEADER(gssapi/gssapi_krb5.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_KRB5_H))

		SAVELIBS="$LIBS"
		LIBS="$LIBS $LIBGSSAPI"
		AC_CHECK_FUNC(gsskrb5_register_acceptor_identity, 
			      AC_DEFINE(HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY))
		LIBS="$SAVELIBS"
		AC_TRY_COMPILE([ #include <krb5.h> ],
			       [ char *tmp = heimdal_version; ],
			       AC_DEFINE(HAVE_HEIMDAL))
	fi
fi

AC_SUBST(LIBGSSAPI)

dnl
dnl End of "$Id: cups-gssapi.m4 5916 2006-08-30 20:46:47Z mike $".
dnl
