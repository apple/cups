#!/bin/sh
#
# "$Id: cups.sh,v 1.4 1999/06/24 18:36:27 mike Exp $"
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
#       44141 Airport View Drive, Suite 204
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

case $1 in
	start | restart)
	if test "$pid" != ""; then
		echo "Restarting CUPS scheduler..."
		kill -HUP $pid
	else
		echo "Starting CUPS scheduler..."
		/usr/sbin/cupsd 2>&1 >/dev/null &
	fi
	;;

	stop)
	if test "$pid" != ""; then
		echo "Stopping CUPS scheduler..."
		kill $pid
		sleep 1
	fi
	;;
esac

#
# End of "$Id: cups.sh,v 1.4 1999/06/24 18:36:27 mike Exp $".
#
