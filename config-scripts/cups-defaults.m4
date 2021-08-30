dnl
dnl Default cupsd configuration settings for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2018 by Apple Inc.
dnl Copyright © 2006-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Set a default systemd WantedBy directive
SYSTEMD_WANTED_BY="printers.target"

dnl Default languages...
LANGUAGES="$(ls -1 locale/cups_*.po 2>/dev/null | sed -e '1,$s/locale\/cups_//' -e '1,$s/\.po//' | tr '\n' ' ')"

AC_ARG_WITH([languages], AS_HELP_STRING([--with-languages], [set installed languages, default=all]), [
    AS_CASE(["$withval"], [none | no], [
        LANGUAGES=""
    ], [all], [
    ], [*], [
        LANGUAGES="$withval"
    ])
])
AC_SUBST([LANGUAGES])

dnl macOS bundle-based localization support
AC_ARG_WITH([bundledir], AS_HELP_STRING([--with-bundledir], [set localization bundle directory]), [
    CUPS_BUNDLEDIR="$withval"
], [
    AS_IF([test "x$host_os_name" = xdarwin -a $host_os_version -ge 100], [
        CUPS_BUNDLEDIR="/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/PrintCore.framework/Versions/A"
	LANGUAGES=""
    ], [
	CUPS_BUNDLEDIR=""
    ])
])

AC_SUBST([CUPS_BUNDLEDIR])
AS_IF([test "x$CUPS_BUNDLEDIR" != x], [
    AC_DEFINE_UNQUOTED([CUPS_BUNDLEDIR], ["$CUPS_BUNDLEDIR"], [macOS bundle directory.])
])

AC_ARG_WITH([bundlelang], AS_HELP_STRING([--with-bundlelang], [set localization bundle base language (English or en)]), [
    cups_bundlelang="$withval"
], [
    AS_IF([test $host_os_version -ge 190], [
	cups_bundlelang="en"
    ], [
	cups_bundlelang="English"
    ])
])

AS_IF([test "x$cups_bundlelang" != x -a "x$CUPS_BUNDLEDIR" != x], [
    CUPS_RESOURCEDIR="$CUPS_BUNDLEDIR/Resources/$cups_bundlelang.lproj"
], [
    CUPS_RESOURCEDIR=""
])
AC_SUBST([CUPS_RESOURCEDIR])

dnl Default executable file permissions
AC_ARG_WITH([exe_file_perm], AS_HELP_STRING([--with-exe-file-perm], [set default executable permissions value, default=0755]), [
    CUPS_EXE_FILE_PERM="$withval"
], [
    CUPS_EXE_FILE_PERM="755"
])
AC_SUBST([CUPS_EXE_FILE_PERM])

dnl Default ConfigFilePerm
AC_ARG_WITH([config_file_perm], AS_HELP_STRING([--with-config-file-perm], [set default ConfigFilePerm value, default=0640]), [
    CUPS_CONFIG_FILE_PERM="$withval"
], [
    CUPS_CONFIG_FILE_PERM="640"
])
AC_SUBST([CUPS_CONFIG_FILE_PERM])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_CONFIG_FILE_PERM], [0$CUPS_CONFIG_FILE_PERM], [Default ConfigFilePerm value.])

dnl Default permissions for cupsd
AC_ARG_WITH([cupsd_file_perm], AS_HELP_STRING([--with-cupsd-file-perm], [set default cupsd permissions, default=0700]), [
    CUPS_CUPSD_FILE_PERM="$withval"
], [
    CUPS_CUPSD_FILE_PERM="700"
])
AC_SUBST([CUPS_CUPSD_FILE_PERM])

dnl Default LogFilePerm
AC_ARG_WITH([log_file_perm], AS_HELP_STRING([--with-log-file-perm], [set default LogFilePerm value, default=0644]), [
    CUPS_LOG_FILE_PERM="$withval"
], [
    CUPS_LOG_FILE_PERM="644"
])
AC_SUBST([CUPS_LOG_FILE_PERM])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_LOG_FILE_PERM], [0$CUPS_LOG_FILE_PERM], [Default LogFilePerm value.])

