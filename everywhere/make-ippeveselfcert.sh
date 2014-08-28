#!/bin/sh
#
# "$Id$"
#
#   Make an IPP Everywhere Printer self-certification package.
#
#   Copyright 2014 The Printer Working Group.
#   Copyright 2007-2013 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

# Make sure we are running in the right directory...
if test ! -f everywhere/make-ippeveselfcert.sh; then
        echo "Run this script from the top-level CUPS source directory, e.g.:"
        echo ""
        echo "    everywhere/make-ippeveselfcert.sh $*"
        echo ""
        exit 1
fi

if test $# != 2; then
	echo "Usage: everywhere/make-ippeveselfcert.sh platform YYYYMMDD"
	exit 1
fi

platform="$1"
fileversion="$2"

echo Creating package directory...
pkgdir="sw-ippeveselfcert10-$fileversion"

test -d $pkgdir && rm -r $pkgdir
mkdir $pkgdir || exit 1

echo Copying package files
cp LICENSE.txt $pkgdir
cp doc/help/man-ipp*.html $pkgdir
cp everywhere/README.txt $pkgdir
cp everywhere/man-ippserver.html $pkgdir
cp everywhere/*-tests.* $pkgdir
cp test/color.jpg $pkgdir
cp test/document-*.pdf $pkgdir
cp test/ippfind-static $pkgdir/ippfind
cp test/ippserver $pkgdir
cp test/ipptool-static $pkgdir/ipptool
cp test/printer.png $pkgdir

if test x$platform = xosx; then
	pkgfile="$pkgdir-osx.dmg"
	echo Creating disk image $pkgfile...
	test -f $pkgfile && rm $pkgfile
	hdiutil create -srcfolder $pkgdir $pkgfile
else
	pkgfile="$pkgdir-$platform.tar.gz"
	echo Creating archive $pkgfile...
	tar czf $pkgfile $pkgdir || exit 1
fi

echo Removing temporary files...
rm -r $pkgdir

echo Done.

#
# End of "$Id$".
#
