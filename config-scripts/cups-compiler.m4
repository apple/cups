dnl
dnl "$Id: cups-compiler.m4,v 1.9.2.5 2002/03/08 20:23:02 mike Exp $"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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
dnl       Hollywood, Maryland 20636-3111 USA
dnl
dnl       Voice: (301) 373-9603
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl
dnl Clear the debugging and non-shared library options unless the user asks
dnl for them...

OPTIM=""
AC_SUBST(OPTIM)

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging [default=no]],
	[if test x$enable_debug = xyes; then
		OPTIM="-g"
	fi])

AC_ARG_WITH(optim, [  --with-optim=\"flags\"    set optimization flags ])

dnl Update compiler options...
if test -n "$GCC"; then
	# Starting with GCC 3.0, you must link C++ programs against either
	# libstdc++ (shared by default), or libsupc++ (always static).  If
	# you care about binary portability between Linux distributions,
	# you need to either 1) build your own GCC with static C++ libraries
	# or 2) link using gcc and libsupc++.  We choose the latter since
	# CUPS doesn't (currently) use any of the stdc++ library.
	#
	# Also, GCC 3.0.x still has problems compiling some code.  You may
	# or may not have success with it.  USE 3.0.x WITH EXTREME CAUTION!
	#
	# Previous versions of GCC do not have the reliance on the stdc++
	# or g++ libraries, so the extra supc++ library is not needed.

	AC_MSG_CHECKING(if libsupc++ is required)

 	SUPC="`$CXX -print-file-name=libsupc++.a 2>/dev/null`"
 	if test -n "$SUPC" -a "$SUPC" != "libsupc++.a"; then
 		# This is gcc 3.x, and it knows of libsupc++, so we need it
 		LIBS="$LIBS -lsupc++"
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
 	fi

	CXX="$CC"

	if test -z "$OPTIM"; then
		if test "x$with_optim" = x; then
			if test $uname = HP-UX; then
				# GCC under HP-UX has bugs with -O2
				OPTIM="-O1"
			else
		       		OPTIM="-O2"
			fi
		else
			OPTIM="$with_optim $OPTIM"
		fi
	fi

	if test $PICFLAG = 1 -a $uname != AIX; then
    		OPTIM="-fPIC $OPTIM"
	fi

	if test "x$with_optim" = x; then
		OPTIM="-Wall $OPTIM"
	fi
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
        	IRIX*)
			if test -z "$OPTIM"; then
				if test "x$with_optim" = x; then
					OPTIM="-O2"
				else
					OPTIM="$with_optim $OPTIM"
				fi
			fi

			if test $uversion -ge 62 -a "x$with_optim" = x; then
				OPTIM="$OPTIM -n32 -mips3"
			fi

			if test "x$with_optim" = x; then
				OPTIM="-fullwarn $OPTIM"
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
				OPTIM="$OPTIM -xarch=generic"
			fi

			if test $PICFLAG = 1; then
				OPTIM="-KPIC $OPTIM"
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

case $uname in
	*BSD)
		ARFLAGS="-rcv"
		;;
	Darwin*)
 		# For Darwin RC_CFLAGS can be "-arch ppc -arch i386 -pipe"
 		CFLAGS="\$(RC_CFLAGS) $CFLAGS"
 		CXXFLAGS="\$(RC_CFLAGS) $CXXFLAGS"

		ARFLAGS="-rcv"
		;;
	*)
		ARFLAGS="crvs"
		;;
esac

dnl
dnl End of "$Id: cups-compiler.m4,v 1.9.2.5 2002/03/08 20:23:02 mike Exp $".
dnl
