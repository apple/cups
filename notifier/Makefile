#
# Notifier makefile for CUPS.
#
# Copyright 2007-2019 by Apple Inc.
# Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
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
		for file in $(NOTIFIERS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done \
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
	$(LD_CC) $(ALL_LDFLAGS) -o dbus dbus.o $(DBUS_NOTIFIERLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# mailto
#

mailto:	mailto.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o mailto mailto.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# rss
#

rss:	rss.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o rss rss.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testnotify
#

testnotify:	testnotify.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o testnotify testnotify.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


$(OBJS):	../Makedefs

include Dependencies
