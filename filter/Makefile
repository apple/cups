#
# "$Id: Makefile,v 1.6 1999/03/24 13:59:46 mike Exp $"
#
#   Filter makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products.
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
#       44145 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

#TARGETS	=	hpgltops \
#			image.a imagetops imagetoraster \
#			pstops \
#			texttoescp texttopcl texttops
TARGETS	=	hpgltops texttops pstops
HPGLOBJS =	hpgl-attr.o hpgl-config.o hpgl-main.o hpgl-prolog.o \
		hpgl-char.o hpgl-input.o hpgl-polygon.o hpgl-vector.o
IMAGEOBJS =	image-colorspace.o image-photocd.o image-sgilib.o \
		image-tiff.o image-gif.o image-png.o image-sgi.o image-zoom.o \
		image-jpeg.o image-pnm.o image-sun.o
OBJS	=	$(HPGLOBJS) $(IMAGEOBJS) imagetops.o imagetoraster.o \
		common.o pstops.o texttops.o textcommon.o

#
# Make all targets...
#

all:	$(TARGETS)

#
# Clean all object files...
#

clean:
	rm -f $(OBJS) $(TARGETS)

#
# Install all targets...
#

install:

#
# hpgltops
#

hpgltops:	$(HPGLOBJS) common.o ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ $(HPGLOBJS) common.o $(LIBS) -lm
$(HPGLOBJS):	hpgltops.h

#
# image.a
#

image.a:	$(IMAGEOBJS) ../Makedefs ../cups/libcups.a
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(IMAGEOBJS)
	$(RANLIB) $@
$(IMAGEOBJS):	image.h

#
# imagetops
#

imagetops:	imagetops.o common.o image.a ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ imagetops.o common.o image.a $(LIBS)
imagetops:	common.h image.h

#
# imagetoraster
#

imagetoraster:	imagetoraster.o common.o image.a ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ imagetoraster.o common.o image.a $(LIBS)
imagetoraster:	common.h image.h

#
# pstops
#

pstops:	pstops.o ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ pstops.o $(LIBS)

#
# texttops
#

texttops:	texttops.o textcommon.o common.o ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ texttops.o textcommon.o common.o $(LIBS)
texttops.o:	common.h textcommon.h

common.o:	common.h
textcommon.o:	textcommon.h common.h
$(OBJS):	../Makedefs ../cups/cups.h ../cups/ppd.h ../cups/language.h

#
# End of "$Id: Makefile,v 1.6 1999/03/24 13:59:46 mike Exp $".
#
