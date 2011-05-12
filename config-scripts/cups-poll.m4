dnl
dnl "$Id$"
dnl
dnl   Select/poll stuff for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
dnl   Copyright 2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_CHECK_FUNC(poll, AC_DEFINE(HAVE_POLL))
AC_CHECK_FUNC(epoll_create, AC_DEFINE(HAVE_EPOLL))
AC_CHECK_FUNC(kqueue, AC_DEFINE(HAVE_KQUEUE))

dnl
dnl End of "$Id$".
dnl
