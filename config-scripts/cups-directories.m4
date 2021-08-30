dnl
dnl Directory stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2017 by Apple Inc.
dnl Copyright © 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
dnl

AC_PREFIX_DEFAULT(/)

dnl Fix "prefix" variable if it hasn't been specified...
AS_IF([test "$prefix" = "NONE"], [
    prefix="/"
])

dnl Fix "exec_prefix" variable if it hasn't been specified...
AS_IF([test "$exec_prefix" = "NONE"], [
    AS_IF([test "$prefix" = "/"], [
	exec_prefix="/usr"
    ], [
	exec_prefix="$prefix"
    ])
])

dnl Fix "bindir" variable...
AS_IF([test "$bindir" = "\${exec_prefix}/bin"], [
    bindir="$exec_prefix/bin"
])

AC_DEFINE_UNQUOTED([CUPS_BINDIR], ["$bindir"], [Location of CUPS user programs.])

dnl Fix "sbindir" variable...
AS_IF([test "$sbindir" = "\${exec_prefix}/sbin"], [
    sbindir="$exec_prefix/sbin"
])

AC_DEFINE_UNQUOTED([CUPS_SBINDIR], ["$sbindir"], [Location of CUPS admin programs.])

dnl Fix "datarootdir" variable if it hasn't been specified...
AS_IF([test "$datarootdir" = "\${prefix}/share"], [
    AS_IF([test "$prefix" = "/"], [
	datarootdir="/usr/share"
    ], [
	datarootdir="$prefix/share"
    ])
])

dnl Fix "datadir" variable if it hasn't been specified...
AS_IF([test "$datadir" = "\${prefix}/share"], [
    AS_IF([test "$prefix" = "/"], [
	datadir="/usr/share"
    ], [
	datadir="$prefix/share"
    ])
], [test "$datadir" = "\${datarootdir}"], [
    datadir="$datarootdir"
])

dnl Fix "includedir" variable if it hasn't been specified...
AS_IF([test "$includedir" = "\${prefix}/include" -a "$prefix" = "/"], [
    includedir="/usr/include"
])
AS_IF([test "$includedir" != "/usr/include"], [
    PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS -I$includedir"
])

dnl Fix "localstatedir" variable if it hasn't been specified...
AS_IF([test "$localstatedir" = "\${prefix}/var"], [
    AS_IF([test "$prefix" = "/"], [
	AS_IF([test "$host_os_name" = darwin], [
	    localstatedir="/private/var"
	], [
	    localstatedir="/var"
	])
    ], [
	localstatedir="$prefix/var"
    ])
])

dnl Fix "sysconfdir" variable if it hasn't been specified...
AS_IF([test "$sysconfdir" = "\${prefix}/etc"], [
    AS_IF([test "$prefix" = "/"], [
	AS_IF([test "$host_os_name" = darwin], [
	    sysconfdir="/private/etc"
	], [
	    sysconfdir="/etc"
	])
    ], [
	sysconfdir="$prefix/etc"
    ])
])

dnl Fix "libdir" variable...
AS_IF([test "$libdir" = "\${exec_prefix}/lib"], [
    AS_CASE(["$host_os_name"], [linux*], [
	AS_IF([test -d /usr/lib64 -a ! -d /usr/lib64/fakeroot], [
	    libdir="$exec_prefix/lib64"
	], [
	    libdir="$exec_prefix/lib"
	])
    ], [*], [
	libdir="$exec_prefix/lib"
    ])
])
AS_IF([test "$libdir" = "/usr/lib"], [
    PKGCONFIG_LIBS="-lcups"
], [
    PKGCONFIG_LIBS="-L$libdir -lcups"
])

dnl Setup default locations...
# Cache data...
AC_ARG_WITH([cachedir], AS_HELP_STRING([--with-cachedir], [set path for cache files]), [
    cachedir="$withval"
], [
    cachedir=""
])

AS_IF([test x$cachedir = x], [
    AS_IF([test "x$host_os_name" = xdarwin], [
	CUPS_CACHEDIR="$localstatedir/spool/cups/cache"
    ], [
	CUPS_CACHEDIR="$localstatedir/cache/cups"
    ])
], [
    CUPS_CACHEDIR="$cachedir"
])
AC_DEFINE_UNQUOTED([CUPS_CACHEDIR], ["$CUPS_CACHEDIR"], [Location of cache files.])
AC_SUBST([CUPS_CACHEDIR])

# Data files
CUPS_DATADIR="$datadir/cups"
AC_DEFINE_UNQUOTED([CUPS_DATADIR], ["$datadir/cups"], [Location of data files.])
AC_SUBST([CUPS_DATADIR])

# Icon directory
AC_ARG_WITH([icondir], AS_HELP_STRING([--with-icondir], [set path for application icons]), [
    icondir="$withval"
], [
    icondir=""
])

AS_IF([test "x$icondir" = x], [
    ICONDIR="/usr/share/icons"
], [
    ICONDIR="$icondir"
])

