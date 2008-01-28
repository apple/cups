#
# "$Id$"
#
#   API library Makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2008 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products, all rights reserved.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs

#
# Object files...
#

LIBOBJS	=	\
		adminutil.o \
		array.o \
		attr.o \
		auth.o \
		backchannel.o \
		backend.o \
		custom.o \
		dest.o \
		dir.o \
		emit.o \
		encode.o \
		file.o \
		getifaddrs.o \
		getputfile.o \
		globals.o \
		http.o \
		http-addr.o \
		http-addrlist.o \
		http-support.o \
		ipp.o \
		ipp-support.o \
		langprintf.o \
		language.o \
		localize.o \
		mark.o \
		md5.o \
		md5passwd.o \
		notify.o \
		options.o \
		page.o \
		ppd.o \
		request.o \
		sidechannel.o \
		snmp.o \
		snprintf.o \
		string.o \
		tempfile.o \
		transcode.o \
		usersys.o \
		util.o
LIB32OBJS	= $(LIBOBJS:.o=.32.o)
LIB64OBJS	= $(LIBOBJS:.o=.64.o)
OBJS	=	\
		$(LIBOBJS) \
		$(LIB32OBJS) \
		$(LIB64OBJS) \
		testadmin.o \
		testarray.o \
		testcups.o \
		testfile.o \
		testhttp.o \
		testi18n.o \
		testipp.o \
		testlang.o \
		testppd.o \
		testsnmp.o \
		php_cups_wrap.o


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
		i18n.h \
		ipp.h \
		language.h \
		ppd.h \
		sidechannel.h \
		snmp.h \
		transcode.h \
		versioning.h


#
# Targets in this directory...
#

TARGETS	=	\
		$(LIBCUPS) \
		$(LIB32CUPS) \
		$(LIB64CUPS) \
		libcups.a \
		testadmin \
		testarray \
		testcups \
		testfile \
		testhttp \
		testi18n \
		testipp \
		testlang \
		testppd \
		testsnmp


#
# Make all targets...
#

all:	$(TARGETS)


#
# Remove object and target files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)
	$(RM) libcups.so libcups.sl libcups.dylib
	$(RM) -r 32bit 64bit


#
# Update dependencies (without system header dependencies...)
#

depend:
	touch Dependencies.tmp
	makedepend -Y -I.. -fDependencies.tmp $(OBJS:.o=.c) >/dev/null 2>&1
	$(RM) Dependencies
	cp Dependencies.tmp Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.32.o: \1\.c /' Dependencies.tmp >>Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.64.o: \1\.c /' Dependencies.tmp >>Dependencies
	$(RM) Dependencies.tmp


#
# Install object and target files...
#

install:	all installhdrs $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	$(INSTALL_DIR) -m 755 $(LIBDIR)
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
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		for file in $(TARGETS); do \
			cp $$file $(SYMROOT); \
		done \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) libcups.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcups.a

installhdrs:
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	for file in $(HEADERS); do \
		$(INSTALL_DATA) $$file $(INCLUDEDIR)/cups; \
	done

install32bit:
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcups.so.2 $(LIB32DIR)/libcups.so.2
	$(LN) libcups.so.2 $(LIB32DIR)/libcups.so

install64bit:
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcups.so.2 $(LIB64DIR)/libcups.so.2
	$(LN) libcups.so.2 $(LIB64DIR)/libcups.so


#
# Uninstall object and target files...
#

uninstall: $(UNINSTALL32) $(UNINSTALL64)
	$(RM) $(LIBDIR)/libcups.2.dylib
	$(RM) $(LIBDIR)/libcups.a
	$(RM) $(LIBDIR)/libcups.dylib
	$(RM) $(LIBDIR)/libcups_s.a
	$(RM) $(LIBDIR)/libcups.sl
	$(RM) $(LIBDIR)/libcups.sl.2
	$(RM) $(LIBDIR)/libcups.so
	$(RM) $(LIBDIR)/libcups.so.2
	-$(RMDIR) $(LIBDIR)
	for file in $(HEADERS); do \
		$(RM) $(INCLUDEDIR)/cups/$$file; \
	done
	-$(RMDIR) $(INCLUDEDIR)/cups

uninstall32bit:
	$(RM) $(LIB32DIR)/libcups.so
	$(RM) $(LIB32DIR)/libcups.so.2
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcups.so
	$(RM) $(LIB64DIR)/libcups.so.2
	-$(RMDIR) $(LIB64DIR)


#
# libcups.so.2, libcups.sl.2
#

libcups.so.2 libcups.sl.2:	$(LIBOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS) $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# 32bit/libcups.so.2
#

32bit/libcups.so.2:	$(LIB32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSO) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(LIB32OBJS) $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	$(RM) 32bit/libcups.so
	$(LN) libcups.so.2 32bit/libcups.so


#
# 64bit/libcups.so.2
#

64bit/libcups.so.2:	$(LIB64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSO) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(LIB64OBJS) $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	$(RM) 64bit/libcups.so
	$(LN) libcups.so.2 64bit/libcups.so


