dnl
dnl "$Id: cups-opsys.m4,v 1.1 2001/06/27 19:06:46 mike Exp $"
dnl
dnl   Operating system stuff for the Common UNIX Printing System (CUPS).
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

dnl Get the operating system and version number...

uname=`uname`
uversion=`uname -r | sed -e '1,$s/[[^0-9]]//g'`
if test x$uname = xIRIX64; then
	uname="IRIX"
fi

dnl
dnl "$Id: cups-opsys.m4,v 1.1 2001/06/27 19:06:46 mike Exp $"
dnl
