dnl
dnl "$Id$"
dnl
dnl   Compiler stuff for the Common UNIX Printing System (CUPS).
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

dnl Clear the debugging and non-shared library options unless the user asks
dnl for them...
OPTIM=""
AC_SUBST(OPTIM)

AC_ARG_WITH(optim, [  --with-optim="flags"    set optimization flags ])

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no],
	[if test x$enable_debug = xyes; then
		OPTIM="-g"
	fi])

dnl Setup support for separate 32/64-bit library generation...
AC_ARG_ENABLE(32bit, [  --enable-32bit          generate 32-bit libraries on 32/64-bit systems, default=no])

C32FLAGS=""
INSTALL32=""
LIB32CUPS=""
LIB32CUPSIMAGE=""
LIB32DIR=""
UNINSTALL32=""

AC_SUBST(C32FLAGS)
AC_SUBST(INSTALL32)
AC_SUBST(LIB32CUPS)
AC_SUBST(LIB32CUPSIMAGE)
AC_SUBST(LIB32DIR)
AC_SUBST(UNINSTALL32)

AC_ARG_ENABLE(64bit, [  --enable-64bit          generate 64-bit libraries on 32/64-bit systems, default=no])

C64FLAGS=""
INSTALL64=""
LIB64CUPS=""
LIB64CUPSIMAGE=""
LIB64DIR=""
UNINSTALL64=""

AC_SUBST(C64FLAGS)
AC_SUBST(INSTALL64)
AC_SUBST(LIB64CUPS)
AC_SUBST(LIB64CUPSIMAGE)
AC_SUBST(LIB64DIR)
AC_SUBST(UNINSTALL64)

dnl Position-Independent Executable support on Linux and *BSD...
AC_ARG_ENABLE(pie, [  --enable-pie            use GCC -fpie option, default=no])

dnl Update compiler options...
CXXLIBS=""
AC_SUBST(CXXLIBS)

if test -n "$GCC"; then
	if test -z "$OPTIM"; then
		if test "x$with_optim" = x; then
			# Default to optimize-for-size and debug
       			OPTIM="-Os -g"
		else
			OPTIM="$with_optim $OPTIM"
		fi
	fi

	if test $PICFLAG = 1 -a $uname != AIX; then
    		OPTIM="-fPIC $OPTIM"
	fi

	case $uname in
		Darwin*)
			if test "x$with_optim" = x; then
				if test "x`uname -m`" = xi386; then
					# Build universal binaries for OSX on Intel...
					OPTIM="-arch i386 -arch ppc $OPTIM"
					DSOFLAGS="-arch i386 -arch ppc $DSOFLAGS"
				fi
			fi
			;;

		Linux*)
			if test x$enable_pie = xyes; then
				OPTIM="$OPTIM -fpie"
				LDFLAGS="$LDFLAGS -pie"
			fi
			;;
	esac

	if test "x$with_optim" = x; then
		# Add useful warning options for tracking down problems...
		OPTIM="-Wall -Wno-format-y2k $OPTIM"
		# Additional warning options for alpha testing...
		OPTIM="-Wshadow -Wunused $OPTIM"
	fi

	case "$uname" in
		IRIX)
			if test "x$enable_32bit" = xyes; then
				# Compiling on an IRIX system, build 32-bit
				# libraries...
				C32FLAGS="-n32 -mips3"
				INSTALL32="install32bit"
				LIB32CUPS="libcups.32.so.2"
				LIB32CUPSIMAGE="libcupsimage.32.so.2"
				LIB32DIR="$prefix/lib32"
				UNINSTALL32="uninstall32bit"
			fi

			if test "x$enable_64bit" = xyes; then
				# Compiling on an IRIX system, build 64-bit
				# libraries...
				C64FLAGS="-64 -mips4"
				INSTALL64="install64bit"
				LIB64CUPS="libcups.64.so.2"
				LIB64CUPSIMAGE="libcupsimage.64.so.2"
				LIB64DIR="$prefix/lib64"
				UNINSTALL64="uninstall64bit"
			fi
			;;

		Linux*)
			if test "x$enable_32bit" = xyes; then
				# Compiling on an 64-bit x86 system, build 32-bit
				# libraries...
				C32FLAGS="-m32"
				INSTALL32="install32bit"
				LIB32CUPS="libcups.32.so.2"
				LIB32CUPSIMAGE="libcupsimage.32.so.2"
				LIB32DIR="$exec_prefix/lib"
				if test -d /usr/lib32; then
					LIB32DIR="${LIB32DIR}32"
				fi
				UNINSTALL32="uninstall32bit"
			fi
			;;

		SunOS*)
			if test "x$enable_32bit" = xyes; then
				# Compiling on a Solaris system, build 32-bit
				# libraries...
				C32FLAGS="-m32"
				INSTALL32="install32bit"
				LIB32CUPS="libcups.32.so.2"
				LIB32CUPSIMAGE="libcupsimage.32.so.2"
				LIB32DIR="$exec_prefix/lib/32"
				UNINSTALL32="uninstall32bit"
			fi

			if test "x$enable_64bit" = xyes; then
				# Compiling on a Solaris system, build 64-bit
				# libraries...
				C64FLAGS="-m64"
				INSTALL64="install64bit"
				LIB64CUPS="libcups.64.so.2"
				LIB64CUPSIMAGE="libcupsimage.64.so.2"
				LIB64DIR="$exec_prefix/lib/64"
				UNINSTALL64="uninstall64bit"
			fi
			;;
	esac
