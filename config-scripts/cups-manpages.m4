dnl
dnl Manpage stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2019 by Apple Inc.
dnl Copyright © 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Fix "mandir" variable...
AS_IF([test "$mandir" = "\${datarootdir}/man" -a "$prefix" = "/"], [
    # New GNU "standards" break previous ones, so make sure we use
    # the right default location for the operating system...
    mandir="\${prefix}/man"
])

AS_IF([test "$mandir" = "\${prefix}/man" -a "$prefix" = "/"], [
    AS_CASE(["$host_os_name"], [darwin* | linux* | gnu* | *bsd*], [
	# Darwin, macOS, Linux, GNU HURD, and *BSD
	mandir="/usr/share/man"
    ], [*], [
	# All others
	mandir="/usr/man"

    ])
])
