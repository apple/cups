#!/bin/sh
#
# Test the lp command.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2005 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

echo "LP Default Test"
echo ""
echo "    lp testfile.pdf"
$runcups $VALGRIND ../systemv/lp ../examples/testfile.pdf 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LP Destination Test"
echo ""
echo "    lp -d Test3 -o fit-to-page testfile.jpg"
$runcups $VALGRIND ../systemv/lp -d Test3 -o fit-to-page ../examples/testfile.jpg 2>&1
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
$runcups $VALGRIND ../systemv/lp -d Test1 -P 1-4 -o job-sheets=classified,classified ../examples/testfile.pdf 2>&1
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
pids=""
while test $i -lt $1; do
	j=1
	while test $j -le $2; do
		$runcups $VALGRIND ../systemv/lp -d test-$j ../examples/testfile.jpg 2>&1
		j=`expr $j + 1`
	done

	$runcups $VALGRIND ../systemv/lp -d Test1 ../examples/testfile.jpg 2>&1 &
	pids="$pids $!"
	$runcups $VALGRIND ../systemv/lp -d Test2 ../examples/testfile.jpg 2>&1 &
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

echo "LPSTAT Completed Jobs Order Test"
echo ""
echo "    lpstat -W completed -o"
$runcups $VALGRIND ../systemv/lpstat -W completed -o | tee $BASE/lpstat-completed.txt
if test "`uniq -d $BASE/lpstat-completed.txt`" != ""; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""
