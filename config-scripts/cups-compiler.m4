dnl
dnl "$Id$"
dnl
dnl   Compiler stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007-2008 by Apple Inc.
dnl   Copyright 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl Clear the debugging and non-shared library options unless the user asks
dnl for them...
OPTIM=""
AC_SUBST(OPTIM)

AC_ARG_WITH(optim, [  --with-optim="flags"    set optimization flags ])
AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no],
	[if test x$enable_debug = xyes; then
		OPTIM="-g"
	fi])

dnl Setup general architecture flags...
AC_ARG_WITH(archflags, [  --with-archflags="flags"
                          set default architecture flags ])

if test -z "$with_archflags"; then
	ARCHFLAGS=""
	LDARCHFLAGS=""
else
	ARCHFLAGS="$with_archflags"
	if test "$uname" = Darwin; then
		# Only link 32-bit programs - 64-bit is for the shared
		# libraries...
		LDARCHFLAGS="`echo $ARCHFLAGS | sed -e '1,$s/-arch x86_64//' -e '1,$s/-arch ppc64//'`"
	else
		LDARCHFLAGS="$ARCHFLAGS"
	fi
fi

AC_SUBST(ARCHFLAGS)
AC_SUBST(LDARCHFLAGS)

dnl Setup support for separate 32/64-bit library generation...
AC_ARG_WITH(arch32flags, [  --with-arch32flags="flags"
                          specifies 32-bit architecture flags])
ARCH32FLAGS=""
AC_SUBST(ARCH32FLAGS)

AC_ARG_WITH(arch64flags, [  --with-arch64flags="flags"
                          specifies 64-bit architecture flags])
ARCH64FLAGS=""
AC_SUBST(ARCH64FLAGS)

dnl Read-only data/program support on Linux...
AC_ARG_ENABLE(relro, [  --enable-relro          use GCC relro option, default=no])

dnl Update compiler options...
CXXLIBS="${CXXLIBS:=}"
AC_SUBST(CXXLIBS)

PIEFLAGS=""
AC_SUBST(PIEFLAGS)

RELROFLAGS=""
AC_SUBST(RELROFLAGS)

LIBCUPSORDER="libcups.order"
AC_ARG_WITH(libcupsorder, [  --with-libcupsorder     libcups secorder file, default=libcups.order],
	if test -f "$withval"; then
		LIBCUPSORDER="$withval"
	fi)
AC_SUBST(LIBCUPSORDER)

LIBCUPSIMAGEORDER="libcupsimage.order"
AC_ARG_WITH(libcupsimageorder, [  --with-libcupsimagesorder
                          libcupsimage secorder file, default=libcupsimage.order],
	if test -f "$withval"; then
		LIBCUPSIMAGEORDER="$withval"
	fi)
AC_SUBST(LIBCUPSIMAGEORDER)

