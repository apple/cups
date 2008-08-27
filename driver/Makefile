#
# "$Id$"
#
#   Makefile for the CUPS base drivers.
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 2002-2005 by Easy Software Products.
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

LIBOBJS	= \
		attr.o \
		check.o \
		cmyk.o \
		dither.o \
		lut.o \
		pack.o \
		rgb.o \
		srgb.o
LIB32OBJS	= $(LIBOBJS:.o=.32.o)
LIB64OBJS	= $(LIBOBJS:.o=.64.o)

OBJS		= \
		commandtoescpx.o \
		commandtopclx.o \
		rastertoescpx.o \
		rastertopclx.o \
		pcl-common.o \
		testcmyk.o \
		testdither.o \
		testrgb.o \
		$(LIBOBJS) \
		$(LIB32OBJS) \
		$(LIB64OBJS)

LIBTARGETS =	\
		$(LIBCUPSDRIVER) \
		$(LIB32CUPSDRIVER) \
		$(LIB64CUPSDRIVER) \
		libcupsdriver.a
UNITTARGETS =	\
		testcmyk \
		testdither \
		testrgb
TARGETS =	\
		$(LIBTARGETS) \
		commandtoescpx \
		commandtopclx \
		rastertoescpx \
		rastertopclx


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
	$(RM) *.bck core.*
	$(RM) $(TARGETS) $(UNITTARGETS)
	$(RM) -r test
	$(RM) libcupsdriver.so libcupsdriver.sl libcupsdriver.dylib
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


#
# Install programs...
#

install-exec:
	$(INSTALL_DIR) $(SERVERBIN)/filter
	for file in commandtoescpx commandtopclx rastertoescpx rastertopclx; do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/filter; \
	done
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in commandtoescpx commandtopclx rastertoescpx rastertopclx; do \
			cp $$file $(SYMROOT); \
		done \
	fi


#
# Install headers...
#

