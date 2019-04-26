#!/bin/sh
#
# Test the lpr command.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "LPR Default Test"
echo ""
echo "    lpr testfile.pdf"
$runcups $VALGRIND ../berkeley/lpr ../examples/testfile.pdf 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPR Destination Test"
echo ""
echo "    lpr -P Test3 -o fit-to-page testfile.jpg"
$runcups $VALGRIND ../berkeley/lpr -P Test3 -o fit-to-page ../examples/testfile.jpg 2>&1
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
$runcups $VALGRIND ../berkeley/lpr -P Test1 -o number-up=4 -o job-sheets=standard,none ../examples/testfile.pdf 2>&1
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
pids=""
while test $i -lt $1; do
	j=1
	while test $j -le $2; do
		$runcups $VALGRIND ../berkeley/lpr -P test-$j ../examples/testfile.jpg 2>&1
		j=`expr $j + 1`
	done

	$runcups $VALGRIND ../berkeley/lpr -P Test1 ../examples/testfile.jpg 2>&1 &
	pids="$pids $!"
	$runcups $VALGRIND ../berkeley/lpr -P Test2 ../examples/testfile.jpg 2>&1 &
	pids="$pids $!"

	i=`expr $i + 1`
done
wait $pids
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

./waitjobs.sh
