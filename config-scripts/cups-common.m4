dnl
dnl "$Id$"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
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

dnl We need at least autoconf 2.50...
AC_PREREQ(2.50)

dnl Set the name of the config header file...
AC_CONFIG_HEADER(config.h)

dnl Versio number information...
CUPS_VERSION="1.2svn"
AC_SUBST(CUPS_VERSION)
AC_DEFINE_UNQUOTED(CUPS_SVERSION, "CUPS v$CUPS_VERSION")
AC_DEFINE_UNQUOTED(CUPS_MINIMAL, "CUPS/$CUPS_VERSION")

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
AC_ARG_ENABLE(install_static, [  --enable-static         install static libraries, default=no])

if test x$enable_install_static = xyes; then
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
AC_CHECK_FUNCS(waitpid)
AC_CHECK_FUNCS(wait3)

dnl See if the tm structure has the tm_gmtoff member...
AC_MSG_CHECKING(for tm_gmtoff member in tm structure)
AC_TRY_COMPILE([#include <time.h>],[struct tm t;
	int o = t.tm_gmtoff;],
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_TM_GMTOFF),
	AC_MSG_RESULT(no))

dnl See if we have POSIX ACL support...
AC_SEARCH_LIBS(acl_init, acl, AC_DEFINE(HAVE_ACL_INIT))

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
case $uname in
        Darwin*)
                BACKLIBS="-framework IOKit"
                CUPSDLIBS="-framework IOKit -framework SystemConfiguration"
                LIBS="-framework CoreFoundation $LIBS"

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

		dnl Check for the new membership functions in MacOSX 10.4 (Tiger)...
		AC_CHECK_HEADER(membership.h,AC_DEFINE(HAVE_MEMBERSHIP_H))
		AC_CHECK_FUNCS(mbr_uid_to_uuid)

		dnl Check for notify_post support
		AC_CHECK_HEADER(notify.h,AC_DEFINE(HAVE_NOTIFY_H))
		AC_CHECK_FUNCS(notify_post)
                ;;

	Linux*)
		dnl Check for DBUS support
                BACKLIBS=""
		CUPSDLIBS=""

		AC_PATH_PROG(PKGCONFIG, pkg-config)
		if test "x$PKGCONFIG" != x; then
			AC_MSG_CHECKING(for DBUS)
			if $PKGCONFIG --exists dbus-1; then
				AC_MSG_RESULT(yes)
				AC_DEFINE(HAVE_DBUS)
				CFLAGS="$CFLAGS `$PKGCONFIG --cflags dbus-1` -DDBUS_API_SUBJECT_TO_CHANGE"
				CUPSDLIBS="`$PKGCONFIG --libs dbus-1`"
			else
				AC_MSG_RESULT(no)
			fi
		fi
		;;

        *)
                BACKLIBS=""
		CUPSDLIBS=""
                ;;
esac

AC_SUBST(BACKLIBS)
AC_SUBST(CUPSDLIBS)

dnl New default port definition for IPP...
AC_ARG_WITH(ipp-port, [  --with-ipp-port         set default port number for IPP ],
	DEFAULT_IPP_PORT="$withval",
	DEFAULT_IPP_PORT="631")

AC_SUBST(DEFAULT_IPP_PORT)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IPP_PORT,$DEFAULT_IPP_PORT)

dnl
dnl End of "$Id$".
dnl
