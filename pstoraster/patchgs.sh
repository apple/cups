#!/bin/sh
#
# "$Id: patchgs.sh,v 1.1.2.1 2001/12/29 19:04:32 mike Exp $"
#
# Patch the GNU Ghostscript 6.52 source distribution with the CUPS driver.
#
# Usage: patchgs.sh /path/to/ghostscript
#

if test $# != 1; then
	echo "Usage: patchgs.sh /path/to/ghostscript"
	exit 1
fi

if ! test -d $1; then
	echo "Ghostscript directory $1 does not exist."
	exit 1
fi

if ! test -f $1/src/unixansi.mak; then
	echo "$1 doesn't seem to contain a current version of Ghostscript."
	exit 1
fi

current=`pwd`

cp pstoraster pstoraster.convs $1
cp cups.mak gdevcups.c $1/src

cd $1
patch -p1 <$current/espgs.patch

#
# End of "$Id: patchgs.sh,v 1.1.2.1 2001/12/29 19:04:32 mike Exp $".
#
