#
# "$Id: Makefile,v 1.41.2.9 2002/03/07 18:58:42 mike Exp $"
#
#   Filter makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2002 by Easy Software Products.
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
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs

TARGETS	=	hpgltops texttops pstops imagetops imagetoraster \
		rastertodymo rastertoepson rastertohp testraster

HPGLOBJS =	hpgl-attr.o hpgl-config.o hpgl-main.o hpgl-prolog.o \
		hpgl-char.o hpgl-input.o hpgl-polygon.o hpgl-vector.o
IMAGEOBJS =	image-bmp.o image-colorspace.o image-gif.o image-jpeg.o \
		image-photocd.o image-pix.o image-png.o image-pnm.o \
		image-sgi.o image-sgilib.o image-sun.o image-tiff.o \
		image-zoom.o image.o raster.o
FORMOBJS =	form-attr.o form-main.o form-ps.o form-text.o form-tree.o
OBJS	=	$(HPGLOBJS) $(IMAGEOBJS) $(FORMOBJS) \
		imagetops.o imagetoraster.o common.o pstops.o raster.o \
		rastertodymo.o rastertoepson.o rastertohp.o texttops.o \
		textcommon.o testraster.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(LIBCUPSIMAGE) `basename $(LIBCUPSIMAGE) .3`


#
# Install all targets...
#

install:
	$(INSTALL_DIR) $(SERVERBIN)/filter
	for file in $(TARGETS); do \
		$(INSTALL_BIN) $$file $(SERVERBIN)/filter; \
	done
	$(INSTALL_DIR) $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSIMAGE) $(LIBDIR)
	-if test $(LIBCUPSIMAGE) = "libcupsimage.so.3" -o $(LIBCUPSIMAGE) = "libcupsimage.sl.3"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .3`; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .3`; \
	fi
	-if test $(LIBCUPSIMAGE) = "libcupsimage.3.dylib"; then \
		$(RM) $(LIBDIR)/libcupsimage.dylib; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/libcupsimage.dylib; \
	fi
	$(INSTALL_DIR) $(INCLUDEDIR)/cups
	$(INSTALL_DATA) image.h $(INCLUDEDIR)/cups
	$(INSTALL_DATA) raster.h $(INCLUDEDIR)/cups


#
# formtops
#

formtops:	$(FORMOBJS) common.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(FORMOBJS) common.o $(LIBS) -lm
$(FORMOBJS):	form.h


#
# hpgltops
#

hpgltops:	$(HPGLOBJS) common.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(HPGLOBJS) common.o $(LIBS) -lm
$(HPGLOBJS):	hpgltops.h


#
# libcupsimage.so.3, libcupsimage.sl.3
#

libcupsimage.so.3 libcupsimage.sl.3:	$(IMAGEOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ $(IMAGEOBJS) $(DSOLIBS) -lm
	$(RM) `basename $@ .3`
	$(LN) $@ `basename $@ .3`


#
# libcupsimage.3.dylib
#

libcupsimage.3.dylib:	$(IMAGEOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ \
		-dylib_install_name $(LIBDIR)/libcupsimage.dylib \
		-dylib_current_version 3.0.0 \
		-dylib_compatibility_version 3.0.0 \
		$(IMAGEOBJS) $(DSOLIBS) $(LINKCUPS) -lm -lcc_dynamic
	$(RM) libcupsimage.dylib
	$(LN) $@ libcupsimage.dylib


#
# libcupsimage_s.a
#

libcupsimage_s.a:	$(IMAGEOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o libcupsimage_s.o $(IMAGEOBJS) $(DSOLIBS) \
		$(LINKCUPS) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupsimage_s.o


#
# libcupsimage.la
#

libcupsimage.la:       $(IMAGEOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) -o $(DSOFLAGS) $@ $(IMAGEOBJS:.o=.lo) $(DSOLIBS) \
		-rpath $(LIBDIR) -version-info 2:0


#
# libcupsimage.a
#

libcupsimage.a:	$(IMAGEOBJS) ../Makedefs
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(IMAGEOBJS)
	$(RANLIB) $@

$(IMAGEOBJS):	image.h
raster.o:	raster.h


#
# imagetops
#

imagetops:	imagetops.o common.o $(LIBCUPSIMAGE) ../Makedefs \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ imagetops.o common.o $(LINKCUPSIMAGE) \
		$(IMGLIBS) $(LIBS)
imagetops.o:	common.h image.h


#
# imagetoraster
#

imagetoraster:	imagetoraster.o common.o $(LIBCUPSIMAGE) ../Makedefs \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ imagetoraster.o common.o $(LINKCUPSIMAGE) \
		$(IMGLIBS) $(LIBS)
imagetoraster.o:	common.h image.h raster.h


#
# pstops
#

pstops:	pstops.o common.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ pstops.o common.o $(LIBS)
pstops.o:	common.h


#
# rastertodymo
#

rastertodymo:	rastertodymo.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertodymo.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
rastertodymo.o:	raster.h


#
# rastertoepson
#

rastertoepson:	rastertoepson.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertoepson.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
rastertoepson.o:	raster.h


#
# rastertohp
#

rastertohp:	rastertohp.o ../Makedefs ../cups/$(LIBCUPS) $(LIBCUPSIMAGE)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ rastertohp.o $(LINKCUPSIMAGE) $(IMGLIBS) $(LIBS)
rastertohp.o:	raster.h


#
# testraster
#

testraster:	testraster.o raster.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testraster.o raster.o
testraster.o:	raster.h


#
# texttops
#

texttops:	texttops.o textcommon.o common.o ../Makedefs \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ texttops.o textcommon.o common.o $(LIBS)
texttops.o:	common.h textcommon.h

common.o:	common.h
textcommon.o:	textcommon.h common.h


$(OBJS):	../Makedefs ../cups/cups.h ../cups/ppd.h ../cups/language.h


#
# End of "$Id: Makefile,v 1.41.2.9 2002/03/07 18:58:42 mike Exp $".
#
