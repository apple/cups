#
# "$Id: Makefile 3572 2003-04-08 19:56:25Z mike $"
#
#   PHP Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../../Makedefs


#
# Where to install and how to compile the PHP module...
#

PHPDIR	=	$(BUILDROOT)`$(PHPCONFIG) --extension-dir`
OPTIONS	=	-I../.. `$(PHPCONFIG) --includes`


#
# Object files...
#

OBJS	=	phpcups.o


#
# Targets in this directory...
#

PHPCUPS	=	phpcups.so


#
# Make all targets...
#

all:	$(PHPCUPS)


#
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(PHPCUPS)


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I../.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:
	echo Installing $(PHPCUPS) in $(PHPDIR)
	$(INSTALL_DIR) $(PHPDIR)
	$(INSTALL_BIN) $(PHPCUPS) $(PHPDIR)
	if test "x$(SYMROOT)" != x; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(PHPCUPS) $(SYMROOT); \
	fi


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Uninstall object and target files...
#

uninstall:
	$(RM) $(PHPDIR)/$(PHPCUPS)
	-$(RMDIR) $(PHPDIR)


#
# phpcups.so
#

phpcups.so:	$(OBJS) ../../Makedefs
	echo Linking $@...
	if test `uname` = Darwin; then \
		DSOFLAGS="-bundle -flat_namespace -undefined suppress $(RC_CFLAGS)"; \
	else \
		DSOFLAGS="$(DSOFLAGS)"; \
	fi; \
	echo $(DSO) $$DSOFLAGS $(ARCHFLAGS) -o $@ $(OBJS) -L../../cups $(LIBS); \
	$(DSO) $$DSOFLAGS $(ARCHFLAGS) -o $@ $(OBJS) -L../../cups $(LIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 3572 2003-04-08 19:56:25Z mike $".
#
