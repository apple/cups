dnl
dnl GSSAPI/Kerberos library detection for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright @ 2007-2017 by Apple Inc.
dnl Copyright @ 2006-2007 by Easy Software Products.
dnl
dnl This file contains Kerberos support code, copyright 2006 by
dnl Jelmer Vernooij.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

AC_ARG_ENABLE([gssapi], AS_HELP_STRING([--enable-gssapi], [enable (deprecated) GSSAPI/Kerberos support]))

LIBGSSAPI=""
AC_SUBST([LIBGSSAPI])

AS_IF([test x$enable_gssapi = xyes], [
    AC_PATH_TOOL([KRB5CONFIG], [krb5-config])
    AS_CASE(["$host_os_name"], [darwin*], [
	# macOS weak-links to the Kerberos framework...
	AC_DEFINE([HAVE_GSSAPI], [1], [Is GSSAPI available?])
	LIBGSSAPI="-weak_framework Kerberos"
	AC_MSG_CHECKING([for GSS framework])
	AS_IF([test -d /System/Library/Frameworks/GSS.framework], [
	    AC_MSG_RESULT([yes])
	    LIBGSSAPI="$LIBGSSAPI -weak_framework GSS"
	], [
	    AC_MSG_RESULT([no])
	])
    ], [sunos*], [
	# Solaris has a non-standard krb5-config, don't use it!
	SAVELIBS="$LIBS"
	AC_CHECK_LIB([gss], [gss_display_status], [
	    AC_DEFINE([HAVE_GSSAPI], [1], [Is GSSAPI available?])
	    AS_IF([test "x$KRB5CONFIG" != x], [
		CFLAGS="$($KRB5CONFIG --cflags) $CFLAGS"
		CPPFLAGS="$($KRB5CONFIG --cflags) $CPPFLAGS"
		LIBGSSAPI="-lgss $($KRB5CONFIG --libs)"
	    ], [
	        LIBGSSAPI="-lgss"
	    ])
	], [
	    AC_MSG_ERROR([--enable-gssapi specified but GSSAPI library cannot be found.])
	])
	LIBS="$SAVELIBS"
    ], [*], [
	# Other platforms just ask for GSSAPI
	AS_IF([test "x$KRB5CONFIG" = x], [
	    AC_MSG_ERROR([--enable-gssapi specified but krb5-config cannot be found.])
	], [
	    AC_DEFINE([HAVE_GSSAPI], [1], [Is GSSAPI available?])
	    CFLAGS="$($KRB5CONFIG --cflags gssapi) $CFLAGS"
	    CPPFLAGS="$($KRB5CONFIG --cflags gssapi) $CPPFLAGS"
	    LIBGSSAPI="$($KRB5CONFIG --libs gssapi)"
	])
    ])

    AC_CHECK_HEADER([krb5.h], [AC_DEFINE([HAVE_KRB5_H], [1], [Have <krb5.h> header?])])
    AS_IF([test -d /System/Library/Frameworks/GSS.framework], [
	AC_CHECK_HEADER([GSS/gssapi.h], [AC_DEFINE([HAVE_GSS_GSSAPI_H], [1], [Have <GSS/gssapi.h> header?])])
	AC_CHECK_HEADER([GSS/gssapi_generic.h], [AC_DEFINE([HAVE_GSS_GSSAPI_GENERIC_H], [1], [Have <GSS/gssapi_generic.h> header?])])
	AC_CHECK_HEADER([GSS/gssapi_spi.h], [AC_DEFINE([HAVE_GSS_GSSAPI_SPI_H], [1], [Have <GSS/gssapi_spi.h> header?])])
    ], [
	AC_CHECK_HEADER([gssapi.h], [AC_DEFINE([HAVE_GSSAPI_H], [1], [Have <gssapi.h> header?])])
	AC_CHECK_HEADER([gssapi/gssapi.h], [AC_DEFINE([HAVE_GSSAPI_GSSAPI_H], [1], [Have <gssapi/gssapi.h> header?])])
    ])

    SAVELIBS="$LIBS"
    LIBS="$LIBS $LIBGSSAPI"
    PKGCONFIG_LIBS_STATIC="$PKGCONFIG_LIBS_STATIC $LIBGSSAPI"
	
    AC_CHECK_FUNC([__ApplePrivate_gss_acquire_cred_ex_f], [
	AC_DEFINE([HAVE_GSS_ACQUIRE_CRED_EX_F], [1], [Have __ApplePrivate_gss_acquire_cred_ex_f function?])
    ])

    AC_MSG_CHECKING([for GSS_C_NT_HOSTBASED_SERVICE])
    AS_IF([test x$ac_cv_header_gssapi_gssapi_h = xyes], [
	AC_COMPILE_IFELSE([
	    AC_LANG_PROGRAM([[#include <gssapi/gssapi.h>]], [[
	        gss_OID foo = GSS_C_NT_HOSTBASED_SERVICE;
	    ]])
	], [
	    AC_DEFINE([HAVE_GSS_C_NT_HOSTBASED_SERVICE], [1], [Have GSS_C_NT_HOSTBASED_SERVICE?])
	    AC_MSG_RESULT([yes])
	], [
	    AC_MSG_RESULT([no])
	])
    ], [test x$ac_cv_header_gss_gssapi_h = xyes], [
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[#include <GSS/gssapi.h>]], [[
	        gss_OID foo = GSS_C_NT_HOSTBASED_SERVICE;
	    ]])
	], [
	    AC_DEFINE([HAVE_GSS_C_NT_HOSTBASED_SERVICE], [1], [Have GSS_C_NT_HOSTBASED_SERVICE?])
	    AC_MSG_RESULT([yes])
	], [
	    AC_MSG_RESULT([no])
	])
    ], [
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[#include <gssapi.h>]], [[
	        gss_OID foo = GSS_C_NT_HOSTBASED_SERVICE;
	    ]])
	], [
	    AC_DEFINE([HAVE_GSS_C_NT_HOSTBASED_SERVICE], [1], [Have GSS_C_NT_HOSTBASED_SERVICE?])
	    AC_MSG_RESULT([yes])
	], [
	    AC_MSG_RESULT([no])
	])
    ])

    LIBS="$SAVELIBS"
])

dnl Default GSS service name...
AC_ARG_WITH([gssservicename], AS_HELP_STRING([--with-gssservicename], [set default gss service name]), [
    default_gssservicename="$withval"
], [
    default_gssservicename="default"
])

AS_IF([test x$default_gssservicename != xno], [
    AS_IF([test "x$default_gssservicename" = "xdefault"], [
	CUPS_DEFAULT_GSSSERVICENAME="host"
    ], [
	CUPS_DEFAULT_GSSSERVICENAME="$default_gssservicename"
    ])
], [
    CUPS_DEFAULT_GSSSERVICENAME=""
])

AC_SUBST([CUPS_DEFAULT_GSSSERVICENAME])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_GSSSERVICENAME], ["$CUPS_DEFAULT_GSSSERVICENAME"], [Default GSSServiceName value.])
