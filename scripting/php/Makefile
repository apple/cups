#
# "$Id: Makefile 3572 2003-04-08 19:56:25Z mike $"
#
#   PHP Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org/
#

include ../../Makedefs


#
# Where to install and how to compile the PHP module...
#

PHPDIR	=	`php-config --extension-dir`
OPTIONS	=	-I../.. `php-config --includes`
PHPLIBS	=	`php-config --ldflags` `php-config --libs`


#
# Object files...
#

OBJS	=	phpcups.o


#
# Targets in this directory...
#

TARGETS	=	phpcups.so


#
# Make all targets...
#

all:	$(TARGETS)


#
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I../.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install object and target files...
#

install:	all
	echo Installing $(TARGETS) in $(PHPDIR)
	$(INSTALL_DIR) $(PHPDIR)
	cp $(TARGETS) $(PHPDIR)


#
# phpcups.so
#

phpcups.so:	$(OBJS) ../../Makedefs
	echo Linking $@...
	if test `uname` = Darwin; then \
		DSOFLAGS="-bundle -flat_namespace -undefined suppress"; \
	else \
		DSOFLAGS="$(DSOFLAGS)"; \
	fi; \
	echo $(DSO) $$DSOFLAGS -o $@ $(OBJS) -L../../cups -lcups $(PHPLIBS); \
	$(DSO) $$DSOFLAGS -o $@ $(OBJS) -L../../cups -lcups $(PHPLIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 3572 2003-04-08 19:56:25Z mike $".
#
