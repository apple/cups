dnl
dnl "$Id: cups-openslp.m4,v 1.2.2.7 2004/02/26 16:59:02 mike Exp $"
dnl
dnl   OpenSLP configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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

AC_ARG_ENABLE(slp, [  --enable-slp            turn on SLP support, default=yes])
AC_ARG_WITH(openslp-libs, [  --with-openslp-libs     set directory for OpenSLP library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(openslp-includes, [  --with-openslp-includes set directory for OpenSLP includes],
    CFLAGS="-I$withval $CFLAGS"
    CXXFLAGS="-I$withval $CXXFLAGS"
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
dnl End of "$Id: cups-openslp.m4,v 1.2.2.7 2004/02/26 16:59:02 mike Exp $".
dnl
