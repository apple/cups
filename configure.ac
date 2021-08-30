dnl
dnl Configuration script for CUPS.
dnl
dnl Copyright © 2020-2021 by OpenPrinting
dnl Copyright © 2007-2020 by Apple Inc.
dnl Copyright © 1997-2007 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl We need at least autoconf 2.60...
AC_PREREQ([2.60])

dnl Package name and version...
AC_INIT([CUPS],[2.3.5],[https://github.com/apple/cups/issues],[cups],[https://www.cups.org/])

dnl This line is provided to ensure that you don't run the autoheader program
dnl against this project.  Doing so is completely unsupported and WILL cause
dnl problems!
AH_TOP([#error "Somebody ran autoheader on this project which is unsupported and WILL cause problems."])

dnl Include configure tests for different areas...
sinclude(config-scripts/cups-opsys.m4)
sinclude(config-scripts/cups-common.m4)
sinclude(config-scripts/cups-directories.m4)
sinclude(config-scripts/cups-manpages.m4)

sinclude(config-scripts/cups-sharedlibs.m4)
sinclude(config-scripts/cups-libtool.m4)
sinclude(config-scripts/cups-compiler.m4)

sinclude(config-scripts/cups-network.m4)
sinclude(config-scripts/cups-poll.m4)
sinclude(config-scripts/cups-gssapi.m4)
sinclude(config-scripts/cups-threads.m4)
sinclude(config-scripts/cups-ssl.m4)
sinclude(config-scripts/cups-pam.m4)
sinclude(config-scripts/cups-largefile.m4)
sinclude(config-scripts/cups-dnssd.m4)
sinclude(config-scripts/cups-startup.m4)
sinclude(config-scripts/cups-defaults.m4)

dnl See what languages are available and make sure we generate the localization
dnl files as needed...
INSTALL_LANGUAGES=""
UNINSTALL_LANGUAGES=""
LANGFILES=""
AS_IF([test "x$LANGUAGES" != x], [
    INSTALL_LANGUAGES="install-languages"
    UNINSTALL_LANGUAGES="uninstall-languages"
    for lang in $LANGUAGES; do
	AS_IF([test -f doc/$lang/index.html.in], [
	    LANGFILES="$LANGFILES doc/$lang/index.html"
	])

	AS_IF([test -f templates/$lang/header.tmpl.in], [
	    LANGFILES="$LANGFILES templates/$lang/header.tmpl"
	])
    done
], [test "x$CUPS_BUNDLEDIR" != x], [
    INSTALL_LANGUAGES="install-langbundle"
    UNINSTALL_LANGUAGES="uninstall-langbundle"
])

AC_SUBST([INSTALL_LANGUAGES])
AC_SUBST([UNINSTALL_LANGUAGES])

dnl Provide autoconf with a list of files to generate and output them...
AC_CONFIG_FILES([
    Makedefs
    conf/cups-files.conf
    conf/cupsd.conf
    conf/mime.convs
    conf/pam.std
    conf/snmp.conf
    cups.pc
    cups-config
    desktop/cups.desktop
    doc/index.html
    scheduler/cups-lpd.xinetd
    scheduler/cups.sh
    scheduler/cups.xml
    scheduler/org.cups.cups-lpd.plist
    scheduler/org.cups.cups-lpdAT.service
    scheduler/org.cups.cupsd.path
    scheduler/org.cups.cupsd.service
    scheduler/org.cups.cupsd.socket
    templates/header.tmpl
    packaging/cups.list
    $LANGFILES
])
AC_OUTPUT

dnl Make sure cups-config script is executable
chmod +x cups-config
