#
# IPP tools makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs


OBJS		=	\
			ippevepcl.o \
			ippeveprinter.o \
			ippeveps.o \
			ippfind.o \
			ipptool.o
IPPTOOLS	=	\
			ippeveprinter \
			$(IPPFIND_BIN) \
			ipptool
TARGETS		=	\
			$(IPPEVECOMMANDS) \
			$(IPPTOOLS) \
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
	$(RM) $(IPPTOOLS) $(IPPEVECOMMANDS) $(OBJS)
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


#
# Install programs...
#

install-exec:
	echo Installing IPP tools in $(BINDIR)...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	for file in $(IPPTOOLS); do \
		$(INSTALL_BIN) $$file $(BINDIR); \
	done
	echo Installing printer commands in $(SERVERBIN)/ippeveprinter...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/ippeveprinter
	for file in $(IPPEVECOMMANDS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/ippeveprinter; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(IPPTOOLS) $(IPPEVECOMMANDS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done; \
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
	echo Uninstalling IPP tools from $(BINDIR)...
	for file in $(IPPTOOLS); do \
		$(RM) $(BINDIR)/$$file; \
	done
	-$(RMDIR) $(BINDIR)
	echo Uninstalling print commands from $(SERVERBIN)/ippeveprinter...
	for file in $(IPPEVECOMMANDS); do \
		$(RM) $(SERVERBIN)/ippeveprinter/$$file; \
	done
	-$(RMDIR) $(SERVERBIN)/ippeveprinter


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
	$(LD_CC) $(ALL_LDFLAGS) -o $@ ippeveprinter.o ../cups/$(LIBCUPSSTATIC) \
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
# Generate the header containing the data for printer.png...
#

pngheader:
	echo "Generating printer-png.h from printer.png..."
	echo "static const unsigned char printer_png[] =" >printer-png.h
	echo "{" >>printer-png.h
	od -t x1 printer.png | cut -b12- | awk '{printf("  "); for (i = 1; i <= NF; i ++) printf("0x%s,", $$i); print "";}' >>printer-png.h
	echo "};" >>printer-png.h


#
# Dependencies...
#

include Dependencies
