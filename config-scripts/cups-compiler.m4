dnl
dnl Compiler stuff for CUPS.
dnl
dnl Copyright 2007-2018 by Apple Inc.
dnl Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
dnl

dnl Clear the debugging and non-shared library options unless the user asks
dnl for them...
INSTALL_STRIP=""
AC_SUBST(INSTALL_STRIP)

AC_ARG_WITH(optim, [  --with-optim            set optimization flags ],
	OPTIM="$withval",
	OPTIM="")
AC_SUBST(OPTIM)

AC_ARG_ENABLE(debug, [  --enable-debug          build with debugging symbols])
AC_ARG_ENABLE(debug_guards, [  --enable-debug-guards   build with memory allocation guards])
AC_ARG_ENABLE(debug_printfs, [  --enable-debug-printfs  build with CUPS_DEBUG_LOG support])
AC_ARG_ENABLE(unit_tests, [  --enable-unit-tests     build and run unit tests])

dnl For debugging, keep symbols, otherwise strip them...
if test x$enable_debug = xyes -a "x$OPTIM" = x; then
	OPTIM="-g"
else
	INSTALL_STRIP="-s"
fi

dnl Debug printfs can slow things down, so provide a separate option for that
if test x$enable_debug_printfs = xyes; then
	CFLAGS="$CFLAGS -DDEBUG"
	CXXFLAGS="$CXXFLAGS -DDEBUG"
fi

dnl Debug guards use an extra 4 bytes for some structures like strings in the
dnl string pool, so provide a separate option for that
if test x$enable_debug_guards = xyes; then
	CFLAGS="$CFLAGS -DDEBUG_GUARDS"
	CXXFLAGS="$CXXFLAGS -DDEBUG_GUARDS"
fi

dnl Unit tests take up time during a compile...
if test x$enable_unit_tests = xyes; then
        if test "$build" != "$host"; then
                AC_MSG_ERROR([Sorry, cannot build unit tests when cross-compiling.])
        fi

	UNITTESTS="unittests"
else
	UNITTESTS=""
fi
AC_SUBST(UNITTESTS)

dnl Setup general architecture flags...
AC_ARG_WITH(archflags, [  --with-archflags        set default architecture flags ])
AC_ARG_WITH(ldarchflags, [  --with-ldarchflags      set program architecture flags ])

if test -z "$with_archflags"; then
	ARCHFLAGS=""
else
	ARCHFLAGS="$with_archflags"
fi

if test -z "$with_ldarchflags"; then
	if test "$host_os_name" = darwin; then
		# Only create Intel programs by default
		LDARCHFLAGS="`echo $ARCHFLAGS | sed -e '1,$s/-arch ppc64//'`"
	else
		LDARCHFLAGS="$ARCHFLAGS"
	fi
else
	LDARCHFLAGS="$with_ldarchflags"
fi

AC_SUBST(ARCHFLAGS)
AC_SUBST(LDARCHFLAGS)

dnl Read-only data/program support on Linux...
AC_ARG_ENABLE(relro, [  --enable-relro          build with the GCC relro option])

dnl Clang/GCC address sanitizer...
AC_ARG_ENABLE(sanitizer, [  --enable-sanitizer      build with AddressSanitizer])

dnl Update compiler options...
CXXLIBS="${CXXLIBS:=}"
AC_SUBST(CXXLIBS)

PIEFLAGS=""
AC_SUBST(PIEFLAGS)

RELROFLAGS=""
AC_SUBST(RELROFLAGS)

WARNING_OPTIONS=""
AC_SUBST(WARNING_OPTIONS)

