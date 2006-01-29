dnl
dnl "$Id$"
dnl
dnl   launchd stuff for the Common UNIX Printing System (CUPS).
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


AC_ARG_ENABLE(launchd, [  --enable-launchd            turn on launchd support, default=yes])

DEFAULT_LAUNCHD_CONF=""
LAUNCHDLIBS=""

if test x$enable_launchd != xno; then
	AC_CHECK_FUNC(launch_msg, AC_DEFINE(HAVE_LAUNCHD))
	AC_CHECK_HEADER(launch.h, AC_DEFINE(HAVE_LAUNCH_H))

	case "$uname" in
		Darwin*)
			# Darwin, MacOS X
			DEFAULT_LAUNCHD_CONF="/System/Library/LaunchDaemons/org.cups.cupsd.plist"
			# liblaunch is already part of libSystem
			;;
		*)
			# All others; this test will need to be updated
			;;
	esac
fi

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LAUNCHD_CONF, "$DEFAULT_LAUNCHD_CONF")
AC_SUBST(DEFAULT_LAUNCHD_CONF)
AC_SUBST(LAUNCHDLIBS)

dnl
dnl End of "$Id$".
dnl
