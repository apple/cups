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

OBJS		= \
		commandtoescpx.o \
		commandtopclx.o \
		rastertoescpx.o \
		rastertopclx.o \
		pcl-common.o \
		testcmyk.o \
		testdither.o \
		testrgb.o \
		$(LIBOBJS)

TARGETS		= \
		libcupsdriver.a \
		commandtoescpx \
		commandtopclx \
		rastertoescpx \
		rastertopclx \
		testcmyk \
		testdither \
		testrgb


#
# Make everything...
#

all:		$(TARGETS)


#
# Clean everything...
#

clean:
	$(RM) $(OBJS) core
	$(RM) *.bck core.*
	$(RM) $(TARGETS)
	$(RM) -r images


#
# Update dependencies...
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install...
#

install:
	$(INSTALL_DIR) $(SERVERBIN)/filter
	for file in commandtoescpx commandtopclx rastertoescpx rastertopclx; do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/filter; \
	done
	$(INSTALL_DIR) $(INCLUDEDIR)/cups
	$(INSTALL_DATA) driver.h $(INCLUDEDIR)/cups


#
# Uninstall...
#

uninstall:
	for file in commandtoescpx commandtopclx rastertoescpx rastertopclx; do \
		$(RM) $(SERVERBIN)/filter/$$file; \
	done
	$(RM) $(INCLUDEDIR)/cups/driver.h


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

commandtopclx:		commandtopclx.o libcupsdriver.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ commandtopclx.o libcupsdriver.a $(LIBS)


#
# commandtoescpx, the ESC/P command printer driver.
#

commandtoescpx:		commandtoescpx.o libcupsdriver.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ commandtoescpx.o libcupsdriver.a $(LIBS)


#
# rastertoescpx, the ESC/P raster printer driver.
#

rastertoescpx:		rastertoescpx.o libcupsdriver.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertoescpx.o libcupsdriver.a \
		$(LINKCUPSIMAGE) $(LIBS)


#
# rastertopclx, the ESC/P raster printer driver.
#

rastertopclx:		rastertopclx.o pcl-common.o libcupsdriver.a \
			../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertopclx.o pcl-common.o libcupsdriver.a \
		$(LINKCUPSIMAGE) $(LIBS)


#
# testcmyk, test cmyk separation functions.
#

testcmyk:		testcmyk.o libcupsdriver.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testcmyk.o libcupsdriver.a ../cups/libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	./testcmyk


#
# testdither, test dithering functions.
#

testdither:		testdither.o libcupsdriver.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testdither.o libcupsdriver.a ../cups/libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	if test ! -d images; then \
		mkdir images; \
	fi
	./testdither > images/0-255.pgm
	./testdither 0 127 255 > images/0-127-255.pgm
	./testdither 0 85 170 255 > images/0-85-170-255.pgm
	./testdither 0 63 127 170 198 227 255 > images/0-63-127-170-198-227-255.pgm
	./testdither 0 210 383 > images/0-210-383.pgm
	./testdither 0 82 255 > images/0-82-255.pgm
	./testdither 0 510 > images/0-510.pgm
	./testdither 0 1020 > images/0-1020.pgm


#
# testrgb, test RGB separation functions.
#

testrgb:		testrgb.o libcupsdriver.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testrgb.o libcupsdriver.a ../cups/libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	./testrgb


#
# libcupsdriver.a, the CUPS driver library...
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
