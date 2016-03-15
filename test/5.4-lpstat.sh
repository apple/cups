#!/bin/sh
#
# "$Id: 5.4-lpstat.sh 12490 2015-02-06 18:45:48Z msweet $"
#
#   Test the lpstat command.
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

echo "LPSTAT Test"
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

echo "LPSTAT Test"
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

#
# End of "$Id: 5.4-lpstat.sh 12490 2015-02-06 18:45:48Z msweet $".
#
