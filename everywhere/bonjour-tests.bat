::@echo off
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

:: Special case second argument: "_keys" and "_values" to show bad/missing TXT keys
if not "%2" == "" (
	echo "FAIL"
	echo "<key>Errors</key><array>" >>"%PLIST%"
	if not defined IPPFIND_TXT_ADMINURL (
		echo "   adminurl is not set."
		echo "<string>adminurl is not set.</string>" >>"%PLIST%"
	) else (
		if "%2" == "_values" (
			set result=FAIL
			set scheme="%IPPFIND_TXT_ADMINURL:~0,7%"
			if "%scheme%" == "http://" set result=PASS
			set scheme="%IPPFIND_TXT_ADMINURL:~0,8%"
			if "%scheme%" == "https://" set result=PASS
			if "%result%" == "FAIL" (
				echo "   adminurl has bad value '%IPPFIND_TXT_ADMINURL%'."
				echo "<string>adminurl has bad value '%IPPFIND_TXT_ADMINURL%'.</string>" >>"%PLIST%"
			)
		)
	)

	if not defined IPPFIND_TXT_PDL (
		echo "   pdl is not set."
		echo "<string>pdl is not set.</string>" >>"%PLIST%"
	) else (
		if "%2" == "_values" (
			set temp="%IPPFIND_TXT_PDL:image/jpeg=%"
			if "%temp%" == "%IPPFIND_TXT_PDL%" (
				echo "   pdl is missing image/jpeg: '%IPPFIND_TXT_PDL%'"
				echo "<string>pdl is missing image/jpeg: '%IPPFIND_TXT_PDL%'.</string>" >>"%PLIST%"
			)

			set temp="%IPPFIND_TXT_PDL:image/pwg-raster=%"
			if "%temp%" == "%IPPFIND_TXT_PDL%" (
				echo "   pdl is missing image/pwg-raster: '%IPPFIND_TXT_PDL%'"
				echo "<string>pdl is missing image/pwg-raster: '%IPPFIND_TXT_PDL%'.</string>" >>"%PLIST%"
			)
		)
	)

	if not defined IPPFIND_TXT_RP (
		echo "   rp is not set."
		echo "<string>rp is not set.</string>" >>"%PLIST%"
	) else (
		if "%2" == "_values" (
			if not "%IPPFIND_TXT_RP%" == "ipp/print" (
				if not "%IPPFIND_TXT_RP:~0,10%" == "ipp/print/" (
					echo "   rp has bad value '%IPPFIND_TXT_RP%'"
					echo "<string>rp has bad value '%IPPFIND_TXT_RP%'.</string>" >>"%PLIST%"
				)
			)
		)
	)

	if not defined IPPFIND_TXT_UUID (
		echo "   UUID is not set."
		echo "<string>UUID is not set.</string>" >>"%PLIST%"
	) else (
		if "%2" == "_values" (
			:: This isn't as effective as the test in bonjour-tests.sh but still
			:: catches the most common error...
			set scheme="%IPPFIND_TXT_UUID:~0,9%"
			if "%scheme%" == "urn:uuid:" (
				echo "   UUID has bad value '%IPPFIND_TXT_UUID%'"
				echo "<string>UUID has bad value '%IPPFIND_TXT_UUID%'.</string>" >>"%PLIST%"
			)
		)
	)

	if "%2" == "_values" (
		ipptool -t -d "ADMINURL=%IPPFIND_TXT_ADMINURL%" -d "UUID=%IPPFIND_TXT_UUID%" %IPPFIND_SERVICE_URI% bonjour-value-tests.test
		echo "<string>" >>"%PLIST%"
		$IPPTOOL -t -d "ADMINURL=$IPPFIND_TXT_ADMINURL" -d "UUID=$IPPFIND_TXT_UUID" $IPPFIND_SERVICE_URI bonjour-value-tests.test | findstr /r "[TD]:" >>"%PLIST%"
		echo "</string>" >>"%PLIST%"
	)

	echo "</array>" >>"%PLIST%"
	echo "<key>Successful</key><false />" >>"%PLIST%"
	echo "</dict>" >>"%PLIST%"

	goto :eof
)


