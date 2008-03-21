#!/bin/sh
#
# "$Id$"
#
#   Perform the complete set of IPP compliance tests specified in the
#   CUPS Software Test Plan.
#
#   Copyright 2007 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

argcount=$#

#
# Make the IPP test program...
#

make

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
		;;
	2)
		echo "Running the medium tests (2)"
		nprinters1=10
		nprinters2=20
		pjobs=20
		;;
	3)
		echo "Running the extreme tests (3)"
		nprinters1=500
		nprinters2=1000
		pjobs=100
		;;
	4)
		echo "Running the torture tests (4)"
		nprinters1=10000
		nprinters2=20000
		pjobs=200
		;;
	*)
		echo "Running the timid tests (1)"
		nprinters1=0
		nprinters2=0
		pjobs=0
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

port=8631
cwd=`pwd`
root=`dirname $cwd`

#
# Make sure that the LPDEST and PRINTER environment variables are
# not included in the environment that is passed to the tests.  These
# will usually cause tests to fail erroneously...
#

typeset +x LPDEST
typeset +x PRINTER

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
		valgrind="valgrind --tool=memcheck --log-file=/tmp/cups-$user/log/valgrind --error-limit=no --leak-check=yes --trace-children=yes"
		echo "Using Valgrind; log files can be found in /tmp/cups-$user/log..."
		;;

	*)
		valgrind=""
		;;
esac

#
# Start by creating temporary directories for the tests...
#

echo "Creating directories for test..."

rm -rf /tmp/cups-$user
mkdir /tmp/cups-$user
mkdir /tmp/cups-$user/bin
mkdir /tmp/cups-$user/bin/backend
mkdir /tmp/cups-$user/bin/filter
mkdir /tmp/cups-$user/certs
mkdir /tmp/cups-$user/share
mkdir /tmp/cups-$user/share/banners
mkdir /tmp/cups-$user/share/model
mkdir /tmp/cups-$user/interfaces
mkdir /tmp/cups-$user/log
mkdir /tmp/cups-$user/ppd
mkdir /tmp/cups-$user/spool
mkdir /tmp/cups-$user/spool/temp
mkdir /tmp/cups-$user/ssl

ln -s $root/backend/http /tmp/cups-$user/bin/backend
ln -s $root/backend/ipp /tmp/cups-$user/bin/backend
ln -s $root/backend/lpd /tmp/cups-$user/bin/backend
ln -s $root/backend/parallel /tmp/cups-$user/bin/backend
ln -s $root/backend/serial /tmp/cups-$user/bin/backend
ln -s $root/backend/snmp /tmp/cups-$user/bin/backend
ln -s $root/backend/socket /tmp/cups-$user/bin/backend
ln -s $root/backend/usb /tmp/cups-$user/bin/backend
ln -s $root/cgi-bin /tmp/cups-$user/bin
ln -s $root/monitor /tmp/cups-$user/bin
ln -s $root/notifier /tmp/cups-$user/bin
ln -s $root/scheduler /tmp/cups-$user/bin/daemon
ln -s $root/filter/hpgltops /tmp/cups-$user/bin/filter
ln -s $root/filter/imagetops /tmp/cups-$user/bin/filter
ln -s $root/filter/imagetoraster /tmp/cups-$user/bin/filter
ln -s $root/filter/pstops /tmp/cups-$user/bin/filter
ln -s $root/filter/rastertoepson /tmp/cups-$user/bin/filter
ln -s $root/filter/rastertohp /tmp/cups-$user/bin/filter
ln -s $root/filter/texttops /tmp/cups-$user/bin/filter
ln -s $root/pdftops/pdftops /tmp/cups-$user/bin/filter

