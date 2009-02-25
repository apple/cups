#!/bin/sh
#
# "$Id: 5.6-lpr.sh 7409 2008-03-29 00:26:03Z mike $"
#
#   Test the lpr command.
#
#   Copyright 2007-2009 by Apple Inc.
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
echo "    lpr testfile.pdf"
../berkeley/lpr testfile.pdf 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPR Destination Test"
echo ""
echo "    lpr -P Test2 testfile.jpg"
../berkeley/lpr -P Test2 testfile.jpg 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPR Options Test"
echo ""
echo "    lpr -P Test1 -o number-up=4 -o job-sheets=standard,none testfile.pdf"
../berkeley/lpr -P Test1 -o number-up=4 -o job-sheets=standard,none testfile.pdf 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPR Flood Test ($1 times in parallel)"
echo ""
echo "    lpr -P Test1 testfile.jpg"
echo "    lpr -P Test2 testfile.jpg"
i=0
while test $i -lt $1; do
	echo "    flood copy $i..." 1>&2

	j=1
	while test $j -le $2; do
		../berkeley/lpr -P test-$j testfile.jpg 2>&1
		j=`expr $j + 1`
	done

	../berkeley/lpr -P Test1 testfile.jpg 2>&1 &
	../berkeley/lpr -P Test2 testfile.jpg 2>&1 &
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

./waitjobs.sh

#
# End of "$Id: 5.6-lpr.sh 7409 2008-03-29 00:26:03Z mike $".
#