dnl Default MaxLogSize
AC_ARG_WITH([max_log_size], AS_HELP_STRING([--with-max-log-size], [set default MaxLogSize value, default=1m]), [
    CUPS_MAX_LOG_SIZE="$withval"
], [
    CUPS_MAX_LOG_SIZE="1m"
])
AC_SUBST([CUPS_MAX_LOG_SIZE])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_MAX_LOG_SIZE], ["$CUPS_MAX_LOG_SIZE"], [Default MaxLogSize value.])

dnl Default ErrorPolicy
AC_ARG_WITH([error_policy], AS_HELP_STRING([--with-error-policy], [set default ErrorPolicy value, default=stop-printer]), [
    CUPS_ERROR_POLICY="$withval"
], [
    CUPS_ERROR_POLICY="stop-printer"
])
AC_SUBST([CUPS_ERROR_POLICY])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_ERROR_POLICY], ["$CUPS_ERROR_POLICY"], [Default ErrorPolicy value.])

dnl Default FatalErrors
AC_ARG_WITH([fatal_errors], AS_HELP_STRING([--with-fatal-errors], [set default FatalErrors value, default=config]), [
    CUPS_FATAL_ERRORS="$withval"
], [
    CUPS_FATAL_ERRORS="config"
])
AC_SUBST([CUPS_FATAL_ERRORS])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_FATAL_ERRORS], ["$CUPS_FATAL_ERRORS"], [Default FatalErrors value.])

dnl Default LogLevel
AC_ARG_WITH([log_level], AS_HELP_STRING([--with-log-level], [set default LogLevel value, default=warn]), [
    CUPS_LOG_LEVEL="$withval"
], [
    CUPS_LOG_LEVEL="warn"
])
AC_SUBST([CUPS_LOG_LEVEL])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_LOG_LEVEL], ["$CUPS_LOG_LEVEL"], [Default LogLevel value.])

dnl Default AccessLogLevel
AC_ARG_WITH(access_log_level, [  --with-access-log-level set default AccessLogLevel value, default=none],
	CUPS_ACCESS_LOG_LEVEL="$withval",
	CUPS_ACCESS_LOG_LEVEL="none")
AC_SUBST(CUPS_ACCESS_LOG_LEVEL)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_ACCESS_LOG_LEVEL, "$CUPS_ACCESS_LOG_LEVEL")

dnl Default PageLogFormat
AC_ARG_ENABLE([page_logging], AS_HELP_STRING([--enable-page-logging], [enable page_log by default]))
AS_IF([test "x$enable_page_logging" = xyes], [
    CUPS_PAGE_LOG_FORMAT=""
], [
    CUPS_PAGE_LOG_FORMAT="PageLogFormat"
])
AC_SUBST([CUPS_PAGE_LOG_FORMAT])

dnl Default SyncOnClose
AC_ARG_ENABLE([sync_on_close], AS_HELP_STRING([--enable-sync-on-close], [enable SyncOnClose (off by default)]))
AS_IF([test "x$enable_sync_on_close" = xyes], [
    CUPS_SYNC_ON_CLOSE="Yes"
    AC_DEFINE([CUPS_DEFAULT_SYNC_ON_CLOSE], [1], [Enable SyncOnClose by default?])
], [
    CUPS_SYNC_ON_CLOSE="No"
])
AC_SUBST([CUPS_SYNC_ON_CLOSE])

dnl Default Browsing
AC_ARG_ENABLE([browsing], AS_HELP_STRING([--disable-browsing], [disable Browsing by default]))
AS_IF([test "x$enable_browsing" = xno], [
    CUPS_BROWSING="No"
    AC_DEFINE_UNQUOTED([CUPS_DEFAULT_BROWSING], [0], [Enable Browsing by default?])
], [
    CUPS_BROWSING="Yes"
    AC_DEFINE_UNQUOTED([CUPS_DEFAULT_BROWSING], [1], [Enable Browsing by default?])
])
AC_SUBST([CUPS_BROWSING])

