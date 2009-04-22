#
# "$Id: Makefile 7938 2008-09-11 23:53:59Z mike $"
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
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


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
	for file in $(KEEP); do \
		if test -r $(SERVERROOT)/$$file ; then \
			$(INSTALL_CONFIG) -g $(CUPS_GROUP) $$file $(SERVERROOT)/$$file.N ; \
		else \
			$(INSTALL_CONFIG) -g $(CUPS_GROUP) $$file $(SERVERROOT) ; \
		fi ; \
	done
	$(INSTALL_CONFIG) -g $(CUPS_GROUP) cupsd.conf $(SERVERROOT)/cupsd.conf.default
	$(INSTALL_DIR) -m 755 $(DATADIR)/mime
	for file in $(REPLACE); do \
		if test -r $(DATADIR)/mime/$$file ; then \
			$(MV) $(DATADIR)/mime/$$file $(DATADIR)/mime/$$file.O ; \
		fi ; \
		if test -r $(SERVERROOT)/$$file ; then \
			$(MV) $(SERVERROOT)/$$file $(DATADIR)/mime/$$file.O ; \
		fi ; \
		$(INSTALL_DATA) $$file $(DATADIR)/mime ; \
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
	for file in $(REPLACE); do \
		$(RM) $(DATADIR)/mime/$$file; \
	done
	-$(RMDIR) $(DATADIR)/mime
	-if test x$(PAMDIR) != x; then \
		$(RM) $(BUILDROOT)$(PAMDIR)/cups; \
		$(RMDIR) $(BUILDROOT)$(PAMDIR); \
	fi


#
# End of "$Id: Makefile 7938 2008-09-11 23:53:59Z mike $".
#
