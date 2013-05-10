#!/bin/sh
#
# "$Id$"
#
#   Test the lpr command.
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

echo "LPR Default Test"
echo ""
echo "    lpr testfile.jpg"
../berkeley/lpr testfile.jpg 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPR Destination Test"
echo ""
echo "    lpr -P Test1 testfile.jpg"
../berkeley/lpr -P Test1 testfile.jpg 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPR Flood Test"
echo ""
echo "    lpr -P Test1 testfile.jpg ($1 times in parallel)"
i=0
while test $i -lt $1; do
	echo "    flood copy $i..." 1>&2
	../berkeley/lpr -P Test1 testfile.jpg 2>&1 &
	lprpid=$!
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

#
# End of "$Id$".
#
