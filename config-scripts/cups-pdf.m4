dnl
dnl "$Id: cups-pdf.m4 7449 2008-04-14 18:27:53Z mike $"
dnl
dnl   PDF filter configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 2007-2009 by Apple Inc.
dnl   Copyright 2006 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

AC_ARG_WITH(pdftops, [  --with-pdftops          set pdftops filter (gs,/path/to/gs,pdftops,/path/to/pdftops,none), default=pdftops ])

PDFTOPS=""
CUPS_PDFTOPS=""
CUPS_GHOSTSCRIPT=""

case "x$with_pdftops" in
	x) # Default/auto
	if test $uname != Darwin; then
		AC_PATH_PROG(CUPS_PDFTOPS, pdftops)
		if test "x$CUPS_PDFTOPS" != x; then
			AC_DEFINE(HAVE_PDFTOPS)
			PDFTOPS="pdftops"
		else
			AC_PATH_PROG(CUPS_GHOSTSCRIPT, gs)
			if test "x$CUPS_GHOSTSCRIPT" != x; then
				AC_DEFINE(HAVE_GHOSTSCRIPT)
				PDFTOPS="pdftops"
			fi
		fi
	fi
	;;

	xgs)
	AC_PATH_PROG(CUPS_GHOSTSCRIPT, gs)
	if test "x$CUPS_GHOSTSCRIPT" != x; then
		AC_DEFINE(HAVE_GHOSTSCRIPT)
		PDFTOPS="pdftops"
	else
		AC_MSG_ERROR(Unable to find gs program!)
		exit 1
	fi
	;;

	x/*/gs) # Use /path/to/gs without any check:
	CUPS_GHOSTSCRIPT="$with_pdftops"
	AC_DEFINE(HAVE_GHOSTSCRIPT)
	PDFTOPS="pdftops"
	;;

	xpdftops)
	AC_PATH_PROG(CUPS_PDFTOPS, pdftops)
	if test "x$CUPS_PDFTOPS" != x; then
		AC_DEFINE(HAVE_PDFTOPS)
		PDFTOPS="pdftops"
	else
		AC_MSG_ERROR(Unable to find pdftops program!)
		exit 1
	fi
	;;

	x/*/pdftops) # Use /path/to/pdftops without any check:
	CUPS_PDFTOPS="$with_pdftops"
	AC_DEFINE(HAVE_PDFTOPS)
	PDFTOPS="pdftops"
	;;

	xnone) # Make no pdftops filter if with_pdftops=none:
	;;

	*) # Invalid with_pdftops value:
	AC_MSG_ERROR(Invalid with_pdftops value!)
	exit 1
	;;
esac

AC_DEFINE_UNQUOTED(CUPS_PDFTOPS, "$CUPS_PDFTOPS")
AC_DEFINE_UNQUOTED(CUPS_GHOSTSCRIPT, "$CUPS_GHOSTSCRIPT")
AC_SUBST(PDFTOPS)

dnl
dnl End of "$Id: cups-pdf.m4 7449 2008-04-14 18:27:53Z mike $".
dnl
