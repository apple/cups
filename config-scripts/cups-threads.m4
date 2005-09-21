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

if test "x$enable_threads" != xno; then
	AC_CHECK_HEADER(pthread.h, AC_DEFINE(HAVE_PTHREAD_H))
	AC_CHECK_LIB(pthread, pthread_create,
		COMMONLIBS="-lpthread $COMMONLIBS")

	if test "x$ac_cv_lib_pthread_pthread_create" = xyes -a x$ac_cv_header_pthread_h = xyes; then
        	have_pthread=yes
	else
        	dnl *BSD uses -pthread option...
        	AC_MSG_CHECKING([for pthread_create using -pthread])
		SAVELIBS="$LIBS"
		LIBS="-pthread $LIBS"
        	AC_TRY_LINK([#include <pthread.h>],
			[pthread_create(0, 0, 0, 0);],
        		COMMONLIBS="-pthread $COMMONLIBS"
        		have_pthread=yes)
		LIBS="$SAVELIBS"
        	AC_MSG_RESULT([$have_pthread])
	fi
fi


dnl
dnl End of "$Id$".
dnl
