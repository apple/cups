#
# "$Id: library.m4 44 2006-05-08 19:33:22Z mike $"
#
# Common library stuff for autoconf...
#

dnl Library commands and flags...
AC_PROG_RANLIB
AC_PATH_PROG(AR,ar)

case "$uname" in
        Darwin* | *BSD*)
                ARFLAGS="-rcv"
                ;;
        *)
                ARFLAGS="crvs"
                ;;
esac

AC_SUBST(ARFLAGS)

dnl PIC support...
if test -n "$GCC"; then
	CFLAGS="-fPIC $CFLAGS"
else
	case `uname` in
		HP-UX*)
			# HP-UX
			CFLAGS="+z $CFLAGS"
			;;

		SunOS*)
			# Solaris
			CFLAGS="-KPIC $CFLAGS"
			;;
	esac
fi


#
# End of "$Id: library.m4 44 2006-05-08 19:33:22Z mike $".
#
