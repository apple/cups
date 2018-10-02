#!/bin/sh
#
# Test the lpadmin command.
#
# Copyright © 2007-2018 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "Add Printer Test"
echo ""
echo "    lpadmin -p Test3 -v file:/dev/null -E -m drv:///sample.drv/deskjet.ppd"
$runcups $VALGRIND ../systemv/lpadmin -p Test3 -v file:/dev/null -E -m drv:///sample.drv/deskjet.ppd 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	if test -f $CUPS_SERVERROOT/ppd/Test3.ppd; then
		echo "    PASSED"
	else
		echo "    FAILED (No PPD)"
		exit 1
	fi
fi
echo ""

echo "Modify Printer Test"
echo ""
echo "    lpadmin -p Test3 -v file:/tmp/Test3 -o PageSize=A4"
$runcups $VALGRIND ../systemv/lpadmin -p Test3 -v file:/tmp/Test3 -o PageSize=A4 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Delete Printer Test"
echo ""
echo "    lpadmin -x Test3"
$runcups $VALGRIND ../systemv/lpadmin -x Test3 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Add Shared Printer Test"
echo ""
echo "    lpadmin -p Test3 -E -v ipp://localhost:$IPP_PORT/printers/Test2 -m everywhere"
$runcups $VALGRIND ../systemv/lpadmin -p Test3 -E -v ipp://localhost:$IPP_PORT/printers/Test2 -m everywhere 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""
