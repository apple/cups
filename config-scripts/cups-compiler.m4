dnl
dnl "$Id: cups-compiler.m4,v 1.9.2.2 2002/01/02 18:50:23 mike Exp $"
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

	case "`$CXX --version`" in
    		3*)
			AC_MSG_WARN(GCC 3.0.x is known to produce incorrect code - use with caution!)
			LIBS="$LIBS -lsupc++"
			;;
    		3.1*)
			LIBS="$LIBS -lsupc++"
			;;
	esac

	CXX="$CC"

	if test -z "$OPTIM"; then
		if test $uname = HP-UX; then
			# GCC under HP-UX has bugs with -O2
			OPTIM="-O1"
		else
		       	OPTIM="-O2"
		fi
	fi

	if test $PICFLAG = 1 -a $uname != AIX; then
    		OPTIM="-fPIC $OPTIM"
	fi

	OPTIM="-Wall $OPTIM"
else
	case $uname in
		AIX*)
			if test -z "$OPTIM"; then
				OPTIM="-O2 -qmaxmem=6000"
			fi
			;;
		HP-UX*)
			if test -z "$OPTIM"; then
				OPTIM="+O2"
			fi

			CFLAGS="-Ae $CFLAGS"

			OPTIM="+DAportable $OPTIM"

			if test $PICFLAG = 1; then
				OPTIM="+z $OPTIM"
			fi
			;;
        	IRIX*)
			if test -z "$OPTIM"; then
        			OPTIM="-O2"
			fi

			if test $uversion -ge 62; then
				OPTIM="$OPTIM -n32 -mips3"
			fi

			OPTIM="-fullwarn $OPTIM"
			;;
		SunOS*)
			# Solaris
			if test -z "$OPTIM"; then
				OPTIM="-xO4"
			fi

			OPTIM="$OPTIM -xarch=generic"

			if test $PICFLAG = 1; then
				OPTIM="-KPIC $OPTIM"
			fi
			;;
		UNIX_SVR*)
			# UnixWare
			if test -z "$OPTIM"; then
				OPTIM="-O"
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
	Darwin* | *BSD)
		ARFLAGS="-rcv"
		;;
	*)
		ARFLAGS="crvs"
		;;
esac

dnl
dnl End of "$Id: cups-compiler.m4,v 1.9.2.2 2002/01/02 18:50:23 mike Exp $".
dnl
