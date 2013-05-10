dnl
dnl "$Id: cups-defaults.m4 9750 2011-05-06 22:53:53Z mike $"
dnl
dnl   Default cupsd configuration settings for CUPS.
dnl
dnl   Copyright 2007-2011 by Apple Inc.
dnl   Copyright 2006-2007 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Apple Inc. and are protected by Federal copyright
dnl   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
dnl   which should have been included with this file.  If this file is
dnl   file is missing or damaged, see the license at "http://www.cups.org/".
dnl

dnl Default languages...
LANGUAGES="`ls -1 locale/cups_*.po | sed -e '1,$s/locale\/cups_//' -e '1,$s/\.po//' | tr '\n' ' '`"

AC_ARG_WITH(languages, [  --with-languages        set installed languages, default=all ],[
	case "$withval" in
		none | no) LANGUAGES="" ;;
		all) ;;
		*) LANGUAGES="$withval" ;;
	esac])
AC_SUBST(LANGUAGES)

dnl Mac OS X bundle-based localization support
AC_ARG_WITH(bundledir, [  --with-bundledir     set Mac OS X localization bundle directory ],
	CUPS_BUNDLEDIR="$withval",
	if test "x$uname" = xDarwin -a $uversion -ge 100; then
		CUPS_BUNDLEDIR="/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/PrintCore.framework/Versions/A"
		LANGUAGES=""
	else
		CUPS_BUNDLEDIR=""
	fi)

AC_SUBST(CUPS_BUNDLEDIR)
if test "x$CUPS_BUNDLEDIR" != x; then
	AC_DEFINE_UNQUOTED(CUPS_BUNDLEDIR, "$CUPS_BUNDLEDIR")
fi

dnl Default ConfigFilePerm
AC_ARG_WITH(config_file_perm, [  --with-config-file-perm set default ConfigFilePerm value, default=0640],
	CUPS_CONFIG_FILE_PERM="$withval",
	if test "x$uname" = xDarwin; then
		CUPS_CONFIG_FILE_PERM="644"
	else
		CUPS_CONFIG_FILE_PERM="640"
	fi)
AC_SUBST(CUPS_CONFIG_FILE_PERM)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_CONFIG_FILE_PERM, 0$CUPS_CONFIG_FILE_PERM)

dnl Default LogFilePerm
AC_ARG_WITH(log_file_perm, [  --with-log-file-perm    set default LogFilePerm value, default=0644],
	CUPS_LOG_FILE_PERM="$withval",
	CUPS_LOG_FILE_PERM="644")
AC_SUBST(CUPS_LOG_FILE_PERM)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LOG_FILE_PERM, 0$CUPS_LOG_FILE_PERM)

dnl Default FatalErrors
AC_ARG_WITH(fatal_errors, [  --with-fatal-errors     set default FatalErrors value, default=config],
	CUPS_FATAL_ERRORS="$withval",
	CUPS_FATAL_ERRORS="config")
AC_SUBST(CUPS_FATAL_ERRORS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_FATAL_ERRORS, "$CUPS_FATAL_ERRORS")


dnl Default LogLevel
AC_ARG_WITH(log_level, [  --with-log-level        set default LogLevel value, default=warn],
	CUPS_LOG_LEVEL="$withval",
	CUPS_LOG_LEVEL="warn")
AC_SUBST(CUPS_LOG_LEVEL)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LOG_LEVEL, "$CUPS_LOG_LEVEL")

dnl Default AccessLogLevel
AC_ARG_WITH(access_log_level, [  --with-access-log-level set default AccessLogLevel value, default=actions],
	CUPS_ACCESS_LOG_LEVEL="$withval",
	CUPS_ACCESS_LOG_LEVEL="actions")
AC_SUBST(CUPS_ACCESS_LOG_LEVEL)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_ACCESS_LOG_LEVEL, "$CUPS_ACCESS_LOG_LEVEL")

dnl Default Browsing
AC_ARG_ENABLE(browsing, [  --disable-browsing      disable Browsing by default])
if test "x$enable_browsing" = xno; then
	CUPS_BROWSING="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSING, 0)
else
	CUPS_BROWSING="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSING, 1)
