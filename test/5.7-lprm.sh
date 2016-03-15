#!/bin/sh
#
# "$Id: 5.7-lprm.sh 11398 2013-11-06 20:11:11Z msweet $"
#
#   Test the lprm command.
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

echo "LPRM Current Test"
echo ""
echo "    lpr -o job-hold-until=indefinite testfile.jpg"
$VALGRIND ../berkeley/lpr -o job-hold-until=indefinite testfile.jpg 2>&1
echo "    lprm"
$VALGRIND ../berkeley/lprm 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "LPRM Destination Test"
echo ""
echo "    lpr -P Test1 -o job-hold-until=indefinite testfile.jpg"
$VALGRIND ../berkeley/lpr -P Test1 -o job-hold-until=indefinite testfile.jpg 2>&1
echo "    lprm Test1"
$VALGRIND ../berkeley/lprm Test1 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

#
# End of "$Id: 5.7-lprm.sh 11398 2013-11-06 20:11:11Z msweet $".
#
