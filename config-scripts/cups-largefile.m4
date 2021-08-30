dnl
dnl Large file support stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2011 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Check for largefile support...
AC_SYS_LARGEFILE

dnl Define largefile options as needed...
LARGEFILE=""
AS_IF([test x$enable_largefile != xno], [
    LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"

    AS_IF([test x$ac_cv_sys_large_files = x1], [
	LARGEFILE="$LARGEFILE -D_LARGE_FILES"
    ])

    AS_IF([test x$ac_cv_sys_file_offset_bits = x64], [
	LARGEFILE="$LARGEFILE -D_FILE_OFFSET_BITS=64"
    ])
])
AC_SUBST([LARGEFILE])

dnl Check for "long long" support...
AC_CACHE_CHECK([for long long int], [ac_cv_c_long_long], [
    AS_IF([test "$GCC" = yes], [
	ac_cv_c_long_long="yes"
    ], [
	AC_COMPILE_IFELSE([
	    AC_LANG_PROGRAM([[ ]], [[long long int i;]])
	], [
	    ac_cv_c_long_long="yes"
	], [
	    ac_cv_c_long_long="no"
	])
    ])
])

AS_IF([test $ac_cv_c_long_long = yes], [
    AC_DEFINE([HAVE_LONG_LONG], [1], [Does the compiler support the long long type?])
])

AC_CHECK_FUNC([strtoll], [
    AC_DEFINE([HAVE_STRTOLL], [1], [Do we have the strtoll function?])
])
