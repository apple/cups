#
# "$Id: Makefile,v 1.53.2.8 2002/03/22 15:47:19 mike Exp $"
#
#   API library Makefile for the Common UNIX Printing System (CUPS).
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

LIBOBJS	=	dest.o emit.o encode.o language.o mark.o options.o \
		page.o ppd.o tempfile.o usersys.o util.o
OBJS	=	$(LIBOBJS) testhttp.o testppd.o


#
# Header files to install...
#

HEADERS	=	cups.h language.h ppd.h


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
	$(RM) $(OBJS) $(TARGETS) `basename $(LIBCUPS) .3`


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
	if test $(LIBCUPS) = "libcups.so.3" -o $(LIBCUPS) = "libcups.sl.3"; then \
		$(INSTALL_LIB) libcups.a $(LIBDIR); \
		$(RM) $(LIBDIR)/`basename $(LIBCUPS) .3`; \
		$(LN) $(LIBCUPS) $(LIBDIR)/`basename $(LIBCUPS) .3`; \
	fi
	if test $(LIBCUPS) = "libcups.3.dylib"; then \
		$(STRIP) -x $(LIBDIR)/$(LIBCUPS); \
		$(INSTALL_LIB) libcups.a $(LIBDIR); \
		$(RM) $(LIBDIR)/libcups.dylib; \
		$(LN) $(LIBCUPS) $(LIBDIR)/libcups.dylib; \
	fi
	if test $(LIBCUPS) = "libcups_s.a"; then \
		$(INSTALL_LIB) libcups.a $(LIBDIR); \
	fi

installhdrs:
	$(INSTALL_DIR) $(INCLUDEDIR)/cups
	for file in $(HEADERS); do \
		$(INSTALL_DATA) $$file $(INCLUDEDIR)/cups; \
	done


#
# libcups.so.3, libcups.sl.3
#

libcups.so.3 libcups.sl.3:	$(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ $(LIBOBJS)
	$(RM) `basename $@ .3`
	$(LN) $@ `basename $@ .3`


#
# libcups.2.dylib
#

libcups.3.dylib:	$(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/libcups.dylib \
		-current_version 3.0.0 \
		-compatibility_version 3.0.0 \
		$(LIBOBJS)
	$(RM) libcups.dylib
	$(LN) $@ libcups.dylib


#
# libcups_s.a
#

libcups_s.a:	$(LIBOBJS) ../Makedefs
	echo Creating $@...
	$(DSO) $(DSOFLAGS) -o libcups_s.o $(LIBOBJS) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcups_s.o


#
# libcups.la
#

libcups.la:    $(LIBOBJS) ../Makedefs
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 3:0


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
# testppd (dependency on static CUPS library is intentional)
#

testppd:	testppd.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testppd.o libcups.a $(NETLIBS)


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile,v 1.53.2.8 2002/03/22 15:47:19 mike Exp $".
#
