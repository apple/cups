dnl
dnl Launch-on-demand/startup stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2017 by Apple Inc.
dnl Copyright © 1997-2005 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

ONDEMANDFLAGS=""
ONDEMANDLIBS=""
AC_SUBST([ONDEMANDFLAGS])
AC_SUBST([ONDEMANDLIBS])

AC_ARG_WITH([ondemand], AS_HELP_STRING([--with-ondemand=...], [Specify the on-demand launch interface (launchd, systemd, upstart)]))

AS_IF([test "x$with_ondemand" = x], [
    AS_IF([test $host_os_name = darwin], [
        with_ondemand="launchd"
    ], [
	with_ondemand="yes"
    ])
], [test "x$with_ondemand" = xyes -a $host_os_name = darwin], [
    with_ondemand="launchd"
], [test "x$with_ondemand" != xno -a "x$with_ondemand" != xsystemd -a "x$with_ondemand" != xupstart -a "x$with_ondemand" != xyes], [
    AC_MSG_ERROR([Unknown --with-ondemand value "$with_ondemand" specified.])
])

dnl Launchd is used on macOS/Darwin...
LAUNCHD_DIR=""
AC_SUBST([LAUNCHD_DIR])

AS_IF([test $with_ondemand = launchd], [
    AC_CHECK_FUNC([launch_activate_socket], [
	AC_DEFINE([HAVE_LAUNCHD], [1], [Have launchd support?])
	AC_DEFINE([HAVE_ONDEMAND], [1], [Have on-demand launch support?])
        with_ondemand="launchd"
    ], [
        AS_IF([test $with_ondemand = launchd], [
            AC_MSG_ERROR([Need launch_activate_socket/liblaunch for launchd support.])
        ])
    ])
    AC_CHECK_HEADER([launch.h], [
        AC_DEFINE([HAVE_LAUNCH_H], [1], [Have <launch.h> header?])
    ])

    AS_IF([test $host_os_name = darwin], [
	LAUNCHD_DIR="/System/Library/LaunchDaemons"
	# liblaunch is already part of libSystem
    ])
])

dnl Systemd is used on Linux...
AC_ARG_WITH([systemd], AS_HELP_STRING([--with-systemd], [set directory for systemd service files]), [
    SYSTEMD_DIR="$withval"
], [
    SYSTEMD_DIR=""
])
AC_SUBST([SYSTEMD_DIR])

AS_IF([test $with_ondemand = systemd -o $with_ondemand = yes], [
    AS_IF([test "x$PKGCONFIG" = x], [
	AS_IF([test $with_ondemand = systemd], [
	    AC_MSG_ERROR([Need pkg-config to enable systemd support.])
	])
    ], [
	AC_MSG_CHECKING([for libsystemd])
	have_systemd="no"
	AS_IF([$PKGCONFIG --exists libsystemd], [
	    AC_MSG_RESULT([yes])
	    have_systemd="yes"
	    with_ondemand="systemd"
	    ONDEMANDFLAGS="$($PKGCONFIG --cflags libsystemd)"
	    ONDEMANDLIBS="$($PKGCONFIG --libs libsystemd)"
	], [$PKGCONFIG --exists libsystemd-daemon], [
	    AC_MSG_RESULT([yes - legacy])
	    have_systemd="yes"
	    with_ondemand="systemd"
	    ONDEMANDFLAGS="$($PKGCONFIG --cflags libsystemd-daemon)"
	    ONDEMANDLIBS="$($PKGCONFIG --libs libsystemd-daemon)"

	    AS_IF([$PKGCONFIG --exists libsystemd-journal], [
		ONDEMANDFLAGS="$ONDEMANDFLAGS $($PKGCONFIG --cflags libsystemd-journal)"
		ONDEMANDLIBS="$ONDEMANDLIBS $($PKGCONFIG --libs libsystemd-journal)"
	    ])
	], [
	    AC_MSG_RESULT([no])
	])

	AS_IF([test $have_systemd = yes], [
	    AC_DEFINE([HAVE_SYSTEMD], [1], [Have systemd support?])
	    AC_DEFINE([HAVE_ONDEMAND], [1], [Have on-demand launch support?])
	    AC_CHECK_HEADER([systemd/sd-journal.h], [
	        AC_DEFINE([HAVE_SYSTEMD_SD_JOURNAL_H], [1], [Have <systemd/sd-journal.h> header?])
	    ])
	    AS_IF([test "x$SYSTEMD_DIR" = x], [
		SYSTEMD_DIR="$($PKGCONFIG --variable=systemdsystemunitdir systemd)"
	    ])
	])
    ])
])