fi
AC_SUBST(CUPS_BROWSING)

dnl Default BrowseLocalProtocols
AC_ARG_WITH(local_protocols, [  --with-local-protocols  set default BrowseLocalProtocols, default="CUPS"],
	default_local_protocols="$withval",
	default_local_protocols="default")

if test x$with_local_protocols != xno; then
	if test "x$default_local_protocols" = "xdefault"; then
		if test "x$DNSSDLIBS" != "x"; then
		CUPS_BROWSE_LOCAL_PROTOCOLS="CUPS dnssd"
	else
		CUPS_BROWSE_LOCAL_PROTOCOLS="CUPS"
		fi
	else
		CUPS_BROWSE_LOCAL_PROTOCOLS="$default_local_protocols"
	fi
else
	CUPS_BROWSE_LOCAL_PROTOCOLS=""
fi

AC_SUBST(CUPS_BROWSE_LOCAL_PROTOCOLS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS,
	"$CUPS_BROWSE_LOCAL_PROTOCOLS")

dnl Default BrowseRemoteProtocols
AC_ARG_WITH(remote_protocols, [  --with-remote-protocols set default BrowseRemoteProtocols, default="CUPS"],
	default_remote_protocols="$withval",
	default_remote_protocols="default")

if test x$with_remote_protocols != xno; then
	if test "x$default_remote_protocols" = "xdefault"; then
		if test "$uname" = "Darwin" -a $uversion -ge 90; then
			CUPS_BROWSE_REMOTE_PROTOCOLS=""
		else
			CUPS_BROWSE_REMOTE_PROTOCOLS="CUPS"
		fi
	else
		CUPS_BROWSE_REMOTE_PROTOCOLS="$default_remote_protocols"
	fi
else
	CUPS_BROWSE_REMOTE_PROTOCOLS=""
fi

AC_SUBST(CUPS_BROWSE_REMOTE_PROTOCOLS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS,
	"$CUPS_BROWSE_REMOTE_PROTOCOLS")

dnl Default BrowseShortNames
AC_ARG_ENABLE(browse_short, [  --disable-browse-short-names
			  disable BrowseShortNames by default])
if test "x$enable_browse_short" = xno; then
	CUPS_BROWSE_SHORT_NAMES="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_SHORT_NAMES, 0)
else
	CUPS_BROWSE_SHORT_NAMES="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_SHORT_NAMES, 1)
fi
AC_SUBST(CUPS_BROWSE_SHORT_NAMES)

dnl Default DefaultShared
AC_ARG_ENABLE(default_shared, [  --disable-default-shared
			  disable DefaultShared by default])
if test "x$enable_default_shared" = xno; then
	CUPS_DEFAULT_SHARED="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DEFAULT_SHARED, 0)
else
	CUPS_DEFAULT_SHARED="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DEFAULT_SHARED, 1)
fi
AC_SUBST(CUPS_DEFAULT_SHARED)

dnl Default ImplicitClasses
AC_ARG_ENABLE(implicit, [  --disable-implicit-classes
                          disable ImplicitClasses by default])
if test "x$enable_implicit" = xno; then
	CUPS_IMPLICIT_CLASSES="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IMPLICIT_CLASSES, 0)
else
	CUPS_IMPLICIT_CLASSES="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IMPLICIT_CLASSES, 1)
fi
AC_SUBST(CUPS_IMPLICIT_CLASSES)

dnl Default UseNetworkDefault
AC_ARG_ENABLE(use_network_default, [  --enable-use-network-default
                          set UseNetworkDefault to Yes by default])
if test "x$enable_use_network_default" != xno; then
	AC_MSG_CHECKING(whether to use network default printers)
	if test "x$enable_use_network_default" = xyes -o $uname != Darwin; then
		CUPS_USE_NETWORK_DEFAULT="Yes"
		AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 1)
		AC_MSG_RESULT(yes)
	else
		CUPS_USE_NETWORK_DEFAULT="No"
		AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 0)
		AC_MSG_RESULT(no)
	fi
else
	CUPS_USE_NETWORK_DEFAULT="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 0)
