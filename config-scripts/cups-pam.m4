dnl
dnl "$Id: cups-pam.m4,v 1.2 2001/07/24 20:23:03 mike Exp $"
dnl
dnl   PAM stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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

AC_ARG_ENABLE(pam, [  --enable-pam            turn on PAM support [default=yes]])

dnl Don't use PAM with AIX...
if test $uname = AIX; then
	enable_pam=no
fi

PAMDIR=""
PAMLIBS=""

if test x$enable_pam != xno; then
	SAVELIBS="$LIBS"
	AC_CHECK_LIB(dl,dlopen)
	AC_CHECK_LIB(pam,pam_start)

	if test x$ac_cv_lib_pam_pam_start != xno; then
		if test x$ac_cv_lib_dl_dlopen != xno; then
			PAMLIBS="-lpam -ldl"
		else
			PAMLIBS="-lpam"
		fi
		if test -d /etc/pam.d; then
			PAMDIR="/etc/pam.d"
		fi
	fi

	LIBS="$SAVELIBS"
fi

AC_SUBST(PAMDIR)
AC_SUBST(PAMLIBS)

dnl
dnl End of "$Id: cups-pam.m4,v 1.2 2001/07/24 20:23:03 mike Exp $".
dnl