dnl Upstart is also used on Linux (e.g., ChromeOS)
AS_IF([test $with_ondemand = upstart], [
    AC_DEFINE([HAVE_UPSTART]. [1], [Have upstart support?])
    AC_DEFINE([HAVE_ONDEMAND], [1], [Have on-demand launch support?])
])

dnl Solaris uses smf
AC_ARG_WITH([smfmanifestdir], AS_HELP_STRING([--with-smfmanifestdir], [set path for Solaris SMF manifest]), [
    SMFMANIFESTDIR="$withval"
], [
    SMFMANIFESTDIR=""
])
AC_SUBST([SMFMANIFESTDIR])

dnl Use init on other platforms...
AC_ARG_WITH([rcdir], AS_HELP_STRING([--with-rcdir], [set path for rc scripts]), [
    rcdir="$withval"
], [
    rcdir=""
])
AC_ARG_WITH([rclevels], AS_HELP_STRING([--with-rclevels], [set run levels for rc scripts]), [
    rclevels="$withval"
], [
    rclevels="2 3 5"
])
AC_ARG_WITH([rcstart], AS_HELP_STRING([--with-rcstart], [set start number for rc scripts]), [
    rcstart="$withval"
], [
    rcstart=""
])
AC_ARG_WITH([rcstop], AS_HELP_STRING([--with-rcstop], [set stop number for rc scripts]), [
    rcstop="$withval"
], [
    rcstop=""
])

AS_IF([test "x$rcdir" = x], [
    AS_IF([test "x$LAUNCHD_DIR" = x -a "x$SYSTEMD_DIR" = x -a "x$SMFMANIFESTDIR" = x], [
	# Fall back on "init", the original service startup interface...
	AS_IF([test -d /sbin/init.d], [
	    # SuSE
	    rcdir="/sbin/init.d"
	], [test -d /etc/init.d], [
	    # Others
	    rcdir="/etc"
	], [
	    # RedHat, NetBSD
	    rcdir="/etc/rc.d"
	])
    ], [
	rcdir="no"
    ])
])

AS_IF([test "x$rcstart" = x], [
    AS_CASE(["$host_os_name"], [linux* | gnu*], [
	# Linux
	rcstart="81"
    ], [sunos*], [
	# Solaris
	rcstart="81"
    ], [*], [
	# Others
	rcstart="99"
    ])
])

AS_IF([test "x$rcstop" = x], [
    AS_CASE(["$host_os_name"], [linux* | gnu*], [
	# Linux
	rcstop="36"
    ], [*], [
	# Others
	rcstop="00"
    ])
])

INITDIR=""
INITDDIR=""
RCLEVELS="$rclevels"
RCSTART="$rcstart"
RCSTOP="$rcstop"
AC_SUBST([INITDIR])
AC_SUBST([INITDDIR])
AC_SUBST([RCLEVELS])
AC_SUBST([RCSTART])
AC_SUBST([RCSTOP])

AS_IF([test "x$rcdir" != xno], [
    AS_IF([test "x$rclevels" = x], [
	INITDDIR="$rcdir"
    ], [
	INITDIR="$rcdir"
    ])
])

dnl Xinetd support...
AC_ARG_WITH([xinetd], AS_HELP_STRING([--with-xinetd], [set path for xinetd config files]), [
    xinetd="$withval"
], [
    xinetd=""
])
XINETD=""
AC_SUBST([XINETD])

AS_IF([test "x$xinetd" = x], [
    AS_IF([test ! -x /sbin/launchd], [
	for dir in /etc/xinetd.d /usr/local/etc/xinetd.d; do
	    AS_IF([test -d $dir], [
		XINETD="$dir"
		break
	    ])
	done
    ])
], [test "x$xinetd" != xno], [
    XINETD="$xinetd"
])
