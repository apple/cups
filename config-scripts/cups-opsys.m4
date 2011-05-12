dnl
dnl "$Id: cups-opsys.m4 6649 2007-07-11 21:46:42Z mike $"
dnl
dnl   Operating system stuff for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
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
	GNU* | GNU/*)
		uname="GNU"
		;;
	IRIX*)
		uname="IRIX"
		;;
	Linux*)
		uname="Linux"
		;;
esac

dnl
dnl "$Id: cups-opsys.m4 6649 2007-07-11 21:46:42Z mike $"
dnl
