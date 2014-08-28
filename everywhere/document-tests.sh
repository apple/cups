#!/bin/sh
#
#  "$Id$"
#
# IPP Everywhere Printer Self-Certification Manual 1.0: Section 7: Document Data Tests.
#
# Copyright 2014 by The Printer Working Group.
#
# This program may be copied and furnished to others, and derivative works
# that comment on, or otherwise explain it or assist in its implementation may
# be prepared, copied, published and distributed, in whole or in part, without
# restriction of any kind, provided that the above copyright notice and this
# paragraph are included on all such copies and derivative works.
#
# The IEEE-ISTO and the Printer Working Group DISCLAIM ANY AND ALL WARRANTIES,
# WHETHER EXPRESS OR IMPLIED INCLUDING (WITHOUT LIMITATION) ANY IMPLIED
# WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
#
# Usage:
#
#   ./document-tests.sh "Printer Name"
#

if test -x ../test/ippfind-static; then
	IPPFIND="../test/ippfind-static"
elif test -x ./ippfind; then
	IPPFIND="./ippfind"
else
	IPPFIND="ippfind"
fi

if test -x ../test/ipptool-static; then
	IPPTOOL="../test/ipptool-static"
elif test -x ./ipptool; then
	IPPTOOL="./ipptool"
else
	IPPTOOL="ipptool"
fi

for file in color.jpg document-a4.pdf document-letter.pdf; do
	if test ! -f $file -a -f ../test/$file; then
		ln -s ../test/$file .
	fi
done

if test "`ls -d pwg-raster-samples-*dpi-20111130 2>/dev/null`" = ""; then
	echo "You must first download and extract the PWG Raster Format sample files from:"
	echo ""
	echo "    http://ftp.pwg.org/pub/pwg/ipp/examples/"
	echo ""
	echo "Before you can run this script."
	exit 1
fi

$IPPFIND "$1._ipp._tcp.local." -x $IPPTOOL -P "$1 Document Results.plist" -I '{}' document-tests.test \;

#
# End of "$Id$".
#
