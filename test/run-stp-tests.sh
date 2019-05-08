#!/bin/sh
#
# Perform the complete set of IPP compliance tests specified in the
# CUPS Software Test Plan.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2007 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

argcount=$#

#
# Don't allow "make check" or "make test" to be run by root...
#

if test "x`id -u`" = x0; then
	echo Please run this as a normal user. Not supported when run as root.
	exit 1
fi

#
# Force the permissions of the files we create...
#

umask 022

#
# Make the IPP test program...
#

make

#
# Solaris has a non-POSIX grep in /bin...
#

if test -x /usr/xpg4/bin/grep; then
	GREP=/usr/xpg4/bin/grep
else
	GREP=grep
fi

#
# Figure out the proper echo options...
#

if (echo "testing\c"; echo 1,2,3) | $GREP c >/dev/null; then
        ac_n=-n
        ac_c=
else
        ac_n=
        ac_c='\c'
fi

#
# Greet the tester...
#

echo "Welcome to the CUPS Automated Test Script."
echo ""
echo "Before we begin, it is important that you understand that the larger"
echo "tests require significant amounts of RAM and disk space.  If you"
echo "attempt to run one of the big tests on a system that lacks sufficient"
echo "disk and virtual memory, the UNIX kernel might decide to kill one or"
echo "more system processes that you've grown attached to, like the X"
echo "server.  The question you may want to ask yourself before running a"
echo "large test is: Do you feel lucky?"
echo ""
echo "OK, now that we have the Dirty Harry quote out of the way, please"
echo "choose the type of test you wish to perform:"
echo ""
echo "0 - No testing, keep the scheduler running for me (all systems)"
echo "1 - Basic conformance test, no load testing (all systems)"
echo "2 - Basic conformance test, some load testing (minimum 256MB VM, 50MB disk)"
echo "3 - Basic conformance test, extreme load testing (minimum 1GB VM, 500MB disk)"
echo "4 - Basic conformance test, torture load testing (minimum 2GB VM, 1GB disk)"
echo ""
echo $ac_n "Enter the number of the test you wish to perform: [1] $ac_c"

if test $# -gt 0; then
	testtype=$1
	shift
else
	read testtype
fi
echo ""

case "$testtype" in
	0)
		echo "Running in test mode (0)"
		nprinters=0
		pjobs=0
		pprinters=0
		loglevel="debug2"
		;;
	2)
		echo "Running the medium tests (2)"
		nprinters=20
		pjobs=20
		pprinters=10
		loglevel="debug"
		;;
	3)
		echo "Running the extreme tests (3)"
		nprinters=1000
		pjobs=100
		pprinters=50
		loglevel="debug"
		;;
	4)
		echo "Running the torture tests (4)"
		nprinters=20000
		pjobs=200
		pprinters=100
		loglevel="debug"
		;;
	*)
		echo "Running the timid tests (1)"
		nprinters=0
		pjobs=10
		pprinters=0
		loglevel="debug2"
		testtype="1"
		;;
esac

#
# See if we want to do SSL testing...
#

echo ""
echo "Now you can choose whether to create a SSL/TLS encryption key and"
echo "certificate for testing:"
echo ""
echo "0 - Do not do SSL/TLS encryption tests"
echo "1 - Test but do not require encryption"
echo "2 - Test and require encryption"
echo ""
echo $ac_n "Enter the number of the SSL/TLS tests to perform: [0] $ac_c"

if test $# -gt 0; then
	ssltype=$1
	shift
else
	read ssltype
fi
echo ""

case "$ssltype" in
	1)
		echo "Will test but not require encryption (1)"
		;;
	2)
		echo "Will test and require encryption (2)"
		;;
	*)
		echo "Not using SSL/TLS (0)"
		ssltype=0
		;;
esac

#
# Information for the server/tests...
#

user="$USER"
if test -z "$user"; then
	if test -x /usr/ucb/whoami; then
		user=`/usr/ucb/whoami`
	else
		user=`whoami`
	fi

	if test -z "$user"; then
		user="unknown"
	fi
fi

port="${CUPS_TESTPORT:=8631}"
cwd=`pwd`
root=`dirname $cwd`
CUPS_TESTROOT="$root"; export CUPS_TESTROOT

BASE="${CUPS_TESTBASE:=}"
if test -z "$BASE"; then
	if test -d /private/tmp; then
		BASE=/private/tmp/cups-$user
	else
		BASE=/tmp/cups-$user
	fi
fi
export BASE

