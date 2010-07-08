#
# "$Id: Makefile 7930 2008-09-10 22:25:54Z mike $"
#
#   Man page makefile for CUPS.
#
#   Copyright 2007-2010 by Apple Inc.
#   Copyright 1993-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs


#
# Man pages...
#

MAN1	=	cancel.$(MAN1EXT) \
		cups-config.$(MAN1EXT) \
		cupstestdsc.$(MAN1EXT) \
		cupstestppd.$(MAN1EXT) \
		ipptool.$(MAN1EXT) \
		lp.$(MAN1EXT) \
		lpoptions.$(MAN1EXT) \
		lppasswd.$(MAN1EXT) \
		lpq.$(MAN1EXT) \
		lprm.$(MAN1EXT) \
		lpr.$(MAN1EXT) \
		lpstat.$(MAN1EXT) \
		ppdc.$(MAN1EXT) \
		ppdhtml.$(MAN1EXT) \
		ppdi.$(MAN1EXT) \
		ppdmerge.$(MAN1EXT) \
		ppdpo.$(MAN1EXT)
MAN5	=	classes.conf.$(MAN5EXT) \
		client.conf.$(MAN5EXT) \
		cups-snmp.conf.$(MAN5EXT) \
		cupsd.conf.$(MAN5EXT) \
		ipptoolfile.$(MAN5EXT) \
		mailto.conf.$(MAN5EXT) \
		mime.convs.$(MAN5EXT) \
		mime.types.$(MAN5EXT) \
		ppdcfile.$(MAN5EXT) \
		printers.conf.$(MAN5EXT) \
		subscriptions.conf.$(MAN5EXT)
MAN7	=	backend.$(MAN7EXT) \
		filter.$(MAN7EXT) \
		notifier.$(MAN7EXT)
MAN8	=	cupsaccept.$(MAN8EXT) \
		cupsaddsmb.$(MAN8EXT) \
		cupsctl.$(MAN8EXT) \
		cupsfilter.$(MAN8EXT) \
		cups-deviced.$(MAN8EXT) \
		cups-driverd.$(MAN8EXT) \
		cups-lpd.$(MAN8EXT) \
		cups-polld.$(MAN8EXT) \
		cupsd.$(MAN8EXT) \
		cupsenable.$(MAN8EXT) \
		lpadmin.$(MAN8EXT) \
		lpinfo.$(MAN8EXT) \
		lpmove.$(MAN8EXT) \
		lpc.$(MAN8EXT)


#
# Make everything...
#

all:	$(MAN1) $(MAN5) $(MAN7) $(MAN8) html


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
	$(RM) $(MAN1) $(MAN5) $(MAN7) $(MAN8)
	for file in $(MAN1); do \
		$(RM) ../doc/help/man-`basename $$file .$(MAN1EXT)`.html; \
	done
	for file in $(MAN5); do \
		$(RM) ../doc/help/man-`basename $$file .$(MAN5EXT)`.html; \
	done
	for file in $(MAN7); do \
		$(RM) ../doc/help/man-`basename $$file .$(MAN7EXT)`.html; \
	done
	for file in $(MAN8); do \
		$(RM) ../doc/help/man-`basename $$file .$(MAN8EXT)`.html; \
	done


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
	$(INSTALL_DIR) -m 755 $(MANDIR)/man1
	for file in $(MAN1); do \
		echo Installing $$file in $(MANDIR)/man1...; \
		$(INSTALL_MAN) $$file $(MANDIR)/man1; \
	done
	$(INSTALL_DIR) -m 755 $(MANDIR)/man5
	for file in $(MAN5); do \
		echo Installing $$file in $(MANDIR)/man5...; \
		$(INSTALL_MAN) $$file $(MANDIR)/man5; \
	done
	$(INSTALL_DIR) -m 755 $(MANDIR)/man7
	for file in $(MAN7); do \
		echo Installing $$file in $(MANDIR)/man7...; \
		$(INSTALL_MAN) $$file $(MANDIR)/man7; \
	done
	$(INSTALL_DIR) -m 755 $(AMANDIR)/man$(MAN8DIR)
	for file in $(MAN8); do \
		echo Installing $$file in $(AMANDIR)/man$(MAN8DIR)...; \
		$(INSTALL_MAN) $$file $(AMANDIR)/man$(MAN8DIR); \
	done
	for file in accept cupsreject reject; do \
		$(RM) $(AMANDIR)/man$(MAN8DIR)/$$file.$(MAN8EXT); \
		$(LN) cupsaccept.$(MAN8EXT) $(AMANDIR)/man$(MAN8DIR)/$$file.$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/man$(MAN8DIR)/cupsdisable.$(MAN8EXT)
	$(LN) cupsenable.$(MAN8EXT) $(AMANDIR)/man$(MAN8DIR)/cupsdisable.$(MAN8EXT)


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
	for file in $(MAN1); do \
		$(RM) $(MANDIR)/man1/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man1
	for file in $(MAN5); do \
		$(RM) $(MANDIR)/man5/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man5
	for file in $(MAN7); do \
		$(RM) $(MANDIR)/man7/$$file; \
	done
	-$(RMDIR) $(MANDIR)/man7
	for file in $(MAN8); do \
		$(RM) $(AMANDIR)/man$(MAN8DIR)/$$file; \
	done
	$(RM) $(AMANDIR)/man$(MAN8DIR)/accept.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8DIR)/cupsreject.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8DIR)/reject.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8DIR)/cupsdisable.$(MAN8EXT)
	-$(RMDIR) $(AMANDIR)/man$(MAN8DIR)


#
# Make html versions of man pages...
#

html:	$(MAN1) $(MAN5) $(MAN7) $(MAN8) mantohtml
	echo Converting man pages to HTML...
	for file in $(MAN1); do \
		echo "    $$file..."; \
		./mantohtml `basename $$file .$(MAN1EXT)`.man >../doc/help/man-`basename $$file .$(MAN1EXT)`.html; \
	done
	for file in $(MAN5); do \
		echo "    $$file..."; \
		./mantohtml `basename $$file .$(MAN5EXT)`.man >../doc/help/man-`basename $$file .$(MAN5EXT)`.html; \
	done
	for file in $(MAN7); do \
		echo "    $$file..."; \
		./mantohtml `basename $$file .$(MAN7EXT)`.man >../doc/help/man-`basename $$file .$(MAN7EXT)`.html; \
	done
	for file in $(MAN8); do \
		echo "    $$file..."; \
		./mantohtml `basename $$file .$(MAN8EXT)`.man >../doc/help/man-`basename $$file .$(MAN8EXT)`.html; \
	done

mantohtml:	mantohtml.o
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ mantohtml.o


#
# End of "$Id: Makefile 7930 2008-09-10 22:25:54Z mike $".
#
