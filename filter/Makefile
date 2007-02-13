#
# "$Id$"
#
#   Filter makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs


FILTERS	=	gziptoany hpgltops texttops pstops $(IMGFILTERS) \
		rastertolabel rastertoepson rastertohp
TARGETS	=	$(FILTERS) \
		$(LIBCUPSIMAGE) \
		libcupsimage.a \
		$(LIB32CUPSIMAGE) \
		$(LIB64CUPSIMAGE) \
		rasterbench \
		testimage \
		testraster

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
		gziptoany.o imagetops.o imagetoraster.o common.o pstops.o \
		rasterbench.o rastertoepson.o rastertohp.o rastertolabel.o \
		testimage.o testraster.o textcommon.o texttops.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)
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
	sed -r -e '1,$$s/^([^.]+)\.o:/\1\.32.o: \1\.c /' Dependencies.tmp >>Dependencies
	sed -r -e '1,$$s/^([^.]+)\.o:/\1\.64.o: \1\.c /' Dependencies.tmp >>Dependencies
	$(RM) Dependencies.tmp


#
# Install all targets...
#

install:	all installhdrs $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/filter
	for file in $(FILTERS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/filter; \
	done
	$(RM) $(SERVERBIN)/filter/rastertodymo
	$(LN) rastertolabel $(SERVERBIN)/filter/rastertodymo
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSIMAGE) $(LIBDIR)
	-if test $(LIBCUPSIMAGE) = "libcupsimage.so.2" -o $(LIBCUPSIMAGE) = "libcupsimage.sl.2"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .2`; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .2`; \
	fi
	-if test $(LIBCUPSIMAGE) = "libcupsimage.2.dylib"; then \
		$(STRIP) -x $(LIBDIR)/$(LIBCUPSIMAGE); \
		$(RM) $(LIBDIR)/libcupsimage.dylib; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/libcupsimage.dylib; \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) libcupsimage.a $(LIBDIR);
	$(RANLIB) $(LIBDIR)/libcupsimage.a;

installhdrs:
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) image.h $(INCLUDEDIR)/cups
	$(INSTALL_DATA) raster.h $(INCLUDEDIR)/cups

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
	$(RM) $(INCLUDEDIR)/cups/raster.h
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
		--intro api-raster.shtml \
		raster.h interpret.c raster.c >../doc/help/api-raster.html


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

libcupsimage.2.dylib:	$(IMAGEOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 2.3.0 \
		-compatibility_version 2.0.0 \
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
	$(CC) $(LDFLAGS) -o $@ testraster.o libcupsimage.a ../cups/libcups.a \
		$(IMGLIBS) $(DSOLIBS) $(LIBS)


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
# End of "$Id$".
#
