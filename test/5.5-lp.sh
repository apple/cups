#!/bin/sh
#
# "$Id: 5.5-lp.sh 12066 2014-07-30 18:30:44Z msweet $"
#
#   Test the lp command.
#
#   Copyright 2007-2014 by Apple Inc.
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
$VALGRIND ../systemv/lp testfile.pdf 2>&1
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
$VALGRIND ../systemv/lp -d Test3 -o fit-to-page testfile.jpg 2>&1
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
$VALGRIND ../systemv/lp -d Test1 -P 1-4 -o job-sheets=classified,classified testfile.pdf 2>&1
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
		$VALGRIND ../systemv/lp -d test-$j testfile.jpg 2>&1
		j=`expr $j + 1`
	done

	$VALGRIND ../systemv/lp -d Test1 testfile.jpg 2>&1 &
	pids="$pids $!"
	$VALGRIND ../systemv/lp -d Test2 testfile.jpg 2>&1 &
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
$VALGRIND ../systemv/lpstat -W completed -o | tee $BASE/lpstat-completed.txt
if test "`uniq -d $BASE/lpstat-completed.txt`" != ""; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""


#
# End of "$Id: 5.5-lp.sh 12066 2014-07-30 18:30:44Z msweet $".
#
