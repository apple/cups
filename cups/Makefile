#
# "$Id: Makefile,v 1.1 1999/01/24 14:19:48 mike Exp $"
#
#   Support library Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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

#
# libcups.a object files...
#

OBJS	=	emit.o filter.o http.o ipp.o mark.o mime.o page.o ppd.o type.o

#
# Header files to install...
#

HEADERS	=	cups.h http.h ipp.h mime.h ppd.h

#
# Targets in this directory...
#

TARGETS	=	libcups.a testmime testppd testhttp

#
# Make all targets...
#

all:	$(TARGETS)

#
# Remove object and target files...
#

clean:
	rm -f $(OBJS) $(TARGETS)

#
# Install object and target files...
#

install:	all
	-$(MKDIR) $(LIBDIR)
	$(CP) libcups.a $(LIBDIR)
	-$(MKDIR) $(INCDIR)/cups
	$(CP) $(HEADERS) $(INCDIR)/cups

#
# libcups.a
#

libcups.a:	$(OBJS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

emit.o:		ppd.h ../config.h ../Makedefs
filter.o:	mime.h ../config.h ../Makedefs
http.o:		http.h ../config.h ../Makedefs
ipp.o:		http.h ipp.h ../config.h ../Makedefs
mark.o:		ppd.h ../config.h ../Makedefs
mime.o:		mime.h ../config.h ../Makedefs
page.o:		ppd.h ../config.h ../Makedefs
ppd.o:		ppd.h ../config.h ../Makedefs
type.o:		mime.h ../config.h ../Makedefs

#
# testhttp
#

testhttp:	testhttp.o libcups.a
	$(CC) $(LDFLAGS) -o $@ testhttp.o libcups.a
	$(STRIP) $@

testhttp.o:	http.h ../Makedefs

#
# testmime
#

testmime:	testmime.o libcups.a
	$(CC) $(LDFLAGS) -o $@ testmime.o libcups.a
	$(STRIP) $@

testmime.o:	mime.h ../Makedefs

#
# testppd
#

testppd:	testppd.o libppd.a
	$(CC) $(LDFLAGS) -o $@ testppd.o libppd.a

testppd.o:	ppd.h ../Makedefs

#
# End of "$Id: Makefile,v 1.1 1999/01/24 14:19:48 mike Exp $".
#
