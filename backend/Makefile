#
# Backend makefile for CUPS.
#
# Copyright 2007-2019 by Apple Inc.
# Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
#

include ../Makedefs

#
# Object files...
#

# RBACKENDS are installed mode 0700 so cupsd will run them as root...
#
# UBACKENDS and ULBACKENDS are installed mode 0755 so cupsd will run them as
# an unprivileged user...
#
# See http://www.cups.org/doc/api-filter.html for more info...
RBACKENDS =	\
		ipp \
		lpd \
		$(DNSSD_BACKEND)
UBACKENDS =	\
		snmp \
		socket
ULBACKENDS =	\
		usb
UNITTESTS =	\
		test1284 \
		testbackend \
		testsupplies
TARGETS =	\
		libbackend.a \
		$(RBACKENDS) \
		$(UBACKENDS) \
		$(ULBACKENDS)
LIBOBJS	=	\
		ieee1284.o \
		network.o \
		runloop.o \
		snmp-supplies.o
OBJS	=	\
		ipp.o \
		lpd.o \
		dnssd.o \
		snmp.o \
		socket.o \
		test1284.o \
		testbackend.o \
		testsupplies.o \
		usb.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Make library targets...
#

libs:	$(ULBACKENDS)


#
# Make unit tests...
#

unittests:	$(UNITTESTS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTESTS) $(LIBOBJS) http https ipps mdns socket-static


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	if test "x$(USBQUIRKS)" != x; then \
		echo Installing USB quirks in $(USBQUIRKS); \
		$(INSTALL_DIR) -m 755 $(USBQUIRKS); \
		$(INSTALL_DATA) org.cups.usb-quirks $(USBQUIRKS); \
	fi


#
# Install programs...
#

install-exec:	$(INSTALLXPC)
	echo Installing backends in $(SERVERBIN)/backend
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/backend
	for file in $(RBACKENDS); do \
		$(LIBTOOL) $(INSTALL_BIN) -m 700 $$file $(SERVERBIN)/backend; \
	done
	for file in $(UBACKENDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/backend; \
	done
	for file in $(IPPALIASES); do \
		$(RM) $(SERVERBIN)/backend/$$file; \
		$(LN) ipp $(SERVERBIN)/backend/$$file; \
	done
	if test "x$(DNSSD_BACKEND)" != x -a `uname` = Darwin; then \
		$(RM) $(SERVERBIN)/backend/mdns; \
		$(LN) $(DNSSD_BACKEND) $(SERVERBIN)/backend/mdns; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(RBACKENDS) $(UBACKENDS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done \
	fi

install-xpc:	ipp
	echo Installing XPC backends in $(SERVERBIN)/apple
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/apple
	$(LIBTOOL) $(INSTALL_BIN) ipp $(SERVERBIN)/apple
	for file in $(IPPALIASES); do \
		$(RM) $(SERVERBIN)/apple/$$file; \
		$(LN) ipp $(SERVERBIN)/apple/$$file; \
	done


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:
	echo Installing backends in $(SERVERBIN)/backend
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/backend
	for file in $(ULBACKENDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/backend; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(ULBACKENDS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done \
	fi


#
# Uninstall all targets...
#

uninstall:
	$(RM) $(SERVERBIN)/apple/ipp
	for file in $(IPPALIASES); do \
		$(RM) $(SERVERBIN)/apple/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/apple
	for file in $(RBACKENDS) $(UBACKENDS) $(ULBACKENDS); do \
		$(RM) $(SERVERBIN)/backend/$$file; \
	done
	for file in $(IPPALIASES); do \
		$(RM) $(SERVERBIN)/backend/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/backend
	-$(RMDIR) $(SERVERBIN)


#
# test1284
#

test1284:	test1284.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o test1284 test1284.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testbackend
#

testbackend:	testbackend.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o testbackend testbackend.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testsupplies
#

testsupplies:	testsupplies.o libbackend.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o testsupplies testsupplies.o libbackend.a \
		$(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# libbackend.a
#

libbackend.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# dnssd
#

dnssd:	dnssd.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o dnssd dnssd.o libbackend.a $(DNSSDLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	if test `uname` = Darwin; then \
		$(RM) mdns; \
		$(LN) dnssd mdns; \
	fi


#
# ipp
#

ipp:	ipp.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o ipp ipp.o libbackend.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	$(RM) http https ipps
	for file in $(IPPALIASES); do \
		$(LN) ipp $$file; \
	done


#
# lpd
#

lpd:	lpd.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o lpd lpd.o libbackend.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# snmp
#

snmp:	snmp.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o snmp snmp.o libbackend.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o socket socket.o libbackend.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

socket-static:	socket.o  ../cups/$(LIBCUPSSTATIC) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o socket-static socket.o libbackend.a \
		$(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# usb
#

usb:	usb.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(ALL_LDFLAGS) -o usb usb.o libbackend.a $(LIBUSB) \
		$(BACKLIBS) $(COMMONLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
usb.o:	usb.c usb-darwin.c usb-libusb.c usb-unix.c


#
# Dependencies...
#

include Dependencies
