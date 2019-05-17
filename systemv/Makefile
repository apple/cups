#
# System V commands makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs

TARGETS	=	cancel cupsaccept cupsctl cupstestppd \
		lp lpadmin lpinfo lpmove lpoptions lpstat
OBJS	=	cancel.o cupsaccept.o cupsctl.o \
		cupstestppd.o lp.o lpadmin.o lpinfo.o lpmove.o lpoptions.o \
		lpstat.o


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
	$(RM) cupsdisable cupsenable cupsreject


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
	$(INSTALL_BIN) cupsctl $(SBINDIR)
	$(INSTALL_BIN) lpadmin $(SBINDIR)
	$(INSTALL_BIN) lpinfo $(SBINDIR)
	$(INSTALL_BIN) lpmove $(SBINDIR)
	$(RM) $(SBINDIR)/cupsdisable
	$(LN) cupsaccept $(SBINDIR)/cupsdisable
	$(RM) $(SBINDIR)/cupsenable
	$(LN) cupsaccept $(SBINDIR)/cupsenable
	$(RM) $(SBINDIR)/cupsreject
	$(LN) cupsaccept $(SBINDIR)/cupsreject
	echo Installing System V user printing commands in $(BINDIR)
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_BIN) cancel $(BINDIR)
	$(INSTALL_BIN) cupstestppd $(BINDIR)
	$(INSTALL_BIN) lp $(BINDIR)
	$(INSTALL_BIN) lpoptions $(BINDIR)
	$(INSTALL_BIN) lpstat $(BINDIR)
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
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
	$(RM) $(BINDIR)/cupstestppd
	$(RM) $(BINDIR)/lp
	$(RM) $(BINDIR)/lpoptions
	$(RM) $(BINDIR)/lpstat
	-$(RMDIR) $(BINDIR)
	$(RM) $(SBINDIR)/cupsaccept
	$(RM) $(SBINDIR)/cupsaccept
	$(RM) $(SBINDIR)/cupsdisable
	$(RM) $(SBINDIR)/cupsenable
	$(RM) $(SBINDIR)/cupsreject
	$(RM) $(SBINDIR)/lpadmin
	$(RM) $(SBINDIR)/lpinfo
	$(RM) $(SBINDIR)/lpmove
	-$(RMDIR) $(SBINDIR)


#
# cancel
#

cancel:	cancel.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cancel cancel.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# cupsaccept
#

cupsaccept:	cupsaccept.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cupsaccept cupsaccept.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	for file in cupsenable cupsdisable cupsreject; do \
		$(RM) $$file; \
		$(LN) cupsaccept $$file; \
	done


#
# cupsctl
#

cupsctl:	cupsctl.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cupsctl cupsctl.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# cupstestppd
#

cupstestppd:	cupstestppd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ cupstestppd.o -lm $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

cupstestppd-static:	cupstestppd.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ cupstestppd.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# lp
#

lp:	lp.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lp lp.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# lpadmin
#

lpadmin:	lpadmin.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lpadmin lpadmin.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# lpinfo
#

lpinfo:	lpinfo.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lpinfo lpinfo.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# lpmove
#

lpmove:	lpmove.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lpmove lpmove.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# lpoptions
#

lpoptions:	lpoptions.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lpoptions lpoptions.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# lpstat
#

lpstat:	lpstat.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lpstat lpstat.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Dependencies...
#

include Dependencies
