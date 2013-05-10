dnl
dnl "$Id: cups-pam.m4 5466 2006-04-26 19:52:27Z mike $"
dnl
dnl   PAP (AppleTalk) stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007 by Apple Inc.
dnl   Copyright 2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
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
