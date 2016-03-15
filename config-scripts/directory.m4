#
# "$Id: library.m4 44 2006-05-08 19:33:22Z mike $"
#
# Common directory stuff for autoconf...
#

dnl Get the operating system, version number, and architecture...
uname=`uname`
uversion=`uname -r | sed -e '1,$s/^[[^0-9]]*\([[0-9]]*\)\.\([[0-9]]*\).*/\1\2/'`
uarch=`uname -m`

case "$uname" in
	GNU* | GNU/*)
		uname="GNU"
		;;
	IRIX*)
		uname="IRIX"
		;;
	Linux*)
		uname="Linux"
		;;
esac

dnl Fix "prefix" variable if it hasn't been specified...
if test "$prefix" = "NONE"; then
	prefix="/"
fi

dnl Fix "exec_prefix" variable if it hasn't been specified...
if test "$exec_prefix" = "NONE"; then
	if test "$prefix" = "/"; then
		exec_prefix="/usr"
	else
		exec_prefix="$prefix"
	fi
fi

dnl Fix "bindir" variable...
if test "$bindir" = "\${exec_prefix}/bin"; then
	bindir="$exec_prefix/bin"
fi

dnl Fix "sbindir" variable...
if test "$sbindir" = "\${exec_prefix}/sbin"; then
	sbindir="$exec_prefix/sbin"
fi

dnl Fix "sharedstatedir" variable if it hasn't been specified...
if test "$sharedstatedir" = "\${prefix}/com" -a "$prefix" = "/"; then
	sharedstatedir="/usr/com"
fi

dnl Fix "datarootdir" variable if it hasn't been specified...
if test "$datarootdir" = "\${prefix}/share"; then
	if test "$prefix" = "/"; then
		datarootdir="/usr/share"
	else
		datarootdir="$prefix/share"
	fi
fi

dnl Fix "datadir" variable if it hasn't been specified...
if test "$datadir" = "\${prefix}/share"; then
	if test "$prefix" = "/"; then
		datadir="/usr/share"
	else
		datadir="$prefix/share"
	fi
elif test "$datadir" = "\${datarootdir}"; then
	datadir="$datarootdir"
fi

dnl Fix "includedir" variable if it hasn't been specified...
if test "$includedir" = "\${prefix}/include" -a "$prefix" = "/"; then
	includedir="/usr/include"
fi

dnl Fix "localstatedir" variable if it hasn't been specified...
if test "$localstatedir" = "\${prefix}/var"; then
	if test "$prefix" = "/"; then
		if test "$uname" = Darwin; then
			localstatedir="/private/var"
		else
			localstatedir="/var"
		fi
	else
		localstatedir="$prefix/var"
	fi
fi

dnl Fix "sysconfdir" variable if it hasn't been specified...
if test "$sysconfdir" = "\${prefix}/etc"; then
	if test "$prefix" = "/"; then
		if test "$uname" = Darwin; then
			sysconfdir="/private/etc"
		else
			sysconfdir="/etc"
		fi
	else
		sysconfdir="$prefix/etc"
	fi
fi

dnl Fix "libdir" variable for IRIX 6.x...
if test "$libdir" = "\${exec_prefix}/lib"; then
	if test "$uname" = "IRIX"; then
		libdir="$exec_prefix/lib32"
	else
		if test "$uname" = Linux -a -d /usr/lib64; then
			libdir="$exec_prefix/lib64"
		else
			libdir="$exec_prefix/lib"
		fi
	fi
fi

#
# End of "$Id: library.m4 44 2006-05-08 19:33:22Z mike $".
#
