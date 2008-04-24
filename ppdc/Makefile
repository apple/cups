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

LIBOBJS =	\
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
LIB32OBJS	= $(LIBOBJS:.o=.32.o)
LIB64OBJS	= $(LIBOBJS:.o=.64.o)
OBJS =		\
		$(LIBOBJS) \
		$(LIB32OBJS) \
		$(LIB64OBJS) \
		drv.o \
		ppdc.o \
		ppdhtml.o \
		ppdi.o \
		ppdmerge.o \
		ppdpo.o
TARGETS	=	\
		$(LIBCUPSPPDC) \
		$(LIB32CUPSPPDC) \
		$(LIB64CUPSPPDC) \
		libcupsppdc.a \
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
	$(RM) libcupsppdc.so libcupsppdc.sl libcupsppdc.dylib
	$(RM) -r 32bit 64bit


#
# Update dependencies...
#

depend:
	touch Dependencies.tmp
	makedepend -Y -I.. -fDependencies.tmp $(OBJS:.o=.c) >/dev/null 2>&1
	$(RM) Dependencies
	cp Dependencies.tmp Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.32.o: \1\.c /' Dependencies.tmp >>Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.64.o: \1\.c /' Dependencies.tmp >>Dependencies
	$(RM) Dependencies.tmp


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	$(INSTALL_DIR) $(DATADIR)/drv
	$(INSTALL_DATA) sample.drv $(DATADIR)/drv
	if test `uname` = Darwin; then \
		$(INSTALL_DIR) /Library/Printers/PPDs.drv; \
	fi


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
	echo Installing header files in $(INCLUDEDIR)/cups...
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) ppdc.h $(INCLUDEDIR)/cups


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	echo Installing libraries in $(LIBDIR)...
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSPPDC) $(LIBDIR)
	if test $(LIBCUPSPPDC) = "libcupsppdc.so.1" -o $(LIBCUPSPPDC) = "libcupsppdc.sl.1"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSPPDC) .1`; \
		$(LN) $(LIBCUPSPPDC) $(LIBDIR)/`basename $(LIBCUPSPPDC) .1`; \
	fi
	if test $(LIBCUPSPPDC) = "libcupsppdc.1.dylib"; then \
		$(RM) $(LIBDIR)/libcupsppdc.dylib; \
		$(LN) $(LIBCUPSPPDC) $(LIBDIR)/libcupsppdc.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPSPPDC) $(SYMROOT); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) libcupsppdc.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsppdc.a

install32bit:
	echo Installing libraries in $(LIB32DIR)...
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcupsppdc.so.1 $(LIB32DIR)/libcupsppdc.so.1
	$(LN) libcupsppdc.so.1 $(LIB32DIR)/libcupsppdc.so

install64bit:
	echo Installing libraries in $(LIB64DIR)...
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcupsppdc.so.1 $(LIB64DIR)/libcupsppdc.so.1
	$(LN) libcupsppdc.so.1 $(LIB64DIR)/libcupsppdc.so


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
	$(RM) $(DATADIR)/drv/sample.drv
	$(RMDIR) $(DATADIR)/drv
	if test `uname` = Darwin; then \
		$(RMDIR) /Library/Printers/PPDs.drv; \
	fi
	$(RM) $(LIBDIR)/libcupsppdc.1.dylib
	$(RM) $(LIBDIR)/libcupsppdc.a
	$(RM) $(LIBDIR)/libcupsppdc.dylib
	$(RM) $(LIBDIR)/libcupsppdc_s.a
	$(RM) $(LIBDIR)/libcupsppdc.sl
	$(RM) $(LIBDIR)/libcupsppdc.sl.1
	$(RM) $(LIBDIR)/libcupsppdc.so
	$(RM) $(LIBDIR)/libcupsppdc.so.1
	-$(RMDIR) $(LIBDIR)
	$(RM) $(INCLUDEDIR)/cups/ppdc.h
	-$(RMDIR) $(INCLUDEDIR)/cups

uninstall32bit:
	$(RM) $(LIB32DIR)/libcupsppdc.so
	$(RM) $(LIB32DIR)/libcupsppdc.so.1
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcupsppdc.so
	$(RM) $(LIB64DIR)/libcupsppdc.so.1
	-$(RMDIR) $(LIB64DIR)


#
# Automatic API help files...
#

apihelp:
	mxmldoc --section "Programming" \
		--title "PPD Compiler API" \
		--css ../doc/cups-printable.css \
		--header api-ppdc.header --intro api-ppdc.shtml \
		ppdc.h $(LIBOBJS:.o=.cxx) >../doc/help/api-ppdc.html

framedhelp:
	mxmldoc --framed api-ppdc \
		--section "Programming" \
		--title "PPD Compiler API" \
		--css ../doc/cups-printable.css \
		--header api-ppdc.header --intro api-ppdc.shtml \
		ppdc.h $(LIBOBJS:.o=.cxx)


#
# drv, the CUPS driver interface program to the PPD compiler.
#

drv:			drv.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ drv.o -L. -lcupsppdc $(LIBS)


#
# ppdc, the PPD compiler.
#

ppdc:			ppdc.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdc.o -L. -lcupsppdc $(LIBS)


ppdc-static:		ppdc.o libcupsppdc.a  ../cups/libcups.a foo.drv foo-fr.po
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o ppdc-static ppdc.o libcupsppdc.a ../cups/libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	echo Testing PPD compiler...
	./ppdc-static -l en,fr -I ../data foo.drv
	./ppdc-static -l en,fr -z -I ../data foo.drv


#
# ppdhtml, the PPD to HTML utility.
#

ppdhtml:			ppdhtml.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdhtml.o -L. -lcupsppdc $(LIBS)


#
# ppdi, import PPD files.
#

ppdi:			ppdi.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdi.o -L. -lcupsppdc $(LIBS)


#
# ppdmerge, merge PPD files.
#

ppdmerge:			ppdmerge.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdmerge.o $(LIBS)


#
# ppdpo, create message catalog files.
#

ppdpo:			ppdpo.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o $@ ppdpo.o -L. -lcupsppdc $(LIBS)


#
# libcupsppdc.so.1, libcupsppdc.sl.1
#

libcupsppdc.so.1 libcupsppdc.sl.1:	$(LIBOBJS)
	echo Linking $@...
	$(DSOXX) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LIBS)
	$(RM) `basename $@ .1`
	$(LN) $@ `basename $@ .1`


#
# 32bit/libcupsppdc.so.1
#

32bit/libcupsppdc.so.1:	$(LIB32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSOXX) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(LIB32OBJS) $(LIBS)
	$(RM) 32bit/libcupsppdc.so
	$(LN) libcupsppdc.so.1 32bit/libcupsppdc.so


#
# 64bit/libcupsppdc.so.1
#

64bit/libcupsppdc.so.1:	$(LIB64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSOXX) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(LIB64OBJS) $(LIBS)
	$(RM) 64bit/libcupsppdc.so
	$(LN) libcupsppdc.so.1 64bit/libcupsppdc.so


#
# libcupsppdc.1.dylib
#

libcupsppdc.1.dylib:	$(LIBOBJS) libcupsppdc.exp
	echo Linking $@...
	$(DSOXX) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 1.0.0 \
		-compatibility_version 1.0.0 \
		-exported_symbols_list libcupsppdc.exp \
		$(LIBOBJS) $(LIBS)
	$(RM) libcupsppdc.dylib
	$(LN) $@ libcupsppdc.dylib


#
# libcupsppdc_s.a
#

libcupsppdc_s.a:	$(LIBOBJS)
	echo Creating $@...
	$(DSOXX) $(DSOFLAGS) -o libcupsppdc_s.o $(LIBOBJS) $(LIBS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupsppdc_s.o


#
# libcupsppdc.la
#

libcupsppdc.la:    $(LIBOBJS)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 1:0 $(LIBS)


#
# libcupsppdc.a
#

libcupsppdc.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# Include dependencies...
#

include Dependencies


#
# End of "$Id$".
#
