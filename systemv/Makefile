#
# "$Id: Makefile,v 1.13 1999/08/12 13:11:02 mike Exp $"
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
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

TARGETS	=	accept cancel lp lpadmin lpstat
OBJS	=	accept.o cancel.o lp.o lpadmin.o lpstat.o

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
	-$(MKDIR) $(BINDIR)
	-$(MKDIR) $(LIBDIR)
	$(CP) accept lpadmin $(SBINDIR)
	-$(LN) accept $(SBINDIR)/reject
	-$(LN) $(SBINDIR)/lpadmin $(LIBDIR)
	-$(LN) $(SBINDIR)/accept $(LIBDIR)
	-$(LN) $(SBINDIR)/accept $(LIBDIR)/reject
	-$(LN) $(SBINDIR)/accept $(BINDIR)/disable
	-$(LN) $(SBINDIR)/accept $(BINDIR)/enable
	$(CP) cancel lp lpstat $(BINDIR)

#
# accept
#

accept:	accept.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o accept accept.o $(LIBS)
	-$(LN) accept disable
	-$(LN) accept enable
	-$(LN) accept reject

accept.o:	../cups/cups.h ../Makedefs

#
# cancel
#

cancel:	cancel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cancel cancel.o $(LIBS)

cancel.o:	../cups/cups.h ../Makedefs

#
# lp
#

lp:	lp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lp lp.o $(LIBS)

lp.o:	../cups/cups.h ../Makedefs

#
# lpadmin
#

lpadmin:	lpadmin.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpadmin lpadmin.o $(LIBZ) $(LIBS)

lpadmin.o:	../cups/cups.h ../Makedefs

#
# lpstat
#

lpstat:	lpstat.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpstat lpstat.o $(LIBS)

lpstat.o:	../cups/cups.h ../Makedefs

#
# End of "$Id: Makefile,v 1.13 1999/08/12 13:11:02 mike Exp $".
#