if test -n "$GCC"; then
	# Add GCC-specific compiler options...
	if test -z "$OPTIM"; then
		if test "x$with_optim" = x; then
			# Default to optimize-for-size and debug
       			OPTIM="-Os -g"
		else
			OPTIM="$with_optim $OPTIM"
		fi
	fi

	# Generate position-independent code as needed...
	if test $PICFLAG = 1 -a $uname != AIX; then
    		OPTIM="-fPIC $OPTIM"
	fi

	# The -fstack-protector option is available with some versions of
	# GCC and adds "stack canaries" which detect when the return address
	# has been overwritten, preventing many types of exploit attacks.
	AC_MSG_CHECKING(if GCC supports -fstack-protector)
	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -fstack-protector"
	AC_TRY_COMPILE(,,
		OPTIM="$OPTIM -fstack-protector"
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no))
	CFLAGS="$OLDCFLAGS"

	# The -pie option is available with some versions of GCC and adds
	# randomization of addresses, which avoids another class of exploits
	# that depend on a fixed address for common functions.
	AC_MSG_CHECKING(if GCC supports -pie)
	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -pie -fPIE"
	AC_TRY_COMPILE(,,
		PIEFLAGS="-pie -fPIE"
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no))
	CFLAGS="$OLDCFLAGS"

	if test "x$with_optim" = x; then
		# Add useful warning options for tracking down problems...
		OPTIM="-Wall -Wno-format-y2k $OPTIM"
		# Additional warning options for development testing...
		if test -d .svn; then
			OPTIM="-Wshadow -Wunused $OPTIM"
		fi
	fi

	case "$uname" in
		Darwin*)
			# -D_FORTIFY_SOURCE=2 adds additional object size
			# checking, basically wrapping all string functions
			# with buffer-limited ones.  Not strictly needed for
			# CUPS since we already use buffer-limited calls, but
			# this will catch any additions that are broken.		
			CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"

			if test x$enable_pie = xyes; then
				# GCC 4 on Mac OS X needs -Wl,-pie as well
				LDFLAGS="$LDFLAGS -Wl,-pie"
			fi
			;;

		HP-UX*)
			if test "x$enable_32bit" = xyes; then
				# Build 32-bit libraries, 64-bit base...
				if test -z "$with_arch32flags"; then
					ARCH32FLAGS="-milp32"
				else
					ARCH32FLAGS="$with_arch32flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="-mlp64"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			fi

			if test "x$enable_64bit" = xyes; then
				# Build 64-bit libraries, 32-bit base...
				if test -z "$with_arch64flags"; then
					ARCH64FLAGS="-mlp64"
				else
					ARCH64FLAGS="$with_arch64flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="-milp32"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;

		IRIX)
			if test "x$enable_32bit" = xyes; then
				# Build 32-bit libraries, 64-bit base...
				if test -z "$with_arch32flags"; then
					ARCH32FLAGS="-n32 -mips3"
				else
					ARCH32FLAGS="$with_arch32flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="-64 -mips4"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			fi

			if test "x$enable_64bit" = xyes; then
				# Build 64-bit libraries, 32-bit base...
				if test -z "$with_arch64flags"; then
					ARCH64FLAGS="-64 -mips4"
				else
					ARCH64FLAGS="$with_arch64flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="-n32 -mips3"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;

		Linux*)
			# The -z relro option is provided by the Linux linker command to
			# make relocatable data read-only.
			if test x$enable_relro = xyes; then
				RELROFLAGS="-Wl,-z,relro"
			fi

			if test "x$enable_32bit" = xyes; then
				# Build 32-bit libraries, 64-bit base...
				if test -z "$with_arch32flags"; then
					ARCH32FLAGS="-m32"
				else
					ARCH32FLAGS="$with_arch32flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="-m64"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			fi

			if test "x$enable_64bit" = xyes; then
				# Build 64-bit libraries, 32-bit base...
				if test -z "$with_arch64flags"; then
					ARCH64FLAGS="-m64"
				else
					ARCH64FLAGS="$with_arch64flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="-m32"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;

		SunOS*)
			if test "x$enable_32bit" = xyes; then
				# Build 32-bit libraries, 64-bit base...
				if test -z "$with_arch32flags"; then
					ARCH32FLAGS="-m32"
				else
					ARCH32FLAGS="$with_arch32flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="-m64"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			fi

			if test "x$enable_64bit" = xyes; then
				# Build 64-bit libraries, 32-bit base...
				if test -z "$with_arch64flags"; then
					ARCH64FLAGS="-m64"
				else
					ARCH64FLAGS="$with_arch64flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="-m32"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;
	esac
