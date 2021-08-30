dnl
dnl Threading stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2017 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
dnl

have_pthread="no"
PTHREAD_FLAGS=""
AC_SUBST([PTHREAD_FLAGS])

AC_CHECK_HEADER([pthread.h], [
    AC_DEFINE([HAVE_PTHREAD_H], [1], [Do we have the <pthread.h> header?])
])

AS_IF([test x$ac_cv_header_pthread_h = xyes], [
    dnl Check various threading options for the platforms we support
    for flag in -lpthreads -lpthread -pthread; do
	AC_MSG_CHECKING([for pthread_create using $flag])
	SAVELIBS="$LIBS"
	LIBS="$flag $LIBS"
	AC_LINK_IFELSE([
	    AC_LANG_PROGRAM([[#include <pthread.h>]], [[
		pthread_create(0, 0, 0, 0);
	    ]])
	], [
	    have_pthread="yes"
	], [
	    LIBS="$SAVELIBS"
	])
	AC_MSG_RESULT([$have_pthread])

	AS_IF([test $have_pthread = yes], [
	    PTHREAD_FLAGS="-D_THREAD_SAFE -D_REENTRANT"

	    # Solaris requires -D_POSIX_PTHREAD_SEMANTICS to be POSIX-
	    # compliant... :(
	    AS_IF([test $host_os_name = sunos], [
		PTHREAD_FLAGS="$PTHREAD_FLAGS -D_POSIX_PTHREAD_SEMANTICS"
	    ])
	    break
	])
    done
])

AS_IF([test $have_pthread = no], [
    AC_MSG_ERROR([CUPS requires threading support.])
])
