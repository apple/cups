dnl
dnl Common configuration stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2019 by Apple Inc.
dnl Copyright © 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Set the name of the config header file...
AC_CONFIG_HEADERS([config.h])

dnl Version number information...
CUPS_VERSION="AC_PACKAGE_VERSION"
CUPS_API_VERSION="$(echo AC_PACKAGE_VERSION | awk -F. '{print $1 "." $2}')"
CUPS_BUILD="cups-$CUPS_VERSION"

AC_ARG_WITH([cups_build], AS_HELP_STRING([--with-cups-build], [set "pkg-config --variable=build" string]), [
    CUPS_BUILD="$withval"
])

AC_SUBST([CUPS_API_VERSION])
AC_SUBST([CUPS_BUILD])
AC_SUBST([CUPS_VERSION])
AC_DEFINE_UNQUOTED([CUPS_SVERSION], ["AC_PACKAGE_NAME v$CUPS_VERSION"], [Version number])
AC_DEFINE_UNQUOTED([CUPS_MINIMAL], ["AC_PACKAGE_NAME/$CUPS_VERSION"], [Version for HTTP headers])

dnl Default compiler flags...
CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"

dnl Checks for programs...
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_RANLIB
AC_PATH_PROG([AR], [ar])
AC_PATH_PROG([CHMOD], [chmod])
AC_PATH_PROG([GZIPPROG], [gzip])
AC_MSG_CHECKING([for install-sh script])
INSTALL="`pwd`/install-sh"
AC_SUBST([INSTALL])
AC_MSG_RESULT([using $INSTALL])
AC_PATH_PROG([LD], [ld])
AC_PATH_PROG([LN], [ln])
AC_PATH_PROG([MKDIR], [mkdir])
AC_PATH_PROG([MV], [mv])
AC_PATH_PROG([RM], [rm])
AC_PATH_PROG([RMDIR], [rmdir])
AC_PATH_PROG([SED], [sed])
AC_PATH_PROG([XDGOPEN], [xdg-open])

AS_IF([test "x$XDGOPEN" = x], [
    CUPS_HTMLVIEW="htmlview"
], [
    CUPS_HTMLVIEW="$XDGOPEN"
])
AC_SUBST([CUPS_HTMLVIEW])

AS_IF([test "x$AR" = x], [
    AC_MSG_ERROR([Unable to find required library archive command.])
])
AS_IF([test "x$CC" = x], [
    AC_MSG_ERROR([Unable to find required C compiler command.])
])

dnl Static library option...
INSTALLSTATIC=""
AC_ARG_ENABLE([static], AS_HELP_STRING([--enable-static], [install static libraries]))

AS_IF([test x$enable_static = xyes], [
    AC_MSG_NOTICE([Installing static libraries...])
    INSTALLSTATIC="installstatic"
])

AC_SUBST([INSTALLSTATIC])

dnl Check for pkg-config, which is used for some other tests later on...
AC_PATH_TOOL([PKGCONFIG], [pkg-config])
PKGCONFIG_CFLAGS=""
PKGCONFIG_LIBS=""
PKGCONFIG_LIBS_STATIC=""
PKGCONFIG_REQUIRES=""
AC_SUBST([PKGCONFIG_CFLAGS])
AC_SUBST([PKGCONFIG_LIBS])
AC_SUBST([PKGCONFIG_LIBS_STATIC])
AC_SUBST([PKGCONFIG_REQUIRES])

dnl Check for libraries...
AC_SEARCH_LIBS([abs], [m], [AC_DEFINE(HAVE_ABS)])
AC_SEARCH_LIBS([crypt], [crypt])
AC_SEARCH_LIBS([fmod], [m])
AC_SEARCH_LIBS([getspent], [sec gen])

LIBMALLOC=""
AC_ARG_ENABLE([mallinfo], AS_HELP_STRING([--enable-mallinfo], [build with malloc debug logging]))

AS_IF([test x$enable_mallinfo = xyes], [
    SAVELIBS="$LIBS"
    LIBS=""
    AC_SEARCH_LIBS([mallinfo], [malloc], [AC_DEFINE(HAVE_MALLINFO)])
    LIBMALLOC="$LIBS"
    LIBS="$SAVELIBS"
])

AC_SUBST([LIBMALLOC])

dnl Check for libpaper support...
AC_ARG_ENABLE([libpaper], AS_HELP_STRING([--enable-libpaper], [build with libpaper support]))

