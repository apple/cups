#!/bin/sh
#
# "$Id: run-stp-tests.sh 12658 2015-05-22 17:53:45Z msweet $"
#
# Perform the complete set of IPP compliance tests specified in the
# CUPS Software Test Plan.
#
# Copyright 2007-2015 by Apple Inc.
# Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
# These coded instructions, statements, and computer programs are the
# property of Apple Inc. and are protected by Federal copyright
# law.  Distribution and use rights are outlined in the file "LICENSE.txt"
# which should have been included with this file.  If this file is
# file is missing or damaged, see the license at "http://www.cups.org/".
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
		nprinters1=0
		nprinters2=0
		pjobs=0
		pprinters=0
		loglevel="debug2"
		;;
	2)
		echo "Running the medium tests (2)"
		nprinters1=10
		nprinters2=20
		pjobs=20
		pprinters=10
		loglevel="debug"
		;;
	3)
		echo "Running the extreme tests (3)"
		nprinters1=500
		nprinters2=1000
		pjobs=100
		pprinters=50
		loglevel="debug"
		;;
	4)
		echo "Running the torture tests (4)"
		nprinters1=10000
		nprinters2=20000
		pjobs=200
		pprinters=100
		loglevel="debug"
		;;
	*)
		echo "Running the timid tests (1)"
		nprinters1=0
		nprinters2=0
		pjobs=10
		pprinters=0
		loglevel="debug2"
		;;
esac

#
# See if we want to do SSL testing...
#

echo ""
echo "Now you can choose whether to create a SSL/TLS encryption key and"
echo "certificate for testing; these tests currently require the OpenSSL"
echo "tools:"
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
		CUPS_DEBUG_FILTER='^(http|_http|ipp|_ipp|cups.*Request|cupsGetResponse|cupsSend).*$'; export CUPS_DEBUG_FILTER
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
	ln -s $root/locale/ppdc_$loc.po $BASE/share/locale/$loc
done
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
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/test/onepage-a4.pdf"
		;;
	*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/test/onepage-letter.pdf"
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
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/test/onepage-a4.ps"
		;;
	*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/test/onepage-letter.ps"
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
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/test/onepage-a4-300-black-1.pwg.gz"
		;;
	*)
		gziptoany "$1" "$2" "$3" "$4" "$5" "$root/test/onepage-letter-300-black-1.pwg.gz"
		;;
esac
EOF
			chmod +x "$BASE/bin/filter/$dst"
			;;
	esac
}

ln -s $root/test/test.convs $BASE/share/mime

if test `uname` = Darwin; then
	instfilter cgimagetopdf imagetopdf pdf
	instfilter cgpdftopdf pdftopdf passthru
	instfilter cgpdftops pdftops ps
	instfilter cgpdftoraster pdftoraster raster
	instfilter cgtexttopdf texttopdf pdf
	instfilter pstocupsraster pstoraster raster
else
	instfilter imagetopdf imagetopdf pdf
	instfilter pdftopdf pdftopdf passthru
	instfilter pdftops pdftops ps
	instfilter pdftoraster pdftoraster raster
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

cat >$BASE/cupsd.conf <<EOF
StrictConformance Yes
Browsing Off
Listen localhost:$port
Listen $BASE/sock
PassEnv LOCALEDIR
PassEnv DYLD_INSERT_LIBRARIES
MaxSubscriptions 3
MaxLogSize 0
AccessLogLevel actions
LogLevel $loglevel
LogTimeFormat usecs
PreserveJobHistory Yes
PreserveJobFiles 5m
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
EOF

if test $ssltype != 0 -a `uname` = Darwin; then
	echo "ServerKeychain $HOME/Library/Keychains/login.keychain" >> $BASE/cups-files.conf
fi

#
# Setup lots of test queues - half with PPD files, half without...
#

echo "Creating printers.conf for test..."

i=1
while test $i -le $nprinters1; do
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

while test $i -le $nprinters2; do
	cat >>$BASE/printers.conf <<EOF
<Printer test-$i>
Accepting Yes
DeviceURI file:/dev/null
Info Test raw printer $i
JobSheets none none
Location CUPS test suite
State Idle
StateMessage Printer $1 is idle.
</Printer>
EOF

	i=`expr $i + 1`
done

if test -f $BASE/printers.conf; then
	cp $BASE/printers.conf $BASE/printers.conf.orig
else
	touch $BASE/printers.conf.orig
fi

#
# Setup the paths...
#

echo "Setting up environment variables for test..."

if test "x$LD_LIBRARY_PATH" = x; then
	LD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/ppdc"
else
	LD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/ppdc:$LD_LIBRARY_PATH"
fi

export LD_LIBRARY_PATH

