#!/bin/sh
#
# Test the lprm command.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "LPRM Current Test"
echo ""
echo "    lpr -o job-hold-until=indefinite testfile.jpg"
$runcups $VALGRIND ../berkeley/lpr -o job-hold-until=indefinite ../examples/testfile.jpg 2>&1
echo "    lprm"
$runcups $VALGRIND ../berkeley/lprm 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPRM Destination Test"
echo ""
echo "    lpr -P Test1 -o job-hold-until=indefinite testfile.jpg"
$runcups $VALGRIND ../berkeley/lpr -P Test1 -o job-hold-until=indefinite ../examples/testfile.jpg 2>&1
echo "    lprm Test1"
$runcups $VALGRIND ../berkeley/lprm Test1 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""