AS_IF([test x$enable_libpaper = xyes], [
    AC_CHECK_LIB([paper], [systempapername], [
	AC_DEFINE([HAVE_LIBPAPER], [1], [Have paper library?])
	LIBPAPER="-lpaper"
    ], [
	LIBPAPER=""
    ])
], [
    LIBPAPER=""
])
AC_SUBST([LIBPAPER])

dnl Checks for header files.
AC_CHECK_HEADER([crypt.h], AC_DEFINE([HAVE_CRYPT_H], [1], [Have <crypt.h> header?]))
AC_CHECK_HEADER([langinfo.h], AC_DEFINE([HAVE_LANGINFO_H], [1], [Have <langinfo.h> header?]))
AC_CHECK_HEADER([malloc.h], AC_DEFINE([HAVE_MALLOC_H], [1], [Have <malloc.h> header?]))
AC_CHECK_HEADER([shadow.h], AC_DEFINE([HAVE_SHADOW_H], [1], [Have <shadow.h> header?]))
AC_CHECK_HEADER([stdint.h], AC_DEFINE([HAVE_STDINT_H], [1], [Have <stdint.h> header?]))
AC_CHECK_HEADER([sys/ioctl.h], AC_DEFINE([HAVE_SYS_IOCTL_H], [1], [Have <sys/ioctl.h> header?]))
AC_CHECK_HEADER([sys/param.h], AC_DEFINE([HAVE_SYS_PARAM_H], [1], [Have <sys/param.h> header?]))
AC_CHECK_HEADER([sys/ucred.h], AC_DEFINE([HAVE_SYS_UCRED_H], [1], [Have <sys/ucred.h> header?]))

dnl Checks for iconv.h and iconv_open
AC_CHECK_HEADER([iconv.h], [
    SAVELIBS="$LIBS"
    LIBS=""
    AC_SEARCH_LIBS([iconv_open], [iconv], [
        AC_DEFINE([HAVE_ICONV_H], [1], [Have <iconv.h> header?])
	SAVELIBS="$SAVELIBS $LIBS"
    ])
    AC_SEARCH_LIBS([libiconv_open], [iconv], [
	AC_DEFINE([HAVE_ICONV_H], [1], [Have <iconv.h> header?])
	SAVELIBS="$SAVELIBS $LIBS"
    ])
    LIBS="$SAVELIBS"
])

dnl Checks for statfs and its many headers...
AC_CHECK_HEADER([sys/mount.h], AC_DEFINE([HAVE_SYS_MOUNT_H], [1], [Have <sys/mount.h> header?]))
AC_CHECK_HEADER([sys/statfs.h], AC_DEFINE([HAVE_SYS_STATFS_H], [1], [Have <sys/statfs.h> header?]))
AC_CHECK_HEADER([sys/statvfs.h], AC_DEFINE([HAVE_SYS_STATVFS_H], [1], [Have <sys/statvfs.h> header?]))
AC_CHECK_HEADER([sys/vfs.h], AC_DEFINE([HAVE_SYS_VFS_H], [1], [Have <sys/vfs.h> header?]))
AC_CHECK_FUNCS([statfs statvfs])

dnl Checks for string functions.
dnl TODO: Remove strdup, snprintf, and vsnprintf checks since they are C99?
AC_CHECK_FUNCS([strdup snprintf vsnprintf])
AC_CHECK_FUNCS([strlcat strlcpy])

dnl Check for random number functions...
AC_CHECK_FUNCS([random lrand48 arc4random])

dnl Check for geteuid function.
AC_CHECK_FUNCS([geteuid])

dnl Check for setpgid function.
AC_CHECK_FUNCS([setpgid])

dnl Check for vsyslog function.
AC_CHECK_FUNCS([vsyslog])

dnl Checks for signal functions.
AS_CASE(["$host_os_name"], [linux* | gnu*], [
    # Do not use sigset on Linux or GNU HURD
], [*], [
    # Use sigset on other platforms, if available
    AC_CHECK_FUNCS([sigset])
])

AC_CHECK_FUNCS([sigaction])

dnl Checks for wait functions.
AC_CHECK_FUNCS([waitpid wait3])

dnl Check for posix_spawn
AC_CHECK_FUNCS([posix_spawn])

dnl Check for getgrouplist
AC_CHECK_FUNCS([getgrouplist])

dnl See if the tm structure has the tm_gmtoff member...
AC_MSG_CHECKING([for tm_gmtoff member in tm structure])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[#include <time.h>]], [[
        struct tm t;
	int o = t.tm_gmtoff;
    ]])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_TM_GMTOFF], [1], [Have tm_gmtoff member in struct tm?])
], [
    AC_MSG_RESULT([no])
])

