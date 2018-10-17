#
# Library Makefile for CUPS.
#
# Copyright © 2007-2018 by Apple Inc.
# Copyright © 1997-2006 by Easy Software Products, all rights reserved.
#
# These coded instructions, statements, and computer programs are the
# property of Apple Inc. and are protected by Federal copyright
# law.  Distribution and use rights are outlined in the file "LICENSE.txt"
# which should have been included with this file.  If this file is
# missing or damaged, see the license at "http://www.cups.org/".
#
# This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs


#
# Options to build libcups without the use of deprecated APIs...
#

OPTIONS	=	-D_CUPS_NO_DEPRECATED=1 -D_PPD_DEPRECATED=""


#
# Object files...
#

LIBOBJS	=	\
		adminutil.o \
		array.o \
		auth.o \
		backchannel.o \
		backend.o \
		debug.o \
		dest.o \
		dest-job.o \
		dest-localization.o \
		dest-options.o \
		dir.o \
		encode.o \
		file.o \
		getdevices.o \
		getifaddrs.o \
		getputfile.o \
		globals.o \
		hash.o \
		http.o \
		http-addr.o \
		http-addrlist.o \
		http-support.o \
		ipp.o \
		ipp-support.o \
		langprintf.o \
		language.o \
		md5.o \
		md5passwd.o \
		notify.o \
		options.o \
		ppd.o \
		ppd-attr.o \
		ppd-cache.o \
		ppd-conflicts.o \
		ppd-custom.o \
		ppd-emit.o \
		ppd-localize.o \
		ppd-mark.o \
		ppd-page.o \
		ppd-util.o \
		pwg-media.o \
		request.o \
		sidechannel.o \
		snmp.o \
		snprintf.o \
		string.o \
		tempfile.o \
		thread.o \
		tls.o \
		transcode.o \
		usersys.o \
		util.o
TESTOBJS	= \
		testadmin.o \
		testarray.o \
		testcache.o \
		testconflicts.o \
		testcreds.o \
		testcups.o \
		testdest.o \
		testfile.o \
		testgetdests.o \
		testhttp.o \
		testi18n.o \
		testipp.o \
		testoptions.o \
		testlang.o \
		testppd.o \
		testpwg.o \
		testsnmp.o \
		tlscheck.o
OBJS	=	\
		$(LIBOBJS) \
		$(TESTOBJS)


#
# Header files to install...
#

HEADERS	=	\
		adminutil.h \
		array.h \
		backend.h \
		cups.h \
		dir.h \
		file.h \
		http.h \
		ipp.h \
		language.h \
		ppd.h \
		pwg.h \
		raster.h \
		sidechannel.h \
		transcode.h \
		versioning.h

HEADERSPRIV =	\
		array-private.h \
		cups-private.h \
		debug-private.h \
		file-private.h \
		http-private.h \
		ipp-private.h \
		language-private.h \
		md5-private.h \
		ppd-private.h \
		pwg-private.h \
		raster-private.h \
		snmp-private.h \
		string-private.h \
		thread-private.h


#
# Targets in this directory...
#

LIBTARGETS =	\
		$(LIBCUPSSTATIC) \
		$(LIBCUPS)

UNITTARGETS =	\
		testadmin \
		testarray \
		testcache \
		testconflicts \
		testcreds \
		testcups \
		testdest \
		testfile \
		testgetdests \
		testhttp \
		testi18n \
		testipp \
		testlang \
		testoptions \
		testppd \
		testpwg \
		testsnmp \
		tlscheck

TARGETS	=	\
		$(LIBTARGETS)


#
# Make all targets...
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
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(TARGETS) $(UNITTARGETS)
	$(RM) libcups.so libcups.dylib


#
# Update dependencies (without system header dependencies...)
#

depend:
	$(CC) -MM $(ALL_CFLAGS) $(OBJS:.o=.c) >Dependencies


#
# Run oclint to check code coverage...
#

oclint:
	oclint -o=oclint.html -html $(LIBOBJS:.o=.c) -- $(ALL_CFLAGS)


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:


#
# Install headers...
#