LD_PRELOAD="$root/cups/libcups.so.2:$root/filter/libcupsimage.so.2:$root/cgi-bin/libcupscgi.so.1:$root/scheduler/libcupsmime.so.1:$root/ppdc/libcupsppdc.so.1"
if test `uname` = SunOS -a -r /usr/lib/libCrun.so.1; then
	LD_PRELOAD="/usr/lib/libCrun.so.1:$LD_PRELOAD"
fi
export LD_PRELOAD

if test "x$DYLD_LIBRARY_PATH" = x; then
	DYLD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/ppdc"
else
	DYLD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/ppdc:$DYLD_LIBRARY_PATH"
fi

export DYLD_LIBRARY_PATH

if test "x$SHLIB_PATH" = x; then
	SHLIB_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/ppdc"
else
	SHLIB_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/ppdc:$SHLIB_PATH"
fi

export SHLIB_PATH

CUPS_DISABLE_APPLE_DEFAULT=yes; export CUPS_DISABLE_APPLE_DEFAULT
CUPS_SERVER=localhost:$port; export CUPS_SERVER
CUPS_SERVERROOT=$BASE; export CUPS_SERVERROOT
CUPS_STATEDIR=$BASE; export CUPS_STATEDIR
CUPS_DATADIR=$BASE/share; export CUPS_DATADIR
LOCALEDIR=$BASE/share/locale; export LOCALEDIR

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
echo "    $VALGRIND ../scheduler/cupsd -c $BASE/cupsd.conf -f >$BASE/log/debug_log 2>&1 &"
echo ""

if test `uname` = Darwin -a "x$VALGRIND" = x; then
	DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib MallocStackLogging=1 ../scheduler/cupsd -c $BASE/cupsd.conf -f >$BASE/log/debug_log 2>&1 &
else
	$VALGRIND ../scheduler/cupsd -c $BASE/cupsd.conf -f >$BASE/log/debug_log 2>&1 &
fi

cupsd=$!

if test "x$testtype" = x0; then
	# Not running tests...
	echo "Scheduler is PID $cupsd and is listening on port $port."
	echo ""

	# Create a helper script to run programs with...
	runcups="$BASE/runcups"

	echo "#!/bin/sh" >$runcups
	echo "# Helper script for running CUPS test instance." >>$runcups
	echo "" >>$runcups
	echo "# Set required environment variables..." >>$runcups
	echo "CUPS_DATADIR=\"$CUPS_DATADIR\"; export CUPS_DATADIR" >>$runcups
	echo "CUPS_SERVER=\"$CUPS_SERVER\"; export CUPS_SERVER" >>$runcups
	echo "CUPS_SERVERROOT=\"$CUPS_SERVERROOT\"; export CUPS_SERVERROOT" >>$runcups
	echo "CUPS_STATEDIR=\"$CUPS_STATEDIR\"; export CUPS_STATEDIR" >>$runcups
	echo "DYLD_LIBRARY_PATH=\"$DYLD_LIBRARY_PATH\"; export DYLD_LIBRARY_PATH" >>$runcups
	echo "LD_LIBRARY_PATH=\"$LD_LIBRARY_PATH\"; export LD_LIBRARY_PATH" >>$runcups
	echo "LD_PRELOAD=\"$LD_PRELOAD\"; export LD_PRELOAD" >>$runcups
	echo "LOCALEDIR=\"$LOCALEDIR\"; export LOCALEDIR" >>$runcups
	echo "SHLIB_PATH=\"$SHLIB_PATH\"; export SHLIB_PATH" >>$runcups
	if test "x$CUPS_DEBUG_LEVEL" != x; then
		echo "CUPS_DEBUG_FILTER='$CUPS_DEBUG_FILTER'; export CUPS_DEBUG_FILTER" >>$runcups
		echo "CUPS_DEBUG_LEVEL=$CUPS_DEBUG_LEVEL; export CUPS_DEBUG_LEVEL" >>$runcups
		echo "CUPS_DEBUG_LOG='$CUPS_DEBUG_LOG'; export CUPS_DEBUG_LOG" >>$runcups
	fi
	echo "" >>$runcups
	echo "# Run command..." >>$runcups
	echo "exec \"\$@\"" >>$runcups

	chmod +x $runcups

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

IPP_PORT=$port; export IPP_PORT

while true; do
	running=`../systemv/lpstat -r 2>/dev/null`
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

if test -d $root/.svn; then
	rev=`svn info . | grep Revision: | awk '{print $2}'`
	strfile=$BASE/cups-str-2.0-r$rev-$user.html
else
	strfile=$BASE/cups-str-2.0-$date-$user.html
fi

rm -f $strfile
cat str-header.html >$strfile

#
# Run the IPP tests...
#

echo ""
echo "Running IPP compliance tests..."

