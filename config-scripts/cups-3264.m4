dnl
dnl "$Id$"
dnl
dnl   32/64-bit library support stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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

dnl Setup support for separate 32/64-bit library generation...
AC_ARG_ENABLE(32bit, [  --enable-32bit          generate 32-bit libraries on 32/64-bit systems, default=no])

INSTALL32=""
LIB32CUPS=""
LIB32CUPSIMAGE=""
LIB32DIR=""
UNINSTALL32=""

AC_SUBST(INSTALL32)
AC_SUBST(LIB32CUPS)
AC_SUBST(LIB32CUPSIMAGE)
AC_SUBST(LIB32DIR)
AC_SUBST(UNINSTALL32)

AC_ARG_ENABLE(64bit, [  --enable-64bit          generate 64-bit libraries on 32/64-bit systems, default=no])

INSTALL64=""
LIB64CUPS=""
LIB64CUPSIMAGE=""
LIB64DIR=""
UNINSTALL64=""

AC_SUBST(INSTALL64)
AC_SUBST(LIB64CUPS)
AC_SUBST(LIB64CUPSIMAGE)
AC_SUBST(LIB64DIR)
AC_SUBST(UNINSTALL64)

case "$uname" in
	HP-UX*)
		if test "x$enable_32bit" = xyes; then
			# Build 32-bit libraries, 64-bit base...
			INSTALL32="install32bit"
			LIB32CUPS="32bit/libcups.so.2"
			LIB32CUPSIMAGE="32bit/libcupsimage.so.2"
			LIB32DIR="$exec_prefix/lib"
			if test -d /usr/lib/hpux32; then
				LIB32DIR="${LIB32DIR}/hpux32"
			fi
			UNINSTALL32="uninstall32bit"
		fi

		if test "x$enable_64bit" = xyes; then
			# Build 64-bit libraries, 32-bit base...
			INSTALL64="install64bit"
			LIB64CUPS="64bit/libcups.so.2"
			LIB64CUPSIMAGE="64bit/libcupsimage.so.2"
			LIB64DIR="$exec_prefix/lib"
			if test -d /usr/lib/hpux64; then
				LIB64DIR="${LIB64DIR}/hpux64"
			fi
			UNINSTALL64="uninstall64bit"
		fi
		;;

	IRIX)
		if test "x$enable_32bit" = xyes; then
			INSTALL32="install32bit"
			LIB32CUPS="32bit/libcups.so.2"
			LIB32CUPSIMAGE="32bit/libcupsimage.so.2"
			LIB32DIR="$prefix/lib32"
			UNINSTALL32="uninstall32bit"
		fi

		if test "x$enable_64bit" = xyes; then
			# Build 64-bit libraries, 32-bit base...
			INSTALL64="install64bit"
			LIB64CUPS="64bit/libcups.so.2"
			LIB64CUPSIMAGE="64bit/libcupsimage.so.2"
			LIB64DIR="$prefix/lib64"
			UNINSTALL64="uninstall64bit"
		fi
		;;

	Linux*)
		if test "x$enable_32bit" = xyes; then
			# Build 32-bit libraries, 64-bit base...
			INSTALL32="install32bit"
			LIB32CUPS="32bit/libcups.so.2"
			LIB32CUPSIMAGE="32bit/libcupsimage.so.2"
			LIB32DIR="$exec_prefix/lib"
			if test -d /usr/lib32; then
				LIB32DIR="${LIB32DIR}32"
			fi
			UNINSTALL32="uninstall32bit"
		fi

		if test "x$enable_64bit" = xyes; then
			# Build 64-bit libraries, 32-bit base...
			INSTALL64="install64bit"
			LIB64CUPS="64bit/libcups.so.2"
			LIB64CUPSIMAGE="64bit/libcupsimage.so.2"
			LIB64DIR="$exec_prefix/lib"
			if test -d /usr/lib64; then
				LIB64DIR="${LIB64DIR}64"
			fi
			UNINSTALL64="uninstall64bit"
		fi
		;;

	SunOS*)
		if test "x$enable_32bit" = xyes; then
			# Build 32-bit libraries, 64-bit base...
			INSTALL32="install32bit"
			LIB32CUPS="32bit/libcups.so.2"
			LIB32CUPSIMAGE="32bit/libcupsimage.so.2"
			LIB32DIR="$exec_prefix/lib/32"
			UNINSTALL32="uninstall32bit"
		fi

		if test "x$enable_64bit" = xyes; then
			# Build 64-bit libraries, 32-bit base...
			INSTALL64="install64bit"
			LIB64CUPS="64bit/libcups.so.2"
			LIB64CUPSIMAGE="64bit/libcupsimage.so.2"
			LIB64DIR="$exec_prefix/lib/64"
			UNINSTALL64="uninstall64bit"
		fi
		;;
esac

dnl
dnl End of "$Id$".
dnl
