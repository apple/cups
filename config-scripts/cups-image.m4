dnl
dnl "$Id: cups-image.m4,v 1.2.2.4 2004/02/26 16:59:02 mike Exp $"
dnl
dnl   Image library stuff for the Common UNIX Printing System (CUPS).
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

dnl Save the current libraries since we don't want the image libraries
dnl included with every program...
SAVELIBS="$LIBS"

dnl Check for image libraries...
LIBJPEG=""
LIBPNG=""
LIBTIFF=""
LIBZ=""

AC_SUBST(LIBJPEG)
AC_SUBST(LIBPNG)
AC_SUBST(LIBTIFF)
AC_SUBST(LIBZ)

AC_CHECK_HEADER(jpeglib.h,
    AC_CHECK_LIB(jpeg, jpeg_destroy_decompress,
	AC_DEFINE(HAVE_LIBJPEG)
	LIBJPEG="-ljpeg"
	LIBS="$LIBS -ljpeg"))

AC_CHECK_HEADER(zlib.h,
    AC_CHECK_LIB(z, gzgets,
	AC_DEFINE(HAVE_LIBZ)
	LIBZ="-lz"
	LIBS="$LIBS -lz"))

dnl PNG library uses math library functions...
AC_CHECK_LIB(m, pow)

AC_CHECK_HEADER(png.h,
    AC_CHECK_LIB(png, png_set_tRNS_to_alpha,
	AC_DEFINE(HAVE_LIBPNG)
	LIBPNG="-lpng -lm"))

AC_CHECK_HEADER(tiff.h,
    AC_CHECK_LIB(tiff, TIFFReadScanline,
	AC_DEFINE(HAVE_LIBTIFF)
	LIBTIFF="-ltiff"))

dnl Restore original LIBS settings...
LIBS="$SAVELIBS"

EXPORT_LIBJPEG="$LIBJPEG"
EXPORT_LIBPNG="$LIBPNG"
EXPORT_LIBTIFF="$LIBTIFF"
EXPORT_LIBZ="$LIBZ"

AC_SUBST(EXPORT_LIBJPEG)
AC_SUBST(EXPORT_LIBPNG)
AC_SUBST(EXPORT_LIBTIFF)
AC_SUBST(EXPORT_LIBZ)

AC_CHECK_HEADER(stdlib.h,AC_DEFINE(HAVE_STDLIB_H))

dnl
dnl End of "$Id: cups-image.m4,v 1.2.2.4 2004/02/26 16:59:02 mike Exp $".
dnl
