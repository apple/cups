#
# "$Id: Makefile,v 1.3 1999/03/06 20:28:10 mike Exp $"
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

TARGETS	=	hpgltops image.a imagetops imagetoraster pstops texttops
HPGLOBJS =	hpgl-attr.o hpgl-config.o hpgl-main.o hpgl-prolog.o \
		hpgl-char.o hpgl-input.o hpgl-polygon.o hpgl-vector.o
IMAGEOBJS =	image-colorspace.o image-photocd.o image-sgilib.o \
		image-tiff.o image-gif.o image-png.o image-sgi.o image-zoom.o \
		image-jpeg.o image-pnm.o image-sun.o
OBJS	=	$(HPGLOBJS) $(IMAGEOBJS) imagetops.o imagetoraster.o \
		pstops.o texttops.o

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

hpgltops:	$(HPGLOBJS) ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ $(HPGLOBJS) $(LIBS)
$(HPGLOBJS):	hpgl2ps.h ../Makedefs

#
# image.a
#

image.a:	$(IMAGEOBJS) ../Makedefs ../cups/libcups.a
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(IMAGEOBJS)
	$(RANLIB) $@
$(IMAGEOBJS):	image.h ../Makedefs

#
# imagetops
#

imagetops:	imagetops.o image.a ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ imagetops.o image.a $(LIBS)
imagetops:	image.h ../Makedefs

#
# imagetoraster
#

imagetoraster:	imagetoraster.o image.a ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ imagetoraster.o image.a $(LIBS)
imagetoraster:	image.h ../Makedefs

#
# pstops
#

pstops:	pstops.o ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ pstops.o $(LIBS)
pstops.o:	../Makedefs

#
# texttops
#

texttops:	texttops.o ../Makedefs ../cups/libcups.a
	$(CC) $(LDFLAGS) -o $@ texttops.o $(LIBS)
texttops.o:	../Makedefs

#
# End of "$Id: Makefile,v 1.3 1999/03/06 20:28:10 mike Exp $".
#
