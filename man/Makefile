#
# "$Id: Makefile,v 1.2 1999/05/19 18:00:55 mike Exp $"
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

MAN1	=	backend.1 filter.1 lprm.1 lpr.1 lpstat.1 lp.1
MAN5	=	classes.conf.5 cupsd.conf.5 mime.convs.5 mime.types.5 \
		printers.conf.5
MAN8	=	accept.8 cupsd.8 enable.8 lpadmin.8 lpc.8

CAT1	=	$(MAN1:.1=.z)
CAT5	=	$(MAN5:.5=.z)
CAT8	=	$(MAN8:.8=.z)

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
	$(CP) $(MAN1) $(MANDIR)/man1
	$(LN) lp.1 $(MANDIR)/man1/cancel.1
	-$(MKDIR) $(MANDIR)/man5
	$(CP) $(MAN5) $(MANDIR)/man5
	-$(MKDIR) $(MANDIR)/man8
	$(CP) $(MAN8) $(MANDIR)/man8
	$(LN) accept.8 $(MANDIR)/man8/reject.8
	$(LN) enable.8 $(MANDIR)/man8/disable.8
	-$(MKDIR) $(MANDIR)/cat1
	$(CP) $(CAT1) $(MANDIR)/cat1
	$(LN) lp.z $(MANDIR)/cat1/cancel.z
	-$(MKDIR) $(MANDIR)/cat5
	$(CP) $(CAT5) $(MANDIR)/cat5
	-$(MKDIR) $(MANDIR)/cat8
	$(CP) $(CAT8) $(MANDIR)/cat8
	$(LN) accept.z $(MANDIR)/cat8/reject.z
	$(LN) enable.z $(MANDIR)/cat8/disable.z

#
# End of "$Id: Makefile,v 1.2 1999/05/19 18:00:55 mike Exp $".
#
