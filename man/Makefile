#
# "$Id: Makefile,v 1.7 2000/02/29 14:47:29 mike Exp $"
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

CAT1	=	backend.1 filter.1 lp.1 lpq.1 lprm.1 lpr.1 lpstat.1
CAT5	=	classes.conf.5 cupsd.conf.5 mime.convs.5 mime.types.5 \
		printers.conf.5
CAT8	=	accept.8 cupsd.8 enable.8 lpadmin.8 lpc.8

MAN1	=	$(CAT1:.1=.man)
MAN5	=	$(CAT5:.5=.man)
MAN8	=	$(CAT8:.8=.man)


#
# Make everything...
#

all:	$(CAT1) $(CAT5) $(CAT8)


#
# Clean all config and object files...
#

clean:


#
# Install files...
#

install:
	-$(MKDIR) $(MANDIR)/man1
	for file in $(MAN1); do \
		$(CP) $$file $(MANDIR)/man1/`basename $$file man`1; \
	done
	$(RM) $(MANDIR)/man1/cancel.1
	$(LN) lp.1 $(MANDIR)/man1/cancel.1
	-$(MKDIR) $(MANDIR)/cat1
	$(CP) $(CAT1) $(MANDIR)/cat1
	$(RM) $(MANDIR)/cat1/cancel.1
	$(LN) lp.1 $(MANDIR)/cat1/cancel.1
	-$(MKDIR) $(MANDIR)/man5
	for file in $(MAN5); do \
		$(CP) $$file $(MANDIR)/man5/`basename $$file man`5; \
	done
	-$(MKDIR) $(MANDIR)/cat5
	$(CP) $(CAT5) $(MANDIR)/cat5
	-$(MKDIR) $(MANDIR)/man8
	for file in $(MAN8); do \
		$(CP) $$file $(MANDIR)/man8/`basename $$file man`8; \
	done
	$(RM) $(MANDIR)/man8/reject.8
	$(LN) accept.8 $(MANDIR)/man8/reject.8
	$(RM) $(MANDIR)/man8/disable.8
	$(LN) enable.8 $(MANDIR)/man8/disable.8
	-$(MKDIR) $(MANDIR)/cat8
	$(CP) $(CAT8) $(MANDIR)/cat8
	$(RM) $(MANDIR)/cat8/reject.8
	$(LN) accept.8 $(MANDIR)/cat8/reject.8
	$(RM) $(MANDIR)/cat8/disable.8
	$(LN) enable.8 $(MANDIR)/cat8/disable.8

#
# End of "$Id: Makefile,v 1.7 2000/02/29 14:47:29 mike Exp $".
#
