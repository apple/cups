#
# "$Id: Makefile,v 1.74 2002/07/30 19:09:25 mike Exp $"
#
#   Support library Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1997-2002 by Easy Software Products, all rights reserved.
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

#
# Object files...
#

LIBOBJS	=	dest.o emit.o encode.o http.o ipp.o language.o mark.o md5.o \
		md5passwd.o options.o page.o ppd.o snprintf.o string.o \
		tempfile.o usersys.o util.o
OBJS	=	$(LIBOBJS) testhttp.o testlang.o testppd.o ppd-debug.o


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
	$(RM) $(OBJS) $(TARGETS) `basename $(LIBCUPS) .2` libcups.dylib


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(OBJS:.o=.c) >/dev/null 2>&1


#
# Install object and target files...
#

install:	all	installhdrs
	$(INSTALL_DIR) $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPS) $(LIBDIR)
	if test $(LIBCUPS) = "libcups.so.2" -o $(LIBCUPS) = "libcups.sl.2"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
		$(LN) $(LIBCUPS) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
	fi
	if test $(LIBCUPS) = "libcups.2.dylib"; then \
		$(STRIP) -x $(LIBDIR)/$(LIBCUPS); \
		$(RM) $(LIBDIR)/libcups.dylib; \
		$(LN) $(LIBCUPS) $(LIBDIR)/libcups.dylib; \
	fi
	if test $(LIBCUPS) != "libcups.a"; then \
		$(INSTALL_LIB) libcups.a $(LIBDIR); \
	fi
	$(RANLIB) $(LIBDIR)/libcups.a

installhdrs:
	$(INSTALL_DIR) $(INCLUDEDIR)/cups
	for file in $(HEADERS); do \
		$(INSTALL_DATA) $$file $(INCLUDEDIR)/cups; \
	done


#
# libcups.so.2, libcups.sl.2
#

libcups.so.2 libcups.sl.2:	$(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ $(LIBOBJS) $(SSLLIBS)
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# libcups.2.dylib
#

libcups.2.dylib:	$(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 2.0.4 \
		-compatibility_version 2.0.0 \
		$(LIBOBJS) $(SSLLIBS)
	$(RM) libcups.dylib
	$(LN) $@ libcups.dylib


#
# libcups_s.a
#

libcups_s.a:	$(LIBOBJS) ../Makedefs
	echo Creating $@...
	$(RM) libcups_s.exp
	(echo _ipp_add_attr; echo _ipp_free_attr) >libcups_s.exp
	$(DSO) $(DSOFLAGS) -Wl,-bexport:libcups_s.exp -o libcups_s.o $(LIBOBJS) $(SSLLIBS) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcups_s.o


#
# libcups.la
#

libcups.la:    $(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 2:4 $(SSLLIBS)


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


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttp.o libcups.a $(NETLIBS) $(SSLLIBS)


#
# testlang (dependency on static CUPS library is intentional)
#

testlang:	testlang.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testlang.o libcups.a $(NETLIBS) $(SSLLIBS)


#
# testppd
#

testppd:	testppd.o ppd-debug.o language.o mark.o page.o string.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testppd.o ppd-debug.o language.o mark.o page.o \
		string.o

ppd-debug.o:	ppd.c ppd.h string.h ../config.h language.h debug.h
	echo Compiling $< with debugging...
	$(CC) $(OPTIM) $(CFLAGS) -DDEBUG -c -o $@ $<


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile,v 1.74 2002/07/30 19:09:25 mike Exp $".
#
