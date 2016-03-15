#
# "$Id$"
#
# Common C++ compiler stuff for autoconf...
#

CXXFLAGS="${CXXFLAGS:=}"

if test x$enable_debug = xyes; then
	CXXFLAGS="-g $CXXFLAGS"
fi

AC_PROG_CXX

dnl Add -Wall for GCC...
if test -n "$GCC"; then
	dnl Extended warnings...
	CXXFLAGS="-Wshadow -Wconversion -Winline $CXXFLAGS"
	dnl Standard warnings...
	CXXFLAGS="-Wall -Wunused -Wno-char-subscripts -Wno-format-y2k $CXXFLAGS"
fi


#
# End of "$Id$".
#
