dnl
dnl "$Id$"
dnl
dnl Launch-on-demand stuff for CUPS.
dnl
dnl Copyright 2007-2014 by Apple Inc.
dnl Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl These coded instructions, statements, and computer programs are the
dnl property of Apple Inc. and are protected by Federal copyright
dnl law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl which should have been included with this file.  If this file is
dnl file is missing or damaged, see the license at "http://www.cups.org/".
dnl

ONDEMANDFLAGS=""
ONDEMANDLIBS=""
AC_SUBST(ONDEMANDFLAGS)
AC_SUBST(ONDEMANDLIBS)

dnl Launchd is used on OS X/Darwin...
AC_ARG_ENABLE(launchd, [  --disable-launchd       disable launchd support])
LAUNCHD_DIR=""
AC_SUBST(LAUNCHD_DIR)

if test x$enable_launchd != xno; then
	AC_CHECK_FUNC(launch_msg, AC_DEFINE(HAVE_LAUNCHD))
	if test $uversion -ge 140; then
		AC_CHECK_FUNC(launch_activate_socket, [
			AC_DEFINE(HAVE_LAUNCHD)
			AC_DEFINE(HAVE_LAUNCH_ACTIVATE_SOCKET)])
	fi
	AC_CHECK_HEADER(launch.h, AC_DEFINE(HAVE_LAUNCH_H))

	case "$uname" in
		Darwin*)
			# Darwin, MacOS X
			LAUNCHD_DIR="/System/Library/LaunchDaemons/org.cups.cupsd.plist"
			# liblaunch is already part of libSystem
			;;
		*)
			# All others; this test will need to be updated
			;;
	esac
fi

dnl Systemd is used on Linux...
AC_ARG_ENABLE(systemd, [  --disable-systemd       disable systemd support])
AC_ARG_WITH(systemd, [  --with-systemd          set directory for systemd service files],
        SYSTEMD_DIR="$withval", SYSTEMD_DIR="")
AC_SUBST(SYSTEMD_DIR)

if test x$enable_systemd != xno; then
	if test "x$PKGCONFIG" = x; then
        	if test x$enable_systemd = xyes; then
	        	AC_MSG_ERROR(Need pkg-config to enable systemd support.)
                fi
        else
        	AC_MSG_CHECKING(for libsystemd-daemon)
                if $PKGCONFIG --exists libsystemd-daemon; then
                        AC_MSG_RESULT(yes)
                        ONDEMANDFLAGS=`$PKGCONFIG --cflags libsystemd-daemon`
                        ONDEMANDLIBS=`$PKGCONFIG --libs libsystemd-daemon`
                        AC_DEFINE(HAVE_SYSTEMD)
			if test "x$SYSTEMD_DIR" = x; then
			        SYSTEMD_DIR="`$PKGCONFIG --variable=systemdsystemunitdir systemd`"
                        fi
                else
                        AC_MSG_RESULT(no)
                fi
        fi
fi

dnl
dnl End of "$Id$".
dnl
