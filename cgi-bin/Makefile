#
# "$Id: Makefile,v 1.4 1999/06/21 18:45:22 mike Exp $"
#
#   CGI makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products.
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

TARGETS	=	classes.cgi jobs.cgi printers.cgi
OBJS	=	classes.o jobs.o printers.o

#
# Make all targets...
#

all:	$(TARGETS)

#
# Clean all object files...
#

clean:
	rm -f $(OBJS) $(TARGETS)

#
# Install all targets...
#

install:
	-$(MKDIR) $(SERVERROOT)/cgi-bin
	$(CP) $(TARGETS) $(SERVERROOT)/cgi-bin

#
# classes.cgi
#

classes.cgi:	classes.o ../Makedefs ../cups/libcups.a
	echo Linking $<...
	$(CC) $(LDFLAGS) -o $@ classes.o $(LIBS)

$(OBJS):	../Makedefs ../cups/cups.h ../cups/ipp.h ../cups/language.h

#
# jobs.cgi
#

jobs.cgi:	jobs.o ../Makedefs ../cups/libcups.a
	echo Linking $<...
	$(CC) $(LDFLAGS) -o $@ jobs.o $(LIBS)

$(OBJS):	../Makedefs ../cups/cups.h ../cups/ipp.h ../cups/language.h

#
# printers.cgi
#

printers.cgi:	printers.o ../Makedefs ../cups/libcups.a
	echo Linking $<...
	$(CC) $(LDFLAGS) -o $@ printers.o $(LIBS)

$(OBJS):	../Makedefs ../cups/cups.h ../cups/ipp.h ../cups/language.h

#
# End of "$Id: Makefile,v 1.4 1999/06/21 18:45:22 mike Exp $".
#
