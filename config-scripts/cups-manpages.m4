dnl
dnl "$Id: cups-manpages.m4,v 1.5.2.3 2003/04/14 19:56:02 mike Exp $"
dnl
dnl   Manpage stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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

dnl Fix "mandir" variable...
if test "$mandir" = "\${prefix}/man" -a "$prefix" = "/"; then
	case "$uname" in
        	Darwin* | Linux* | *BSD* | AIX*)
        		# Darwin, MacOS X, Linux, *BSD, and AIX
        		mandir="/usr/share/man"
        		AMANDIR="/usr/share/man"
        		PMANDIR="/usr/share/man"
        		;;
        	IRIX*)
        		# SGI IRIX
        		mandir="/usr/share/catman/u_man"
        		AMANDIR="/usr/share/catman/a_man"
        		PMANDIR="/usr/share/catman/p_man"
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
	*BSD* | Darwin*)
		# *BSD
		CAT1EXT=0
		CAT3EXT=0
		CAT5EXT=0
		CAT8EXT=0
		MAN8EXT=8
		MAN8DIR=8
		;;
	IRIX*)
		# SGI IRIX
		CAT1EXT=z
		CAT3EXT=z
		CAT5EXT=z
		CAT8EXT=z
		MAN8EXT=1m
		MAN8DIR=1
		;;
	SunOS* | HP-UX*)
		# Solaris and HP-UX
		CAT1EXT=1
		CAT3EXT=3
		CAT5EXT=5
		CAT8EXT=1m
		MAN8EXT=1m
		MAN8DIR=1m
		;;
	*)
		# All others
		CAT1EXT=1
		CAT3EXT=3
		CAT5EXT=5
		CAT8EXT=8
		MAN8EXT=8
		MAN8DIR=8
		;;
esac

AC_SUBST(CAT1EXT)
AC_SUBST(CAT3EXT)
AC_SUBST(CAT5EXT)
AC_SUBST(CAT8EXT)
AC_SUBST(MAN8EXT)
AC_SUBST(MAN8DIR)

dnl
dnl End of "$Id: cups-manpages.m4,v 1.5.2.3 2003/04/14 19:56:02 mike Exp $".
dnl
