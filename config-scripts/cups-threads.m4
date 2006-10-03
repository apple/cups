dnl
dnl "$Id$"
dnl
dnl   Threading stuff for the Common UNIX Printing System (CUPS).
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

AC_ARG_ENABLE(threads, [  --enable-threads        enable multi-threading support])

have_pthread=no
PTHREAD_FLAGS=""

if test "x$enable_threads" != xno; then
	AC_CHECK_HEADER(pthread.h, AC_DEFINE(HAVE_PTHREAD_H))

	if test x$ac_cv_header_pthread_h = xyes; then
		dnl Check various threading options for the platforms we support
		for flag in -lpthreads -lpthread -pthread; do
        		AC_MSG_CHECKING([for pthread_create using $flag])
			SAVELIBS="$LIBS"
			LIBS="$flag $LIBS"
        		AC_TRY_LINK([#include <pthread.h>],
				[pthread_create(0, 0, 0, 0);],
        			have_pthread=yes,
				LIBS="$SAVELIBS")
        		AC_MSG_RESULT([$have_pthread])

			if test $have_pthread = yes; then
				PTHREAD_FLAGS="-D_THREAD_SAFE -D_REENTRANT"

				# Solaris requires _POSIX_PTHREAD_SEMANTICS to
				# be POSIX-compliant... :(
				if test $uname = SunOS; then
					PTHREAD_FLAGS="$PTHREAD_FLAGS _POSIX_PTHREAD_SEMANTICS"
				fi
				break
			fi
		done
	fi
fi

AC_SUBST(PTHREAD_FLAGS)

dnl
dnl End of "$Id$".
dnl
