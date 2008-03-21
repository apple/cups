#
# "$Id$"
#
#   Makefile for the CUPS PPD Compiler.
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 2002-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

#
# Include standard definitions...
#

include ../Makedefs


#
# Object files...
#

PPDCOBJS	= \
		ppdc-array.o \
		ppdc-attr.o \
		ppdc-catalog.o \
		ppdc-choice.o \
		ppdc-constraint.o \
		ppdc-driver.o \
		ppdc-file.o \
		ppdc-filter.o \
		ppdc-font.o \
		ppdc-group.o \
		ppdc-import.o \
		ppdc-mediasize.o \
		ppdc-message.o \
		ppdc-option.o \
		ppdc-profile.o \
		ppdc-shared.o \
		ppdc-source.o \
		ppdc-string.o \
		ppdc-variable.o

OBJS		= \
		drv.o \
		ppdc.o \
		ppdhtml.o \
		ppdi.o \
		ppdmerge.o \
		ppdpo.o \
		$(PPDCOBJS)

TARGETS		= \
		libppdc.a \
		drv \
		ppdc-static \
		ppdc \
		ppdhtml \
		ppdi \
		ppdmerge \
		ppdpo


#
# Make everything...
#

all:		$(TARGETS)


#
# Clean everything...
#

clean:
	$(RM) $(OBJS) core
	$(RM) *.bak *.bck core.*
	$(RM) $(TARGETS)
	$(RM) -r ppd
	$(RM) test.drv


#
# Update dependencies...
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.cxx) >/dev/null 2>&1


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	$(INSTALL_DIR) $(DATADIR)/drv


#
# Install programs...
#

install-exec:
	echo Installing PPD compiler programs...
	$(INSTALL_DIR) $(BINDIR)
	$(INSTALL_BIN) ppdc $(BINDIR)
	$(INSTALL_BIN) ppdhtml $(BINDIR)
	$(INSTALL_BIN) ppdi $(BINDIR)
	$(INSTALL_BIN) ppdmerge $(BINDIR)
	$(INSTALL_BIN) ppdpo $(BINDIR)
	$(INSTALL_DIR) $(SERVERBIN)/driver
	$(INSTALL_BIN) drv $(SERVERBIN)/driver
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
# Uninstall...
#

uninstall:
	$(RM) $(BINDIR)/ppdc
	$(RM) $(BINDIR)/ppdhtml
	$(RM) $(BINDIR)/ppdi
	$(RM) $(BINDIR)/ppdmerge
	$(RM) $(BINDIR)/ppdpo
	$(RM) $(SERVERBIN)/driver/drv
	$(RMDIR) $(SERVERBIN)/driver
	$(RMDIR) $(DATADIR)/drv


#
# drv, the CUPS driver interface program to the PPD compiler.
#

drv:			drv.o libppdc.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ drv.o libppdc.a $(LIBS)


#
# ppdc, the PPD compiler.
#

ppdc:			ppdc.o libppdc.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdc.o libppdc.a $(LIBS)


ppdc-static:		ppdc.o libppdc.a  ../cups/libcups.a foo.drv foo-fr.po
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o ppdc-static ppdc.o libppdc.a ../cups/libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	echo Testing PPD compiler...
	./ppdc-static -l en,fr -I ../data foo.drv
	./ppdc-static -l en,fr -z -I ../data foo.drv


#
# ppdhtml, the PPD to HTML utility.
#

ppdhtml:			ppdhtml.o libppdc.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdhtml.o libppdc.a $(LIBS)


#
# ppdi, import PPD files.
#

ppdi:			ppdi.o libppdc.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdi.o libppdc.a $(LIBS)


#
# ppdmerge, merge PPD files.
#

ppdmerge:			ppdmerge.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdmerge.o $(LIBS)


#
# ppdpo, create message catalog files.
#

ppdpo:			ppdpo.o libppdc.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdpo.o libppdc.a $(LIBS)


#
# libppdc.a, the PPD compiler library...
#

libppdc.a:		$(PPDCOBJS)
	echo Creating $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(PPDCOBJS)
	$(RANLIB) $@


#
# Include dependencies...
#

include Dependencies


#
# End of "$Id$".
#