dnl Default BrowseLocalProtocols
AC_ARG_WITH([local_protocols], AS_HELP_STRING([--with-local-protocols], [set default BrowseLocalProtocols, default=""]), [
    default_local_protocols="$withval"
], [
    default_local_protocols="default"
])

AS_IF([test x$with_local_protocols != xno], [
    AS_IF([test "x$default_local_protocols" = "xdefault"], [
	AS_IF([test "x$DNSSD_BACKEND" != "x"], [
	    CUPS_BROWSE_LOCAL_PROTOCOLS="dnssd"
	], [
	    CUPS_BROWSE_LOCAL_PROTOCOLS=""
	])
    ], [
	CUPS_BROWSE_LOCAL_PROTOCOLS="$default_local_protocols"
    ])
], [
    CUPS_BROWSE_LOCAL_PROTOCOLS=""
])

AC_SUBST([CUPS_BROWSE_LOCAL_PROTOCOLS])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS], ["$CUPS_BROWSE_LOCAL_PROTOCOLS"], [Default BrowseLocalProtocols value.])

dnl Default DefaultShared
AC_ARG_ENABLE([default_shared], AS_HELP_STRING([--disable-default-shared], [disable DefaultShared by default]))
AS_IF([test "x$enable_default_shared" = xno], [
    CUPS_DEFAULT_SHARED="No"
    AC_DEFINE_UNQUOTED([CUPS_DEFAULT_DEFAULT_SHARED], [0], [Default DefaultShared value.])
], [
    CUPS_DEFAULT_SHARED="Yes"
    AC_DEFINE_UNQUOTED([CUPS_DEFAULT_DEFAULT_SHARED], [1], [Default DefaultShared value.])
])
AC_SUBST([CUPS_DEFAULT_SHARED])

dnl Determine the correct username and group for this OS...
AC_ARG_WITH([cups_user], AS_HELP_STRING([--with-cups-user], [set default user for CUPS]), [
    CUPS_USER="$withval"
], [
    AC_MSG_CHECKING([for default print user])
    AS_IF([test x$host_os_name = xdarwin], [
	AS_IF([test "x$(id -u _lp 2>/dev/null)" = x], [
	    CUPS_USER="lp"
	], [
	    CUPS_USER="_lp"
	])
	AC_MSG_RESULT([$CUPS_USER])
    ], [test -f /etc/passwd], [
	CUPS_USER=""
	for user in lp lpd guest daemon nobody; do
	    AS_IF([test "$(grep \^${user}: /etc/passwd)" != ""], [
		CUPS_USER="$user"
		AC_MSG_RESULT([$user])
		break
	    ])
	done

	AS_IF([test x$CUPS_USER = x], [
	    CUPS_USER="nobody"
	    AC_MSG_RESULT([not found, using "$CUPS_USER"])
	])
    ], [
	CUPS_USER="nobody"
	AC_MSG_RESULT([no password file, using "$CUPS_USER"])
    ])
])

AS_IF([test "x$CUPS_USER" = "xroot" -o "x$CUPS_USER" = "x0"], [
    AC_MSG_ERROR([The default user for CUPS cannot be root.])
])

