dnl
dnl "$Id: cups-threads.m4 11324 2013-10-04 03:11:42Z msweet $"
dnl
dnl   Threading stuff for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(threads, [  --disable-threads       disable multi-threading support])

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

				# Solaris requires -D_POSIX_PTHREAD_SEMANTICS to
				# be POSIX-compliant... :(
				if test $uname = SunOS; then
					PTHREAD_FLAGS="$PTHREAD_FLAGS -D_POSIX_PTHREAD_SEMANTICS"
				fi
				break
			fi
		done
	fi
fi

AC_SUBST(PTHREAD_FLAGS)

dnl
dnl End of "$Id: cups-threads.m4 11324 2013-10-04 03:11:42Z msweet $".
dnl
