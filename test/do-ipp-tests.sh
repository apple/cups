#!/bin/sh
#
# "$Id: do-ipp-tests.sh,v 1.2 2001/02/23 01:15:16 mike Exp $"
#
#  Perform the complete set of IPP compliance tests specified in the
#  CUPS Software Test Plan.
#

#
# Make the IPP test program...
#

make

#
# Information for the server/tests...
#

user=`whoami`
port=8631
cwd=`pwd`
root=`dirname $cwd`

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
ln -s $root/pstoraster/pstoraster /tmp/$user/bin/filter

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
ln -s $root/pstoraster /tmp/$user/share
ln -s $root/templates /tmp/$user/share

#
# Then create the necessary config files...
#

cat >/tmp/$user/cupsd.conf <<EOF
Browsing Off
Listen 127.0.0.1:$port
User $user
ServerRoot /tmp/$user
ServerBin /tmp/$user/bin
DataDir /tmp/$user/share
DocumentRoot $root/doc
RequestRoot /tmp/$user/spool
TempDir /tmp/$user/spool/temp
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

cp /etc/cups/mime.types /tmp/$user/mime.types
cp /etc/cups/mime.convs /tmp/$user/mime.convs

#
# Setup the DSO path...
#

LD_LIBRARY_PATH=$root/cups:$root/filter; export LD_LIBRARY_PATH

#
# Start the server; run as foreground daemon in the background...
#

echo "Starting scheduler..."

../scheduler/cupsd -c /tmp/$user/cupsd.conf -f &
cupsd=$!

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
# Run the tests, creating a report in the current directory...
#

echo "Running tests..."

rm -f ipp-test.report

echo "IPP Test Report" >ipp-test.report
echo "" >>ipp-test.report
echo "Date: `date`" >>ipp-test.report
echo "User: $user" >>ipp-test.report
echo "Host: `hostname`" >>ipp-test.report

for file in [0-9]*.test; do
	echo "Performing $file..."
	echo "" >>ipp-test.report

	ipptest ipp://localhost:$port/printers $file >>ipp-test.report
	status=$?

	if test $status != 0; then
		break
	fi
done

if test $status != 0; then
	echo "Test failed."
	echo ""
	echo "See the following files for details:"
	echo ""
	echo "    ipp-test.report"
	echo "    /tmp/$user/log/error_log"
else
	echo "All tests passed."
	echo ""
	echo "See the following files for details:"
	echo ""
	echo "    ipp-test.report"
fi

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
# End of "$Id: do-ipp-tests.sh,v 1.2 2001/02/23 01:15:16 mike Exp $"
#
