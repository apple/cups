#
# "$Id: Makefile,v 1.8 1999/07/08 14:20:23 mike Exp $"
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
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

TARGETS	=	ipp lpd parallel serial smb socket
OBJS	=	ipp.o lpd.o parallel.o serial.o socket.o

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
	-$(MKDIR) $(SERVERROOT)/backend
	$(CP) $(TARGETS) $(SERVERROOT)/backend
	$(LN) ipp $(SERVERROOT)/backend/http
	$(CHMOD) u+s $(SERVERROOT)/backend/lpd

#
# ipp
#

ipp:	ipp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ipp ipp.o $(LIBS)
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
# smb
#
# Note: reading through these commands is a good way to get a headache... :)
#

smb:	smb.sh ../Makedefs
	echo Generating $@...
	$(RM) smb
	sedcmd="1,\$$s/^SMBCLIENT=.\*/SMBCLIENT=`echo $(SMBCLIENT) | sed -e '1,$$s/\\//\\\\\\//g'`/" ;\
	$(SED) -e "$$sedcmd" <smb.sh >smb
	$(CHMOD) +x smb

#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o socket socket.o $(LIBS)

socket.o:	../cups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.8 1999/07/08 14:20:23 mike Exp $".
#
