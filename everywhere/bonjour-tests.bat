@echo off
::
::  "$Id: bonjour-tests.bat 12249 2014-11-14 12:54:05Z msweet $"
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

set PLIST=%1 Bonjour Results.plist
echo Sending output to "%PLIST%"...

:: Write the standard XML plist header...
echo ^<?xml version=^"1.0^" encoding=^"UTF-8^"?^> >"%PLIST%"
echo ^<!DOCTYPE plist PUBLIC ^"-//Apple Computer//DTD PLIST 1.0//EN^" ^"http://www.apple.com/DTDs/PropertyList-1.0.dtd^"^> >>"%PLIST%"
echo ^<plist version=^"1.0^"^> >>"%PLIST%"
echo ^<dict^> >>"%PLIST%"
echo ^<key^>Tests^</key^>^<array^> >>"%PLIST%"

set total=0
set pass=0
set fail=0
set skip=0

:: B-1. IPP Browse test: Printers appear in a search for "_ipp._tcp,_print" services?
set /a total+=1
set <NUL /p="B-1. IPP Browse test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-1. IPP Browse test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

set result=FAIL
ippfind _ipp._tcp,_print.local. --name "%1" --quiet && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
) else (
	set /a fail+=1
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"
) else (
	echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
)
echo ^</dict^> >>"%PLIST%"

:: B-2. IPP TXT keys test: The IPP TXT record contains all required keys.
set /a total+=1
set <NUL /p="B-2. IPP TXT keys test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-2. IPP TXT keys test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

set result=FAIL
ippfind "%1._ipp._tcp.local." --txt adminurl --txt pdl --txt rp --txt UUID --quiet && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
) else (
	set /a fail+=1
	echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo adminurl="{txt_adminurl}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo pdl="{txt_pdl}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo rp="{txt_rp}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo UUID="{txt_uuid}" ";" >>"%PLIST%"
	echo ^</string^>^</array^> >>"%PLIST%"
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"

	ippfind "%1._ipp._tcp.local." -x echo adminurl="{txt_adminurl}" ";"
	ippfind "%1._ipp._tcp.local." -x echo pdl="{txt_pdl}" ";"
	ippfind "%1._ipp._tcp.local." -x echo rp="{txt_rp}" ";"
	ippfind "%1._ipp._tcp.local." -x echo UUID="{txt_uuid}" ";"
) else (
	echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
)
echo ^</dict^> >>"%PLIST%"

:: B-3. IPP Resolve test: Printer responds to an IPP Get-Printer-Attributes request using the resolved hostname, port, and resource path.
set /a total+=1
set <NUL /p="B-3. IPP Resolve test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-3. IPP Resolve test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

set result=FAIL
(ippfind "%1._ipp._tcp.local." --ls && set result=PASS) >nul:
if "%result%" == "PASS" (
	set /a pass+=1
) else (
	set /a fail+=1
	echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." --ls >>"%PLIST%"
	echo ^</string^>^</array^> >>"%PLIST%"
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"
) else (
	echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
)
echo ^</dict^> >>"%PLIST%"

:: B-4. IPP TXT values test: The IPP TXT record values match the reported IPP attribute values.
set /a total+=1
set <NUL /p="B-4. IPP TXT values test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-4. IPP TXT values test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

set result=FAIL
ippfind "%1._ipp._tcp.local." --txt-adminurl ^^^(http:^|https:^)// --txt-pdl image/pwg-raster --txt-pdl image/jpeg --txt-rp ^^ipp/^(print^|print/[^^/]+^)$ --txt-UUID ^^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$ -x ipptool -q -d "ADMINURL={txt_adminurl}" -d "UUID={txt_uuid}" "{}" bonjour-value-tests.test ";" && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
) else (
	set /a fail+=1
	echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo adminurl="{txt_adminurl}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo pdl="{txt_pdl}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo rp="{txt_rp}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x echo UUID="{txt_uuid}" ";" >>"%PLIST%"
	ippfind "%1._ipp._tcp.local." -x ipptool -t "{}" bonjour-value-tests.test ";" | findstr /r [TG][EO][DT]: >>"%PLIST%"
	echo ^</string^>^</array^> >>"%PLIST%"
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"

	ippfind "%1._ipp._tcp.local." -x echo adminurl="{txt_adminurl}" ";"
	ippfind "%1._ipp._tcp.local." -x echo pdl="{txt_pdl}" ";"
	ippfind "%1._ipp._tcp.local." -x echo rp="{txt_rp}" ";"
	ippfind "%1._ipp._tcp.local." -x echo UUID="{txt_uuid}" ";"
	ippfind "%1._ipp._tcp.local." -x ipptool -t "{}" bonjour-value-tests.test ";" | findstr /r [TG][EO][DT]:
) else (
	echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
)
echo ^</dict^> >>"%PLIST%"

