#!/bin/sh
#
# Test the lpinfo command.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "LPINFO Devices Test"
echo ""
echo "    lpinfo -v"
$runcups $VALGRIND ../systemv/lpinfo -v 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPINFO Drivers Test"
echo ""
echo "    lpinfo -m"
$runcups $VALGRIND ../systemv/lpinfo -m 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPINFO Drivers Test"
echo ""
echo "    lpinfo -m | grep -q sample.drv"
$runcups $VALGRIND ../systemv/lpinfo -m | grep -q sample.drv 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""