#
# Make sure that the LPDEST and PRINTER environment variables are
# not included in the environment that is passed to the tests.  These
# will usually cause tests to fail erroneously...
#

unset LPDEST
unset PRINTER

#
# See if we want to use valgrind...
#

echo ""
echo "This test script can use the Valgrind software from:"
echo ""
echo "    http://developer.kde.org/~sewardj/"
echo ""
echo $ac_n "Enter Y to use Valgrind or N to not use Valgrind: [N] $ac_c"

if test $# -gt 0; then
	usevalgrind=$1
	shift
else
	read usevalgrind
fi
echo ""

case "$usevalgrind" in
	Y* | y*)
		VALGRIND="valgrind --tool=memcheck --log-file=$BASE/log/valgrind.%p --error-limit=no --leak-check=yes --trace-children=yes"
		if test `uname` = Darwin; then
			VALGRIND="$VALGRIND --dsymutil=yes"
		fi
		export VALGRIND
		echo "Using Valgrind; log files can be found in $BASE/log..."
		;;

	*)
		VALGRIND=""
		export VALGRIND
		;;
esac

#
# See if we want to do debug logging of the libraries...
#

echo ""
echo "If CUPS was built with the --enable-debug-printfs configure option, you"
echo "can enable debug logging of the libraries."
echo ""
echo $ac_n "Enter Y or a number from 0 to 9 to enable debug logging or N to not: [N] $ac_c"

if test $# -gt 0; then
	usedebugprintfs=$1
	shift
else
	read usedebugprintfs
fi
echo ""

case "$usedebugprintfs" in
	Y* | y*)
		echo "Enabling debug printfs (level 5); log files can be found in $BASE/log..."
		CUPS_DEBUG_LOG="$BASE/log/debug_printfs.%d"; export CUPS_DEBUG_LOG
		CUPS_DEBUG_LEVEL=5; export CUPS_DEBUG_LEVEL
		CUPS_DEBUG_FILTER='^(http|_http|ipp|_ipp|cups.*Request|cupsGetResponse|cupsSend).*$'; export CUPS_DEBUG_FILTER
		;;

	0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9)
		echo "Enabling debug printfs (level $usedebugprintfs); log files can be found in $BASE/log..."
		CUPS_DEBUG_LOG="$BASE/log/debug_printfs.%d"; export CUPS_DEBUG_LOG
		CUPS_DEBUG_LEVEL="$usedebugprintfs"; export CUPS_DEBUG_LEVEL
		CUPS_DEBUG_FILTER='^(http|_http|ipp|_ipp|cups.*Request|cupsGetResponse|cupsSend|mime).*$'; export CUPS_DEBUG_FILTER
		;;

	*)
		;;
esac

#
# Start by creating temporary directories for the tests...
#

echo "Creating directories for test..."

rm -rf $BASE
mkdir $BASE
mkdir $BASE/bin
mkdir $BASE/bin/backend
mkdir $BASE/bin/driver
mkdir $BASE/bin/filter
mkdir $BASE/certs
mkdir $BASE/share
mkdir $BASE/share/banners
mkdir $BASE/share/drv
mkdir $BASE/share/locale
for file in ../locale/cups_*.po; do
	loc=`basename $file .po | cut -c 6-`
	mkdir $BASE/share/locale/$loc
	ln -s $root/locale/cups_$loc.po $BASE/share/locale/$loc
done
mkdir $BASE/share/locale/en
ln -s $root/locale/cups.pot $BASE/share/locale/en/cups_en.po
mkdir $BASE/share/mime
mkdir $BASE/share/model
mkdir $BASE/share/ppdc
mkdir $BASE/interfaces
mkdir $BASE/log
mkdir $BASE/ppd
mkdir $BASE/spool
mkdir $BASE/spool/temp
mkdir $BASE/ssl