ln -s $root/data/classified /tmp/cups-$user/share/banners
ln -s $root/data/confidential /tmp/cups-$user/share/banners
ln -s $root/data/secret /tmp/cups-$user/share/banners
ln -s $root/data/standard /tmp/cups-$user/share/banners
ln -s $root/data/topsecret /tmp/cups-$user/share/banners
ln -s $root/data/unclassified /tmp/cups-$user/share/banners
ln -s $root/data /tmp/cups-$user/share/charmaps
ln -s $root/data /tmp/cups-$user/share/charsets
ln -s $root/data /tmp/cups-$user/share
ln -s $root/fonts /tmp/cups-$user/share
ln -s $root/ppd/*.ppd /tmp/cups-$user/share/model
ln -s $root/templates /tmp/cups-$user/share

if test $ssltype != 0; then
	mkdir $root/ssl
	cp server.* $root/ssl
fi

#
# Mac OS X filters and configuration files...
#

if test `uname` = Darwin; then
	ln -s /usr/libexec/cups/filter/cgpdfto* /tmp/cups-$user/bin/filter
	ln -s /usr/libexec/cups/filter/nsimagetopdf /tmp/cups-$user/bin/filter
	ln -s /usr/libexec/cups/filter/nstexttopdf /tmp/cups-$user/bin/filter
	ln -s /usr/libexec/cups/filter/pictwpstops /tmp/cups-$user/bin/filter
	ln -s /usr/libexec/cups/filter/pstoappleps /tmp/cups-$user/bin/filter
	ln -s /usr/libexec/cups/filter/pstocupsraster /tmp/cups-$user/bin/filter
	ln -s /usr/libexec/cups/filter/pstopdffilter /tmp/cups-$user/bin/filter

	ln -s /private/etc/cups/apple.* /tmp/cups-$user
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

cat >/tmp/cups-$user/cupsd.conf <<EOF
Browsing Off
FileDevice yes
Printcap
Listen 127.0.0.1:$port
User $user
ServerRoot /tmp/cups-$user
StateDir /tmp/cups-$user
ServerBin /tmp/cups-$user/bin
CacheDir /tmp/cups-$user/share
DataDir /tmp/cups-$user/share
FontPath /tmp/cups-$user/share/fonts
DocumentRoot $root/doc
RequestRoot /tmp/cups-$user/spool
TempDir /tmp/cups-$user/spool/temp
MaxLogSize 0
AccessLog /tmp/cups-$user/log/access_log
ErrorLog /tmp/cups-$user/log/error_log
PageLog /tmp/cups-$user/log/page_log
LogLevel debug
PreserveJobHistory Yes
<Policy default>
<Limit All>
Order Deny,Allow
Deny from all
Allow from 127.0.0.1
$encryption
</Limit>
</Policy>
EOF

touch /tmp/cups-$user/classes.conf
touch /tmp/cups-$user/printers.conf

#
# Setup lots of test queues - 500 with PPD files, 500 without...
#

echo "Creating printers.conf for test..."

i=1
while test $i -le $nprinters1; do
	cat >>/tmp/cups-$user/printers.conf <<EOF
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

	cp testps.ppd /tmp/cups-$user/ppd/test-$i.ppd

	i=`expr $i + 1`
done

while test $i -le $nprinters2; do
	cat >>/tmp/cups-$user/printers.conf <<EOF
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

cp /tmp/cups-$user/printers.conf /tmp/cups-$user/printers.conf.orig

cp $root/conf/mime.types /tmp/cups-$user/mime.types
cp $root/conf/mime.convs /tmp/cups-$user/mime.convs

#
# Setup the paths...
#

echo "Setting up environment variables for test..."

if test "x$LD_LIBRARY_PATH" = x; then
	LD_LIBRARY_PATH="$root/cups:$root/filter"
else
	LD_LIBRARY_PATH="$root/cups:$root/filter:$LD_LIBRARY_PATH"
fi

export LD_LIBRARY_PATH

LD_PRELOAD="$root/cups/libcups.so.2:$root/filter/libcupsimage.so.2"
export LD_PRELOAD

if test "x$DYLD_LIBRARY_PATH" = x; then
	DYLD_LIBRARY_PATH="$root/cups:$root/filter"
else
	DYLD_LIBRARY_PATH="$root/cups:$root/filter:$DYLD_LIBRARY_PATH"
fi

export DYLD_LIBRARY_PATH

if test "x$SHLIB_PATH" = x; then
	SHLIB_PATH="$root/cups:$root/filter"
else
	SHLIB_PATH="$root/cups:$root/filter:$SHLIB_PATH"
fi

export SHLIB_PATH

CUPS_SERVER=localhost; export CUPS_SERVER
CUPS_SERVERROOT=/tmp/cups-$user; export CUPS_SERVERROOT
CUPS_STATEDIR=/tmp/cups-$user; export CUPS_STATEDIR
CUPS_DATADIR=/tmp/cups-$user/share; export CUPS_DATADIR

#
# Set a new home directory to avoid getting user options mixed in...
#

HOME=/tmp/cups-$user
export HOME

#
# Force POSIX locale for tests...
#

LANG=C
export LANG

#
# Start the server; run as foreground daemon in the background...
#

echo "Starting scheduler:"
echo "    $valgrind ../scheduler/cupsd -c /tmp/cups-$user/cupsd.conf -f >/tmp/cups-$user/log/debug_log 2>&1 &"
echo ""

$valgrind ../scheduler/cupsd -c /tmp/cups-$user/cupsd.conf -f >/tmp/cups-$user/log/debug_log 2>&1 &
cupsd=$!

#if test -x /usr/bin/strace; then
#	# Trace system calls in cupsd if we have strace...
#	/usr/bin/strace -tt -o /tmp/cups-$user/log/cupsd.trace -p $cupsd &
#fi

if test "x$testtype" = x0; then
	echo "Scheduler is PID $cupsd and is listening on port 8631."
	echo ""
	echo "Set the IPP_PORT environment variable to 8631 to test the software"
	echo "interactively from the command-line."
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

strfile=cups-str-1.4-`date +%Y-%m-%d`-$user.html

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
for file in 4*.test; do
	echo "Performing $file..."
	echo "" >>$strfile

	./ipptest ipp://localhost:$port/printers $file >>$strfile
	status=$?

	if test $status != 0; then
		echo Test failed.
		fail=`expr $fail + 1`
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
echo `date "+%Y-%m-%d"` by $user on `hostname`. >>$strfile
echo "<PRE>" >>$strfile

for file in 5*.sh; do
	echo "Performing $file..."
	echo "" >>$strfile
	echo "\"$file\":" >>$strfile

	sh $file $pjobs >>$strfile
	status=$?

	if test $status != 0; then
		echo Test failed.
		fail=`expr $fail + 1`
	fi
done

echo "</PRE>" >>$strfile

#
# Wait for jobs to complete...
#

while true; do
	jobs=`../systemv/lpstat 2>/dev/null`
	if test "x$jobs" = "x"; then
		break
	fi

	echo "Waiting for jobs to complete..."
	sleep 10
done

#
# Stop the server...
#

kill $cupsd

#
# Append the log files for post-mortim...
#

echo "<H1>3 - Log Files</H1>" >>$strfile

echo "<H2>access_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
cat /tmp/cups-$user/log/access_log >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>error_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
cat /tmp/cups-$user/log/error_log >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>page_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
cat /tmp/cups-$user/log/page_log >>$strfile
echo "</PRE>" >>$strfile

if test -f /tmp/cups-$user/log/cupsd.trace; then
	echo "<H2>cupsd.trace</H2>" >>$strfile
	echo "<PRE>" >>$strfile
	cat /tmp/cups-$user/log/cupsd.trace >>$strfile
	echo "</PRE>" >>$strfile
fi

#
# Format the reports and tell the user where to find them...
#

cat str-trailer.html >>$strfile

echo ""

if test $fail != 0; then
	echo "$fail tests failed."
else
	echo "All tests were successful."
fi

echo "Log files can be found in /tmp/cups-$user/log."
echo "A HTML report was created in test/$strfile."
echo ""

if test $fail != 0; then
	exit 1
fi

#
# End of "$Id$"
#
