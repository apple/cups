#!/bin/sh
#
# "$Id: run-stp-tests.sh,v 1.4.2.17 2003/11/19 19:59:26 mike Exp $"
#
#   Perform the complete set of IPP compliance tests specified in the
#   CUPS Software Test Plan.
#
#   Copyright 1997-2002 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

#
# Make the IPP test program...
#

make

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
echo "1 - Basic conformance test, no load testing (all systems)"
echo "2 - Basic conformance test, some load testing (minimum 256MB VM, 50MB disk)"
echo "3 - Basic conformance test, extreme load testing (minimum 1GB VM, 500MB disk)"
echo "4 - Basic conformance test, torture load testing (minimum 2GB VM, 1GB disk)"
echo ""
echo "Please enter the number of the test you wish to perform:"

read testtype

case "$testtype" in
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
# Information for the server/tests...
#

user=`whoami`
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
echo "Please enter Y to use Valgrind or N to not use Valgrind:"

read usevalgrind

case "$usevalgrind" in
	Y* | y*)
		valgrind="valgrind --logfile=/tmp/$user/log/valgrind --error-limit=no --leak-check=yes --trace-children=yes"
		echo "Using Valgrind; log files can be found in /tmp/$user/log..."
		;;

	*)
		valgrind=""
		;;
esac

#
# Start by creating temporary directories for the tests...
#

rm -rf /tmp/$user
mkdir /tmp/$user
mkdir /tmp/$user/bin
mkdir /tmp/$user/bin/backend
mkdir /tmp/$user/bin/filter
mkdir /tmp/$user/certs
mkdir /tmp/$user/share
mkdir /tmp/$user/share/banners
mkdir /tmp/$user/share/model
mkdir /tmp/$user/interfaces
mkdir /tmp/$user/log
mkdir /tmp/$user/ppd
mkdir /tmp/$user/spool
mkdir /tmp/$user/spool/temp
mkdir /tmp/$user/ssl

ln -s $root/backend/http /tmp/$user/bin/backend
ln -s $root/backend/ipp /tmp/$user/bin/backend
ln -s $root/backend/lpd /tmp/$user/bin/backend
ln -s $root/backend/parallel /tmp/$user/bin/backend
ln -s $root/backend/serial /tmp/$user/bin/backend
ln -s $root/backend/socket /tmp/$user/bin/backend
ln -s $root/backend/usb /tmp/$user/bin/backend
ln -s $root/cgi-bin /tmp/$user/bin
ln -s $root/filter/hpgltops /tmp/$user/bin/filter
ln -s $root/filter/imagetops /tmp/$user/bin/filter
ln -s $root/filter/imagetoraster /tmp/$user/bin/filter
ln -s $root/filter/pstops /tmp/$user/bin/filter
ln -s $root/filter/rastertoepson /tmp/$user/bin/filter
ln -s $root/filter/rastertohp /tmp/$user/bin/filter
ln -s $root/filter/texttops /tmp/$user/bin/filter
ln -s $root/pdftops/pdftops /tmp/$user/bin/filter