ln -s $root/backend/dnssd $BASE/bin/backend
ln -s $root/backend/http $BASE/bin/backend
ln -s $root/backend/ipp $BASE/bin/backend
ln -s ipp $BASE/bin/backend/ipps
ln -s $root/backend/lpd $BASE/bin/backend
ln -s $root/backend/mdns $BASE/bin/backend
ln -s $root/backend/pseudo $BASE/bin/backend
ln -s $root/backend/snmp $BASE/bin/backend
ln -s $root/backend/socket $BASE/bin/backend
ln -s $root/backend/usb $BASE/bin/backend
ln -s $root/cgi-bin $BASE/bin
ln -s $root/monitor $BASE/bin
ln -s $root/notifier $BASE/bin
ln -s $root/scheduler $BASE/bin/daemon
ln -s $root/filter/commandtops $BASE/bin/filter
ln -s $root/filter/gziptoany $BASE/bin/filter
ln -s $root/filter/pstops $BASE/bin/filter
ln -s $root/filter/rastertoepson $BASE/bin/filter
ln -s $root/filter/rastertohp $BASE/bin/filter
ln -s $root/filter/rastertolabel $BASE/bin/filter
ln -s $root/filter/rastertopwg $BASE/bin/filter
cat >$BASE/share/banners/standard <<EOF
           ==== Cover Page ====


      Job: {?printer-name}-{?job-id}
    Owner: {?job-originating-user-name}
     Name: {?job-name}
    Pages: {?job-impressions}


           ==== Cover Page ====
EOF
cat >$BASE/share/banners/classified <<EOF
           ==== Classified - Do Not Disclose ====


      Job: {?printer-name}-{?job-id}
    Owner: {?job-originating-user-name}
     Name: {?job-name}
    Pages: {?job-impressions}


           ==== Classified - Do Not Disclose ====
