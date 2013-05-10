#
# "$Id$"
#
#   PPD compiler example makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 2002-2005 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

#
# Include standard definitions...
#

include ../Makedefs


#
# Examples...
#

EXAMPLES =	\
		color.drv \
		constraint.drv \
		custom.drv \
		grouping.drv \
		laserjet-basic.drv \
		laserjet-pjl.drv \
		minimum.drv \
		postscript.drv \
		r300-basic.drv \
		r300-colorman.drv \
		r300-remote.drv


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
# Clean everything...
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
	$(INSTALL_DIR) $(DATADIR)/examples
	for file in $(EXAMPLES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/examples; \
	done


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
	for file in $(EXAMPLES); do \
		$(RM) $(DATADIR)/examples/$$file; \
	done
	-$(RMDIR) $(DATADIR)/examples


#
# End of "$Id$".
#
