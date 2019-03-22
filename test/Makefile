#
# IPP test makefile for CUPS.
#
# Copyright 2007-2019 by Apple Inc.
# Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
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
			get-printer-attributes-suite.test \
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
			ippevepcl.o \
			ippeveprinter.o \
			ippeveps.o \
			ippfind.o \
			ipptool.o
TARGETS		=	\
			$(IPPEVECOMMANDS) \
			ippeveprinter \
			$(IPPFIND_BIN) \
			ipptool \
			$(LOCALTARGET)


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
	$(RM) $(TARGETS) $(OBJS)
	$(RM) ippeveprinter-static ippfind-static ipptool-static


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
	$(INSTALL_BIN) ippeveprinter $(BINDIR)
	if test -x ippfind; then \
		$(INSTALL_BIN) ippfind $(BINDIR); \
	fi
	$(INSTALL_BIN) ipptool $(BINDIR)
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/ippeveprinter
	for file in $(IPPEVECOMMANDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/ippeveprinter; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp ippeveprinter $(SYMROOT); \
		dsymutil $(SYMROOT)/ippeveprinter; \
		if test -x ippfind; then \
			cp ippfind $(SYMROOT); \
			dsymutil $(SYMROOT)/ippfind; \
		fi; \
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
	for file in ippeveprinter ippfind ipptool; do \
		$(RM) $(BINDIR)/$$file; \
	done
	-$(RMDIR) $(BINDIR)
	for file in $(IPPEVECOMMANDS); do \
		$(RM) $(SERVERBIN)/ippeveprinter/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/ippeveprinter
	for file in $(DATAFILES); do \
		$(RM) $(DATADIR)/ipptool/$$file; \
	done
	for file in $(TESTFILES); do \
		$(RM) $(DATADIR)/ipptool/$$file; \
	done
	-$(RMDIR) $(DATADIR)/ipptool


#
# Local programs (not built when cross-compiling...)
#

local:	ippeveprinter-static ipptool-static


#
# ippeveprinter
#

ippeveprinter:	ippeveprinter.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveprinter.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippeveprinter-static
#

ippeveprinter-static:	ippeveprinter.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveprinter.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippevepcl
#

ippevepcl:	ippevepcl.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippevepcl.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippeveps
#

ippeveps:	ippeveps.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveps.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippfind
#

ippfind:	ippfind.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippfind.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ippfind-static
#

ippfind-static:	ippfind.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippfind.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ipptool
#

ipptool:	ipptool.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ipptool.o $(LIBS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# ipptool-static
#

ipptool-static:	ipptool.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ipptool.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Dependencies...
#

include Dependencies
