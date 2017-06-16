#!/bin/sh
#
# Test the lpstat command.
#
# Copyright 2007-2017 by Apple Inc.
# Copyright 1997-2005 by Easy Software Products, all rights reserved.
#
# These coded instructions, statements, and computer programs are the
# property of Apple Inc. and are protected by Federal copyright
# law.  Distribution and use rights are outlined in the file "LICENSE.txt"
# which should have been included with this file.  If this file is
# file is missing or damaged, see the license at "http://www.cups.org/".
#

echo "LPSTAT Basic Test"
echo ""
echo "    lpstat -t"
$VALGRIND ../systemv/lpstat -t 2>&1
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
printers="`$VALGRIND ../systemv/lpstat -e 2>&1`"
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
server="`$VALGRIND ../systemv/lpstat -H 2>&1`"
if test $? != 0 -o "x$server" != x$CUPS_SERVER; then
	echo "    FAILED ($server)"
	exit 1
else
	echo "    PASSED ($server)"
fi
echo ""
