#
# "$Id: Makefile,v 1.16 2000/02/18 17:48:08 mike Exp $"
#
#   System V commands makefile for the Common UNIX Printing System (CUPS).
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

TARGETS	=	accept cancel lp lpadmin lppasswd lpstat
OBJS	=	accept.o cancel.o lp.o lpadmin.o lppasswd.o lpstat.o


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
	$(CP) cancel lp lppasswd lpstat $(BINDIR)
	$(CHMOD) u+s $(BINDIR)/lppasswd
	$(RM) $(SBINDIR)/reject
	$(LN) accept $(SBINDIR)/reject
	$(RM) $(LIBDIR)/lpadmin
	$(LN) ../sbin/lpadmin $(LIBDIR)
	$(RM) $(LIBDIR)/accept
	$(LN) ../sbin/accept $(LIBDIR)
	$(RM) $(LIBDIR)/reject
	$(LN) ../sbin/accept $(LIBDIR)/reject
	$(RM) $(BINDIR)/disable
	$(LN) ../sbin/accept $(BINDIR)/disable
	$(RM) $(BINDIR)/enable
	$(LN) ../sbin/accept $(BINDIR)/enable


#
# accept
#

accept:	accept.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o accept accept.o $(LIBS)

accept.o:	../cups/cups.h


#
# cancel
#

cancel:	cancel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cancel cancel.o $(LIBS)

cancel.o:	../cups/cups.h


#
# lp
#

lp:	lp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lp lp.o $(LIBS)

lp.o:	../cups/cups.h


#
# lpadmin
#

lpadmin:	lpadmin.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpadmin lpadmin.o $(LIBZ) $(LIBS)

lpadmin.o:	../cups/cups.h


#
# lppasswd
#

lppasswd:	lppasswd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lppasswd lppasswd.o $(LIBZ) $(LIBS)

lppasswd.o:	../cups/cups.h ../cups/md5.h ../cups/string.h ../config.h


#
# lpstat
#

lpstat:	lpstat.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpstat lpstat.o $(LIBS)

lpstat.o:	../cups/cups.h


$(OBJS):	../Makedefs

#
# End of "$Id: Makefile,v 1.16 2000/02/18 17:48:08 mike Exp $".
#