echo "<H1>1 - IPP Compliance Tests</H1>" >>$strfile
echo "<P>This section provides the results to the IPP compliance tests" >>$strfile
echo "outlined in the CUPS Software Test Plan. These tests were run on" >>$strfile
echo `date "+%Y-%m-%d"` by $user on `hostname`. >>$strfile
echo "<PRE>" >>$strfile

fail=0
for file in 4*.test ipp-2.1.test; do
	echo $ac_n "Performing $file: $ac_c"
	echo "" >>$strfile

	if test $file = ipp-2.1.test; then
		uri="ipp://localhost:$port/printers/Test1"
		options="-V 2.1 -d NOPRINT=1 -f testfile.ps"
	else
		uri="ipp://localhost:$port/printers"
		options=""
	fi
	$VALGRIND ./ipptool -tI $options $uri $file >> $strfile
	status=$?

	if test $status != 0; then
		echo FAIL
		fail=`expr $fail + 1`
	else
		echo PASS
	fi
done

echo "</PRE>" >>$strfile

#
# Run the command tests...
#

echo ""
echo "Running command tests..."

echo "<H1>2 - Command Tests</H1>" >>$strfile
echo "<P>This section provides the results to the command tests" >>$strfile
echo "outlined in the CUPS Software Test Plan. These tests were run on" >>$strfile
echo $date by $user on `hostname`. >>$strfile
echo "<PRE>" >>$strfile

for file in 5*.sh; do
	echo $ac_n "Performing $file: $ac_c"
	echo "" >>$strfile
	echo "\"$file\":" >>$strfile

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
# Log all allocations made by the scheduler...
#
if test `uname` = Darwin -a "x$VALGRIND" = x; then
	malloc_history $cupsd -callTree -showContent >$BASE/log/malloc_log 2>&1
fi

#
# Restart the server...
#

echo $ac_n "Performing restart test: $ac_c"
echo "" >>$strfile
echo "\"5.10-restart\":" >>$strfile

kill -HUP $cupsd

while true; do
	sleep 10

	running=`../systemv/lpstat -r 2>/dev/null`
	if test "x$running" = "xscheduler is running"; then
		break
	fi
done

description="`../systemv/lpstat -l -p Test1 | grep Description | sed -e '1,$s/^[^:]*: //g'`"
if test "x$description" != "xTest Printer 1"; then
	echo "Failed, printer-info for Test1 is '$description', expected 'Test Printer 1'." >>$strfile
	echo "FAIL (got '$description', expected 'Test Printer 1')"
	fail=`expr $fail + 1`
else
	echo "Passed." >>$strfile
	echo PASS
fi

echo "</PRE>" >>$strfile

#
# Stop the server...
#

kill $cupsd

#
# Append the log files for post-mortim...
#

echo "<H1>3 - Log Files</H1>" >>$strfile

#
# Verify counts...
#

echo "Test Summary"
echo ""
echo "<H2>Summary</H2>" >>$strfile

# Job control files
count=`ls -1 $BASE/spool | wc -l`
count=`expr $count - 1`
if test $count != 0; then
	echo "FAIL: $count job control files were not purged."
	echo "<P>FAIL: $count job control files were not purged.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: All job control files purged."
	echo "<P>PASS: All job control files purged.</P>" >>$strfile
fi

# Pages printed on Test1 (within 1 page for timing-dependent cancel issues)
count=`$GREP '^Test1 ' $BASE/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=`expr $pjobs \* 2 + 34`
expected2=`expr $expected + 2`
if test $count -lt $expected -a $count -gt $expected2; then
	echo "FAIL: Printer 'Test1' produced $count page(s), expected $expected."
	echo "<P>FAIL: Printer 'Test1' produced $count page(s), expected $expected.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test1' correctly produced $count page(s)."
	echo "<P>PASS: Printer 'Test1' correctly produced $count page(s).</P>" >>$strfile
fi

# Paged printed on Test2
count=`$GREP '^Test2 ' $BASE/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=`expr $pjobs \* 2 + 3`
if test $count != $expected; then
	echo "FAIL: Printer 'Test2' produced $count page(s), expected $expected."
	echo "<P>FAIL: Printer 'Test2' produced $count page(s), expected $expected.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test2' correctly produced $count page(s)."
	echo "<P>PASS: Printer 'Test2' correctly produced $count page(s).</P>" >>$strfile
fi

