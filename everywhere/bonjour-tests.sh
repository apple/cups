#!/bin/sh
#
#  "$Id$"
#
# IPP Everywhere Printer Self-Certification Manual 1.0: Section 5: Bonjour Tests.
#
# Copyright 2014 by The Printer Working Group.
#
# This program may be copied and furnished to others, and derivative works
# that comment on, or otherwise explain it or assist in its implementation may
# be prepared, copied, published and distributed, in whole or in part, without
# restriction of any kind, provided that the above copyright notice and this
# paragraph are included on all such copies and derivative works.
#
# The IEEE-ISTO and the Printer Working Group DISCLAIM ANY AND ALL WARRANTIES,
# WHETHER EXPRESS OR IMPLIED INCLUDING (WITHOUT LIMITATION) ANY IMPLIED
# WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Usage:
#
#   ./bonjour-tests.sh "Printer Name"
#

if test -x ../test/ippfind-static; then
	IPPFIND="../test/ippfind-static"
elif test -x ./ippfind; then
	IPPFIND="./ippfind"
else
	IPPFIND="ippfind"
fi

if test -x ../test/ipptool-static; then
	IPPTOOL="../test/ipptool-static"
elif test -x ./ipptool; then
	IPPTOOL="./ipptool"
else
	IPPTOOL="ipptool"
fi

PLIST="$1 Bonjour Results.plist"

#
# Figure out the proper echo options...
#

if (echo "testing\c"; echo 1,2,3) | grep c >/dev/null; then
        ac_n=-n
        ac_c=
else
        ac_n=
        ac_c='\c'
fi

# Special case "_failN" name to show bad/missing TXT keys
if test "$2" = _fail2 -o "$2" = _fail4 -o "$2" = _fail5.3 -o "$2" = _fail5.5; then
	echo "FAIL"
	echo "<key>Errors</key><array>" >>"$PLIST"
	if test "${IPPFIND_TXT_ADMINURL:-NOTSET}" = NOTSET; then
		echo "   adminurl is not set."
		echo "<string>adminurl is not set.</string>" >>"$PLIST"
	elif test "$2" = _fail4 -o "$2" = _fail5.5; then
		case "$IPPFIND_TXT_ADMINURL" in
			http://* | https://*)
				;;
			*)
				echo "   adminurl has bad value '$IPPFIND_TXT_ADMINURL'."
				echo "<string>adminurl has bad value '$IPPFIND_TXT_ADMINURL'.</string>" >>"$PLIST"
				;;
		esac
	fi

	if test "${IPPFIND_TXT_PDL:-NOTSET}" = NOTSET; then
		echo "   pdl is not set."
		echo "<string>pdl is not set.</string>" >>"$PLIST"
	elif test "$2" = _fail4 -o "$2" = _fail5.5; then
		case "$IPPFIND_TXT_PDL" in
			*image/jpeg*)
				;;
			*)
				echo "   pdl is missing image/jpeg: '$IPPFIND_TXT_PDL'"
				echo "<string>pdl is missing image/jpeg: '$IPPFIND_TXT_PDL'.</string>" >>"$PLIST"
				;;
		esac

		case "$IPPFIND_TXT_PDL" in
			*image/pwg-raster*)
				;;
			*)
				echo "   pdl is missing image/pwg-raster: '$IPPFIND_TXT_PDL'"
				echo "<string>pdl is missing image/pwg-raster: '$IPPFIND_TXT_PDL'.</string>" >>"$PLIST"
				;;
		esac
	fi

	if test "${IPPFIND_TXT_RP:-NOTSET}" = NOTSET; then
		echo "   rp is not set."
		echo "<string>rp is not set.</string>" >>"$PLIST"
	elif test "$2" = _fail4 -o "$2" = _fail5.5; then
		case "$IPPFIND_TXT_RP" in
			ipp/print | ipp/print/*)
				;;
			*)
				echo "   rp has bad value '$IPPFIND_TXT_RP'"
				echo "<string>rp has bad value '$IPPFIND_TXT_RP'.</string>" >>"$PLIST"
				;;
		esac
	fi

	if test "${IPPFIND_TXT_UUID:-NOTSET}" = NOTSET; then
		echo "   UUID is not set."
		echo "<string>UUID is not set.</string>" >>"$PLIST"
	elif test "$2" = _fail4 -o "$2" = _fail5.5; then
		case "$IPPFIND_TXT_UUID" in
			[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]-[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]-[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]-[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]-[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])
				;;
			*)
				echo "   UUID has bad value '$IPPFIND_TXT_UUID'"
				echo "<string>UUID has bad value '$IPPFIND_TXT_UUID'.</string>" >>"$PLIST"
				;;
		esac
	fi

	if test "$2" = _fail4 -o "$2" = _fail5.5; then
		$IPPTOOL -t $IPPFIND_SERVICE_URI bonjour-value-tests.test
		$IPPTOOL -t $IPPFIND_SERVICE_URI bonjour-value-tests.test | egrep '(GOT|EXPECTED):' | sed -e '1,$s/^[ 	]*//' | awk '{print "<string>" $0 "</string>" }' >>"$PLIST"
	fi

	echo "</array>" >>"$PLIST"
	echo "<key>Successful</key><false />" >>"$PLIST"
	echo "</dict>" >>"$PLIST"

	exit 0
