dnl
dnl "$Id$"
dnl
dnl   LDAP configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2003-2006 by Easy Software Products, all rights reserved.
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
dnl         WWW: http://www.cups.org/
dnl

AC_ARG_ENABLE(ldap, [  --enable-ldap           turn on LDAP support, default=yes])
AC_ARG_WITH(openldap-libs, [  --with-openldap-libs    set directory for OpenLDAP library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(openldap-includes, [  --with-openldap-includes
                          set directory for OpenLDAP includes],
    CFLAGS="-I$withval $CFLAGS"
    CXXFLAGS="-I$withval $CXXFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

LIBLDAP=""

if test x$enable_ldap != xno; then
    AC_CHECK_HEADER(ldap.h,
	AC_CHECK_LIB(ldap, ldap_initialize,
            AC_DEFINE(HAVE_LDAP)
            AC_DEFINE(HAVE_OPENLDAP)
	    LIBLDAP="-lldap"))
fi

AC_SUBST(LIBLDAP)


dnl
dnl End of "$Id$".
dnl
