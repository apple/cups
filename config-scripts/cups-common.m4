dnl
dnl "$Id$"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007-2008 by Apple Inc.
dnl   Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl We need at least autoconf 2.60...
AC_PREREQ(2.60)

dnl Set the name of the config header file...
AC_CONFIG_HEADER(config.h)

dnl Version number information...
CUPS_VERSION="1.4svn"
CUPS_REVISION=""
if test -z "$CUPS_REVISION" -a -d .svn; then
	CUPS_REVISION="-r`svnversion . | awk -F: '{print $NF}' | sed -e '1,$s/[[a-zA-Z]]*//g'`"
fi

AC_SUBST(CUPS_VERSION)
AC_SUBST(CUPS_REVISION)
AC_DEFINE_UNQUOTED(CUPS_SVERSION, "CUPS v$CUPS_VERSION$CUPS_REVISION")
AC_DEFINE_UNQUOTED(CUPS_MINIMAL, "CUPS/$CUPS_VERSION$CUPS_REVISION")

dnl Default compiler flags...
CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"

dnl Checks for programs...
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)
AC_PATH_PROG(HTMLDOC,htmldoc)
AC_PATH_PROG(LD,ld)
AC_PATH_PROG(LN,ln)
AC_PATH_PROG(MV,mv)
AC_PATH_PROG(RM,rm)
AC_PATH_PROG(RMDIR,rmdir)
AC_PATH_PROG(SED,sed)
AC_PATH_PROG(XDGOPEN,xdg-open)
if test "x$XDGOPEN" = x; then
	CUPS_HTMLVIEW="htmlview"
else
	CUPS_HTMLVIEW="$XDGOPEN"
fi
AC_SUBST(CUPS_HTMLVIEW)

AC_MSG_CHECKING(for install-sh script)
INSTALL="`pwd`/install-sh -c"
AC_SUBST(INSTALL)
AC_MSG_RESULT(using $INSTALL)

if test "x$AR" = x; then
	AC_MSG_ERROR([Unable to find required library archive command.])
fi
if test "x$CC" = x; then
	AC_MSG_ERROR([Unable to find required C compiler command.])
fi

dnl Static library option...
INSTALLSTATIC=""
AC_ARG_ENABLE(static, [  --enable-static         install static libraries, default=no])

if test x$enable_static = xyes; then
	echo Installing static libraries...
	INSTALLSTATIC="installstatic"
fi

AC_SUBST(INSTALLSTATIC)

dnl Check for libraries...
AC_SEARCH_LIBS(crypt, crypt)
AC_SEARCH_LIBS(getspent, sec gen)

LIBMALLOC=""
AC_ARG_ENABLE(mallinfo, [  --enable-mallinfo       turn on malloc debug information, default=no])

if test x$enable_mallinfo = xyes; then
	SAVELIBS="$LIBS"
	LIBS=""
	AC_SEARCH_LIBS(mallinfo, malloc, AC_DEFINE(HAVE_MALLINFO))
	LIBMALLOC="$LIBS"
	LIBS="$SAVELIBS"
fi

AC_SUBST(LIBMALLOC)

dnl Check for libpaper support...
AC_ARG_ENABLE(libpaper, [  --enable-libpaper       turn on libpaper support, default=no])

if test x$enable_libpaper = xyes; then
	AC_CHECK_LIB(paper,systempapername,
		AC_DEFINE(HAVE_LIBPAPER)
		LIBPAPER="-lpaper",
		LIBPAPER="")
else
	LIBPAPER=""
fi
AC_SUBST(LIBPAPER)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER(crypt.h,AC_DEFINE(HAVE_CRYPT_H))
AC_CHECK_HEADER(langinfo.h,AC_DEFINE(HAVE_LANGINFO_H))
AC_CHECK_HEADER(malloc.h,AC_DEFINE(HAVE_MALLOC_H))
AC_CHECK_HEADER(shadow.h,AC_DEFINE(HAVE_SHADOW_H))
AC_CHECK_HEADER(string.h,AC_DEFINE(HAVE_STRING_H))
AC_CHECK_HEADER(strings.h,AC_DEFINE(HAVE_STRINGS_H))
AC_CHECK_HEADER(bstring.h,AC_DEFINE(HAVE_BSTRING_H))
AC_CHECK_HEADER(usersec.h,AC_DEFINE(HAVE_USERSEC_H))
AC_CHECK_HEADER(sys/ioctl.h,AC_DEFINE(HAVE_SYS_IOCTL_H))
AC_CHECK_HEADER(sys/param.h,AC_DEFINE(HAVE_SYS_PARAM_H))
AC_CHECK_HEADER(sys/ucred.h,AC_DEFINE(HAVE_SYS_UCRED_H))
AC_CHECK_HEADER(scsi/sg.h,AC_DEFINE(HAVE_SCSI_SG_H))