else
	# Add vendor-specific compiler options...
	case $uname in
		AIX*)
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-O2 -qmaxmem=6000"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi
			;;
		HP-UX*)
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="+O2"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi

			CFLAGS="-Ae $CFLAGS"

			if test $PICFLAG = 1; then
				OPTIM="+z $OPTIM"
			fi

			if test "x$enable_32bit" = xyes; then
				# Build 32-bit libraries, 64-bit base...
				if test -z "$with_arch32flags"; then
					ARCH32FLAGS="+DD32"
				else
					ARCH32FLAGS="$with_arch32flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="+DD64"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			fi

			if test "x$enable_64bit" = xyes; then
				# Build 64-bit libraries, 32-bit base...
				if test -z "$with_arch64flags"; then
					ARCH64FLAGS="+DD64"
				else
					ARCH64FLAGS="$with_arch64flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="+DD32"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;
        	IRIX)
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-O2"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi

			if test "x$with_optim" = x; then
				OPTIM="-fullwarn -woff 1183,1209,1349,1506,3201 $OPTIM"
			fi

			if test "x$enable_32bit" = xyes; then
				# Build 32-bit libraries, 64-bit base...
				if test -z "$with_arch32flags"; then
					ARCH32FLAGS="-n32 -mips3"
				else
					ARCH32FLAGS="$with_arch32flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="-64 -mips4"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			fi

			if test "x$enable_64bit" = xyes; then
				# Build 64-bit libraries, 32-bit base...
				if test -z "$with_arch64flags"; then
					ARCH64FLAGS="-64 -mips4"
				else
					ARCH64FLAGS="$with_arch64flags"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="-n32 -mips3"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;
		OSF*)
			# Tru64 UNIX aka Digital UNIX aka OSF/1
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-O"
				else
					OPTIM="$with_optim"
				fi
			fi
			;;
		SunOS*)
			# Solaris
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-xO2"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi

			if test $PICFLAG = 1; then
				OPTIM="-KPIC $OPTIM"
			fi

			if test "x$enable_32bit" = xyes; then
				# Compiling on a Solaris system, build 64-bit
				# binaries with separate 32-bit libraries...
				ARCH32FLAGS="-xarch=generic"

				if test "x$with_optim" = x; then
					# Suppress all of Sun's questionable
					# warning messages, and default to
					# 64-bit compiles of everything else...
					OPTIM="-w $OPTIM"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch64flags"; then
						ARCHFLAGS="-xarch=generic64"
					else
						ARCHFLAGS="$with_arch64flags"
					fi
				fi
			else
				if test "x$enable_64bit" = xyes; then
					# Build 64-bit libraries...
					ARCH64FLAGS="-xarch=generic64"
				fi

				if test "x$with_optim" = x; then
					# Suppress all of Sun's questionable
					# warning messages, and default to
					# 32-bit compiles of everything else...
					OPTIM="-w $OPTIM"
				fi

				if test -z "$with_archflags"; then
					if test -z "$with_arch32flags"; then
						ARCHFLAGS="-xarch=generic"
					else
						ARCHFLAGS="$with_arch32flags"
					fi
				fi
			fi
			;;
		UNIX_SVR*)
			# UnixWare
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-O"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi

			if test $PICFLAG = 1; then
				OPTIM="-KPIC $OPTIM"
			fi
			;;
		*)
			# Running some other operating system; inform the user they
			# should contribute the necessary options to
			# cups-support@cups.org...
			echo "Building CUPS with default compiler optimizations; contact"
			echo "cups-bugs@cups.org with uname and compiler options needed"
			echo "for your platform, or set the CFLAGS and LDFLAGS environment"
			echo "variables before running configure."
			;;
	esac
fi

# Add general compiler options per platform...
case $uname in
	HP-UX*)
		# HP-UX 10.20 (at least) needs this definition to get the
		# h_errno global...
		OPTIM="$OPTIM -D_XOPEN_SOURCE_EXTENDED"

		# HP-UX 11.00 (at least) needs this definition to get the
		# u_short type used by the IP headers...
		OPTIM="$OPTIM -D_INCLUDE_HPUX_SOURCE"

		# HP-UX 11.23 (at least) needs this definition to get the
		# IPv6 header to work...
		OPTIM="$OPTIM -D_HPUX_SOURCE"
		;;

	OSF*)
		# Tru64 UNIX aka Digital UNIX aka OSF/1 need to be told
		# to be POSIX-compliant...
		OPTIM="$OPTIM -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -D_OSF_SOURCE"
		;;
esac

dnl
dnl End of "$Id$".
dnl