EOF
ln -s $root/data $BASE/share
ln -s $root/ppdc/sample.drv $BASE/share/drv
ln -s $root/conf/mime.types $BASE/share/mime
ln -s $root/conf/mime.convs $BASE/share/mime
ln -s $root/data/*.h $BASE/share/ppdc
ln -s $root/data/*.defs $BASE/share/ppdc
ln -s $root/templates $BASE/share

#
# Local filters and configuration files...
#

instfilter() {
	# instfilter src dst format
	#
	# See if the filter exists in a standard location; if so, make a
	# symlink, otherwise create a dummy script for the specified format.
	#
	src="$1"
	dst="$2"
	format="$3"

	for dir in /usr/local/libexec/cups/filter /usr/libexec/cups/filter /usr/lib/cups/filter; do
		if test -x "$dir/$src"; then
			ln -s "$dir/$src" "$BASE/bin/filter/$dst"
			return
		fi
	done

	# Source filter not present, create a dummy filter
	case $format in
		passthru)
			ln -s gziptoany "$BASE/bin/filter/$dst"
			;;
		pdf)
			cat >"$BASE/bin/filter/$dst" <<EOF
#!/bin/sh
trap "" TERM
trap "" PIPE
gziptoany "$1" "$2" "$3" "$4" "$5" \$6 >/dev/null
case "\$5" in
	*media=a4* | *media=iso_a4* | *PageSize=A4*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/examples/onepage-a4.pdf"
		;;
	*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/examples/onepage-letter.pdf"
		;;
esac
EOF
			chmod +x "$BASE/bin/filter/$dst"
			;;
		ps)
			cat >"$BASE/bin/filter/$dst" <<EOF
#!/bin/sh
trap "" TERM
trap "" PIPE
gziptoany "$1" "$2" "$3" "$4" "$5" \$6 >/dev/null
case "\$5" in
	*media=a4* | *media=iso_a4* | *PageSize=A4*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/examples/onepage-a4.ps"
		;;
	*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/examples/onepage-letter.ps"
		;;
esac
EOF
			chmod +x "$BASE/bin/filter/$dst"
			;;
		raster)
			cat >"$BASE/bin/filter/$dst" <<EOF
#!/bin/sh
trap "" TERM
trap "" PIPE
gziptoany "$1" "$2" "$3" "$4" "$5" \$6 >/dev/null
case "\$5" in
	*media=a4* | *media=iso_a4* | *PageSize=A4*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/examples/onepage-a4-300-black-1.pwg"
		;;
	*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/examples/onepage-letter-300-black-1.pwg"
		;;
esac
EOF
			chmod +x "$BASE/bin/filter/$dst"
			;;
	esac
}

ln -s $root/test/test.convs $BASE/share/mime
ln -s $root/test/test.types $BASE/share/mime

if test `uname` = Darwin; then
	instfilter cgimagetopdf imagetopdf pdf
	instfilter cgpdftopdf pdftopdf passthru
	instfilter cgpdftops pdftops ps
	instfilter cgpdftoraster pdftoraster raster
	instfilter cgpdftoraster pdftourf raster
	instfilter cgtexttopdf texttopdf pdf
	instfilter pstocupsraster pstoraster raster
else
	instfilter imagetopdf imagetopdf pdf
	instfilter pdftopdf pdftopdf passthru
	instfilter pdftops pdftops ps
	instfilter pdftoraster pdftoraster raster
	instfilter pdftoraster pdftourf raster
	instfilter pstoraster pstoraster raster
	instfilter texttopdf texttopdf pdf

	if test -d /usr/share/cups/charsets; then
		ln -s /usr/share/cups/charsets $BASE/share
	fi
fi

#
# Then create the necessary config files...
#

echo "Creating cupsd.conf for test..."

if test $ssltype = 2; then
	encryption="Encryption Required"
else
	encryption=""
fi

if test $testtype = 0; then
	jobhistory="30m"
	jobfiles="5m"
else
	jobhistory="30"
	jobfiles="Off"
fi

cat >$BASE/cupsd.conf <<EOF
StrictConformance Yes
Browsing Off
Listen localhost:$port
Listen $BASE/sock
MaxSubscriptions 3
MaxLogSize 0
AccessLogLevel actions
LogLevel $loglevel
LogTimeFormat usecs
PreserveJobHistory $jobhistory
PreserveJobFiles $jobfiles
<Policy default>
<Limit All>
Order Allow,Deny
$encryption
</Limit>
</Policy>
EOF

if test $testtype = 0; then
	echo WebInterface yes >>$BASE/cupsd.conf
fi

cat >$BASE/cups-files.conf <<EOF
FileDevice yes
Printcap
User $user
ServerRoot $BASE
StateDir $BASE
ServerBin $BASE/bin
CacheDir $BASE/share
DataDir $BASE/share
FontPath $BASE/share/fonts
DocumentRoot $root/doc
RequestRoot $BASE/spool
TempDir $BASE/spool/temp
AccessLog $BASE/log/access_log
ErrorLog $BASE/log/error_log
PageLog $BASE/log/page_log

PassEnv DYLD_INSERT_LIBRARIES
PassEnv DYLD_LIBRARY_PATH
PassEnv LD_LIBRARY_PATH
PassEnv LD_PRELOAD
PassEnv LOCALEDIR
PassEnv ASAN_OPTIONS

Sandboxing Off
EOF

if test $ssltype != 0 -a `uname` = Darwin; then
	echo "ServerKeychain $HOME/Library/Keychains/login.keychain" >> $BASE/cups-files.conf
fi

#
# Setup lots of test queues with PPD files...
#

echo "Creating printers.conf for test..."

i=1
while test $i -le $nprinters; do
	cat >>$BASE/printers.conf <<EOF
<Printer test-$i>
Accepting Yes
DeviceURI file:/dev/null
Info Test PS printer $i
JobSheets none none
Location CUPS test suite
State Idle
StateMessage Printer $1 is idle.
</Printer>
EOF

	cp testps.ppd $BASE/ppd/test-$i.ppd

	i=`expr $i + 1`
done

if test -f $BASE/printers.conf; then
	cp $BASE/printers.conf $BASE/printers.conf.orig
else
	touch $BASE/printers.conf.orig
fi

#
# Create a helper script to run programs with...
#

echo "Setting up environment variables for test..."

if test "x$ASAN_OPTIONS" = x; then
	# AddressSanitizer on Linux reports memory leaks from the main function
	# which is basically useless - in general, programs do not need to free
	# every object before exit since the OS will recover the process's
	# memory.
	ASAN_OPTIONS="detect_leaks=false"
	export ASAN_OPTIONS
fi

if test -f "$root/cups/libcups.so.2"; then
	if test "x$LD_LIBRARY_PATH" = x; then
		LD_LIBRARY_PATH="$root/cups"
	else
		LD_LIBRARY_PATH="$root/cups:$LD_LIBRARY_PATH"
	fi

	LD_PRELOAD="$root/cups/libcups.so.2:$root/cups/libcupsimage.so.2"
	if test `uname` = SunOS -a -r /usr/lib/libCrun.so.1; then
		LD_PRELOAD="/usr/lib/libCrun.so.1:$LD_PRELOAD"
	fi
fi

if test -f "$root/cups/libcups.2.dylib"; then
	if test "x$DYLD_INSERT_LIBRARIES" = x; then
		DYLD_INSERT_LIBRARIES="$root/cups/libcups.2.dylib:$root/cups/libcupsimage.2.dylib"
	else
		DYLD_INSERT_LIBRARIES="$root/cups/libcups.2.dylib:$root/cups/libcupsimage.2.dylib:$DYLD_INSERT_LIBRARIES"
	fi

	if test "x$DYLD_LIBRARY_PATH" = x; then
		DYLD_LIBRARY_PATH="$root/cups"
	else
		DYLD_LIBRARY_PATH="$root/cups:$DYLD_LIBRARY_PATH"
	fi
fi

# These get exported because they don't have side-effects...
CUPS_DISABLE_APPLE_DEFAULT=yes; export CUPS_DISABLE_APPLE_DEFAULT
CUPS_SERVER=localhost:$port; export CUPS_SERVER
CUPS_SERVERROOT=$BASE; export CUPS_SERVERROOT
CUPS_STATEDIR=$BASE; export CUPS_STATEDIR
CUPS_DATADIR=$BASE/share; export CUPS_DATADIR
IPP_PORT=$port; export IPP_PORT
LOCALEDIR=$BASE/share/locale; export LOCALEDIR

echo "Creating wrapper script..."

runcups="$BASE/runcups"; export runcups

echo "#!/bin/sh" >$runcups
echo "# Helper script for running CUPS test instance." >>$runcups
echo "" >>$runcups
echo "# Set required environment variables..." >>$runcups
echo "CUPS_DATADIR=\"$CUPS_DATADIR\"; export CUPS_DATADIR" >>$runcups
echo "CUPS_SERVER=\"$CUPS_SERVER\"; export CUPS_SERVER" >>$runcups
echo "CUPS_SERVERROOT=\"$CUPS_SERVERROOT\"; export CUPS_SERVERROOT" >>$runcups
echo "CUPS_STATEDIR=\"$CUPS_STATEDIR\"; export CUPS_STATEDIR" >>$runcups
echo "DYLD_INSERT_LIBRARIES=\"$DYLD_INSERT_LIBRARIES\"; export DYLD_INSERT_LIBRARIES" >>$runcups
echo "DYLD_LIBRARY_PATH=\"$DYLD_LIBRARY_PATH\"; export DYLD_LIBRARY_PATH" >>$runcups
# IPP_PORT=$port; export IPP_PORT
echo "LD_LIBRARY_PATH=\"$LD_LIBRARY_PATH\"; export LD_LIBRARY_PATH" >>$runcups
echo "LD_PRELOAD=\"$LD_PRELOAD\"; export LD_PRELOAD" >>$runcups
echo "LOCALEDIR=\"$LOCALEDIR\"; export LOCALEDIR" >>$runcups
if test "x$CUPS_DEBUG_LEVEL" != x; then
	echo "CUPS_DEBUG_FILTER='$CUPS_DEBUG_FILTER'; export CUPS_DEBUG_FILTER" >>$runcups
	echo "CUPS_DEBUG_LEVEL=$CUPS_DEBUG_LEVEL; export CUPS_DEBUG_LEVEL" >>$runcups
	echo "CUPS_DEBUG_LOG='$CUPS_DEBUG_LOG'; export CUPS_DEBUG_LOG" >>$runcups
fi
echo "" >>$runcups
echo "# Run command..." >>$runcups
echo "exec \"\$@\"" >>$runcups

chmod +x $runcups

#
# Set a new home directory to avoid getting user options mixed in...
#

HOME=$BASE
export HOME

#
# Force POSIX locale for tests...
#

LANG=C
export LANG

LC_MESSAGES=C
export LC_MESSAGES

#
# Start the server; run as foreground daemon in the background...
#

echo "Starting scheduler:"
echo "    $runcups $VALGRIND ../scheduler/cupsd -c $BASE/cupsd.conf -f >$BASE/log/debug_log 2>&1 &"
echo ""

$runcups $VALGRIND ../scheduler/cupsd -c $BASE/cupsd.conf -f >$BASE/log/debug_log 2>&1 &

cupsd=$!

if test "x$testtype" = x0; then
	# Not running tests...
	echo "Scheduler is PID $cupsd and is listening on port $port."
	echo ""

	echo "The $runcups helper script can be used to test programs"
	echo "with the server."
	exit 0
fi

if test $argcount -eq 0; then
	echo "Scheduler is PID $cupsd; run debugger now if you need to."
	echo ""
	echo $ac_n "Press ENTER to continue... $ac_c"
	read junk
else
	echo "Scheduler is PID $cupsd."
	sleep 2
fi

while true; do
	running=`$runcups ../systemv/lpstat -r 2>/dev/null`
	if test "x$running" = "xscheduler is running"; then
		break
	fi

	echo "Waiting for scheduler to become ready..."
	sleep 10
done

#
# Create the test report source file...
#

date=`date "+%Y-%m-%d"`

strfile=$BASE/cups-str-$date-$user.html

rm -f $strfile
cat str-header.html >$strfile

#
# Run the IPP tests...
#

echo ""
echo "Running IPP compliance tests..."

echo "    <h1><a name='IPP'>1 - IPP Compliance Tests</a></h1>" >>$strfile
echo "    <p>This section provides the results to the IPP compliance tests" >>$strfile
echo "    outlined in the CUPS Software Test Plan. These tests were run on" >>$strfile
echo "    $date by $user on `hostname`." >>$strfile
echo "    <pre>" >>$strfile

fail=0
for file in 4*.test ../examples/ipp-2.1.test; do
	echo $ac_n "Performing `basename $file`: $ac_c"
	echo "" >>$strfile
        echo $ac_n "`date '+[%d/%b/%Y:%H:%M:%S %z]'` $ac_c" >>$strfile

	if test $file = ../examples/ipp-2.1.test; then
		uri="ipp://localhost:$port/printers/Test1"
		options="-V 2.1 -d NOPRINT=1 -f testfile.ps"
	else
		uri="ipp://localhost:$port/printers"
		options=""
	fi
	$runcups $VALGRIND ../tools/ipptool -tI $options $uri $file >> $strfile
	status=$?

	if test $status != 0; then
		echo FAIL
		fail=`expr $fail + 1`
	else
		echo PASS
	fi
done

echo "    </pre>" >>$strfile

#
# Run the command tests...
#

echo ""
echo "Running command tests..."

echo "    <h1><a name='COMMAND'>2 - Command Tests</a></h1>" >>$strfile
echo "    <p>This section provides the results to the command tests" >>$strfile
echo "    outlined in the CUPS Software Test Plan. These tests were run on" >>$strfile
echo "    $date by $user on `hostname`." >>$strfile
echo "    <pre>" >>$strfile

for file in 5*.sh; do
	echo $ac_n "Performing $file: $ac_c"
	echo "" >>$strfile
        echo "`date '+[%d/%b/%Y:%H:%M:%S %z]'` \"$file\":" >>$strfile

	sh $file $pjobs $pprinters >> $strfile
	status=$?

	if test $status != 0; then
		echo FAIL
		fail=`expr $fail + 1`
	else
		echo PASS
	fi
done

#
# Restart the server...
#

echo $ac_n "Performing restart test: $ac_c"
echo "" >>$strfile
echo "`date '+[%d/%b/%Y:%H:%M:%S %z]'` \"5.10-restart\":" >>$strfile

kill -HUP $cupsd

while true; do
	sleep 10

	running=`$runcups ../systemv/lpstat -r 2>/dev/null`
	if test "x$running" = "xscheduler is running"; then
		break
	fi
done

description="`$runcups ../systemv/lpstat -l -p Test1 | grep Description | sed -e '1,$s/^[^:]*: //g'`"
if test "x$description" != "xTest Printer 1"; then
	echo "Failed, printer-info for Test1 is '$description', expected 'Test Printer 1'." >>$strfile
	echo "FAIL (got '$description', expected 'Test Printer 1')"
	fail=`expr $fail + 1`
else
	echo "Passed." >>$strfile
	echo PASS
fi


#
# Perform job history test...
#

echo $ac_n "Starting history test: $ac_c"
echo "" >>$strfile
echo "`date '+[%d/%b/%Y:%H:%M:%S %z]'` \"5.11-history\":" >>$strfile

echo "    lp -d Test1 testfile.jpg" >>$strfile

$runcups ../systemv/lp -d Test1 ../examples/testfile.jpg 2>&1 >>$strfile
if test $? != 0; then
	echo "FAIL (unable to queue test job)"
	echo "    FAILED" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS"
	echo "    PASSED" >>$strfile

	./waitjobs.sh >>$strfile

        echo $ac_n "Verifying that history still exists: $ac_c"

	echo "    ls -l $BASE/spool" >>$strfile
	count=`ls -1 $BASE/spool | wc -l`
	if test $count = 1; then
		echo "FAIL"
		echo "    FAILED (job control files not present)" >>$strfile
		ls -l $BASE/spool >>$strfile
		fail=`expr $fail + 1`
	else
		echo "PASS"
		echo "    PASSED" >>$strfile

		echo $ac_n "Waiting for job history to expire: $ac_c"
		echo "" >>$strfile
		echo "    sleep 35" >>$strfile
		sleep 35

		echo "    lpstat" >>$strfile
		$runcups ../systemv/lpstat 2>&1 >>$strfile

		echo "    ls -l $BASE/spool" >>$strfile
		count=`ls -1 $BASE/spool | wc -l`
		if test $count != 1; then
			echo "FAIL"
			echo "    FAILED (job control files still present)" >>$strfile
			ls -l $BASE/spool >>$strfile
			fail=`expr $fail + 1`
		else
			echo "PASS"
			echo "    PASSED" >>$strfile
		fi
	fi
fi


#
# Stop the server...
#

echo "    </pre>" >>$strfile

kill $cupsd
wait $cupsd
cupsdstatus=$?

#
# Verify counts...
#

echo "Test Summary"
echo ""
echo "    <h1><a name='SUMMARY'>3 - Test Summary</a></h1>" >>$strfile

if test $cupsdstatus != 0; then
	echo "FAIL: cupsd failed with exit status $cupsdstatus."
	echo "    <p>FAIL: cupsd failed with exit status $cupsdstatus.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: cupsd exited with no errors."
	echo "    <p>PASS: cupsd exited with no errors.</p>" >>$strfile
fi

# Job control files
count=`ls -1 $BASE/spool | wc -l`
count=`expr $count - 1`
if test $count != 0; then
	echo "FAIL: $count job control files were not purged."
	echo "    <p>FAIL: $count job control files were not purged.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: All job control files purged."
	echo "    <p>PASS: All job control files purged.</p>" >>$strfile
fi

# Pages printed on Test1 (within 1 page for timing-dependent cancel issues)
count=`$GREP '^Test1 ' $BASE/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=`expr $pjobs \* 2 + 34`
expected2=`expr $expected + 2`
if test $count -lt $expected -a $count -gt $expected2; then
	echo "FAIL: Printer 'Test1' produced $count page(s), expected $expected."
	echo "    <p>FAIL: Printer 'Test1' produced $count page(s), expected $expected.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test1' correctly produced $count page(s)."
	echo "    <p>PASS: Printer 'Test1' correctly produced $count page(s).</p>" >>$strfile
fi

# Paged printed on Test2
count=`$GREP '^Test2 ' $BASE/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=`expr $pjobs \* 2 + 3`
if test $count != $expected; then
	echo "FAIL: Printer 'Test2' produced $count page(s), expected $expected."
	echo "    <p>FAIL: Printer 'Test2' produced $count page(s), expected $expected.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test2' correctly produced $count page(s)."
	echo "    <p>PASS: Printer 'Test2' correctly produced $count page(s).</p>" >>$strfile
fi

# Paged printed on Test3
count=`$GREP '^Test3 ' $BASE/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=2
if test $count != $expected; then
	echo "FAIL: Printer 'Test3' produced $count page(s), expected $expected."
	echo "    <p>FAIL: Printer 'Test3' produced $count page(s), expected $expected.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test3' correctly produced $count page(s)."
	echo "    <p>PASS: Printer 'Test3' correctly produced $count page(s).</p>" >>$strfile
fi

# Requests logged
count=`wc -l $BASE/log/access_log | awk '{print $1}'`
expected=`expr 35 + 18 + 30 + $pjobs \* 8 + $pprinters \* $pjobs \* 4 + 2`
if test $count != $expected; then
	echo "FAIL: $count requests logged, expected $expected."
	echo "    <p>FAIL: $count requests logged, expected $expected.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count requests logged."
	echo "    <p>PASS: $count requests logged.</p>" >>$strfile
fi

# Did CUPS-Get-Default get logged?
if $GREP -q CUPS-Get-Default $BASE/log/access_log; then
	echo "FAIL: CUPS-Get-Default logged with 'AccessLogLevel actions'"
	echo "    <p>FAIL: CUPS-Get-Default logged with 'AccessLogLevel actions'</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP CUPS-Get-Default $BASE/log/access_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: CUPS-Get-Default not logged."
	echo "    <p>PASS: CUPS-Get-Default not logged.</p>" >>$strfile
fi

# Emergency log messages
count=`$GREP '^X ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count emergency messages, expected 0."
	$GREP '^X ' $BASE/log/error_log
	echo "    <p>FAIL: $count emergency messages, expected 0.</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP '^X ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count emergency messages."
	echo "    <p>PASS: $count emergency messages.</p>" >>$strfile
fi

# Alert log messages
count=`$GREP '^A ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count alert messages, expected 0."
	$GREP '^A ' $BASE/log/error_log
	echo "    <p>FAIL: $count alert messages, expected 0.</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP '^A ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count alert messages."
	echo "    <p>PASS: $count alert messages.</p>" >>$strfile
fi

# Critical log messages
count=`$GREP '^C ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count critical messages, expected 0."
	$GREP '^C ' $BASE/log/error_log
	echo "    <p>FAIL: $count critical messages, expected 0.</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP '^C ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count critical messages."
	echo "    <p>PASS: $count critical messages.</p>" >>$strfile
fi

# Error log messages
count=`$GREP '^E ' $BASE/log/error_log | $GREP -v 'Unknown default SystemGroup' | wc -l | awk '{print $1}'`
if test $count != 33; then
	echo "FAIL: $count error messages, expected 33."
	$GREP '^E ' $BASE/log/error_log
	echo "    <p>FAIL: $count error messages, expected 33.</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP '^E ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count error messages."
	echo "    <p>PASS: $count error messages.</p>" >>$strfile
fi

# Warning log messages
count=`$GREP '^W ' $BASE/log/error_log | $GREP -v CreateProfile | wc -l | awk '{print $1}'`
if test $count != 8; then
	echo "FAIL: $count warning messages, expected 8."
	$GREP '^W ' $BASE/log/error_log
	echo "    <p>FAIL: $count warning messages, expected 8.</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP '^W ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count warning messages."
	echo "    <p>PASS: $count warning messages.</p>" >>$strfile
fi

# Notice log messages
count=`$GREP '^N ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count notice messages, expected 0."
	$GREP '^N ' $BASE/log/error_log
	echo "    <p>FAIL: $count notice messages, expected 0.</p>" >>$strfile
	echo "    <pre>" >>$strfile
	$GREP '^N ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "    </pre>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count notice messages."
	echo "    <p>PASS: $count notice messages.</p>" >>$strfile
fi

# Info log messages
count=`$GREP '^I ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count info messages, expected more than 0."
	echo "    <p>FAIL: $count info messages, expected more than 0.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count info messages."
	echo "    <p>PASS: $count info messages.</p>" >>$strfile
fi

# Debug log messages
count=`$GREP '^D ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count debug messages, expected more than 0."
	echo "    <p>FAIL: $count debug messages, expected more than 0.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count debug messages."
	echo "    <p>PASS: $count debug messages.</p>" >>$strfile
fi

# Debug2 log messages
count=`$GREP '^d ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0 -a $loglevel = debug2; then
	echo "FAIL: $count debug2 messages, expected more than 0."
	echo "    <p>FAIL: $count debug2 messages, expected more than 0.</p>" >>$strfile
	fail=`expr $fail + 1`
elif test $count != 0 -a $loglevel = debug; then
	echo "FAIL: $count debug2 messages, expected 0."
	echo "    <p>FAIL: $count debug2 messages, expected 0.</p>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count debug2 messages."
	echo "    <p>PASS: $count debug2 messages.</p>" >>$strfile
fi

#
# Log files...
#

echo "    <h1><a name='LOGS'>4 - Log Files</a></h1>" >>$strfile

for file in $BASE/log/*_log; do
        baselog=`basename $file`

        echo "    <h2><a name=\"$baselog\">$baselog</a></h2>" >>$strfile
        case $baselog in
                error_log)
                        echo "    <blockquote>Note: debug2 messages have been filtered out of the HTML report.</blockquote>" >>$strfile
                        echo "    <pre>" >>$strfile
                        $GREP -v '^d' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
                        echo "    </pre>" >>$strfile
                        ;;

                *)
                        echo "    <pre>" >>$strfile
                        sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' $file >>$strfile
                        echo "    </pre>" >>$strfile
                        ;;
        esac
done

#
# Format the reports and tell the user where to find them...
#

cat str-trailer.html >>$strfile

echo ""
for file in $BASE/log/*_log; do
        baselog=`basename $file`
        cp $file $baselog-$date-$user
        echo "Copied log file \"$baselog-$date-$user\" to test directory."
done
cp $strfile .
echo "Copied report file \"cups-str-$date-$user.html\" to test directory."

# Clean out old failure log files after 1 week...
find . -name \*_log-\*-$user -a -mtime +7 -print -exec rm -f '{}' \; | awk '{print "Removed old log file \"" substr($1,3) "\" from test directory."}'
find . -name cups-str-\*-$user.html -a -mtime +7 -print -exec rm -f '{}' \; | awk '{print "Removed old report file \"" $1 "\" from test directory."}'

echo ""

if test $fail != 0; then
	echo "$fail tests failed."
	exit 1
else
	echo "All tests were successful."
fi
