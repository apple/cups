#
# "$Id: Makefile 12278 2014-12-01 13:53:56Z msweet $"
#
#   Datafile makefile for CUPS.
#
#   Copyright 2007-2014 by Apple Inc.
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


#
# End of "$Id: Makefile 12278 2014-12-01 13:53:56Z msweet $".
#
