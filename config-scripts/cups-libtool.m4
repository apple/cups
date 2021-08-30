dnl
dnl Libtool stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2018 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

AC_ARG_ENABLE([libtool_unsupported], AS_HELP_STRING([--enable-libtool-unsupported=/path/to/libtool], [build with libtool (UNSUPPORTED)]), [
    AS_IF([test x$enable_libtool_unsupported != xno], [
	AS_IF([test x$enable_libtool_unsupported = xyes], [
	    AC_MSG_ERROR([Use --enable-libtool-unsupported=/path/to/libtool.])
	])
	LIBTOOL="$enable_libtool_unsupported"
	enable_shared="no"
	AC_MSG_WARN([WARNING: libtool is not supported.])
    ], [
	LIBTOOL=""
    ])
])

AS_IF([test x$LIBTOOL != x], [
    DSO="\$(LIBTOOL) --mode=link --tag=CC ${CC}"
    DSOXX="\$(LIBTOOL) --mode=link --tag=CXX ${CXX}"

    LD_CC="\$(LIBTOOL) --mode=link --tag=CC ${CC}"
    LD_CXX="\$(LIBTOOL) --mode=link --tag=CXX ${CXX}"

    LIBCUPS="libcups.la"
    LIBCUPSSTATIC="libcups.la"
    LIBCUPSCGI="libcupscgi.la"
    LIBCUPSIMAGE="libcupsimage.la"
    LIBCUPSMIME="libcupsmime.la"
    LIBCUPSPPDC="libcupsppdc.la"

    LIBTOOL_CC="\$(LIBTOOL) --mode=compile --tag=CC"
    LIBTOOL_CXX="\$(LIBTOOL) --mode=compile --tag=CXX"
    LIBTOOL_INSTALL="\$(LIBTOOL) --mode=install"

    LINKCUPS="../cups/\$(LIBCUPS)"
    LINKCUPSIMAGE="../cups/\$(LIBCUPSIMAGE)"
], [
    LD_CC="\$(CC)"
    LD_CXX="\$(CXX)"

    LIBTOOL_CC=""
    LIBTOOL_CXX=""
    LIBTOOL_INSTALL=""
])

AC_SUBST([LD_CC])
AC_SUBST([LD_CXX])

AC_SUBST([LIBTOOL])
AC_SUBST([LIBTOOL_CC])
AC_SUBST([LIBTOOL_CXX])
AC_SUBST([LIBTOOL_INSTALL])
