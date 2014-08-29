@echo off
::
::  "$Id$"
::
:: IPP Everywhere Printer Self-Certification Manual 1.0: Section 5: Bonjour Tests.
::
:: Copyright 2014 by The Printer Working Group.
::
:: This program may be copied and furnished to others, and derivative works
:: that comment on, or otherwise explain it or assist in its implementation may
:: be prepared, copied, published and distributed, in whole or in part, without
:: restriction of any kind, provided that the above copyright notice and this
:: paragraph are included on all such copies and derivative works.
::
:: The IEEE-ISTO and the Printer Working Group DISCLAIM ANY AND ALL WARRANTIES,
:: WHETHER EXPRESS OR IMPLIED INCLUDING (WITHOUT LIMITATION) ANY IMPLIED
:: WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
::
:: Usage:
::
::   bonjour-tests.bat 'Printer Name'
::

set PLIST="%1 Bonjour Results.plist"

:: Special case "_failN" name to show bad/missing TXT keys
if not "%2" == "" {
	echo "FAIL"
	echo "<key>Errors</key><array>" >>"%PLIST%"
	if not defined IPPFIND_TXT_ADMINURL {
		echo "   adminurl is not set."
		echo "<string>adminurl is not set.</string>" >>"%PLIST%"
	} else {
		if test "%2" == "_value" {
			if not %IPPFIND_TXT_ADMINURL:~0,7% == "http://" {
				if not %IPPFIND_TXT_ADMINURL:~0,8% == "https://" {
					echo "   adminurl has bad value '%IPPFIND_TXT_ADMINURL%'."
					echo "<string>adminurl has bad value '%IPPFIND_TXT_ADMINURL%'.</string>" >>"%PLIST%"
				}
			}
		}
	}

	if not defined IPPFIND_TXT_PDL {
		echo "   pdl is not set."
		echo "<string>pdl is not set.</string>" >>"%PLIST%"
	} else {
		if "%2" == "_value" {
			set temp=%IPPFIND_TXT_PDL:image/jpeg=%
			if "%temp%" == "%IPPFIND_TXT_PDL%" {
				echo "   pdl is missing image/jpeg: '%IPPFIND_TXT_PDL%'"
				echo "<string>pdl is missing image/jpeg: '%IPPFIND_TXT_PDL%'.</string>" >>"%PLIST%"
			}

			set temp=%IPPFIND_TXT_PDL:image/pwg-raster=%
			if "%temp%" == "%IPPFIND_TXT_PDL%" {
				echo "   pdl is missing image/pwg-raster: '%IPPFIND_TXT_PDL%'"
				echo "<string>pdl is missing image/pwg-raster: '%IPPFIND_TXT_PDL%'.</string>" >>"%PLIST%"
			}
		}
	}

	if not defined IPPFIND_TXT_RP {
		echo "   rp is not set."
		echo "<string>rp is not set.</string>" >>"%PLIST%"
	} else {
		if "%2" == "_value" {
			if not "%IPPFIND_TXT_RP%" == "ipp/print" {
				if not "%IPPFIND_TXT_RP:~0,10%" == "ipp/print/" {
					echo "   rp has bad value '%IPPFIND_TXT_RP%'"
					echo "<string>rp has bad value '%IPPFIND_TXT_RP%'.</string>" >>"%PLIST%"
				}
			}
		}
	}

	if not defined IPPFIND_TXT_UUID {
		echo "   UUID is not set."
		echo "<string>UUID is not set.</string>" >>"%PLIST%"
	} else {
		if "%2" == "_value" {
			:: This isn't as effective as the test in bonjour-tests.sh...
			if "%IPPFIND_TXT_UUID:~0,9%" == "urn:uuid:" {
				echo "   UUID has bad value '%IPPFIND_TXT_UUID%'"
				echo "<string>UUID has bad value '%IPPFIND_TXT_UUID%'.</string>" >>"%PLIST%"
			}
		}
	}

	if "%2" == "_value" {
		ipptool -t -d "ADMINURL=%IPPFIND_TXT_ADMINURL%" -d "UUID=%IPPFIND_TXT_UUID%" %IPPFIND_SERVICE_URI% bonjour-value-tests.test
		echo "<string>" >>"%PLIST%"
		$IPPTOOL -t -d "ADMINURL=$IPPFIND_TXT_ADMINURL" -d "UUID=$IPPFIND_TXT_UUID" $IPPFIND_SERVICE_URI bonjour-value-tests.test | findstr /r '[TD]:' >>"%PLIST%"
		echo "</string>" >>"%PLIST%"
	}

	echo "</array>" >>"%PLIST%"
	echo "<key>Successful</key><false />" >>"%PLIST%"
	echo "</dict>" >>"%PLIST%"

	exit
}


:: Write the standard XML plist header...
echo '<?xml version="1.0" encoding="UTF-8"?>' >"%PLIST%"
echo '<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >>"%PLIST%"
echo '<plist version="1.0">' >>"%PLIST%"
echo '<dict>' >>"%PLIST%"
echo '<key>Tests</key><array>' >>"%PLIST%"

set total=0
set pass=0
set fail=0
set skip=0

:: B-1. IPP Browse test: Printers appear in a search for "_ipp._tcp,_print" services?
start_test "B-1. IPP Browse test"
$IPPFIND _ipp._tcp,_print.local. --name "$1" --quiet
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	end_test FAIL
fi

