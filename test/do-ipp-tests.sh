#!/bin/sh
#
# "$Id: do-ipp-tests.sh,v 1.1 2001/02/22 16:48:13 mike Exp $"
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

#
# Start by creating a temporary directory for the tests...
#

rm -rf /tmp/$user
mkdir /tmp/$user
mkdir /tmp/$user/certs
mkdir /tmp/$user/log
mkdir /tmp/$user/spool
mkdir /tmp/$user/spool/temp

#
# Then create the necessary config files...
#

cat >/tmp/$user/cupsd.conf <<EOF
Listen 127.0.0.1:$port
User $user
ServerRoot /tmp/$user
RequestRoot /tmp/$user/spool
TempDir /tmp/$user/spool/temp
AccessLog /tmp/$user/log/access_log
ErrorLog /tmp/$user/log/error_log
PageLog /tmp/$user/log/page_log
LogLevel debug2
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
# Start the server; run as foreground daemon in the background...
#

echo Starting scheduler...

echo XXXXX: Change to use installed cupsd...
../scheduler/cupsd -c /tmp/$user/cupsd.conf -f &
cupsd=$!

IPP_PORT=$port; export IPP_PORT

while true; do
	echo XXXXX: Change to use installed lpstat...
	running=`../systemv/lpstat -r 2>/dev/null`
	if test "x$running" = "xscheduler is running"; then
		break
	fi

	echo Waiting for scheduler to become ready...
	sleep 5
done

#
# Run the tests, creating a report in the current directory...
#

echo Running tests...

rm -f ipp-test.report

echo IPP Test Report >ipp-test.report
echo "" >>ipp-test.report
echo Date: `date` >>ipp-test.report
echo User: $user >>ipp-test.report
echo Host: `hostname` >>ipp-test.report

for file in 0*.test; do
	echo Performing $file...
	echo "" >>ipp-test.report
	ipptest ipp://localhost:$port/printers $file >>ipp-test.report || break
done

if test $$ != 0; then
	echo Test failed.
	echo "See the following files for details:"
	echo ""
	echo "    ipp-test.report"
	echo "    /tmp/$user/log/error_log"
else
	echo All tests passed.
fi

#
# Stop the server...
#

kill $cupsd

#
# End of "$Id: do-ipp-tests.sh,v 1.1 2001/02/22 16:48:13 mike Exp $"
#
