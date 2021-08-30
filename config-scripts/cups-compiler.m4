dnl
dnl Compiler tests for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2018 by Apple Inc.
dnl Copyright © 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Clear the debugging and non-shared library options unless the user asks
dnl for them...
INSTALL_STRIP=""
AC_SUBST(INSTALL_STRIP)

AC_ARG_WITH([optim], AS_HELP_STRING([--with-optim], [set optimization flags]), [
    OPTIM="$withval"
], [
    OPTIM=""
])
AC_SUBST([OPTIM])

AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [build with debugging symbols]))
AC_ARG_ENABLE([debug_guards], AS_HELP_STRING([--enable-debug-guards], [build with memory allocation guards]))
AC_ARG_ENABLE([debug_printfs], AS_HELP_STRING([--enable-debug-printfs], [build with CUPS_DEBUG_LOG support]))
AC_ARG_ENABLE([maintainer], AS_HELP_STRING([--enable-maintainer], [turn on maintainer mode (warnings as errors)]))
AC_ARG_ENABLE([unit_tests], AS_HELP_STRING([--enable-unit-tests], [build and run unit tests]))

dnl For debugging, keep symbols, otherwise strip them...
AS_IF([test x$enable_debug = xyes -a "x$OPTIM" = x], [
    OPTIM="-g"
], [
    INSTALL_STRIP="-s"
])

dnl Debug printfs can slow things down, so provide a separate option for that
AS_IF([test x$enable_debug_printfs = xyes], [
    CFLAGS="$CFLAGS -DDEBUG"
    CXXFLAGS="$CXXFLAGS -DDEBUG"
])

dnl Debug guards use an extra 4 bytes for some structures like strings in the
dnl string pool, so provide a separate option for that
AS_IF([test x$enable_debug_guards = xyes], [
    CFLAGS="$CFLAGS -DDEBUG_GUARDS"
    CXXFLAGS="$CXXFLAGS -DDEBUG_GUARDS"
])

dnl Unit tests take up time during a compile...
AS_IF([test x$enable_unit_tests = xyes], [
    AS_IF([test "$build" != "$host"], [
	AC_MSG_ERROR([Sorry, cannot build unit tests when cross-compiling.])
    ])

    UNITTESTS="unittests"
], [
    UNITTESTS=""
])
AC_SUBST([UNITTESTS])

dnl Setup general architecture flags...
AC_ARG_WITH([archflags], AS_HELP_STRING([--with-archflags], [set default architecture flags]))
AC_ARG_WITH([ldarchflags], AS_HELP_STRING([--with-ldarchflags], [set program architecture flags]))

AS_IF([test -z "$with_archflags"], [
    ARCHFLAGS=""
], [
    ARCHFLAGS="$with_archflags"
])

AS_IF([test -z "$with_ldarchflags"], [
    LDARCHFLAGS="$ARCHFLAGS"
], [
    LDARCHFLAGS="$with_ldarchflags"
])

AC_SUBST([ARCHFLAGS])
AC_SUBST([LDARCHFLAGS])

dnl Read-only data/program support on Linux...
AC_ARG_ENABLE([relro], AS_HELP_STRING([--enable-relro], [build with the relro option]))

dnl Clang/GCC address sanitizer...
AC_ARG_ENABLE([sanitizer], AS_HELP_STRING([--enable-sanitizer], [build with AddressSanitizer]))

dnl Update compiler options...
CXXLIBS="${CXXLIBS:=}"
AC_SUBST([CXXLIBS])

PIEFLAGS=""
AC_SUBST([PIEFLAGS])

RELROFLAGS=""
AC_SUBST([RELROFLAGS])

WARNING_OPTIONS=""
AC_SUBST([WARNING_OPTIONS])

