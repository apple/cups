dnl
dnl "$Id: cups-compiler.m4,v 1.5 2001/07/03 17:07:29 mike Exp $"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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

AC_ARG_ENABLE(debug, [  --enable-debug        turn on debugging [default=no]],
	[if test x$enable_debug = xyes; then
		OPTIM="-g"
	fi])

dnl Update compiler options...
if test -n "$GCC"; then
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

dnl
dnl End of "$Id: cups-compiler.m4,v 1.5 2001/07/03 17:07:29 mike Exp $".
dnl