dnl See if the stat structure has the st_gen member...
AC_MSG_CHECKING([for st_gen member in stat structure])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[#include <sys/stat.h>]], [[
        struct stat t;
	int o = t.st_gen;
    ]])
], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_ST_GEN], [1], [Have st_gen member in struct stat?])
], [
    AC_MSG_RESULT([no])
])

dnl See if we have the removefile(3) function for securely removing files
AC_CHECK_FUNCS([removefile])

dnl See if we have libusb...
AC_ARG_ENABLE([libusb], AS_HELP_STRING([--enable-libusb], [use libusb for USB printing]))

LIBUSB=""
USBQUIRKS=""
AC_SUBST([LIBUSB])
AC_SUBST([USBQUIRKS])

AS_IF([test "x$PKGCONFIG" != x], [
    AS_IF([test x$enable_libusb != xno -a $host_os_name != darwin], [
	AC_MSG_CHECKING([for libusb-1.0])
	AS_IF([$PKGCONFIG --exists libusb-1.0], [
	    AC_MSG_RESULT([yes])
	    AC_DEFINE([HAVE_LIBUSB], [1], [Have usb library?])
	    CFLAGS="$CFLAGS $($PKGCONFIG --cflags libusb-1.0)"
	    LIBUSB="$($PKGCONFIG --libs libusb-1.0)"
	    USBQUIRKS="\$(DATADIR)/usb"
	], [
	    AC_MSG_RESULT([no])
	    AS_IF([test x$enable_libusb = xyes], [
		AC_MSG_ERROR([libusb required for --enable-libusb.])
	    ])
	])
    ])
], [test x$enable_libusb = xyes], [
    AC_MSG_ERROR([Need pkg-config to enable libusb support.])
])

dnl See if we have libwrap for TCP wrappers support...
AC_ARG_ENABLE([tcp_wrappers], AS_HELP_STRING([--enable-tcp-wrappers], [use libwrap for TCP wrappers support]))

LIBWRAP=""
AC_SUBST([LIBWRAP])

AS_IF([test x$enable_tcp_wrappers = xyes], [
    AC_CHECK_LIB([wrap], [hosts_access], [
	AC_CHECK_HEADER([tcpd.h], [
	    AC_DEFINE([HAVE_TCPD_H], [1], [Have <tcpd.h> header?])
	    LIBWRAP="-lwrap"
	])
    ])
])

dnl ZLIB
INSTALL_GZIP=""
LIBZ=""
AC_CHECK_HEADER([zlib.h], [
    AC_CHECK_LIB([z], [gzgets], [
	AC_DEFINE([HAVE_LIBZ], [1], [Have zlib library?])
	LIBZ="-lz"
	LIBS="$LIBS -lz"
	AC_CHECK_LIB([z], [inflateCopy], [
	    AC_DEFINE([HAVE_INFLATECOPY], [1], [Have inflateCopy function?])
	])
	AS_IF([test "x$GZIPPROG" != x], [
	    INSTALL_GZIP="-z"
	])
    ])
])
AC_SUBST([INSTALL_GZIP])
AC_SUBST([LIBZ])

PKGCONFIG_LIBS_STATIC="$PKGCONFIG_LIBS_STATIC $LIBZ"

dnl Flags for "ar" command...
AS_CASE([host_os_name], [darwin* | *bsd*], [
    ARFLAGS="-rcv"
], [*], [
    ARFLAGS="crvs"
])
AC_SUBST([ARFLAGS])

dnl Prep libraries specifically for cupsd and backends...
BACKLIBS=""
SERVERLIBS=""
AC_SUBST([BACKLIBS])
AC_SUBST([SERVERLIBS])

dnl See if we have POSIX ACL support...
SAVELIBS="$LIBS"
LIBS=""
AC_ARG_ENABLE([acl], AS_HELP_STRING([--enable-acl], [build with POSIX ACL support]))
AS_IF([test "x$enable_acl" != xno], [
    AC_SEARCH_LIBS([acl_init], [acl], [
        AC_DEFINE([HAVE_ACL_INIT], [1], [Have acl_init function?])
    ])
    SERVERLIBS="$SERVERLIBS $LIBS"
])
LIBS="$SAVELIBS"

dnl Check for DBUS support
DBUSDIR=""
DBUS_NOTIFIER=""
DBUS_NOTIFIERLIBS=""