:: B-5. TLS tests: Performed only if TLS is supported
set /a total+=1
set <NUL /p="B-5. TLS tests: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-5. TLS tests^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

set result=SKIP
ippfind "%1._ipp._tcp.local." --txt tls --quiet && set result=PASS
if "%result%" == "PASS" (
	set /a pass+=1
	set HAVE_TLS=1
) else (
	set /a skip+=1
	set HAVE_TLS=0
)

echo %result%
if "%result%" == "SKIP" (
	echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
	echo ^<key^>Skipped^</key^>^<true /^> >>"%PLIST%"
) else (
	echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
)
echo ^</dict^> >>"%PLIST%"

:: B-5.1 HTTP Upgrade test: Printer responds to an IPP Get-Printer-Attributes request after doing an HTTP Upgrade to TLS.
set /a total+=1
set <NUL /p="B-5.1 HTTP Upgrade test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-5.1 HTTP Upgrade test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind "%1._ipp._tcp.local." -x ipptool -E -q "{}" bonjour-access-tests.test ";" && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
	) else (
		set /a fail+=1
		echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST"
		ippfind "%1._ipp._tcp.local." -x ipptool -E -q "{}" bonjour-access-tests.test ";" >>"%PLIST%"
		echo ^</string^>^</array^> >>"%PLIST%"
	)
) else (
	set /a skip+=1
	set result=SKIP
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"
) else (
	if "%result%" == "SKIP" (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
		echo ^<key^>Skipped^</key^>^<true /^> >>"%PLIST%"
	) else (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
	)
)
echo ^</dict^> >>"%PLIST%"

:: B-5.2 IPPS Browse test: Printer appears in a search for "_ipps._tcp,_print" services.
set /a total+=1
set <NUL /p="B-5.2 IPPS Browse test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-5.2 IPPS Browse test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind _ipps._tcp,_print.local. --name "%1" --quiet && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
	) else (
		set /a fail+=1
	)
) else (
	set /a skip+=1
	set result=SKIP
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"
) else (
	if "%result%" == "SKIP" (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
		echo ^<key^>Skipped^</key^>^<true /^> >>"%PLIST%"
	) else (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
	)
)
echo ^</dict^> >>"%PLIST%"

:: B-5.3 IPPS TXT keys test: The TXT record for IPPS contains all required keys
set /a total+=1
set <NUL /p="B-5.3 IPPS TXT keys test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-5.3 IPPS TXT keys test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind "%1._ipps._tcp.local." --txt adminurl --txt pdl --txt rp --txt TLS --txt UUID --quiet && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
	) else (
		set /a fail+=1
	        echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo adminurl={txt_adminurl}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo pdl={txt_pdl}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo rp={txt_rp}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo TLS={txt_tls}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo UUID={txt_uuid}" ";" >>"%PLIST%"
	        echo ^</string^>^</array^> >>"%PLIST%"
	)
) else (
	set /a skip+=1
	set result=SKIP
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"

        ippfind "%1._ipps._tcp.local." -x echo     adminurl={txt_adminurl}" ";"
        ippfind "%1._ipps._tcp.local." -x echo     pdl={txt_pdl}" ";"
        ippfind "%1._ipps._tcp.local." -x echo     rp={txt_rp}" ";"
        ippfind "%1._ipps._tcp.local." -x echo     TLS={txt_tls}" ";"
        ippfind "%1._ipps._tcp.local." -x echo     UUID={txt_uuid}" ";"
) else (
	if "%result%" == "SKIP" (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
		echo ^<key^>Skipped^</key^>^<true /^> >>"%PLIST%"
	) else (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
	)
)
echo ^</dict^> >>"%PLIST%"

:: B-5.4 IPPS Resolve test: Printer responds to an IPPS Get-Printer-Attributes request using the resolved hostname, port, and resource path.
set /a total+=1
set <NUL /p="B-5.4 IPPS Resolve test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-5.4 IPPS Resolve test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

