#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   IPP test makefile for CUPS.
#
#   Copyright 2007-2010 by Apple Inc.
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

TESTFILES	=	\
			create-printer-subscription.test \
			get-completed-jobs.test \
			get-jobs.test \
			ipp-1.1.test \
			ipp-2.0.test \
			ipp-2.1.test \
			testfile.jpg \
			testfile.pdf \
			testfile.ps \
			testfile.txt
OBJS		=	\
			ippserver.o \
			ipptool.o
TARGETS		=	\
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
	$(RM) $(TARGETS) $(OBJS)


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


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
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp ipptool $(SYMROOT); \
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
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