install-headers:
	echo Installing header files into $(INCLUDEDIR)/cups...
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	for file in $(HEADERS); do \
		$(INSTALL_DATA) $$file $(INCLUDEDIR)/cups; \
	done
	if test "x$(privateinclude)" != x; then \
		echo Installing private header files into $(PRIVATEINCLUDE)...; \
		$(INSTALL_DIR) -m 755 $(PRIVATEINCLUDE); \
		for file in $(HEADERSPRIV); do \
			$(INSTALL_DATA) $$file $(PRIVATEINCLUDE)/$$file; \
		done; \
	fi


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC)
	echo Installing libraries in $(LIBDIR)...
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPS) $(LIBDIR)
	if test $(LIBCUPS) = "libcups.so.2"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
		$(LN) $(LIBCUPS) $(LIBDIR)/`basename $(LIBCUPS) .2`; \
	fi
	if test $(LIBCUPS) = "libcups.2.dylib"; then \
		$(RM) $(LIBDIR)/libcups.dylib; \
		$(LN) $(LIBCUPS) $(LIBDIR)/libcups.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPS) $(SYMROOT); \
		dsymutil $(SYMROOT)/$(LIBCUPS); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) -m 755 $(LIBCUPSSTATIC) $(LIBDIR)
	$(RANLIB) $(LIBDIR)/$(LIBCUPSSTATIC)
	$(CHMOD) 555 $(LIBDIR)/$(LIBCUPSSTATIC)


#
# Uninstall object and target files...
#

uninstall:
	$(RM) $(LIBDIR)/libcups.2.dylib
	$(RM) $(LIBDIR)/$(LIBCUPSSTATIC)
	$(RM) $(LIBDIR)/libcups.dylib
	$(RM) $(LIBDIR)/libcups.so
	$(RM) $(LIBDIR)/libcups.so.2
	-$(RMDIR) $(LIBDIR)
	for file in $(HEADERS); do \
		$(RM) $(INCLUDEDIR)/cups/$$file; \
	done
	-$(RMDIR) $(INCLUDEDIR)/cups


#
# libcups.so.2
#

