dnl
dnl "$Id: cups-sharedlibs.m4,v 1.6.2.9 2002/05/09 02:22:05 mike Exp $"
dnl
dnl   Shared library support for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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

PICFLAG=1
DSOFLAGS="${DSOFLAGS:=}"

AC_ARG_ENABLE(shared, [  --enable-shared         turn on shared libraries [default=yes]])

if test x$enable_shared != xno; then
	case "$uname" in
		SunOS* | UNIX_S*)
			LIBCUPS="libcups.so.2"
			LIBCUPSIMAGE="libcupsimage.so.2"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-h,\$@ -G \$(OPTIM)"
			;;
		HP-UX*)
			LIBCUPS="libcups.sl.2"
			LIBCUPSIMAGE="libcupsimage.sl.2"
			DSO="ld"
			DSOFLAGS="$DSOFLAGS -b -z +h \$@"
			;;
		IRIX*)
			LIBCUPS="libcups.so.2"
			LIBCUPSIMAGE="libcupsimage.so.2"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-rpath,\$(libdir),-set_version,sgi3.0,-soname,\$@ -shared \$(OPTIM)"
			;;
		OSF1* | Linux* | *BSD*)
			LIBCUPS="libcups.so.2"
			LIBCUPSIMAGE="libcupsimage.so.2"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\$@ -shared \$(OPTIM)"
			;;
		Darwin*)
			LIBCUPS="libcups.2.dylib"
			LIBCUPSIMAGE="libcupsimage.2.dylib"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS \$(RC_FLAGS) -dynamiclib -lc"
			;;
		AIX*)
			LIBCUPS="libcups_s.a"
			LIBCUPSIMAGE="libcupsimage_s.a"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-bexpall,-bM:SRE,-bnoentry"
			;;
		*)
			echo "Warning: shared libraries may not be supported.  Trying -shared"
			echo "         option with compiler."
			LIBCUPS="libcups.so.2"
			LIBCUPSIMAGE="libcupsimage.so.2"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\$@ -shared \$(OPTIM)"
			;;
	esac
else
	PICFLAG=0
	LIBCUPS="libcups.a"
	LIBCUPSIMAGE="libcupsimage.a"
	DSO=":"
fi

AC_SUBST(DSO)
AC_SUBST(DSOFLAGS)
AC_SUBST(LIBCUPS)
AC_SUBST(LIBCUPSIMAGE)

if test x$enable_shared = xno; then
	LINKCUPS="-L../cups -lcups \$(SSLLIBS)"
	LINKCUPSIMAGE="-L../filter -lcupsimage"
else
	if test $uname = AIX; then
		LINKCUPS="-L../cups -lcups_s"
		LINKCUPSIMAGE="-L../filter -lcupsimage_s"
	else
		LINKCUPS="-L../cups -lcups"
		LINKCUPSIMAGE="-L../filter -lcupsimage"
	fi
fi

AC_SUBST(LINKCUPS)
AC_SUBST(LINKCUPSIMAGE)

dnl Update libraries for DSOs...
if test "$DSO" != ":"; then
	# When using DSOs the image libraries are linked to libcupsimage.so
	# rather than to the executables.  This makes things smaller if you
	# are using any static libraries, and it also allows us to distribute
	# a single DSO rather than a bunch...
	DSOLIBS="\$(LIBPNG) \$(LIBTIFF) \$(LIBJPEG) \$(LIBZ)"
	IMGLIBS=""

	# The *BSD, HP-UX, and Solaris run-time linkers need help when
	# deciding where to find a DSO.  Add linker options to tell them
	# where to find the DSO (usually in /usr/lib...  duh!)
	case $uname in
                HP-UX*)
			# HP-UX
                	DSOFLAGS="+b $libdir +fb $DSOFLAGS"
                	LDFLAGS="$LDFLAGS -Wl,+b,$libdir,+fb"
                	;;
                SunOS*)
                	# Solaris
                	DSOFLAGS="-R$libdir $DSOFLAGS"
                	LDFLAGS="$LDFLAGS -R$libdir"
                	;;
                *BSD*)
                        # *BSD
                	DSOFLAGS="-Wl,-R$libdir $DSOFLAGS"
                        LDFLAGS="$LDFLAGS -Wl,-R$libdir"
                        ;;
                Linux*)
                        # Linux
                	DSOFLAGS="-Wl,-rpath,$libdir $DSOFLAGS"
                        LDFLAGS="$LDFLAGS -Wl,-rpath,$libdir"
                        ;;
	esac
else
	DSOLIBS=""
	IMGLIBS="\$(LIBPNG) \$(LIBTIFF) \$(LIBJPEG) \$(LIBZ)"
fi

AC_SUBST(DSOLIBS)
AC_SUBST(IMGLIBS)

dnl
dnl End of "$Id: cups-sharedlibs.m4,v 1.6.2.9 2002/05/09 02:22:05 mike Exp $".
dnl
