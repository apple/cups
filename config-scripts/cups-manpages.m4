dnl
dnl Manpage stuff for CUPS.
dnl
dnl Copyright © 2007-2019 by Apple Inc.
dnl Copyright © 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Fix "mandir" variable...
if test "$mandir" = "\${datarootdir}/man" -a "$prefix" = "/"; then
	# New GNU "standards" break previous ones, so make sure we use
	# the right default location for the operating system...
	mandir="\${prefix}/man"
fi

if test "$mandir" = "\${prefix}/man" -a "$prefix" = "/"; then
	case "$host_os_name" in
        	darwin* | linux* | gnu* | *bsd*)
        		# Darwin, macOS, Linux, GNU HURD, and *BSD
        		mandir="/usr/share/man"
        		;;
        	*)
        		# All others
        		mandir="/usr/man"
        		;;
	esac
fi
