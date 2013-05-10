dnl
dnl "$Id: cups-slp.m4 9771 2011-05-12 05:21:56Z mike $"
dnl
dnl   OpenSLP configuration stuff for CUPS.
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

AC_ARG_ENABLE(slp, [  --disable-slp           disable SLP support])
AC_ARG_WITH(openslp-libs, [  --with-openslp-libs     set directory for OpenSLP library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(openslp-includes, [  --with-openslp-includes set directory for OpenSLP includes],
    CFLAGS="-I$withval $CFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

LIBSLP=""

if test x$enable_slp != xno; then
    AC_CHECK_HEADER(slp.h,
	AC_CHECK_LIB(slp, SLPOpen,
            AC_DEFINE(HAVE_LIBSLP)
	    LIBSLP="-lslp"))
fi

AC_SUBST(LIBSLP)


dnl
dnl End of "$Id: cups-slp.m4 9771 2011-05-12 05:21:56Z mike $".
dnl
