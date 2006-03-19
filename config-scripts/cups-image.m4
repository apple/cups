dnl
dnl "$Id$"
dnl
dnl   Image library/filter stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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

dnl See if we want the image filters included at all...
AC_ARG_ENABLE(image, [  --enable-image          turn on image filters, default=auto])

IMGFILTERS=""
if test "x$enable_image" != xno; then
        AC_MSG_CHECKING(whether to build image filters)
        if test "x$enable_image" = xyes -o $uname != Darwin; then
		IMGFILTERS="imagetops imagetoraster"
                AC_MSG_RESULT(yes)
        else
                AC_MSG_RESULT(no)
        fi
fi

AC_SUBST(IMGFILTERS)

dnl Save the current libraries since we don't want the image libraries
dnl included with every program...
SAVELIBS="$LIBS"

dnl Check for image libraries...
AC_ARG_ENABLE(jpeg, [  --enable-jpeg           turn on JPEG support, default=yes])
AC_ARG_ENABLE(png, [  --enable-png            turn on PNG support, default=yes])
AC_ARG_ENABLE(tiff, [  --enable-tiff           turn on TIFF support, default=yes])

LIBJPEG=""
LIBPNG=""
LIBTIFF=""
LIBZ=""

AC_SUBST(LIBJPEG)
AC_SUBST(LIBPNG)
AC_SUBST(LIBTIFF)
AC_SUBST(LIBZ)

if test x$enable_jpeg != xno; then
    AC_CHECK_HEADER(jpeglib.h,
	AC_CHECK_LIB(jpeg, jpeg_destroy_decompress,
	    AC_DEFINE(HAVE_LIBJPEG)
	    LIBJPEG="-ljpeg"
	    LIBS="$LIBS -ljpeg"))
else
    AC_MSG_NOTICE([JPEG support disabled with --disable-jpeg.])
fi

AC_CHECK_HEADER(zlib.h,
    AC_CHECK_LIB(z, gzgets,
	AC_DEFINE(HAVE_LIBZ)
	LIBZ="-lz"
	LIBS="$LIBS -lz"))

dnl PNG library uses math library functions...
AC_CHECK_LIB(m, pow)

if test x$enable_png != xno; then
    AC_CHECK_HEADER(png.h,
	AC_CHECK_LIB(png, png_set_tRNS_to_alpha,
	    AC_DEFINE(HAVE_LIBPNG)
	    LIBPNG="-lpng -lm"))
else
    AC_MSG_NOTICE([PNG support disabled with --disable-png.])
fi

if test x$enable_tiff != xno; then
    AC_CHECK_HEADER(tiff.h,
	AC_CHECK_LIB(tiff, TIFFReadScanline,
	AC_DEFINE(HAVE_LIBTIFF)
	LIBTIFF="-ltiff"))
else
    AC_MSG_NOTICE([TIFF support disabled with --disable-tiff.])
fi

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
dnl End of "$Id$".
dnl
