#
# Man page makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1993-2006 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


#
# Man pages...
#

MAN1	=	cancel.1 \
		cups.1 \
		cups-config.1 \
		cupstestppd.1 \
		ippeveprinter.1 \
		$(IPPFIND_MAN) \
		ipptool.1 \
		lp.1 \
		lpoptions.1 \
		lpq.1 \
		lprm.1 \
		lpr.1 \
		lpstat.1 \
		ppdc.1 \
		ppdhtml.1 \
		ppdi.1 \
		ppdmerge.1 \
		ppdpo.1
MAN5	=	classes.conf.5 \
		client.conf.5 \
		cups-files.conf.5 \
		cups-snmp.conf.5 \
		cupsd.conf.5 \
		cupsd-logs.5 \
		ipptoolfile.5 \
		mailto.conf.5 \
		mime.convs.5 \
		mime.types.5 \
		ppdcfile.5 \
		printers.conf.5 \
		subscriptions.conf.5
MAN7	=	backend.7 \
		filter.7 \
		ippevepcl.7 \
		notifier.7
MAN8	=	cupsaccept.8 \
		cupsctl.8 \
		cupsfilter.8 \
		cups-lpd.8 \
		cups-snmp.8 \
		cupsd.8 \
		cupsd-helper.8 \
		cupsenable.8 \
		lpadmin.8 \
		lpinfo.8 \
		lpmove.8 \
		lpc.8


#
# Make everything...
#

all:	$(MAN1) $(MAN5) $(MAN7) $(MAN8)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


#
# Clean all config and object files...
#

clean:
	$(RM) mantohtml mantohtml.o


#
# Dummy depend target...
#

depend:


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data: all
	echo Installing man pages in $(MANDIR)/man1...
	$(INSTALL_DIR) -m 755 $(MANDIR)/man1
	for file in $(MAN1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man1; \
	done
	echo Installing man pages in $(MANDIR)/man5...
	$(INSTALL_DIR) -m 755 $(MANDIR)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man5; \
	done
	echo Installing man pages in $(MANDIR)/man7...
	$(INSTALL_DIR) -m 755 $(MANDIR)/man7
	for file in $(MAN7); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man7; \
	done
	$(RM) $(MANDIR)/man7/ippeveps.7
	$(LN) ippevepcl.7 $(MANDIR)/man7/ippeveps.7
	echo Installing man pages in $(MANDIR)/man8...
	$(INSTALL_DIR) -m 755 $(MANDIR)/man8
	for file in $(MAN8); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man8; \
	done
	$(RM) $(MANDIR)/man8/cupsdisable.8
	$(LN) cupsenable.8 $(MANDIR)/man8/cupsdisable.8
	$(RM) $(MANDIR)/man8/cupsreject.8
	$(LN) cupsaccept.8 $(MANDIR)/man8/cupsreject.8
	for file in cups-deviced.8 cups-driverd.8 cups-exec.8; do \
		$(RM) $(MANDIR)/man8/$$file; \
		$(LN) cupsd-helper.8 $(MANDIR)/man8/$$file; \
	done


#
# Install programs...
#

install-exec:


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Uninstall files...
#

uninstall:
	echo Uninstalling man pages from $(MANDIR)/man1...
	for file in $(MAN1); do \
		$(RM) $(MANDIR)/man1/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man1
	echo Uninstalling man pages from $(MANDIR)/man5...
	for file in $(MAN5); do \
		$(RM) $(MANDIR)/man5/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man5
	echo Uninstalling man pages from $(MANDIR)/man7...
	for file in $(MAN7) ippeveps.7; do \
		$(RM) $(MANDIR)/man7/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man7
	echo Uninstalling man pages from $(MANDIR)/man8...
	for file in $(MAN8) cupsenable.8 cupsreject.8 cups-deviced.8 cups-driverd.8 cups-exec.8; do \
		$(RM) $(MANDIR)/man8/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man8


#
# Local programs (not built when cross-compiling...)
#

local:	html


#
# Make html versions of man pages...
#

html:	$(MAN1) $(MAN5) $(MAN7) $(MAN8) mantohtml
	echo Converting man pages to HTML...
	for file in $(MAN1); do \
		echo "    $$file..."; \
		./mantohtml $$file >../doc/help/man-`basename $$file .1`.html; \
	done
	for file in $(MAN5); do \
		echo "    $$file..."; \
		./mantohtml $$file >../doc/help/man-`basename $$file .5`.html; \
	done
	for file in $(MAN7); do \
		echo "    $$file..."; \
		./mantohtml $$file >../doc/help/man-`basename $$file .7`.html; \
	done
	for file in $(MAN8); do \
		echo "    $$file..."; \
		./mantohtml $$file >../doc/help/man-`basename $$file .8`.html; \
	done

mantohtml:	mantohtml.o ../cups/$(LIBCUPSSTATIC)
	$(LD_CC) $(ARCHFLAGS) $(ALL_LDFLAGS) -o $@ mantohtml.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
