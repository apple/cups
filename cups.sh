#!/bin/sh
#
# "$Id: cups.sh,v 1.5 1999/07/21 19:04:33 mike Exp $"
#
#   Startup/shutdown script for the Common UNIX Printing System (CUPS).
#
#   Linux chkconfig stuff:
#
#   chkconfig: 2345 60 60
#   description: Startup/shutdown script for the Common UNIX \
#                Printing System (CUPS).
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

# See what program to use for configuration stuff...
case "`uname`" in
	IRIX*)
		IS_ON=/sbin/chkconfig
		;;

	*)
		IS_ON=/bin/true
		;;
esac

# The verbose flag controls the printing of the names of
# daemons as they are started.
if $IS_ON verbose; then
	ECHO=echo
else
	ECHO=:
fi

# See if the CUPS server is running...
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

# Start or stop the CUPS server based upon the first argument to the script.
case $1 in
	start | restart | reload)
		if test "$pid" != ""; then
			if $IS_ON cups; then
				kill -HUP $pid
				$ECHO "cups: scheduler restarted."
			else
				kill $pid
				$ECHO "cups: scheduler stopped."
			fi
		else
			if $IS_ON cups; then
				/usr/sbin/cupsd 2>&1 >/dev/null &
				$ECHO "cups: scheduler started."
			fi
		fi
		;;

	stop)
		if test "$pid" != ""; then
			kill $pid
			$ECHO "cups: scheduler stopped."
		fi
		;;

	status)
		if test "$pid" != ""; then
			echo "cups: Scheduler is running."
		else
			echo "cups: Scheduler is not running."
		fi
		;;

	*)
		echo "Usage: cups {reload|restart|start|status|stop}"
		exit 1
		;;
esac

exit 0


#
# End of "$Id: cups.sh,v 1.5 1999/07/21 19:04:33 mike Exp $".
#
