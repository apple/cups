#
# "$Id: Makefile,v 1.36 2000/08/18 17:24:42 mike Exp $"
#
#   Filter makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2000 by Easy Software Products.
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

include ../Makedefs

TARGETS	=	hpgltops texttops pstops imagetops imagetoraster \
		rastertoepson rastertohp

HPGLOBJS =	hpgl-attr.o hpgl-config.o hpgl-main.o hpgl-prolog.o \
		hpgl-char.o hpgl-input.o hpgl-polygon.o hpgl-vector.o
IMAGEOBJS =	image-bmp.o image-colorspace.o image-gif.o image-jpeg.o \
		image-photocd.o image-pix.o image-png.o image-pnm.o \
		image-sgi.o image-sgilib.o image-sun.o image-tiff.o \
		image-zoom.o image.o raster.o
FORMOBJS =	form-attr.o form-main.o form-ps.o form-text.o form-tree.o
OBJS	=	$(HPGLOBJS) $(IMAGEOBJS) $(FORMOBJS) \
		imagetops.o imagetoraster.o common.o pstops.o raster.o \
		rastertoepson.o rastertohp.o texttops.o textcommon.o


#
# Make all targets...
#

all:	$(TARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(LIBCUPSIMAGE) `basename $(LIBCUPSIMAGE) .2`


#
# Install all targets...
#

install:
	-$(MKDIR) $(SERVERBIN)/filter
	$(CHMOD) ugo+rx $(SERVERBIN)
	$(CHMOD) ugo+rx $(SERVERBIN)/filter
	$(INSTALL_BIN) $(TARGETS) $(SERVERBIN)/filter
	-$(MKDIR) $(INCLUDEDIR)/cups
	$(CHMOD) ugo+rx $(INCLUDEDIR)
	$(CHMOD) ugo+rx $(INCLUDEDIR)/cups
	$(INSTALL_DATA) raster.h $(INCLUDEDIR)/cups
	-$(MKDIR) $(LIBDIR)
	$(CHMOD) ugo+rx $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSIMAGE) $(LIBDIR)
	-if test $(LIBCUPSIMAGE) != "libcupsimage.a" -a $(LIBCUPSIMAGE) != "libcupsimage.la"; then \
		$(RM) `basename $(LIBCUPSIMAGE) .2`; \
		$(LN) $(LIBCUPSIMAGE) `basename $(LIBCUPSIMAGE) .2`; \
	fi


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
# libcupsimage.so.2, libcupsimage.sl.2
#

libcupsimage.so.2 libcupsimage.sl.2:	$(IMAGEOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $@ $(IMAGEOBJS) $(DSOLIBS) -lm
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# libcupsimage.la
#

libcupsimage.la:       $(IMAGEOBJS) ../Makedefs
	echo Linking $@...
	$(CC) -o $@ $(IMAGEOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 2:0


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
imagetops:	common.h image.h


#
# imagetoraster
#

imagetoraster:	imagetoraster.o common.o $(LIBCUPSIMAGE) ../Makedefs \
		../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ imagetoraster.o common.o $(LINKCUPSIMAGE) \
		$(IMGLIBS) $(LIBS)
imagetoraster:	common.h image.h raster.h


#
# pstops
#

pstops:	pstops.o common.o ../Makedefs ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ pstops.o common.o $(LIBS)
pstops.o:	common.h


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
# End of "$Id: Makefile,v 1.36 2000/08/18 17:24:42 mike Exp $".
#