# Paged printed on Test3
count=`$GREP '^Test3 ' $BASE/log/page_log | grep -v total | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=2
if test $count != $expected; then
	echo "FAIL: Printer 'Test3' produced $count page(s), expected $expected."
	echo "<P>FAIL: Printer 'Test3' produced $count page(s), expected $expected.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test3' correctly produced $count page(s)."
	echo "<P>PASS: Printer 'Test3' correctly produced $count page(s).</P>" >>$strfile
fi

# Requests logged
count=`wc -l $BASE/log/access_log | awk '{print $1}'`
expected=`expr 37 + 18 + 30 + $pjobs \* 8 + $pprinters \* $pjobs \* 4`
if test $count != $expected; then
	echo "FAIL: $count requests logged, expected $expected."
	echo "<P>FAIL: $count requests logged, expected $expected.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count requests logged."
	echo "<P>PASS: $count requests logged.</P>" >>$strfile
fi

# Did CUPS-Get-Default get logged?
if $GREP -q CUPS-Get-Default $BASE/log/access_log; then
	echo "FAIL: CUPS-Get-Default logged with 'AccessLogLevel actions'"
	echo "<P>FAIL: CUPS-Get-Default logged with 'AccessLogLevel actions'</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP CUPS-Get-Default $BASE/log/access_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: CUPS-Get-Default not logged."
	echo "<P>PASS: CUPS-Get-Default not logged.</P>" >>$strfile
fi

# Emergency log messages
count=`$GREP '^X ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count emergency messages, expected 0."
	$GREP '^X ' $BASE/log/error_log
	echo "<P>FAIL: $count emergency messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP '^X ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count emergency messages."
	echo "<P>PASS: $count emergency messages.</P>" >>$strfile
fi

# Alert log messages
count=`$GREP '^A ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count alert messages, expected 0."
	$GREP '^A ' $BASE/log/error_log
	echo "<P>FAIL: $count alert messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP '^A ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count alert messages."
	echo "<P>PASS: $count alert messages.</P>" >>$strfile
fi

# Critical log messages
count=`$GREP '^C ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count critical messages, expected 0."
	$GREP '^C ' $BASE/log/error_log
	echo "<P>FAIL: $count critical messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP '^C ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count critical messages."
	echo "<P>PASS: $count critical messages.</P>" >>$strfile
fi

# Error log messages
count=`$GREP '^E ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 33; then
	echo "FAIL: $count error messages, expected 33."
	$GREP '^E ' $BASE/log/error_log
	echo "<P>FAIL: $count error messages, expected 33.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP '^E ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count error messages."
	echo "<P>PASS: $count error messages.</P>" >>$strfile
fi

# Warning log messages
count=`$GREP '^W ' $BASE/log/error_log | $GREP -v CreateProfile | wc -l | awk '{print $1}'`
if test $count != 18; then
	echo "FAIL: $count warning messages, expected 18."
	$GREP '^W ' $BASE/log/error_log
	echo "<P>FAIL: $count warning messages, expected 18.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP '^W ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count warning messages."
	echo "<P>PASS: $count warning messages.</P>" >>$strfile
fi

# Notice log messages
count=`$GREP '^N ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count notice messages, expected 0."
	$GREP '^N ' $BASE/log/error_log
	echo "<P>FAIL: $count notice messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	$GREP '^N ' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count notice messages."
	echo "<P>PASS: $count notice messages.</P>" >>$strfile
fi

# Info log messages
count=`$GREP '^I ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count info messages, expected more than 0."
	echo "<P>FAIL: $count info messages, expected more than 0.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count info messages."
	echo "<P>PASS: $count info messages.</P>" >>$strfile
fi

# Debug log messages
count=`$GREP '^D ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count debug messages, expected more than 0."
	echo "<P>FAIL: $count debug messages, expected more than 0.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count debug messages."
	echo "<P>PASS: $count debug messages.</P>" >>$strfile
fi

# Debug2 log messages
count=`$GREP '^d ' $BASE/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count debug2 messages, expected more than 0."
	echo "<P>FAIL: $count debug2 messages, expected more than 0.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count debug2 messages."
	echo "<P>PASS: $count debug2 messages.</P>" >>$strfile
fi

# Log files...
echo "<H2>access_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' $BASE/log/access_log >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>error_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
$GREP -v '^d' $BASE/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>page_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' $BASE/log/page_log >>$strfile
echo "</PRE>" >>$strfile

#
# Format the reports and tell the user where to find them...
#

cat str-trailer.html >>$strfile

echo ""

if test $fail != 0; then
	echo "$fail tests failed."

	if test -d $root/.svn; then
		cp $BASE/log/error_log error_log-r$rev-$user
	else
		cp $BASE/log/error_log error_log-$date-$user
	fi

	cp $strfile .
else
	echo "All tests were successful."
fi

echo "Log files can be found in $BASE/log."
echo "A HTML report was created in $strfile."
echo ""

if test $fail != 0; then
	echo "Copies of the error_log and `basename $strfile` files are in"
	echo "`pwd`."
	echo ""

	exit 1
fi

#
# End of "$Id: run-stp-tests.sh 12658 2015-05-22 17:53:45Z msweet $"
#