dnl Checks for string functions.
AC_CHECK_FUNCS(strdup strcasecmp strncasecmp strlcat strlcpy)
if test "$uname" = "HP-UX" -a "$uversion" = "1020"; then
	echo Forcing snprintf emulation for HP-UX.
else
	AC_CHECK_FUNCS(snprintf vsnprintf)
fi

dnl Check for random number functions...
AC_CHECK_FUNCS(random mrand48 lrand48)

dnl Checks for mkstemp and mkstemps functions.
AC_CHECK_FUNCS(mkstemp mkstemps)

dnl Check for geteuid function.
AC_CHECK_FUNCS(geteuid)

dnl Check for vsyslog function.
AC_CHECK_FUNCS(vsyslog)

dnl Checks for signal functions.
case "$uname" in
	Linux | GNU)
		# Do not use sigset on Linux or GNU HURD
		;;
	*)
		# Use sigset on other platforms, if available
		AC_CHECK_FUNCS(sigset)
		;;
esac

AC_CHECK_FUNCS(sigaction)

dnl Checks for wait functions.
AC_CHECK_FUNCS(waitpid wait3)

dnl See if the tm structure has the tm_gmtoff member...
AC_MSG_CHECKING(for tm_gmtoff member in tm structure)
AC_TRY_COMPILE([#include <time.h>],[struct tm t;
	int o = t.tm_gmtoff;],
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_TM_GMTOFF),
	AC_MSG_RESULT(no))

dnl See if we have the removefile(3) function for securely removing files
AC_CHECK_FUNCS(removefile)

dnl See if we have libusb...
AC_ARG_ENABLE(libusb, [  --enable-libusb         use libusb for USB printing, default=auto])

LIBUSB=""
AC_SUBST(LIBUSB)

if test x$enable_libusb = xyes; then
	check_libusb=yes
elif test x$enable_libusb != xno -a $uname != Darwin; then
	check_libusb=yes
else
	check_libusb=no
fi

if test $check_libusb = yes; then
	AC_CHECK_LIB(usb, usb_init,[
		AC_CHECK_HEADER(usb.h,
			AC_DEFINE(HAVE_USB_H)
			LIBUSB="-lusb")])
fi

dnl See if we have libwrap for TCP wrappers support...
AC_ARG_ENABLE(tcp_wrappers, [  --enable-tcp-wrappers   use libwrap for TCP wrappers support, default=no])

LIBWRAP=""
AC_SUBST(LIBWRAP)

if test x$enable_tcp_wrappers = xyes; then
	AC_CHECK_LIB(wrap, hosts_access,[
		AC_CHECK_HEADER(tcpd.h,
			AC_DEFINE(HAVE_TCPD_H)
			LIBWRAP="-lwrap")])
fi

dnl Flags for "ar" command...
case $uname in
        Darwin* | *BSD*)
                ARFLAGS="-rcv"
                ;;
        *)
                ARFLAGS="crvs"
                ;;
esac

AC_SUBST(ARFLAGS)

dnl Prep libraries specifically for cupsd and backends...
BACKLIBS=""
CUPSDLIBS=""
AC_SUBST(BACKLIBS)
AC_SUBST(CUPSDLIBS)

dnl See if we have POSIX ACL support...
SAVELIBS="$LIBS"
LIBS=""
AC_ARG_ENABLE(acl, [  --enable-acl            enable POSIX ACL support, default=auto])
if test "x$enable_acl" != xno; then
	AC_SEARCH_LIBS(acl_init, acl, AC_DEFINE(HAVE_ACL_INIT))
	CUPSDLIBS="$CUPSDLIBS $LIBS"
fi
LIBS="$SAVELIBS"

dnl Check for DBUS support
if test -d /etc/dbus-1; then
	DBUSDIR="/etc/dbus-1"
else
	DBUSDIR=""
fi

AC_ARG_ENABLE(dbus, [  --enable-dbus           enable DBUS support, default=auto])
AC_ARG_WITH(dbusdir, [  --with-dbusdir          set DBUS configuration directory ],
	DBUSDIR="$withval")