fi
AC_SUBST(CUPS_USE_NETWORK_DEFAULT)

dnl Determine the correct username and group for this OS...
AC_ARG_WITH(cups_user, [  --with-cups-user        set default user for CUPS],
	CUPS_USER="$withval",
	AC_MSG_CHECKING(for default print user)
	if test x$uname = xDarwin; then
		if test x`id -u _lp 2>/dev/null` = x; then
			CUPS_USER="lp";
		else
			CUPS_USER="_lp";
		fi
		AC_MSG_RESULT($CUPS_USER)
	elif test -f /etc/passwd; then
		CUPS_USER=""
		for user in lp lpd guest daemon nobody; do
			if test "`grep \^${user}: /etc/passwd`" != ""; then
				CUPS_USER="$user"
				AC_MSG_RESULT($user)
				break;
			fi
		done

		if test x$CUPS_USER = x; then
			CUPS_USER="nobody"
			AC_MSG_RESULT(not found, using "$CUPS_USER")
		fi
	else
		CUPS_USER="nobody"
		AC_MSG_RESULT(no password file, using "$CUPS_USER")
	fi)

if test "x$CUPS_USER" = "xroot" -o "x$CUPS_USER" = "x0"; then
	AC_MSG_ERROR([The default user for CUPS cannot be root!])
fi

AC_ARG_WITH(cups_group, [  --with-cups-group       set default group for CUPS],
	CUPS_GROUP="$withval",
	AC_MSG_CHECKING(for default print group)
	if test x$uname = xDarwin; then
		if test x`id -g _lp 2>/dev/null` = x; then
			CUPS_GROUP="lp";
		else
			CUPS_GROUP="_lp";
		fi
		AC_MSG_RESULT($CUPS_GROUP)
	elif test -f /etc/group; then
		GROUP_LIST="_lp lp nobody"
		CUPS_GROUP=""
		for group in $GROUP_LIST; do
			if test "`grep \^${group}: /etc/group`" != ""; then
				CUPS_GROUP="$group"
				AC_MSG_RESULT($group)
				break;
			fi
		done

		if test x$CUPS_GROUP = x; then
			CUPS_GROUP="nobody"
			AC_MSG_RESULT(not found, using "$CUPS_GROUP")
		fi
	else
		CUPS_GROUP="nobody"
		AC_MSG_RESULT(no group file, using "$CUPS_GROUP")
	fi)

if test "x$CUPS_GROUP" = "xroot" -o "x$CUPS_GROUP" = "xwheel" -o "x$CUPS_GROUP" = "x0"; then
	AC_MSG_ERROR([The default group for CUPS cannot be root!])
fi

AC_ARG_WITH(system_groups, [  --with-system-groups    set default system groups for CUPS],
	CUPS_SYSTEM_GROUPS="$withval",
	if test x$uname = xDarwin; then
		CUPS_SYSTEM_GROUPS="admin"
	else
		AC_MSG_CHECKING(for default system groups)
		if test -f /etc/group; then
			CUPS_SYSTEM_GROUPS=""
			GROUP_LIST="lpadmin sys system root"
			for group in $GROUP_LIST; do
				if test "`grep \^${group}: /etc/group`" != ""; then
					if test "x$CUPS_SYSTEM_GROUPS" = x; then
						CUPS_SYSTEM_GROUPS="$group"
					else
						CUPS_SYSTEM_GROUPS="$CUPS_SYSTEM_GROUPS $group"
					fi
				fi
			done

			if test "x$CUPS_SYSTEM_GROUPS" = x; then
				CUPS_SYSTEM_GROUPS="$GROUP_LIST"
				AC_MSG_RESULT(no groups found, using "$CUPS_SYSTEM_GROUPS")
			else
				AC_MSG_RESULT("$CUPS_SYSTEM_GROUPS")
			fi
		else
			CUPS_SYSTEM_GROUPS="$GROUP_LIST"
			AC_MSG_RESULT(no group file, using "$CUPS_SYSTEM_GROUPS")
		fi
	fi)

CUPS_PRIMARY_SYSTEM_GROUP="`echo $CUPS_SYSTEM_GROUPS | awk '{print $1}'`"

