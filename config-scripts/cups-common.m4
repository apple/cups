dnl
dnl "$Id: cups-common.m4,v 1.12.2.7 2002/03/08 20:23:01 mike Exp $"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
dnl       Hollywood, Maryland 20636-3111 USA
dnl
dnl       Voice: (301) 373-9603
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

dnl We need at least autoconf 2.13...
AC_PREREQ(2.13)

dnl Set the name of the config header file...
AC_CONFIG_HEADER(config.h)

CUPS_VERSION="1.2.0a1"
AC_SUBST(CUPS_VERSION)
AC_DEFINE_UNQUOTED(CUPS_SVERSION, "CUPS v$CUPS_VERSION")

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
AC_PATH_PROG(MV,mv)
AC_PATH_PROG(NROFF,nroff)
if test "$NROFF" = ""; then
	AC_PATH_PROG(GROFF,groff)
	if test "$GROFF" = ""; then
        	NROFF="echo"
	else
        	NROFF="$GROFF -T ascii"
	fi
fi
AC_PATH_PROG(RM,rm)
AC_PATH_PROG(SED,sed)
AC_PATH_PROG(STRIP,strip)

dnl Check for libraries...
AC_CHECK_LIB(c,crypt,LIBS="$LIBS")
if test "$ac_cv_lib_c_crypt" = "no"; then
	AC_CHECK_LIB(crypt,crypt)
fi
AC_SEARCH_LIBS(getspent, sec, gen)

LIBMALLOC=""
AC_CHECK_LIB(c,mallinfo,LIBS="$LIBS"; AC_DEFINE(HAVE_MALLINFO),LIBS="$LIBS")
if test "$ac_cv_lib_c_mallinfo" = "no"; then
	AC_CHECK_LIB(malloc,mallinfo,
	             LIBS="$LIBS"
		     LIBMALLOC="-lmalloc"
		     AC_DEFINE(HAVE_MALLINFO),
		     LIBS="$LIBS")
fi
AC_SUBST(LIBMALLOC)

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADER(crypt.h,AC_DEFINE(HAVE_CRYPT_H))
AC_CHECK_HEADER(malloc.h,AC_DEFINE(HAVE_MALLOC_H))
AC_CHECK_HEADER(shadow.h,AC_DEFINE(HAVE_SHADOW_H))
AC_CHECK_HEADER(string.h,AC_DEFINE(HAVE_STRING_H))
AC_CHECK_HEADER(strings.h,AC_DEFINE(HAVE_STRINGS_H))
AC_CHECK_HEADER(usersec.h,AC_DEFINE(HAVE_USERSEC_H))
AC_CHECK_HEADER(sys/ioctl.h,AC_DEFINE(HAVE_SYS_IOCTL_H))

dnl Checks for string functions.
AC_CHECK_FUNCS(strdup strcasecmp strncasecmp)
if test "$uname" = "HP-UX" -a "$uversion" = "1020"; then
	echo Forcing snprintf emulation for HP-UX.
else
	AC_CHECK_FUNCS(snprintf vsnprintf)
fi

dnl Check OS version and use appropriate format string for strftime...
AC_MSG_CHECKING(for correct format string to use with strftime)

case "$uname" in
	IRIX* | SunOS*)
		# IRIX and SunOS
		AC_MSG_RESULT(NULL)
		AC_DEFINE(CUPS_STRFTIME_FORMAT, NULL)
		;;
	*)
		# All others
		AC_MSG_RESULT("%c")
		AC_DEFINE(CUPS_STRFTIME_FORMAT, "%c")
		;;
esac

dnl Checks for mkstemp and mkstemps functions.
AC_CHECK_FUNCS(mkstemp mkstemps)

dnl Checks for vsyslog function.
AC_CHECK_FUNCS(vsyslog)

dnl Checks for signal functions.
if test "$uname" != "Linux"; then
	AC_CHECK_FUNCS(sigset)
fi

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

dnl
dnl End of "$Id: cups-common.m4,v 1.12.2.7 2002/03/08 20:23:01 mike Exp $".
dnl