if test "x$enable_dbus" != xno; then
	AC_PATH_PROG(PKGCONFIG, pkg-config)
	if test "x$PKGCONFIG" != x; then
		AC_MSG_CHECKING(for DBUS)
		if $PKGCONFIG --exists dbus-1; then
			AC_MSG_RESULT(yes)
			AC_DEFINE(HAVE_DBUS)
			CFLAGS="$CFLAGS `$PKGCONFIG --cflags dbus-1` -DDBUS_API_SUBJECT_TO_CHANGE"
			CUPSDLIBS="$CUPSDLIBS `$PKGCONFIG --libs dbus-1`"
			AC_CHECK_LIB(dbus-1,
			    dbus_message_iter_init_append,
			    AC_DEFINE(HAVE_DBUS_MESSAGE_ITER_INIT_APPEND),,
			    `$PKGCONFIG --libs dbus-1`)
		else
			AC_MSG_RESULT(no)
		fi
	fi
fi

AC_SUBST(DBUSDIR)

dnl Extra platform-specific libraries...
CUPS_DEFAULT_PRINTADMIN_AUTH="@SYSTEM"
CUPS_SYSTEM_AUTHKEY=""
FONTS="fonts"
LEGACY_BACKENDS="parallel scsi"

case $uname in
        Darwin*)
		FONTS=""
		LEGACY_BACKENDS=""
                BACKLIBS="$BACKLIBS -framework IOKit"
                CUPSDLIBS="$CUPSDLIBS -sectorder __TEXT __text cupsd.order -e start -framework IOKit -framework SystemConfiguration -weak_framework ApplicationServices"
                LIBS="-framework SystemConfiguration -framework CoreFoundation $LIBS"

		dnl Check for framework headers...
		AC_CHECK_HEADER(CoreFoundation/CoreFoundation.h,AC_DEFINE(HAVE_COREFOUNDATION_H))
		AC_CHECK_HEADER(CoreFoundation/CFPriv.h,AC_DEFINE(HAVE_CFPRIV_H))
		AC_CHECK_HEADER(CoreFoundation/CFBundlePriv.h,AC_DEFINE(HAVE_CFBUNDLEPRIV_H))

		dnl Check for the new membership functions in MacOSX 10.4...
		AC_CHECK_HEADER(membership.h,AC_DEFINE(HAVE_MEMBERSHIP_H))
		AC_CHECK_HEADER(membershipPriv.h,AC_DEFINE(HAVE_MEMBERSHIPPRIV_H))
		AC_CHECK_FUNCS(mbr_uid_to_uuid)

		dnl Need <dlfcn.h> header...
		AC_CHECK_HEADER(dlfcn.h,AC_DEFINE(HAVE_DLFCN_H))

		dnl Check for notify_post support
		AC_CHECK_HEADER(notify.h,AC_DEFINE(HAVE_NOTIFY_H))
		AC_CHECK_FUNCS(notify_post)

		dnl Check for Authorization Services support
		AC_ARG_WITH(adminkey, [  --with-adminkey         set the default SystemAuthKey value],
			default_adminkey="$withval",
			default_adminkey="default")
 		AC_ARG_WITH(operkey, [  --with-operkey          set the default operator @AUTHKEY value],
			default_operkey="$withval",
			default_operkey="default")
 
		AC_CHECK_HEADER(Security/Authorization.h, [
			AC_DEFINE(HAVE_AUTHORIZATION_H)

			if test "x$default_adminkey" != xdefault; then
				CUPS_SYSTEM_AUTHKEY="SystemGroupAuthKey $default_adminkey"
			elif grep -q system.print.operator /etc/authorization; then
				CUPS_SYSTEM_AUTHKEY="SystemGroupAuthKey system.print.admin"
			else
				CUPS_SYSTEM_AUTHKEY="SystemGroupAuthKey system.preferences"
			fi

			if test "x$default_operkey" != xdefault; then
				CUPS_DEFAULT_PRINTADMIN_AUTH="@AUTHKEY($default_operkey) @admin @lpadmin"
			elif grep -q system.print.operator /etc/authorization; then
				CUPS_DEFAULT_PRINTADMIN_AUTH="@AUTHKEY(system.print.operator) @admin @lpadmin"
			else
				CUPS_DEFAULT_PRINTADMIN_AUTH="@AUTHKEY(system.print.admin) @admin @lpadmin"
			fi])
		AC_CHECK_HEADER(Security/SecBasePriv.h,AC_DEFINE(HAVE_SECBASEPRIV_H))

		dnl Check for sandbox/Seatbelt support
		AC_CHECK_HEADER(sandbox.h,AC_DEFINE(HAVE_SANDBOX_H))
                ;;
esac

AC_SUBST(CUPS_DEFAULT_PRINTADMIN_AUTH)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_PRINTADMIN_AUTH, "$CUPS_DEFAULT_PRINTADMIN_AUTH")
AC_SUBST(CUPS_SYSTEM_AUTHKEY)
AC_SUBST(FONTS)
AC_SUBST(LEGACY_BACKENDS)

dnl
dnl End of "$Id$".
dnl
