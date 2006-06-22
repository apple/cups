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

RBACKENDS =	ipp lpd
UBACKENDS =	$(PAP) parallel scsi serial snmp socket usb
TARGETS	=	betest test1284 libbackend.a $(RBACKENDS) $(UBACKENDS)
LIBOBJS	=	ieee1284.o runloop.o
OBJS	=	betest.o ipp.o lpd.o pap.o parallel.o scsi.o \
		serial.o snmp.o socket.o test1284.o usb.o


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
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/backend
	for file in $(RBACKENDS); do \
		$(LIBTOOL) $(INSTALL) -m 700 $$file $(SERVERBIN)/backend; \
	done
	for file in $(UBACKENDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/backend; \
	done
	$(RM) $(SERVERBIN)/backend/http
	$(LN) ipp $(SERVERBIN)/backend/http


#
# Uninstall all targets...
#

uninstall:
	for file in $(RBACKENDS) $(UBACKENDS); do \
		$(RM) $(SERVERBIN)/backend/$$file; \
	done
	$(RM) $(SERVERBIN)/backend/http
	-$(RMDIR) $(SERVERBIN)/backend
	-$(RMDIR) $(SERVERBIN)


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
	$(CC) $(LDFLAGS) -o test1284 test1284.o ../cups/libcups.a \
		$(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# libbackend.a
#

libbackend.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


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
# pap
#

pap:	pap.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o pap pap.o $(BACKLIBS) $(LIBS) -framework AppleTalk


#
# parallel
#

parallel:	parallel.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o parallel parallel.o libbackend.a $(LIBS)


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

serial:	serial.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o serial serial.o libbackend.a $(BACKLIBS) $(LIBS)


#
# snmp
#

snmp:	snmp.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o snmp snmp.o libbackend.a $(LIBS)


#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o socket socket.o libbackend.a $(LIBS)


#
# usb
#

usb:	usb.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o usb usb.o libbackend.a $(BACKLIBS) $(LIBS)
usb.o:	usb.c usb-darwin.c usb-unix.c


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
