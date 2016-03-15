#!/bin/sh
#
# "$Id: waitjobs.sh 1253 2009-02-25 23:37:25Z msweet $"
#
# Script to wait for jobs to complete.
#
#   Copyright 2008-2009 by Apple Inc.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

#
# Get timeout from command-line
#

if test $# = 1; then
	timeout=$1
else
	timeout=360
fi

#
# Figure out the proper echo options...
#

if (echo "testing\c"; echo 1,2,3) | grep c >/dev/null; then
        ac_n=-n
        ac_c=
else
        ac_n=
        ac_c='\c'
fi

echo $ac_n "Waiting for jobs to complete...$ac_c"
oldjobs=0

while test $timeout -gt 0; do
	jobs=`../systemv/lpstat 2>/dev/null | wc -l | tr -d ' '`
	if test $jobs = 0; then
		break
	fi

	if test $jobs != $oldjobs; then
		echo $ac_n "$jobs...$ac_c"
		oldjobs=$jobs
	fi

	sleep 5
	timeout=`expr $timeout - 5`
done

echo ""

#
# End of "$Id: waitjobs.sh 1253 2009-02-25 23:37:25Z msweet $".
#