fi


# Write the standard XML plist header...
cat >"$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
<key>Tests</key><array>
EOF

total=0
pass=0
fail=0
skip=0

# start_test "name"
start_test() {
	total=`expr $total + 1`
	echo $ac_n "$1: $ac_c"
	echo "<dict><key>Name</key><string>$1</string>" >>"$PLIST"
	echo "<key>FileId</key><string>org.pwg.ipp-everywhere.20140826.bonjour</string>" >>"$PLIST"
}
# end_test PASS/FAIL/SKIP
end_test() {
	echo $1
	if test $1 = FAIL; then
		echo "<key>Successful</key><false />" >>"$PLIST"
	elif test $1 = SKIP; then
		echo "<key>Successful</key><true />" >>"$PLIST"
		echo "<key>Skipped</key><true />" >>"$PLIST"
	else
		echo "<key>Successful</key><true />" >>"$PLIST"
	fi
	echo "</dict>" >>"$PLIST"
}

# B-1. IPP Browse test: Printers appear in a search for "_ipp._tcp,_print" services?
start_test "B-1. IPP Browse test"
$IPPFIND _ipp._tcp,_print.local. --name "$1" --quiet
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	end_test FAIL
fi

# B-2. IPP TXT keys test: The IPP TXT record contains all required keys.
start_test "B-2. IPP TXT keys test"
$IPPFIND "$1._ipp._tcp.local." --txt adminurl --txt pdl --txt rp --txt UUID --quiet
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	$IPPFIND "$1._ipp._tcp.local." -x ./bonjour-tests.sh '{service_name}' _fail2 \;
fi

# B-3. IPP Resolve test: Printer responds to an IPP Get-Printer-Attributes request using the resolved hostname, port, and resource path.
start_test "B-3. IPP Resolve test"
$IPPFIND "$1._ipp._tcp.local." --ls >/dev/null
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	echo "<key>Errors</key><array>" >>"$PLIST"
	$IPPFIND "$1._ipp._tcp.local." --ls | awk '{ print "<string>" $0 "</string>" }' >>"$PLIST"
	echo "</array>" >>"$PLIST"
	end_test FAIL
fi

# B-4. IPP TXT values test: The IPP TXT record values match the reported IPP attribute values.
start_test "B-4. IPP TXT values test"
$IPPFIND "$1._ipp._tcp.local." --txt-adminurl '^(http:|https:)//' --txt-pdl 'image/pwg-raster' --txt-pdl 'image/jpeg' --txt-rp '^ipp/(print|print/[^/]+)$' --txt-UUID '^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$' -x $IPPTOOL -q '{}' bonjour-value-tests.test \;
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	$IPPFIND "$1._ipp._tcp.local." -x ./bonjour-tests.sh '{service_name}' _fail4 \;
fi

