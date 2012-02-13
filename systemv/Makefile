#
# "$Id: Makefile 7929 2008-09-10 22:23:59Z mike $"
#
#   System V commands makefile for CUPS.
#
#   Copyright 2007-2012 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

TARGETS	=	cancel cupsaccept cupsaddsmb cupsctl cupstestdsc cupstestppd \
		lp lpadmin lpinfo lpmove lpoptions lppasswd lpstat
OBJS	=	cancel.o cupsaccept.o cupsaddsmb.o cupsctl.o cupstestdsc.o \
		cupstestppd.o lp.o lpadmin.o lpinfo.o lpmove.o lpoptions.o \
		lppasswd.o lpstat.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)
	$(RM) accept cupsdisable cupsenable cupsreject reject


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:
	echo Installing System V admin printing commands in $(SBINDIR)
	$(INSTALL_DIR) -m 755 $(SBINDIR)
	$(INSTALL_BIN) cupsaccept $(SBINDIR)
	$(INSTALL_BIN) cupsaddsmb $(SBINDIR)
	$(INSTALL_BIN) cupsctl $(SBINDIR)
	$(INSTALL_BIN) lpadmin $(SBINDIR)
	$(INSTALL_BIN) lpinfo $(SBINDIR)
	$(INSTALL_BIN) lpmove $(SBINDIR)
	$(RM) $(SBINDIR)/accept
	$(LN) cupsaccept $(SBINDIR)/accept
	$(RM) $(SBINDIR)/cupsdisable
	$(LN) cupsaccept $(SBINDIR)/cupsdisable
	$(RM) $(SBINDIR)/cupsenable
	$(LN) cupsaccept $(SBINDIR)/cupsenable
	$(RM) $(SBINDIR)/cupsreject
	$(LN) cupsaccept $(SBINDIR)/cupsreject
	$(RM) $(SBINDIR)/reject
	$(LN) cupsaccept $(SBINDIR)/reject
	echo Installing System V user printing commands in $(BINDIR)
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_BIN) cancel $(BINDIR)
	$(INSTALL_BIN) cupstestdsc $(BINDIR)
	$(INSTALL_BIN) cupstestppd $(BINDIR)
	$(INSTALL_BIN) lp $(BINDIR)
	$(INSTALL_BIN) lpoptions $(BINDIR)
	$(INSTALL_BIN) lpstat $(BINDIR)
	$(INSTALL_BIN) lppasswd $(BINDIR)
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
		done \
	fi


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Uninstall all targets...
#

uninstall:
	$(RM) $(BINDIR)/cancel
	$(RM) $(BINDIR)/cupstestdsc
	$(RM) $(BINDIR)/cupstestppd
	$(RM) $(BINDIR)/lp
	$(RM) $(BINDIR)/lpoptions
	$(RM) $(BINDIR)/lppasswd
	$(RM) $(BINDIR)/lpstat
	-$(RMDIR) $(BINDIR)
	$(RM) $(SBINDIR)/accept
	$(RM) $(SBINDIR)/cupsaccept
	$(RM) $(SBINDIR)/cupsaddsmb
	$(RM) $(SBINDIR)/cupsaccept
	$(RM) $(SBINDIR)/cupsdisable
	$(RM) $(SBINDIR)/cupsenable
	$(RM) $(SBINDIR)/cupsreject
	$(RM) $(SBINDIR)/lpadmin
	$(RM) $(SBINDIR)/lpinfo
	$(RM) $(SBINDIR)/lpmove
	$(RM) $(SBINDIR)/reject
	-$(RMDIR) $(SBINDIR)


#
# cancel
#

cancel:	cancel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cancel cancel.o $(LIBS)


#
# cupsaccept
#

cupsaccept:	cupsaccept.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsaccept cupsaccept.o $(LIBS)
	for file in accept cupsenable cupsdisable cupsreject reject; do \
		$(RM) $$file; \
		$(LN) cupsaccept $$file; \
	done


#
# cupsaddsmb
#

cupsaddsmb:	cupsaddsmb.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsaddsmb cupsaddsmb.o $(LIBS)


#
# cupsctl
#

cupsctl:	cupsctl.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsctl cupsctl.o $(LIBS)


#
# cupstestdsc
#

cupstestdsc:	cupstestdsc.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ cupstestdsc.o $(LIBS)


#
# cupstestppd
#

cupstestppd:	cupstestppd.o ../cups/$(LIBCUPS) ../filter/$(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ cupstestppd.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)

cupstestppd-static:	cupstestppd.o ../cups/$(LIBCUPSSTATIC) ../filter/libcupsimage.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ cupstestppd.o ../filter/libcupsimage.a \
		../cups/$(LIBCUPSSTATIC) $(IMGLIBS) $(LIBGSSAPI) $(LIBS) $(LIBZ)


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
# End of "$Id: Makefile 7929 2008-09-10 22:23:59Z mike $".
#
