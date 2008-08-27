#
# "$Id$"
#
#   Scheduler Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1997-2007 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#

include ../Makedefs

CUPSDOBJS =	\
		auth.o \
		banners.o \
		cert.o \
		classes.o \
		client.o \
		conf.o \
		dirsvc.o \
		env.o \
		main.o \
		ipp.o \
		listen.o \
		job.o \
		log.o \
		network.o \
		policy.o \
		printers.o \
		process.o \
		quotas.o \
		removefile.o \
		select.o \
		server.o \
		statbuf.o \
		subscriptions.o \
		sysman.o
LIBOBJS =	\
		filter.o \
		mime.o \
		type.o
LIB32OBJS	= $(LIBOBJS:.o=.32.o)
LIB64OBJS	= $(LIBOBJS:.o=.64.o)
COBJS	=	\
		$(CUPSDOBJS) \
		$(LIBOBJS) \
		$(LIB32OBJS) \
		$(LIB64OBJS) \
		cupsfilter.o \
		cups-deviced.o \
		cups-lpd.o \
		cups-polld.o \
		testdirsvc.o \
		testlpd.o \
		testmime.o \
		testspeed.o \
		testsub.o \
		util.o
CXXOBJS	=	\
		cups-driverd.o
OBJS	=	\
		$(COBJS) \
		$(CXXOBJS)
LIBTARGETS =	\
		$(LIBCUPSMIME) \
		$(LIB32CUPSMIME) \
		$(LIB64CUPSMIME) \
		libcupsmime.a \
		testmime

TARGETS	=	\
		$(LIBTARGETS) \
		cupsd \
		cupsfilter \
		cups-deviced \
		cups-driverd \
		cups-lpd \
		cups-polld \
		testdirsvc \
		testlpd \
		testspeed \
		testsub


#
# Make everything...
#

all:		$(TARGETS)


#
# Make library targets...
#

libs:		$(LIBTARGETS)


#
# Make unit tests...
#

unittests:


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS)
	$(RM) $(TARGETS) convert
	$(RM) libcupsmime.so libcupsmime.sl libcupsmime.dylib
	$(RM) -r 32bit 64bit


#
# Update dependencies (without system header dependencies...)
#

depend:
	touch Dependencies.tmp Dependencies.tmplib
	makedepend -Y -I.. -fDependencies.tmp $(COBJS:.o=.c) \
		$(CXXOBJS:.o=.cxx) >/dev/null 2>&1
	makedepend -Y -I.. -fDependencies.tmplib $(LIBOBJS:.o=.c) >/dev/null 2>&1
	$(RM) Dependencies
	cp Dependencies.tmp Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.32.o: \1\.c /' \
		Dependencies.tmplib >>Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.64.o: \1\.c /' \
		Dependencies.tmplib >>Dependencies
	$(RM) Dependencies.tmp Dependencies.tmplib


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:
	echo Creating $(SERVERBIN)/driver...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/driver
	echo Creating $(SERVERROOT)...
	$(INSTALL_DIR) -m 755 $(SERVERROOT)
	-chgrp $(CUPS_GROUP) $(SERVERROOT)
	echo Creating $(SERVERROOT)/interfaces...
	$(INSTALL_DIR) -m 755 $(SERVERROOT)/interfaces
	-chgrp $(CUPS_GROUP) $(SERVERROOT)/interfaces
	echo Creating $(SERVERROOT)/ppd...
	$(INSTALL_DIR) -m 755 $(SERVERROOT)/ppd
	-chgrp $(CUPS_GROUP) $(SERVERROOT)/ppd
	-if test "x`uname`" != xDarwin; then \
		echo Creating $(SERVERROOT)/ssl...; \
		$(INSTALL_DIR) -m 700 $(SERVERROOT)/ssl; \
		chgrp $(CUPS_GROUP) $(SERVERROOT)/ssl || true; \
		echo Creating $(STATEDIR)...; \
		$(INSTALL_DIR) -m 755 $(STATEDIR); \
		echo Creating $(STATEDIR)/certs...; \
		$(INSTALL_DIR) -m 511 $(STATEDIR)/certs; \
		chgrp $(CUPS_PRIMARY_SYSTEM_GROUP) $(STATEDIR)/certs || true; \
	fi
	echo Creating $(LOGDIR)...
	$(INSTALL_DIR) -m 755 $(LOGDIR)
	echo Creating $(REQUESTS)...
	$(INSTALL_DIR) -m 710 $(REQUESTS)
	-chgrp $(CUPS_GROUP) $(REQUESTS)
	echo Creating $(REQUESTS)/tmp...
	$(INSTALL_DIR) -m 1770 $(REQUESTS)/tmp
	-chgrp $(CUPS_GROUP) $(REQUESTS)/tmp
	echo Creating $(CACHEDIR)...
	$(INSTALL_DIR) -m 775 $(CACHEDIR)
	-chgrp $(CUPS_GROUP) $(CACHEDIR)
#	echo Creating $(CACHEDIR)/ppd...
#	$(INSTALL_DIR) -m 755 $(CACHEDIR)/ppd
#	-chgrp $(CUPS_GROUP) $(CACHEDIR)/ppd