:: Write the standard XML plist header...
echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" >"%PLIST%"
echo "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" >>"%PLIST%"
echo "<plist version=\"1.0\">" >>"%PLIST%"
echo "<dict>" >>"%PLIST%"
echo "<key>Tests</key><array>" >>"%PLIST%"

set total=0
set pass=0
set fail=0
set skip=0

:: B-1. IPP Browse test: Printers appear in a search for "_ipp._tcp,_print" services?
call :start_test "B-1. IPP Browse test"
set result=FAIL
ippfind _ipp._tcp,_print.local. --name "%1" --quiet && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
	call :end_test PASS
) else (
	set /a fail+=1
	call :end_test FAIL
)

:: B-2. IPP TXT keys test: The IPP TXT record contains all required keys.
call :start_test "B-2. IPP TXT keys test"
set result=FAIL
ippfind "%1._ipp._tcp.local." --txt adminurl --txt pdl --txt rp --txt UUID --quiet && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
	call :end_test PASS
) else (
	set /a fail+=1
	ippfind "%1._ipp._tcp.local." -x bonjour-tests.bat "{service_name}" _keys ";"
)

:: B-3. IPP Resolve test: Printer responds to an IPP Get-Printer-Attributes request using the resolved hostname, port, and resource path.
call :start_test "B-3. IPP Resolve test"
set result=FAIL
(ippfind "%1._ipp._tcp.local." --ls && set result=PASS) >nul:
if "%result%" == "PASS" (
	set /a pass+=1
	call :end_test PASS
) else (
	set /a fail+=1
	echo "<key>Errors</key><array><string>" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." --ls >>"%PLIST%"
	echo "</string></array>" >>"%PLIST%"
	call :end_test FAIL
)

:: B-4. IPP TXT values test: The IPP TXT record values match the reported IPP attribute values.
call :start_test "B-4. IPP TXT values test"
set result=FAIL
ippfind "%1._ipp._tcp.local." --txt-adminurl "^(http:|https:)//" --txt-pdl image/pwg-raster --txt-pdl image/jpeg --txt-rp "^ipp/(print|print/[^/]+)$" --txt-UUID "^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$" -x $IPPTOOL -q -d "ADMINURL={txt_adminurl}" -d "UUID={txt_uuid}" "{}" bonjour-value-tests.test ";" && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
	call :end_test PASS
) else (
	set /a fail+=1
	ippfind "%1._ipp._tcp.local." -x bonjour-tests.bat "{service_name}" _values ";"
)

:: B-5. TLS tests: Performed only if TLS is supported
call :start_test "B-5. TLS tests"
set result=FAIL
find "%1._ipp._tcp.local." --txt tls --quiet && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
	set HAVE_TLS=1
	call :end_test PASS
) else (
	set /a skip+=1
	set HAVE_TLS=0
	call :end_test SKIP
)

:: B-5.1 HTTP Upgrade test: Printer responds to an IPP Get-Printer-Attributes request after doing an HTTP Upgrade to TLS.
call :start_test "B-5.1 HTTP Upgrade test"
if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind "%1._ipp._tcp.local." -x ipptool -E -q "{}" bonjour-access-tests.test ";" && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
		call :end_test PASS
	) else (
		set /a fail+=1
		echo "<key>Errors</key><array><string>" >>"%PLIST"
		ippfind "%1._ipp._tcp.local." -x ipptool -E -q "{}" bonjour-access-tests.test ";" >>"%PLIST%"
		echo "</string></array>" >>"%PLIST%"
		call :end_test FAIL
	)
) else (
	set /a skip+=1
	call :end_test SKIP
)

