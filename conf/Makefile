#
# "$Id$"
#
#   Configuration file makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1993-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

#
# Config files...
#

KEEP	=	cupsd.conf snmp.conf
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
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
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
		$(INSTALL_DATA) $$file $(SERVERROOT) ; \
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
# Install programs...
#

install-exec:


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


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
