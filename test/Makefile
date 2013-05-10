#
# "$Id$"
#
#   IPP test makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
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
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies ipptest.c >/dev/null 2>&1


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


#
# purify target for doing tests on the CUPS API...
#

purify:	ipptest.o ../cups/$(LIBCUPS)
	echo Linking $@...
	purify $(CC) $(LDFLAGS) -o ipptest.pure ipptest.o $(LIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
