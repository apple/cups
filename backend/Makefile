#
# "$Id: Makefile,v 1.25.2.3 2002/01/02 18:04:17 mike Exp $"
#
#   Backend makefile for the Common UNIX Printing System (CUPS).
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
	$(RM) $(OBJS) $(TARGETS) http


#
# Install all targets...
#

install:
	$(INSTALL_DIR) $(SERVERBIN)/backend
	for file in $(BACKENDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/backend; \
	done
	$(RM) $(SERVERBIN)/backend/http
	$(LN) ipp $(SERVERBIN)/backend/http


#
# betest
#

betest:	betest.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o betest betest.o $(LIBS)

betest.o:	../cups/string.h


#
# ipp
#

ipp:	ipp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ipp ipp.o $(LIBS)
	$(RM) http
	$(LN) ipp http

ipp.o:	../cups/cups.h


#
# lpd
#

lpd:	lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpd lpd.o $(LIBS)

lpd.o:	../cups/cups.h


#
# parallel
#

parallel:	parallel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o parallel parallel.o $(LIBS)

parallel.o:	../cups/cups.h


#
# serial
#

serial:	serial.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o serial serial.o $(LIBS)

serial.o:	../cups/cups.h


#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o socket socket.o $(LIBS)

socket.o:	../cups/cups.h


#
# usb
#

usb:	usb.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o usb usb.o $(LIBS)

usb.o:	../cups/cups.h


$(OBJS):	../config.h ../Makedefs

#
# End of "$Id: Makefile,v 1.25.2.3 2002/01/02 18:04:17 mike Exp $".
#
