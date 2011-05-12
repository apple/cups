#
# "$Id: Makefile 7875 2008-08-27 22:53:31Z mike $"
#
#   Scheduler Makefile for CUPS.
#
#   Copyright 2007-2011 by Apple Inc.
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
		file.o \
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
		select.o \
		server.o \
		statbuf.o \
		subscriptions.o \
		sysman.o
LIBOBJS =	\
		filter.o \
		mime.o \
		type.o
COBJS	=	\
		$(CUPSDOBJS) \
		$(LIBOBJS) \
		cupsfilter.o \
		cups-deviced.o \
		cups-exec.o \
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
		libcupsmime.a

UNITTARGETS =	\
		testdirsvc \
		testlpd \
		testmime \
		testspeed \
		testsub

PROGRAMS =	\
		cupsd \
		cupsfilter \
		cups-deviced \
		cups-driverd \
		cups-exec \
		cups-lpd \
		cups-polld

TARGETS	=	\
		$(LIBTARGETS) \
		$(PROGRAMS)


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

unittests:	$(UNITTARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS)
	$(RM) $(TARGETS) $(UNITTARGETS) convert
	$(RM) libcupsmime.so libcupsmime.sl libcupsmime.dylib


#
# Update dependencies (without system header dependencies...)
#

depend:
	makedepend -Y -I.. -fDependencies $(COBJS:.o=.c) \
		$(CXXOBJS:.o=.cxx) >/dev/null 2>&1


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
	$(INSTALL_DIR) -m 755 -g $(CUPS_GROUP) $(SERVERROOT)
	echo Creating $(SERVERROOT)/interfaces...
	$(INSTALL_DIR) -m 755 -g $(CUPS_GROUP) $(SERVERROOT)/interfaces
	echo Creating $(SERVERROOT)/ppd...
	$(INSTALL_DIR) -m 755 -g $(CUPS_GROUP) $(SERVERROOT)/ppd
	if test "x`uname`" != xDarwin; then \
		echo Creating $(SERVERROOT)/ssl...; \
		$(INSTALL_DIR) -m 700 -g $(CUPS_GROUP) $(SERVERROOT)/ssl; \
	fi
	if test "$(STATEDIR)" != "$(SERVERROOT)"; then \
		echo Creating $(STATEDIR)...; \
		$(INSTALL_DIR) -m 755 $(STATEDIR); \
	fi
	echo Creating $(STATEDIR)/certs...
	$(INSTALL_DIR) -m 511 -o $(CUPS_USER) -g $(CUPS_PRIMARY_SYSTEM_GROUP) \
		$(STATEDIR)/certs
	echo Creating $(LOGDIR)...
	$(INSTALL_DIR) -m 755 $(LOGDIR)
	echo Creating $(REQUESTS)...
	$(INSTALL_DIR) -m 710 -g $(CUPS_GROUP) $(REQUESTS)
	echo Creating $(REQUESTS)/tmp...
	$(INSTALL_DIR) -m 1770 -g $(CUPS_GROUP) $(REQUESTS)/tmp
	echo Creating $(CACHEDIR)...
	$(INSTALL_DIR) -m 775 -g $(CUPS_GROUP) $(CACHEDIR)
	if test "x$(INITDIR)" != x; then \
		echo Installing init scripts...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/init.d; \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDIR)/init.d/cups; \
		for level in $(RCLEVELS); do \
			$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc$${level}.d; \
			$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc$${level}.d/S$(RCSTART)cups; \
			if test `uname` = HP-UX; then \
				level=`expr $$level - 1`; \
				$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc$${level}.d; \
			fi; \
			$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc$${level}.d/K$(RCSTOP)cups; \
		done; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc0.d/K$(RCSTOP)cups; \
	fi
	if test "x$(INITDIR)" = x -a "x$(INITDDIR)" != x; then \
		$(INSTALL_DIR) $(BUILDROOT)$(INITDDIR); \
		if test "$(INITDDIR)" = "/System/Library/LaunchDaemons"; then \
			echo Installing LaunchDaemons configuration files...; \
			$(INSTALL_DATA) org.cups.cupsd.plist $(BUILDROOT)$(DEFAULT_LAUNCHD_CONF); \
			$(INSTALL_DATA) org.cups.cups-lpd.plist $(BUILDROOT)/System/Library/LaunchDaemons; \
		else \
			echo Installing RC script...; \
			$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
	fi
	if test "x$(SMFMANIFESTDIR)" != x; then \
		echo Installing SMF manifest in $(SMFMANIFESTDIR)...;\
		$(INSTALL_DIR) $(BUILDROOT)/$(SMFMANIFESTDIR); \
		$(INSTALL_SCRIPT) cups.xml $(BUILDROOT)$(SMFMANIFESTDIR)/cups.xml; \
	fi
	if test "x$(XINETD)" != x; then \
		echo Installing xinetd configuration file for cups-lpd...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(XINETD); \
		$(INSTALL_DATA) cups-lpd.xinetd $(BUILDROOT)$(XINETD)/cups-lpd; \
	fi


#
# Install programs...
#

install-exec:
	echo Installing programs in $(SBINDIR)...
	$(INSTALL_DIR) -m 755 $(SBINDIR)
	$(INSTALL_BIN) -m 500 cupsd $(SBINDIR)
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
	$(INSTALL_BIN) cups-exec $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-lpd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-polld $(SERVERBIN)/daemon
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(PROGRAMS); do \
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

