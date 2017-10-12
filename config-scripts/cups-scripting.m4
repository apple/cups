dnl
dnl Scripting configuration stuff for CUPS.
dnl
dnl Copyright 2007-2017 by Apple Inc.
dnl Copyright 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl These coded instructions, statements, and computer programs are the
dnl property of Apple Inc. and are protected by Federal copyright
dnl law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl which should have been included with this file.  If this file is
dnl missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl Do we have Java?
AC_ARG_WITH(java, [  --with-java             set Java interpreter for web interfaces ],
	CUPS_JAVA="$withval",
	CUPS_JAVA="auto")

if test "x$CUPS_JAVA" = xauto; then
	AC_PATH_PROG(JAVA,java)
	CUPS_JAVA="$JAVA"
elif test "x$CUPS_JAVA" = xno; then
        CUPS_JAVA=""
fi

AC_DEFINE_UNQUOTED(CUPS_JAVA, "$CUPS_JAVA")

if test "x$CUPS_JAVA" != x; then
	AC_DEFINE(HAVE_JAVA)
fi

dnl Do we have Perl?
AC_ARG_WITH(perl, [  --with-perl             set Perl interpreter for web interfaces ],
	CUPS_PERL="$withval",
	CUPS_PERL="auto")

if test "x$CUPS_PERL" = xauto; then
	AC_PATH_PROG(PERL,perl)
	CUPS_PERL="$PERL"
elif test "x$CUPS_PERL" = xno; then
        CUPS_PERL=""
fi

AC_DEFINE_UNQUOTED(CUPS_PERL, "$CUPS_PERL")

if test "x$CUPS_PERL" != x; then
	AC_DEFINE(HAVE_PERL)
fi

dnl Do we have PHP?
AC_ARG_WITH(php, [  --with-php              set PHP interpreter for web interfaces ],
	CUPS_PHP="$withval",
	CUPS_PHP="auto")

if test "x$CUPS_PHP" = xauto; then
	AC_PATH_PROG(PHPCGI,php-cgi)
	if test "x$PHPCGI" = x; then
		AC_PATH_PROG(PHP,php)
		CUPS_PHP="$PHP"
	else
		CUPS_PHP="$PHPCGI"
	fi
elif test "x$CUPS_PHP" = xno; then
        CUPS_PHP=""
fi

AC_DEFINE_UNQUOTED(CUPS_PHP, "$CUPS_PHP")

if test "x$CUPS_PHP" = x; then
	CUPS_PHP="no"
else
	AC_DEFINE(HAVE_PHP)
fi

dnl Do we have Python?
AC_ARG_WITH(python, [  --with-python           set Python interpreter for web interfaces ],
	CUPS_PYTHON="$withval",
	CUPS_PYTHON="auto")

if test "x$CUPS_PYTHON" = xauto; then
	AC_PATH_PROG(PYTHON,python)
	CUPS_PYTHON="$PYTHON"
elif test "x$CUPS_PYTHON" = xno; then
        CUPS_PYTHON=""
fi

AC_DEFINE_UNQUOTED(CUPS_PYTHON, "$CUPS_PYTHON")

if test "x$CUPS_PYTHON" != x; then
	AC_DEFINE(HAVE_PYTHON)
fi
