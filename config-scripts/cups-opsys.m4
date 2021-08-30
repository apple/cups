dnl
dnl Operating system stuff for CUPS.
dnl
dnl Copyright © 2021 by OpenPrinting.
dnl Copyright © 2007-2019 by Apple Inc.
dnl Copyright © 1997-2006 by Easy Software Products, all rights reserved.
dnl
dnl Licensed under Apache License v2.0.  See the file "LICENSE" for more
dnl information.
dnl

dnl Get the build and host platforms and split the host_os value
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

[host_os_name="$(echo $host_os | sed -e '1,$s/[0-9.]*$//g')"]
[host_os_version="$(echo $host_os | sed -e '1,$s/^[^0-9.]*//g' | awk -F. '{print $1 $2}')"]
# Linux often does not yield an OS version we can use...
AS_IF([test "x$host_os_version" = x], [
    host_os_version="0"
])

dnl Determine whether we are cross-compiling...
AS_IF([test "$build" = "$host"], [
    # No, build local targets
    LOCALTARGET="local"
], [
    # Yes, don't build local targets
    LOCALTARGET=""
])
AC_SUBST([LOCALTARGET])

AC_PATH_PROGS([CODE_SIGN], [codesign true])
