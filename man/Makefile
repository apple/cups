#
# "$Id$"
#
#   Man page makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2005 by Easy Software Products.
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
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs


#
# Man pages...
#

MAN1	=	backend.$(MAN1EXT) \
		cups-config.$(MAN1EXT) \
		cupstestppd.$(MAN1EXT) \
		filter.$(MAN1EXT) \
		lp.$(MAN1EXT) \
		lpoptions.$(MAN1EXT) \
		lppasswd.$(MAN1EXT) \
		lpq.$(MAN1EXT) \
		lprm.$(MAN1EXT) \
		lpr.$(MAN1EXT) \
		lpstat.$(MAN1EXT)
MAN5	=	classes.conf.$(MAN5EXT) \
		cupsd.conf.$(MAN5EXT) \
		mime.convs.$(MAN5EXT) \
		mime.types.$(MAN5EXT) \
		printers.conf.$(MAN5EXT)
MAN8	=	accept.$(MAN8EXT) \
		cupsaddsmb.$(MAN8EXT) \
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

all:	$(MAN1) $(MAN5) $(MAN8)


#
# Clean all config and object files...
#

clean:
	$(RM) $(MAN1) $(MAN5) $(MAN8)


#
# Dummy depend target...
#

depend:


#
# Install files...
#

install:	all
	$(INSTALL_DIR) $(MANDIR)/man1
	for file in $(MAN1); do \
		echo Installing $$file in $(MANDIR)/man1...; \
		$(INSTALL_MAN) $$file $(MANDIR)/man1; \
	done
	$(RM) $(MANDIR)/man1/cancel.$(MAN1EXT)
	$(LN) lp.$(MAN1EXT) $(MANDIR)/man1/cancel.$(MAN1EXT)
	$(INSTALL_DIR) $(MANDIR)/man5
	for file in $(MAN5); do \
		echo Installing $$file in $(MANDIR)/man5...; \
		$(INSTALL_MAN) $$file $(MANDIR)/man5; \
	done
	$(INSTALL_DIR) $(AMANDIR)/man$(MAN8EXT)
	for file in $(MAN8); do \
		echo Installing $$file in $(AMANDIR)/man$(MAN8EXT)...; \
		$(INSTALL_MAN) $$file $(AMANDIR)/man$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(LN) accept.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8EXT)/cupsdisable.$(MAN8EXT)
	$(LN) cupsenable.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/cupsdisable.$(MAN8EXT)


#
# End of "$Id$".
#