libcups.so.2:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(LDFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LIBGSSAPI) \
		$(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# libcups.2.dylib
#

libcups.2.dylib:	$(LIBOBJS) $(LIBCUPSORDER)
	echo Creating export list for $@...
	nm -gm $(LIBOBJS) 2>/dev/null | grep "__text" | grep -v weak | \
		awk '{print $$NF}' | \
		grep -v -E -e '^(_cupsConnect|_cupsCharset|_cupsEncodingName|_cupsSetDefaults|_cupsSetHTTPError|_cupsUserDefault)$$' | \
		sort >t.exp
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(LDFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 2.13.0 \
		-compatibility_version 2.0.0 \
		-exported_symbols_list t.exp \
		$(LIBOBJS) $(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) \
		$(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	$(RM) libcups.dylib t.exp
	$(LN) $@ libcups.dylib


#
# libcups.la
#

libcups.la:    $(LIBOBJS)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) \
		-rpath $(LIBDIR) -version-info 2:13 $(LIBGSSAPI) $(SSLLIBS) \
		$(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)


#
# libcups.a
#

libcups.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# libcups2.def (Windows DLL exports file...)
#

libcups2.def: $(LIBOBJS) Makefile
	echo Generating $@...
	echo "LIBRARY libcups2" >libcups2.def
	echo "VERSION 2.13" >>libcups2.def
	echo "EXPORTS" >>libcups2.def
	(nm $(LIBOBJS) 2>/dev/null | grep "T _" | awk '{print $$3}'; \
	 echo __cups_strcpy; echo __cups_strlcat; echo __cups_strlcpy) | \
		grep -v -E \
		    -e 'cups_debug|Apple|BackChannel|Backend|FileCheck|Filter|GSSService|SetNegotiate|SideChannel|SNMP' \
		    -e 'Block$$' | \
		sed -e '1,$$s/^_//' | sort >>libcups2.def


#
# testadmin (dependency on static CUPS library is intentional)
#

testadmin:	testadmin.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testadmin.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testarray (dependency on static CUPS library is intentional)
#

testarray:	testarray.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testarray.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running array API tests...
	./testarray


#
# testcache (dependency on static CUPS library is intentional)
#

testcache:	testcache.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testcache.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testconflicts (dependency on static CUPS library is intentional)
#

testconflicts:	testconflicts.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testconflicts.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testcreds (dependency on static CUPS library is intentional)
#

testcreds:	testcreds.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testcreds.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testcups (dependency on static CUPS library is intentional)
#

testcups:	testcups.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testcups.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testdest (dependency on static CUPS library is intentional)
#

testdest:	testdest.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testdest.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testfile (dependency on static CUPS library is intentional)
#

testfile:	testfile.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testfile.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running file API tests...
	./testfile


#
# testgetdests (dependency on static CUPS library is intentional)
#

testgetdests:	testgetdests.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testgetdests.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testhttp.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running HTTP API tests...
	./testhttp


#
# testipp (dependency on static CUPS library is intentional)
#

testipp:	testipp.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testipp.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running IPP API tests...
	./testipp


#
# testi18n (dependency on static CUPS library is intentional)
#

testi18n:	testi18n.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testi18n.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running internationalization API tests...
	./testi18n


#
# testlang (dependency on static CUPS library is intentional)
#

testlang:	testlang.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testlang.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Creating locale directory structure...
	$(RM) -r locale
	$(MKDIR) locale/en
	echo 'msgid "No"' > locale/en/cups_en.po
	echo 'msgstr "No"' >> locale/en/cups_en.po
	echo 'msgid "Yes"' >> locale/en/cups_en.po
	echo 'msgstr "Yes"' >> locale/en/cups_en.po
	for po in ../locale/cups_*.po; do \
		lang=`basename $$po .po | sed -e '1,$$s/^cups_//'`; \
		$(MKDIR) locale/$$lang; \
		$(LN) ../../$$po locale/$$lang; \
	done
	echo Running language API tests...
	LOCALEDIR=locale ./testlang


#
# testoptions (dependency on static CUPS library is intentional)
#

testoptions:	testoptions.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testoptions.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running option API tests...
	./testoptions


#
# testppd (dependency on static CUPS library is intentional)
#

testppd:	testppd.o $(LIBCUPSSTATIC) test.ppd test2.ppd
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testppd.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running PPD API tests...
	./testppd


#
# testpwg (dependency on static CUPS library is intentional)
#

testpwg:	testpwg.o $(LIBCUPSSTATIC) test.ppd
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ testpwg.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@
	echo Running PWG API tests...
	./testpwg test.ppd


#
# testsnmp (dependency on static CUPS library is intentional)
#

testsnmp:	testsnmp.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(LDFLAGS) -o $@ testsnmp.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# tlscheck (dependency on static CUPS library is intentional)
#

tlscheck:	tlscheck.o $(LIBCUPSSTATIC)
	echo Linking $@...
	$(LD_CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ tlscheck.o $(LIBCUPSSTATIC) \
		$(LIBGSSAPI) $(SSLLIBS) $(DNSSDLIBS) $(COMMONLIBS) $(LIBZ)
	$(CODE_SIGN) -s "$(CODE_SIGN_IDENTITY)" $@


#
# Automatic API help files...
#

apihelp:
	echo Generating CUPS API help files...
	$(RM) cupspm.xml
	mxmldoc --section "Programming" --body cupspm.md \
		cupspm.xml \
		auth.c cups.h dest*.c encode.c http.h http*.c ipp.h ipp*.c \
		options.c tls-darwin.c usersys.c util.c \
		--coverimage cupspm.png \
		--epub ../doc/help/cupspm.epub
	mxmldoc --section "Programming" --body cupspm.md \
		cupspm.xml > ../doc/help/cupspm.html
	$(RM) cupspm.xml
	mxmldoc --section "Programming" --title "Administration APIs" \
		--css ../doc/cups-printable.css \
		--header api-admin.header --intro api-admin.shtml \
		api-admin.xml \
		adminutil.c adminutil.h getdevices.c >../doc/help/api-admin.html
	$(RM) api-admin.xml
	mxmldoc --section "Programming" --title "PPD API (DEPRECATED)" \
		--css ../doc/cups-printable.css \
		--header api-ppd.header --intro api-ppd.shtml \
		api-ppd.xml ppd.h ppd-*.c >../doc/help/api-ppd.html
	$(RM) api-ppd.xml
	mxmldoc --section "Programming" \
		--title "Filter and Backend Programming" \
		--css ../doc/cups-printable.css \
		--header api-filter.header --intro api-filter.shtml \
		api-filter.xml \
		backchannel.c backend.h backend.c sidechannel.c sidechannel.h \
		>../doc/help/api-filter.html
	$(RM) api-filter.xml


#
# Lines of code computation...
#

sloc:
	echo "libcups: \c"
	sloccount $(LIBOBJS:.o=.c) 2>/dev/null | grep "Total Physical" | awk '{print $$9}'


#
# Dependencies...
#

include Dependencies
tls.o: tls-darwin.c tls-gnutls.c tls-sspi.c
