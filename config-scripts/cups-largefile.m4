dnl
dnl "$Id$"
dnl
dnl   Large file support stuff for the Common UNIX Printing System (CUPS).
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

dnl Check for largefile support...
AC_SYS_LARGEFILE

dnl Define largefile options as needed...
LARGEFILE=""
if test x$enable_largefile != xno; then
	LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"

	if test $ac_cv_sys_large_files = 1; then
		LARGEFILE="$LARGEFILE -D_LARGE_FILES"
	fi

	if test $ac_cv_sys_file_offset_bits = 64; then
		LARGEFILE="$LARGEFILE -D_FILE_OFFSET_BITS=64"
	fi
fi
AC_SUBST(LARGEFILE)

dnl Check for "long long" support...
AC_CACHE_CHECK(for long long int, ac_cv_c_long_long,
	[if test "$GCC" = yes; then
		ac_cv_c_long_long=yes
	else
		AC_TRY_COMPILE(,[long long int i;],
			ac_cv_c_long_long=yes,
			ac_cv_c_long_long=no)
	fi])

if test $ac_cv_c_long_long = yes; then
	AC_DEFINE(HAVE_LONG_LONG)
fi

AC_CHECK_FUNC(strtoll, AC_DEFINE(HAVE_STRTOLL))

dnl
dnl End of "$Id$".
dnl
