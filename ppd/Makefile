#
# "$Id: Makefile,v 1.8.2.4 2003/01/07 18:27:13 mike Exp $"
#
#   PPD file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2003 by Easy Software Products.
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
# PPD files...
#

FILES	=	deskjet.ppd deskjet2.ppd dymo.ppd epson9.ppd epson24.ppd \
		laserjet.ppd okidata9.ppd okidat24.ppd stcolor.ppd \
		stcolor2.ppd stphoto.ppd stphoto2.ppd


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
	$(INSTALL_DIR) $(DATADIR)/model
	for file in $(FILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/model; \
	done


#
# End of "$Id: Makefile,v 1.8.2.4 2003/01/07 18:27:13 mike Exp $".
#
