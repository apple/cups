#
# "$Id: Makefile,v 1.18 2000/02/29 14:47:27 mike Exp $"
#
#   Backend makefile for the Common UNIX Printing System (CUPS).
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

BACKENDS =	ipp lpd parallel serial socket usb
TARGETS	=	betest $(BACKENDS)
OBJS	=	betest.o ipp.o lpd.o parallel.o serial.o socket.o usb.o


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
	-$(MKDIR) $(SERVERBIN)/backend
	$(CP) $(BACKENDS) $(SERVERBIN)/backend
	$(RM) $(SERVERBIN)/backend/http
	$(LN) ipp $(SERVERBIN)/backend/http


#
# betest
#

betest:	betest.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o betest betest.o $(LIBS)

betest.o:	../cups/string.h ../Makedefs


#
# ipp
#

ipp:	ipp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ipp ipp.o $(LIBS)
	$(RM) http
	$(LN) ipp http

ipp.o:	../cups/cups.h ../Makedefs


#
# lpd
#

lpd:	lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpd lpd.o $(LIBS)

lpd.o:	../cups/cups.h ../Makedefs


#
# parallel
#

parallel:	parallel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o parallel parallel.o $(LIBS)

parallel.o:	../cups/cups.h ../Makedefs


#
# serial
#

serial:	serial.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o serial serial.o $(LIBS)

serial.o:	../cups/cups.h ../Makedefs


#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o socket socket.o $(LIBS)

socket.o:	../cups/cups.h ../Makedefs


#
# usb
#

usb:	usb.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o usb usb.o $(LIBS)

usb.o:	../cups/cups.h ../Makedefs


#
# End of "$Id: Makefile,v 1.18 2000/02/29 14:47:27 mike Exp $".
#