#
# Install programs...
#

install-exec:
	echo Installing programs in $(SBINDIR)...
	$(INSTALL_DIR) -m 755 $(SBINDIR)
	$(INSTALL_BIN) cupsd $(SBINDIR)
	$(INSTALL_BIN) cupsfilter $(SBINDIR)
	-if test "x`uname`" = xDarwin; then \
		$(INSTALL_DIR) $(BUILDROOT)/System/Library/Printers/Libraries; \
		$(LN) $(sbindir)/cupsfilter $(BUILDROOT)/System/Library/Printers/Libraries/convert; \
	fi
	echo Installing programs in $(SERVERBIN)/daemon...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-deviced $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-driverd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-lpd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-polld $(SERVERBIN)/daemon
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
		done \
	fi


#
# Install headers...
#

install-headers:
	echo Installing header files in $(INCLUDEDIR)/cups...
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) mime.h $(INCLUDEDIR)/cups


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	echo Installing libraries in $(LIBDIR)...
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSMIME) $(LIBDIR)
	if test $(LIBCUPSMIME) = "libcupsmime.so.1" -o $(LIBCUPSMIME) = "libcupsmime.sl.1"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSMIME) .1`; \
		$(LN) $(LIBCUPSMIME) $(LIBDIR)/`basename $(LIBCUPSMIME) .1`; \
	fi
	if test $(LIBCUPSMIME) = "libcupsmime.1.dylib"; then \
		$(RM) $(LIBDIR)/libcupsmime.dylib; \
		$(LN) $(LIBCUPSMIME) $(LIBDIR)/libcupsmime.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPSMIME) $(SYMROOT); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) libcupsmime.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsmime.a

install32bit:
	echo Installing libraries in $(LIB32DIR)...
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcupsmime.so.1 $(LIB32DIR)/libcupsmime.so.1
	$(LN) libcupsmime.so.1 $(LIB32DIR)/libcupsmime.so

install64bit:
	echo Installing libraries in $(LIB64DIR)...
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcupsmime.so.1 $(LIB64DIR)/libcupsmime.so.1
	$(LN) libcupsmime.so.1 $(LIB64DIR)/libcupsmime.so


#
# Uninstall the scheduler...
#

uninstall: $(UNINSTALL32) $(UNINSTALL64)
	$(RM) $(SBINDIR)/cupsd
	$(RM) $(SBINDIR)/cupsfilter
	$(RM) $(SERVERBIN)/daemon/cups-deviced
	$(RM) $(SERVERBIN)/daemon/cups-driverd
	$(RM) $(SERVERBIN)/daemon/cups-lpd
	$(RM) $(SERVERBIN)/daemon/cups-polld
	$(RM) $(BUILDROOT)/System/Library/Printers/Libraries/convert
	-$(RMDIR) $(STATEDIR)/certs
	-$(RMDIR) $(STATEDIR)
	-$(RMDIR) $(SERVERROOT)/ppd
	-$(RMDIR) $(SERVERROOT)/interfaces
	-$(RMDIR) $(SERVERROOT)
	-$(RMDIR) $(SERVERBIN)/driver
	-$(RMDIR) $(SERVERBIN)/daemon
	-$(RMDIR) $(SERVERBIN)
	-$(RMDIR) $(SBINDIR)
	-$(RMDIR) $(REQUESTS)/tmp
	-$(RMDIR) $(REQUESTS)
	-$(RMDIR) $(LOGDIR)
	-$(RMDIR) $(CACHEDIR)
	$(RM) $(LIBDIR)/libcupsmime.1.dylib
	$(RM) $(LIBDIR)/libcupsmime.a
	$(RM) $(LIBDIR)/libcupsmime.dylib
	$(RM) $(LIBDIR)/libcupsmime_s.a
	$(RM) $(LIBDIR)/libcupsmime.sl
	$(RM) $(LIBDIR)/libcupsmime.sl.1
	$(RM) $(LIBDIR)/libcupsmime.so
	$(RM) $(LIBDIR)/libcupsmime.so.1
	-$(RMDIR) $(LIBDIR)
	$(RM) $(INCLUDEDIR)/cups/mime.h
	-$(RMDIR) $(INCLUDEDIR)/cups

uninstall32bit:
	$(RM) $(LIB32DIR)/libcupsmime.so
	$(RM) $(LIB32DIR)/libcupsmime.so.1
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcupsmime.so
	$(RM) $(LIB64DIR)/libcupsmime.so.1
	-$(RMDIR) $(LIB64DIR)


#
# Automatic API help files...
#

apihelp:
	mxmldoc --section "Programming" \
		--title "MIME API" \
		--css ../doc/cups-printable.css \
		--header api-mime.header --intro api-mime.shtml \
		mime.h $(LIBOBJS:.o=.c) >../doc/help/api-mime.html

framedhelp:
	mxmldoc --framed api-mime \
		--section "Programming" \
		--title "MIME API" \
		--css ../doc/cups-printable.css \
		--header api-mime.header --intro api-mime.shtml \
		mime.h $(LIBOBJS:.o=.c)


#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(CUPSDOBJS) $(LIBCUPSMIME) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd $(CUPSDOBJS) -L. -lcupsmime \
		$(LIBZ) $(SSLLIBS) $(LIBSLP) $(LIBLDAP) $(PAMLIBS) \
		$(LIBPAPER) $(LIBMALLOC) $(CUPSDLIBS) $(DNSSDLIBS) $(LIBS) \
		$(LIBGSSAPI) $(LIBWRAP)

cupsd-static:	$(CUPSDOBJS) libcupsmime.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd-static $(CUPSDOBJS) libcupsmime.a \
		$(LIBZ) $(SSLLIBS) $(LIBSLP) $(LIBLDAP) $(PAMLIBS) \
		../cups/libcups.a $(COMMONLIBS) $(LIBZ) $(LIBPAPER) \
		$(LIBMALLOC) $(CUPSDLIBS) $(DNSSDLIBS) $(LIBGSSAPI) \
		$(LIBWRAP)


#
# Make the cupsfilter utility.
#

cupsfilter:	cupsfilter.o $(LIBCUPSMIME) ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsfilter cupsfilter.o -L. -lcupsmime $(LIBS)
	$(RM) convert
	$(LN) cupsfilter convert


#
# Make the device daemon, "cups-deviced".
#

cups-deviced:	cups-deviced.o util.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-deviced cups-deviced.o util.o $(LIBS)


#
# Make the driver daemon, "cups-driverd".
#

cups-driverd:	cups-driverd.o util.o ../cups/$(LIBCUPS) ../ppdc/$(LIBCUPSPPDC)
	echo Linking $@...
	$(CXX) $(LDFLAGS) -o cups-driverd cups-driverd.o util.o \
		-L../ppdc -lcupsppdc $(LIBS)


#
# Make the line printer daemon, "cups-lpd".
#

cups-lpd:	cups-lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-lpd cups-lpd.o $(LIBS)


#
# Make the polling daemon, "cups-polld".
#

cups-polld:	cups-polld.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-polld cups-polld.o $(LIBS)


#
# libcupsmime.so.1, libcupsmime.sl.1
#

libcupsmime.so.1 libcupsmime.sl.1:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LIBS)
	$(RM) `basename $@ .1`
	$(LN) $@ `basename $@ .1`


#
# 32bit/libcupsmime.so.1
#

32bit/libcupsmime.so.1:	$(LIB32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSO) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(LIB32OBJS) $(LIBS)
	$(RM) 32bit/libcupsmime.so
	$(LN) libcupsmime.so.1 32bit/libcupsmime.so


#
# 64bit/libcupsmime.so.1
#

64bit/libcupsmime.so.1:	$(LIB64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSO) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(LIB64OBJS) $(LIBS)
	$(RM) 64bit/libcupsmime.so
	$(LN) libcupsmime.so.1 64bit/libcupsmime.so


#
# libcupsmime.1.dylib
#

libcupsmime.1.dylib:	$(LIBOBJS) libcupsmime.exp
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 1.0.0 \
		-compatibility_version 1.0.0 \
		-exported_symbols_list libcupsmime.exp \
		$(LIBOBJS) $(LIBS)
	$(RM) libcupsmime.dylib
	$(LN) $@ libcupsmime.dylib


#
# libcupsmime_s.a
#

libcupsmime_s.a:	$(LIBOBJS)
	echo Creating $@...
	$(DSO) $(DSOFLAGS) -o libcupsmime_s.o $(LIBOBJS) $(LIBS)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupsmime_s.o


#
# libcupsmime.la
#

libcupsmime.la:    $(LIBOBJS)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 1:0 $(LIBS)


#
# libcupsmime.a
#

libcupsmime.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# Make the test program, "testdirsvc".
#

testdirsvc:	testdirsvc.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testdirsvc testdirsvc.o $(COMMONLIBS) $(NETLIBS)


#
# Make the test program, "testlpd".
#

testlpd:	testlpd.o ../cups/libcups.a cups-lpd
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testlpd testlpd.o ../cups/libcups.a \
		$(COMMONLIBS) $(LIBZ) $(SSLLIBS) $(LIBGSSAPI)


#
# testmime
#

testmime:	testmime.o libcupsmime.a ../cups/libcups.a
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testmime.o libcupsmime.a \
		../cups/libcups.a $(COMMONLIBS) $(LIBZ) $(SSLLIBS) $(LIBGSSAPI)
	echo Running MIME tests...
	./testmime


#
# Make the test program, "testspeed".
#

testspeed:	testspeed.o ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testspeed testspeed.o ../cups/libcups.a \
		$(SSLLIBS) $(COMMONLIBS) $(LIBZ) $(LIBGSSAPI)


#
# Make the test program, "testsub".
#

testsub:	testsub.o ../cups/libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testsub testsub.o ../cups/libcups.a \
		$(SSLLIBS) $(COMMONLIBS) $(LIBZ) $(LIBGSSAPI)


#
# Dependencies...
#
  
include Dependencies


#
# End of "$Id$".
#