if "%HAVE_TLS%" == "1" (
	set result=FAIL
	(ippfind "%1._ipps._tcp.local." --ls && set result=PASS) >nul:
	if "%result%" == "PASS" (
		set /a pass+=1
	) else (
		set /a fail+=1
		echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST%"
		ippfind "%1._ipps._tcp.local." --ls >>"%PLIST%"
		echo ^</string^>^</array^> >>"%PLIST%"
	)
) else (
	set /a skip+=1
	set result=SKIP
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"
) else (
	if "%result%" == "SKIP" (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
		echo ^<key^>Skipped^</key^>^<true /^> >>"%PLIST%"
	) else (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
	)
)
echo ^</dict^> >>"%PLIST%"

:: B-5.5 IPPS TXT values test: The TXT record values for IPPS match the reported IPPS attribute values.
set /a total+=1
set <NUL /p="B-5.5 IPPS TXT values test: "
echo ^<dict^>^<key^>Name^</key^>^<string^>B-5.5 IPPS TXT values test^</string^> >>"%PLIST%"
echo ^<key^>FileId^</key^>^<string^>org.pwg.ipp-everywhere.20140826.bonjour^</string^> >>"%PLIST%"

if "%HAVE_TLS%" == "1" (
	set result=FAIL
	ippfind "%1._ipps._tcp.local." --txt-adminurl ^^^(http:^|https:^)// --txt-pdl image/pwg-raster --txt-pdl image/jpeg --txt-rp ^^ipp/^(print^|print/[^^/]+^)$ --txt-UUID ^^[0-9a-fA-F]{8,8}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{4,4}-[0-9a-fA-F]{12,12}$ -x ipptool -q "{}" bonjour-value-tests.test ";" && set result=PASS
	if "%result%" == "PASS" (
		set /a pass+=1
	) else (
		set /a fail+=1
	        echo ^<key^>Errors^</key^>^<array^>^<string^> >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo adminurl="{txt_adminurl}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo pdl="{txt_pdl}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo rp="{txt_rp}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo TLS="{txt_tls}" ";" >>"%PLIST%"
	        ippfind "%1._ipps._tcp.local." -x echo UUID="{txt_uuid}" ";" >>"%PLIST%"
		ippfind "%1._ipps._tcp.local." -x ipptool -t "{}" bonjour-value-tests.test ";" | findstr /r [TG][EO][DT]: >>"%PLIST"
	        echo ^</string^>^</array^> >>"%PLIST%"
	)
) else (
	set /a skip+=1
	set result=SKIP
)

echo %result%
if "%result%" == "FAIL" (
	echo ^<key^>Successful^</key^>^<false /^> >>"%PLIST%"

        ippfind "%1._ipps._tcp.local." -x echo adminurl="{txt_adminurl}" ";"
        ippfind "%1._ipps._tcp.local." -x echo pdl="{txt_pdl}" ";"
        ippfind "%1._ipps._tcp.local." -x echo rp="{txt_rp}" ";"
        ippfind "%1._ipps._tcp.local." -x echo TLS="{txt_tls}" ";"
        ippfind "%1._ipps._tcp.local." -x echo UUID="{txt_uuid}" ";"
	ippfind "%1._ipp._tcp.local." -x ipptool -t "{}" bonjour-value-tests.test ";" | findstr /r [TG][EO][DT]:
) else (
	if "%result%" == "SKIP" (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
		echo ^<key^>Skipped^</key^>^<true /^> >>"%PLIST%"
	) else (
		echo ^<key^>Successful^</key^>^<true /^> >>"%PLIST%"
	)
)
echo ^</dict^> >>"%PLIST%"

:: Finish up...
echo ^</array^> >>"%PLIST%"
echo ^<key^>Successful^</key^> >>"%PLIST%"
if %fail% gtr 0 (
	echo ^<false /^> >>"%PLIST%"
) else (
	echo ^<true /^> >>"%PLIST%"
)
echo ^</dict^> >>"%PLIST%"
echo ^</plist^> >>"%PLIST%"

set /a score=%pass% + %skip%
set /a score=100 * %score% / %total%
echo Summary: %total% tests, %pass% passed, %fail% failed, %skip% skipped
echo Score: %score%%%

::
:: End of "$Id: bonjour-tests.bat 12249 2014-11-14 12:54:05Z msweet $".
::
