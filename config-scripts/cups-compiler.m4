dnl
dnl "$Id$"
dnl
dnl   Common configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging, default=no],
	[if test x$enable_debug = xyes; then
		OPTIM="-g"
	fi])

AC_ARG_WITH(optim, [  --with-optim="flags"    set optimization flags ])

dnl Update compiler options...
CXXLIBS=""
AC_SUBST(CXXLIBS)

if test -n "$GCC"; then
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

	AC_MSG_CHECKING(if GCC supports -fpie)

	OLDCFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -fpie"
	AC_TRY_COMPILE(,,
		OPTIM="$OPTIM -fpie"
		LDFLAGS="$LDFLAGS -pie"
		AC_MSG_RESULT(yes),
		CFLAGS="$OLDCFLAGS"
		AC_MSG_RESULT(no))

	if test "x$with_optim" = x; then
		# Add useful warning options for tracking down problems...
		OPTIM="-Wall -Wno-format-y2k $OPTIM"
		# Additional warning options for alpha testing...
		OPTIM="-Wshadow -Wunused $OPTIM"
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
        	IRIX)
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
				# Show most warnings, but suppress the
				# ones about arguments not being used,
				# string constants assigned to const
				# char *'s, etc.  We only set the warning
				# options on IRIX 6.2 and higher because
				# of limitations in the older SGI compiler
				# tools.
				if test $uversion -ge 62; then
					OPTIM="-fullwarn -woff 1183,1209,1349,3201 $OPTIM"
				fi
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
