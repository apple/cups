dnl
dnl "$Id: cups-scripting.m4,v 1.1.2.1 2003/03/14 18:10:40 mike Exp $"
dnl
dnl   Scripting configuration stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2003 by Easy Software Products, all rights reserved.
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

dnl Do we have Java?
AC_ARG_WITH(java, [  --with-java             set Java interpreter for web interfaces ],
	CUPS_JAVA="$withval",
	CUPS_JAVA="")

if test "x$CUPS_JAVA" = x; then
	AC_PATH_PROG(JAVA,java)
	CUPS_JAVA="$JAVA"
fi

AC_DEFINE_UNQUOTED(CUPS_JAVA, "$CUPS_JAVA")

if test "x$CUPS_JAVA" != x; then
	AC_DEFINE(HAVE_JAVA)
fi

dnl Do we have Perl?
AC_ARG_WITH(perl, [  --with-perl             set Perl interpreter for web interfaces ],
	CUPS_PERL="$withval",
	CUPS_PERL="")

if test "x$CUPS_PERL" = x; then
	AC_PATH_PROG(PERL,perl)
	CUPS_PERL="$PERL"
fi

AC_DEFINE_UNQUOTED(CUPS_PERL, "$CUPS_PERL")

if test "x$CUPS_PERL" != x; then
	AC_DEFINE(HAVE_PERL)
fi

dnl Do we have PHP?
AC_ARG_WITH(php, [  --with-php              set PHP interpreter for web interfaces ],
	CUPS_PHP="$withval",
	CUPS_PHP="")

if test "x$CUPS_PHP" = x; then
	AC_PATH_PROG(PHP,php)
	CUPS_PHP="$PHP"
fi

AC_DEFINE_UNQUOTED(CUPS_PHP, "$CUPS_PHP")

if test "x$CUPS_PHP" != x; then
	AC_DEFINE(HAVE_PHP)
fi

dnl Do we have Python?
AC_ARG_WITH(python, [  --with-python           set Python interpreter for web interfaces ],
	CUPS_PYTHON="$withval",
	CUPS_PYTHON="")

if test "x$CUPS_PYTHON" = x; then
	AC_PATH_PROG(PYTHON,python)
	CUPS_PYTHON="$PYTHON"
fi

AC_DEFINE_UNQUOTED(CUPS_PYTHON, "$CUPS_PYTHON")

if test "x$CUPS_PYTHON" != x; then
	AC_DEFINE(HAVE_PYTHON)
fi

dnl
dnl End of "$Id: cups-scripting.m4,v 1.1.2.1 2003/03/14 18:10:40 mike Exp $".
dnl
