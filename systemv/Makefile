#
# "$Id: Makefile,v 1.23.2.8 2004/06/29 13:15:11 mike Exp $"
#
#   System V commands makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2004 by Easy Software Products, all rights reserved.
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
#       Hollywood, Maryland 20636-3142 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

TARGETS	=	accept cancel cupsaddsmb cupstestppd lp lpadmin lpinfo \
		lpmove lpoptions lppasswd lpstat
OBJS	=	accept.o cancel.o cupsaddsmb.o cupstestppd.o lp.o \
		lpadmin.o lpinfo.o lpmove.o lpoptions.o lppasswd.o \
		lpstat.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) disable enable reject


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install all targets...
#

install:	all
	$(INSTALL_DIR) $(SBINDIR)
	$(INSTALL_BIN) accept $(SBINDIR)
	$(RM) $(SBINDIR)/reject
	$(LN) accept $(SBINDIR)/reject
	$(INSTALL_BIN) cupsaddsmb $(SBINDIR)
	$(INSTALL_BIN) lpadmin $(SBINDIR)
	$(INSTALL_BIN) lpinfo $(SBINDIR)
	$(INSTALL_BIN) lpmove $(SBINDIR)
	$(INSTALL_DIR) $(BINDIR)
	$(INSTALL_BIN) cancel $(BINDIR)
	$(INSTALL_BIN) cupstestppd $(SBINDIR)
	$(RM) $(BINDIR)/disable
	$(LN) ../sbin/accept $(BINDIR)/disable
	$(RM) $(BINDIR)/enable
	$(LN) ../sbin/accept $(BINDIR)/enable
	$(INSTALL_BIN) lp $(BINDIR)
	$(INSTALL_BIN) lpoptions $(BINDIR)
	$(INSTALL_BIN) lpstat $(BINDIR)
	$(INSTALL_BIN) -m 4755 -o $(CUPS_USER) -g $(CUPS_GROUP) lppasswd $(BINDIR)


#
# accept
#

accept:	accept.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o accept accept.o $(LIBS)
	$(RM) reject enable disable
	$(LN) accept reject
	$(LN) accept enable
	$(LN) accept disable


#
# cancel
#

cancel:	cancel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cancel cancel.o $(LIBS)


#
# cupsaddsmb
#

cupsaddsmb:	cupsaddsmb.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsaddsmb cupsaddsmb.o $(LIBS)


#
# cupstestppd
#

cupstestppd:	cupstestppd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ cupstestppd.o $(LIBS)


#
# lp
#

lp:	lp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lp lp.o $(LIBS)


#
# lpadmin
#

lpadmin:	lpadmin.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpadmin lpadmin.o $(LIBZ) $(LIBS)


#
# lpinfo
#

lpinfo:	lpinfo.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpinfo lpinfo.o $(LIBS)


#
# lpmove
#

lpmove:	lpmove.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpmove lpmove.o $(LIBS)


#
# lpoptions
#

lpoptions:	lpoptions.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpoptions lpoptions.o $(LIBZ) $(LIBS)


#
# lppasswd
#

lppasswd:	lppasswd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lppasswd lppasswd.o $(LIBZ) $(LIBS)


#
# lpstat
#

lpstat:	lpstat.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpstat lpstat.o $(LIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile,v 1.23.2.8 2004/06/29 13:15:11 mike Exp $".
#