AC_ARG_WITH([cups_group], AS_HELP_STRING([--with-cups-group], [set default group for CUPS]), [
    CUPS_GROUP="$withval"
], [
    AC_MSG_CHECKING([for default print group])
    AS_IF([test x$host_os_name = xdarwin], [
	AS_IF([test "x$(id -g _lp 2>/dev/null)" = x], [
	    CUPS_GROUP="lp"
	], [
	    CUPS_GROUP="_lp"
	])
	AC_MSG_RESULT([$CUPS_GROUP])
    ], [test -f /etc/group], [
	GROUP_LIST="_lp lp nobody"
	CUPS_GROUP=""
	for group in $GROUP_LIST; do
	    AS_IF([test "$(grep \^${group}: /etc/group)" != ""], [
		CUPS_GROUP="$group"
		AC_MSG_RESULT([$group])
		break
	    ])
	done

	AS_IF([test x$CUPS_GROUP = x], [
	    CUPS_GROUP="nobody"
	    AC_MSG_RESULT([not found, using "$CUPS_GROUP"])
	])
    ], [
	CUPS_GROUP="nobody"
	AC_MSG_RESULT([no group file, using "$CUPS_GROUP"])
    ])
])

AS_IF([test "x$CUPS_GROUP" = "xroot" -o "x$CUPS_GROUP" = "xwheel" -o "x$CUPS_GROUP" = "x0"], [
    AC_MSG_ERROR([The default group for CUPS cannot be root.])
])

AC_ARG_WITH([system_groups], AS_HELP_STRING([--with-system-groups], [set default system groups for CUPS]), [
    CUPS_SYSTEM_GROUPS="$withval"
], [
    AS_IF([test x$host_os_name = xdarwin], [
	CUPS_SYSTEM_GROUPS="admin"
    ], [
	AC_MSG_CHECKING([for default system groups])
	AS_IF([test -f /etc/group], [
	    CUPS_SYSTEM_GROUPS=""
	    GROUP_LIST="lpadmin sys system root wheel"
	    for group in $GROUP_LIST; do
		AS_IF([test "$(grep \^${group}: /etc/group)" != ""], [
		    AS_IF([test "x$CUPS_SYSTEM_GROUPS" = x], [
			CUPS_SYSTEM_GROUPS="$group"
		    ], [
			CUPS_SYSTEM_GROUPS="$CUPS_SYSTEM_GROUPS $group"
		    ])
		])
	    done

	    AS_IF([test "x$CUPS_SYSTEM_GROUPS" = x], [
		CUPS_SYSTEM_GROUPS="$GROUP_LIST"
		AC_MSG_RESULT([no groups found, using "$CUPS_SYSTEM_GROUPS"])
	    ], [
		AC_MSG_RESULT(["$CUPS_SYSTEM_GROUPS"])
	    ])
	], [
	    CUPS_SYSTEM_GROUPS="$GROUP_LIST"
	    AC_MSG_RESULT([no group file, using "$CUPS_SYSTEM_GROUPS"])
	])
    ])
])

CUPS_PRIMARY_SYSTEM_GROUP="$(echo $CUPS_SYSTEM_GROUPS | awk '{print $1}')"

for group in $CUPS_SYSTEM_GROUPS; do
    AS_IF([test "x$CUPS_GROUP" = "x$group"], [
	AC_MSG_ERROR([The default system groups cannot contain the default CUPS group.])
    ])
done

AC_SUBST([CUPS_USER])
AC_SUBST([CUPS_GROUP])
AC_SUBST([CUPS_SYSTEM_GROUPS])
AC_SUBST([CUPS_PRIMARY_SYSTEM_GROUP])

AC_DEFINE_UNQUOTED([CUPS_DEFAULT_USER], ["$CUPS_USER"], [Default User value.])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_GROUP], ["$CUPS_GROUP"], [Default Group value.])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_SYSTEM_GROUPS], ["$CUPS_SYSTEM_GROUPS"], [Default SystemGroup value(s).])


dnl Default printcap file...
AC_ARG_WITH([printcap], AS_HELP_STRING([--with-printcap], [set default printcap file]), [
    default_printcap="$withval"
], [
    default_printcap="default"
])

AS_IF([test x$default_printcap != xno], [
    AS_IF([test "x$default_printcap" = "xdefault"], [
	AS_CASE([$host_os_name], [darwin*], [
	    CUPS_DEFAULT_PRINTCAP="/Library/Preferences/org.cups.printers.plist"
	], [sunos*], [
	    CUPS_DEFAULT_PRINTCAP="/etc/printers.conf"
	], [*], [
	    CUPS_DEFAULT_PRINTCAP="/etc/printcap"
	])
    ], [
	CUPS_DEFAULT_PRINTCAP="$default_printcap"
    ])
], [
    CUPS_DEFAULT_PRINTCAP=""
])

