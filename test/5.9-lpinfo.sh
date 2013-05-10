#!/bin/sh
#
# "$Id$"
#
#   Test the lpinfo command.
#
#   Copyright 2007 by Apple Inc.
#   Copyright 1997-2005 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

echo "LPINFO Devices Test"
echo ""
echo "    lpinfo -v"
../systemv/lpinfo -v 2>&1
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
../systemv/lpinfo -m 2>&1
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
