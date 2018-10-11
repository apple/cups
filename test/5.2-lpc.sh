#!/bin/sh
#
# Test the lpc command.
#
# Copyright © 2007 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "LPC Test"
echo ""
echo "    lpc status"
$runcups $VALGRIND ../berkeley/lpc status 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""
