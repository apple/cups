dnl
dnl "$Id: cups-pam.m4 5466 2006-04-26 19:52:27Z mike $"
dnl
dnl   PAP (AppleTalk) stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2006 by Easy Software Products, all rights reserved.
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

# Currently the PAP backend is only supported on MacOS X with the AppleTalk
# SDK installed...
PAP=""
if test $uname = Darwin; then
	PAP="pap"
	AC_CHECK_HEADER(AppleTalk/at_proto.h)
fi

AC_SUBST(PAP)

dnl
dnl End of "$Id: cups-pam.m4 5466 2006-04-26 19:52:27Z mike $".
dnl
