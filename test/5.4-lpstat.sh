#!/bin/sh
#
# Test the lpstat command.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "LPSTAT Basic Test"
echo ""
echo "    lpstat -t"
$runcups $VALGRIND ../systemv/lpstat -t 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPSTAT Enumeration Test"
echo ""
echo "    lpstat -e"
printers="`$runcups $VALGRIND ../systemv/lpstat -e 2>&1`"
if test $? != 0 -o "x$printers" = x; then
	echo "    FAILED"
	exit 1
else
	for printer in $printers; do
	        echo $printer
	done
	echo "    PASSED"
fi
echo ""

echo "LPSTAT Get Host Test"
echo ""
echo "    lpstat -H"
server="`$runcups $VALGRIND ../systemv/lpstat -H 2>&1`"
if test $? != 0 -o "x$server" != x$CUPS_SERVER; then
	echo "    FAILED ($server)"
	exit 1
else
	echo "    PASSED ($server)"
fi
echo ""
