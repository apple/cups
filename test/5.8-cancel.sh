#!/bin/sh
#
# Test the cancel command.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "Cancel Destination Test"
echo ""
echo "    lp -d Test1 -o job-hold-until=indefinite testfile.jpg"
$runcups $VALGRIND ../systemv/lp -d Test1 -o job-hold-until=indefinite ../examples/testfile.jpg 2>&1
echo "    cancel Test1"
$runcups $VALGRIND ../systemv/cancel Test1 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Cancel All Test"
echo ""
echo "    cancel -a"
$runcups $VALGRIND ../systemv/cancel -a 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Purge All Test"
echo ""
echo "    cancel -a -x"
$runcups $VALGRIND ../systemv/cancel -a -x 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""
