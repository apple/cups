#
# "$Id: Makefile,v 1.12 2001/02/20 17:33:00 mike Exp $"
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

MAN1	=	backend.man filter.man lp.man lpoptions.man lpq.man lprm.man \
		lpr.man lpstat.man
MAN5	=	classes.conf.man cupsd.conf.man mime.convs.man mime.types.man \
		printers.conf.man
MAN8	=	accept.man cups-lpd.man cups-polld.man cupsd.man enable.man \
		lpadmin.man lpinfo.man lpmove.man lpc.man

CAT1	=	$(MAN1:.man=.$(CAT1EXT))
CAT5	=	$(MAN5:.man=.$(CAT1EXT))
CAT8	=	$(MAN8:.man=.$(CAT1EXT))


#
# Make everything...
#

all:	$(CAT1) $(CAT5) $(CAT8)


#
# Clean all config and object files...
#

clean:
	$(RM) $(CAT1) $(CAT5) $(CAT8)


#
# Install files...
#

install:
	-$(MKDIR) $(MANDIR)/man1
	-$(CHMOD) ugo+rx $(MANDIR)
	-$(CHMOD) ugo+rx $(MANDIR)/man1
	for file in $(MAN1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man1/`basename $$file man`1; \
	done
	$(RM) $(MANDIR)/man1/cancel.1
	$(LN) lp.1 $(MANDIR)/man1/cancel.1
	-$(MKDIR) $(MANDIR)/man5
	-$(CHMOD) ugo+rx $(MANDIR)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/man5/`basename $$file man`5; \
	done
	-$(MKDIR) $(AMANDIR)/man$(MAN8EXT)
	-$(CHMOD) ugo+rx $(AMANDIR)/man$(MAN8EXT)
	for file in $(MAN8); do \
		$(INSTALL_MAN) $$file $(AMANDIR)/man$(MAN8EXT)/`basename $$file man`$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(LN) accept.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(RM) $(AMANDIR)/man$(MAN8EXT)/disable.$(MAN8EXT)
	$(LN) enable.$(MAN8EXT) $(AMANDIR)/man$(MAN8EXT)/disable.$(MAN8EXT)
	-$(MKDIR) $(MANDIR)/cat1
	-$(CHMOD) ugo+rx $(MANDIR)/cat1
	$(INSTALL_MAN) $(CAT1) $(MANDIR)/cat1
	$(RM) $(MANDIR)/cat1/cancel.$(CAT1EXT)
	$(LN) lp.$(CAT1EXT) $(MANDIR)/cat1/cancel.$(CAT1EXT)
	-$(MKDIR) $(MANDIR)/cat5
	-$(CHMOD) ugo+rx $(MANDIR)/cat5
	$(INSTALL_MAN) $(CAT5) $(MANDIR)/cat5
	-$(MKDIR) $(AMANDIR)/cat$(MAN8EXT)
	-$(CHMOD) ugo+rx $(AMANDIR)/cat$(MAN8EXT)
	$(INSTALL_MAN) $(CAT8) $(AMANDIR)/cat$(MAN8EXT)
	$(RM) $(AMANDIR)/cat$(MAN8EXT)/reject.$(CAT8EXT)
	$(LN) accept.$(CAT1EXT) $(AMANDIR)/cat$(MAN8EXT)/reject.$(CAT8EXT)
	$(RM) $(AMANDIR)/cat$(MAN8EXT)/disable.$(CAT8EXT)
	$(LN) enable.$(CAT1EXT) $(AMANDIR)/cat$(MAN8EXT)/disable.$(CAT8EXT)


#
# End of "$Id: Makefile,v 1.12 2001/02/20 17:33:00 mike Exp $".
#
