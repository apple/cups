#
# "$Id$"
#
#   Notifier makefile for the Common UNIX Printing System (CUPS).
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

include ../Makedefs


TARGETS =	mailto rss testnotify
OBJS	=	mailto.o rss.o testnotify.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	$(INSTALL_DIR) -m 775 $(CACHEDIR)/rss
	-chgrp $(CUPS_GROUP) $(CACHEDIR)/rss
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
		done \
	fi


#
# Install programs...
#

install-exec:
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/notifier
	for file in $(TARGETS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/notifier; \
	done


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


#
# Uninstall all targets...
#

uninstall:
	for file in $(TARGETS); do \
		$(RM) $(SERVERBIN)/notifier/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/notifier
	-$(RMDIR) $(SERVERBIN)
	-$(RMDIR) $(CACHEDIR)/rss


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# mailto
#

mailto:	mailto.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o mailto mailto.o $(LIBS)


#
# rss
#

rss:	rss.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o rss rss.o $(LIBS)


#
# testnotify
#

testnotify:	testnotify.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testnotify testnotify.o $(LIBS)


$(OBJS):	../Makedefs

include Dependencies


#
# End of "$Id$".
#
