dnl
dnl "$Id: cups-pam.m4,v 1.2.2.7 2004/02/26 16:59:02 mike Exp $"
dnl
dnl   PAM stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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

AC_ARG_ENABLE(pam, [  --enable-pam            turn on PAM support, default=yes])

dnl Don't use PAM with AIX...
if test $uname = AIX; then
	enable_pam=no
fi

PAMDIR=""
PAMFILE=""
PAMLIBS=""
PAMMOD="pam_unknown.so"

if test x$enable_pam != xno; then
	SAVELIBS="$LIBS"

	AC_CHECK_LIB(dl,dlopen)
	AC_CHECK_LIB(pam,pam_start)
	AC_CHECK_HEADER(pam/pam_appl.h,AC_DEFINE(HAVE_PAM_PAM_APPL_H))

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
			fi
		done
	fi

	LIBS="$SAVELIBS"

	case "$uname" in
		Darwin*)
			# Darwin, MacOS X
			PAMFILE="pam.darwin"
			;;
		IRIX*)
			# SGI IRIX
			PAMFILE="pam.irix"
			;;
		*)
			# All others; this test might need to be updated
			# as Linux distributors move things around...
			for mod in pam_unix2.so pam_unix.so pam_pwdb.so; do
				if test -f /lib/security/$mod; then
					PAMMOD="$mod"
				fi
			done

			PAMFILE="pam.std"
			;;
	esac
fi

AC_SUBST(PAMDIR)
AC_SUBST(PAMFILE)
AC_SUBST(PAMLIBS)
AC_SUBST(PAMMOD)

dnl
dnl End of "$Id: cups-pam.m4,v 1.2.2.7 2004/02/26 16:59:02 mike Exp $".
dnl