#
# libcups.2.dylib
#

libcups.2.dylib:	$(LIBOBJS) $(LIBCUPSORDER) libcups.exp
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 2.8.0 \
		-compatibility_version 2.0.0 \
		-exported_symbols_list libcups.exp \
		-sectorder __TEXT __text $(LIBCUPSORDER) \
		$(LIBOBJS) $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)
	$(RM) libcups.dylib
	$(LN) $@ libcups.dylib


#
# libcups_s.a
#

libcups_s.a:	$(LIBOBJS) libcups_s.exp
	echo Creating $@...
	$(DSO) $(DSOFLAGS) -Wl,-bexport:libcups_s.exp -o libcups_s.o $(LIBOBJS) $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcups_s.o


#
# libcups.la
#

libcups.la:    $(LIBOBJS)
	echo Linking $@...
	$(CC) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(LIBOBJS:.o=.lo) -rpath $(LIBDIR) \
		-version-info 2:8 $(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# libcups.a
#

libcups.a:	$(LIBOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@


#
# CUPS language bindings for various scripting languages...
#
# NOTE: Not currently used or functional - see the scripting/php directory
#       for the hand-written bindings...
#

phpcups.so:	$(LIBCUPS) php_cups_wrap.o
	echo Linking $@...
	if test `uname` = Darwin; then \
		DSOFLAGS="-bundle -flat_namespace -undefined suppress"; \
	else \
		DSOFLAGS="$(DSOFLAGS)"; \
	fi; \
	$(DSO) $$DSOFLAGS -o $@ php_cups_wrap.o $(LIBS) `php-config --ldflags --libs`

php_cups_wrap.o:	php_cups_wrap.c
	echo Compiling $<...
	$(CC) $(CFLAGS) `php-config --includes` -c $<
php_cups_wrap.c:	cups.h
	echo Creating $< using SWIG...
	swig -php -o $@ -module cups cups.h


#
# testadmin (dependency on static CUPS library is intentional)
#

testadmin:	testadmin.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testadmin.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testarray (dependency on static CUPS library is intentional)
#

testarray:	testarray.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testarray.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testcups (dependency on static CUPS library is intentional)
#

testcups:	testcups.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testcups.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testfile (dependency on static CUPS library is intentional)
#

testfile:	testfile.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testfile.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testhttp (dependency on static CUPS library is intentional)
#

testhttp:	testhttp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testhttp.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testipp (dependency on static CUPS library is intentional)
#

testipp:	testipp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testipp.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testi18n (dependency on static CUPS library is intentional)
#

testi18n:	testi18n.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testi18n.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testlang (dependency on static CUPS library is intentional)
#

testlang:	testlang.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testlang.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testppd (dependency on static CUPS library is intentional)
#

testppd:	testppd.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testppd.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# testsnmp (dependency on static CUPS library is intentional)
#

testsnmp:	testsnmp.o libcups.a
	echo Linking $@...
	$(CC) $(LDFLAGS) -o $@ testsnmp.o libcups.a \
		$(LIBGSSAPI) $(SSLLIBS) $(COMMONLIBS) $(LIBZ)


#
# Automatic API help files...
#

apihelp:
	echo Generating CUPS API help files...
	mxmldoc --section "Programming" --title "Array API" \
		--css ../doc/cups-printable.css \
		--header api-array.header --intro api-array.shtml \
		array.h array.c >../doc/help/api-array.html
	mxmldoc --section "Programming" --title "CUPS API" \
		--css ../doc/cups-printable.css \
		--header api-cups.header --intro api-cups.shtml \
		cups.h dest.c getputfile.c language.c notify.c \
		options.c tempfile.c usersys.c \
		util.c >../doc/help/api-cups.html
	mxmldoc --section "Programming" --title "File and Directory APIs" \
		--css ../doc/cups-printable.css \
		--header api-filedir.header --intro api-filedir.shtml \
		file.h file.c dir.h dir.c >../doc/help/api-filedir.html
	mxmldoc --section "Programming" --title "PPD API" \
		--css ../doc/cups-printable.css \
		--header api-ppd.header --intro api-ppd.shtml \
		ppd.h attr.c custom.c emit.c localize.c mark.c page.c \
		ppd.c >../doc/help/api-ppd.html
	mxmldoc --section "Programming" --title "HTTP and IPP APIs" \
		--css ../doc/cups-printable.css \
		--header api-httpipp.header --intro api-httpipp.shtml \
		http.h ipp.h auth.c encode.c http.c http-addr.c \
		http-support.c ipp.c ipp-support.c md5passwd.c \
		request.c >../doc/help/api-httpipp.html
	mxmldoc --section "Programming" --title "Filter and Backend APIs" \
		--css ../doc/cups-printable.css \
		--header api-filter.header --intro api-filter.shtml \
		backchannel.c sidechannel.c sidechannel.h >../doc/help/api-filter.html


#
# Dependencies...
#

include Dependencies


#
# End of "$Id$".
#
