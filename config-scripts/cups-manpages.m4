dnl
dnl "$Id: cups-manpages.m4 11342 2013-10-18 20:36:01Z msweet $"
dnl
dnl Manpage stuff for CUPS.
dnl
dnl Copyright 2007-2013 by Apple Inc.
dnl Copyright 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl These coded instructions, statements, and computer programs are the
dnl property of Apple Inc. and are protected by Federal copyright
dnl law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl which should have been included with this file.  If this file is
dnl file is missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl Fix "mandir" variable...
if test "$mandir" = "\${datarootdir}/man" -a "$prefix" = "/"; then
	# New GNU "standards" break previous ones, so make sure we use
	# the right default location for the operating system...
	mandir="\${prefix}/man"
fi

if test "$mandir" = "\${prefix}/man" -a "$prefix" = "/"; then
	case "$uname" in
        	Darwin* | Linux | GNU | *BSD*)
        		# Darwin, MacOS X, Linux, GNU HURD, and *BSD
        		mandir="/usr/share/man"
        		AMANDIR="/usr/share/man"
        		PMANDIR="/usr/share/man"
        		;;
        	*)
        		# All others
        		mandir="/usr/man"
        		AMANDIR="/usr/man"
        		PMANDIR="/usr/man"
        		;;
	esac
else
	AMANDIR="$mandir"
	PMANDIR="$mandir"
fi

AC_SUBST(AMANDIR)
AC_SUBST(PMANDIR)

dnl Setup manpage extensions...
case "$uname" in
	SunOS*)
		# Solaris
		MAN1EXT=1
		MAN5EXT=5
		MAN7EXT=7
		MAN8EXT=1m
		MAN8DIR=1m
		;;
	Linux* | GNU* | Darwin*)
		# Linux, GNU Hurd, and OS X
		MAN1EXT=1.gz
		MAN5EXT=5.gz
		MAN7EXT=7.gz
		MAN8EXT=8.gz
		MAN8DIR=8
		;;
	*)
		# All others
		MAN1EXT=1
		MAN5EXT=5
		MAN7EXT=7
		MAN8EXT=8
		MAN8DIR=8
		;;
esac

AC_SUBST(MAN1EXT)
AC_SUBST(MAN5EXT)
AC_SUBST(MAN7EXT)
AC_SUBST(MAN8EXT)
AC_SUBST(MAN8DIR)

dnl
dnl End of "$Id: cups-manpages.m4 11342 2013-10-18 20:36:01Z msweet $".
dnl