:: B-5.2 IPPS Browse test: Printer appears in a search for "_ipps._tcp,_print" services.
call :start_test "B-5.2 IPPS Browse test"
if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind _ipps._tcp,_print.local. --name "%1" --quiet && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
		call :end_test PASS
	) else (
		set /a fail+=1
		call :end_test FAIL
	)
) else (
	set /a skip+=1
	call :end_test SKIP
)

:: B-5.3 IPPS TXT keys test: The TXT record for IPPS contains all required keys
call :start_test "B-5.3 IPPS TXT keys test"
if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind "%1._ipps._tcp.local." --txt adminurl --txt pdl --txt rp --txt TLS --txt UUID --quiet && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
		call :end_test PASS
	) else (
		set /a fail+=1
		ippfind "%1._ipps._tcp.local." -x bonjour-tests.bat "{service_name}" _keys ";"
	)
) else (
	set /a skip+=1
	call :end_test SKIP
)

:: B-5.4 IPPS Resolve test: Printer responds to an IPPS Get-Printer-Attributes request using the resolved hostname, port, and resource path.
call :start_test "B-5.4 IPPS Resolve test"
if "%HAVE_TLS%" == "1" (
	set result=FAIL
	(ippfind "%1._ipps._tcp.local." --ls && set result=PASS) >nul:
	if "%result%" == "PASS" (
		set /a pass+=1
		call :end_test PASS
	) else (
		set /a fail+=1
		echo "<key>Errors</key><array><string>" >>"%PLIST%"
		ippfind "%1._ipps._tcp.local." --ls >>"%PLIST%"
		echo "</string></array>" >>"%PLIST%"
		call :end_test FAIL
	)
) else (
	set /a skip+=1
	call :end_test SKIP
)

:: B-5.5 IPPS TXT values test: The TXT record values for IPPS match the reported IPPS attribute values.
call :start_test "B-5.5 IPPS TXT values test"
if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind "%1._ipps._tcp.local." --txt-adminurl "^(http:|https:)//" --txt-pdl image/pwg-raster --txt-pdl image/jpeg --txt-rp "^ipp/(print|print/[^/]+)$" --txt-UUID "^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$" -x ipptool -q -d "ADMINURL={txt_adminurl}" -d "UUID={txt_uuid}" "{}" bonjour-value-tests.test ";" && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
		call :end_test PASS
	) else (
		set /a fail+=1
		ippfind "%1._ipps._tcp.local." -x bonjour-tests.bat "{service_name}" _values ";"
	)
) else (
	set /a skip+=1
	call :end_test SKIP
)

:: Finish up...
echo "</array>" >>"%PLIST%"
echo "<key>Successful</key>" >>"%PLIST%"
if %fail% gtr 0 (
	echo "<false />" >>"%PLIST%"
) else (
	echo "<true />" >>"%PLIST%"
)
echo "</dict>" >>"%PLIST%"
echo "</plist>" >>"%PLIST%"

set /a score=%pass% + %skip%
set /a score=100 * %score% / %total%
echo "Summary: %total% tests, %pass% passed, %fail% failed, %skip% skipped"
echo "Score: %score%^%"

exit

:: call :start_test "name"
:start_test
	set /a total+=1
	setlocal
	set name=%1
	set name=%name:~1,-1%
	set <NUL /p="%name%: "
	echo "<dict><key>Name</key><string>%name%</string>" >>"%PLIST%"
	echo "<key>FileId</key><string>org.pwg.ipp-everywhere.20140826.bonjour</string>" >>"%PLIST%"
	endlocal
	goto :eof

:: call :end_test PASS/FAIL/SKIP
:end_test
	setlocal
	echo %1
	if "%1" == "FAIL" (
		echo "<key>Successful</key><false />" >>"%PLIST%"
	) else (
		if "%1" == "SKIP" (
			echo "<key>Successful</key><true />" >>"%PLIST%"
			echo "<key>Skipped</key><true />" >>"%PLIST%"
		) else (
			echo "<key>Successful</key><true />" >>"%PLIST%"
		)
	)
	echo "</dict>" >>"%PLIST%"
	endlocal
	goto :eof

::
:: End of "$Id$".
::
