dnl
dnl "$Id: cups-ldap.m4 9771 2011-05-12 05:21:56Z mike $"
dnl
dnl   LDAP configuration stuff for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
dnl   Copyright 2003-2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(ldap, [  --disable-ldap          disable LDAP support])
AC_ARG_WITH(ldap-libs, [  --with-ldap-libs        set directory for LDAP library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(ldap-includes, [  --with-ldap-includes    set directory for LDAP includes],
    CFLAGS="-I$withval $CFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

LIBLDAP=""

if test x$enable_ldap != xno; then
    AC_CHECK_HEADER(ldap.h, [
	AC_CHECK_LIB(ldap, ldap_initialize,
	    AC_DEFINE(HAVE_LDAP)
	    AC_DEFINE(HAVE_OPENLDAP)
	    LIBLDAP="-lldap"
	    AC_CHECK_LIB(ldap, ldap_start_tls,
		AC_DEFINE(HAVE_LDAP_SSL)),

	    AC_CHECK_LIB(ldap, ldap_init,
		AC_DEFINE(HAVE_LDAP)
		AC_DEFINE(HAVE_MOZILLA_LDAP)
		LIBLDAP="-lldap"
		AC_CHECK_HEADER(ldap_ssl.h, AC_DEFINE(HAVE_LDAP_SSL_H),,[#include <ldap.h>])
		AC_CHECK_LIB(ldap, ldapssl_init,
		    AC_DEFINE(HAVE_LDAP_SSL)))
	)
	AC_CHECK_LIB(ldap, ldap_set_rebind_proc, AC_DEFINE(HAVE_LDAP_REBIND_PROC))
    ])
fi

AC_SUBST(LIBLDAP)


dnl
dnl End of "$Id: cups-ldap.m4 9771 2011-05-12 05:21:56Z mike $".
dnl
