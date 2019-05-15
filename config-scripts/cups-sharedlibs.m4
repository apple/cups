dnl
dnl Shared library support for CUPS.
dnl
dnl Copyright © 2007-2018 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

PICFLAG=1
DSOFLAGS="${DSOFLAGS:=}"

AC_ARG_ENABLE(shared, [  --disable-shared        do not create shared libraries])

cupsbase="cups"
LIBCUPSBASE="lib$cupsbase"
LIBCUPSIMAGE=""
LIBCUPSSTATIC="lib$cupsbase.a"

if test x$enable_shared != xno; then
	case "$host_os_name" in
		sunos*)
			LIBCUPS="lib$cupsbase.so.2"
			if test "x$cupsimagebase" != x; then
				LIBCUPSIMAGE="lib$cupsimagebase.so.2"
			fi
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-h\`basename \$@\` -G"
			;;
		linux* | gnu* | *bsd*)
			LIBCUPS="lib$cupsbase.so.2"
			if test "x$cupsimagebase" != x; then
				LIBCUPSIMAGE="lib$cupsimagebase.so.2"
			fi
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared"
			;;
		darwin*)
			LIBCUPS="lib$cupsbase.2.dylib"
			if test "x$cupsimagebase" != x; then
				LIBCUPSIMAGE="lib$cupsimagebase.2.dylib"
			fi
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-no_warn_inits -dynamiclib -single_module -lc"
			;;
		*)
			echo "Warning: shared libraries may not be supported.  Trying -shared"
			echo "         option with compiler."
			LIBCUPS="lib$cupsbase.so.2"
			if test "x$cupsimagebase" != x; then
				LIBCUPSIMAGE="lib$cupsimagebase.so.2"
			fi
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared"
			;;
	esac
else
	PICFLAG=0
	LIBCUPS="lib$cupsbase.a"
	if test "x$cupsimagebase" != x; then
		LIBCUPSIMAGE="lib$cupsimagebase.a"
	fi
	DSO=":"
	DSOXX=":"
fi

AC_SUBST(DSO)
AC_SUBST(DSOXX)
AC_SUBST(DSOFLAGS)
AC_SUBST(LIBCUPS)
AC_SUBST(LIBCUPSBASE)
AC_SUBST(LIBCUPSIMAGE)
AC_SUBST(LIBCUPSSTATIC)

if test x$enable_shared = xno; then
	LINKCUPS="../cups/lib$cupsbase.a \$(LIBS)"
	EXTLINKCUPS="-lcups \$LIBS"
else
	LINKCUPS="-L../cups -l${cupsbase}"
	EXTLINKCUPS="-lcups"
fi

AC_SUBST(EXTLINKCUPS)
AC_SUBST(LINKCUPS)

dnl Update libraries for DSOs...
EXPORT_LDFLAGS=""

if test "$DSO" != ":"; then
	# Tell the run-time linkers where to find a DSO.  Some platforms
	# need this option, even when the library is installed in a
	# standard location...
	case $host_os_name in
                sunos*)
                	# Solaris...
			if test $exec_prefix != /usr; then
				DSOFLAGS="-R$libdir $DSOFLAGS"
				LDFLAGS="$LDFLAGS -R$libdir"
				EXPORT_LDFLAGS="-R$libdir"
			fi
			;;
                *bsd*)
                        # *BSD...
			if test $exec_prefix != /usr; then
				DSOFLAGS="-Wl,-R$libdir $DSOFLAGS"
				LDFLAGS="$LDFLAGS -Wl,-R$libdir"
				EXPORT_LDFLAGS="-Wl,-R$libdir"
			fi
			;;
                linux* | gnu*)
                        # Linux, and HURD...
			if test $exec_prefix != /usr; then
				DSOFLAGS="-Wl,-rpath,$libdir $DSOFLAGS"
				LDFLAGS="$LDFLAGS -Wl,-rpath,$libdir"
				EXPORT_LDFLAGS="-Wl,-rpath,$libdir"
			fi
			;;
	esac
fi

AC_SUBST(EXPORT_LDFLAGS)
