dnl
dnl "$Id$"
dnl
dnl   Directory stuff for the Common UNIX Printing System (CUPS).
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

AC_PREFIX_DEFAULT(/)

AC_ARG_WITH(fontpath, [  --with-fontpath         set font path for pstoraster],fontpath="$withval",fontpath="")
AC_ARG_WITH(docdir, [  --with-docdir           set path for documentation],docdir="$withval",docdir="")
AC_ARG_WITH(logdir, [  --with-logdir           set path for log files],logdir="$withval",logdir="")
AC_ARG_WITH(rcdir, [  --with-rcdir            set path for rc scripts],rcdir="$withval",rcdir="")

dnl Fix "prefix" variable if it hasn't been specified...
if test "$prefix" = "NONE"; then
	prefix="/"
fi

dnl Fix "exec_prefix" variable if it hasn't been specified...
if test "$exec_prefix" = "NONE"; then
	if test "$prefix" = "/"; then
		exec_prefix="/usr"
	else
		exec_prefix="$prefix"
	fi
fi

dnl Fix "sharedstatedir" variable if it hasn't been specified...
if test "$sharedstatedir" = "\${prefix}/com" -a "$prefix" = "/"; then
	sharedstatedir="/usr/com"
fi

dnl Fix "datadir" variable if it hasn't been specified...
if test "$datadir" = "\${prefix}/share"; then
	if test "$prefix" = "/"; then
		datadir="/usr/share"
	else
		datadir="$prefix/share"
	fi
fi

dnl Fix "includedir" variable if it hasn't been specified...
if test "$includedir" = "\${prefix}/include" -a "$prefix" = "/"; then
	includedir="/usr/include"
fi

dnl Fix "localstatedir" variable if it hasn't been specified...
if test "$localstatedir" = "\${prefix}/var"; then
	if test "$prefix" = "/"; then
		localstatedir="/var"
	else
		localstatedir="$prefix/var"
	fi
fi

dnl Fix "sysconfdir" variable if it hasn't been specified...
if test "$sysconfdir" = "\${prefix}/etc"; then
	if test "$prefix" = "/"; then
		sysconfdir="/etc"
	else
		sysconfdir="$prefix/etc"
	fi
fi

dnl Fix "libdir" variable for IRIX 6.x...
if test "$libdir" = "\${exec_prefix}/lib"; then
	libdir="$exec_prefix/lib"
fi

if test "$uname" = "IRIX" -a $uversion -ge 62; then
	libdir="$exec_prefix/lib32"
fi

dnl Fix "fontpath" variable...
if test "x$fontpath" = "x"; then
	fontpath="$datadir/cups/fonts"
fi

dnl Setup init.d locations...
if test x$rcdir = x; then
	case "$uname" in
		FreeBSD* | OpenBSD*)
			# FreeBSD and OpenBSD
			INITDIR=""
			INITDDIR=""
			;;

		NetBSD*)
			# NetBSD
			INITDIR=""
			INITDDIR="/etc/rc.d"
			;;

		Darwin*)
			# Darwin and MacOS X...
			INITDIR=""
			INITDDIR="/System/Library/StartupItems/PrintingServices"
			;;

		Linux | GNU)
			# Linux/HURD seems to choose an init.d directory at random...
			if test -d /sbin/init.d; then
				# SuSE
				INITDIR="/sbin/init.d"
				INITDDIR=".."
			else
				if test -d /etc/rc.d; then
					# RedHat
					INITDIR="/etc/rc.d"
					INITDDIR="../init.d"
				else
					# Others
					INITDIR="/etc"
					INITDDIR="../init.d"
				fi
			fi
			;;

		OSF1* | HP-UX*)
			INITDIR="/sbin"
			INITDDIR="../init.d"
			;;

		AIX*)
			INITDIR="/etc/rc.d"
			INITDDIR=".."
			;;

		*)
			INITDIR="/etc"
			INITDDIR="../init.d"
			;;

	esac
else
	INITDIR=""
	INITDDIR="$rcdir"
fi

AC_SUBST(INITDIR)
AC_SUBST(INITDDIR)

dnl Setup default locations...
CUPS_SERVERROOT="$sysconfdir/cups"
CUPS_REQUESTS="$localstatedir/spool/cups"

AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$sysconfdir/cups")
AC_DEFINE_UNQUOTED(CUPS_REQUESTS, "$localstatedir/spool/cups")

if test x$logdir = x; then
	CUPS_LOGDIR="$localstatedir/log/cups"
	AC_DEFINE_UNQUOTED(CUPS_LOGDIR, "$localstatedir/log/cups")
else
	CUPS_LOGDIR="$logdir"
	AC_DEFINE_UNQUOTED(CUPS_LOGDIR, "$logdir")
fi

dnl See what directory to put server executables...
case "$uname" in
	*BSD* | Darwin*)
		# *BSD and Darwin (MacOS X)
		INSTALL_SYSV=""
		CUPS_SERVERBIN="$exec_prefix/libexec/cups"
		AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$exec_prefix/libexec/cups")
		;;
	*)
		# All others
		INSTALL_SYSV="install-sysv"
		CUPS_SERVERBIN="$libdir/cups"
		AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$libdir/cups")
		;;
esac

AC_SUBST(INSTALL_SYSV)
AC_SUBST(CUPS_SERVERROOT)
AC_SUBST(CUPS_SERVERBIN)
AC_SUBST(CUPS_LOGDIR)
AC_SUBST(CUPS_REQUESTS)

dnl Set the CUPS_LOCALE directory...
case "$uname" in
	Linux | GNU | *BSD* | Darwin*)
		CUPS_LOCALEDIR="$datadir/locale"
		AC_DEFINE_UNQUOTED(CUPS_LOCALEDIR, "$datadir/locale")
		;;

	OSF1* | AIX*)
		CUPS_LOCALEDIR="$exec_prefix/lib/nls/msg"
		AC_DEFINE_UNQUOTED(CUPS_LOCALEDIR, "$exec_prefix/lib/nls/msg")
		;;

	*)
		# This is the standard System V location...
		CUPS_LOCALEDIR="$exec_prefix/lib/locale"
		AC_DEFINE_UNQUOTED(CUPS_LOCALEDIR, "$exec_prefix/lib/locale")
		;;
esac

AC_SUBST(CUPS_LOCALEDIR)

dnl Set the CUPS_DATADIR directory...
CUPS_DATADIR="$datadir/cups"
AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$datadir/cups")
AC_SUBST(CUPS_DATADIR)

dnl Set the CUPS_DOCROOT directory...
if test x$docdir = x; then
	CUPS_DOCROOT="$datadir/doc/cups"
	docdir="$datadir/doc/cups"
else
	CUPS_DOCROOT="$docdir"
fi

AC_DEFINE_UNQUOTED(CUPS_DOCROOT, "$docdir")
AC_SUBST(CUPS_DOCROOT)

dnl Set the CUPS_FONTPATH directory...
CUPS_FONTPATH="$fontpath"
AC_SUBST(CUPS_FONTPATH)
AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$fontpath")

dnl
dnl End of "$Id$".
dnl
