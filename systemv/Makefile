#
# "$Id: Makefile,v 1.2 1999/03/03 21:18:57 mike Exp $"
#
#   System V commands makefile for the Common UNIX Printing System (CUPS).
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

OPTIONS	=	-DDEBUG
TARGETS	=	cancel lp lpstat
OBJS	=	cancel.o lp.o lpstat.o

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
# cancel
#

cancel:	cancel.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o cancel cancel.o $(LIBS)

cancel.o:	../cups/cups.h ../Makedefs

#
# lp
#

lp:	lp.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o lp lp.o $(LIBS)

lp.o:	../cups/cups.h ../Makedefs

#
# lpstat
#

lpstat:	lpstat.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o lpstat lpstat.o $(LIBS)

lpstat.o:	../cups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.2 1999/03/03 21:18:57 mike Exp $".
#
