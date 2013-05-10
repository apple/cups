#
# "$Id$"
#
#   Configuration file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2006 by Easy Software Products.
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
# Config files...
#

KEEP	=	cupsd.conf
REPLACE	=	mime.convs mime.types


#
# Make everything...
#

all:


#
# Clean all config and object files...
#

clean:


#
# Dummy depend...
#

depend:


#
# Install files...
#

install:	all
	$(INSTALL_DIR) -m 755 $(SERVERROOT)
	for file in $(KEEP); do \
		if test -r $(SERVERROOT)/$$file ; then \
			$(INSTALL_CONFIG) $$file $(SERVERROOT)/$$file.N ; \
			chgrp $(CUPS_GROUP) $(SERVERROOT)/$$file.N || true; \
		else \
			$(INSTALL_CONFIG) $$file $(SERVERROOT) ; \
			chgrp $(CUPS_GROUP) $(SERVERROOT)/$$file || true; \
		fi ; \
	done
	$(INSTALL_CONFIG) cupsd.conf $(SERVERROOT)/cupsd.conf.default
	-chgrp $(CUPS_GROUP) $(SERVERROOT)/cupsd.conf.default
	for file in $(REPLACE); do \
		if test -r $(SERVERROOT)/$$file ; then \
			$(MV) $(SERVERROOT)/$$file $(SERVERROOT)/$$file.O ; \
		fi ; \
		$(INSTALL_CONFIG) $$file $(SERVERROOT) ; \
		chgrp $(CUPS_GROUP) $(SERVERROOT)/$$file || true; \
	done
	-if test x$(PAMDIR) != x; then \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(PAMDIR); \
		if test -r $(BUILDROOT)$(PAMDIR)/cups/$(PAMFILE) ; then \
			$(INSTALL_DATA) $(PAMFILE) $(BUILDROOT)$(PAMDIR)/cups.N ; \
		else \
			$(INSTALL_DATA) $(PAMFILE) $(BUILDROOT)$(PAMDIR)/cups ; \
		fi ; \
	fi


#
# Uninstall files...
#

uninstall:
	for file in $(KEEP) $(REPLACE) cupsd.conf.default; do \
		$(RM) $(SERVERROOT)/$$file; \
	done
	-$(RMDIR) $(SERVERROOT)
	-if test x$(PAMDIR) != x; then \
		$(RM) $(BUILDROOT)$(PAMDIR)/cups; \
		$(RMDIR) $(BUILDROOT)$(PAMDIR); \
	fi


#
# End of "$Id$".
#
