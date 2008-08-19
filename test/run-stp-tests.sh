#!/bin/sh
#
# "$Id$"
#
#   Perform the complete set of IPP compliance tests specified in the
#   CUPS Software Test Plan.
#
#   Copyright 2007-2008 by Apple Inc.
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
		pjobs=10
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
mkdir /tmp/cups-$user/bin/driver
mkdir /tmp/cups-$user/bin/filter
mkdir /tmp/cups-$user/certs
mkdir /tmp/cups-$user/share
mkdir /tmp/cups-$user/share/banners
mkdir /tmp/cups-$user/share/drv
mkdir /tmp/cups-$user/share/locale
for file in ../locale/cups_*.po; do
	loc=`basename $file .po | cut -c 6-`
	mkdir /tmp/cups-$user/share/locale/$loc
	ln -s $root/locale/cups_$loc.po /tmp/cups-$user/share/locale/$loc
	ln -s $root/locale/ppdc_$loc.po /tmp/cups-$user/share/locale/$loc
done
mkdir /tmp/cups-$user/share/mime
mkdir /tmp/cups-$user/share/model
mkdir /tmp/cups-$user/share/ppdc
mkdir /tmp/cups-$user/interfaces
mkdir /tmp/cups-$user/log
mkdir /tmp/cups-$user/ppd
mkdir /tmp/cups-$user/spool
mkdir /tmp/cups-$user/spool/temp
mkdir /tmp/cups-$user/ssl