else
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

			if test "x$with_optim" = x; then
				OPTIM="+DAportable $OPTIM"
			fi

			if test $PICFLAG = 1; then
				OPTIM="+z $OPTIM"
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
				# Compiling on an IRIX system, build 32-bit
				# libraries...
				C32FLAGS="-n32 -mips3"
				INSTALL32="install32bit"
				LIB32CUPS="libcups.32.so.2"
				LIB32CUPSIMAGE="libcupsimage.32.so.2"
				LIB32DIR="$exec_prefix/lib32"
				UNINSTALL32="uninstall32bit"
			fi

			if test "x$enable_64bit" = xyes; then
				# Compiling on an IRIX system, build 64-bit
				# libraries...
				C64FLAGS="-64 -mips4"
				INSTALL64="install64bit"
				LIB64CUPS="libcups.64.so.2"
				LIB64CUPSIMAGE="libcupsimage.64.so.2"
				LIB64DIR="$exec_prefix/lib64"
				UNINSTALL64="uninstall64bit"
			fi
			;;
		SunOS*)
			# Solaris
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-xO4"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi

			if test "x$with_optim" = x; then
				# Specify "generic" SPARC output and suppress
				# all of Sun's questionable warning messages...
				OPTIM="-w $OPTIM -xarch=generic"
			fi

			if test $PICFLAG = 1; then
				OPTIM="-KPIC $OPTIM"
			fi

			if test "x$enable_32bit" = xyes; then
				# Compiling on a Solaris system, build 32-bit
				# libraries...
				C32FLAGS="-xarch=v8"
				INSTALL32="install32bit"
				LIB32CUPS="libcups.32.so.2"
				LIB32CUPSIMAGE="libcupsimage.32.so.2"
				LIB32DIR="$exec_prefix/lib/32"
				UNINSTALL32="uninstall32bit"
			fi

			if test "x$enable_64bit" = xyes; then
				# Compiling on a Solaris system, build 64-bit
				# libraries...
				C64FLAGS="-xarch=v9 -xcode=pic32"
				INSTALL64="install64bit"
				LIB64CUPS="libcups.64.so.2"
				LIB64CUPSIMAGE="libcupsimage.64.so.2"
				LIB64DIR="$exec_prefix/lib/64"
				UNINSTALL64="uninstall64bit"
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
			echo "for your platform, or set the CFLAGS and CXXFLAGS"
			echo "environment variable before running configure."
			;;
	esac
fi

if test $uname = HP-UX; then
	# HP-UX 10.20 (at least) needs this definition to get the
	# h_errno global...
	OPTIM="$OPTIM -D_XOPEN_SOURCE_EXTENDED"

	# HP-UX 11.00 (at least) needs this definition to get the
	# u_short type used by the IP headers...
	OPTIM="$OPTIM -D_INCLUDE_HPUX_SOURCE"
fi

dnl
dnl End of "$Id$".
dnl