AC_SUBST([CUPS_DEFAULT_PRINTCAP])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_PRINTCAP], ["$CUPS_DEFAULT_PRINTCAP"], [Default Printcap value.])

dnl Default MaxCopies value...
AC_ARG_WITH([max_copies], AS_HELP_STRING([--with-max-copies], [set default max copies value, default=9999]), [
    CUPS_MAX_COPIES="$withval"
], [
    CUPS_MAX_COPIES="9999"
])

AC_SUBST([CUPS_MAX_COPIES])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_MAX_COPIES], [$CUPS_MAX_COPIES], [Default MaxCopies value.])

dnl Default raw printing state
AC_ARG_ENABLE([raw_printing], AS_HELP_STRING([--disable-raw-printing], [do not allow raw printing by default]))
AS_IF([test "x$enable_raw_printing" != xno], [
    DEFAULT_RAW_PRINTING=""
], [
    DEFAULT_RAW_PRINTING="#"
])
AC_SUBST([DEFAULT_RAW_PRINTING])

dnl Default SNMP options...
AC_ARG_WITH([snmp_address], AS_HELP_STRING([--with-snmp-address], [set SNMP query address, default=auto]), [
    AS_IF([test "x$withval" = x], [
	CUPS_SNMP_ADDRESS=""
    ], [
	CUPS_SNMP_ADDRESS="Address $withval"
    ])
], [
    AS_IF([test "x$host_os_name" = xdarwin], [
	CUPS_SNMP_ADDRESS=""
    ], [
	CUPS_SNMP_ADDRESS="Address @LOCAL"
    ])
])

AC_ARG_WITH([snmp_community], AS_HELP_STRING([--with-snmp-community], [set SNMP community, default=public]), [
    CUPS_SNMP_COMMUNITY="Community $withval"
], [
    CUPS_SNMP_COMMUNITY="Community public"
])

AC_SUBST([CUPS_SNMP_ADDRESS])
AC_SUBST([CUPS_SNMP_COMMUNITY])

dnl New default port definition for IPP...
AC_ARG_WITH([ipp_port], AS_HELP_STRING([--with-ipp-port], [set port number for IPP, default=631]), [
    DEFAULT_IPP_PORT="$withval"
], [
    DEFAULT_IPP_PORT="631"
])

AC_SUBST([DEFAULT_IPP_PORT])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_IPP_PORT], [$DEFAULT_IPP_PORT], [Default IPP port number.])

dnl Web interface...
AC_ARG_ENABLE([webif], AS_HELP_STRING([--enable-webif], [enable the web interface by default, default=no for macOS]))
AS_CASE(["x$enable_webif"], [xno], [
    CUPS_WEBIF=No
    CUPS_DEFAULT_WEBIF=0
], [xyes], [
    CUPS_WEBIF=Yes
    CUPS_DEFAULT_WEBIF=1
], [*], [
    AS_IF([test $host_os_name = darwin], [
	CUPS_WEBIF=No
	CUPS_DEFAULT_WEBIF=0
    ], [
	CUPS_WEBIF=Yes
	CUPS_DEFAULT_WEBIF=1
    ])
])

AC_SUBST([CUPS_WEBIF])
AC_DEFINE_UNQUOTED([CUPS_DEFAULT_WEBIF], [$CUPS_DEFAULT_WEBIF], [Default WebInterface value.])

AS_IF([test $CUPS_WEBIF = Yes || test $CUPS_BROWSING = Yes], [
  SYSTEMD_WANTED_BY="$SYSTEMD_WANTED_BY multi-user.target"], [
  ])
AC_SUBST([SYSTEMD_WANTED_BY])
