dnl
dnl "$Id: cups-opsys.m4 11324 2013-10-04 03:11:42Z msweet $"
dnl
dnl   Operating system stuff for CUPS.
dnl
dnl   Copyright 2007-2012 by Apple Inc.
dnl   Copyright 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl Get the operating system, version number, and architecture...
uname=`uname`
uversion=`uname -r | sed -e '1,$s/^[[^0-9]]*\([[0-9]]*\)\.\([[0-9]]*\).*/\1\2/'`
uarch=`uname -m`

case "$uname" in
	Darwin*)
		uname="Darwin"
		if test $uversion -lt 120; then
			AC_MSG_ERROR([Sorry, this version of CUPS requires OS X 10.8 or higher.])
		fi
		;;

	GNU* | GNU/*)
		uname="GNU"
		;;
	Linux*)
		uname="Linux"
		;;
esac

dnl
dnl "$Id: cups-opsys.m4 11324 2013-10-04 03:11:42Z msweet $"
dnl
