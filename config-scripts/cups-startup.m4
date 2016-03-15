dnl
dnl "$Id: cups-startup.m4 12351 2014-12-09 22:18:45Z msweet $"
dnl
dnl Launch-on-demand/startup stuff for CUPS.
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
			LAUNCHD_DIR="/System/Library/LaunchDaemons"
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

dnl Solaris uses smf
SMFMANIFESTDIR=""
AC_SUBST(SMFMANIFESTDIR)
AC_ARG_WITH(smfmanifestdir, [  --with-smfmanifestdir   set path for Solaris SMF manifest],SMFMANIFESTDIR="$withval")

dnl Use init on other platforms...
AC_ARG_WITH(rcdir, [  --with-rcdir            set path for rc scripts],rcdir="$withval",rcdir="")
AC_ARG_WITH(rclevels, [  --with-rclevels         set run levels for rc scripts],rclevels="$withval",rclevels="2 3 5")
AC_ARG_WITH(rcstart, [  --with-rcstart          set start number for rc scripts],rcstart="$withval",rcstart="")
AC_ARG_WITH(rcstop, [  --with-rcstop           set stop number for rc scripts],rcstop="$withval",rcstop="")

if test x$rcdir = x; then
	if test x$LAUNCHD_DIR = x -a x$SYSTEMD_DIR = x -a x$SMFMANIFESTDIR = x; then
                # Fall back on "init", the original service startup interface...
                if test -d /sbin/init.d; then
                        # SuSE
                        rcdir="/sbin/init.d"
                elif test -d /etc/init.d; then
                        # Others
                        rcdir="/etc"
                else
                        # RedHat, NetBSD
                        rcdir="/etc/rc.d"
                fi
        else
        	rcdir="no"
	fi
fi

if test "x$rcstart" = x; then
	case "$uname" in
        	Linux | GNU | GNU/k*BSD*)
                	# Linux
                        rcstart="81"
                      	;;

		SunOS*)
			# Solaris
                        rcstart="81"
			;;

                *)
                        # Others
                        rcstart="99"
                        ;;
	esac
fi

if test "x$rcstop" = x; then
	case "$uname" in
        	Linux | GNU | GNU/k*BSD*)
                	# Linux
                        rcstop="36"
                      	;;

                *)
                        # Others
                        rcstop="00"
                        ;;
	esac
fi

INITDIR=""
INITDDIR=""
RCLEVELS="$rclevels"
RCSTART="$rcstart"
RCSTOP="$rcstop"
AC_SUBST(INITDIR)
AC_SUBST(INITDDIR)
AC_SUBST(RCLEVELS)
AC_SUBST(RCSTART)
AC_SUBST(RCSTOP)

if test "x$rcdir" != xno; then
	if test "x$rclevels" = x; then
		INITDDIR="$rcdir"
	else
		INITDIR="$rcdir"
	fi
fi

dnl Xinetd support...
AC_ARG_WITH(xinetd, [  --with-xinetd           set path for xinetd config files],xinetd="$withval",xinetd="")
XINETD=""
AC_SUBST(XINETD)

if test "x$xinetd" = x; then
	if test ! -x /sbin/launchd; then
                for dir in /etc/xinetd.d /usr/local/etc/xinetd.d; do
                        if test -d $dir; then
                                XINETD="$dir"
                                break
                        fi
                done
        fi
elif test "x$xinetd" != xno; then
	XINETD="$xinetd"
fi


dnl
dnl End of "$Id: cups-startup.m4 12351 2014-12-09 22:18:45Z msweet $".
dnl
