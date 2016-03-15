dnl
dnl "$Id: cups-directories.m4 11717 2014-03-21 16:42:53Z msweet $"
dnl
dnl Directory stuff for CUPS.
dnl
dnl Copyright 2007-2014 by Apple Inc.
dnl Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl These coded instructions, statements, and computer programs are the
dnl property of Apple Inc. and are protected by Federal copyright
dnl law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl which should have been included with this file.  If this file is
dnl file is missing or damaged, see the license at "http://www.cups.org/".
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

AC_DEFINE_UNQUOTED(CUPS_BINDIR, "$bindir")

dnl Fix "sbindir" variable...
if test "$sbindir" = "\${exec_prefix}/sbin"; then
	sbindir="$exec_prefix/sbin"
fi

AC_DEFINE_UNQUOTED(CUPS_SBINDIR, "$sbindir")

dnl Fix "sharedstatedir" variable if it hasn't been specified...
if test "$sharedstatedir" = "\${prefix}/com" -a "$prefix" = "/"; then
	sharedstatedir="/usr/com"
fi

dnl Fix "datarootdir" variable if it hasn't been specified...
if test "$datarootdir" = "\${prefix}/share"; then
	if test "$prefix" = "/"; then
		datarootdir="/usr/share"
	else
		datarootdir="$prefix/share"
	fi
fi

dnl Fix "datadir" variable if it hasn't been specified...
if test "$datadir" = "\${prefix}/share"; then
	if test "$prefix" = "/"; then
		datadir="/usr/share"
	else
		datadir="$prefix/share"
	fi
elif test "$datadir" = "\${datarootdir}"; then
	datadir="$datarootdir"
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

dnl Fix "libdir" variable...
if test "$libdir" = "\${exec_prefix}/lib"; then
	case "$uname" in
		Linux*)
			if test -d /usr/lib64 -a ! -d /usr/lib64/fakeroot; then
				libdir="$exec_prefix/lib64"
			fi
			;;
	esac
fi

dnl Setup private include directory...
AC_ARG_WITH(privateinclude, [  --with-privateinclude   set path for private include files, default=none],privateinclude="$withval",privateinclude="")
if test "x$privateinclude" != x -a "x$privateinclude" != xnone; then
	PRIVATEINCLUDE="$privateinclude/cups"
else
	privateinclude=""
	PRIVATEINCLUDE=""
fi
AC_SUBST(privateinclude)
AC_SUBST(PRIVATEINCLUDE)

dnl LPD sharing support...
AC_ARG_WITH(lpdconfig, [  --with-lpdconfig        set URI for LPD config file],
	LPDCONFIG="$withval", LPDCONFIG="")

if test "x$LPDCONFIG" = x; then
	if test -f /System/Library/LaunchDaemons/org.cups.cups-lpd.plist; then
		LPDCONFIG="launchd:///System/Library/LaunchDaemons/org.cups.cups-lpd.plist"
	elif test "x$XINETD" != x; then
		LPDCONFIG="xinetd://$XINETD/cups-lpd"
	fi
fi

if test "x$LPDCONFIG" = xoff; then
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LPD_CONFIG, "")
else
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LPD_CONFIG, "$LPDCONFIG")
fi

dnl SMB sharing support...
AC_ARG_WITH(smbconfig, [  --with-smbconfig        set URI for Samba config file],
	SMBCONFIG="$withval", SMBCONFIG="")

if test "x$SMBCONFIG" = x; then
	if test -f /System/Library/LaunchDaemons/smbd.plist; then
		SMBCONFIG="launchd:///System/Library/LaunchDaemons/smbd.plist"
	else
		for dir in /etc /etc/samba /usr/local/etc; do
			if test -f $dir/smb.conf; then
				SMBCONFIG="samba://$dir/smb.conf"
				break
			fi
		done
	fi
fi

if test "x$SMBCONFIG" = xoff; then
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_SMB_CONFIG, "")
else
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_SMB_CONFIG, "$SMBCONFIG")
fi

dnl Setup default locations...
# Cache data...
AC_ARG_WITH(cachedir, [  --with-cachedir         set path for cache files],cachedir="$withval",cachedir="")

if test x$cachedir = x; then
	if test "x$uname" = xDarwin; then
		CUPS_CACHEDIR="$localstatedir/spool/cups/cache"
	else
		CUPS_CACHEDIR="$localstatedir/cache/cups"
	fi
else
	CUPS_CACHEDIR="$cachedir"
fi
AC_DEFINE_UNQUOTED(CUPS_CACHEDIR, "$CUPS_CACHEDIR")
AC_SUBST(CUPS_CACHEDIR)

# Data files
CUPS_DATADIR="$datadir/cups"
AC_DEFINE_UNQUOTED(CUPS_DATADIR, "$datadir/cups")
AC_SUBST(CUPS_DATADIR)

# Icon directory
AC_ARG_WITH(icondir, [  --with-icondir          set path for application icons],icondir="$withval",icondir="")

if test "x$icondir" = x -a -d /usr/share/icons; then
	ICONDIR="/usr/share/icons"
else
	ICONDIR="$icondir"
fi

AC_SUBST(ICONDIR)

# Menu directory
AC_ARG_WITH(menudir, [  --with-menudir          set path for application menus],menudir="$withval",menudir="")

if test "x$menudir" = x -a -d /usr/share/applications; then
	MENUDIR="/usr/share/applications"
else
	MENUDIR="$menudir"
fi

AC_SUBST(MENUDIR)

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
if test "$localedir" = "\${datarootdir}/locale"; then
	case "$uname" in
		Linux | GNU | *BSD* | Darwin*)
			CUPS_LOCALEDIR="$datarootdir/locale"
			;;

		*)
			# This is the standard System V location...
			CUPS_LOCALEDIR="$exec_prefix/lib/locale"
			;;
	esac
else
	CUPS_LOCALEDIR="$localedir"
fi

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
		CUPS_SERVERBIN="$exec_prefix/lib/cups"
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
AC_ARG_WITH(rundir, [  --with-rundir           set transient run-time state directory],CUPS_STATEDIR="$withval",[
	case "$uname" in
		Darwin*)
			# Darwin (OS X)
			CUPS_STATEDIR="$CUPS_SERVERROOT"
			;;
		*)
			# All others
			CUPS_STATEDIR="$localstatedir/run/cups"
			;;
	esac])
AC_DEFINE_UNQUOTED(CUPS_STATEDIR, "$CUPS_STATEDIR")
AC_SUBST(CUPS_STATEDIR)

dnl
dnl End of "$Id: cups-directories.m4 11717 2014-03-21 16:42:53Z msweet $".
dnl
