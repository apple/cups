#
# "$Id: Makefile,v 1.1 2000/05/19 21:14:00 mike Exp $"
#
#   IPP test makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2000 by Easy Software Products, all rights reserved.
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

include ../Makedefs


#
# Make all targets...
#

all:	ipptest


#
# Clean all object files...
#

clean:
	$(RM) ipptest ipptest.o


#
# Install all targets...
#

install:


#
# ipptest
#

ipptest:	ipptest.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ipptest ipptest.o $(LIBS)

ipptest.o:	../cups/cups.h ../Makedefs


#
# End of "$Id: Makefile,v 1.1 2000/05/19 21:14:00 mike Exp $".
#
