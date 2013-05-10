#!/bin/sh
#
# "$Id$"
#
#   Test the lpadmin command.
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

echo "Add Printer Test"
echo ""
echo "    lpadmin -p Test3 -v file:/dev/null -E -m deskjet.ppd"
../systemv/lpadmin -p Test3 -v file:/dev/null -E -m deskjet.ppd 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Modify Printer Test"
echo ""
echo "    lpadmin -p Test3 -v file:/tmp/Test3 -o PageSize=A4"
../systemv/lpadmin -p Test3 -v file:/tmp/Test3 -o PageSize=A4 2>&1
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
../systemv/lpadmin -x Test3 2>&1
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
