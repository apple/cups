#
# "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $"
#
#   Filter makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2009 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs


FILTERS	=	\
		$(BANNERTOPS) \
		commandtops \
		gziptoany \
		hpgltops \
		$(TEXTTOPS) \
		pstops \
		$(IMGFILTERS) \
		$(PDFTOPS) \
		rastertolabel \
		rastertoepson \
		rastertohp
LIBTARGETS =	\
		$(LIBCUPSIMAGE) \
		libcupsimage.a \
		$(LIB32CUPSIMAGE) \
		$(LIB64CUPSIMAGE)
UNITTARGETS =	\
		rasterbench \
		testimage \
		testraster
TARGETS	=	\
		$(LIBTARGETS) \
		$(FILTERS)

HPGLOBJS =	hpgl-attr.o hpgl-config.o hpgl-main.o hpgl-prolog.o \
		hpgl-char.o hpgl-input.o hpgl-polygon.o hpgl-vector.o
IMAGEOBJS =	image-bmp.o image-colorspace.o image-gif.o image-jpeg.o \
		image-photocd.o image-pix.o image-png.o image-pnm.o \
		image-sgi.o image-sgilib.o image-sun.o image-tiff.o \
		image-zoom.o image.o error.o interpret.o raster.o
IMAGE32OBJS =	$(IMAGEOBJS:.o=.32.o)
IMAGE64OBJS =	$(IMAGEOBJS:.o=.64.o)
FORMOBJS =	form-attr.o form-main.o form-ps.o form-text.o form-tree.o
OBJS	=	$(HPGLOBJS) $(IMAGEOBJS) $(FORMOBJS) \
		bannertops.o commandtops.o gziptoany.o imagetops.o \
		imagetoraster.o common.o pdftops.o pstext.o pstops.o \
		rasterbench.o rastertoepson.o rastertohp.o rastertolabel.o \
		testimage.o testraster.o textcommon.o texttops.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Make library targets...
#

libs:		$(LIBTARGETS)


#
# Make unit tests...
#

unittests:	$(UNITTARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTARGETS)
	$(RM) libcupsimage.so libcupsimage.sl libcupsimage.dylib
	$(RM) -r 32bit 64bit


#
# Update dependencies (without system header dependencies...)
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
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/filter
	for file in $(FILTERS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/filter; \
	done
	$(RM) $(SERVERBIN)/filter/rastertodymo
	$(LN) rastertolabel $(SERVERBIN)/filter/rastertodymo
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
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) image.h $(INCLUDEDIR)/cups


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSIMAGE) $(LIBDIR)
	-if test $(LIBCUPSIMAGE) = "libcupsimage.so.2" -o $(LIBCUPSIMAGE) = "libcupsimage.sl.2"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .2`; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .2`; \
	fi
	-if test $(LIBCUPSIMAGE) = "libcupsimage.2.dylib"; then \
		$(RM) $(LIBDIR)/libcupsimage.dylib; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/libcupsimage.dylib; \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) -m 755 libcupsimage.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsimage.a
	$(CHMOD) 555 $(LIBDIR)/libcupsimage.a

install32bit:
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcupsimage.so.2 $(LIB32DIR)/libcupsimage.so.2
	$(LN) libcupsimage.so.2 $(LIB32DIR)/libcupsimage.so

install64bit:
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcupsimage.so.2 $(LIB64DIR)/libcupsimage.so.2
	$(LN) libcupsimage.so.2 $(LIB64DIR)/libcupsimage.so


#
# Uninstall all targets...
#

