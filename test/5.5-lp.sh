#!/bin/sh
#
# "$Id$"
#
#   Test the lp command.
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1997-2005 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

echo "LP Default Test"
echo ""
echo "    lp testfile.pdf"
../systemv/lp testfile.pdf 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LP Destination Test"
echo ""
echo "    lp -d Test2 testfile.jpg"
../systemv/lp -d Test2 testfile.jpg 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LP Options Test"
echo ""
echo "    lp -d Test1 -P 1-4 -o job-sheets=classified,classified testfile.pdf"
../systemv/lp -d Test1 -P 1-4 -o job-sheets=classified,classified testfile.pdf 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LP Flood Test ($1 times in parallel)"
echo ""
echo "    lp -d Test1 testfile.jpg"
echo "    lp -d Test2 testfile.jpg"
i=0
while test $i -lt $1; do
	echo "    flood copy $i..." 1>&2
	../systemv/lp -d Test1 testfile.jpg 2>&1 &
	../systemv/lp -d Test2 testfile.jpg 2>&1 &
	lppid=$!
	i=`expr $i + 1`
done
wait $lppid
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

./waitjobs.sh

#
# End of "$Id$".
#
