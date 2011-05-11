dnl
dnl "$Id: cups-image.m4 6649 2007-07-11 21:46:42Z mike $"
dnl
dnl   Image library/filter stuff for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
dnl   Copyright 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl See if we want the image filters included at all...
AC_ARG_ENABLE(image, [  --enable-image          always build the image filters])

DEFAULT_IMAGEFILTERS="#"
IMGFILTERS=""
if test "x$enable_image" != xno; then
        AC_MSG_CHECKING(whether to build image filters)
        if test "x$enable_image" = xyes -o $uname != Darwin; then
		IMGFILTERS="imagetops imagetoraster"
		DEFAULT_IMAGEFILTERS=""
                AC_MSG_RESULT(yes)
        else
                AC_MSG_RESULT(no)
        fi
fi

AC_SUBST(DEFAULT_IMAGEFILTERS)
AC_SUBST(IMGFILTERS)

dnl Check for image libraries...
AC_ARG_ENABLE(jpeg, [  --disable-jpeg          disable JPEG support])
AC_ARG_ENABLE(png, [  --disable-png           disable PNG support])
AC_ARG_ENABLE(tiff, [  --disable-tiff          disable TIFF support])

LIBJPEG=""
LIBPNG=""
LIBTIFF=""
LIBZ=""

AC_SUBST(LIBJPEG)
AC_SUBST(LIBPNG)
AC_SUBST(LIBTIFF)
AC_SUBST(LIBZ)

dnl Image libraries use math library functions...
AC_SEARCH_LIBS(pow, m)

dnl Save the current libraries since we don't want the image libraries
dnl included with every program...
SAVELIBS="$LIBS"

dnl JPEG library...
if test x$enable_jpeg != xno; then
    AC_CHECK_HEADER(jpeglib.h,
	AC_CHECK_LIB(jpeg, jpeg_destroy_decompress,
	    AC_DEFINE(HAVE_LIBJPEG)
	    LIBJPEG="-ljpeg"
	    LIBS="$LIBS -ljpeg"))
else
    AC_MSG_NOTICE([JPEG support disabled with --disable-jpeg.])
fi

dnl ZLIB library...
AC_CHECK_HEADER(zlib.h,
    AC_CHECK_LIB(z, gzgets,
	AC_DEFINE(HAVE_LIBZ)
	LIBZ="-lz"
	LIBS="$LIBS -lz"))

dnl PNG library...
if test x$enable_png != xno; then
    AC_CHECK_HEADER(png.h,
	AC_CHECK_LIB(png, png_create_read_struct,
	    AC_DEFINE(HAVE_LIBPNG)
	    LIBPNG="-lpng"))
else
    AC_MSG_NOTICE([PNG support disabled with --disable-png.])
fi

dnl TIFF library...
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
dnl End of "$Id: cups-image.m4 6649 2007-07-11 21:46:42Z mike $".
dnl
