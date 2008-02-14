#
# "$Id$"
#
#   Makefile for the CUPS PPD Compiler.
#
#   Copyright 2007 by Apple Inc.
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
	makedepend -Y -I.. -fDependencies *.cxx >/dev/null 2>&1


#
# Install...
#

install:
	echo Installing PPD compiler programs...
	$(INSTALL_DIR) $(BUILDROOT)$(bindir)
	$(INSTALL_BIN) ppdc $(BUILDROOT)$(bindir)
	$(INSTALL_BIN) ppdhtml $(BUILDROOT)$(bindir)
	$(INSTALL_BIN) ppdi $(BUILDROOT)$(bindir)
	$(INSTALL_BIN) ppdmerge $(BUILDROOT)$(bindir)
	$(INSTALL_BIN) ppdpo $(BUILDROOT)$(bindir)
	$(INSTALL_DIR) $(BUILDROOT)$(CUPS_SERVERBIN)/driver
	$(INSTALL_BIN) drv $(BUILDROOT)$(CUPS_SERVERBIN)/driver
	$(INSTALL_DIR) $(BUILDROOT)$(CUPS_DATADIR)/drv


#
# Uninstall...
#

uninstall:
	$(RM) $(BUILDROOT)$(bindir)/ppdc
	$(RM) $(BUILDROOT)$(bindir)/ppdhtml
	$(RM) $(BUILDROOT)$(bindir)/ppdi
	$(RM) $(BUILDROOT)$(bindir)/ppdmerge
	$(RM) $(BUILDROOT)$(bindir)/ppdpo
	$(RM) $(BUILDROOT)$(CUPS_SERVERBIN)/driver/drv


#
# drv, the CUPS driver interface program.
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
