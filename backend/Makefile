#
# "$Id: Makefile 7924 2008-09-10 17:36:13Z mike $"
#
#   Backend makefile for CUPS.
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
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs

#
# Object files...
#

# RBACKENDS are installed mode 0700 so cupsd will run them as root...
#
# UBACKENDS are installed mode 0755 so cupsd will run them as an unprivileged
# user...
#
# See http://www.cups.org/documentation.php/api-filter.html for more info...
RBACKENDS =	\
		ipp \
		lpd \
		$(DNSSD_BACKEND)
UBACKENDS =	\
		snmp \
		socket \
		usb
UNITTESTS =	\
		test1284 \
		testbackend \
		testsupplies
TARGETS =	\
		libbackend.a \
		$(RBACKENDS) \
		$(UBACKENDS)
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

libs:


#
# Make unit tests...
#

unittests:	$(UNITTESTS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTESTS) $(LIBOBJS) http https ipps mdns


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
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
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


#
# Uninstall all targets...
#

uninstall:
	$(RM) $(SERVERBIN)/apple/ipp
	for file in $(IPPALIASES); do \
		$(RM) $(SERVERBIN)/apple/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/apple
	for file in $(RBACKENDS) $(UBACKENDS); do \
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
	$(CC) $(LDFLAGS) -o test1284 test1284.o ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# testbackend
#

testbackend:	testbackend.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testbackend testbackend.o ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# testsupplies
#

testsupplies:	testsupplies.o libbackend.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testsupplies testsupplies.o libbackend.a \
		../cups/$(LIBCUPSSTATIC) $(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) \
		$(COMMONLIBS) $(LIBZ)


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
	$(CC) $(LDFLAGS) -o dnssd dnssd.o libbackend.a $(LIBS)
	if test `uname` = Darwin; then \
		$(RM) mdns; \
		$(LN) dnssd mdns; \
	fi


#
# ipp
#

ipp:	ipp.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o ipp ipp.o libbackend.a $(LIBS)
	$(RM) http
	$(LN) ipp http


#
# lpd
#

lpd:	lpd.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpd lpd.o libbackend.a $(LIBS)


#
# snmp
#

snmp:	snmp.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o snmp snmp.o libbackend.a $(LIBS)


#
# socket
#

socket:	socket.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o socket socket.o libbackend.a $(LIBS)


#
# usb
#

usb:	usb.o ../cups/$(LIBCUPS) libbackend.a
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o usb usb.o libbackend.a $(LIBUSB) \
		$(BACKLIBS) $(LIBS)
usb.o:	usb.c usb-darwin.c usb-libusb.c usb-unix.c


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 7924 2008-09-10 17:36:13Z mike $".
#