uninstall: $(UNINSTALL32) $(UNINSTALL64)
	for file in $(FILTERS); do \
		$(RM) $(SERVERBIN)/filter/$$file; \
	done
	$(RM) $(SERVERBIN)/filter/rastertodymo
	-$(RMDIR) $(SERVERBIN)/filter
	-$(RMDIR) $(SERVERBIN)
	$(RM) $(LIBDIR)/libcupsimage.2.dylib
	$(RM) $(LIBDIR)/libcupsimage.a
	$(RM) $(LIBDIR)/libcupsimage.dylib
	$(RM) $(LIBDIR)/libcupsimage_s.a
	$(RM) $(LIBDIR)/libcupsimage.sl
	$(RM) $(LIBDIR)/libcupsimage.sl.2
	$(RM) $(LIBDIR)/libcupsimage.so
	$(RM) $(LIBDIR)/libcupsimage.so.2
	-$(RMDIR) $(LIBDIR)
	$(RM) $(INCLUDEDIR)/cups/image.h
	-$(RMDIR) $(INCLUDEDIR)/cups

uninstall32bit:
	$(RM) $(LIB32DIR)/libcupsimage.so
	$(RM) $(LIB32DIR)/libcupsimage.so.2
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcupsimage.so
	$(RM) $(LIB64DIR)/libcupsimage.so.2
	-$(RMDIR) $(LIB64DIR)


#
# Automatic API help files...
#

apihelp:
	echo Generating CUPS API help files...
	mxmldoc --section "Programming" --title "Raster API" \
		--css ../doc/cups-printable.css \
		--header api-raster.header --intro api-raster.shtml \
		api-raster.xml \
		../cups/raster.h interpret.c raster.c \
		>../doc/help/api-raster.html
	mxmldoc --tokens help/api-raster.html api-raster.xml >../doc/help/api-raster.tokens
	$(RM) api-raster.xml
	mxmldoc --section "Programming" \
		--title "Developing PostScript Printer Drivers" \
		--css ../doc/cups-printable.css \
		--header postscript-driver.header \
		--intro postscript-driver.shtml \
		>../doc/help/postscript-driver.html
	mxmldoc --section "Programming" \
		--title "Introduction to the PPD Compiler" \
		--css ../doc/cups-printable.css \
		--header ppd-compiler.header \
		--intro ppd-compiler.shtml \
		>../doc/help/ppd-compiler.html
	mxmldoc --section "Programming" \
		--title "Developing Raster Printer Drivers" \
		--css ../doc/cups-printable.css \
		--header raster-driver.header \
		--intro raster-driver.shtml \
		>../doc/help/raster-driver.html

framedhelp:
	echo Generating CUPS API help files...
	mxmldoc --section "Programming" --title "Raster API" \
		--framed ../cups/api-raster \
		--css ../doc/cups-printable.css \
		--header api-raster.header --intro api-raster.shtml \
		../cups/raster.h interpret.c raster.c
	mxmldoc --section "Programming" \
		--title "Developing PostScript Printer Drivers" \
		--framed ../cups/postscript-driver \
		--css ../doc/cups-printable.css \
		--header postscript-driver.header \
		--intro postscript-driver.shtml
	mxmldoc --section "Programming" \
		--title "Introduction to the PPD Compiler" \
		--framed ../cups/ppd-compiler \
		--css ../doc/cups-printable.css \
		--header ppd-compiler.header \
		--intro ppd-compiler.shtml
	mxmldoc --section "Programming" \
		--title "Developing Raster Printer Drivers" \
		--framed ../cups/raster-driver \
		--css ../doc/cups-printable.css \
		--header raster-driver.header \
		--intro raster-driver.shtml


#
# bannertops
#

bannertops:	bannertops.o pstext.o common.o ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ bannertops.o pstext.o common.o $(LINKCUPSIMAGE) \
		$(IMGLIBS) $(LIBS)


#
# commandtops
#

commandtops:	commandtops.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ commandtops.o $(LIBS)


#
# formtops
#

formtops:	$(FORMOBJS) common.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(FORMOBJS) common.o $(LIBS) -lm


#
# gziptoany
#

gziptoany:	gziptoany.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ gziptoany.o $(LIBZ) $(LIBS)


#
# hpgltops
#

hpgltops:	$(HPGLOBJS) common.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(HPGLOBJS) common.o $(LIBS) -lm


