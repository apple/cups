#
# "$Id: Makefile,v 1.12.2.1 2001/12/26 16:52:41 mike Exp $"
#
#   Man page makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-1999 by Easy Software Products.
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

#
# Man pages...
#

MAN1	=	backend.man filter.man lp.man lpoptions.man lppasswd.man \
		lpq.man lprm.man lpr.man lpstat.man
MAN3	=	cups-config.man
MAN5	=	classes.conf.man cupsd.conf.man mime.convs.man mime.types.man \
		printers.conf.man
MAN8	=	accept.man cupsaddsmb.man cups-lpd.man cups-polld.man \
		cupsd.man enable.man lpadmin.man lpinfo.man lpmove.man \
		lpc.man

CAT1	=	$(MAN1:.man=.$(CAT1EXT))
CAT3	=	$(MAN3:.man=.$(CAT3EXT))
CAT5	=	$(MAN5:.man=.$(CAT5EXT))
CAT8	=	$(MAN8:.man=.$(CAT8EXT))


#
# Make everything...
#

all:	$(CAT1) $(CAT3) $(CAT5) $(CAT8)


#
# Clean all config and object files...
#

clean:
	$(RM) $(CAT1) $(CAT3) $(CAT5) $(CAT8)


#
# Install files...
#

install:
	$(INSTALL_DIR) $(MANDIR)/man1
	for file in $(MAN1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man1/`basename $$file man`1; \
	done
	$(RM) $(MANDIR)/man1/cancel.1
	$(LN) lp.1 $(MANDIR)/man1/cancel.1
	$(INSTALL_DIR) $(PMANDIR)/man3
	for file in $(MAN3); do \
		$(INSTALL_MAN) $$file $(PMANDIR)/man3/`basename $$file man`3; \
	done
	$(INSTALL_DIR) $(MANDIR)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man5/`basename $$file man`5; \
	done
	$(INSTALL_DIR) $(AMANDIR)/man$(MAN8EXT)
	for file in $(MAN8); do \
		$(INSTALL_MAN) $$file $(AMANDIR)/man$(MAN8EXT)/`basename $$file man`$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(LN) accept.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8EXT)/disable.$(MAN8EXT)
	$(LN) enable.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/disable.$(MAN8EXT)
	$(INSTALL_DIR) $(MANDIR)/cat1
	for file in $(CAT1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/cat1; \
	done
	$(RM) $(MANDIR)/cat1/cancel.$(CAT1EXT)
	$(LN) lp.$(CAT1EXT) $(MANDIR)/cat1/cancel.$(CAT1EXT)
	$(INSTALL_DIR) $(PMANDIR)/cat3
	for file in $(CAT3); do \
		$(INSTALL_MAN) $$file $(PMANDIR)/cat3; \
	done
	$(INSTALL_DIR) $(MANDIR)/cat5
	for file in $(CAT5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/cat5; \
	done
	$(INSTALL_DIR) $(AMANDIR)/cat$(MAN8EXT)
	for file in $(CAT8); do \
		$(INSTALL_MAN) $$file $(AMANDIR)/cat$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/cat$(MAN8EXT)/reject.$(CAT8EXT)
	$(LN) accept.$(CAT8EXT) $(AMANDIR)/cat$(MAN8EXT)/reject.$(CAT8EXT)
	$(RM) $(AMANDIR)/cat$(MAN8EXT)/disable.$(CAT8EXT)
	$(LN) enable.$(CAT8EXT) $(AMANDIR)/cat$(MAN8EXT)/disable.$(CAT8EXT)


#
# End of "$Id: Makefile,v 1.12.2.1 2001/12/26 16:52:41 mike Exp $".
#
