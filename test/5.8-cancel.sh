#!/bin/sh
#
# "$Id: 5.8-cancel.sh,v 1.2.2.1 2002/01/11 18:06:30 mike Exp $"
#
#   Test the cancel command.
#
#   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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

echo "Cancel Destination Test"
echo ""
echo "    cancel Test1"
../systemv/cancel Test1 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

echo "Cancel All Test"
echo ""
echo "    cancel -a -"
../systemv/cancel -a - 2>&1
if test $? != 0; then
	echo "    FAILED"
	exit 1
else
	echo "    PASSED"
fi
echo ""

#
# End of "$Id: 5.8-cancel.sh,v 1.2.2.1 2002/01/11 18:06:30 mike Exp $".
#
