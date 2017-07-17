#
# Makefile for the CUPS PPD Compiler.
#
# Copyright 2007-2017 by Apple Inc.
# Copyright 2002-2006 by Easy Software Products.
#
# These coded instructions, statements, and computer programs are the
# property of Apple Inc. and are protected by Federal copyright
# law.  Distribution and use rights are outlined in the file "LICENSE.txt"
# which should have been included with this file.  If this file is
# missing or damaged, see the license at "http://www.cups.org/".
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
OBJS =		\
		$(LIBOBJS) \
		genstrings.o \
		ppdc.o \
		ppdhtml.o \
		ppdi.o \
		ppdmerge.o \
		ppdpo.o \
		testcatalog.o
LIBTARGETS =	\
		$(LIBCUPSPPDC) \
		libcupsppdc.a
UNITTARGETS =	\
		ppdc-static \
		ppdi-static \
		testcatalog
EXECTARGETS =	\
		ppdc \
		ppdhtml \
		ppdi \
		ppdmerge \
		ppdpo

TARGETS	=	\
		$(LIBTARGETS) \
		$(EXECTARGETS) \
		$(LOCALTARGET)


#
# Make everything...
#

all:		$(TARGETS)


#
# Make library targets...
#

libs:		$(LIBTARGETS)


#
# Make unit tests...
#

unittests:	$(UNITTARGETS)


#
# Clean everything...
#

clean:
	$(RM) $(OBJS) core
	$(RM) *.bak *.bck core.*
	$(RM) $(TARGETS) $(UNITTARGETS) genstrings
	$(RM) -r ppd ppd2
	$(RM) sample-import.drv sample.c test.drv
	$(RM) libcupsppdc.so libcupsppdc.dylib


#
# Update dependencies...
#

depend:
	$(CXX) -MM $(ALL_CXXFLAGS) $(OBJS:.o=.cxx) >Dependencies


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


#
# Install programs...
#

install-exec:
	echo Installing PPD compiler programs...
	$(INSTALL_DIR) $(BINDIR)
	for file in $(EXECTARGETS); do \
		$(INSTALL_BIN) $$file $(BINDIR); \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(EXECTARGETS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done \
	fi


#
# Install headers...
#

install-headers:
	if test "x$(privateinclude)" != x; then \
		echo Installing private header files into $(PRIVATEINCLUDE)...; \
		$(INSTALL_DIR) -m 755 $(PRIVATEINCLUDE); \
		$(INSTALL_DATA) ppdc.h $(PRIVATEINCLUDE); \
	fi


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC)
	echo Installing libraries in $(LIBDIR)...
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSPPDC) $(LIBDIR)
	if test $(LIBCUPSPPDC) = "libcupsppdc.so.1"; then \
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
		dsymutil $(SYMROOT)/$(LIBCUPSPPDC); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) -m 755 libcupsppdc.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsppdc.a
	$(CHMOD) 555 $(LIBDIR)/libcupsppdc.a


#
# Uninstall...
#

uninstall:
	for file in $(EXECTARGETS); do \
		$(RM) $(BINDIR)/$$file; \
	done
	$(RM) $(DATADIR)/drv/sample.drv
	$(RMDIR) $(DATADIR)/drv
	$(RM) $(LIBDIR)/libcupsppdc.1.dylib
	$(RM) $(LIBDIR)/libcupsppdc.a
	$(RM) $(LIBDIR)/libcupsppdc.dylib
	$(RM) $(LIBDIR)/libcupsppdc.so
	$(RM) $(LIBDIR)/libcupsppdc.so.1
	-$(RMDIR) $(LIBDIR)
	-if test "x$(privateinclude)" != x; then \
		$(RM) $(PRIVATEINCLUDE)/ppdc.h; \
		$(RMDIR) $(PRIVATEINCLUDE); \
	fi


#
# Local programs (not built when cross-compiling...)
#

local:	genstrings


#
# genstrings - generate GNU gettext strings.
#