AC_SUBST([ICONDIR])

# Menu directory
AC_ARG_WITH([menudir], AS_HELP_STRING([--with-menudir], [set path for application menus]), [
    menudir="$withval"
], [
    menudir=""
])

AS_IF([test "x$menudir" = x], [
    MENUDIR="/usr/share/applications"
], [
    MENUDIR="$menudir"
])

AC_SUBST([MENUDIR])

# Documentation files
AC_ARG_WITH([docdir], AS_HELP_STRING([--with-docdir], [set path for documentation]), [
    docdir="$withval"
], [
    docdir=""
])

AS_IF([test x$docdir = x], [
    CUPS_DOCROOT="$datadir/doc/cups"
    docdir="$datadir/doc/cups"
], [
    CUPS_DOCROOT="$docdir"
])

AC_DEFINE_UNQUOTED([CUPS_DOCROOT], ["$docdir"], [Location of documentation files.])
AC_SUBST([CUPS_DOCROOT])

# Locale data
AS_IF([test "$localedir" = "\${datarootdir}/locale"], [
    AS_CASE(["$host_os_name"], [linux* | gnu* | *bsd* | darwin*], [
	CUPS_LOCALEDIR="$datarootdir/locale"
    ], [*], [
	# This is the standard System V location...
	CUPS_LOCALEDIR="$exec_prefix/lib/locale"
    ])
], [
    CUPS_LOCALEDIR="$localedir"
])

AC_DEFINE_UNQUOTED([CUPS_LOCALEDIR], ["$CUPS_LOCALEDIR"], [Location of localization files.])
AC_SUBST([CUPS_LOCALEDIR])


# cups.pc file...
AC_ARG_WITH([pkgconfpath], AS_HELP_STRING([--with-pkgconfpath], [set path for cups.pc file]), [
    pkgconfpath="$withval"
], [
    pkgconfpath=""
])

AS_IF([test x$pkgconfpath = x], [
    CUPS_PKGCONFPATH="$exec_prefix/lib/pkgconfig"
], [
    CUPS_PKGCONFPATH="$pkgconfpath"
])
AC_DEFINE_UNQUOTED([CUPS_PKGCONFPATH], ["$CUPS_PKGCONFPATH"], [Location of cups.pc file.])
AC_SUBST([CUPS_PKGCONFPATH])



# Log files...
AC_ARG_WITH([logdir], AS_HELP_STRING([--with-logdir], [set path for log files]), [
    logdir="$withval"
], [
    logdir=""
])

AS_IF([test x$logdir = x], [
    CUPS_LOGDIR="$localstatedir/log/cups"
], [
    CUPS_LOGDIR="$logdir"
])
AC_DEFINE_UNQUOTED([CUPS_LOGDIR], ["$CUPS_LOGDIR"], [Location of log files.])
AC_SUBST([CUPS_LOGDIR])

# Longer-term spool data
CUPS_REQUESTS="$localstatedir/spool/cups"
AC_DEFINE_UNQUOTED([CUPS_REQUESTS], ["$localstatedir/spool/cups"], [Location of spool directory.])
AC_SUBST([CUPS_REQUESTS])

# Server executables...
AS_CASE(["$host_os_name"], [*-gnu], [
    # GNUs
    INSTALL_SYSV="install-sysv"
    CUPS_SERVERBIN="$exec_prefix/lib/cups"
], [*bsd* | darwin*], [
    # *BSD and Darwin (macOS)
    INSTALL_SYSV=""
    CUPS_SERVERBIN="$exec_prefix/libexec/cups"
], [*], [
    # All others
    INSTALL_SYSV="install-sysv"
    CUPS_SERVERBIN="$exec_prefix/lib/cups"
])

AC_DEFINE_UNQUOTED([CUPS_SERVERBIN], ["$CUPS_SERVERBIN"], [Location of server programs.])
AC_SUBST([CUPS_SERVERBIN])
AC_SUBST([INSTALL_SYSV])

# Configuration files
CUPS_SERVERROOT="$sysconfdir/cups"
AC_DEFINE_UNQUOTED([CUPS_SERVERROOT], ["$sysconfdir/cups"], [Location of server configuration files.])
AC_SUBST([CUPS_SERVERROOT])

# Transient run-time state
AC_ARG_WITH([rundir], AS_HELP_STRING([--with-rundir], [set transient run-time state directory]), [
    CUPS_STATEDIR="$withval"
], [
    AS_CASE(["$host_os_name"], [darwin*], [
	# Darwin (macOS)
	CUPS_STATEDIR="$CUPS_SERVERROOT"
    ], [*], [
	# All others
	CUPS_STATEDIR="$localstatedir/run/cups"
    ])
])
AC_DEFINE_UNQUOTED([CUPS_STATEDIR], ["$CUPS_STATEDIR"], [Location of transient state files.])
AC_SUBST([CUPS_STATEDIR])
