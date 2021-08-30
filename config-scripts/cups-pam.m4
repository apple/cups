dnl
dnl PAM stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2017 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

AC_ARG_ENABLE([pam], AS_HELP_STRING([--disable-pam], [disable PAM support]))
AC_ARG_WITH([pam_module], AS_HELP_STRING([--with-pam-module], [set the PAM module to use]))

PAMDIR=""
PAMFILE="pam.std"
PAMLIBS=""
PAMMOD="pam_unknown.so"
PAMMODAUTH="pam_unknown.so"

AS_IF([test x$enable_pam != xno], [
    SAVELIBS="$LIBS"

    AC_CHECK_LIB([dl], [dlopen])
    AC_CHECK_LIB([pam], [pam_start])
    AC_CHECK_LIB([pam], [pam_set_item], [
        AC_DEFINE([HAVE_PAM_SET_ITEM], [1], [Have pam_set_item function?])
    ])
    AC_CHECK_LIB([pam], [pam_setcred], [
        AC_DEFINE([HAVE_PAM_SETCRED], [1], [Have pam_setcred function?])
    ])
    AC_CHECK_HEADER([security/pam_appl.h])
    AS_IF([test x$ac_cv_header_security_pam_appl_h != xyes], [
	AC_CHECK_HEADER([pam/pam_appl.h], [
	    AC_DEFINE([HAVE_PAM_PAM_APPL_H], [1], [Have <pam/pam_appl.h> header?])
	])
    ])

    AS_IF([test x$ac_cv_lib_pam_pam_start != xno], [
	# Set the necessary libraries for PAM...
	AS_IF([test x$ac_cv_lib_dl_dlopen != xno], [
	    PAMLIBS="-lpam -ldl"
	], [
	    PAMLIBS="-lpam"
	])

	# Find the PAM configuration directory, if any...
	for dir in /private/etc/pam.d /etc/pam.d; do
	    AS_IF([test -d $dir], [
		PAMDIR="$dir"
		break;
	    ])
	done
    ])

    LIBS="$SAVELIBS"

    AS_CASE(["$host_os_name"], [darwin*], [
	# Darwin/macOS
	AS_IF([test "x$with_pam_module" != x], [
	    PAMFILE="pam.$with_pam_module"
	], [test -f /usr/lib/pam/pam_opendirectory.so.2], [
	    PAMFILE="pam.opendirectory"
	], [
	    PAMFILE="pam.securityserver"
	])
    ], [*], [
	# All others; this test might need to be updated
	# as Linux distributors move things around...
	AS_IF([test "x$with_pam_module" != x], [
	    PAMMOD="pam_${with_pam_module}.so"
	], [test -f /etc/pam.d/common-auth], [
	    PAMFILE="pam.common"
	], [
	    moddir=""
	    for dir in /lib/security /lib64/security /lib/x86_64-linux-gnu/security /var/lib/pam; do
		AS_IF([test -d $dir], [
		    moddir="$dir"
		    break;
		])
	    done

	    AS_IF([test -f $moddir/pam_unix2.so], [
		PAMMOD="pam_unix2.so"
	    ], [test -f $moddir/pam_unix.so], [
		PAMMOD="pam_unix.so"
	    ])
	])

	AS_IF([test "x$PAMMOD" = xpam_unix.so], [
	    PAMMODAUTH="$PAMMOD shadow nodelay"
	], [
	    PAMMODAUTH="$PAMMOD nodelay"
	])
    ])
])

AC_SUBST([PAMDIR])
AC_SUBST([PAMFILE])
AC_SUBST([PAMLIBS])
AC_SUBST([PAMMOD])
AC_SUBST([PAMMODAUTH])
