#
# "$Id: Makefile 1725 2009-10-15 18:50:47Z msweet $"
#
#   Desktop makefile for CUPS.
#
#   Copyright 2007-2009 by Apple Inc.
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
	if test "x$(DBUSDIR)" != x; then \
		echo Installing cups.conf in $(DBUSDIR)...;\
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(DBUSDIR)/system.d; \
		$(INSTALL_DATA) cups.conf $(BUILDROOT)$(DBUSDIR)/system.d/cups.conf; \
	fi
	if test "x$(MENUDIR)" != x; then \
		echo Installing desktop menu...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(MENUDIR); \
		$(INSTALL_DATA) cups.desktop $(BUILDROOT)$(MENUDIR); \
	fi
	if test "x$(ICONDIR)" != x; then \
		echo Installing desktop icons...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/16x16/apps; \
		$(INSTALL_DATA) cups-16.png $(BUILDROOT)$(ICONDIR)/hicolor/16x16/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/32x32/apps; \
		$(INSTALL_DATA) cups-32.png $(BUILDROOT)$(ICONDIR)/hicolor/32x32/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/64x64/apps; \
		$(INSTALL_DATA) cups-64.png $(BUILDROOT)$(ICONDIR)/hicolor/64x64/apps/cups.png; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(ICONDIR)/hicolor/128x128/apps; \
		$(INSTALL_DATA) cups-128.png $(BUILDROOT)$(ICONDIR)/hicolor/128x128/apps/cups.png; \
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
	if test "x$(DBUSDIR)" != x; then \
		echo Uninstalling cups.conf in $(DBUSDIR)...;\
		$(RM) $(BUILDROOT)$(DBUSDIR)/cups.conf; \
		$(RMDIR) $(BUILDROOT)$(DBUSDIR); \
	fi
	if test "x$(MENUDIR)" != x; then \
		echo Uninstalling desktop menu...; \
		$(RM) $(BUILDROOT)$(MENUDIR)/cups.desktop; \
	fi
	if test "x$(ICONDIR)" != x; then \
		echo Uninstalling desktop icons...; \
		$(RM) $(BUILDROOT)$(ICONDIR)/hicolor/16x16/apps/cups.png; \
		$(RM) $(BUILDROOT)$(ICONDIR)/hicolor/32x32/apps/cups.png; \
		$(RM) $(BUILDROOT)$(ICONDIR)/hicolor/64x64/apps/cups.png; \
		$(RM) $(BUILDROOT)$(ICONDIR)/hicolor/128x128/apps/cups.png; \
	fi


#
# End of "$Id: Makefile 1725 2009-10-15 18:50:47Z msweet $".
#
