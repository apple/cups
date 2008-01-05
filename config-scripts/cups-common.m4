dnl
dnl "$Id$"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007 by Apple Inc.
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

dnl Versio number information...
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
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
if test "$INSTALL" = "$ac_install_sh"; then
	# Use full path to install-sh script...
	INSTALL="`pwd`/install-sh -c"
fi
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)
AC_PATH_PROG(HTMLDOC,htmldoc)
AC_PATH_PROG(LD,ld)
AC_PATH_PROG(LN,ln)
AC_PATH_PROG(MV,mv)
AC_PATH_PROG(RM,rm)
AC_PATH_PROG(RMDIR,rmdir)
AC_PATH_PROG(SED,sed)
AC_PATH_PROG(STRIP,strip)

if test "x$AR" = x; then
	AC_MSG_ERROR([Unable to find required library archive command.])
fi
if test "x$CC" = x; then
	AC_MSG_ERROR([Unable to find required C compiler command.])
fi
if test "x$CXX" = x; then
	AC_MSG_ERROR([Unable to find required C++ compiler command.])
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
	AC_CHECK_LIB(c,mallinfo,LIBS="$LIBS"; AC_DEFINE(HAVE_MALLINFO),LIBS="$LIBS")
	if test "$ac_cv_lib_c_mallinfo" = "no"; then
		AC_CHECK_LIB(malloc,mallinfo,
	        	     LIBS="$LIBS"
			     LIBMALLOC="-lmalloc"
			     AC_DEFINE(HAVE_MALLINFO),
			     LIBS="$LIBS")
	fi
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

dnl Extra platform-specific libraries...
BACKLIBS=""
CUPSDLIBS=""
DBUSDIR=""
CUPS_DEFAULT_PRINTADMIN_AUTH="@SYSTEM"
CUPS_SYSTEM_AUTHKEY=""

AC_ARG_ENABLE(dbus, [  --enable-dbus           enable DBUS support, default=auto])

FONTS="fonts"
AC_SUBST(FONTS)
LEGACY_BACKENDS="parallel scsi"
AC_SUBST(LEGACY_BACKENDS)

case $uname in
        Darwin*)
		FONTS=""
		LEGACY_BACKENDS=""
                BACKLIBS="-framework IOKit"
                CUPSDLIBS="-sectorder __TEXT __text cupsd.order -e start -framework IOKit -framework SystemConfiguration"
                LIBS="-framework SystemConfiguration -framework CoreFoundation $LIBS"

		dnl Check for CFLocaleCreateCanonicalLocaleIdentifierFromString...
		AC_MSG_CHECKING(for CFLocaleCreateCanonicalLocaleIdentifierFromString)
		if test "$uname" = "Darwin" -a $uversion -ge 70; then
			AC_DEFINE(HAVE_CF_LOCALE_ID)
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
		fi

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
		AC_CHECK_HEADER(Security/Authorization.h, [
			AC_DEFINE(HAVE_AUTHORIZATION_H)
			CUPS_DEFAULT_PRINTADMIN_AUTH="@AUTHKEY(system.print.admin) @admin @lpadmin"
			CUPS_SYSTEM_AUTHKEY="SystemGroupAuthKey system.preferences"])
		AC_CHECK_HEADER(Security/SecBasePriv.h,AC_DEFINE(HAVE_SECBASEPRIV_H))

		dnl Check for sandbox/Seatbelt support
		AC_CHECK_HEADER(sandbox.h,AC_DEFINE(HAVE_SANDBOX_H))
                ;;

	Linux*)
		dnl Check for DBUS support
		if test "x$enable_dbus" != xno; then
			AC_PATH_PROG(PKGCONFIG, pkg-config)
			if test "x$PKGCONFIG" != x; then
				AC_MSG_CHECKING(for DBUS)
				if $PKGCONFIG --exists dbus-1; then
					AC_MSG_RESULT(yes)
					AC_DEFINE(HAVE_DBUS)
					CFLAGS="$CFLAGS `$PKGCONFIG --cflags dbus-1` -DDBUS_API_SUBJECT_TO_CHANGE"
					CUPSDLIBS="`$PKGCONFIG --libs dbus-1`"
					AC_ARG_WITH(dbusdir, [  --with-dbusdir          set DBUS configuration directory ], dbusdir="$withval", dbusdir="/etc/dbus-1")
					DBUSDIR="$dbusdir"
					AC_CHECK_LIB(dbus-1,
					    dbus_message_iter_init_append,
					    AC_DEFINE(HAVE_DBUS_MESSAGE_ITER_INIT_APPEND))
				else
					AC_MSG_RESULT(no)
				fi
			fi
		fi
		;;
esac

AC_SUBST(CUPS_DEFAULT_PRINTADMIN_AUTH)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_PRINTADMIN_AUTH, "$CUPS_DEFAULT_PRINTADMIN_AUTH")
AC_SUBST(CUPS_SYSTEM_AUTHKEY)

dnl See if we have POSIX ACL support...
SAVELIBS="$LIBS"
LIBS=""
AC_SEARCH_LIBS(acl_init, acl, AC_DEFINE(HAVE_ACL_INIT))
CUPSDLIBS="$CUPSDLIBS $LIBS"
LIBS="$SAVELIBS"

AC_SUBST(BACKLIBS)
AC_SUBST(CUPSDLIBS)
AC_SUBST(DBUSDIR)

dnl New default port definition for IPP...
AC_ARG_WITH(ipp-port, [  --with-ipp-port         set default port number for IPP ],
	DEFAULT_IPP_PORT="$withval",
	DEFAULT_IPP_PORT="631")

AC_SUBST(DEFAULT_IPP_PORT)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IPP_PORT,$DEFAULT_IPP_PORT)

dnl
dnl End of "$Id$".
dnl
