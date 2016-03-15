#
# "$Id: compiler.m4 142 2006-08-17 14:50:46Z mike $"
#
# Common compiler stuff for autoconf...
#

CFLAGS="${CFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"
LIBS="${LIBS:=}"

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no],
    [if test x$enable_debug = xyes; then
	CFLAGS="-g $CFLAGS"
	LDFLAGS="-g $LDFLAGS"
    fi])


AC_PROG_CC
AC_PATH_PROG(RM,rm)

dnl Support large files.
AC_SYS_LARGEFILE

dnl Add -Wall for GCC...
if test -n "$GCC"; then
	dnl Extended warnings...
	CFLAGS="-Wshadow -Winline $CFLAGS"
	dnl Standard warnings...
	CFLAGS="-Wall -Wunused -Wno-char-subscripts -Wno-format-y2k $CFLAGS"
fi


#
# End of "$Id: compiler.m4 142 2006-08-17 14:50:46Z mike $".
#
