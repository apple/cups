#
# "$Id: Makefile,v 1.13.2.1 2001/12/26 16:52:07 mike Exp $"
#
#   Berkeley commands makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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

TARGETS	=	lpc lpq lpr lprm
OBJS	=	lpc.o lpq.o lpr.o lprm.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)


#
# Install all targets...
#

install:
	$(INSTALL_DIR) $(BINDIR)
	$(INSTALL_BIN) lpq $(BINDIR)
	$(INSTALL_BIN) lpr $(BINDIR)
	$(INSTALL_BIN) lprm $(BINDIR)
	$(INSTALL_DIR) $(SBINDIR)
	$(INSTALL_BIN) lpc $(SBINDIR)


#
# lpc
#

lpc:	lpc.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpc lpc.o $(LIBS)

lpc.o:	../cups/cups.h ../Makedefs


#
# lpq
#

lpq:	lpq.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpq lpq.o $(LIBS)

lpq.o:	../cups/cups.h ../Makedefs


#
# lpr
#

lpr:	lpr.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpr lpr.o $(LIBS)

lpr.o:	../cups/cups.h ../Makedefs


#
# lprm
#

lprm:	lprm.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lprm lprm.o $(LIBS)

lprm.o:	../cups/cups.h ../Makedefs


#
# End of "$Id: Makefile,v 1.13.2.1 2001/12/26 16:52:07 mike Exp $".
#
