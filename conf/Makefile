#
# "$Id: Makefile,v 1.3 1999/05/15 03:39:33 mike Exp $"
#
#   Configuration file makefile for the Common UNIX Printing System (CUPS).
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
# Config files...
#

FILES	=	classes.conf cupsd.conf mime.convs mime.types printers.conf

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
	-$(MKDIR) $(SERVERROOT)/conf
	for file in $(FILES); do \
		if test -e $(SERVERROOT)/conf/$$file ; then \
			$(CP) $$file $(SERVERROOT)/conf/$$file.N ; \
		else \
			$(CP) $$file $(SERVERROOT)/conf ; \
		fi ; \
	done

#
# End of "$Id: Makefile,v 1.3 1999/05/15 03:39:33 mike Exp $".
#