install-libs: $(INSTALLSTATIC)
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
	$(INSTALL_LIB) -m 755 libcupsmime.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsmime.a
	$(CHMOD) 555 $(LIBDIR)/libcupsmime.a


#
# Uninstall the scheduler...
#

uninstall:
	$(RM) $(SBINDIR)/cupsd
	$(RM) $(SBINDIR)/cupsfilter
	$(RM) $(SERVERBIN)/daemon/cups-deviced
	$(RM) $(SERVERBIN)/daemon/cups-driverd
	$(RM) $(SERVERBIN)/daemon/cups-exec
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
	echo Uninstalling startup script...
	if test "x$(INITDIR)" != x; then \
		$(RM) $(BUILDROOT)$(INITDIR)/init.d/cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/init.d; \
		$(RM)  $(BUILDROOT)$(INITDIR)/rc0.d/K00cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc2.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc2.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc3.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc3.d; \
		$(RM) $(BUILDROOT)$(INITDIR)/rc5.d/S99cups; \
		$(RMDIR) $(BUILDROOT)$(INITDIR)/rc5.d; \
	fi
	if test "x$(INITDIR)" = x -a "x$(INITDDIR)" != x; then \
		if test "$(INITDDIR)" = "/System/Library/StartupItems/PrintingServices"; then \
			$(RM) $(BUILDROOT)$(INITDDIR)/PrintingServices; \
			$(RM) $(BUILDROOT)$(INITDDIR)/StartupParameters.plist; \
			$(RM) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj/Localizable.strings; \
			$(RMDIR) $(BUILDROOT)$(INITDDIR)/Resources/English.lproj; \
		elif test "$(INITDDIR)" = "/System/Library/LaunchDaemons"; then \
			$(RM) $(BUILDROOT)$(INITDDIR)/org.cups.cupsd.plist; \
			$(RM) $(BUILDROOT)$(INITDDIR)/org.cups.cups-lpd.plist; \
			$(RMDIR) $(BUILDROOT)/System/Library/StartupItems/PrintingServices; \
		else \
			$(INSTALL_SCRIPT) init/cups.sh $(BUILDROOT)$(INITDDIR)/cups; \
		fi \
		$(RMDIR) $(BUILDROOT)$(INITDDIR); \
	fi
	if test "x$(SMFMANIFESTDIR)" != x; then \
		echo Uninstalling SMF manifest in $(SMFMANIFESTDIR)...;\
		$(RM) $(BUILDROOT)$(SMFMANIFESTDIR)/cups.xml; \
	fi
	if test "x$(XINETD)" != x; then \
		echo Uninstalling xinetd configuration file for cups-lpd...; \
		$(RM) $(BUILDROOT)$(XINETD)/cups-lpd; \
	fi


#
# Automatic API help files...
#

apihelp:
	mxmldoc --section "Programming" \
		--title "MIME API" \
		--css ../doc/cups-printable.css \
		--header api-mime.header --intro api-mime.shtml \
		mime.h $(LIBOBJS:.o=.c) >../doc/help/api-mime.html
	mxmldoc --tokens help/api-mime.html api-mime.xml >../doc/help/api-mime.tokens
	$(RM) api-mime.xml

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
		$(LIBPAPER) $(LIBMALLOC) $(SERVERLIBS) $(DNSSDLIBS) $(LIBS) \
		$(LIBGSSAPI) $(LIBWRAP)

cupsd-static:	$(CUPSDOBJS) libcupsmime.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cupsd-static $(CUPSDOBJS) libcupsmime.a \
		$(LIBZ) $(SSLLIBS) $(LIBSLP) $(LIBLDAP) $(PAMLIBS) \
		../cups/$(LIBCUPSSTATIC) $(COMMONLIBS) $(LIBZ) $(LIBPAPER) \
		$(LIBMALLOC) $(SERVERLIBS) $(DNSSDLIBS) $(LIBGSSAPI) \
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
# Make the sandbox execution helper, "cups-exec".
#

cups-exec:	cups-exec.o
	echo Linking $@...
	$(CC) $(LDFLAGS) -o cups-exec cups-exec.o $(LIBS)


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

testlpd:	testlpd.o ../cups/$(LIBCUPSSTATIC) cups-lpd
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testlpd testlpd.o ../cups/$(LIBCUPSSTATIC) \
		$(COMMONLIBS) $(LIBZ) $(SSLLIBS) $(DNSSDLIBS) $(LIBGSSAPI)


#
# testmime
#

testmime:	testmime.o libcupsmime.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testmime.o libcupsmime.a \
		../cups/$(LIBCUPSSTATIC) $(COMMONLIBS) $(LIBZ) $(SSLLIBS) \
		$(DNSSDLIBS) $(LIBGSSAPI)
	echo Running MIME tests...
	./testmime


#
# Make the test program, "testspeed".
#

testspeed:	testspeed.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testspeed testspeed.o ../cups/$(LIBCUPSSTATIC) \
		$(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ) $(LIBGSSAPI)


#
# Make the test program, "testsub".
#

testsub:	testsub.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(CC) $(LDFLAGS) -o testsub testsub.o ../cups/$(LIBCUPSSTATIC) \
		$(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ) $(LIBGSSAPI)


#
# Lines of code computation...
#

sloc:
	echo "cupsd: \c"
	sloccount $(CUPSDOBJS:.o=.c) $(LIBOBJS:.o=.c) cups-driverd.cxx cups-polld.c cups-lpd.c 2>/dev/null | grep "Total Physical" | awk '{print $$9}'


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 7875 2008-08-27 22:53:31Z mike $".
#
