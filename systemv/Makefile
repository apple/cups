#
# "$Id: Makefile,v 1.1 1999/02/26 15:11:34 mike Exp $"
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

TARGETS	=	lp
OBJS	=	lp.o

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
# lp
#

lp:	lp.o ../cups/libcups.a
	$(CC) $(LDFLAGS) -o lp lp.o $(LIBS)

lp.o:	../cups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.1 1999/02/26 15:11:34 mike Exp $".
#
