#
# "$Id: Makefile,v 1.1.2.1 2002/05/21 20:05:49 mike Exp $"
#
#   French man page makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2002 by Easy Software Products.
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

include ../../Makedefs
include ../Makefile.common


#
# This is the French manpage makefile...
#

LANGUAGE =	fr


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
# Dummy depend target...
#

depend:


#
# Install files...
#

install:	all
	$(INSTALL_DIR) $(MANDIR)/$(LANGUAGE)/man1
	for file in $(MAN1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/$(LANGUAGE)/man1/`basename $$file man`1; \
	done
	$(RM) $(MANDIR)/$(LANGUAGE)/man1/cancel.1
	$(LN) lp.1 $(MANDIR)/$(LANGUAGE)/man1/cancel.1
	$(INSTALL_DIR) $(PMANDIR)/fr/man3
	for file in $(MAN3); do \
		$(INSTALL_MAN) $$file $(PMANDIR)/fr/man3/`basename $$file man`3; \
	done
	$(INSTALL_DIR) $(MANDIR)/$(LANGUAGE)/man5
	for file in $(MAN5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/$(LANGUAGE)/man5/`basename $$file man`5; \
	done
	$(INSTALL_DIR) $(AMANDIR)/$(LANGUAGE)/man$(MAN8EXT)
	for file in $(MAN8); do \
		$(INSTALL_MAN) $$file $(AMANDIR)/$(LANGUAGE)/man$(MAN8EXT)/`basename $$file man`$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/$(LANGUAGE)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(LN) accept.$(MAN8EXT) $(AMANDIR)/$(LANGUAGE)/man$(MAN8EXT)/reject.$(MAN8EXT)
	$(RM) $(AMANDIR)/$(LANGUAGE)/man$(MAN8EXT)/disable.$(MAN8EXT)
	$(LN) enable.$(MAN8EXT) $(AMANDIR)/$(LANGUAGE)/man$(MAN8EXT)/disable.$(MAN8EXT)
	$(INSTALL_DIR) $(MANDIR)/$(LANGUAGE)/cat1
	for file in $(CAT1); do \
		$(INSTALL_MAN) $$file $(MANDIR)/$(LANGUAGE)/cat1; \
	done
	$(RM) $(MANDIR)/$(LANGUAGE)/cat1/cancel.$(CAT1EXT)
	$(LN) lp.$(CAT1EXT) $(MANDIR)/$(LANGUAGE)/cat1/cancel.$(CAT1EXT)
	$(INSTALL_DIR) $(PMANDIR)/fr/cat3
	for file in $(CAT3); do \
		$(INSTALL_MAN) $$file $(PMANDIR)/fr/cat3; \
	done
	$(INSTALL_DIR) $(MANDIR)/$(LANGUAGE)/cat5
	for file in $(CAT5); do \
		$(INSTALL_MAN) $$file $(MANDIR)/$(LANGUAGE)/cat5; \
	done
	$(INSTALL_DIR) $(AMANDIR)/$(LANGUAGE)/cat$(MAN8EXT)
	for file in $(CAT8); do \
		$(INSTALL_MAN) $$file $(AMANDIR)/$(LANGUAGE)/cat$(MAN8EXT); \
	done
	$(RM) $(AMANDIR)/$(LANGUAGE)/cat$(MAN8EXT)/reject.$(CAT8EXT)
	$(LN) accept.$(CAT8EXT) $(AMANDIR)/$(LANGUAGE)/cat$(MAN8EXT)/reject.$(CAT8EXT)
	$(RM) $(AMANDIR)/$(LANGUAGE)/cat$(MAN8EXT)/disable.$(CAT8EXT)
	$(LN) enable.$(CAT8EXT) $(AMANDIR)/$(LANGUAGE)/cat$(MAN8EXT)/disable.$(CAT8EXT)
	for dir in $(LANGDIRS); do \
		(cd $$dir; make install); \
	done


#
# End of "$Id: Makefile,v 1.1.2.1 2002/05/21 20:05:49 mike Exp $".
#
