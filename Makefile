#
# "$Id: Makefile,v 1.9 1999/05/19 18:00:49 mike Exp $"
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

include Makedefs

#
# Directories to make...
#

DIRS	=	cups backend berkeley cgi-bin filter man pstoraster \
		scheduler systemv

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
	(cd conf; make install)
	(cd data; make install)
	(cd doc; make install)
	(cd fonts; make install)
	(cd ppd; make install)

#
# End of "$Id: Makefile,v 1.9 1999/05/19 18:00:49 mike Exp $".
#
