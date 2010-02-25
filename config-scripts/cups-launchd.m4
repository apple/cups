dnl
dnl "$Id: cups-launchd.m4 6649 2007-07-11 21:46:42Z mike $"
dnl
dnl   launchd stuff for CUPS.
dnl
dnl   Copyright 2007-2010 by Apple Inc.
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl


AC_ARG_ENABLE(launchd, [  --disable-launchd       disable launchd support])

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

AC_SUBST(DEFAULT_LAUNCHD_CONF)
AC_SUBST(LAUNCHDLIBS)

dnl
dnl End of "$Id: cups-launchd.m4 6649 2007-07-11 21:46:42Z mike $".
dnl
