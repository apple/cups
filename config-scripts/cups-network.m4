dnl
dnl "$Id: cups-network.m4,v 1.1.2.12 2004/06/29 03:46:29 mike Exp $"
dnl
dnl   Networking stuff for the Common UNIX Printing System (CUPS).
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
dnl       Hollywood, Maryland 20636-3142 USA
dnl
dnl       Voice: (301) 373-9600
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

NETLIBS=""

if test "$uname" != "IRIX"; then
	AC_CHECK_LIB(socket,socket,NETLIBS="-lsocket")
	AC_CHECK_LIB(nsl,gethostbyaddr,NETLIBS="$NETLIBS -lnsl")
fi

AC_CHECK_FUNCS(rresvport getifaddrs hstrerror)

AC_CHECK_MEMBER(struct sockaddr.sa_len,,,[#include <sys/socket.h>])
AC_CHECK_HEADER(sys/sockio.h,AC_DEFINE(HAVE_SYS_SOCKIO_H))

AC_SUBST(NETLIBS)

if test "$uname" = "SunOS"; then
	case "$uversion" in
		55* | 56*)
			maxfiles=1024
			;;
		*)
			maxfiles=4096
			;;
	esac
else
	maxfiles=4096
fi

AC_ARG_WITH(maxfiles, [  --with-maxfiles=N       set maximum number of file descriptors for scheduler ],
	maxfiles=$withval)

AC_DEFINE_UNQUOTED(CUPS_MAX_FDS, $maxfiles)

dnl
dnl End of "$Id: cups-network.m4,v 1.1.2.12 2004/06/29 03:46:29 mike Exp $".
dnl
