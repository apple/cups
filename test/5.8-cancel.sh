#!/bin/sh
#
# "$Id$"
#
#   Test the cancel command.
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

echo "Cancel Destination Test"
echo ""
echo "    lp -d Test1 -o job-hold-until=indefinite testfile.jpg"
../systemv/lp -d Test1 -o job-hold-until=indefinite testfile.jpg 2>&1
echo "    cancel Test1"
../systemv/cancel Test1 2>&1
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
../systemv/cancel -a 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

#
# End of "$Id$".
#
