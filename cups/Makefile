#
# "$Id: Makefile,v 1.21 1999/07/27 11:37:17 mike Exp $"
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
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3111 USA
#
#       Voice: (301) 373-9603
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

#
# Object files...
#

OBJS	=	emit.o filter.o http.o ipp.o language.o mark.o mime.o \
		options.o page.o ppd.o raster.o string.o type.o usersys.o \
		util.o

#
# Header files to install...
#

HEADERS	=	cups.h http.h ipp.h language.h mime.h ppd.h raster.h

#
# Targets in this directory...
#

TARGETS	=	$(LIBCUPS) testmime testppd testhttp

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
	-$(MKDIR) $(INCLUDEDIR)/cups
	$(CP) $(HEADERS) $(INCLUDEDIR)/cups
	-$(MKDIR) $(LIBDIR)
	$(CP) $(LIBCUPS) $(LIBDIR)
	if test $(LIBCUPS) != "libcups.a"; then \
		$(LN) $(LIBCUPS) `basename $(LIBCUPS) .1` \
	fi

#
# libcups.so.1, libcups.sl.1
#

libcups.so.1 libcups.sl.1:	$(OBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $@ $(OBJS)
	-$(LN) $@ `basename $@ .1`

#
# libcups.a
#

libcups.a:	$(OBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

#
# cups_C.h - the default POSIX locale that is compiled in.
#

cups_C.h:	../locale/C/cups_C
	echo Generating $@...
	$(RM) cups_C.h
	$(AWK) '{print "\"" $$0 "\","}' < ../locale/C/cups_C > cups_C.h

emit.o:		ppd.h ../config.h ../Makedefs
filter.o:	mime.h ../config.h ../Makedefs
http.o:		http.h ipp.h string.h ../config.h ../Makedefs
ipp.o:		http.h ipp.h ../config.h ../Makedefs
language.o:	cups_C.h language.h string.h ../config.h ../Makedefs
mark.o:		ppd.h ../config.h ../Makedefs
mime.o:		mime.h ../config.h ../Makedefs
options.o:	cups.h ../config.h ../Makedefs
page.o:		ppd.h ../config.h ../Makedefs
ppd.o:		language.h ppd.h ../config.h ../Makedefs
raster.o:	raster.h ../config.h ../Makedefs
string.o:	string.h ../config.h ../Makedefs
type.o:		mime.h ../config.h ../Makedefs
usersys.o:	cups.h ../config.h ../Makedefs
util.o:		cups.h http.h ipp.h ../config.h ../Makedefs

#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttp.o libcups.a $(NETLIBS)

testhttp.o:	http.h ../Makedefs

#
# testmime (dependency on static CUPS library is intentional)
#

testmime:	testmime.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testmime.o libcups.a

testmime.o:	mime.h ../Makedefs

#
# testppd (dependency on static CUPS library is intentional)
#

testppd:	testppd.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testppd.o libcups.a $(NETLIBS)

testppd.o:	ppd.h ../Makedefs

#
# End of "$Id: Makefile,v 1.21 1999/07/27 11:37:17 mike Exp $".
#
