dnl
dnl   "$Id: cups-gssapi.m4 11911 2014-06-10 13:54:53Z msweet $"
dnl
dnl   GSSAPI/Kerberos library detection for CUPS.
dnl
dnl   Copyright 2007-2013 by Apple Inc.
dnl   Copyright 2006-2007 by Easy Software Products.
dnl
dnl   This file contains Kerberos support code, copyright 2006 by
dnl   Jelmer Vernooij.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(gssapi, [  --disable-gssapi        disable GSSAPI support])

LIBGSSAPI=""
AC_SUBST(LIBGSSAPI)

if test x$enable_gssapi != xno; then
	AC_PATH_TOOL(KRB5CONFIG, krb5-config)
	if test "x$KRB5CONFIG" != x; then
		case "$uname" in
			Darwin)
				# OS X weak-links to the Kerberos framework...
				LIBGSSAPI="-weak_framework Kerberos"
				AC_MSG_CHECKING(for GSS framework)
				if test -d /System/Library/Frameworks/GSS.framework; then
					AC_MSG_RESULT(yes)
					LIBGSSAPI="$LIBGSSAPI -weak_framework GSS"
				else
					AC_MSG_RESULT(no)
				fi
				;;
			SunOS*)
				# Solaris has a non-standard krb5-config, don't use it!
				AC_CHECK_LIB(gss, gss_display_status,
					AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
					CFLAGS="`$KRB5CONFIG --cflags` $CFLAGS"
					CPPFLAGS="`$KRB5CONFIG --cflags` $CPPFLAGS"
					LIBGSSAPI="-lgss `$KRB5CONFIG --libs`")
				;;
			*)
				# Other platforms just ask for GSSAPI
				CFLAGS="`$KRB5CONFIG --cflags gssapi` $CFLAGS"
				CPPFLAGS="`$KRB5CONFIG --cflags gssapi` $CPPFLAGS"
				LIBGSSAPI="`$KRB5CONFIG --libs gssapi`"
				;;
		esac
		AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
	else
		# Check for vendor-specific implementations...
		case "$uname" in
			HP-UX*)
				AC_CHECK_LIB(gss, gss_display_status,
					AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
					LIBGSSAPI="-lgss -lgssapi_krb5")
				;;
			SunOS*)
				AC_CHECK_LIB(gss, gss_display_status,
					AC_DEFINE(HAVE_GSSAPI, 1, [Whether GSSAPI is available])
					LIBGSSAPI="-lgss")
				;;
		esac
	fi

	if test "x$LIBGSSAPI" != x; then
		AC_CHECK_HEADER(krb5.h, AC_DEFINE(HAVE_KRB5_H))
		if test -d /System/Library/Frameworks/GSS.framework; then
			AC_CHECK_HEADER(GSS/gssapi.h, AC_DEFINE(HAVE_GSS_GSSAPI_H))
			AC_CHECK_HEADER(GSS/gssapi_generic.h, AC_DEFINE(HAVE_GSS_GSSAPI_GENERIC_H))
			AC_CHECK_HEADER(GSS/gssapi_spi.h, AC_DEFINE(HAVE_GSS_GSSAPI_SPI_H))
		else
			AC_CHECK_HEADER(gssapi.h, AC_DEFINE(HAVE_GSSAPI_H))
			AC_CHECK_HEADER(gssapi/gssapi.h, AC_DEFINE(HAVE_GSSAPI_GSSAPI_H))
		fi

		SAVELIBS="$LIBS"
		LIBS="$LIBS $LIBGSSAPI"

		AC_CHECK_FUNC(__ApplePrivate_gss_acquire_cred_ex_f,
			      AC_DEFINE(HAVE_GSS_ACQUIRE_CRED_EX_F))

		AC_MSG_CHECKING(for GSS_C_NT_HOSTBASED_SERVICE)
		if test x$ac_cv_header_gssapi_gssapi_h = xyes; then
			AC_TRY_COMPILE([ #include <gssapi/gssapi.h> ],
				       [ gss_OID foo = GSS_C_NT_HOSTBASED_SERVICE; ],
				       AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE)
				       AC_MSG_RESULT(yes),
				       AC_MSG_RESULT(no))
		elif test x$ac_cv_header_gss_gssapi_h = xyes; then
			AC_TRY_COMPILE([ #include <GSS/gssapi.h> ],
				       [ gss_OID foo = GSS_C_NT_HOSTBASED_SERVICE; ],
				       AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE)
				       AC_MSG_RESULT(yes),
				       AC_MSG_RESULT(no))
		else
			AC_TRY_COMPILE([ #include <gssapi.h> ],
				       [ gss_OID foo = GSS_C_NT_HOSTBASED_SERVICE; ],
				       AC_DEFINE(HAVE_GSS_C_NT_HOSTBASED_SERVICE)
				       AC_MSG_RESULT(yes),
				       AC_MSG_RESULT(no))
		fi

		LIBS="$SAVELIBS"
	fi
fi

dnl Default GSS service name...
AC_ARG_WITH(gssservicename, [  --with-gssservicename   set default gss service name],
	default_gssservicename="$withval",
	default_gssservicename="default")

if test x$default_gssservicename != xno; then
	if test "x$default_gssservicename" = "xdefault"; then
		CUPS_DEFAULT_GSSSERVICENAME="host"
	else
		CUPS_DEFAULT_GSSSERVICENAME="$default_gssservicename"
	fi
else
	CUPS_DEFAULT_GSSSERVICENAME=""
fi

AC_SUBST(CUPS_DEFAULT_GSSSERVICENAME)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_GSSSERVICENAME, "$CUPS_DEFAULT_GSSSERVICENAME")

dnl
dnl End of "$Id: cups-gssapi.m4 11911 2014-06-10 13:54:53Z msweet $".
dnl