if test -n "$GCC"; then
	# Add GCC-specific compiler options...

        # Address sanitizer is a useful tool to use when developing/debugging
        # code but adds about 2x overhead...
	if test x$enable_sanitizer = xyes; then
		# Use -fsanitize=address with debugging...
		OPTIM="$OPTIM -g -fsanitize=address"
	else
		# Otherwise use the Fortify enhancements to catch any unbounded
		# string operations...
		CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
		CXXFLAGS="$CXXFLAGS -D_FORTIFY_SOURCE=2"
	fi

	# Default optimization options...
	if test -z "$OPTIM"; then
		# Default to optimize-for-size and debug
		OPTIM="-Os -g"
	fi

	# Generate position-independent code as needed...
	if test $PICFLAG = 1; then
    		OPTIM="-fPIC $OPTIM"
	fi

	# The -fstack-protector option is available with some versions of
	# GCC and adds "stack canaries" which detect when the return address
	# has been overwritten, preventing many types of exploit attacks.
	AC_MSG_CHECKING(whether compiler supports -fstack-protector)
	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -fstack-protector"
	AC_TRY_LINK(,,
		if test "x$LSB_BUILD" = xy; then
			# Can't use stack-protector with LSB binaries...
			OPTIM="$OPTIM -fno-stack-protector"
		else
			OPTIM="$OPTIM -fstack-protector"
		fi
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no))
	CFLAGS="$OLDCFLAGS"

	if test "x$LSB_BUILD" != xy; then
		# The -fPIE option is available with some versions of GCC and
		# adds randomization of addresses, which avoids another class of
		# exploits that depend on a fixed address for common functions.
		#
		# Not available to LSB binaries...
		AC_MSG_CHECKING(whether compiler supports -fPIE)
		OLDCFLAGS="$CFLAGS"
		case "$host_os_name" in
			darwin*)
				CFLAGS="$CFLAGS -fPIE -Wl,-pie"
				AC_TRY_COMPILE(,,[
					PIEFLAGS="-fPIE -Wl,-pie"
					AC_MSG_RESULT(yes)],
					AC_MSG_RESULT(no))
				;;

			*)
				CFLAGS="$CFLAGS -fPIE -pie"
				AC_TRY_COMPILE(,,[
					PIEFLAGS="-fPIE -pie"
					AC_MSG_RESULT(yes)],
					AC_MSG_RESULT(no))
				;;
		esac
		CFLAGS="$OLDCFLAGS"
	fi

	# Add useful warning options for tracking down problems...
	WARNING_OPTIONS="-Wall -Wno-format-y2k -Wunused -Wno-unused-result -Wsign-conversion"

	# Test GCC version for certain warning flags since -Werror
	# doesn't trigger...
	gccversion=`$CC --version | head -1 | awk '{print $NF}'`
	case "$gccversion" in
		1.* | 2.* | 3.* | 4.* | 5.* | 6.* | \(clang-*)
			;;
		*)
			WARNING_OPTIONS="$WARNING_OPTIONS -Wno-format-truncation -Wno-format-overflow -Wno-tautological-compare"
			;;
	esac

	# Additional warning options for development testing...
	if test -d .git; then
		WARNING_OPTIONS="-Werror -Wno-error=deprecated-declarations $WARNING_OPTIONS"
	fi
else
	# Add vendor-specific compiler options...
	case $host_os_name in
		sunos*)
			# Solaris
			if test -z "$OPTIM"; then
				OPTIM="-xO2"
			fi

			if test $PICFLAG = 1; then
				OPTIM="-KPIC $OPTIM"
			fi
			;;
		*)
			# Running some other operating system; inform the user
			# they should contribute the necessary options via
			# Github...
			echo "Building CUPS with default compiler optimizations; contact the CUPS developers on Github"
			echo "(https://github.com/apple/cups/issues) with the uname and compiler options needed for"
			echo "your platform, or set the CFLAGS and LDFLAGS environment variables before running"
			echo "configure."
			;;
	esac
fi

# Add general compiler options per platform...
case $host_os_name in
	linux*)
		# glibc 2.8 and higher breaks peer credentials unless you
		# define _GNU_SOURCE...
		OPTIM="$OPTIM -D_GNU_SOURCE"

		# The -z relro option is provided by the Linux linker command to
		# make relocatable data read-only.
		if test x$enable_relro = xyes; then
			RELROFLAGS="-Wl,-z,relro,-z,now"
		fi
		;;
esac
