dnl
dnl "$Id$"
dnl
dnl   Default cupsd configuration settings for the Common UNIX Printing System
dnl   (CUPS).
dnl
dnl   Copyright 2006 by Easy Software Products, all rights reserved.
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

dnl Default ConfigFilePerm
AC_ARG_WITH(config_perm, [  --with-config-file-perm set default ConfigFilePerm value, default=0640],
	CUPS_CONFIG_FILE_PERM="$withval",
	if test "x$uname" = xDarwin; then
		CUPS_CONFIG_FILE_PERM="644"
	else
		CUPS_CONFIG_FILE_PERM="640"
	fi)
AC_SUBST(CUPS_CONFIG_FILE_PERM)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_CONFIG_FILE_PERM, 0$CUPS_CONFIG_FILE_PERM)

dnl Default LogFilePerm
AC_ARG_WITH(log_perm, [  --with-log-file-perm    set default LogFilePerm value, default=0644],
	CUPS_LOG_FILE_PERM="$withval",
	CUPS_LOG_FILE_PERM="644")
AC_SUBST(CUPS_LOG_FILE_PERM)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_LOG_FILE_PERM, 0$CUPS_LOG_FILE_PERM)

dnl Default Browsing
AC_ARG_ENABLE(browsing, [  --enable-browsing       enable Browsing by default, default=yes])
if test "x$enable_browsing" = xno; then
	CUPS_BROWSING="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSING, 0)
else
	CUPS_BROWSING="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSING, 1)
fi
AC_SUBST(CUPS_BROWSING)

dnl Default BrowseLocalProtocols
AC_ARG_WITH(browse_local, [  --with-local-protocols  set default BrowseLocalProtocols, default="CUPS"],
	CUPS_BROWSE_LOCAL_PROTOCOLS="$withval",
	CUPS_BROWSE_LOCAL_PROTOCOLS="CUPS")
AC_SUBST(CUPS_BROWSE_LOCAL_PROTOCOLS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_LOCAL_PROTOCOLS,
	"$CUPS_BROWSE_LOCAL_PROTOCOLS")

dnl Default BrowseRemoteProtocols
AC_ARG_WITH(browse_remote, [  --with-remote-protocols set default BrowseRemoteProtocols, default="CUPS"],
	CUPS_BROWSE_REMOTE_PROTOCOLS="$withval",
	CUPS_BROWSE_REMOTE_PROTOCOLS="CUPS")
AC_SUBST(CUPS_BROWSE_REMOTE_PROTOCOLS)
AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_REMOTE_PROTOCOLS,
	"$CUPS_BROWSE_REMOTE_PROTOCOLS")

dnl Default BrowseShortNames
AC_ARG_ENABLE(browse_short, [  --enable-browse-short-names
                          enable BrowseShortNames by default, default=yes])
if test "x$enable_browse_short" = xno; then
	CUPS_BROWSE_SHORT_NAMES="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_SHORT_NAMES, 0)
else
	CUPS_BROWSE_SHORT_NAMES="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_BROWSE_SHORT_NAMES, 1)
fi
AC_SUBST(CUPS_BROWSE_SHORT_NAMES)

dnl Default DefaultShared
AC_ARG_ENABLE(default_shared, [  --enable-default-shared enable DefaultShared by default, default=yes])
if test "x$enable_default_shared" = xno; then
	CUPS_DEFAULT_SHARED="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DEFAULT_SHARED, 0)
else
	CUPS_DEFAULT_SHARED="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DEFAULT_SHARED, 1)
fi
AC_SUBST(CUPS_DEFAULT_SHARED)

dnl Default ImplicitClasses
AC_ARG_ENABLE(implicit, [  --enable-implicit-classes
                          enable ImplicitClasses by default, default=yes])
if test "x$enable_implicit" = xno; then
	CUPS_IMPLICIT_CLASSES="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IMPLICIT_CLASSES, 0)
else
	CUPS_IMPLICIT_CLASSES="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_IMPLICIT_CLASSES, 1)
fi
AC_SUBST(CUPS_IMPLICIT_CLASSES)

dnl Default UseNetworkDefault
AC_ARG_ENABLE(network_default, [  --enable-use-network-default
                          enable UseNetworkDefault by default, default=yes])
if test "x$enable_network_default" = xno; then
	CUPS_USE_NETWORK_DEFAULT="No"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 0)
else
	CUPS_USE_NETWORK_DEFAULT="Yes"
	AC_DEFINE_UNQUOTED(CUPS_DEFAULT_USE_NETWORK_DEFAULT, 1)
fi
AC_SUBST(CUPS_USE_NETWORK_DEFAULT)


dnl
dnl End of "$Id$".
dnl
