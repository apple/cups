#
# "$Id$"
#
#   Backend makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs

BACKENDS =	ipp lpd parallel scsi serial socket usb
TARGETS	=	betest test1284 $(BACKENDS)
OBJS	=	betest.o ipp.o lpd.o parallel.o scsi.o \
		serial.o socket.o test1284.o usb.o


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
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install all targets...
#

install:	all
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


#
# test1284
#

test1284:	test1284.o ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o test1284 test1284.o ../cups/libcups.a


#
# ieee1394
#

ieee1394:	ieee1394.o ieee1394-linux.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ieee1394 ieee1394.o ieee1394-linux.o -lraw1394 $(LIBS)

ieee1394.o:		ieee1394.h
ieee1394-linux.o:	ieee1394.h


#
# ipp
#

ipp:	ipp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ipp ipp.o $(LIBS)
	$(RM) http
	$(LN) ipp http


#
# lpd
#

lpd:	lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpd lpd.o $(LIBS)


#
# parallel
#

parallel:	parallel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o parallel parallel.o $(LIBS)


#
# scsi
#

scsi:	scsi.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o scsi scsi.o $(LIBS)

scsi.o:	scsi.c scsi-irix.c scsi-linux.c


#
# serial
#

serial:	serial.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o serial serial.o $(BACKLIBS) $(LIBS)


#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o socket socket.o $(LIBS)


#
# usb
#

usb:	usb.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o usb usb.o $(BACKLIBS) $(LIBS)
usb.o:	usb.c usb-darwin.c usb-unix.c ieee1284.c


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