:: B-2. IPP TXT keys test: The IPP TXT record contains all required keys.
start_test "B-2. IPP TXT keys test"
$IPPFIND "$1._ipp._tcp.local." --txt adminurl --txt pdl --txt rp --txt UUID --quiet
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	$IPPFIND "$1._ipp._tcp.local." -x ./bonjour-tests.sh '{service_name}' _fail2 \;
fi

:: B-3. IPP Resolve test: Printer responds to an IPP Get-Printer-Attributes request using the resolved hostname, port, and resource path.
start_test "B-3. IPP Resolve test"
$IPPFIND "$1._ipp._tcp.local." --ls >/dev/null
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	echo "<key>Errors</key><array>" >>"%PLIST%"
	$IPPFIND "$1._ipp._tcp.local." --ls | awk '{ print "<string>" $0 "</string>" }' >>"%PLIST%"
	echo "</array>" >>"%PLIST%"
	end_test FAIL
fi

:: B-4. IPP TXT values test: The IPP TXT record values match the reported IPP attribute values.
start_test "B-4. IPP TXT values test"
$IPPFIND "$1._ipp._tcp.local." --txt-adminurl '^(http:|https:)//' --txt-pdl 'image/pwg-raster' --txt-pdl 'image/jpeg' --txt-rp '^ipp/(print|print/[^/]+)$' --txt-UUID '^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$' -x $IPPTOOL -q -d 'ADMINURL={txt_adminurl}' -d 'UUID={txt_uuid}' '{}' bonjour-value-tests.test \;
if test $? = 0; then
	pass=`expr $pass + 1`
	end_test PASS
else
	fail=`expr $fail + 1`
	$IPPFIND "$1._ipp._tcp.local." -x ./bonjour-tests.sh '{service_name}' _fail4 \;
fi

:: B-5. TLS tests: Performed only if TLS is supported
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

:: B-5.1 HTTP Upgrade test: Printer responds to an IPP Get-Printer-Attributes request after doing an HTTP Upgrade to TLS.
start_test "B-5.1 HTTP Upgrade test"
if test $HAVE_TLS = 1; then
	error=`$IPPFIND "$1._ipp._tcp.local." -x $IPPTOOL -E -q '{}' bonjour-access-tests.test \; 2>&1`
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		echo "<key>Errors</key><array><string>$error</string></array>" >>"%PLIST%"

		end_test FAIL
		echo "    $error"
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

:: B-5.2 IPPS Browse test: Printer appears in a search for "_ipps._tcp,_print" services.
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

:: B-5.3 IPPS TXT keys test: The TXT record for IPPS contains all required keys
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

:: B-5.4 IPPS Resolve test: Printer responds to an IPPS Get-Printer-Attributes request using the resolved hostname, port, and resource path.
start_test "B-5.4 IPPS Resolve test"
if test $HAVE_TLS = 1; then
	$IPPFIND "$1._ipps._tcp.local." --ls >/dev/null
	if test $? = 0; then
		pass=`expr $pass + 1`
		end_test PASS
	else
		fail=`expr $fail + 1`
		echo "<key>Errors</key><array>" >>"%PLIST%"
		$IPPFIND "$1._ipps._tcp.local." --ls | awk '{ print "<string>" $0 "</string>" }' >>"%PLIST%"
		echo "</array>" >>"%PLIST%"
		end_test FAIL
	fi
else
	skip=`expr $skip + 1`
	end_test SKIP
fi

:: B-5.5 IPPS TXT values test: The TXT record values for IPPS match the reported IPPS attribute values.
start_test "B-5.5 IPPS TXT values test"
if test $HAVE_TLS = 1; then
	$IPPFIND "$1._ipps._tcp.local." --txt-adminurl '^(http:|https:)//' --txt-pdl 'image/pwg-raster' --txt-pdl 'image/jpeg' --txt-rp '^ipp/(print|print/[^/]+)$' --txt-UUID '^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$' -x $IPPTOOL -q -d 'ADMINURL={txt_adminurl}' -d 'UUID={txt_uuid}' '{}' bonjour-value-tests.test \;
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

:: Finish up...
if test $fail -gt 0; then
	cat >>"%PLIST%" <<EOF
</array>
<key>Successful</key>
<false />
EOF
else
	cat >>"%PLIST%" <<EOF
</array>
<key>Successful</key>
<true />
EOF
fi

cat >>"%PLIST%" <<EOF
</dict>
</plist>
EOF

score=`expr $pass + $skip`
score=`expr 100 \* $score / $total`
echo "Summary: $total tests, $pass passed, $fail failed, $skip skipped"
echo "Score: ${score}%"

exit

:: call :start_test "name"
:start_test
	setlocal
	set /a total=total+1
	set <NUL /p="%1: "
	echo "<dict><key>Name</key><string>$1</string>" >>"%PLIST%"
	echo "<key>FileId</key><string>org.pwg.ipp-everywhere.20140826.bonjour</string>" >>"%PLIST%"
	endlocal
	goto :eof

:: call :end_test PASS/FAIL/SKIP
:end_test
	setlocal
	echo %1
	if "%1" == "FAIL" {
		echo "<key>Successful</key><false />" >>"%PLIST%"
	} else {
		if "%1" == "SKIP" {
			echo "<key>Successful</key><true />" >>"%PLIST%"
			echo "<key>Skipped</key><true />" >>"%PLIST%"
		} else {
			echo "<key>Successful</key><true />" >>"%PLIST%"
		}
	}
	echo "</dict>" >>"%PLIST%"
	endlocal
	goto :eof

::
:: End of "$Id$".
::
