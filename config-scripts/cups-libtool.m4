dnl
dnl "$Id: cups-libtool.m4,v 1.2 2001/08/06 19:37:09 mike Exp $"
dnl
dnl   Libtool stuff for the Common UNIX Printing System (CUPS).
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

AC_ARG_ENABLE(libtool_unsupported, [  --enable-libtool-unsupported=LIBTOOL_PATH
                          turn on building with libtool (UNSUPPORTED!) [default=no]],
	[if test x$enable_libtool_unsupported != xno; then
		LIBTOOL="$enable_libtool_unsupported"
		enable_shared=no
		echo "WARNING: libtool is not supported or endorsed by Easy Software Products."
		echo "         WE DO NOT PROVIDE TECHNICAL SUPPORT FOR LIBTOOL PROBLEMS."
		echo "         (even if you have a support contract)"
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
dnl End of "$Id: cups-libtool.m4,v 1.2 2001/08/06 19:37:09 mike Exp $".
dnl
