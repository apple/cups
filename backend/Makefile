#
# "$Id: Makefile,v 1.1 1999/03/06 20:26:05 mike Exp $"
#
#   Backend makefile for the Common UNIX Printing System (CUPS).
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

include ../Makedefs

TARGETS	=	lpd parallel serial smb socket
OBJS	=	lpd.o parallel.o serial.o smb.o socket.o

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

#
# lpd
#

lpd:	lpd.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o lpd lpd.o $(LIBS)

lpd.o:	../cups/cups.h ../Makedefs

#
# parallel
#

parallel:	parallel.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o parallel parallel.o $(LIBS)

parallel.o:	../cups/cups.h ../Makedefs

#
# serial
#

serial:	serial.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o serial serial.o $(LIBS)

serial.o:	../cups/cups.h ../Makedefs

#
# smb
#

smb:	smb.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o smb smb.o $(LIBS)

smb.o:	../cups/cups.h ../Makedefs

#
# socket
#

socket:	socket.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o socket socket.o $(LIBS)

socket.o:	../cups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.1 1999/03/06 20:26:05 mike Exp $".
#