for group in $CUPS_SYSTEM_GROUPS; do
	if test "x$CUPS_GROUP" = "x$group"; then
		AC_MSG_ERROR([The default system groups cannot contain the default CUPS group!])
	fi
done

AC_SUBST(CUPS_USER)
AC_SUBST(CUPS_GROUP)
AC_SUBST(CUPS_SYSTEM_GROUPS)
AC_SUBST(CUPS_PRIMARY_SYSTEM_GROUP)

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USER, "$CUPS_USER")
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_GROUP, "$CUPS_GROUP")
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_SYSTEM_GROUPS, "$CUPS_SYSTEM_GROUPS")

dnl Default printcap file...
AC_ARG_WITH(printcap, [  --with-printcap         set default printcap file],
	default_printcap="$withval",
	default_printcap="default")

if test x$default_printcap != xno; then
	if test "x$default_printcap" = "xdefault"; then
		case $uname in
			Darwin*)
				if test $uversion -ge 90; then
					CUPS_DEFAULT_PRINTCAP="/Library/Preferences/org.cups.printers.plist"
				else
					CUPS_DEFAULT_PRINTCAP="/etc/printcap"
				fi
				;;
			SunOS*)
				CUPS_DEFAULT_PRINTCAP="/etc/printers.conf"
				;;
			*)
				CUPS_DEFAULT_PRINTCAP="/etc/printcap"
				;;
		esac
	else
		CUPS_DEFAULT_PRINTCAP="$default_printcap"
	fi
else
	CUPS_DEFAULT_PRINTCAP=""
fi

AC_SUBST(CUPS_DEFAULT_PRINTCAP)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_PRINTCAP, "$CUPS_DEFAULT_PRINTCAP")

dnl Default LPD config file...
AC_ARG_WITH(lpdconfigfile, [  --with-lpdconfigfile    set default LPDConfigFile URI],
	default_lpdconfigfile="$withval",
	default_lpdconfigfile="default")

if test x$default_lpdconfigfile != xno; then
	if test "x$default_lpdconfigfile" = "xdefault"; then
		case $uname in
			Darwin*)
				CUPS_DEFAULT_LPD_CONFIG_FILE="launchd:///System/Library/LaunchDaemons/org.cups.cups-lpd.plist"
				;;
			*)
				if test "x$XINETD" != x; then
					CUPS_DEFAULT_LPD_CONFIG_FILE="xinetd://$XINETD/cups-lpd"
				else
					CUPS_DEFAULT_LPD_CONFIG_FILE=""
				fi
				;;
		esac
	else
		CUPS_DEFAULT_LPD_CONFIG_FILE="$default_lpdconfigfile"
	fi
else
	CUPS_DEFAULT_LPD_CONFIG_FILE=""
fi

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LPD_CONFIG_FILE, "$CUPS_DEFAULT_LPD_CONFIG_FILE")

dnl Default SMB config file...
AC_ARG_WITH(smbconfigfile, [  --with-smbconfigfile    set default SMBConfigFile URI],
	default_smbconfigfile="$withval",
	default_smbconfigfile="default")

if test x$default_smbconfigfile != xno; then
	if test "x$default_smbconfigfile" = "xdefault"; then
		if test -f /etc/smb.conf; then
			CUPS_DEFAULT_SMB_CONFIG_FILE="samba:///etc/smb.conf"
		else
			CUPS_DEFAULT_SMB_CONFIG_FILE=""
		fi
	else
		CUPS_DEFAULT_SMB_CONFIG_FILE="$default_smbconfigfile"
	fi
else
	CUPS_DEFAULT_SMB_CONFIG_FILE=""
fi

AC_DEFINE_UNQUOTED(CUPS_DEFAULT_SMB_CONFIG_FILE, "$CUPS_DEFAULT_SMB_CONFIG_FILE")

dnl Default MaxCopies value...
AC_ARG_WITH(max-copies, [  --with-max-copies       set default max copies value, default=9999 ],
	CUPS_MAX_COPIES="$withval",
	CUPS_MAX_COPIES="9999")

AC_SUBST(CUPS_MAX_COPIES)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_MAX_COPIES, $CUPS_MAX_COPIES)

