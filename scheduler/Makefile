#
# Scheduler Makefile for CUPS.
#
# Copyright © 2007-2019 by Apple Inc.
# Copyright © 1997-2007 by Easy Software Products, all rights reserved.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

include ../Makedefs

CUPSDOBJS =	\
		auth.o \
		banners.o \
		cert.o \
		classes.o \
		client.o \
		colorman.o \
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
		libcupsmime.a

UNITTARGETS =	\
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
		cups-lpd

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

libs:


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


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(COBJS:.o=.c) >Dependencies
	$(CXX) -MM $(ALL_CXXFLAGS) $(CXXOBJS:.o=.cxx) >>Dependencies


#
# Run oclint to check code coverage...
#

oclint:
	oclint -o=oclint.html -html $(CUPSDOBJS:.o=.c) $(LIBOBJS:.o=.c) -- $(ALL_CFLAGS)


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
	$(INSTALL_DIR) -m 770 -g $(CUPS_GROUP) $(CACHEDIR)
	if test "x$(INITDIR)" != x; then \
		echo Installing init scripts...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/init.d; \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDIR)/init.d/cups; \
		for level in $(RCLEVELS); do \
			$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc$${level}.d; \
			$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc$${level}.d/S$(RCSTART)cups; \
			$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc$${level}.d/K$(RCSTOP)cups; \
		done; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDIR)/rc0.d; \
		$(LN) ../init.d/cups $(BUILDROOT)$(INITDIR)/rc0.d/K$(RCSTOP)cups; \
	fi
	if test "x$(INITDDIR)" != x; then \
		echo Installing init script...; \
		$(INSTALL_DIR) -m 755 $(BUILDROOT)$(INITDDIR); \
		$(INSTALL_SCRIPT) cups.sh $(BUILDROOT)$(INITDDIR); \
	fi
	if test "x$(LAUNCHD_DIR)" != x; then \
		echo Installing launchd configuration files...; \
		$(INSTALL_DIR) $(BUILDROOT)$(LAUNCHD_DIR); \
		$(INSTALL_DATA) org.cups.cupsd.plist $(BUILDROOT)$(LAUNCHD_DIR); \
		$(INSTALL_DATA) org.cups.cups-lpd.plist $(BUILDROOT)$(LAUNCHD_DIR); \
	fi
	if test "x$(SMFMANIFESTDIR)" != x; then \
		echo Installing SMF manifest in $(SMFMANIFESTDIR)...;\
		$(INSTALL_DIR) $(BUILDROOT)/$(SMFMANIFESTDIR); \
		$(INSTALL_SCRIPT) cups.xml $(BUILDROOT)$(SMFMANIFESTDIR)/cups.xml; \
	fi
	if test "x$(SYSTEMD_DIR)" != x; then \
		echo Installing systemd configuration files...; \
		$(INSTALL_DIR) $(BUILDROOT)$(SYSTEMD_DIR); \
		$(INSTALL_DATA) org.cups.cupsd.path $(BUILDROOT)$(SYSTEMD_DIR); \
		$(INSTALL_DATA) org.cups.cupsd.service $(BUILDROOT)$(SYSTEMD_DIR); \
		$(INSTALL_DATA) org.cups.cupsd.socket $(BUILDROOT)$(SYSTEMD_DIR); \
		$(INSTALL_DATA) org.cups.cups-lpdAT.service $(BUILDROOT)$(SYSTEMD_DIR)/org.cups.cups-lpd@.service; \
		$(INSTALL_DATA) org.cups.cups-lpd.socket $(BUILDROOT)$(SYSTEMD_DIR); \
	elif test "x$(XINETD)" != x; then \
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
	$(INSTALL_BIN) -m $(CUPS_CUPSD_FILE_PERM) cupsd $(SBINDIR)
	$(INSTALL_BIN) cupsfilter $(SBINDIR)
	echo Installing programs in $(SERVERBIN)/daemon...
	$(INSTALL_DIR) -m 755 $(SERVERBIN)
	$(INSTALL_DIR) -m 755 $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-deviced $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-driverd $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-exec $(SERVERBIN)/daemon
	$(INSTALL_BIN) cups-lpd $(SERVERBIN)/daemon
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(PROGRAMS); do \
			cp $$file $(SYMROOT); \
			dsymutil $(SYMROOT)/$$file; \
		done \
	fi


#
# Install headers...
#

install-headers:


#
# Install libraries...
#

