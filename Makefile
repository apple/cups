#
# "$Id: Makefile,v 1.7 1999/04/28 15:51:32 mike Exp $"
#
#   Top-level Makefile for the Common UNIX Printing System (CUPS).
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

#
# Directorys to make...
#

DIRS	=	cups backend cgi-bin driver filter gui scheduler systemv

#
# Make all targets...
#

all:
	for dir in $(DIRS); do\
		(cd $$dir; make);\
	done

#
# Remove object and target files...
#

clean:
	for dir in $(DIRS); do\
		(cd $$dir; make clean);\
	done
#
# Install object and target files...
#

install:
	for dir in $(DIRS); do\
		(cd $$dir; make install);\
	done

#
# End of "$Id: Makefile,v 1.7 1999/04/28 15:51:32 mike Exp $".
#
