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

dnl Fix "bindir" variable...
if test "$bindir" = "\${exec_prefix}/bin"; then
	bindir="$exec_prefix/bin"
fi

dnl Fix "sbindir" variable...
if test "$sbindir" = "\${exec_prefix}/sbin"; then
	sbindir="$exec_prefix/sbin"
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
		if test "$uname" = Darwin; then
			localstatedir="/private/var"
		else
			localstatedir="/var"
		fi
	else
		localstatedir="$prefix/var"
	fi
fi

dnl Fix "sysconfdir" variable if it hasn't been specified...
if test "$sysconfdir" = "\${prefix}/etc"; then
	if test "$prefix" = "/"; then
		if test "$uname" = Darwin; then
			sysconfdir="/private/etc"
		else
			sysconfdir="/etc"
		fi
	else
		sysconfdir="$prefix/etc"
	fi
fi

dnl Fix "libdir" variable for IRIX 6.x...
if test "$libdir" = "\${exec_prefix}/lib"; then
	if test "$uname" = "IRIX" -a $uversion -ge 62; then
		libdir="$exec_prefix/lib32"
	else
		libdir="$exec_prefix/lib"
	fi
fi

dnl Setup init.d locations...
AC_ARG_WITH(rcdir, [  --with-rcdir            set path for rc scripts],rcdir="$withval",rcdir="")

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
				if test -d /etc/init.d; then
					# Others
					INITDIR="/etc"
					INITDDIR="../init.d"
				else
					# RedHat
					INITDIR="/etc/rc.d"
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
# Cache data...
AC_ARG_WITH(cachedir, [  --with-cachedir         set path for cache files],cachedir="$withval",cachedir="")

if test x$cachedir = x; then
	CUPS_CACHEDIR="$localstatedir/cache/cups"
else
	CUPS_CACHEDIR="$cachedir"
fi
AC_DEFINE_UNQUOTED(CUPS_CACHEDIR, "$CUPS_CACHEDIR")
AC_SUBST(CUPS_CACHEDIR)

# Data files
CUPS_DATADIR="$datadir/cups"
AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$datadir/cups")
AC_SUBST(CUPS_DATADIR)

# Documentation files
AC_ARG_WITH(docdir, [  --with-docdir           set path for documentation],docdir="$withval",docdir="")

if test x$docdir = x; then
	CUPS_DOCROOT="$datadir/doc/cups"
	docdir="$datadir/doc/cups"
else
	CUPS_DOCROOT="$docdir"
fi

AC_DEFINE_UNQUOTED(CUPS_DOCROOT, "$docdir")
AC_SUBST(CUPS_DOCROOT)

# Fonts
AC_ARG_WITH(fontpath, [  --with-fontpath         set font path for pstoraster],fontpath="$withval",fontpath="")

if test "x$fontpath" = "x"; then
	CUPS_FONTPATH="$datadir/cups/fonts"
else
	CUPS_FONTPATH="$fontpath"
fi

AC_SUBST(CUPS_FONTPATH)
AC_DEFINE_UNQUOTED(CUPS_FONTPATH, "$CUPS_FONTPATH")

# Locale data
case "$uname" in
	Linux | GNU | *BSD* | Darwin*)
		CUPS_LOCALEDIR="$datadir/locale"
		;;

	OSF1* | AIX*)
		CUPS_LOCALEDIR="$exec_prefix/lib/nls/msg"
		;;

	*)
		# This is the standard System V location...
		CUPS_LOCALEDIR="$exec_prefix/lib/locale"
		;;
esac

AC_DEFINE_UNQUOTED(CUPS_LOCALEDIR, "$CUPS_LOCALEDIR")
AC_SUBST(CUPS_LOCALEDIR)

# Log files...
AC_ARG_WITH(logdir, [  --with-logdir           set path for log files],logdir="$withval",logdir="")

if test x$logdir = x; then
	CUPS_LOGDIR="$localstatedir/log/cups"
	AC_DEFINE_UNQUOTED(CUPS_LOGDIR, "$localstatedir/log/cups")
else
	CUPS_LOGDIR="$logdir"
fi
AC_DEFINE_UNQUOTED(CUPS_LOGDIR, "$CUPS_LOGDIR")
AC_SUBST(CUPS_LOGDIR)

# Longer-term spool data
CUPS_REQUESTS="$localstatedir/spool/cups"
AC_DEFINE_UNQUOTED(CUPS_REQUESTS, "$localstatedir/spool/cups")
AC_SUBST(CUPS_REQUESTS)

# Server executables...
case "$uname" in
	*BSD* | Darwin*)
		# *BSD and Darwin (MacOS X)
		INSTALL_SYSV=""
		CUPS_SERVERBIN="$exec_prefix/libexec/cups"
		;;
	*)
		# All others
		INSTALL_SYSV="install-sysv"
		CUPS_SERVERBIN="$libdir/cups"
		;;
esac

AC_DEFINE_UNQUOTED(CUPS_SERVERBIN, "$CUPS_SERVERBIN")
AC_SUBST(CUPS_SERVERBIN)
AC_SUBST(INSTALL_SYSV)

# Configuration files
CUPS_SERVERROOT="$sysconfdir/cups"
AC_DEFINE_UNQUOTED(CUPS_SERVERROOT, "$sysconfdir/cups")
AC_SUBST(CUPS_SERVERROOT)

# Transient run-time state
CUPS_STATEDIR="$localstatedir/run/cups"
AC_DEFINE_UNQUOTED(CUPS_STATEDIR, "$localstatedir/run/cups")
AC_SUBST(CUPS_STATEDIR)

dnl
dnl End of "$Id$".
dnl