ln -s $root/backend/http /tmp/cups-$user/bin/backend
ln -s $root/backend/ipp /tmp/cups-$user/bin/backend
ln -s $root/backend/lpd /tmp/cups-$user/bin/backend
ln -s $root/backend/mdns /tmp/cups-$user/bin/backend
ln -s $root/backend/parallel /tmp/cups-$user/bin/backend
ln -s $root/backend/serial /tmp/cups-$user/bin/backend
ln -s $root/backend/snmp /tmp/cups-$user/bin/backend
ln -s $root/backend/socket /tmp/cups-$user/bin/backend
ln -s $root/backend/usb /tmp/cups-$user/bin/backend
ln -s $root/cgi-bin /tmp/cups-$user/bin
ln -s $root/monitor /tmp/cups-$user/bin
ln -s $root/notifier /tmp/cups-$user/bin
ln -s $root/scheduler /tmp/cups-$user/bin/daemon
ln -s $root/filter/commandtops /tmp/cups-$user/bin/filter
ln -s $root/filter/hpgltops /tmp/cups-$user/bin/filter
ln -s $root/filter/pstops /tmp/cups-$user/bin/filter
ln -s $root/filter/rastertoepson /tmp/cups-$user/bin/filter
ln -s $root/filter/rastertohp /tmp/cups-$user/bin/filter
ln -s $root/filter/texttops /tmp/cups-$user/bin/filter

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
ln -s $root/ppdc/sample.drv /tmp/cups-$user/share/drv
ln -s $root/conf/mime.types /tmp/cups-$user/share/mime
ln -s $root/conf/mime.convs /tmp/cups-$user/share/mime
ln -s $root/data/*.h /tmp/cups-$user/share/ppdc
ln -s $root/data/*.defs /tmp/cups-$user/share/ppdc
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

	if test -f /private/etc/cups/apple.types; then
		ln -s /private/etc/cups/apple.* /tmp/cups-$user/share/mime
	elif test -f /usr/share/cups/mime/apple.types; then
		ln -s /usr/share/cups/mime/apple.* /tmp/cups-$user/share/mime
	fi
else
	ln -s $root/filter/imagetops /tmp/cups-$user/bin/filter
	ln -s $root/filter/imagetoraster /tmp/cups-$user/bin/filter
	ln -s $root/filter/pdftops /tmp/cups-$user/bin/filter
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
PassEnv LOCALEDIR
DocumentRoot $root/doc
RequestRoot /tmp/cups-$user/spool
TempDir /tmp/cups-$user/spool/temp
MaxLogSize 0
AccessLog /tmp/cups-$user/log/access_log
ErrorLog /tmp/cups-$user/log/error_log
PageLog /tmp/cups-$user/log/page_log
LogLevel debug2
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

#
# Setup lots of test queues - half with PPD files, half without...
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

#
# Setup the paths...
#

echo "Setting up environment variables for test..."

if test "x$LD_LIBRARY_PATH" = x; then
	LD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/driver:$root/ppdc"
else
	LD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/driver:$root/ppdc:$LD_LIBRARY_PATH"
fi

export LD_LIBRARY_PATH

LD_PRELOAD="$root/cups/libcups.so.2:$root/filter/libcupsimage.so.2:$root/cgi-bin/libcupscgi.so.1:$root/scheduler/libcupsmime.so.1:$root/driver/libcupsdriver.so.1:$root/ppdc/libcupsppdc.so.1"
export LD_PRELOAD

if test "x$DYLD_LIBRARY_PATH" = x; then
	DYLD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/driver:$root/ppdc"
else
	DYLD_LIBRARY_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/driver:$root/ppdc:$DYLD_LIBRARY_PATH"
fi

export DYLD_LIBRARY_PATH

if test "x$SHLIB_PATH" = x; then
	SHLIB_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/driver:$root/ppdc"
else
	SHLIB_PATH="$root/cups:$root/filter:$root/cgi-bin:$root/scheduler:$root/driver:$root/ppdc:$SHLIB_PATH"
fi

export SHLIB_PATH

CUPS_SERVER=localhost; export CUPS_SERVER
CUPS_SERVERROOT=/tmp/cups-$user; export CUPS_SERVERROOT
CUPS_STATEDIR=/tmp/cups-$user; export CUPS_STATEDIR
CUPS_DATADIR=/tmp/cups-$user/share; export CUPS_DATADIR
LOCALEDIR=/tmp/cups-$user/share/locale; export LOCALEDIR

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

date=`date "+%Y-%m-%d"`
strfile=/tmp/cups-$user/cups-str-1.4-$date-$user.html

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

	./ipptest ipp://localhost:$port/printers $file | tee -a $strfile
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
echo $date by $user on `hostname`. >>$strfile
echo "<PRE>" >>$strfile

for file in 5*.sh; do
	echo "Performing $file..."
	echo "" >>$strfile
	echo "\"$file\":" >>$strfile

	sh $file $pjobs | tee -a $strfile
	status=$?

	if test $status != 0; then
		echo Test failed.
		fail=`expr $fail + 1`
	fi
done

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

# Pages printed on Test1 (within 1 page for timing-dependent cancel issues)
count=`grep '^Test1 ' /tmp/cups-$user/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
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
count=`grep '^Test2 ' /tmp/cups-$user/log/page_log | awk 'BEGIN{count=0}{count=count+$7}END{print count}'`
expected=`expr $pjobs \* 2 + 3`
if test $count != $expected; then
	echo "FAIL: Printer 'Test2' produced $count page(s), expected $expected."
	echo "<P>FAIL: Printer 'Test2' produced $count page(s), expected $expected.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: Printer 'Test2' correctly produced $count page(s)."
	echo "<P>PASS: Printer 'Test2' correctly produced $count page(s).</P>" >>$strfile
fi

# Requested processed
count=`wc -l /tmp/cups-$user/log/access_log | awk '{print $1}'`
echo "PASS: $count requests processed."
echo "<P>PASS: $count requests processed.</P>" >>$strfile

# Emergency log messages
count=`grep '^X ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count emergency messages, expected 0."
	grep '^X ' /tmp/cups-$user/log/error_log
	echo "<P>FAIL: $count emergency messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	grep '^X ' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count emergency messages."
	echo "<P>PASS: $count emergency messages.</P>" >>$strfile
fi

# Alert log messages
count=`grep '^A ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count alert messages, expected 0."
	grep '^A ' /tmp/cups-$user/log/error_log
	echo "<P>FAIL: $count alert messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	grep '^A ' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count alert messages."
	echo "<P>PASS: $count alert messages.</P>" >>$strfile
fi

# Critical log messages
count=`grep '^C ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count critical messages, expected 0."
	grep '^C ' /tmp/cups-$user/log/error_log
	echo "<P>FAIL: $count critical messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	grep '^C ' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count critical messages."
	echo "<P>PASS: $count critical messages.</P>" >>$strfile
fi

# Error log messages
count=`grep '^E ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count != 17; then
	echo "FAIL: $count error messages, expected 17."
	grep '^E ' /tmp/cups-$user/log/error_log
	echo "<P>FAIL: $count error messages, expected 9.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	grep '^E ' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count error messages."
	echo "<P>PASS: $count error messages.</P>" >>$strfile
fi

# Warning log messages
count=`grep '^W ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count warning messages, expected 0."
	grep '^W ' /tmp/cups-$user/log/error_log
	echo "<P>FAIL: $count warning messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	grep '^W ' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count warning messages."
	echo "<P>PASS: $count warning messages.</P>" >>$strfile
fi

# Notice log messages
count=`grep '^N ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count != 0; then
	echo "FAIL: $count notice messages, expected 0."
	grep '^N ' /tmp/cups-$user/log/error_log
	echo "<P>FAIL: $count notice messages, expected 0.</P>" >>$strfile
	echo "<PRE>" >>$strfile
	grep '^N ' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
	echo "</PRE>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count notice messages."
	echo "<P>PASS: $count notice messages.</P>" >>$strfile
fi

# Info log messages
count=`grep '^I ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count info messages, expected more than 0."
	echo "<P>FAIL: $count info messages, expected more than 0.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count info messages."
	echo "<P>PASS: $count info messages.</P>" >>$strfile
fi

# Debug log messages
count=`grep '^D ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count debug messages, expected more than 0."
	echo "<P>FAIL: $count debug messages, expected more than 0.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count debug messages."
	echo "<P>PASS: $count debug messages.</P>" >>$strfile
fi

# Debug2 log messages
count=`grep '^d ' /tmp/cups-$user/log/error_log | wc -l | awk '{print $1}'`
if test $count = 0; then
	echo "FAIL: $count debug2 messages, expected more than 0."
	echo "<P>FAIL: $count debug2 messages, expected more than 0.</P>" >>$strfile
	fail=`expr $fail + 1`
else
	echo "PASS: $count debug2 messages."
	echo "<P>PASS: $count debug2 messages.</P>" >>$strfile
fi

# Page log file...
if grep -q 'testfile.pdf Letter' /tmp/cups-$user/log/page_log; then
	echo "PASS: page_log formatted correctly."
	echo "<P>PASS: page_log formatted correctly.</P>" >>$strfile
else
	echo "FAIL: page_log formatted incorrectly."
	echo "<P>FAIL: page_log formatted incorrectly.</P>" >>$strfile
	fail=`expr $fail + 1`
fi

# Log files...
echo "<H2>access_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' /tmp/cups-$user/log/access_log >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>error_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
grep -v '^[dD]' /tmp/cups-$user/log/error_log | sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>page_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
sed -e '1,$s/&/&amp;/g' -e '1,$s/</&lt;/g' /tmp/cups-$user/log/page_log >>$strfile
echo "</PRE>" >>$strfile

#
# Format the reports and tell the user where to find them...
#

cat str-trailer.html >>$strfile

echo ""

if test $fail != 0; then
	echo "$fail tests failed."
	cp /tmp/cups-$user/log/error_log error_log-$date-$user
	cp $strfile .
else
	echo "All tests were successful."
fi

echo "Log files can be found in /tmp/cups-$user/log."
echo "A HTML report was created in $strfile."
echo ""

if test $fail != 0; then
	echo "Copies of the error_log and `basename $strfile` files are in"
	echo "`pwd`."
	echo ""

	exit 1
fi

#
# End of "$Id$"
#
