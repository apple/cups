#
# "$Id: Makefile,v 1.13 1999/06/21 18:49:39 mike Exp $"
#
#   Scheduler Makefile for the Common UNIX Printing System (CUPS).
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

include ../Makedefs

CUPSDOBJS =	auth.o classes.o client.o conf.o dirsvc.o main.o ipp.o \
		listen.o job.o printers.o
OBJS	=	$(CUPSDOBJS) testspeed.o

#
# Make everything...
#

all:	cupsd testspeed

#
# Clean all object files...
#

clean:
	rm -f $(OBJS) cupsd testspeed

#
# Install the scheduler...
#

install:
	-$(MKDIR) $(SBINDIR)
	$(CP) cupsd $(SBINDIR)
	-$(MKDIR) $(SERVERROOT)/interfaces
	-$(MKDIR) $(SERVERROOT)/logs
	-$(MKDIR) $(SERVERROOT)/ppd
	-$(MKDIR) $(SERVERROOT)/requests

#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(CUPSDOBJS) ../cups/libcups.a
	echo Linking $<...
	$(CC) $(LDFLAGS) -o cupsd $(CUPSDOBJS) $(LIBS)

$(CUPSDOBJS):	auth.h classes.h client.h conf.h cupsd.h dirsvc.h job.h \
		printers.h ../cups/cups.h ../cups/http.h ../cups/ipp.h \
		../cups/language.h ../cups/mime.h ../cups/string.h

#
# Make the test program, "testspeed".
#

testspeed:	testspeed.o ../cups/libcups.a
	echo Linking $<...
	$(CC) $(LDFLAGS) -o testspeed testspeed.o $(LIBS)

testspeed.o:	../cups/cups.h ../cups/http.h ../cups/ipp.h ../cups/language.h

$(OBJS):	../config.h ../Makedefs

#
# End of "$Id: Makefile,v 1.13 1999/06/21 18:49:39 mike Exp $".
#
