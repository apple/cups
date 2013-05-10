dnl
dnl "$Id: cups-sharedlibs.m4 9153 2010-06-16 00:48:25Z mike $"
dnl
dnl   Shared library support for CUPS.
dnl
dnl   Copyright 2007-2010 by Apple Inc.
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

PICFLAG=1
DSOFLAGS="${DSOFLAGS:=}"

AC_ARG_ENABLE(shared, [  --disable-shared        do not create shared libraries])

cupsbase="cups"
LIBCUPSBASE="lib$cupsbase"
LIBCUPSSTATIC="lib$cupsbase.a"

if test x$enable_shared != xno; then
	case "$uname" in
		SunOS*)
			LIBCUPS="lib$cupsbase.so.2"
			LIBCUPSCGI="libcupscgi.so.1"
			LIBCUPSDRIVER="libcupsdriver.so.1"
			LIBCUPSIMAGE="libcupsimage.so.2"
			LIBCUPSMIME="libcupsmime.so.1"
			LIBCUPSPPDC="libcupsppdc.so.1"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-h\`basename \$@\` -G \$(OPTIM)"
			;;
		UNIX_S*)
			LIBCUPS="lib$cupsbase.so.2"
			LIBCUPSCGI="libcupscgi.so.1"
			LIBCUPSDRIVER="libcupsdriver.so.1"
			LIBCUPSIMAGE="libcupsimage.so.2"
			LIBCUPSMIME="libcupsmime.so.1"
			LIBCUPSPPDC="libcupsppdc.so.1"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-h,\`basename \$@\` -G \$(OPTIM)"
			;;
		HP-UX*)
			case "$uarch" in
				ia64)
					LIBCUPS="lib$cupsbase.so.2"
					LIBCUPSCGI="libcupscgi.so.1"
					LIBCUPSDRIVER="libcupsdriver.so.1"
					LIBCUPSIMAGE="libcupsimage.so.2"
					LIBCUPSMIME="libcupsmime.so.1"
					LIBCUPSPPDC="libcupsppdc.so.1"
					DSO="\$(CC)"
					DSOXX="\$(CXX)"
					DSOFLAGS="$DSOFLAGS -Wl,-b,-z,+h,\`basename \$@\`"
					;;
				*)
					LIBCUPS="lib$cupsbase.sl.2"
					LIBCUPSCGI="libcupscgi.sl.1"
					LIBCUPSDRIVER="libcupsdriver.sl.1"
					LIBCUPSIMAGE="libcupsimage.sl.2"
					LIBCUPSMIME="libcupsmime.sl.1"
					LIBCUPSPPDC="libcupsppdc.sl.1"
					DSO="\$(LD)"
					DSOXX="\$(LD)"
					DSOFLAGS="$DSOFLAGS -b -z +h \`basename \$@\`"
					;;
			esac
			;;
		IRIX)
			LIBCUPS="lib$cupsbase.so.2"
			LIBCUPSCGI="libcupscgi.so.1"
			LIBCUPSDRIVER="libcupsdriver.so.1"
			LIBCUPSIMAGE="libcupsimage.so.2"
			LIBCUPSMIME="libcupsmime.so.1"
			LIBCUPSPPDC="libcupsppdc.so.1"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -set_version,sgi2.6,-soname,\`basename \$@\` -shared \$(OPTIM)"
			;;
		OSF1* | Linux | GNU | *BSD*)
			LIBCUPS="lib$cupsbase.so.2"
			LIBCUPSCGI="libcupscgi.so.1"
			LIBCUPSDRIVER="libcupsdriver.so.1"
			LIBCUPSIMAGE="libcupsimage.so.2"
			LIBCUPSMIME="libcupsmime.so.1"
			LIBCUPSPPDC="libcupsppdc.so.1"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared \$(OPTIM)"
			;;
		Darwin*)
			LIBCUPS="lib$cupsbase.2.dylib"
			LIBCUPSCGI="libcupscgi.1.dylib"
			LIBCUPSDRIVER="libcupsdriver.1.dylib"
			LIBCUPSIMAGE="libcupsimage.2.dylib"
			LIBCUPSMIME="libcupsmime.1.dylib"
			LIBCUPSPPDC="libcupsppdc.1.dylib"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -dynamiclib -single_module -lc"
			;;
		AIX*)
			LIBCUPS="lib${cupsbase}_s.a"
			LIBCUPSBASE="${cupsbase}_s"
			LIBCUPSCGI="libcupscgi_s.a"
			LIBCUPSDRIVER="libcupsdriver_s.a"
			LIBCUPSIMAGE="libcupsimage_s.a"
			LIBCUPSMIME="libcupsmime_s.a"
			LIBCUPSPPDC="libcupsppdc_s.a"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-bexpall,-bM:SRE,-bnoentry,-blibpath:\$(libdir)"
			;;
		*)
			echo "Warning: shared libraries may not be supported.  Trying -shared"
			echo "         option with compiler."
			LIBCUPS="lib$cupsbase.so.2"
			LIBCUPSCGI="libcupscgi.so.1"
			LIBCUPSDRIVER="libcupsdriver.so.1"
			LIBCUPSIMAGE="libcupsimage.so.2"
			LIBCUPSMIME="libcupsmime.so.1"
			LIBCUPSPPDC="libcupsppdc.so.1"
			DSO="\$(CC)"
			DSOXX="\$(CXX)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\`basename \$@\` -shared \$(OPTIM)"
			;;
	esac
else
	PICFLAG=0
	LIBCUPS="lib$cupsbase.a"
	LIBCUPSCGI="libcupscgi.a"
	LIBCUPSDRIVER="libcupsdriver.a"
	LIBCUPSIMAGE="libcupsimage.a"
	LIBCUPSMIME="libcupsmime.a"
	LIBCUPSPPDC="libcupsppdc.a"
	DSO=":"
	DSOXX=":"
fi

# 32-bit and 64-bit libraries need variations of the standard
# DSOFLAGS...
DSO32FLAGS="$DSOFLAGS"
DSO64FLAGS="$DSOFLAGS"

AC_SUBST(DSO)
AC_SUBST(DSOXX)
AC_SUBST(DSOFLAGS)
AC_SUBST(DSO32FLAGS)
AC_SUBST(DSO64FLAGS)
AC_SUBST(LIBCUPS)
AC_SUBST(LIBCUPSBASE)
AC_SUBST(LIBCUPSCGI)
AC_SUBST(LIBCUPSDRIVER)
AC_SUBST(LIBCUPSIMAGE)
AC_SUBST(LIBCUPSMIME)
AC_SUBST(LIBCUPSPPDC)
AC_SUBST(LIBCUPSSTATIC)

if test x$enable_shared = xno; then
	LINKCUPS="../cups/lib$cupsbase.a"
	LINKCUPSIMAGE="../filter/libcupsimage.a"

	EXTLINKCUPS="-lcups"
	EXTLINKCUPSDRIVER="-lcupsdriver"
	EXTLINKCUPSIMAGE="-lcupsimage"
else
	if test $uname = AIX; then
		LINKCUPS="-l${cupsbase}_s"
		LINKCUPSIMAGE="-lcupsimage_s"

		EXTLINKCUPS="-lcups_s"
		EXTLINKCUPSDRIVER="-lcupsdriver_s"
		EXTLINKCUPSIMAGE="-lcupsimage_s"
	else
		LINKCUPS="-l${cupsbase}"
		LINKCUPSIMAGE="-lcupsimage"

		EXTLINKCUPS="-lcups"
		EXTLINKCUPSDRIVER="-lcupsdriver"
		EXTLINKCUPSIMAGE="-lcupsimage"
	fi
fi

AC_SUBST(EXTLINKCUPS)
AC_SUBST(EXTLINKCUPSDRIVER)
AC_SUBST(EXTLINKCUPSIMAGE)
AC_SUBST(LINKCUPS)
AC_SUBST(LINKCUPSIMAGE)

dnl Update libraries for DSOs...
EXPORT_LDFLAGS=""

if test "$DSO" != ":"; then
	# When using DSOs the image libraries are linked to libcupsimage.so
	# rather than to the executables.  This makes things smaller if you
	# are using any static libraries, and it also allows us to distribute
	# a single DSO rather than a bunch...
	DSOLIBS="\$(LIBTIFF) \$(LIBPNG) \$(LIBJPEG) \$(LIBZ)"
	IMGLIBS=""

	# Tell the run-time linkers where to find a DSO.  Some platforms
	# need this option, even when the library is installed in a
	# standard location...
	case $uname in
                HP-UX*)
			# HP-UX needs the path, even for /usr/lib...
			case "$uarch" in
				ia64)
					DSOFLAGS="-Wl,+s,+b,$libdir $DSOFLAGS"
					DSO32FLAGS="-Wl,+s,+b,$LIB32DIR $DSO32FLAGS"
					DSO64FLAGS="-Wl,+s,+b,$LIB64DIR $DSO64FLAGS"
					;;
				*)
                			DSOFLAGS="+s +b $libdir $DSOFLAGS"
                			DSO32FLAGS="+s +b $LIB32DIR $DSO32FLAGS"
                			DSO64FLAGS="+s +b $LIB64DIR $DSO64FLAGS"
					;;
			esac
                	LDFLAGS="$LDFLAGS -Wl,+s,+b,$libdir"
                	EXPORT_LDFLAGS="-Wl,+s,+b,$libdir"
			;;
                SunOS*)
                	# Solaris...
			if test $exec_prefix != /usr; then
				DSOFLAGS="-R$libdir $DSOFLAGS"
				DSO32FLAGS="-R$LIB32DIR $DSO32FLAGS"
				DSO64FLAGS="-R$LIB64DIR $DSO64FLAGS"
				LDFLAGS="$LDFLAGS -R$libdir"
				EXPORT_LDFLAGS="-R$libdir"
			fi
			;;
                *BSD*)
                        # *BSD...
			if test $exec_prefix != /usr; then
				DSOFLAGS="-Wl,-R$libdir $DSOFLAGS"
				DSO32FLAGS="-Wl,-R$LIB32DIR $DSO32FLAGS"
				DSO64FLAGS="-Wl,-R$LIB64DIR $DSO64FLAGS"
				LDFLAGS="$LDFLAGS -Wl,-R$libdir"
				EXPORT_LDFLAGS="-Wl,-R$libdir"
			fi
			;;
                IRIX | Linux | GNU)
                        # IRIX, Linux, and HURD...
			if test $exec_prefix != /usr; then
				DSOFLAGS="-Wl,-rpath,$libdir $DSOFLAGS"
				DSO32FLAGS="-Wl,-rpath,$LIB32DIR $DSO32FLAGS"
				DSO64FLAGS="-Wl,-rpath,$LIB64DIR $DSO64FLAGS"
				LDFLAGS="$LDFLAGS -Wl,-rpath,$libdir"
				EXPORT_LDFLAGS="-Wl,-rpath,$libdir"
			fi
			;;
	esac
else
	DSOLIBS=""
	IMGLIBS="\$(LIBTIFF) \$(LIBPNG) \$(LIBJPEG) \$(LIBZ)"
fi

AC_SUBST(DSOLIBS)
AC_SUBST(IMGLIBS)
AC_SUBST(EXPORT_LDFLAGS)

dnl
dnl End of "$Id: cups-sharedlibs.m4 9153 2010-06-16 00:48:25Z mike $".
dnl
