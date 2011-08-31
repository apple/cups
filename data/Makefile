#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   Datafile makefile for CUPS.
#
#   Copyright 2007-2011 by Apple Inc.
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
# Data files...
#

BANNERS	=	\
		classified \
		confidential \
		secret \
		standard \
		topsecret \
		unclassified

DATAFILES =	\
		testprint

PPDCFILES =	\
		epson.h \
		font.defs \
		hp.h \
		label.h \
		media.defs \
		raster.defs


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
	$(INSTALL_DIR) -m 755 $(DATADIR)/banners
	for file in $(BANNERS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/banners; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/data
	for file in $(DATAFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/data; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/model
	$(INSTALL_DIR) -m 755 $(DATADIR)/ppdc
	for file in $(PPDCFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/ppdc; \
	done
	$(INSTALL_DIR) -m 755 $(DATADIR)/profiles


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
	for file in $(BANNERS); do \
		$(RM) $(DATADIR)/banners/$$file; \
	done
	for file in $(DATAFILES); do \
		$(RM) $(DATADIR)/data/$$file; \
	done
	for file in $(PPDCFILES); do \
		$(RM) $(DATADIR)/ppdc/$$file; \
	done
	-$(RMDIR) $(DATADIR)/profiles
	-$(RMDIR) $(DATADIR)/ppdc
	-$(RMDIR) $(DATADIR)/model
	-$(RMDIR) $(DATADIR)/data
	-$(RMDIR) $(DATADIR)/banners
	-$(RMDIR) $(DATADIR)


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