AS_IF([test -n "$GCC"], [
    # Add GCC/Clang compiler options...

    # Address sanitizer is a useful tool to use when developing/debugging
    # code but adds about 2x overhead...
    AS_IF([test x$enable_sanitizer = xyes], [
	# Use -fsanitize=address with debugging...
	OPTIM="$OPTIM -g -fsanitize=address"
    ], [
	# Otherwise use the Fortify enhancements to catch any unbounded
	# string operations...
	CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
	CXXFLAGS="$CXXFLAGS -D_FORTIFY_SOURCE=2"
    ])

    # Default optimization options...
    AS_IF([test -z "$OPTIM"], [
	# Default to optimize-for-size and debug
	OPTIM="-Os -g"
    ])

    # Generate position-independent code as needed...
    AS_IF([test $PICFLAG = 1], [
	OPTIM="-fPIC $OPTIM"
    ])

    # The -fstack-protector option is available with some versions of
    # GCC and adds "stack canaries" which detect when the return address
    # has been overwritten, preventing many types of exploit attacks.
    AC_MSG_CHECKING([whether compiler supports -fstack-protector])
    OLDCFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -fstack-protector"
    AC_LINK_IFELSE([AC_LANG_PROGRAM()], [
	AS_IF([test "x$LSB_BUILD" = xy], [
	    # Can't use stack-protector with LSB binaries...
	    OPTIM="$OPTIM -fno-stack-protector"
	], [
	    OPTIM="$OPTIM -fstack-protector"
	])
	AC_MSG_RESULT([yes])
    ], [
	AC_MSG_RESULT([no])
    ])
    CFLAGS="$OLDCFLAGS"

    AS_IF([test "x$LSB_BUILD" != xy], [
	# The -fPIE option is available with some versions of GCC and
	# adds randomization of addresses, which avoids another class of
	# exploits that depend on a fixed address for common functions.
	#
	# Not available to LSB binaries...
	AC_MSG_CHECKING([whether compiler supports -fPIE])
	OLDCFLAGS="$CFLAGS"
	AS_CASE(["$host_os_name"], [darwin*], [
	    CFLAGS="$CFLAGS -fPIE -Wl,-pie"
	    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
		PIEFLAGS="-fPIE -Wl,-pie"
		AC_MSG_RESULT([yes])
	    ], [
		AC_MSG_RESULT([no])
	    ])
	], [*], [
	    CFLAGS="$CFLAGS -fPIE -pie"
	    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
		PIEFLAGS="-fPIE -pie"
		AC_MSG_RESULT([yes])
	    ], [
		AC_MSG_RESULT([no])
	    ])
	])
	CFLAGS="$OLDCFLAGS"
    ])

    dnl Show all standard warnings + unused variables when compiling...
    WARNING_OPTIONS="-Wall -Wunused"

    dnl Drop some not-useful/unreliable warnings...
    for warning in char-subscripts deprecated-declarations format-truncation format-y2k switch unused-result; do
	AC_MSG_CHECKING([whether compiler supports -Wno-$warning])

	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -Wno-$warning -Werror"

	AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
	    AC_MSG_RESULT(yes)
	    WARNING_OPTIONS="$WARNING_OPTIONS -Wno-$warning"
        ], [
	    AC_MSG_RESULT(no)
	])

	CFLAGS="$OLDCFLAGS"
    done

    dnl Maintainer mode enables -Werror...
    AS_IF([test x$enable_maintainer = xyes], [
	WARNING_OPTIONS="$WARNING_OPTIONS -Werror"
    ])
], [
    # Add vendor-specific compiler options...
    AS_CASE([$host_os_name], [sunos*], [
	# Solaris
	AS_IF([test -z "$OPTIM"], [
	    OPTIM="-xO2"
	])

	AS_IF([test $PICFLAG = 1], [
	    OPTIM="-KPIC $OPTIM"
	])
    ], [*], [
	# Running some other operating system; inform the user
	# they should contribute the necessary options via
	# Github...
	echo "Building CUPS with default compiler optimizations."
	echo "Contact the OpenPrinting CUPS developers on Github with the uname and compiler"
	echo "options needed for your platform, or set the CFLAGS and LDFLAGS environment"
	echo "variables before running configure."
	echo ""
	echo "https://github.com/openprinting/cups"
    ])
])

# Add general compiler options per platform...
AS_CASE([$host_os_name], [linux*], [
    # glibc 2.8 and higher breaks peer credentials unless you
    # define _GNU_SOURCE...
    OPTIM="$OPTIM -D_GNU_SOURCE"

    # The -z relro option is provided by the Linux linker command to
    # make relocatable data read-only.
    AS_IF([test x$enable_relro = xyes], [
	RELROFLAGS="-Wl,-z,relro,-z,now"
    ])
])
