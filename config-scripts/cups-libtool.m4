dnl
dnl "$Id: cups-libtool.m4 6649 2007-07-11 21:46:42Z mike $"
dnl
dnl   Libtool stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007-2009 by Apple Inc.
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_ENABLE(libtool_unsupported, [  --enable-libtool-unsupported
                          build with libtool (UNSUPPORTED!)],
	[if test x$enable_libtool_unsupported != xno; then
		LIBTOOL="$enable_libtool_unsupported"
		enable_shared=no
		echo "WARNING: libtool is not supported or endorsed by Apple Inc."
		echo "         WE DO NOT PROVIDE SUPPORT FOR LIBTOOL PROBLEMS."
	else
		LIBTOOL=""
	fi])

AC_SUBST(LIBTOOL)

if test x$LIBTOOL != x; then
	LIBCUPS="libcups.la"
	LIBCUPSIMAGE="libcupsimage.la"
	LINKCUPS="../cups/\$(LIBCUPS)"
	LINKCUPSIMAGE="../filter/\$(LIBCUPSIMAGE)"
	DSO="\$(CC)"
fi

dnl
dnl End of "$Id: cups-libtool.m4 6649 2007-07-11 21:46:42Z mike $".
dnl
