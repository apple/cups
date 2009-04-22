#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   Berkeley commands makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2009 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs


TARGETS	=	lpc lpq lpr lprm
OBJS	=	lpc.o lpq.o lpr.o lprm.o


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
	$(RM) $(OBJS) $(TARGETS)


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


#
# Install programs...
#

install-exec:
	echo Installing Berkeley user printing commands in $(BINDIR)...
	$(INSTALL_DIR) -m 755 $(BINDIR)
	$(INSTALL_BIN) lpq $(BINDIR)
	$(INSTALL_BIN) lpr $(BINDIR)
	$(INSTALL_BIN) lprm $(BINDIR)
	echo Installing Berkeley admin printing commands in $(BINDIR)...
	$(INSTALL_DIR) -m 755 $(SBINDIR)
	$(INSTALL_BIN) lpc $(SBINDIR)
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
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
	$(RM) $(BINDIR)/lpq
	$(RM) $(BINDIR)/lpr
	$(RM) $(BINDIR)/lprm
	$(RM) $(SBINDIR)/lpc
	-$(RMDIR) $(SBINDIR)
	-$(RMDIR) $(BINDIR)


#
# lpc
#

lpc:	lpc.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpc lpc.o $(LIBS)


#
# lpq
#

lpq:	lpq.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpq lpq.o $(LIBS)


#
# lpr
#

lpr:	lpr.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lpr lpr.o $(LIBS)


#
# lprm
#

lprm:	lprm.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o lprm lprm.o $(LIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
