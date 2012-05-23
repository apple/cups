#!/bin/sh
#
# "$Id: 5.1-lpadmin.sh 7494 2008-04-25 18:36:46Z mike $"
#
#   Test the lpadmin command.
#
#   Copyright 2007-2012 by Apple Inc.
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
echo "    lpadmin -p Test3 -v file:/dev/null -E -m drv:///sample.drv/deskjet.ppd"
$VALGRIND ../systemv/lpadmin -p Test3 -v file:/dev/null -E -m drv:///sample.drv/deskjet.ppd 2>&1
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
$VALGRIND ../systemv/lpadmin -p Test3 -v file:/tmp/Test3 -o PageSize=A4 2>&1
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
$VALGRIND ../systemv/lpadmin -x Test3 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Add Shared Printer Test"
echo ""
echo "    lpadmin -p Test3 -E -v ipp://localhost:8631/printers/Test2 -m raw"
$VALGRIND ../systemv/lpadmin -p Test3 -E -v ipp://localhost:8631/printers/Test2 -m raw 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

#
# End of "$Id: 5.1-lpadmin.sh 7494 2008-04-25 18:36:46Z mike $".
#
