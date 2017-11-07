#
# Datafile makefile for CUPS.
#
# Copyright 2007-2014 by Apple Inc.
# Copyright 1993-2006 by Easy Software Products.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
#

include ../Makedefs

#
# Data files...
#

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
	$(INSTALL_DIR) -m 755 $(DATADIR)/data
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
	for file in $(PPDCFILES); do \
		$(RM) $(DATADIR)/ppdc/$$file; \
	done
	-$(RMDIR) $(DATADIR)/profiles
	-$(RMDIR) $(DATADIR)/ppdc
	-$(RMDIR) $(DATADIR)/model
	-$(RMDIR) $(DATADIR)/data
	-$(RMDIR) $(DATADIR)/banners
	-$(RMDIR) $(DATADIR)
