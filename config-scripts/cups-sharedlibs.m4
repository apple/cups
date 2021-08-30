dnl
dnl Shared library support for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2018 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

PICFLAG="1"
DSOFLAGS="${DSOFLAGS:=}"

AC_ARG_ENABLE([shared], AS_HELP_STRING([--disable-shared], [do not create shared libraries]))

cupsbase="cups"
LIBCUPSBASE="lib$cupsbase"
LIBCUPSIMAGE=""
LIBCUPSSTATIC="lib$cupsbase.a"

AS_IF([test x$enable_shared != xno], [
    AS_CASE(["$host_os_name"], [sunos*], [
	LIBCUPS="lib$cupsbase.so.2"
	AS_IF([test "x$cupsimagebase" != x], [
	    LIBCUPSIMAGE="lib$cupsimagebase.so.2"
	])
	DSO="\$(CC)"
	DSOXX="\$(CXX)"
	DSOFLAGS="$DSOFLAGS -Wl,-h\`basename \$@\` -G"
    ], [linux* | gnu* | *bsd*], [
	LIBCUPS="lib$cupsbase.so.2"
	AS_IF([test "x$cupsimagebase" != x], [
	    LIBCUPSIMAGE="lib$cupsimagebase.so.2"
	])
	DSO="\$(CC)"
	DSOXX="\$(CXX)"
	DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared"
    ], [darwin*], [
	LIBCUPS="lib$cupsbase.2.dylib"
	AS_IF([test "x$cupsimagebase" != x], [
	    LIBCUPSIMAGE="lib$cupsimagebase.2.dylib"
	])
	DSO="\$(CC)"
	DSOXX="\$(CXX)"
	DSOFLAGS="$DSOFLAGS -Wl,-no_warn_inits -dynamiclib -single_module -lc"
    ], [*], [
	AC_MSG_NOTICE([Warning: Shared libraries may not work, trying -shared option.])
	LIBCUPS="lib$cupsbase.so.2"
	AS_IF([test "x$cupsimagebase" != x], [
	    LIBCUPSIMAGE="lib$cupsimagebase.so.2"
	])
	DSO="\$(CC)"
	DSOXX="\$(CXX)"
	DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared"
    ])
], [
    PICFLAG=0
    LIBCUPS="lib$cupsbase.a"
    AS_IF([test "x$cupsimagebase" != x], [
	LIBCUPSIMAGE="lib$cupsimagebase.a"
    ])
    DSO=":"
    DSOXX=":"
])

AC_SUBST([DSO])
AC_SUBST([DSOXX])
AC_SUBST([DSOFLAGS])
AC_SUBST([LIBCUPS])
AC_SUBST([LIBCUPSBASE])
AC_SUBST([LIBCUPSIMAGE])
AC_SUBST([LIBCUPSSTATIC])

AS_IF([test x$enable_shared = xno], [
    LINKCUPS="../cups/lib$cupsbase.a \$(LIBS)"
    EXTLINKCUPS="-lcups \$LIBS"
], [
    LINKCUPS="-L../cups -l${cupsbase}"
    EXTLINKCUPS="-lcups"
])

AC_SUBST([EXTLINKCUPS])
AC_SUBST([LINKCUPS])

dnl Update libraries for DSOs...
EXPORT_LDFLAGS=""

AS_IF([test "$DSO" != ":"], [
    # Tell the run-time linkers where to find a DSO.  Some platforms
    # need this option, even when the library is installed in a
    # standard location...
    AS_CASE([$host_os_name], [sunos*], [
	# Solaris...
	AS_IF([test $exec_prefix != /usr], [
	    DSOFLAGS="-R$libdir $DSOFLAGS"
	    LDFLAGS="$LDFLAGS -R$libdir"
	    EXPORT_LDFLAGS="-R$libdir"
	])
    ], [*bsd*], [
	# *BSD...
	AS_IF([test $exec_prefix != /usr], [
	    DSOFLAGS="-Wl,-R$libdir $DSOFLAGS"
	    LDFLAGS="$LDFLAGS -Wl,-R$libdir"
	    EXPORT_LDFLAGS="-Wl,-R$libdir"
	])
    ], [linux* | gnu*], [
	# Linux, and HURD...
	AS_IF([test $exec_prefix != /usr], [
	    DSOFLAGS="-Wl,-rpath,$libdir $DSOFLAGS"
	    LDFLAGS="$LDFLAGS -Wl,-rpath,$libdir"
	    EXPORT_LDFLAGS="-Wl,-rpath,$libdir"
	])
    ])
])

AC_SUBST([EXPORT_LDFLAGS])
