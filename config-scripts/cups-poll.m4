dnl
dnl "$Id$"
dnl
dnl   Select/poll stuff for the Common UNIX Printing System (CUPS).
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

AC_CHECK_FUNC(poll, AC_DEFINE(HAVE_POLL))
AC_CHECK_FUNC(epoll_create, AC_DEFINE(HAVE_EPOLL))
AC_CHECK_FUNC(kqueue, AC_DEFINE(HAVE_KQUEUE))

dnl
dnl End of "$Id$".
dnl
