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
			ipp-1.1.test \
			ipp-2.0.test \
			ipp-2.1.test \
			testfile.jpg \
			testfile.pdf \
			testfile.ps \
			testfile.txt


#
# Make all targets...
#

all:	ipptest ipptest-static


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
	$(RM) ipptest ipptest.o ipptest-static


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies ipptest.c >/dev/null 2>&1


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	echo Installing sample ipptest files in $(DATADIR)/ipptest...
	$(INSTALL_DIR) -m 755 $(DATADIR)/ipptest
	for file in $(TESTFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/ipptest; \
	done


#
# Install programs...
#

install-exec:
	echo Installing ipptest in $(BINDIR)...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_BIN) ipptest $(BINDIR)
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp ipptest $(SYMROOT); \
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
# ipptest
#

ipptest:	ipptest.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ipptest.o $(LIBS)


#
# ipptest-static
#

ipptest-static:	ipptest.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ ipptest.o  ../cups/$(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
