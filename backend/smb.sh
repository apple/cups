#!/bin/sh
#
# "$Id: smb.sh,v 1.2 1999/09/08 21:12:09 mike Exp $"
#
#   SMB printing script for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-1999 by Easy Software Products, all rights reserved.
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

SMBCLIENT=/usr/local/samba/bin/smbclient

#
# Get the URI from the "$_" variable; we normally would use $0, however BASH
# (incorrectly) assigns the name of the script (/var/cups/backend/smb) to
# $0 instead of the passed argv[0] string.  However, "$_" gets clobbered by
# commands that are run (including "test" commands in "if" statements), so
# we have to set the URI before checking to see if we have enough arguments...
#

uri="$_"

#
# Usage:
#
#     printer job user title copies options [filename]
#

if [ $# -lt 5 -o $# -gt 6 ]; then
	# Too few or too many arguments
	echo 'Usage: smb job-id user title copies options [file]' >&2
	exit 1
fi

#
# If "filename" is not on the command-line, then we read the print
# data from stdin and write it to a temporary file.
#

if [ $# = 5 ]; then
	# Collect all print data and put it in a temporary file...
	if [ "$TMPDIR" = "" ]; then
		TMPDIR=/var/tmp
	fi

	filename="$TMPDIR/$$.smb"
	cat >$filename
else
	# Use the file on the command-line...
	filename="$6"
fi

#
# Take apart the URI...
#

host=`echo $uri | awk -F/ '{print substr($3, index($3, "@") + 1)}'`
user=`echo $uri | awk -F/ '{print substr($3, 0, index($3, "@") - 1)}'`
if [ "$user" != "" ]; then
	user="-U $user"
fi
printer=`echo $uri | awk -F/ '{print $4}'`

#
# Send the file to the remote system...
#

$SMBCLIENT //$host/$printer $user -P -N <<EOF
print $filename
EOF

#
# Lastly, remove the temporary file as needed...
#

if [ $# = 5 ]; then
	rm -f $filename
fi

#
# End of "$Id: smb.sh,v 1.2 1999/09/08 21:12:09 mike Exp $".
#
