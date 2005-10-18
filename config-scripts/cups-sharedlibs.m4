dnl
dnl "$Id$"
dnl
dnl   Shared library support for the Common UNIX Printing System (CUPS).
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

PICFLAG=1
DSOFLAGS="${DSOFLAGS:=}"

AC_ARG_ENABLE(shared, [  --enable-shared         turn on shared libraries, default=yes])

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
		IRIX)
			LIBCUPS="libcups.so.2"
			LIBCUPSIMAGE="libcupsimage.so.2"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-rpath,\$(libdir),-set_version,sgi2.6,-soname,\$@ -shared \$(OPTIM)"
			;;
		OSF1* | Linux | GNU | *BSD*)
			LIBCUPS="libcups.so.2"
			LIBCUPSIMAGE="libcupsimage.so.2"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-soname,\$@ -shared \$(OPTIM)"
			;;
		Darwin*)
			LIBCUPS="libcups.2.dylib"
			LIBCUPSIMAGE="libcupsimage.2.dylib"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS \$(RC_CFLAGS) -dynamiclib -lc"
			;;
		AIX*)
			LIBCUPS="libcups_s.a"
			LIBCUPSIMAGE="libcupsimage_s.a"
			DSO="\$(CC)"
			DSOFLAGS="$DSOFLAGS -Wl,-bexpall,-bM:SRE,-bnoentry,-blibpath:\$(libdir)"
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
	LINKCUPS="../cups/libcups.a"
	LINKCUPSIMAGE="../filter/libcupsimage.a"
else
	if test $uname = AIX; then
		LINKCUPS="-lcups_s"
		LINKCUPSIMAGE="-lcupsimage_s"
	else
		LINKCUPS="-lcups"
		LINKCUPSIMAGE="-lcupsimage"
	fi
fi

AC_SUBST(LINKCUPS)
AC_SUBST(LINKCUPSIMAGE)

dnl Update libraries for DSOs...
EXPORT_LDFLAGS=""

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
                	DSOFLAGS="+s +b $libdir $DSOFLAGS"
                	LDFLAGS="$LDFLAGS -Wl,+s,+b,$libdir"
                	EXPORT_LDFLAGS="-Wl,+s,+b,$libdir"
                	;;
                SunOS*)
                	# Solaris
                	DSOFLAGS="-R$libdir $DSOFLAGS"
                	LDFLAGS="$LDFLAGS -R$libdir"
                	EXPORT_LDFLAGS="-R$libdir"
                	;;
                *BSD*)
                        # *BSD
                	DSOFLAGS="-Wl,-R$libdir $DSOFLAGS"
                        LDFLAGS="$LDFLAGS -Wl,-R$libdir"
                        EXPORT_LDFLAGS="-Wl,-R$libdir"
                        ;;
                Linux | GNU)
                        # Linux and HURD
                	DSOFLAGS="-Wl,-rpath,$libdir $DSOFLAGS"
                        LDFLAGS="$LDFLAGS -Wl,-rpath,$libdir"
                        EXPORT_LDFLAGS="-Wl,-rpath,$libdir"
                        ;;
	esac
else
	DSOLIBS=""
	IMGLIBS="\$(LIBPNG) \$(LIBTIFF) \$(LIBJPEG) \$(LIBZ)"
fi

AC_SUBST(DSOLIBS)
AC_SUBST(IMGLIBS)
AC_SUBST(EXPORT_LDFLAGS)

dnl
dnl End of "$Id$".
dnl