AC_ARG_ENABLE([dbus], AS_HELP_STRING([--disable-dbus], [build without DBUS support]))
AC_ARG_WITH([dbusdir], AS_HELP_STRING([--with-dbusdir], [set DBUS configuration directory]), [
    DBUSDIR="$withval"
])

AS_IF([test "x$enable_dbus" != xno -a "x$PKGCONFIG" != x -a "x$host_os_name" != xdarwin], [
    AC_MSG_CHECKING([for DBUS])
    AS_IF([$PKGCONFIG --exists dbus-1], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_DBUS], [1], [Have dbus library?])
	CFLAGS="$CFLAGS $($PKGCONFIG --cflags dbus-1) -DDBUS_API_SUBJECT_TO_CHANGE"
	SERVERLIBS="$SERVERLIBS $($PKGCONFIG --libs dbus-1)"
	DBUS_NOTIFIER="dbus"
	DBUS_NOTIFIERLIBS="$($PKGCONFIG --libs dbus-1)"
	SAVELIBS="$LIBS"
	LIBS="$LIBS $DBUS_NOTIFIERLIBS"
	AC_CHECK_FUNC([dbus_message_iter_init_append], [
	    AC_DEFINE([HAVE_DBUS_MESSAGE_ITER_INIT_APPEND], [1], [Have dbus_message_iter_init_append function?])
	])
	AC_CHECK_FUNC([dbus_threads_init_default], [
	    AC_DEFINE([HAVE_DBUS_THREADS_INIT], [1], [Have dbus_threads_init_default function?])
	])
	LIBS="$SAVELIBS"
	AS_IF([test -d /etc/dbus-1 -a "x$DBUSDIR" = x], [
	    DBUSDIR="/etc/dbus-1"
	])
    ], [
	AC_MSG_RESULT([no])
    ])
])

AC_SUBST([DBUSDIR])
AC_SUBST([DBUS_NOTIFIER])
AC_SUBST([DBUS_NOTIFIERLIBS])

dnl Extra platform-specific libraries...
CUPS_DEFAULT_PRINTOPERATOR_AUTH="@SYSTEM"
CUPS_DEFAULT_SYSTEM_AUTHKEY=""
CUPS_SYSTEM_AUTHKEY=""
INSTALLXPC=""

AS_CASE([$host_os_name], [darwin*], [
    BACKLIBS="$BACKLIBS -framework IOKit"
    SERVERLIBS="$SERVERLIBS -framework IOKit -weak_framework ApplicationServices"
    LIBS="-framework CoreFoundation -framework Security $LIBS"
    PKGCONFIG_LIBS_STATIC="$PKGCONFIG_LIBS_STATIC -framework CoreFoundation -framework Security"

    dnl Check for framework headers...
    AC_CHECK_HEADER([ApplicationServices/ApplicationServices.h], [
        AC_DEFINE([HAVE_APPLICATIONSERVICES_H], [1], [Have <ApplicationServices/ApplicationServices.h>?])
    ])
    AC_CHECK_HEADER([CoreFoundation/CoreFoundation.h], [
        AC_DEFINE([HAVE_COREFOUNDATION_H], [1], [Have <CoreFoundation/CoreFoundation.h>?])
    ])

    dnl Check for dynamic store function...
    SAVELIBS="$LIBS"
    LIBS="-framework SystemConfiguration $LIBS"
    AC_CHECK_FUNCS([SCDynamicStoreCopyComputerName], [
	AC_DEFINE([HAVE_SCDYNAMICSTORECOPYCOMPUTERNAME], [1], [Have SCDynamicStoreCopyComputerName function?])
    ],[
	LIBS="$SAVELIBS"
    ])

    dnl Check for the new membership functions in MacOSX 10.4...
    AC_CHECK_HEADER([membership.h], [
        AC_DEFINE([HAVE_MEMBERSHIP_H], [1], [Have <membership.h>?])
    ])
    AC_CHECK_FUNCS([mbr_uid_to_uuid])

    dnl Need <dlfcn.h> header...
    AC_CHECK_HEADER([dlfcn.h], [
        AC_DEFINE([HAVE_DLFCN_H], [1], [Have <dlfcn.h>?])
    ])

    dnl Check for notify_post support
    AC_CHECK_HEADER([notify.h], [
        AC_DEFINE([HAVE_NOTIFY_H], [1], [Have <notify.h>?])
    ])
    AC_CHECK_FUNCS(notify_post)

    dnl Check for Authorization Services support
    AC_ARG_WITH([adminkey], AS_HELP_STRING([--with-adminkey], [set the default SystemAuthKey value]), [
	default_adminkey="$withval"
    ], [
	default_adminkey="default"
    ])
    AC_ARG_WITH([operkey], AS_HELP_STRING([--with-operkey], [set the default operator @AUTHKEY value]), [
	default_operkey="$withval"
    ], [
	default_operkey="default"
    ])

    AC_CHECK_HEADER([Security/Authorization.h], [
	AC_DEFINE([HAVE_AUTHORIZATION_H], [1], [Have <Security/Authorization.h>?])

	AS_IF([test "x$default_adminkey" != xdefault], [
	    CUPS_SYSTEM_AUTHKEY="SystemGroupAuthKey $default_adminkey"
	    CUPS_DEFAULT_SYSTEM_AUTHKEY="$default_adminkey"
	], [
	    CUPS_SYSTEM_AUTHKEY="SystemGroupAuthKey system.print.admin"
	    CUPS_DEFAULT_SYSTEM_AUTHKEY="system.print.admin"
	])

	AS_IF([test "x$default_operkey" != xdefault], [
	    CUPS_DEFAULT_PRINTOPERATOR_AUTH="@AUTHKEY($default_operkey) @admin @lpadmin"
	], [
	    CUPS_DEFAULT_PRINTOPERATOR_AUTH="@AUTHKEY(system.print.operator) @admin @lpadmin"
	])
    ])

    dnl Check for sandbox/Seatbelt support
    AC_CHECK_HEADER([sandbox.h], [
        AC_DEFINE([HAVE_SANDBOX_H], [1], [Have <sandbox.h>?])
    ])

    dnl Check for XPC support
    AC_CHECK_HEADER([xpc/xpc.h], [
	AC_DEFINE([HAVE_XPC], [1], [Have <xpc.h>?])
	INSTALLXPC="install-xpc"
    ])
])

