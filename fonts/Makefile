#
# "$Id: Makefile,v 1.6 1999/05/10 17:42:00 mike Exp $"
#
#   Fonts makefile for the Common UNIX Printing System (CUPS).
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
# Font files...
#

FONTS	=	Courier Courier-Bold Courier-BoldOblique Courier-Oblique \
		Symbol


#
# Make everything...
#

all:


#
# Clean all config and object files...
#

clean:


#
# Install files...
#

install:
	$(INSTALL_DIR) $(DATADIR)/fonts
	for file in $(FONTS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/fonts; \
	done


#
# End of "$Id: Makefile,v 1.6 1999/05/10 17:42:00 mike Exp $".
#
