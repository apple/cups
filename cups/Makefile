#
# "$Id: Makefile,v 1.53 2001/03/09 20:06:06 mike Exp $"
#
#   Support library Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2001 by Easy Software Products, all rights reserved.
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

LIBOBJS	=	dest.o emit.o encode.o http.o ipp.o language.o mark.o md5.o \
		md5passwd.o options.o page.o ppd.o snprintf.o string.o \
		tempfile.o usersys.o util.o
OBJS	=	$(LIBOBJS) testhttp.o testppd.o


#
# Header files to install...
#

HEADERS	=	cups.h http.h ipp.h language.h md5.h ppd.h


#
# Targets in this directory...
#

TARGETS	=	$(LIBCUPS) libcups.a


#
# Make all targets...
#

all:	$(TARGETS)


#
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) `basename $(LIBCUPS) .2`


#
# Install object and target files...
#

install:	all
	-$(MKDIR) $(INCLUDEDIR)/cups
	$(CHMOD) ugo+rx $(INCLUDEDIR)
	$(CHMOD) ugo+rx $(INCLUDEDIR)/cups
	$(INSTALL_DATA) $(HEADERS) $(INCLUDEDIR)/cups
	-$(MKDIR) $(LIBDIR)
	$(CHMOD) ugo+rx $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPS) $(LIBDIR)
	if test $(LIBCUPS) != "libcups.a" -a $(LIBCUPS) != "libcups.la"; then \
		$(INSTALL_LIB) libcups.a $(LIBDIR); \
		$(RM) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
		$(LN) $(LIBCUPS) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
	fi


#
# libcups.so.2, libcups.sl.1
#

libcups.so.2 libcups.sl.2:	$(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ $(LIBOBJS) $(SSLLIBS)
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# libcups.la
#

libcups.la:    $(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 2:2 $(SSLLIBS)


#
# libcups.a
#

libcups.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# cups_C.h - the default POSIX locale that is compiled in.
#

cups_C.h:	../locale/C/cups_C
	echo Generating $@...
	$(RM) cups_C.h
	$(AWK) '{print "\"" $$0 "\","}' < ../locale/C/cups_C > cups_C.h

dest.o:		cups.h http.h ipp.h language.h string.h
emit.o:		ppd.h
encode.o:	cups.h ipp.h string.h
http.o:		http.h ipp.h md5.h string.h
ipp.o:		http.h ipp.h string.h language.h
language.o:	cups_C.h language.h string.h
mark.o:		ppd.h
md5.o:		md5.h
options.o:	cups.h
page.o:		ppd.h
ppd.o:		language.h ppd.h
snprintf.o:	string.h
string.o:	string.h
tempfile.o:	cups.h string.h
usersys.o:	cups.h
util.o:		cups.h http.h ipp.h


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttp.o libcups.a $(NETLIBS) $(SSLLIBS)

testhttp.o:	http.h


#
# testppd (dependency on static CUPS library is intentional)
#

testppd:	testppd.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testppd.o libcups.a $(NETLIBS)

testppd.o:	ppd.h

$(OBJS):	../Makedefs ../config.h


#
# End of "$Id: Makefile,v 1.53 2001/03/09 20:06:06 mike Exp $".
#
