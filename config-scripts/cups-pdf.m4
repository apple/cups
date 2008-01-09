dnl
dnl "$Id$"
dnl
dnl   PDF filter configuration stuff for the Common UNIX Printing System (CUPS).
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

AC_ARG_ENABLE(pdftops, [  --enable-pdftops        build pdftops filter, default=auto ])

PDFTOPS=""

if test "x$enable_pdftops" != xno; then
	AC_PATH_PROG(CUPS_PDFTOPS, pdftops)
	AC_DEFINE_UNQUOTED(CUPS_PDFTOPS, "$CUPS_PDFTOPS")

	if test "x$CUPS_PDFTOPS" != x; then
		AC_MSG_CHECKING(whether to build pdftops filter)
		if test x$enable_pdftops = xyes -o $uname != Darwin; then
			PDFTOPS="pdftops"
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
		fi
	elif test x$enable_pdftops = xyes; then
		AC_MSG_ERROR(Unable to find pdftops program!)
		exit 1
	fi
fi

AC_SUBST(PDFTOPS)

dnl
dnl End of "$Id$".
dnl