ln -s $root/data/classified /tmp/$user/share/banners
ln -s $root/data/confidential /tmp/$user/share/banners
ln -s $root/data/secret /tmp/$user/share/banners
ln -s $root/data/standard /tmp/$user/share/banners
ln -s $root/data/topsecret /tmp/$user/share/banners
ln -s $root/data/unclassified /tmp/$user/share/banners
ln -s $root/data /tmp/$user/share/charsets
ln -s $root/data /tmp/$user/share
ln -s $root/fonts /tmp/$user/share
ln -s $root/ppd/*.ppd /tmp/$user/share/model
ln -s $root/templates /tmp/$user/share

#
# Then create the necessary config files...
#

cat >/tmp/$user/cupsd.conf <<EOF
Browsing Off
FileDevice yes
Listen 127.0.0.1:$port
User $user
ServerRoot /tmp/$user
ServerBin /tmp/$user/bin
DataDir /tmp/$user/share
FontPath /tmp/$user/share/fonts
DocumentRoot $root/doc
RequestRoot /tmp/$user/spool
TempDir /tmp/$user/spool/temp
MaxLogSize 0
AccessLog /tmp/$user/log/access_log
ErrorLog /tmp/$user/log/error_log
PageLog /tmp/$user/log/page_log
LogLevel debug2
PreserveJobHistory Yes
<Location />
Order deny,allow
Deny from all
Allow from 127.0.0.1
</Location>
<Location /admin>
Order deny,allow
Deny from all
Allow from 127.0.0.1
Require valid-user
</Location>
EOF

touch /tmp/$user/classes.conf
touch /tmp/$user/printers.conf

#
# Setup lots of test queues - 500 with PPD files, 500 without...
#

i=1
while test $i -le $nprinters1; do
	cat >>/tmp/$user/printers.conf <<EOF
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

	cp testps.ppd /tmp/$user/ppd/test-$i.ppd

	i=`expr $i + 1`
done

while test $i -le $nprinters2; do
	cat >>/tmp/$user/printers.conf <<EOF
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

cp /tmp/$user/printers.conf /tmp/$user/printers.conf.orig

cp $root/conf/mime.types /tmp/$user/mime.types
cp $root/conf/mime.convs /tmp/$user/mime.convs

#
# Setup the paths...
#

if test "x$LD_LIBRARY_PATH" = x; then
	LD_LIBRARY_PATH="$root/cups:$root/filter"
else
	LD_LIBRARY_PATH="$root/cups:$root/filter:$LD_LIBRARY_PATH"
fi

export LD_LIBRARY_PATH

LD_PRELOAD="$root/cups/libcups.so:$root/filter/libcupsimage.so"
export LD_PRELOAD

if test "x$DYLD_LIBRARY_PATH" = x; then
	DYLD_LIBRARY_PATH="$root/cups:$root/filter"
else
	DYLD_LIBRARY_PATH="$root/cups:$root/filter:$DYLD_LIBRARY_PATH"
fi

export SHLIB_PATH

if test "x$SHLIB_PATH" = x; then
	SHLIB_PATH="$root/cups:$root/filter"
else
	SHLIB_PATH="$root/cups:$root/filter:$SHLIB_PATH"
fi

export SHLIB_PATH

CUPS_SERVERROOT=/tmp/$user; export CUPS_SERVERROOT
CUPS_DATADIR=/tmp/$user/share; export CUPS_DATADIR

#
# Set a new home directory to avoid getting user options mixed in...
#

HOME=/tmp/$user
export HOME

#
# Start the server; run as foreground daemon in the background...
#

echo "Starting scheduler..."

$valgrind ../scheduler/cupsd -c /tmp/$user/cupsd.conf -f >/tmp/$user/log/debug_log &
cupsd=$!

#if test -x /usr/bin/strace; then
#	# Trace system calls in cupsd if we have strace...
#	/usr/bin/strace -tt -o /tmp/$user/log/cupsd.trace -p $cupsd &
#fi

echo "Scheduler is PID $cupsd; run debugger now if you need to."
echo ""
echo "Press ENTER to continue..."
read junk

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

strfile=cups-str-1.2-`date +%Y-%m-%d`-`whoami`.shtml

rm -f $strfile
cat str-header.html >$strfile

#
# Run the IPP tests...
#

echo "Running IPP compliance tests..."

echo "<H1>IPP Compliance Tests</H1>" >>$strfile
echo "<P>This section provides the results to the IPP compliance tests" >>$strfile
echo "outlined in the CUPS Software Test Plan. These tests were run on" >>$strfile
echo `date "+%Y-%m-%d"` by `whoami` on `hostname`. >>$strfile
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

echo "Running command tests..."

echo "<H1>Command Tests</H1>" >>$strfile
echo "<P>This section provides the results to the command tests" >>$strfile
echo "outlined in the CUPS Software Test Plan. These tests were run on" >>$strfile
echo `date "+%Y-%m-%d"` by `whoami` on `hostname`. >>$strfile
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

echo "<H1>Log Files</H1>" >>$strfile

echo "<H2>access_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
cat /tmp/$user/log/access_log >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>error_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
cat /tmp/$user/log/error_log >>$strfile
echo "</PRE>" >>$strfile

echo "<H2>page_log</H2>" >>$strfile
echo "<PRE>" >>$strfile
cat /tmp/$user/log/page_log >>$strfile
echo "</PRE>" >>$strfile

if test -f /tmp/$user/log/cupsd.trace; then
	echo "<H2>cupsd.trace</H2>" >>$strfile
	echo "<PRE>" >>$strfile
	cat /tmp/$user/log/cupsd.trace >>$strfile
	echo "</PRE>" >>$strfile
fi

#
# Format the reports and tell the user where to find them...
#

echo "Formatting reports..."

cat str-trailer.html >>$strfile

htmlfile=`basename $strfile .shtml`.html
pdffile=`basename $strfile .shtml`.pdf

htmldoc --numbered --verbose --titleimage ../doc/images/cups-large.gif \
	-f $htmlfile $strfile
htmldoc --numbered --verbose --titleimage ../doc/images/cups-large.gif \
	-f $pdffile $strfile

echo ""

if test $fail != 0; then
	echo "$fail tests failed."
else
	echo "All tests were successful."
fi

if test "x$valgrind" != x; then
	echo "Valgrind lof files can be found in /tmp/$user/log."
fi

echo ""
echo "See the following files for details:"
echo ""
echo "    $htmlfile"
echo "    $pdffile"
echo ""

#
# End of "$Id: run-stp-tests.sh,v 1.4.2.17 2003/11/19 19:59:26 mike Exp $"
#
