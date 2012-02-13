#
# "$Id: Makefile 7920 2008-09-10 01:04:10Z mike $"
#
#   Notifier makefile for CUPS.
#
#   Copyright 2007-2012 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs


NOTIFIERS =	$(DBUS_NOTIFIER) mailto rss
TARGETS =	$(NOTIFIERS) testnotify
OBJS	=	dbus.o mailto.o rss.o testnotify.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Make library targets...
#

libs:


#
# Make unit tests...
#

unittests:


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) dbus.h


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	$(INSTALL_DIR) -m 775 -g $(CUPS_GROUP) $(CACHEDIR)/rss


#
# Install programs...
#

install-exec:
	echo Installing notifiers in $(SERVERBIN)/notifier...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/notifier
	for file in $(NOTIFIERS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/notifier; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(NOTIFIERS) $(SYMROOT); \
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
# Uninstall all targets...
#

uninstall:
	for file in $(NOTIFIERS); do \
		$(RM) $(SERVERBIN)/notifier/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/notifier
	-$(RMDIR) $(SERVERBIN)
	-$(RMDIR) $(CACHEDIR)/rss


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies


#
# dbus
#

dbus:	dbus.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o dbus dbus.o $(DBUS_NOTIFIERLIBS) $(LIBS)


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
# End of "$Id: Makefile 7920 2008-09-10 01:04:10Z mike $".
#