AC_SUBST([CUPS_DEFAULT_PRINTOPERATOR_AUTH])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_PRINTOPERATOR_AUTH], ["$CUPS_DEFAULT_PRINTOPERATOR_AUTH"], [Default authorization for print operators?])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_SYSTEM_AUTHKEY], ["$CUPS_DEFAULT_SYSTEM_AUTHKEY"], [Default system authorization key for macOS?])
AC_SUBST([CUPS_SYSTEM_AUTHKEY])
AC_SUBST([INSTALLXPC])

dnl Check for build components
COMPONENTS="all"

AC_ARG_WITH([components], AS_HELP_STRING([--with-components], [set components to build: "all" (default) builds everything, "core" builds libcups and ipptool, "libcups" builds just libcups, "libcupslite" builds just libcups without driver support]), [
    COMPONENTS="$withval"
])

cupsimagebase="cupsimage"
IPPEVECOMMANDS="ippevepcl ippeveps"
LIBCUPSOBJS="\$(COREOBJS) \$(DRIVEROBJS)"
LIBHEADERS="\$(COREHEADERS) \$(DRIVERHEADERS)"
LIBHEADERSPRIV="\$(COREHEADERSPRIV) \$(DRIVERHEADERSPRIV)"

AS_CASE(["$COMPONENTS"], [all], [
    BUILDDIRS="tools filter backend berkeley cgi-bin monitor notifier ppdc scheduler systemv conf data desktop locale man doc examples templates"
], [core], [
    BUILDDIRS="tools examples locale"
], [corelite], [
    AC_DEFINE([CUPS_LITE], [1], [Building CUPS without driver support?])
    BUILDDIRS="tools examples locale"
    cupsimagebase=""
    LIBCUPSOBJS="\$(COREOBJS)"
    LIBHEADERS="\$(COREHEADERS)"
    LIBHEADERSPRIV="\$(COREHEADERSPRIV)"
], [libcups], [
    BUILDDIRS="locale"
    cupsimagebase=""
], [libcupslite], [
    AC_DEFINE([CUPS_LITE], [1], [Building CUPS without driver support?])
    BUILDDIRS="locale"
    cupsimagebase=""
    LIBCUPSOBJS="\$(COREOBJS)"
    LIBHEADERS="\$(COREHEADERS)"
    LIBHEADERSPRIV="\$(COREHEADERSPRIV)"
], [*], [
    AC_MSG_ERROR([Bad build component "$COMPONENT" specified.])
])

AC_SUBST([BUILDDIRS])
AC_SUBST([IPPEVECOMMANDS])
AC_SUBST([LIBCUPSOBJS])
AC_SUBST([LIBHEADERS])
AC_SUBST([LIBHEADERSPRIV])
