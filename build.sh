#!/bin/sh
#
# Common script for building the current stable and development (master)
# branches of CUPS.
#
# Copyright 2007-2016 by Apple Inc.
# Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
# These coded instructions, statements, and computer programs are the
# property of Apple Inc. and are protected by Federal copyright
# law.  Distribution and use rights are outlined in the file "LICENSE.txt"
# which should have been included with this file.  If this file is
# file is missing or damaged, see the license at "http://www.cups.org/".
#
# Usage:
#
#     build.sh [--if-changed] [--quiet] [--update] [target(s)]
#

# Copy any build/test environment variables...
BUILDOPTIONS=""
BUILDNOTIFY=""
BUILDSUBJECT="[`hostname`]"
if test -f buildtest.env; then
	. ./buildtest.env
fi

# Get the path to this script...
BASEDIR=`dirname $0`
if test $BASEDIR = .; then
	BASEDIR=`pwd`
fi

# Make sure the temporary directory is there...
if test ! -d $BASEDIR/temp; then
	mkdir -p $BASEDIR/temp
fi

# Setup common path stuff...
PATH=""
for dir in /usr/xpg4/bin /usr/ccs/bin /opt/SUNWspro/bin /opt/sfw/bin \
	/usr/sfw/bin /usr/bsd /usr/freeware/bin /opt/aCC/bin /usr/contrib/bin \
	/usr/local/bin $HOME/oss/bin; do
	if test -d $dir; then
		PATH="$PATH:$dir"
	fi
done

if test "x$PATH" = x; then
	PATH="$BASEDIR/temp/bin:/usr/bin:/bin"
else
	PATH="$BASEDIR/temp/bin$PATH:/usr/bin:/bin"
fi

export PATH

if test -x /usr/xpg4/bin/grep; then
	grep=/usr/xpg4/bin/grep
else
	grep=grep
fi

if test -x /usr/local/bin/gmake; then
	make=/usr/local/bin/gmake
else
	make=make
fi

# Parse command-line arguments:
ifchanged=no
quiet=no
update=no
targets=""

while test $# -gt 0; do
	case "$1" in
		--ifchanged | --if-changed) ifchanged=yes;;
		--quiet) quiet=yes;;
		--update | update) update=yes;;
		*) targets="$targets $1";;
	esac

	shift
done


# Start the build...
if test $quiet = yes; then
	if test $update = yes; then
		exec >/dev/null 2>&1
	else
		if test -f build.log; then
			mv build.log build.log.O
		fi

		exec >build.log 2>&1
	fi
fi

if test "x$targets" = x; then
	targets="all"
fi

echo "Starting build of '$targets' on `date`"

if test -f git.log; then
	# Show Subversion updates...
	echo git pull --recurse-submodules; git submodule update --recursive
	cat git.log
	git status | $grep modified: >git.log
	if test -s git.log; then
		echo ""
		echo ERROR: Local files have modifications:
		echo ""
		cat git.log
		echo ""
	fi
	rm -f git.log
fi

# Update and then build safely...
if test $update = yes; then
	cd $BASEDIR
	(git pull --recurse-submodules; git submodule update --recursive) 2>&1 >git.log

	# Need to exec since this script might change...
	options=""
	if test $quiet = yes; then
		options="--quiet $options"
	fi
	if test $ifchanged = yes; then
		options="--ifchanged $options"
	fi

	exec ./build.sh $options $targets
fi

# Look for changes to the dependent projects
changed=0

for dir in . tools stable development; do
	rev=`git rev-parse HEAD $dir`
	if test -f $dir/.buildrev; then
		oldrev=`cat $dir/.buildrev`
	else
		oldrev=""
	fi

	if test "x$rev" != "x$oldrev"; then
		changed=1
	fi

	echo $rev >$dir/.buildrev
done

if test $ifchanged = yes -a $changed = 0; then
	echo No changes to build.
	exit 0
fi

# Setup the compiler flags to point to all of the dependent libraries...
#CFLAGS="-I$BASEDIR/temp/include"
#CPPFLAGS="-I$BASEDIR/temp/include"
#CXXFLAGS="-I$BASEDIR/temp/include"
#DSOFLAGS="-L$BASEDIR/temp/lib"
#LDFLAGS="-L$BASEDIR/temp/lib"

CFLAGS="${CFLAGS:=}"
CPPFLAGS="${CPPFLAGS:=}"
CXXFLAGS="${CXXFLAGS:=}"
DSOFLAGS="${DSOFLAGS:=}"
LDFLAGS="${LDFLAGS:=}"

export CFLAGS
export CPPFLAGS
export CXXFLAGS
export DSOFLAGS
export LDFLAGS

case `uname` in
	Linux*)
		if test "`uname -m`" = x86_64; then
			# 64-bit Linux needs this, at least for now...
			X_LIBS="-L/usr/X11R6/lib64"
			export X_LIBS
		fi
		;;
	*BSD*)
		CFLAGS="$CFLAGS -I/usr/local/include"
		CPPFLAGS="$CPPFLAGS -I/usr/local/include"
		DSOFLAGS="$DSOFLAGS -L/usr/local/lib"
		LDFLAGS="$LDFLAGS -L/usr/local/lib"
		;;
esac

# Show the environment...
echo Environment:
env

# Do the build using the top-level makefile...
echo $make -f build.mak BASEDIR=$BASEDIR BUILDOPTIONS="$BUILDOPTIONS" $targets
$make -f build.mak BASEDIR=$BASEDIR BUILDOPTIONS="$BUILDOPTIONS" $targets </dev/null
status=$?

if test $status = 0; then
	echo Build succeeded on `date`
else
	echo Build failed on `date`
fi

# If configured, send the build log to a central server for processing...
if test "x$BUILDNOTIFY" != x -a -x temp/bin/sendbuildlog -a $quiet = yes; then
	attachments=""

	date=`date "+%Y-%m-%d"`

	if test -f stable/test/cups-str-2.1-$date-$USER.html; then
		attachments="$attachments -a stable/test/cups-str-2.1-$date-$USER.html"
	fi

	if test -f developent/test/cups-str-2.2-$date-$USER.html; then
		attachments="$attachments -a developent/test/cups-str-2.2-$date-$USER.html"
	fi

	temp/bin/sendbuildlog -b $status -s "$BUILDSUBJECT r$rev" $attachments "$BUILDNOTIFY" build.log
fi