# B-5. TLS tests: Performed only if TLS is supported
start_test "B-5. TLS tests"
$IPPFIND "$1._ipp._tcp.local." --txt tls --quiet
if test $? = 0; then
	pass=`expr $pass + 1`
	HAVE_TLS=1
	end_test PASS
else
	skip=`expr $skip + 1`
	HAVE_TLS=0
	end_test SKIP
fi

# B-5.1 HTTP Upgrade test: Printer responds to an IPP Get-Printer-Attributes request after doing an HTTP Upgrade to TLS.
start_test "B-5.1 HTTP Upgrade test"
if test $HAVE_TLS = 1; then
	error=`$IPPFIND "$1._ipp._tcp.local." -x $IPPTOOL -E -q '{}' bonjour-access-tests.test \; 2>&1`
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		echo "<key>Errors</key><array><string>$error</string></array>" >>"$PLIST"

		end_test FAIL
		echo "    $error"
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

# B-5.2 IPPS Browse test: Printer appears in a search for "_ipps._tcp,_print" services.
start_test "B-5.2 IPPS Browse test"
if test $HAVE_TLS = 1; then
	$IPPFIND _ipps._tcp,_print.local. --name "$1" --quiet
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		end_test FAIL
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

# B-5.3 IPPS TXT keys test: The TXT record for IPPS contains all required keys
start_test "B-5.3 IPPS TXT keys test"
if test $HAVE_TLS = 1; then
	$IPPFIND "$1._ipps._tcp.local." --txt adminurl --txt pdl --txt rp --txt TLS --txt UUID --quiet
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		$IPPFIND "$1._ipps._tcp.local." -x ./bonjour-tests.sh '{service_name}' _fail5.3 \;
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

# B-5.4 IPPS Resolve test: Printer responds to an IPPS Get-Printer-Attributes request using the resolved hostname, port, and resource path.
start_test "B-5.4 IPPS Resolve test"
if test $HAVE_TLS = 1; then
	$IPPFIND "$1._ipps._tcp.local." --ls >/dev/null
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		echo "<key>Errors</key><array>" >>"$PLIST"
		$IPPFIND "$1._ipps._tcp.local." --ls | awk '{ print "<string>" $0 "</string>" }' >>"$PLIST"
		echo "</array>" >>"$PLIST"
		end_test FAIL
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

# B-5.5 IPPS TXT values test: The TXT record values for IPPS match the reported IPPS attribute values.
start_test "B-5.5 IPPS TXT values test"
if test $HAVE_TLS = 1; then
	$IPPFIND "$1._ipps._tcp.local." --txt-adminurl '^(http:|https:)//' --txt-pdl 'image/pwg-raster' --txt-pdl 'image/jpeg' --txt-rp '^ipp/(print|print/[^/]+)$' --txt-UUID '^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$' -x $IPPTOOL -q '{}' bonjour-value-tests.test \;
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		$IPPFIND "$1._ipps._tcp.local." -x ./bonjour-tests.sh '{service_name}' _fail5.5 \;
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

# Finish up...
if test $fail -gt 0; then
	cat >>"$PLIST" <<EOF
</array>
<key>Successful</key>
<false />
EOF
else
	cat >>"$PLIST" <<EOF
</array>
<key>Successful</key>
<true />
EOF
fi

cat >>"$PLIST" <<EOF
</dict>
</plist>
EOF

score=`expr $pass + $skip`
score=`expr 100 \* $score / $total`
echo "Summary: $total tests, $pass passed, $fail failed, $skip skipped"
echo "Score: ${score}%"

#
# End of "$Id$".
#
