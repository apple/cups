dnl
dnl "$Id: cups-opsys.m4,v 1.5.2.7 2004/02/26 16:59:02 mike Exp $"
dnl
dnl   Operating system stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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

dnl Get the operating system and version number...
uname=`uname`
uversion=`uname -r | sed -e '1,$s/[[^0-9]]//g'`
if test x$uname = xIRIX64; then
	uname="IRIX"
fi

dnl Determine the correct username and group for this OS...
AC_ARG_WITH(cups-user, [  --with-cups-user        set default user for CUPS],
	CUPS_USER="$withval",
	AC_MSG_CHECKING(for default print user)
	if test -f /etc/passwd; then
		CUPS_USER=""
		for user in lp lpd guest daemon nobody; do
			if test "`grep \^${user}: /etc/passwd`" != ""; then
				CUPS_USER="$user"
				AC_MSG_RESULT($user)
				break;
			fi
		done

		if test x$CUPS_USER = x; then
			CUPS_USER="${USER:=nobody}"
			AC_MSG_RESULT(not found, using "$CUPS_USER")
		fi
	else
		CUPS_USER="${USER:=nobody}"
		AC_MSG_RESULT(no password file, using "$CUPS_USER")
	fi)

AC_ARG_WITH(cups-group, [  --with-cups-group       set default group for CUPS],
	CUPS_GROUP="$withval",
	AC_MSG_CHECKING(for default print group)
	if test -f /etc/group; then
		if test x$uname = xDarwin; then
			GROUP_LIST="lp admin"
		else
			GROUP_LIST="sys system root"
		fi

		CUPS_GROUP=""
		for group in $GROUP_LIST; do
			if test "`grep \^${group}: /etc/group`" != ""; then
				CUPS_GROUP="$group"
				AC_MSG_RESULT($group)
				break;
			fi
		done

		if test x$CUPS_GROUP = x; then
			CUPS_GROUP="${GROUP:=nobody}"
			AC_MSG_RESULT(not found, using "$CUPS_GROUP")
		fi
	else
		CUPS_GROUP="${GROUP:=nobody}"
		AC_MSG_RESULT(no group file, using "$CUPS_GROUP")
	fi)

AC_SUBST(CUPS_USER)
AC_SUBST(CUPS_GROUP)

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USER, "$CUPS_USER")
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_GROUP, "$CUPS_GROUP")

dnl
dnl "$Id: cups-opsys.m4,v 1.5.2.7 2004/02/26 16:59:02 mike Exp $"
dnl