install-headers:
	echo Installing header files in $(INCLUDEDIR)/cups...
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) driver.h $(INCLUDEDIR)/cups


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	echo Installing libraries in $(LIBDIR)...
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSDRIVER) $(LIBDIR)
	if test $(LIBCUPSDRIVER) = "libcupsdriver.so.1" -o $(LIBCUPSDRIVER) = "libcupsdriver.sl.1"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSDRIVER) .1`; \
		$(LN) $(LIBCUPSDRIVER) $(LIBDIR)/`basename $(LIBCUPSDRIVER) .1`; \
	fi
	if test $(LIBCUPSDRIVER) = "libcupsdriver.1.dylib"; then \
		$(RM) $(LIBDIR)/libcupsdriver.dylib; \
		$(LN) $(LIBCUPSDRIVER) $(LIBDIR)/libcupsdriver.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPSDRIVER) $(SYMROOT); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) libcupsdriver.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsdriver.a

install32bit:
	echo Installing libraries in $(LIB32DIR)...
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcupsdriver.so.1 $(LIB32DIR)/libcupsdriver.so.1
	$(LN) libcupsdriver.so.1 $(LIB32DIR)/libcupsdriver.so

install64bit:
	echo Installing libraries in $(LIB64DIR)...
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcupsdriver.so.1 $(LIB64DIR)/libcupsdriver.so.1
	$(LN) libcupsdriver.so.1 $(LIB64DIR)/libcupsdriver.so


#
# Uninstall...
#

uninstall: $(UNINSTALL32) $(UNINSTALL64)
	for file in commandtoescpx commandtopclx rastertoescpx rastertopclx; do \
		$(RM) $(SERVERBIN)/filter/$$file; \
	done
	$(RM) $(LIBDIR)/libcupsdriver.1.dylib
	$(RM) $(LIBDIR)/libcupsdriver.a
	$(RM) $(LIBDIR)/libcupsdriver.dylib
	$(RM) $(LIBDIR)/libcupsdriver_s.a
	$(RM) $(LIBDIR)/libcupsdriver.sl
	$(RM) $(LIBDIR)/libcupsdriver.sl.1
	$(RM) $(LIBDIR)/libcupsdriver.so
	$(RM) $(LIBDIR)/libcupsdriver.so.1
	-$(RMDIR) $(LIBDIR)
	$(RM) $(INCLUDEDIR)/cups/driver.h
	-$(RMDIR) $(INCLUDEDIR)/cups

uninstall32bit:
	$(RM) $(LIB32DIR)/libcupsdriver.so
	$(RM) $(LIB32DIR)/libcupsdriver.so.1
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcupsdriver.so
	$(RM) $(LIB64DIR)/libcupsdriver.so.1
	-$(RMDIR) $(LIB64DIR)


#
# Automatic API help files...
#

apihelp:
	mxmldoc --section "Programming" \
		--title "Printer Driver API" \
		--css ../doc/cups-printable.css \
		--header api-driver.header --intro api-driver.shtml \
		driver.h $(LIBOBJS:.o=.c) >../doc/help/api-driver.html

framedhelp:
	mxmldoc --framed api-driver \
		--section "Programming" \
		--title "Printer Driver API" \
		--css ../doc/cups-printable.css \
		--header api-driver.header --intro api-driver.shtml \
		driver.h $(LIBOBJS:.o=.c)


#
# commandtopclx, the PCL command printer driver.
#

commandtopclx:		commandtopclx.o $(LIBCUPSDRIVER) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ commandtopclx.o -L. -lcupsdriver $(LIBS)


#
# commandtoescpx, the ESC/P command printer driver.
#

commandtoescpx:		commandtoescpx.o $(LIBCUPSDRIVER) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ commandtoescpx.o -L. -lcupsdriver $(LIBS)


#
# rastertoescpx, the ESC/P raster printer driver.
#

rastertoescpx:		rastertoescpx.o $(LIBCUPSDRIVER) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertoescpx.o -L. -lcupsdriver \
		$(LINKCUPSIMAGE) $(LIBS)


#
# rastertopclx, the ESC/P raster printer driver.
#

rastertopclx:		rastertopclx.o pcl-common.o $(LIBCUPSDRIVER) \
			../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertopclx.o pcl-common.o -L. -lcupsdriver \
		$(LINKCUPSIMAGE) $(LIBS)


#
# test, make a common test subdirectory for the other test programs.
#

test:
	if test ! -d test; then \
		rm -rf test; \
		mkdir test; \
	fi


#
# testcmyk, test cmyk separation functions.
#

testcmyk:		test testcmyk.o libcupsdriver.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testcmyk.o libcupsdriver.a \
		../cups/libcups.a $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	echo Running CMYK API tests...
	./testcmyk > test/testcmyk.log


#
# testdither, test dithering functions.
#

testdither:		test testdither.o libcupsdriver.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testdither.o libcupsdriver.a \
		../cups/libcups.a $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	echo Running dither API tests...
	./testdither > test/0-255.pgm 2>test/0-255.log
	./testdither 0 127 255 > test/0-127-255.pgm 2>test/0-127-255.log
	./testdither 0 85 170 255 > test/0-85-170-255.pgm 2>test/0-85-170-255.log
	./testdither 0 63 127 170 198 227 255 > test/0-63-127-170-198-227-255.pgm 2>test/0-63-127-170-198-227-255.log
	./testdither 0 210 383 > test/0-210-383.pgm 2>test/0-210-383.log
	./testdither 0 82 255 > test/0-82-255.pgm 2>test/0-82-255.log
	./testdither 0 510 > test/0-510.pgm 2>test/0-510.log
	./testdither 0 1020 > test/0-1020.pgm 2>test/0-1020.log


#
# testrgb, test RGB separation functions.
#

testrgb:		test testrgb.o libcupsdriver.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testrgb.o libcupsdriver.a \
		../cups/libcups.a $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	echo Running RGB API tests...
	./testrgb > test/testrgb.log 2>&1 || echo "RGB tests failed!"


#
# libcupsdriver.so.1, libcupsdriver.sl.1
#

libcupsdriver.so.1 libcupsdriver.sl.1:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LIBS)
	$(RM) `basename $@ .1`
	$(LN) $@ `basename $@ .1`


#
# 32bit/libcupsdriver.so.1
#

32bit/libcupsdriver.so.1:	$(LIB32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSO) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(LIB32OBJS) $(LIBS)
	$(RM) 32bit/libcupsdriver.so
	$(LN) libcupsdriver.so.1 32bit/libcupsdriver.so


#
# 64bit/libcupsdriver.so.1
#

64bit/libcupsdriver.so.1:	$(LIB64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSO) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(LIB64OBJS) $(LIBS)
	$(RM) 64bit/libcupsdriver.so
	$(LN) libcupsdriver.so.1 64bit/libcupsdriver.so


#
# libcupsdriver.1.dylib
#

libcupsdriver.1.dylib:	$(LIBOBJS) libcupsdriver.exp
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 1.0.0 \
		-compatibility_version 1.0.0 \
		-exported_symbols_list libcupsdriver.exp \
		$(LIBOBJS) $(LIBS)
	$(RM) libcupsdriver.dylib
	$(LN) $@ libcupsdriver.dylib


#
# libcupsdriver_s.a
#

libcupsdriver_s.a:	$(LIBOBJS)
	echo Creating $@...
	$(DSO) $(DSOFLAGS) -o libcupsdriver_s.o $(LIBOBJS) $(LIBS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupsdriver_s.o


#
# libcupsdriver.la
#

libcupsdriver.la:    $(LIBOBJS)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 1:0 $(LIBS)


#
# libcupsdriver.a
#

libcupsdriver.a:	$(LIBOBJS)
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