#
# libcupsimage.so.2, libcupsimage.sl.2
#

libcupsimage.so.2 libcupsimage.sl.2:	$(IMAGEOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(IMAGEOBJS) $(DSOLIBS) \
		-L../cups $(LINKCUPS) -lm
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# 32bit/libcupsimage.so.2
#

32bit/libcupsimage.so.2:	$(IMAGE32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSO) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(IMAGE32OBJS) $(DSOLIBS) \
		-L../cups/32bit $(LINKCUPS) -lm


#
# 64bit/libcupsimage.so.2
#

64bit/libcupsimage.so.2:	$(IMAGE64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSO) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(IMAGE64OBJS) $(DSOLIBS) \
		-L../cups/64bit $(LINKCUPS) -lm


#
# libcupsimage.2.dylib
#

libcupsimage.2.dylib:	$(IMAGEOBJS) $(LIBCUPSIMAGEORDER)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 2.3.0 \
		-compatibility_version 2.0.0 \
		-sectorder __TEXT __text $(LIBCUPSIMAGEORDER) \
		$(IMAGEOBJS) $(DSOLIBS) -L../cups $(LINKCUPS) -lm
	$(RM) libcupsimage.dylib
	$(LN) $@ libcupsimage.dylib


#
# libcupsimage_s.a
#

libcupsimage_s.a:	$(IMAGEOBJS) libcupsimage_s.exp
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -Wl,-berok,-bexport:libcupsimage_s.exp \
		-o libcupsimage_s.o $(IMAGEOBJS) $(DSOLIBS) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupsimage_s.o


#
# libcupsimage.la
#

libcupsimage.la:       $(IMAGEOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(IMAGEOBJS:.o=.lo) $(DSOLIBS) \
		-L../cups $(LINKCUPS) \
		-rpath $(LIBDIR) -version-info 2:3


#
# libcupsimage.a
#

libcupsimage.a:	$(IMAGEOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(IMAGEOBJS)
	$(RANLIB) $@


#
# testimage
#

testimage:	testimage.o libcupsimage.a ../Makedefs
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testimage.o libcupsimage.a \
		$(IMGLIBS) $(DSOLIBS) $(LIBS)


#
# imagetops
#

imagetops:	imagetops.o common.o $(LIBCUPSIMAGE) \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ imagetops.o common.o $(LINKCUPSIMAGE) \
		$(IMGLIBS) $(LIBS)


#
# imagetoraster
#

imagetoraster:	imagetoraster.o common.o $(LIBCUPSIMAGE) \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ imagetoraster.o common.o $(LINKCUPSIMAGE) \
		$(IMGLIBS) $(LIBS)


#
# pstops
#

pstops:	pstops.o common.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ pstops.o common.o $(LIBS) -lm


#
# pdftops
#

pdftops:	pdftops.o common.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ pdftops.o common.o $(LIBS)


#
# rastertolabel
#

rastertolabel:	rastertolabel.o ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertolabel.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)


#
# rastertoepson
#

rastertoepson:	rastertoepson.o ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertoepson.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)


#
# rastertohp
#

rastertohp:	rastertohp.o ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertohp.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)


#
# testraster
#

testraster:	testraster.o ../cups/libcups.a libcupsimage.a
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testraster.o libcupsimage.a \
		../cups/libcups.a $(IMGLIBS) $(DSOLIBS) $(COMMONLIBS) \
		$(SSLLIBS) $(DNSSDLIBS) $(LIBGSSAPI)
	echo Running raster API tests...
	./testraster


#
# rasterbench
#

rasterbench:	rasterbench.o libcupsimage.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rasterbench.o libcupsimage.a $(LIBS)


#
# texttops
#

texttops:	texttops.o textcommon.o common.o \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ texttops.o textcommon.o common.o $(LIBS)


#
# Dependencies...
#
  
include Dependencies


#
# End of "$Id: Makefile 7871 2008-08-27 21:12:43Z mike $".
#
