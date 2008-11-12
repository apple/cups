#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   Fonts makefile for the Common UNIX Printing System (CUPS).
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
# Font files...
#

FONTS	=	\
		Monospace \
		Monospace-Bold \
		Monospace-BoldOblique \
		Monospace-Oblique


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
# Dummy depend target...
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
	$(INSTALL_DIR) -m 755 $(DATADIR)/fonts
	if test "x$(BANNERTOPS)" != x -o "x$(TEXTTOPS)" != x; then \
		for file in $(FONTS); do \
			$(INSTALL_DATA) $$file $(DATADIR)/fonts; \
		done \
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
	for file in $(FONTS); do \
		$(RM) $(DATADIR)/fonts/$$file; \
	done
	$(RMDIR) $(DATADIR)/fonts


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
