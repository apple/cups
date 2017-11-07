dnl
dnl Select/poll stuff for CUPS.
dnl
dnl Copyright 2007-2011 by Apple Inc.
dnl Copyright 2006 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
dnl

AC_CHECK_FUNC(poll, AC_DEFINE(HAVE_POLL))
AC_CHECK_FUNC(epoll_create, AC_DEFINE(HAVE_EPOLL))
AC_CHECK_FUNC(kqueue, AC_DEFINE(HAVE_KQUEUE))
