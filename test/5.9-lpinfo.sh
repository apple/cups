#!/bin/sh
#
# "$Id: 5.9-lpinfo.sh 7711 2008-07-02 04:39:27Z mike $"
#
#   Test the lpinfo command.
#
#   Copyright 2007-2011 by Apple Inc.
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
$VALGRIND ../systemv/lpinfo -v 2>&1
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
$VALGRIND ../systemv/lpinfo -m 2>&1
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
$VALGRIND ../systemv/lpinfo -m | grep -q sample.drv 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

#
# End of "$Id: 5.9-lpinfo.sh 7711 2008-07-02 04:39:27Z mike $".
#