dnl Default raw printing state
AC_ARG_ENABLE(raw_printing, [  --disable-raw-printing  do not allow raw printing by default])
if test "x$enable_raw_printing" != xno; then
	DEFAULT_RAW_PRINTING=""
else
	DEFAULT_RAW_PRINTING="#"
fi
AC_SUBST(DEFAULT_RAW_PRINTING)

dnl Default SNMP options...
AC_ARG_WITH(snmp-address, [  --with-snmp-address     set SNMP query address, default=auto ],
	if test "x$withval" = x; then
		CUPS_SNMP_ADDRESS=""
	else
		CUPS_SNMP_ADDRESS="Address $withval"
	fi,
	if test "x$uname" = xDarwin; then
		CUPS_SNMP_ADDRESS=""
	else
		CUPS_SNMP_ADDRESS="Address @LOCAL"
	fi)

AC_ARG_WITH(snmp-community, [  --with-snmp-community   set SNMP community, default=public ],
	CUPS_SNMP_COMMUNITY="Community $withval",
	CUPS_SNMP_COMMUNITY="Community public")

AC_SUBST(CUPS_SNMP_ADDRESS)
AC_SUBST(CUPS_SNMP_COMMUNITY)

dnl New default port definition for IPP...
AC_ARG_WITH(ipp-port, [  --with-ipp-port         set port number for IPP, default=631 ],
	DEFAULT_IPP_PORT="$withval",
	DEFAULT_IPP_PORT="631")

AC_SUBST(DEFAULT_IPP_PORT)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IPP_PORT,$DEFAULT_IPP_PORT)

dnl Filters
AC_ARG_ENABLE(bannertops, [  --enable-bannertops     always build the banner filter ])
AC_ARG_ENABLE(texttops, [  --enable-texttops       always build the text filter ])

if test "x$enable_bannertops" = xno; then
	BANNERTOPS=""
	DEFAULT_BANNERTOPS="#"
elif test "x$enable_bannertops" = xyes; then
	BANNERTOPS="bannertops"
	DEFAULT_BANNERTOPS=""
elif test $uname = Darwin; then
	BANNERTOPS=""
	DEFAULT_BANNERTOPS="#"
else
	BANNERTOPS="bannertops"
	DEFAULT_BANNERTOPS=""
fi

if test "x$enable_texttops" = xno; then
	TEXTTOPS=""
	DEFAULT_TEXTTOPS="#"
elif test "x$enable_texttops" = xyes; then
	TEXTTOPS="texttops"
	DEFAULT_TEXTTOPS=""
elif test $uname = Darwin; then
	TEXTTOPS=""
	DEFAULT_TEXTTOPS="#"
else
	TEXTTOPS="texttops"
	DEFAULT_TEXTTOPS=""
fi

AC_SUBST(BANNERTOPS)
AC_SUBST(DEFAULT_BANNERTOPS)
AC_SUBST(DEFAULT_TEXTTOPS)
AC_SUBST(TEXTTOPS)

dnl Fonts
if test "x$BANNERTOPS" = x -a "x$TEXTTOPS" = x; then
	FONTS=""
else
	FONTS="fonts"
fi

AC_SUBST(FONTS)

dnl Web interface...
AC_ARG_ENABLE(webif, [  --enable-webif          enable the web interface by default, default=no for Mac OS X])
case "x$enable_webif" in
	xno)
		CUPS_WEBIF=No
		CUPS_DEFAULT_WEBIF=0
		;;
	xyes)
		CUPS_WEBIF=Yes
		CUPS_DEFAULT_WEBIF=1
		;;
	*)
		if test $uname = Darwin; then
			CUPS_WEBIF=No
			CUPS_DEFAULT_WEBIF=0
		else
			CUPS_WEBIF=Yes
			CUPS_DEFAULT_WEBIF=1
		fi
		;;
esac

AC_SUBST(CUPS_WEBIF)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_WEBIF, $CUPS_DEFAULT_WEBIF)

dnl
dnl End of "$Id: cups-defaults.m4 9750 2011-05-06 22:53:53Z mike $".
dnl
