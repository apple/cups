#
# "$Id: Makefile 12414 2015-01-21 00:02:04Z msweet $"
#
#   IPP test makefile for CUPS.
#
#   Copyright 2007-2014 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs


#
# Sample test files.
#

DATAFILES	=	\
			color.jpg \
			document-a4.pdf \
			document-a4.ps \
			document-letter.pdf \
			document-letter.ps \
			gray.jpg \
			onepage-a4.pdf \
			onepage-a4.ps \
			onepage-letter.pdf \
			onepage-letter.ps \
			testfile.jpg \
			testfile.pcl \
			testfile.pdf \
			testfile.ps \
			testfile.txt
TESTFILES	=	\
			create-printer-subscription.test \
			get-completed-jobs.test \
			get-jobs.test \
			get-notifications.test \
			get-printer-attributes.test \
			get-subscriptions.test \
			ipp-1.1.test \
			ipp-2.0.test \
			ipp-2.1.test \
			ipp-2.2.test \
			ipp-everywhere.test \
			print-job.test \
			print-job-deflate.test \
			print-job-gzip.test \
			validate-job.test
OBJS		=	\
			ippfind.o \
			ippinfra.o \
			ippproxy.o \
			ippserver.o \
			ipptool.o \
			xmltotest.o
TARGETS		=	\
			$(IPPFIND_BIN) \
			ippserver \
			ipptool \
			ipptool-static


#
# Make all targets...
#

all:		$(TARGETS)


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
	$(RM) $(TARGETS) $(OBJS) ippfind-static ippinfra ippinfra-shared ippproxy ippproxy-shared ippserver-shared


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
	echo Installing sample ipptool files in $(DATADIR)/ipptool...
	$(INSTALL_DIR) -m 755 $(DATADIR)/ipptool
	for file in $(DATAFILES); do \
		$(INSTALL_COMPDATA) $$file $(DATADIR)/ipptool; \
	done
	for file in $(TESTFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/ipptool; \
	done


#
# Install programs...
#

install-exec:
	echo Installing ipptool in $(BINDIR)...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_BIN) ipptool $(BINDIR)
	if test -x ippfind; then \
		$(INSTALL_BIN) ippfind $(BINDIR); \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp ippfind $(SYMROOT); \
		dsymutil $(SYMROOT)/ippfind; \
		cp ipptool $(SYMROOT); \
		dsymutil $(SYMROOT)/ipptool; \
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
# Unnstall all targets...
#

uninstall:
	echo Uninstalling sample ipptool files from $(DATADIR)/ipptool...
	for file in $(DATAFILES); do \
		$(RM) $(DATADIR)/ipptool/$$file; \
	done
	for file in $(TESTFILES); do \
		$(RM) $(DATADIR)/ipptool/$$file; \
	done
	-$(RMDIR) $(DATADIR)/ipptool


#
# ippfind
#

ippfind:	ippfind.o ../cups/$(LIBCUPS) ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippfind.o $(LIBS)
	echo Linking $@-static...
	$(CC) $(LDFLAGS) -o $@-static ippfind.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# ippinfra
#

ippinfra:	ippinfra.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippinfra.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# ippinfra-shared
#

ippinfra-shared:	ippinfra.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippinfra.o $(LIBS)


#
# ippproxy
#

ippproxy:	ippproxy.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippproxy.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# ippproxy-shared
#

ippproxy-shared:	ippproxy.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippproxy.o $(LIBS)


#
# ippserver
#

ippserver:	ippserver.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippserver.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# ippserver-shared
#

ippserver-shared:	ippserver.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ippserver.o $(LIBS)


#
# ipptool
#

ipptool:	ipptool.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ipptool.o $(LIBS)


#
# ipptool-static
#

ipptool-static:	ipptool.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ipptool.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# xmltotest
#

xmltotest: xmltotest.o ../cups/$(LIBUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ xmltotest.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ) \
		$(LIBMXML)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 12414 2015-01-21 00:02:04Z msweet $".
#
