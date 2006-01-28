#!/bin/sh
# This is a dummy script to ensure that the CUPS domain socket is world-
# writable.  This works around a problem in launchd...

if test -e /private/var/run/cupsd; then
	chmod g+w,o+w /private/var/run/cupsd
fi
