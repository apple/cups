#!/bin/sh
#
# "$Id: cups.sh,v 1.1 1999/05/20 16:16:47 mike Exp $"
#
#   Startup/shutdown script for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44145 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

# See if the CUPS daemon is running, and if so stop it...
case "`uname`" in
	IRIX* | HP-UX | SunOS)
		pid=`ps -e | awk '{print $1,$4}' | grep cupsd | awk '{print $1}'`
		;;
	OSF1)
		pid=`ps -e | awk '{print $1,$5}' | grep cupsd | awk '{print $1}'`
		;;
	Linux)
		pid=`ps ax | awk '{print $1,$5}' | grep cupsd | awk '{print $1}'`
		;;
	*)
		pid=""
		;;
esac

if test "$pid" != ""; then
	kill $pid
	sleep 1
fi
	
# Restart the CUPS daemon as necessary...
if test "$1" = "start"; then
	/usr/sbin/cupsd 2>&1 >/dev/null &
fi

#
# End of "$Id: cups.sh,v 1.1 1999/05/20 16:16:47 mike Exp $".
#