genstrings:		genstrings.o libcupsppdc.a ../cups/$(LIBCUPSSTATIC) \
			sample.drv ../data/media.defs
	echo Linking $@...
	$(LD_CXX) $(ARCHFLAGS) $(LDFLAGS) -o genstrings genstrings.o \
		libcupsppdc.a ../cups/$(LIBCUPSSTATIC) $(LIBGSSAPI) $(SSLLIBS) \
		$(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	echo Generating localization strings...
	./genstrings >sample.c


#
# ppdc, the PPD compiler.
#

ppdc:			ppdc.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CXX) $(LDFLAGS) -o $@ ppdc.o -L. -lcupsppdc $(LIBS)


ppdc-static:		ppdc.o libcupsppdc.a ../cups/$(LIBCUPSSTATIC) foo.drv foo-fr.po
	echo Linking $@...
	$(LD_CXX) $(ARCHFLAGS) $(LDFLAGS) -o ppdc-static ppdc.o libcupsppdc.a \
		../cups/$(LIBCUPSSTATIC) $(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) \
		$(COMMONLIBS) $(LIBZ)
	echo Testing PPD compiler...
	./ppdc-static -l en,fr -I ../data foo.drv
	./ppdc-static -l en,fr -z -I ../data foo.drv


#
# ppdhtml, the PPD to HTML utility.
#

ppdhtml:			ppdhtml.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CXX) $(LDFLAGS) -o $@ ppdhtml.o -L. -lcupsppdc $(LIBS)


#
# ppdi, import PPD files.
#

ppdi:			ppdi.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CXX) $(LDFLAGS) -o $@ ppdi.o -L. -lcupsppdc $(LIBS)


ppdi-static:		ppdc-static ppdi.o libcupsppdc.a  ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CXX) $(ARCHFLAGS) $(LDFLAGS) -o ppdi-static ppdi.o libcupsppdc.a \
		../cups/$(LIBCUPSSTATIC) $(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) \
		$(COMMONLIBS) $(LIBZ)
	echo Testing PPD importer...
	$(RM) -r ppd ppd2 sample-import.drv
	./ppdc-static -l en -I ../data sample.drv
	./ppdi-static -I ../data -o sample-import.drv ppd/*
	./ppdc-static -l en -I ../data -d ppd2 sample-import.drv
	if diff -r ppd ppd2 >/dev/null; then \
		echo PPD import OK; \
	else \
		echo PPD import FAILED; \
		exit 1; \
	fi


#
# ppdmerge, merge PPD files.
#

ppdmerge:			ppdmerge.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CXX) $(LDFLAGS) -o $@ ppdmerge.o $(LIBS)


#
# ppdpo, create message catalog files.
#

ppdpo:			ppdpo.o $(LIBCUPSPPDC) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CXX) $(LDFLAGS) -o $@ ppdpo.o -L. -lcupsppdc $(LIBS)


#
# testcatalog, test ppdcCatalog class.
#

testcatalog:		testcatalog.o libcupsppdc.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CXX) $(LDFLAGS) -o $@ testcatalog.o libcupsppdc.a \
		../cups/$(LIBCUPSSTATIC) $(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) \
		$(COMMONLIBS) $(LIBZ)


#
# libcupsppdc.so.1
#

libcupsppdc.so.1:	$(LIBOBJS) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(DSOXX) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LINKCUPS)
	$(RM) `basename $@ .1`
	$(LN) $@ `basename $@ .1`


#
# libcupsppdc.1.dylib
#

libcupsppdc.1.dylib:	$(LIBOBJS) ../cups/$(LIBCUPS)
	echo Creating export list for $@...
	nm -gm $(LIBOBJS) | grep "__text" | grep -v weak | awk '{print $$NF}' | sort >t.exp
	echo Linking $@...
	$(DSOXX) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 1.0.0 \
		-compatibility_version 1.0.0 \
		-exported_symbols_list t.exp \
		$(LIBOBJS) $(LINKCUPS) $(COMMONLIBS)
	$(RM) libcupsppdc.dylib t.exp
	$(LN) $@ libcupsppdc.dylib


#
# libcupsppdc.la
#

libcupsppdc.la:    $(LIBOBJS) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 1:0 $(LINKCUPS)


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