install-libs:


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
	if test "x$(INITDIR)" != x; then \
		echo Uninstalling init scripts...; \
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
	if test "x$(INITDDIR)" != x; then \
		echo Uninstalling startup script...; \
		$(RM) $(BUILDROOT)$(INITDDIR)/cups.sh; \
		$(RMDIR) $(BUILDROOT)$(INITDDIR); \
	fi
	if test "x$LAUNCHD_DIR" != x; then \
		echo Uninstalling launchd files...; \
		$(RM) $(BUILDROOT)$(LAUNCHD_DIR)/org.cups.cupsd.plist; \
		$(RM) $(BUILDROOT)$(LAUNCHD_DIR)/org.cups.cups-lpd.plist; \
	fi
	if test "x$SYSTEMD_DIR" != x; then \
		echo Uninstalling systemd files...; \
		$(RM) $(BUILDROOT)$(SYSTEMD_DIR)/org.cups.cupsd.path; \
		$(RM) $(BUILDROOT)$(SYSTEMD_DIR)/org.cups.cupsd.service; \
		$(RM) $(BUILDROOT)$(SYSTEMD_DIR)/org.cups.cupsd.socket; \
	fi
	if test "x$(SMFMANIFESTDIR)" != x; then \
		echo Uninstalling SMF manifest file...;\
		$(RM) $(BUILDROOT)$(SMFMANIFESTDIR)/cups.xml; \
	fi
	if test "x$(XINETD)" != x; then \
		echo Uninstalling xinetd configuration file for cups-lpd...; \
		$(RM) $(BUILDROOT)$(XINETD)/cups-lpd; \
	fi


#
# Make the scheduler executable, "cupsd".
#

cupsd:	$(CUPSDOBJS) libcupsmime.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cupsd $(CUPSDOBJS) libcupsmime.a \
		$(PAMLIBS) $(LIBPAPER) $(LIBMALLOC) $(DNSSDLIBS) $(SERVERLIBS) \
		$(ONDEMANDLIBS) $(LIBWRAP) $(LIBGSSAPI) $(COMMONLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@

cupsd-static:	$(CUPSDOBJS) libcupsmime.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cupsd-static $(CUPSDOBJS) libcupsmime.a \
		$(PAMLIBS) $(LIBPAPER) $(LIBMALLOC) $(SERVERLIBS) $(ONDEMANDLIBS) \ $(LIBWRAP) $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Make the cupsfilter utility.
#

cupsfilter:	cupsfilter.o libcupsmime.a ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cupsfilter cupsfilter.o libcupsmime.a $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	$(RM) convert
	$(LN) cupsfilter convert

cupsfilter-static:	cupsfilter.o libcupsmime.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cupsfilter-static cupsfilter.o libcupsmime.a \
		$(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Make the device daemon, "cups-deviced".
#

cups-deviced:	cups-deviced.o util.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cups-deviced cups-deviced.o util.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Make the driver daemon, "cups-driverd".
#

cups-driverd:	cups-driverd.o util.o ../cups/$(LIBCUPS) ../ppdc/libcupsppdc.a
	echo Linking $@...
	$(LD_CXX) $(ALL_LDFLAGS) -o cups-driverd cups-driverd.o util.o \
		../ppdc/libcupsppdc.a $(COMMONLIBS) $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Make the sandbox execution helper, "cups-exec".
#

cups-exec:	cups-exec.o
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cups-exec cups-exec.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Make the line printer daemon, "cups-lpd".
#

cups-lpd:	cups-lpd.o ../cups/$(LIBCUPS)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o cups-lpd cups-lpd.o $(LINKCUPS)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# libcupsmime.a
#

libcupsmime.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# Make the test program, "testlpd".
#

testlpd:	testlpd.o ../cups/$(LIBCUPSSTATIC) cups-lpd
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o testlpd testlpd.o $(LINKCUPSSTATIC)


#
# testmime
#

testmime:	testmime.o libcupsmime.a ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(ALL_LDFLAGS) -o $@ testmime.o libcupsmime.a \
		$(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running MIME tests...
	./testmime


#
# Make the test program, "testspeed".
#

testspeed:	testspeed.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o testspeed testspeed.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Make the test program, "testsub".
#

testsub:	testsub.o ../cups/$(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ALL_LDFLAGS) -o testsub testsub.o $(LINKCUPSSTATIC)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Lines of code computation...
#

sloc:
	echo "cupsd: \c"
	sloccount $(CUPSDOBJS:.o=.c) $(LIBOBJS:.o=.c) cups-driverd.cxx cups-lpd.c 2>/dev/null | grep "Total Physical" | awk '{print $$9}'


#
# Dependencies...
#

include Dependencies
