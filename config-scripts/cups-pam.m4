dnl
dnl "$Id: cups-pam.m4 7960 2008-09-17 19:42:02Z mike $"
dnl
dnl   PAM stuff for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(pam, [  --disable-pam           disable PAM support])
AC_ARG_WITH(pam_module, [  --with-pam-module       specify the PAM module to use])

dnl Don't use PAM with AIX...
if test $uname = AIX; then
	enable_pam=no
fi

PAMDIR=""
PAMFILE="pam.std"
PAMLIBS=""
PAMMOD="pam_unknown.so"
PAMMODAUTH="pam_unknown.so"

if test x$enable_pam != xno; then
	SAVELIBS="$LIBS"

	AC_CHECK_LIB(dl,dlopen)
	AC_CHECK_LIB(pam,pam_start)
	AC_CHECK_LIB(pam,pam_set_item,AC_DEFINE(HAVE_PAM_SET_ITEM))
	AC_CHECK_LIB(pam,pam_setcred,AC_DEFINE(HAVE_PAM_SETCRED))
	AC_CHECK_HEADER(security/pam_appl.h)
	if test x$ac_cv_header_security_pam_appl_h != xyes; then
		AC_CHECK_HEADER(pam/pam_appl.h,
			AC_DEFINE(HAVE_PAM_PAM_APPL_H))
	fi

	if test x$ac_cv_lib_pam_pam_start != xno; then
		# Set the necessary libraries for PAM...
		if test x$ac_cv_lib_dl_dlopen != xno; then
			PAMLIBS="-lpam -ldl"
		else
			PAMLIBS="-lpam"
		fi

		# Find the PAM configuration directory, if any...
		for dir in /private/etc/pam.d /etc/pam.d; do
			if test -d $dir; then
				PAMDIR=$dir
				break;
			fi
		done
	fi

	LIBS="$SAVELIBS"

	case "$uname" in
		Darwin*)
			# Darwin/Mac OS X
			if test "x$with_pam_module" != x; then
				PAMFILE="pam.$with_pam_module"
			elif test -f /usr/lib/pam/pam_opendirectory.so.2; then
				PAMFILE="pam.opendirectory"
			else
				PAMFILE="pam.securityserver"
			fi
			;;

		*)
			# All others; this test might need to be updated
			# as Linux distributors move things around...
			if test "x$with_pam_module" != x; then
				PAMMOD="pam_${with_pam_module}.so"
			elif test -f /lib/security/pam_unix2.so; then
				PAMMOD="pam_unix2.so"
			elif test -f /lib/security/pam_unix.so; then
				PAMMOD="pam_unix.so"
			fi

			if test "x$PAMMOD" = xpam_unix.so; then
				PAMMODAUTH="$PAMMOD shadow nodelay"
			else
				PAMMODAUTH="$PAMMOD nodelay"
			fi
			;;
	esac
fi

AC_SUBST(PAMDIR)
AC_SUBST(PAMFILE)
AC_SUBST(PAMLIBS)
AC_SUBST(PAMMOD)
AC_SUBST(PAMMODAUTH)

dnl
dnl End of "$Id: cups-pam.m4 7960 2008-09-17 19:42:02Z mike $".
dnl
